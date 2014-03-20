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

/*=========================================================
    Config
===========================================================*/

struct ConfigKey
{
    TSTR name;
    List<TSTR> ValueList;
};

struct ConfigSection
{
    TSTR name;
    List<ConfigKey> Keys;
};


class BASE_EXPORT ConfigFile
{
public:
    ConfigFile() : bOpen(0), strFileName(), lpFileData(NULL), dwLength(0) {}
    ~ConfigFile() {Close();}

    BOOL  Create(CTSTR lpConfigFile);
    BOOL  Open(CTSTR lpConfigFile, BOOL bOpenAlways=FALSE);
    void  Close();

    BOOL  SaveAs(CTSTR lpPath);
    inline CTSTR GetFilePath() const {return strFileName;}
    void  SetFilePath(CTSTR lpPath);

    String GetString(CTSTR lpSection, CTSTR lpKey, CTSTR def=NULL);
    CTSTR GetStringPtr(CTSTR lpSection, CTSTR lpKey, CTSTR def=NULL);
    int   GetInt(CTSTR lpSection, CTSTR lpKey, int def=0);
    DWORD GetHex(CTSTR lpSection, CTSTR lpKey, DWORD def=0);
    float GetFloat(CTSTR lpSection, CTSTR lpKey, float def=0.0f);
    Color4 GetColor(CTSTR lpSection, CTSTR lpKey);

    BOOL  GetStringList(CTSTR lpSection, CTSTR lpKey, StringList &StrList);
    BOOL  GetIntList(CTSTR lpSection, CTSTR lpKey, List<int> &IntList);
    BOOL  GetFloatList(CTSTR lpSection, CTSTR lpKey, List<float> &FloatList);
    BOOL  GetColorList(CTSTR lpSection, CTSTR lpKey, List<Color4> &ColorList);

    void  SetString(CTSTR lpSection, CTSTR lpKey, CTSTR lpString);
    void  SetInt(CTSTR lpSection, CTSTR lpKey, int number);
    void  SetHex(CTSTR lpSection, CTSTR lpKey, DWORD number);
    void  SetFloat(CTSTR lpSection, CTSTR lpKey, float number);
    void  SetColor(CTSTR lpSection, CTSTR lpKey, const Color4 &color);

    void  SetStringList(CTSTR lpSection, CTSTR lpKey, StringList &StrList);
    void  SetIntList(CTSTR lpSection, CTSTR lpKey, List<int> &IntList);
    void  SetFloatList(CTSTR lpSection, CTSTR lpKey, List<float> &FloatList);
    void  SetColorList(CTSTR lpSection, CTSTR lpKey, List<Color4> &ColorList);

    void  AddString(CTSTR lpSection, CTSTR lpKey, CTSTR lpString);
    void  AddInt(CTSTR lpSection, CTSTR lpKey, int number);
    void  AddFloat(CTSTR lpSection, CTSTR lpKey, float number);
    void  AddColor(CTSTR lpSection, CTSTR lpKey, const Color4 &color);

    BOOL  HasKey(CTSTR lpSection, CTSTR lpKey);

    void  Remove(CTSTR lpSection, CTSTR lpKey);

    inline Color3 GetColor3(CTSTR lpSection, CTSTR lpKey)
    {
        return Color3(GetColor(lpSection, lpKey));
    }

    inline void SetColor3(CTSTR lpSection, CTSTR lpKey, const Color3 &color)
    {
        return SetColor(lpSection, lpKey, Color4(color));
    }


private:
    BOOL  LoadFile(DWORD dwOpenMode);
    void  LoadData();
    void  SetKey(CTSTR lpSection, CTSTR lpKey, CTSTR newvalue);
    void  AddKey(CTSTR lpSection, CTSTR lpKey, CTSTR newvalue);

    List<ConfigSection> Sections;

    BOOL  bOpen;
    String strFileName;
    TSTR lpFileData;
    DWORD dwLength;
};
