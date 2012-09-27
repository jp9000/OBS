/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/


#include "Main.h"


void OBS::RegisterSceneClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc)
{
    if(!lpClassName || !*lpClassName)
    {
        AppWarning(TEXT("OBS::RegisterSceneClass: No class name specified"));
        return;
    }

    if(!createProc)
    {
        AppWarning(TEXT("OBS::RegisterSceneClass: No create procedure specified"));
        return;
    }

    if(GetSceneClass(lpClassName))
    {
        AppWarning(TEXT("OBS::RegisterSceneClass: Tried to register '%s', but it already exists"), lpClassName);
        return;
    }

    ClassInfo *classInfo  = sceneClasses.CreateNew();
    classInfo->strClass   = lpClassName;
    classInfo->strName    = lpDisplayName;
    classInfo->createProc = createProc;
    classInfo->configProc = configProc;
}

void OBS::RegisterImageSourceClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc)
{
    if(!lpClassName || !*lpClassName)
    {
        AppWarning(TEXT("OBS::RegisterImageSourceClass: No class name specified"));
        return;
    }

    if(!createProc)
    {
        AppWarning(TEXT("OBS::RegisterImageSourceClass: No create procedure specified"));
        return;
    }

    if(GetImageSourceClass(lpClassName))
    {
        AppWarning(TEXT("OBS::RegisterImageSourceClass: Tried to register '%s', but it already exists"), lpClassName);
        return;
    }

    ClassInfo *classInfo  = imageSourceClasses.CreateNew();
    classInfo->strClass   = lpClassName;
    classInfo->strName    = lpDisplayName;
    classInfo->createProc = createProc;
    classInfo->configProc = configProc;
}

Scene* OBS::CreateScene(CTSTR lpClassName, XElement *data)
{
    for(UINT i=0; i<sceneClasses.Num(); i++)
    {
        if(sceneClasses[i].strClass.CompareI(lpClassName))
            return (Scene*)sceneClasses[i].createProc(data);
    }

    AppWarning(TEXT("OBS::CreateScene: Could not find scene class '%s'"), lpClassName);
    return NULL;
}

ImageSource* OBS::CreateImageSource(CTSTR lpClassName, XElement *data)
{
    for(UINT i=0; i<imageSourceClasses.Num(); i++)
    {
        if(imageSourceClasses[i].strClass.CompareI(lpClassName))
            return (ImageSource*)imageSourceClasses[i].createProc(data);
    }

    AppWarning(TEXT("OBS::CreateImageSource: Could not find image source class '%s'"), lpClassName);
    return NULL;
}

void OBS::ConfigureScene(XElement *element)
{
    if(!element)
    {
        AppWarning(TEXT("OBS::ConfigureScene: NULL element specified"));
        return;
    }

    CTSTR lpClassName = element->GetString(TEXT("class"));
    if(!lpClassName)
    {
        AppWarning(TEXT("OBS::ConfigureScene: No class specified for scene '%s'"), element->GetName());
        return;
    }

    for(UINT i=0; i<sceneClasses.Num(); i++)
    {
        if(sceneClasses[i].strClass.CompareI(lpClassName))
        {
            if(sceneClasses[i].configProc)
                sceneClasses[i].configProc(element, false);
            return;
        }
    }

    AppWarning(TEXT("OBS::ConfigureScene: Could not find scene class '%s'"), lpClassName);
}

void OBS::ConfigureImageSource(XElement *element)
{
    if(!element)
    {
        AppWarning(TEXT("OBS::ConfigureImageSource: NULL element specified"));
        return;
    }

    CTSTR lpClassName = element->GetString(TEXT("class"));
    if(!lpClassName)
    {
        AppWarning(TEXT("OBS::ConfigureImageSource: No class specified for image source '%s'"), element->GetName());
        return;
    }

    for(UINT i=0; i<imageSourceClasses.Num(); i++)
    {
        if(imageSourceClasses[i].strClass.CompareI(lpClassName))
        {
            if(imageSourceClasses[i].configProc)
                imageSourceClasses[i].configProc(element, false);
            return;
        }
    }

    AppWarning(TEXT("OBS::ConfigureImageSource: Could not find scene class '%s'"), lpClassName);
}

bool OBS::SetScene(CTSTR lpScene)
{
    HWND hwndScenes = GetDlgItem(hwndMain, ID_SCENES);
    UINT curSel = (UINT)SendMessage(hwndScenes, LB_GETCURSEL, 0, 0);

    //-------------------------

    if(curSel != LB_ERR)
    {
        UINT textLen = (UINT)SendMessage(hwndScenes, LB_GETTEXTLEN, curSel, 0);

        String strLBName;
        strLBName.SetLength(textLen);

        SendMessage(hwndScenes, LB_GETTEXT, curSel, (LPARAM)strLBName.Array());
        if(!strLBName.CompareI(lpScene))
        {
            UINT id = (UINT)SendMessage(hwndScenes, LB_FINDSTRINGEXACT, -1, (LPARAM)lpScene);
            if(id == LB_ERR)
                return false;

            SendMessage(hwndScenes, LB_SETCURSEL, id, 0);
        }
    }
    else
    {
        UINT id = (UINT)SendMessage(hwndScenes, LB_FINDSTRINGEXACT, -1, (LPARAM)lpScene);
        if(id == LB_ERR)
            return false;

        SendMessage(hwndScenes, LB_SETCURSEL, id, 0);
    }

    //-------------------------

    XElement *scenes = scenesConfig.GetElement(TEXT("scenes"));
    XElement *newSceneElement = scenes->GetElement(lpScene);
    if(!newSceneElement)
        return false;

    if(sceneElement == newSceneElement)
        return true;

    sceneElement = newSceneElement;

    CTSTR lpClass = sceneElement->GetString(TEXT("class"));
    if(!lpClass)
    {
        AppWarning(TEXT("OBS::SetScene: no class found for scene '%s'"), newSceneElement->GetName());
        return false;
    }

    XElement *sceneData = newSceneElement->GetElement(TEXT("data"));

    //-------------------------

    Scene *newScene = NULL;
    if(bRunning)
        newScene = CreateScene(lpClass, sceneData);

    //-------------------------

    HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);
    SendMessage(hwndSources, LB_RESETCONTENT, 0, 0);

    XElement *sources = sceneElement->GetElement(TEXT("sources"));
    if(sources)
    {
        UINT numSources = sources->NumElements();
        for(UINT i=0; i<numSources; i++)
        {
            XElement *sourceElement = sources->GetElementByID(i);
            UINT id = (UINT)SendMessage(hwndSources, LB_ADDSTRING, 0, (LPARAM)sourceElement->GetName());

            if(bRunning && newScene)
                newScene->AddImageSource(sourceElement);
        }
    }

    if(scene && newScene->HasMissingSources())
        MessageBox(hwndMain, Str("Scene.MissingSources"), NULL, 0);

    if(bRunning)
    {
        //todo: cache scenes maybe?  undecided.  not really as necessary with global sources
        OSEnterMutex(hSceneMutex);

        if(scene)
            scene->EndScene();

        Scene *previousScene = scene;
        scene = newScene;

        scene->BeginScene();

        if(!bTransitioning)
        {
            bTransitioning = true;
            transitionAlpha = 0.0f;
        }

        OSLeaveMutex(hSceneMutex);

        delete previousScene;
    }

    return true;
}

