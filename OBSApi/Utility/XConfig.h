/********************************************************************************
 Copyright (C) 2001-2012 Hugh Bailey <obs.jim@gmail.com>

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


//A compact JSON implementation


enum
{
    XConfig_Data,
    XConfig_Element
};

class BASE_EXPORT XBaseItem
{
    friend class XElement;
    friend class XConfig;

protected:
    inline XBaseItem(int type, CTSTR lpName) : type(type), strName(lpName) {}

    virtual ~XBaseItem() {}

    String strName;
    int type;

public:
    inline int GetType() const     {return type;}
    inline bool IsData() const     {return type == XConfig_Data;}
    inline bool IsElement() const  {return type == XConfig_Element;}

    inline CTSTR GetName() const        {return strName;}
    inline void  SetName(CTSTR lpName)  {strName = lpName;}
};


class XDataItem : public XBaseItem
{
    friend class XElement;
    friend class XConfig;

protected:
    inline XDataItem(CTSTR lpName, CTSTR lpData)
        : XBaseItem(XConfig_Data, lpName), strData(lpData)
    {}

    String strData;

public:
    inline CTSTR GetData() const        {return strData;}
    inline void  SetData(CTSTR lpData)  {strData = lpData;}
};


class BASE_EXPORT XElement : public XBaseItem
{
    friend class XConfig;

    XConfig *file;

    XElement *parent;
    List<XBaseItem*> SubItems;

    inline XElement(XConfig *XConfig, XElement *parentElement, CTSTR lpName)
        : XBaseItem(XConfig_Element, lpName), parent(parentElement), file(XConfig)
    {}

protected:
    ~XElement();

    XElement* NewElementCopy(XElement* element, bool bSelfAsRoot);

public:

    inline void ReverseOrder()
    {
        UINT count = SubItems.Num()/2;
        for(UINT i=0; i<count; i++)
            SubItems.SwapValues(i, SubItems.Num()-1-i);
    }

    inline bool HasItem(CTSTR lpName) const
    {
        for (UINT i=0; i<SubItems.Num(); i++) {
            if (SubItems[i]->strName.CompareI(lpName))
                return true;
        }

        return false;
    }

    CTSTR GetString(CTSTR lpName, TSTR def=NULL) const;
    int   GetInt(CTSTR lpName, int def=0) const;
    float GetFloat(CTSTR lpName, float def=0.0f) const;
    inline DWORD GetColor(CTSTR lpName, DWORD def=0) const
    {
        return (DWORD)GetInt(lpName, (int)def);
    }
    inline DWORD GetHex(CTSTR lpName, DWORD def=0) const
    {
        return (DWORD)GetInt(lpName, (int)def);
    }

    void  GetStringList(CTSTR lpName, StringList &stringList) const;
    void  GetIntList(CTSTR lpName, List<int> &IntList) const;
    void  GetFloatList(CTSTR lpName, List<float> &FloatList) const;
    inline void GetColorList(CTSTR lpName, List<DWORD> &ColorList) const
    {
        GetIntList(lpName, *(List<int>*)&ColorList);
    }
    inline void GetHexList(CTSTR lpName, List<DWORD> &HexList) const
    {
        GetIntList(lpName, *(List<int>*)&HexList);
    }

    void  SetString(CTSTR lpName, CTSTR lpString);
    void  SetInt(CTSTR lpName, int number);
    void  SetFloat(CTSTR lpName, float number);
    void  SetHex(CTSTR lpName, DWORD hex);
    inline void SetColor(CTSTR lpName, DWORD color)
    {
        SetHex(lpName, color);
    }

    void  SetStringList(CTSTR lpName, List<TSTR> &stringList);
    void  SetStringList(CTSTR lpName, StringList &stringList);
    void  SetIntList(CTSTR lpName, List<int> &IntList);
    void  SetFloatList(CTSTR lpName, List<float> &FloatList);
    void  SetHexList(CTSTR lpName, List<DWORD> &HexList);
    inline void SetColorList(CTSTR lpName, List<DWORD> &ColorList)
    {
        SetHexList(lpName, ColorList);
    }

    void  AddString(CTSTR lpName, TSTR lpString);
    void  AddInt(CTSTR lpName, int number);
    void  AddFloat(CTSTR lpName, float number);
    void  AddHex(CTSTR lpName, DWORD hex);
    inline void AddColor(CTSTR lpName, DWORD color)
    {
        AddHex(lpName, color);
    }

    void  AddStringList(CTSTR lpName, List<TSTR> &stringList);
    void  AddStringList(CTSTR lpName, StringList &stringList);
    void  AddIntList(CTSTR lpName, List<int> &IntList);
    void  AddFloatList(CTSTR lpName, List<float> &FloatList);
    void  AddHexList(CTSTR lpName, List<DWORD> &HexList);
    inline void AddColorList(CTSTR lpName, List<DWORD> &ColorList)
    {
        AddHexList(lpName, ColorList);
    }

    void  RemoveItem(CTSTR lpName);

    //----------------

    bool Import(CTSTR lpFile);
    bool Export(CTSTR lpFile);

    XElement* GetElement(CTSTR lpName) const;
    XElement* GetElementByID(DWORD elementID) const;
    XElement* GetElementByItem(CTSTR lpName, CTSTR lpItemName, CTSTR lpItemValue) const;
    XElement* CreateElement(CTSTR lpName);
    XElement* InsertElement(UINT pos, CTSTR lpName);
    XElement* CopyElement(XElement* element, CTSTR lpNewName);
    void  GetElementList(CTSTR lpName, List<XElement*> &Elements) const;
    void  RemoveElement(XElement *element);
    void  RemoveElement(CTSTR lpName);

    inline XElement* GetParent() const {return parent;}

    XBaseItem* GetBaseItem(CTSTR lpName) const;
    XBaseItem* GetBaseItemByID(DWORD itemID) const;

    XDataItem* GetDataItem(CTSTR lpName) const;
    XDataItem* GetDataItemByID(DWORD itemID) const;

    DWORD NumElements(CTSTR lpName=NULL);
    DWORD NumBaseItems(CTSTR lpName=NULL);
    DWORD NumDataItems(CTSTR lpName=NULL);

    void MoveUp();
    void MoveDown();
    void MoveToTop();
    void MoveToBottom();
};


class BASE_EXPORT XConfig
{
    friend class XElement;

    XElement *RootElement;
    String strFileName;

    bool ReadFileData(XElement *curElement, int level, TSTR &lpFileData);
    bool WriteFileData(XFile &file, int indent, XElement *curElement);
    bool WriteFileItem(XFile &file, int indent, XBaseItem *curItem);

    static String ConvertToTextString(String &string);
    static String ProcessString(TSTR &lpTemp);

    bool ReadFileData2(XElement *curElement, int level, TSTR &lpFileData, bool isJSON);

public:
    inline XConfig() : RootElement(NULL) {}
    inline XConfig(TSTR lpFile) : RootElement(NULL) {Open(lpFile);}

    inline ~XConfig() {Close();}

    bool    Open(CTSTR lpFile);
    bool    ParseString(const String& config);
    void    Close(bool bSave=false);
    void    Save();
    void    SaveTo(CTSTR lpPath);

    inline bool IsOpen() const {return RootElement != NULL;}

    inline XElement *GetRootElement() {return RootElement;}
    inline XElement *GetElement(CTSTR lpName) {return RootElement->GetElement(lpName);}
    inline XElement *GetElementByID(DWORD elementID) {return RootElement->GetElementByID(elementID);}
    inline XElement *CreateElement(CTSTR lpName) {return RootElement->CreateElement(lpName);}
};

