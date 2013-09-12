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


#include "OBSApi.h"


SceneItem::~SceneItem()
{
    delete source;
}

void SceneItem::SetName(CTSTR lpNewName)
{
    element->SetName(lpNewName);
}

void SceneItem::SetRender(bool render)
{
    element->SetInt(TEXT("render"), (int)((render)?1:0));
    bRender = render;
    CTSTR lpClass = element->GetString(TEXT("class"));

    if (bRender) {
        if (!lpClass) {
            AppWarning(TEXT("No class for source '%s' in scene '%s'"), element->GetName(), API->GetSceneElement()->GetName());
        } else {
            XElement *data = element->GetElement(TEXT("data"));
            source = API->CreateImageSource(lpClass, data);
            if(!source) {
                AppWarning(TEXT("Could not create image source '%s' in scene '%s'"), element->GetName(), API->GetSceneElement()->GetName());
            } else {
                API->EnterSceneMutex();
                if (parent && parent->bSceneStarted) {
                    source->BeginScene();

                    if(scmp(lpClass, L"GlobalSource") == 0)
                        source->GlobalSourceEnterScene();
                }
                API->LeaveSceneMutex();
            }
        }
    } else {
        if (source) {
            API->EnterSceneMutex();

            ImageSource *src = source;
            source = NULL;

            if(scmp(lpClass, L"GlobalSource") == 0)
                src->GlobalSourceLeaveScene();

            if (parent && parent->bSceneStarted)
                src->EndScene();
            delete src;

            API->LeaveSceneMutex();
        }
    }
}

Vect4 SceneItem::GetCrop()
{
    Vect4 scaledCrop = crop;
    Vect2 scale = GetScale();
    scaledCrop.x /= scale.x; scaledCrop.y /= scale.y;
    scaledCrop.z /= scale.y; scaledCrop.w /= scale.x;
    return scaledCrop;
}

// The following functions return the difference in x/y coordinates, not the absolute distances.
Vect2 SceneItem::GetCropTL()
{
    return Vect2(GetCrop().x, GetCrop().y);
}

Vect2 SceneItem::GetCropTR()
{
    return Vect2(-GetCrop().w, GetCrop().y);
}

Vect2 SceneItem::GetCropBR()
{
    return Vect2(-GetCrop().w, -GetCrop().z);
}

Vect2 SceneItem::GetCropBL()
{
    return Vect2(GetCrop().x, -GetCrop().z);
}


void SceneItem::Update()
{
    pos = Vect2(element->GetFloat(TEXT("x")), element->GetFloat(TEXT("y")));
    size = Vect2(element->GetFloat(TEXT("cx"), 100.0f), element->GetFloat(TEXT("cy"), 100.0f));

    //just reset the size if configuration changed, that way the user doesn't have to screw with the size
    //if(source) size = source->GetSize();
    /*element->SetInt(TEXT("cx"), int(size.x));
    element->SetInt(TEXT("cy"), int(size.y));*/
}

void SceneItem::MoveUp()
{
    SceneItem *thisItem = this;
    UINT id = parent->sceneItems.FindValueIndex(thisItem);
    assert(id != INVALID);

    if(id > 0)
    {
        API->EnterSceneMutex();

        parent->sceneItems.SwapValues(id, id-1);
        GetElement()->MoveUp();

        API->LeaveSceneMutex();
    }
}

void SceneItem::MoveDown()
{
    SceneItem *thisItem = this;
    UINT id = parent->sceneItems.FindValueIndex(thisItem);
    assert(id != INVALID);

    if(id < (parent->sceneItems.Num()-1))
    {
        API->EnterSceneMutex();

        parent->sceneItems.SwapValues(id, id+1);
        GetElement()->MoveDown();

        API->LeaveSceneMutex();
    }
}

void SceneItem::MoveToTop()
{
    SceneItem *thisItem = this;
    UINT id = parent->sceneItems.FindValueIndex(thisItem);
    assert(id != INVALID);

    if(id > 0)
    {
        API->EnterSceneMutex();

        parent->sceneItems.Remove(id);
        parent->sceneItems.Insert(0, this);

        GetElement()->MoveToTop();

        API->LeaveSceneMutex();
    }
}

void SceneItem::MoveToBottom()
{
    SceneItem *thisItem = this;
    UINT id = parent->sceneItems.FindValueIndex(thisItem);
    assert(id != INVALID);

    if(id < (parent->sceneItems.Num()-1))
    {
        API->EnterSceneMutex();

        parent->sceneItems.Remove(id);
        parent->sceneItems << this;

        GetElement()->MoveToBottom();

        API->LeaveSceneMutex();
    }
}

//====================================================================================

Scene::~Scene()
{
    for(UINT i=0; i<sceneItems.Num(); i++)
        delete sceneItems[i];
}

SceneItem* Scene::AddImageSource(XElement *sourceElement)
{
    return InsertImageSource(sceneItems.Num(), sourceElement);
}

SceneItem* Scene::InsertImageSource(UINT pos, XElement *sourceElement)
{
    if(GetSceneItem(sourceElement->GetName()) != NULL)
    {
        AppWarning(TEXT("Scene source '%s' already in scene.  actually, no one should get this error.  if you do send it to jim immidiately."), sourceElement->GetName());
        return NULL;
    }

    if(pos > sceneItems.Num())
    {
        AppWarning(TEXT("Scene::InsertImageSource: pos >= sceneItems.Num()"));
        pos = sceneItems.Num();
    }

    bool render = sourceElement->GetInt(TEXT("render"), 1) > 0;

    float x  = sourceElement->GetFloat(TEXT("x"));
    float y  = sourceElement->GetFloat(TEXT("y"));
    float cx = sourceElement->GetFloat(TEXT("cx"), 100);
    float cy = sourceElement->GetFloat(TEXT("cy"), 100);

    SceneItem *item = new SceneItem;
    item->element = sourceElement;
    item->parent = this;
    item->pos = Vect2(x, y);
    item->size = Vect2(cx, cy);
    item->crop.w = sourceElement->GetFloat(TEXT("crop.right"));
    item->crop.x = sourceElement->GetFloat(TEXT("crop.left"));
    item->crop.y = sourceElement->GetFloat(TEXT("crop.top"));
    item->crop.z = sourceElement->GetFloat(TEXT("crop.bottom"));
    item->SetRender(render);

    API->EnterSceneMutex();
    sceneItems.Insert(pos, item);
    API->LeaveSceneMutex();

    /*if(!source)
        bMissingSources = true;*/

    return item;
}

void Scene::RemoveImageSource(SceneItem *item)
{
    if(bSceneStarted && item->source) item->source->EndScene();
    item->GetElement()->GetParent()->RemoveElement(item->GetElement());
    sceneItems.RemoveItem(item);
    delete item;
}

void Scene::RemoveImageSource(CTSTR lpName)
{
    for(UINT i=0; i<sceneItems.Num(); i++)
    {
        if(scmpi(sceneItems[i]->GetName(), lpName) == 0)
        {
            RemoveImageSource(sceneItems[i]);
            return;
        }
    }
}

void Scene::Preprocess()
{
    for(UINT i=0; i<sceneItems.Num(); i++)
    {
        SceneItem *item = sceneItems[i];
        if(item->source && item->bRender)
            item->source->Preprocess();
    }
}

void Scene::Tick(float fSeconds)
{
    for(UINT i=0; i<sceneItems.Num(); i++)
    {
        SceneItem *item = sceneItems[i];
        if(item->source)
            item->source->Tick(fSeconds);
    }
}

void Scene::Render()
{
    GS->ClearColorBuffer();

    for(int i=sceneItems.Num()-1; i>=0; i--)
    {
        SceneItem *item = sceneItems[i];
        if(item->source && item->bRender)
        {
            GS->SetCropping (item->GetCrop().x, item->GetCrop().y, item->GetCrop().w, item->GetCrop().z);
            item->source->Render(item->pos, item->size);
            GS->SetCropping (0.0f, 0.0f, 0.0f, 0.0f);
        }
    }
}

void Scene::RenderSelections(Shader *solidPixelShader)
{
    for(UINT i=0; i<sceneItems.Num(); i++)
    {
        SceneItem *item = sceneItems[i];

        if(item->bSelected)
        {
            Vect2 baseScale = item->GetSource() ? item->GetSource()->GetSize() : item->GetSize();
            Vect2 cropFactor = baseScale / item->GetSize();
            Vect4 crop = item->GetCrop();
            Vect2 pos  = API->MapFrameToWindowPos(item->GetPos());
            Vect2 scale = API->GetFrameToWindowScale();
            crop.x *= scale.x; crop.y *= scale.y;
            crop.z *= scale.y; crop.w *= scale.x;
            pos.x += crop.x;
            pos.y += crop.y;
            Vect2 size = API->MapFrameToWindowSize(item->GetSize());
            size.x -= (crop.x + crop.w);
            size.y -= (crop.y + crop.z);
            Vect2 selectBoxSize = Vect2(10.0f, 10.0f);
            
            DrawBox(pos, selectBoxSize);
            DrawBox((pos+size)-selectBoxSize, selectBoxSize);
            DrawBox(pos+Vect2(size.x-selectBoxSize.x, 0.0f), selectBoxSize);
            DrawBox(pos+Vect2(0.0f, size.y-selectBoxSize.y), selectBoxSize);

            // Top
            if (CloseFloat(crop.y, 0.0f)) solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0xFF0000);
            else solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0x00FF00);
            DrawBox(pos, Vect2(size.x, 0.0f));

            // Left
            if (CloseFloat(crop.x, 0.0f)) solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0xFF0000);
            else solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0x00FF00);
            DrawBox(pos, Vect2(0.0f, size.y));

            // Right
            if (CloseFloat(crop.w, 0.0f)) solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0xFF0000);
            else solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0x00FF00);
            DrawBox(pos+Vect2(size.x, 0.0f), Vect2(0.0f, size.y));

            // Bottom
            if (CloseFloat(crop.z, 0.0f)) solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0xFF0000);
            else solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0x00FF00);
            DrawBox(pos+Vect2(0.0f, size.y), Vect2(size.x, 0.0f));

//#define DRAW_UNCROPPED_SELECTION_BOX
#ifdef DRAW_UNCROPPED_SELECTION_BOX
            Vect2 realPos = API->MapFrameToWindowPos(item->GetPos());
            Vect2 realSize = item->GetSize() * scale;
            solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0xFFFFFF);
            solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0xFFFFFF);
            DrawBox(realPos, Vect2(0.0f, realSize.y));
            DrawBox(realPos, Vect2(realSize.x, 0.0f));
            DrawBox(realPos+Vect2(realSize.x, 0.0f), Vect2(0.0f, realSize.y));
            DrawBox(realPos+Vect2(0.0f, realSize.y), Vect2(realSize.x, 0.0f));
#endif
        }
    }
}

void Scene::BeginScene()
{
    if(bSceneStarted)
        return;

    for(UINT i=0; i<sceneItems.Num(); i++)
    {
        SceneItem *item = sceneItems[i];
        if(item->source)
            item->source->BeginScene();
    }

    bSceneStarted = true;
}

void Scene::EndScene()
{
    if(!bSceneStarted)
        return;

    for(UINT i=0; i<sceneItems.Num(); i++)
    {
        SceneItem *item = sceneItems[i];
        if(item->source)
            item->source->EndScene();
    }

    bSceneStarted = false;
}
