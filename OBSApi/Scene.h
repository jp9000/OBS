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


#pragma once


//-------------------------------------------------------------------

class BASE_EXPORT ImageSource
{
public:
    virtual ~ImageSource() {}
    virtual void Preprocess() {}
    virtual void Tick(float fSeconds) {}
    virtual void Render(const Vect2 &pos, const Vect2 &size)=0;

    virtual Vect2 GetSize() const=0;

    virtual void UpdateSettings() {}

    virtual void BeginScene() {}
    virtual void EndScene() {}
};


//====================================================================================

class BASE_EXPORT SceneItem
{
    friend class Scene;
    friend class OBS;

    Scene *parent;

    ImageSource *source;
    XElement *element;

    Vect2 startPos, startSize;
    Vect2 pos, size;

    bool bSelected;

public:
    ~SceneItem();

    inline CTSTR GetName() const                {return element->GetName();}
    inline ImageSource* GetSource() const       {return source;}
    inline XElement* GetElement() const         {return element;}

    inline Vect2 GetPos() const                 {return pos;}
    inline Vect2 GetSize() const                {return size;}

    inline void SetPos(const Vect2 &newPos)     {pos = newPos;}
    inline void SetSize(const Vect2 &newSize)   {size = newSize;}

    inline void Select(bool bSelect)            {bSelected = bSelect;}
    inline bool IsSelected() const              {return bSelected;}

    inline UINT GetID();

    void SetName(CTSTR lpNewName);

    void Update();

    void MoveUp();
    void MoveDown();
    void MoveToTop();
    void MoveToBottom();
};

//====================================================================================

class BASE_EXPORT Scene
{
    friend class SceneItem;
    friend class OBS;

    bool bSceneStarted;
    bool bMissingSources;

    List<SceneItem*> sceneItems;
    DWORD bkColor;

    inline void DeselectAll()
    {
        for(UINT i=0; i<sceneItems.Num(); i++)
            sceneItems[i]->bSelected = false;
    }

public:
    virtual ~Scene();

    virtual SceneItem* AddImageSource(XElement *sourceElement);

    virtual void RemoveImageSource(SceneItem *item);
    void RemoveImageSource(CTSTR lpName);

    virtual void Tick(float fSeconds);
    virtual void Preprocess();
    virtual void Render();

    virtual void UpdateSettings() {}

    virtual void RenderSelections();

    virtual void BeginScene();
    virtual void EndScene();

    //--------------------------------

    inline bool HasMissingSources() const {return bMissingSources;}

    inline void GetItemsOnPoint(const Vect2 &pos, List<SceneItem*> &items) const
    {
        for(UINT i=0; i<sceneItems.Num(); i++)
        {
            SceneItem *item = sceneItems[i];
            Vect2 upperLeft  = item->GetPos();
            Vect2 lowerRight = upperLeft+item->GetSize();
            if(pos.x >= upperLeft.x && pos.y >= upperLeft.y && pos.x <= lowerRight.x && pos.y <= lowerRight.y)
                items << item;
        }
    }

    //--------------------------------

    inline UINT NumSceneItems() const {return sceneItems.Num();}
    inline SceneItem* GetSceneItem(UINT id) const {return sceneItems[id];}

    inline SceneItem* GetSceneItem(CTSTR lpName) const
    {
        for(UINT i=0; i<sceneItems.Num(); i++)
        {
            if(scmpi(sceneItems[i]->GetName(), lpName) == 0)
                return sceneItems[i];
        }

        return NULL;
    }

    inline void GetSelectedItems(List<SceneItem*> &items) const
    {
        items.Clear();

        for(UINT i=0; i<sceneItems.Num(); i++)
        {
            if(sceneItems[i]->bSelected)
                items << sceneItems[i];
        }
    }
};


inline UINT SceneItem::GetID()
{
    SceneItem *thisItem = this;
    return parent->sceneItems.FindValueIndex(thisItem);
}
