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


#include "XT.h"




/*========================================================
  XElement
=========================================================*/

XElement::~XElement()
{
    DWORD i;

    for(i=0; i<SubItems.Num(); i++)
        delete SubItems[i];
    SubItems.Clear();
}

CTSTR XElement::GetString(CTSTR lpName, TSTR def) const
{
    assert(lpName);

    XDataItem *item = GetDataItem(lpName);
    if(item) return item->strData;

    return def;
}

int   XElement::GetInt(CTSTR lpName, int def) const
{
    assert(lpName);

    XDataItem *item = GetDataItem(lpName);
    if(item)
    {
        CTSTR lpValue = item->strData;

        if( (*LPWORD(lpValue) == 'x0') ||
            (*LPWORD(lpValue) == 'X0') )
        {
            return tstring_base_to_uint(lpValue+2, NULL, 16);
        }
        else if(scmpi(lpValue, TEXT("true")) == 0)
            return 1;
        else if(scmpi(lpValue, TEXT("false")) == 0)
            return 0;
        else
            return tstring_base_to_uint(lpValue, NULL, 0);
    }

    return def;
}

float XElement::GetFloat(CTSTR lpName, float def) const
{
    assert(lpName);

    XDataItem *item = GetDataItem(lpName);
    if(item)
        return (float)tstof(item->strData);

    return def;
}


void  XElement::GetStringList(CTSTR lpName, StringList &stringList) const
{
    assert(lpName);

    stringList.Clear();

    for(DWORD i=0; i<SubItems.Num(); i++)
    {
        if(!SubItems[i]->IsData()) continue;

        XDataItem *item = static_cast<XDataItem*>(SubItems[i]);
        if(item->strName.CompareI(lpName))
            stringList << item->strData;
    }
}

void  XElement::GetIntList(CTSTR lpName, List<int> &IntList) const
{
    assert(lpName);

    IntList.Clear();

    for(DWORD i=0; i<SubItems.Num(); i++)
    {
        if(!SubItems[i]->IsData()) continue;

        XDataItem *item = static_cast<XDataItem*>(SubItems[i]);
        if(item->strName.CompareI(lpName))
        {
            CTSTR lpValue = item->strData;

            if( (*LPWORD(lpValue) == 'x0') ||
                (*LPWORD(lpValue) == 'X0') )
            {
                IntList << tstring_base_to_uint(lpValue+2, NULL, 16);
            }
            else if(scmpi(lpValue, TEXT("true")) == 0)
                IntList << 1;
            else if(scmpi(lpValue, TEXT("false")) == 0)
                IntList << 0;
            else
                IntList << tstring_base_to_uint(lpValue, NULL, 0);
        }
    }
}


void  XElement::SetString(CTSTR lpName, CTSTR lpString)
{
    assert(lpName);

    if(!lpString) lpString = TEXT("");

    XDataItem *item = GetDataItem(lpName);
    if(item)
    {
        item->strData = lpString;
        return;
    }

    SubItems << new XDataItem(lpName, lpString);
}

void  XElement::SetInt(CTSTR lpName, int number)
{
    assert(lpName);

    String intStr = IntString(number);

    XDataItem *item = GetDataItem(lpName);
    if(item)
    {
        item->strData = intStr;
        return;
    }

    SubItems << new XDataItem(lpName, intStr);
}

void  XElement::SetFloat(CTSTR lpName, float number)
{
    assert(lpName);

    String floatStr = FloatString(number);

    XDataItem *item = GetDataItem(lpName);
    if(item)
    {
        item->strData = floatStr;
        return;
    }

    SubItems << new XDataItem(lpName, floatStr);
}

void  XElement::SetHex(CTSTR lpName, DWORD hex)
{
    assert(lpName);

    String hexStr;
    hexStr << TEXT("0x") << IntString(hex, 16);

    XDataItem *item = GetDataItem(lpName);
    if(item)
    {
        item->strData = hexStr;
        return;
    }

    SubItems << new XDataItem(lpName, hexStr);
}


void  XElement::SetStringList(CTSTR lpName, List<TSTR> &stringList)
{
    RemoveItem(lpName);
    AddStringList(lpName, stringList);
}

void  XElement::SetStringList(CTSTR lpName, StringList &stringList)
{
    RemoveItem(lpName);
    AddStringList(lpName, stringList);
}

void  XElement::SetIntList(CTSTR lpName, List<int> &IntList)
{
    RemoveItem(lpName);
    AddIntList(lpName, IntList);
}

void  XElement::SetFloatList(CTSTR lpName, List<float> &FloatList)
{
    RemoveItem(lpName);
    AddFloatList(lpName, FloatList);
}

void  XElement::SetHexList(CTSTR lpName, List<DWORD> &HexList)
{
    RemoveItem(lpName);
    AddHexList(lpName, HexList);
}


void  XElement::AddString(CTSTR lpName, TSTR lpString)
{
    assert(lpName);

    if(!lpString) lpString = TEXT("");

    SubItems << new XDataItem(lpName, lpString);
}

void  XElement::AddInt(CTSTR lpName, int number)
{
    assert(lpName);

    SubItems << new XDataItem(lpName, IntString(number));
}

void  XElement::AddFloat(CTSTR lpName, float number)
{
    assert(lpName);

    SubItems << new XDataItem(lpName, FloatString(number));
}

void  XElement::AddHex(CTSTR lpName, DWORD hex)
{
    assert(lpName);

    String hexStr;
    hexStr << TEXT("0x") << IntString(hex, 16);

    SubItems << new XDataItem(lpName, hexStr);
}


void  XElement::AddStringList(CTSTR lpName, List<TSTR> &stringList)
{
    assert(lpName);

    for(DWORD i=0; i<stringList.Num(); i++)
    {
        assert(stringList[i]);

        AddString(lpName, stringList[i]);
    }
}

void  XElement::AddStringList(CTSTR lpName, StringList &stringList)
{
    assert(lpName);

    for(DWORD i=0; i<stringList.Num(); i++)
    {
        assert(stringList[i]);

        AddString(lpName, stringList[i]);
    }
}

void  XElement::AddIntList(CTSTR lpName, List<int> &IntList)
{
    assert(lpName);

    for(DWORD i=0; i<IntList.Num(); i++)
        AddInt(lpName, IntList[i]);
}

void  XElement::AddFloatList(CTSTR lpName, List<float> &FloatList)
{
    assert(lpName);

    for(DWORD i=0; i<FloatList.Num(); i++)
        AddFloat(lpName, FloatList[i]);
}

void  XElement::AddHexList(CTSTR lpName, List<DWORD> &HexList)
{
    assert(lpName);

    for(DWORD i=0; i<HexList.Num(); i++)
        AddHex(lpName, HexList[i]);
}


void  XElement::RemoveItem(CTSTR lpName)
{
    assert(lpName);

    if(lpName)
    {
        for(DWORD i=0; i<SubItems.Num(); i++)
        {
            if(!SubItems[i]->IsData()) continue;

            if(static_cast<XDataItem*>(SubItems[i])->strName.CompareI(lpName))
            {
                delete SubItems[i];
                SubItems.Remove(i--);
            }
        }
    }
    else
    {
        for(DWORD i=0; i<SubItems.Num(); i++)
        {
            if(!SubItems[i]->IsData()) continue;

            delete SubItems[i];
            SubItems.Remove(i--);
        }
    }
}

//---------------------------

XElement* XElement::GetElement(CTSTR lpName) const
{
    assert(lpName);

    for(DWORD i=0; i<SubItems.Num(); i++)
    {
        if(!SubItems[i]->IsElement()) continue;

        XElement *element = static_cast<XElement*>(SubItems[i]);
        if(element->strName.CompareI(lpName))
            return element;
    }

    return NULL;
}

XElement* XElement::GetElementByID(DWORD elementID) const
{
    int id=0;

    for(DWORD i=0; i<SubItems.Num(); i++)
    {
        if(!SubItems[i]->IsElement()) continue;

        if(id == elementID)
            return static_cast<XElement*>(SubItems[i]);
        ++id;
    }

    return NULL;
}

XElement* XElement::GetElementByItem(CTSTR lpName, CTSTR lpItemName, CTSTR lpItemValue) const
{
    assert(lpItemName);
    assert(lpItemValue);

    if(lpName)
    {
        for(DWORD i=0; i<SubItems.Num(); i++)
        {
            if(!SubItems[i]->IsElement()) continue;

            XElement *element = static_cast<XElement*>(SubItems[i]);
            if(element->strName.CompareI(lpName))
            {
                if(scmpi(element->GetString(lpItemName), lpItemValue) == 0)
                    return element;
            }
        }
    }
    else
    {
        for(DWORD i=0; i<SubItems.Num(); i++)
        {
            if(!SubItems[i]->IsElement()) continue;

            XElement *element = static_cast<XElement*>(SubItems[i]);
            if(scmpi(element->GetString(lpItemName), lpItemValue) == 0)
                return element;
        }
    }

    return NULL;
}

XElement* XElement::CreateElement(CTSTR lpName)
{
    assert(lpName);

    XElement *newElement = new XElement(file, this, lpName);

    SubItems << newElement;

    return newElement;
}

void  XElement::GetElementList(CTSTR lpName, List<XElement*> &Elements) const
{
    Elements.Clear();

    for(DWORD i=0; i<SubItems.Num(); i++)
    {
        if(!SubItems[i]->IsElement()) continue;

        XElement *element = static_cast<XElement*>(SubItems[i]);
        if(!lpName || element->strName.CompareI(lpName))
            Elements << element;
    }
}

void  XElement::RemoveElement(XElement *element)
{
    assert(element);

    for(DWORD i=0; i<SubItems.Num(); i++)
    {
        if(SubItems[i] == element)
        {
            SubItems.Remove(i);
            delete element;
            break;
        }
    }
}

void  XElement::RemoveElement(CTSTR lpName)
{
    assert(lpName);

    for(DWORD i=0; i<SubItems.Num(); i++)
    {
        if(!SubItems[i]->IsElement()) continue;

        XElement *element = static_cast<XElement*>(SubItems[i]);
        if(element->strName.CompareI(lpName))
        {
            delete element;
            SubItems.Remove(i--);
        }
    }
}


XDataItem* XElement::GetDataItem(CTSTR lpName) const
{
    for(DWORD i=0; i<SubItems.Num(); i++)
    {
        if(!SubItems[i]->IsData()) continue;

        XDataItem *data = static_cast<XDataItem*>(SubItems[i]);
        if(data->strName.CompareI(lpName))
            return data;
    }

    return NULL;
}

XDataItem* XElement::GetDataItemByID(DWORD itemID) const
{
    int id=0;

    for(DWORD i=0; i<SubItems.Num(); i++)
    {
        if(!SubItems[i]->IsData()) continue;

        if(id == itemID)
            return static_cast<XDataItem*>(SubItems[i]);
        ++id;
    }

    return NULL;
}


XBaseItem* XElement::GetBaseItem(CTSTR lpName) const
{
    for(DWORD i=0; i<SubItems.Num(); i++)
    {
        if(SubItems[i]->strName.CompareI(lpName))
            return SubItems[i];
    }

    return NULL;
}

XBaseItem* XElement::GetBaseItemByID(DWORD itemID) const
{
    if(itemID >= SubItems.Num())
        return NULL;

    return SubItems[itemID];
}


DWORD XElement::NumElements(CTSTR lpName)
{
    int num=0;

    for(DWORD i=0; i<SubItems.Num(); i++)
    {
        if(!SubItems[i]->IsElement()) continue;

        XElement *element = static_cast<XElement*>(SubItems[i]);
        if(!lpName || element->strName.CompareI(lpName))
            ++num;
    }

    return num;
}

DWORD XElement::NumDataItems(CTSTR lpName)
{
    int num=0;

    for(DWORD i=0; i<SubItems.Num(); i++)
    {
        if(!SubItems[i]->IsData()) continue;

        XDataItem *data = static_cast<XDataItem*>(SubItems[i]);
        if(!lpName || data->strName.CompareI(lpName))
            ++num;
    }

    return num;
}

DWORD XElement::NumBaseItems(CTSTR lpName)
{
    int num=0;

    for(DWORD i=0; i<SubItems.Num(); i++)
    {
        XDataItem *data = static_cast<XDataItem*>(SubItems[i]);
        if(!lpName || data->strName.CompareI(lpName))
            ++num;
    }

    return num;
}


void XElement::MoveUp()
{
    UINT lastElement = INVALID;
    for(UINT i=0; i<parent->SubItems.Num(); i++)
    {
        XBaseItem *baseItem = parent->SubItems[i];
        if(baseItem->GetType() == XConfig_Element)
        {
            if(baseItem == this)
            {
                if(lastElement != INVALID)
                    parent->SubItems.SwapValues(lastElement, i);

                break;
            }
            else
                lastElement = i;
        }
    }
}

void XElement::MoveDown()
{
    UINT lastElement = INVALID;
    for(int i=int(parent->SubItems.Num()-1); i>=0; i--)
    {
        XBaseItem *baseItem = parent->SubItems[(UINT)i];
        if(baseItem->GetType() == XConfig_Element)
        {
            if(baseItem == this)
            {
                if(lastElement != INVALID)
                    parent->SubItems.SwapValues(lastElement, (UINT)i);

                break;
            }
            else
                lastElement = (UINT)i;
        }
    }
}

void XElement::MoveToTop()
{
    XElement *thisItem = this;
    parent->SubItems.RemoveItem(thisItem);
    parent->SubItems.Insert(0, thisItem);
}

void XElement::MoveToBottom()
{
    XElement *thisItem = this;
    parent->SubItems.RemoveItem(thisItem);
    parent->SubItems.Add(thisItem);
}

bool XElement::Import(CTSTR lpFile)
{
    XFile importFile;
    if(!importFile.Open(lpFile, XFILE_READ, XFILE_OPENEXISTING))
        return false;

    String strFileData;
    importFile.ReadFileToString(strFileData);

    TSTR lpFileData = strFileData;
    file->ReadFileData(this, lpFileData);

    return true;
}

bool XElement::Export(CTSTR lpFile)
{
    XFile exportFile;
    if(!exportFile.Open(lpFile, XFILE_WRITE, XFILE_CREATEALWAYS))
        return false;

    file->WriteFileItem(exportFile, 0, this);

    return true;
}


/*========================================================
  XConfig
=========================================================*/

String XConfig::ConvertToTextString(String &string)
{
    String stringOut = string;
    stringOut.FindReplace(TEXT("\\"), TEXT("\\\\"));
    stringOut.FindReplace(TEXT("\r"), TEXT("\\r"));
    stringOut.FindReplace(TEXT("\n"), TEXT("\\n"));
    stringOut.FindReplace(TEXT("\""), TEXT("\\\""));
    stringOut.FindReplace(TEXT("\t"), TEXT("\\t"));

    return String() << TEXT("\"") << stringOut << TEXT("\"");
}

String XConfig::ProcessString(TSTR &lpTemp)
{
    TSTR lpStart = lpTemp;

    BOOL bFoundEnd = FALSE;
    while(*++lpTemp)
    {
        if(*lpTemp == '"' && lpTemp[-1] != '\\')
        {
            bFoundEnd = TRUE;
            break;
        }
    }

    if(!bFoundEnd)
        return String();

    ++lpTemp;

    TCHAR backupChar = *lpTemp;
    *lpTemp = 0;
    String string = lpStart;
    *lpTemp = backupChar;

    String stringOut = string.Mid(1, string.Length()-1);

    TSTR lpStringOut = stringOut;
    while(*lpStringOut != 0 && (lpStringOut = schr(lpStringOut, '\\')) != 0)
    {
        switch(lpStringOut[1])
        {
            case 0:     *lpStringOut = 0; break;
            case '"':   *lpStringOut = '"';  scpy(lpStringOut+1, lpStringOut+2); break;
            case 't':   *lpStringOut = '\t'; scpy(lpStringOut+1, lpStringOut+2); break;
            case 'r':   *lpStringOut = '\r'; scpy(lpStringOut+1, lpStringOut+2); break;
            case 'n':   *lpStringOut = '\n'; scpy(lpStringOut+1, lpStringOut+2); break;
            case '\\':  scpy(lpStringOut+1, lpStringOut+2); break;
        }

        lpStringOut++;
    }

    stringOut.SetLength(slen(stringOut));

    return stringOut;
}


bool  XConfig::ReadFileData(XElement *curElement, TSTR &lpTemp)
{
    while(*lpTemp)
    {
        if(*lpTemp == '}')
            return (curElement != RootElement);

        if( *lpTemp != ' '   &&
            *lpTemp != L'　' &&
            *lpTemp != '\t'  &&
            *lpTemp != '\r'  &&
            *lpTemp != '\n'  )
        {
            String strName;

            if(*lpTemp == '"')
                strName = ProcessString(lpTemp);
            else
            {
                TSTR lpDataStart = lpTemp;

                lpTemp = schr(lpTemp, ':');
                if(!lpTemp)
                    return false;

                *lpTemp = 0;
                strName = lpDataStart;
                *lpTemp = ':';

                strName.KillSpaces();
            }

            //---------------------------

            lpTemp = schr(lpTemp, ':');
            if(!lpTemp)
                return false;

            ++lpTemp;

            while( *lpTemp == ' '   ||
                   *lpTemp == L'　' ||
                   *lpTemp == '\t'  )
            {
                ++lpTemp;
            }

            //---------------------------

            if(*lpTemp == '{') //element
            {
                ++lpTemp;

                XElement *newElement = curElement->CreateElement(strName);
                if(!ReadFileData(newElement, lpTemp))
                    return false;
            }
            else //item
            {
                String data;

                if(*lpTemp == '"')
                    data = ProcessString(lpTemp);
                else
                {
                    TSTR lpDataStart = lpTemp;

                    lpTemp = schr(lpTemp, '\n');
                    if(!lpTemp)
                        return false;

                    if(lpTemp[-1] == '\r') --lpTemp;

                    if(lpTemp != lpDataStart)
                    {
                        TCHAR oldChar = *lpTemp;
                        *lpTemp = 0;
                        data = lpDataStart;
                        *lpTemp = oldChar;

                        data.KillSpaces();
                    }
                }

                lpTemp = schr(lpTemp, '\n');
                if(!lpTemp && curElement != RootElement)
                    return false;

                curElement->SubItems << new XDataItem(strName, data);
            }
        }

        ++lpTemp;
    }

    return (curElement == RootElement);
}

void  XConfig::WriteFileItem(XFile &file, int indent, XBaseItem *baseItem)
{
    int j;

    if(baseItem->IsData())
    {
        XDataItem *item = static_cast<XDataItem*>(baseItem);

        String strItem;

        for(j=0; j<indent; j++)
            strItem << TEXT("  ");

        if( item->strName.IsValid()                         && (
            item->strName[0] == ' '                         ||
            item->strName[0] == '\t'                        ||
            item->strName[0] == '{'                         ||
            item->strName[item->strName.Length()-1] == ' '  ||
            item->strName[item->strName.Length()-1] == '\t' ||
            schr(item->strName, '\n')                       ||
            schr(item->strName, '"')                        ||
            schr(item->strName, ':')                        ))
        {
            strItem << ConvertToTextString(item->strName);
        }
        else
            strItem << item->strName;

        strItem << TEXT(" : ");

        if( item->strData.IsValid()                         && (
            item->strData[0] == ' '                         ||
            item->strData[0] == '\t'                        ||
            item->strData[0] == '{'                         ||
            item->strData[item->strData.Length()-1] == ' '  ||
            item->strData[item->strData.Length()-1] == '\t' ||
            schr(item->strData, '\n')                       ||
            schr(item->strData, '"')                        ||
            schr(item->strData, ':')                        ))
        {
            strItem << ConvertToTextString(item->strData);
        }
        else
            strItem << item->strData;

        strItem << TEXT("\r\n");

        file.WriteAsUTF8(strItem);
    }
    else if(baseItem->IsElement())
    {
        XElement *element = static_cast<XElement*>(baseItem);

        String strElement;

        for(j=0; j<indent; j++)
            strElement << TEXT("  ");

        if( element->strName.IsValid()                            && (
            element->strName[0] == ' '                            ||
            element->strName[0] == '\t'                           ||
            element->strName[0] == '{'                            ||
            element->strName[element->strName.Length()-1] == ' '  ||
            element->strName[element->strName.Length()-1] == '\t' ||
            schr(element->strName, '\n')                          ||
            schr(element->strName, '"')                           ||
            schr(element->strName, ':')                           ))
        {
            strElement << ConvertToTextString(element->strName);
        }
        else
            strElement << element->strName;

        strElement << TEXT(" : {\r\n");

        file.WriteAsUTF8(strElement);

        strElement.Clear();

        WriteFileData(file, indent+1, element);

        for(j=0; j<indent; j++)
            strElement << TEXT("  ");
        strElement << TEXT("}\r\n");

        file.WriteAsUTF8(strElement);
    }
}

void  XConfig::WriteFileData(XFile &file, int indent, XElement *curElement)
{
    DWORD i;

    for(i=0; i<curElement->SubItems.Num(); i++)
    {
        XBaseItem *baseItem = curElement->SubItems[i];
        WriteFileItem(file, indent, baseItem);
    }
}


bool  XConfig::Open(CTSTR lpFile)
{
    if(RootElement)
    {
        if(strFileName.CompareI(lpFile))
            return true;

        Close();
    }

    //-------------------------------------
    XFile file;

    if(!file.Open(lpFile, XFILE_READ, XFILE_OPENALWAYS))
        return false;

    RootElement = new XElement(this, NULL, TEXT("Root"));
    strFileName = lpFile;

    DWORD dwFileSize = (DWORD)file.GetFileSize();

    LPSTR lpFileDataUTF8 = (LPSTR)Allocate(dwFileSize+1);
    zero(lpFileDataUTF8, dwFileSize+1);
    file.Read(lpFileDataUTF8, dwFileSize);

    TSTR lpFileData = utf8_createTstr(lpFileDataUTF8);
    Free(lpFileDataUTF8);

    //-------------------------------------
    // remove comments

    TSTR lpComment, lpEndComment;

    while(lpComment = sstr(lpFileData, TEXT("/*")))
    {
        lpEndComment = sstr(lpFileData, TEXT("*/"));

        assert(lpEndComment);
        assert(lpComment < lpEndComment);

        if(!lpEndComment || (lpComment > lpEndComment))
        {
            file.Close();

            Close(false);
            Free(lpFileData);

            CrashError(TEXT("Error parsing X file '%s'"), strFileName.Array());
        }

        mcpy(lpComment, lpEndComment+3, slen(lpEndComment+3)+1);
    }

    //-------------------------------------

    TSTR lpTemp = lpFileData;
    if(!ReadFileData(RootElement, lpTemp))
    {
        for(DWORD i=0; i<RootElement->SubItems.Num(); i++)
            delete RootElement->SubItems[i];

        CrashError(TEXT("Error parsing X file '%s'"), strFileName.Array());

        Free(lpFileData);
        Close(false);
        file.Close();
    }

    Free(lpFileData);

    file.Close();

    return true;
}

void  XConfig::Close(bool bSave)
{
    if(RootElement && bSave)
    {
        XFile file;
        file.Open(strFileName, XFILE_WRITE, XFILE_CREATEALWAYS);

        WriteFileData(file, 0, RootElement);

        file.Close();
    }

    delete RootElement;
    RootElement = NULL;

    strFileName.Clear();
}
