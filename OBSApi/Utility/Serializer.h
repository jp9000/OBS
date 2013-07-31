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

#define SERIALIZE_SEEK_START    0
#define SERIALIZE_SEEK_CURRENT  1
#define SERIALIZE_SEEK_END      2


class BASE_EXPORT Serializer
{
public:
    Serializer()                        {}
    virtual ~Serializer()               {}

    virtual BOOL IsLoading()=0;

    virtual void Serialize(LPCVOID lpData, DWORD length)=0;

    virtual UINT64 Seek(INT64 offset, DWORD seekType=SERIALIZE_SEEK_START)=0;

    virtual UINT64 GetPos() const=0;

    virtual BOOL DataPending() {return FALSE;}

    friend inline Serializer& operator<<(Serializer &s, BYTE &b)       {s.Serialize(&b, 1);  return s;}
    friend inline Serializer& operator<<(Serializer &s, char &b)       {s.Serialize(&b, 1);  return s;}

    friend inline Serializer& operator<<(Serializer &s, WORD  &w)      {s.Serialize(&w, 2);  return s;}
    friend inline Serializer& operator<<(Serializer &s, short &w)      {s.Serialize(&w, 2);  return s;}

    friend inline Serializer& operator<<(Serializer &s, DWORD &dw)     {s.Serialize(&dw, 4); return s;}
    friend inline Serializer& operator<<(Serializer &s, LONG  &dw)     {s.Serialize(&dw, 4); return s;}

    friend inline Serializer& operator<<(Serializer &s, UINT  &dw)     {s.Serialize(&dw, 4); return s;}
    friend inline Serializer& operator<<(Serializer &s, int   &dw)     {s.Serialize(&dw, 4); return s;}

    friend inline Serializer& operator<<(Serializer &s, bool  &bv)     {BOOL bVal = (BOOL)bv; s.Serialize(&bVal, 4); return s;}

    friend inline Serializer& operator<<(Serializer &s, LONG64 &qw)    {s.Serialize(&qw, 8); return s;}
    friend inline Serializer& operator<<(Serializer &s, QWORD &qw)     {s.Serialize(&qw, 8); return s;}

    friend inline Serializer& operator<<(Serializer &s, float &f)      {s.Serialize(&f, 4);  return s;}
    friend inline Serializer& operator<<(Serializer &s, double &d)     {s.Serialize(&d, 8);  return s;}

    inline Serializer& OutputByte (BYTE  cVal)       {if(!IsLoading()) Serialize(&cVal,  1); return *this;}
    inline Serializer& OutputChar (char  cVal)       {if(!IsLoading()) Serialize(&cVal,  1); return *this;}
    inline Serializer& OutputWord (WORD  sVal)       {if(!IsLoading()) Serialize(&sVal,  2); return *this;}
    inline Serializer& OutputShort(short sVal)       {if(!IsLoading()) Serialize(&sVal,  2); return *this;}
    inline Serializer& OutputDword(DWORD lVal)       {if(!IsLoading()) Serialize(&lVal,  4); return *this;}
    inline Serializer& OutputLong (long  lVal)       {if(!IsLoading()) Serialize(&lVal,  4); return *this;}
    inline Serializer& OutputQword(QWORD qVal)       {if(!IsLoading()) Serialize(&qVal,  8); return *this;}
    inline Serializer& OutputInt64(INT64 qVal)       {if(!IsLoading()) Serialize(&qVal,  8); return *this;}

    inline Serializer& OutputFloat(float fVal)       {if(!IsLoading()) Serialize(&fVal,  4); return *this;}
    inline Serializer& OutputDouble(double dVal)     {if(!IsLoading()) Serialize(&dVal,  8); return *this;}
};
