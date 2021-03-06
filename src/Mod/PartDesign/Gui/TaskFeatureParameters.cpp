/***************************************************************************
 *   Copyright (C) 2015 Alexander Golubev (Fat-Zer) <fatzer2@gmail.com>    *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"
#ifndef _PreComp_
#include <QMessageBox>
#endif

#include <Gui/Application.h>
#include <Gui/Command.h>
#include <Gui/BitmapFactory.h>
#include <Mod/PartDesign/App/Feature.h>
#include <Mod/PartDesign/App/Body.h>

#include "TaskFeatureParameters.h"

using namespace PartDesignGui;
using namespace Gui;

/*********************************************************************
 *                      Task Feature Parameters                      *
 *********************************************************************/

TaskFeatureParameters::TaskFeatureParameters(PartDesignGui::ViewProvider *vp, QWidget *parent,
                                                     const std::string& pixmapname, const QString& parname)
    : TaskBox(Gui::BitmapFactory().pixmap(pixmapname.c_str()),parname,true, parent),
      vp(vp), blockUpdate(false)
{ }

void TaskFeatureParameters::onUpdateView(bool on)
{
    blockUpdate = !on;
    recomputeFeature();
}

void TaskFeatureParameters::recomputeFeature()
{
    if (!blockUpdate) {
        App::DocumentObject* obj = vp->getObject ();
        assert (obj);
        obj->getDocument()->recomputeFeature ( obj );
    }
}

/*********************************************************************
 *                            Task Dialog                            *
 *********************************************************************/
TaskDlgFeatureParameters::TaskDlgFeatureParameters(PartDesignGui::ViewProvider *vp)
    : TaskDialog(),vp(vp)
{
    assert(vp);
}

TaskDlgFeatureParameters::~TaskDlgFeatureParameters()
{

}

bool TaskDlgFeatureParameters::accept() {
    App::DocumentObject* feature = vp->getObject();

    try {
        // Iterate over parameter dialogs and apply all parameters from them
        for ( QWidget *wgt : Content ) {
            TaskFeatureParameters *param = qobject_cast<TaskFeatureParameters *> (wgt);
            if(!param)
                continue;
            
            param->saveHistory ();
            param->apply ();
        }
        // Make sure the feature is what we are expecting
        // Should be fine but you never know...
        if ( !feature->getTypeId().isDerivedFrom(PartDesign::Feature::getClassTypeId()) ) {
            throw Base::Exception("Bad object processed in the feature dialog.");
        }

        App::DocumentObject* previous = static_cast<PartDesign::Feature*>(feature)->getBaseObject(/* silent = */ true );

        if (previous) {
            Gui::Command::doCommand(Gui::Command::Gui,"Gui.activeDocument().hide(\"%s\")",
                    previous->getNameInDocument());
        }

        Gui::Command::doCommand(Gui::Command::Doc,"App.ActiveDocument.recompute()");

        if (!feature->isValid()) {
            throw Base::Exception(vp->getObject()->getStatusString());
        }

        Gui::Command::doCommand(Gui::Command::Gui,"Gui.activeDocument().resetEdit()");
        Gui::Command::commitCommand();
    } catch (const Base::Exception& e) {
        // Generally the only thing that should fail is feature->isValid() others should be fine
        QMessageBox::warning( 0, tr("Input error"), QString::fromLatin1(e.what()));
        return false;
    }

    return true;
}

bool TaskDlgFeatureParameters::reject()
{
    PartDesign::Feature* feature = static_cast<PartDesign::Feature*>(vp->getObject());

    PartDesign::Body* body = PartDesign::Body::findBodyOf(feature);

    // Find out previous feature we won't be able to do it after abort
    // (at least in the body case)
    App::DocumentObject* previous = feature->getBaseObject(/* silent = */ true );

    // roll back the done things
    Gui::Command::abortCommand();
    Gui::Command::doCommand(Gui::Command::Gui,"Gui.activeDocument().resetEdit()");

    // if abort command deleted the object make the previous feature visible again
    if (!Gui::Application::Instance->getViewProvider(feature)) {
        // Make the tip or the previous feature visiable again with preference to the previous one
        // TODO: ViewProvider::onDelete has the same code. May be this one is excess?
        if (previous && Gui::Application::Instance->getViewProvider(previous)) {
            Gui::Application::Instance->getViewProvider(previous)->show();
        } else if (body != NULL) {
            App::DocumentObject* tip = body->Tip.getValue();
            if (tip && Gui::Application::Instance->getViewProvider(tip)) {
                Gui::Application::Instance->getViewProvider(tip)->show();
            }
        }
    }

    return true;
}

#include "moc_TaskFeatureParameters.cpp"
