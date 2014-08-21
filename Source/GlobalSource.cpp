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


class GlobalSource : public ImageSource
{
    ImageSource *globalSource;

    XElement *data;
    ClassInfo *sourceClass;

public:
    void Init(XElement *data)
    {
        this->data = data;
        UpdateSettings();
    }

    virtual void GlobalSourceLeaveScene() {if (globalSource) globalSource->GlobalSourceLeaveScene();}
    virtual void GlobalSourceEnterScene() {if (globalSource) globalSource->GlobalSourceEnterScene();}

    //called elsewhere automatically by the app
    //void Preprocess() {globalSource->Preprocess();}
    //void Tick(float fSeconds) {globalSource->Tick(fSeconds);}

    void Render(const Vect2 &pos, const Vect2 &size) {if (globalSource) globalSource->Render(pos, size);}
    Vect2 GetSize() const {return globalSource ? globalSource->GetSize() : Vect2(0.0f, 0.0f);}

    void UpdateSettings()
    {
        String strName = data->GetString(TEXT("name"));
        globalSource = App->GetGlobalSource(strName);
    }

    //-------------------------------------------------------------

    virtual void SetFloat(CTSTR lpName, float fValue)         {if (globalSource) globalSource->SetFloat  (lpName, fValue);}
    virtual void SetInt(CTSTR lpName, int iValue)             {if (globalSource) globalSource->SetInt    (lpName, iValue);}
    virtual void SetString(CTSTR lpName, CTSTR lpVal)         {if (globalSource) globalSource->SetString (lpName, lpVal);}
    virtual void SetVector(CTSTR lpName, const Vect &value)   {if (globalSource) globalSource->SetVector (lpName, value);}
    virtual void SetVector2(CTSTR lpName, const Vect2 &value) {if (globalSource) globalSource->SetVector2(lpName, value);}
    virtual void SetVector4(CTSTR lpName, const Vect4 &value) {if (globalSource) globalSource->SetVector4(lpName, value);}
    virtual void SetMatrix(CTSTR lpName, const Matrix &mat)   {if (globalSource) globalSource->SetMatrix (lpName, mat);}

    //-------------------------------------------------------------

    virtual bool GetFloat(CTSTR lpName, float &fValue)   const {return globalSource ? globalSource->GetFloat  (lpName, fValue) : false;}
    virtual bool GetInt(CTSTR lpName, int &iValue)       const {return globalSource ? globalSource->GetInt    (lpName, iValue) : false;}
    virtual bool GetString(CTSTR lpName, String &strVal) const {return globalSource ? globalSource->GetString (lpName, strVal) : false;}
    virtual bool GetVector(CTSTR lpName, Vect &value)    const {return globalSource ? globalSource->GetVector (lpName, value)  : false;}
    virtual bool GetVector2(CTSTR lpName, Vect2 &value)  const {return globalSource ? globalSource->GetVector2(lpName, value)  : false;}
    virtual bool GetVector4(CTSTR lpName, Vect4 &value)  const {return globalSource ? globalSource->GetVector4(lpName, value)  : false;}
    virtual bool GetMatrix(CTSTR lpName, Matrix &mat)    const {return globalSource ? globalSource->GetMatrix (lpName, mat)    : false;}
};


ImageSource* STDCALL CreateGlobalSource(XElement *data)
{
    GlobalSource *source = new GlobalSource;
    source->Init(data);

    /*XElement *sourceElement = data->GetParent();

    Vect2 size = source->GetSize();
    sourceElement->SetInt(TEXT("cx"), int(size.x+EPSILON));
    sourceElement->SetInt(TEXT("cy"), int(size.y+EPSILON));*/

    return source;
}

bool STDCALL OBS::ConfigGlobalSource(XElement *element, bool bCreating)
{
    XElement *data = element->GetElement(TEXT("data"));
    CTSTR lpGlobalSourceName = data->GetString(TEXT("name"));

    XElement *globalSources = App->scenesConfig.GetElement(TEXT("global sources"));
    if(!globalSources) //shouldn't happen
        return false;

    XElement *globalSourceElement = globalSources->GetElement(lpGlobalSourceName);
    if(!globalSourceElement) //shouldn't happen
        return false;

    CTSTR lpClass = globalSourceElement->GetString(TEXT("class"));

    ClassInfo *classInfo = App->GetImageSourceClass(lpClass);
    if(!classInfo) //shouldn't happen
        return false;

    if(classInfo->configProc)
    {
        if(!classInfo->configProc(globalSourceElement, bCreating))
            return false;

        element->SetInt(TEXT("cx"), globalSourceElement->GetInt(TEXT("cx")));
        element->SetInt(TEXT("cy"), globalSourceElement->GetInt(TEXT("cy")));

        if(App->bRunning)
        {
            for(UINT i=0; i<App->globalSources.Num(); i++)
            {
                GlobalSourceInfo &info = App->globalSources[i];
                if(info.strName.CompareI(lpGlobalSourceName) && info.source)
                {
                    App->EnterSceneMutex ();
                    info.source->UpdateSettings();
                    App->LeaveSceneMutex ();
                    break;
                }
            }
        }
    }

    return true;
}

ImageSource* OBS::AddGlobalSourceToScene(CTSTR lpName)
{
    XElement *globals = scenesConfig.GetElement(TEXT("global sources"));
    if(globals)
    {
        XElement *globalSourceElement = globals->GetElement(lpName);
        if(globalSourceElement)
        {
            CTSTR lpClass = globalSourceElement->GetString(TEXT("class"));
            if(lpClass)
            {
                ImageSource *newGlobalSource = CreateImageSource(lpClass, globalSourceElement->GetElement(TEXT("data")));
                if(newGlobalSource)
                {
                    App->EnterSceneMutex();

                    GlobalSourceInfo *info = globalSources.CreateNew();
                    info->strName = lpName;
                    info->element = globalSourceElement;
                    info->source = newGlobalSource;

                    info->source->BeginScene();

                    App->LeaveSceneMutex();

                    return newGlobalSource;
                }
            }
        }
    }

    AppWarning(TEXT("OBS::AddGlobalSourceToScene: Could not find global source '%s'"), lpName);
    return NULL;
}

void OBS::GetGlobalSourceNames(List<CTSTR> &globalSourceNames, bool mainSceneGlobalSourceNames)
{
    globalSourceNames.Clear();

    if(!mainSceneGlobalSourceNames)
    {
        XElement *globals = scenesConfig.GetElement(TEXT("global sources"));
        if(globals)
        {
            UINT numSources = globals->NumElements();
            for(UINT i=0; i<numSources; i++)
            {
                XElement *sourceElement = globals->GetElementByID(i);
                globalSourceNames << sourceElement->GetName();
            }
        }
    }
    else
    {
        XElement *globals = globalSourcesImportConfig.GetElement(TEXT("global sources"));
        if(globals)
        {
            UINT numSources = globals->NumElements();
            for(UINT i=0; i<numSources; i++)
            {
                XElement *sourceElement = globals->GetElementByID(i);
                globalSourceNames << sourceElement->GetName();
            }
        }
    }
}

XElement* OBS::GetGlobalSourceElement(CTSTR lpName)
{
    XElement *globals = scenesConfig.GetElement(TEXT("global sources"));
    if(globals)
        return globals->GetElement(lpName);

    return NULL;
}
