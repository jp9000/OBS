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

void SceneItem::Update()
{
    pos = Vect2(element->GetFloat(TEXT("x")), element->GetFloat(TEXT("y")));
    //size = Vect2(element->GetFloat(TEXT("cx"), 100), element->GetFloat(TEXT("cy"), 100));

    //just reset the size if configuration changed, that way the user doesn't have to screw with the size
    if(source) size = source->GetSize();
    element->SetInt(TEXT("cx"), int(size.x));
    element->SetInt(TEXT("cy"), int(size.y));
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
    traceIn(Scene::~Scene);

    for(UINT i=0; i<sceneItems.Num(); i++)
        delete sceneItems[i];

    traceOut;
}

SceneItem* Scene::AddImageSource(XElement *sourceElement)
{
    traceIn(Scene::AddImageSource);

    if(GetSceneItem(sourceElement->GetName()) != NULL)
    {
        AppWarning(TEXT("Scene source '%s' already in scene.  actually, no one should get this error.  if you do send it to jim immidiately."), sourceElement->GetName());
        return NULL;
    }

    CTSTR lpClass = sourceElement->GetString(TEXT("class"));
    ImageSource *source = NULL;

    if(!lpClass)
        AppWarning(TEXT("No class for source '%s' in scene '%s'"), sourceElement->GetName(), API->GetSceneElement()->GetName());
    else
    {
        source = API->CreateImageSource(lpClass, sourceElement->GetElement(TEXT("data")));
        if(!source)
            AppWarning(TEXT("Could not create image source '%s' in scene '%s'"), sourceElement->GetName(), API->GetSceneElement()->GetName());
    }

    float x  = sourceElement->GetFloat(TEXT("x"));
    float y  = sourceElement->GetFloat(TEXT("y"));
    float cx = sourceElement->GetFloat(TEXT("cx"), 100);
    float cy = sourceElement->GetFloat(TEXT("cy"), 100);

    SceneItem *item = new SceneItem;
    item->element = sourceElement;
    item->parent = this;
    item->source = source;
    item->pos = Vect2(x, y);
    item->size = Vect2(cx, cy);

    API->EnterSceneMutex();
    if(bSceneStarted) source->BeginScene();
    sceneItems << item;
    API->LeaveSceneMutex();

    if(!source)
        bMissingSources = true;

    return item;

    traceOut;
}

void Scene::RemoveImageSource(SceneItem *item)
{
    traceIn(Scene::RemoveImageSource);

    if(bSceneStarted) item->source->EndScene();
    item->GetElement()->GetParent()->RemoveElement(item->GetElement());
    sceneItems.RemoveItem(item);
    delete item;

    traceOut;
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
    traceIn(Scene::Preprocess);

    for(UINT i=0; i<sceneItems.Num(); i++)
    {
        SceneItem *item = sceneItems[i];
        if(item->source)
            item->source->Preprocess();
    }

    traceOut;
}

void Scene::Tick(float fSeconds)
{
    traceIn(Scene::Tick);

    for(UINT i=0; i<sceneItems.Num(); i++)
    {
        SceneItem *item = sceneItems[i];
        if(item->source)
            item->source->Tick(fSeconds);
    }

    traceOut;
}

void Scene::Render()
{
    traceIn(Scene::Render);

    GS->ClearColorBuffer();

    for(UINT i=0; i<sceneItems.Num(); i++)
    {
        SceneItem *item = sceneItems[i];
        if(item->source)
            item->source->Render(item->pos, item->size);
    }

    traceOut;
}

void Scene::RenderSelections()
{
    traceIn(Scene::RenderSelections);

    for(UINT i=0; i<sceneItems.Num(); i++)
    {
        SceneItem *item = sceneItems[i];
        if(item->bSelected)
        {
            Vect2 pos  = item->GetPos()+1.0f;
            Vect2 size = item->GetSize()-2.0f;

            Vect2 outputSize = API->GetBaseSize();
            Vect2 frameSize  = API->GetRenderFrameSize();
            float sizeAdjust = outputSize.x/frameSize.x;

            Vect2 selectBoxSize = Vect2(10.0f, 10.0f)*sizeAdjust;

            DrawBox(pos, selectBoxSize);
            DrawBox((pos+size)-selectBoxSize, selectBoxSize);
            DrawBox(pos+Vect2(size.x-selectBoxSize.x, 0.0f), selectBoxSize);
            DrawBox(pos+Vect2(0.0f, size.y-selectBoxSize.y), selectBoxSize);
            DrawBox(pos, size);
        }
    }

    traceOut;
}

void Scene::BeginScene()
{
    traceIn(Scene::BeginScene);

    if(bSceneStarted)
        return;

    for(UINT i=0; i<sceneItems.Num(); i++)
    {
        SceneItem *item = sceneItems[i];
        if(item->source)
            item->source->BeginScene();
    }

    bSceneStarted = true;

    traceOut;
}

void Scene::EndScene()
{
    traceIn(Scene::EndScene);

    if(!bSceneStarted)
        return;

    for(UINT i=0; i<sceneItems.Num(); i++)
    {
        SceneItem *item = sceneItems[i];
        if(item->source)
            item->source->EndScene();
    }

    bSceneStarted = false;

    traceOut;
}
