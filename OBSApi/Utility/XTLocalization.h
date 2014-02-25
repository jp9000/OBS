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


//------------------------------------------------------------------
// Localization structures
//------------------------------------------------------------------

struct LocaleStringItem
{
    String      lookup;
    String      strValue;
};

struct LocaleStringCache : public List<LocaleStringItem*>
{
    inline ~LocaleStringCache()
    {
        for(unsigned int i=0; i<Num(); i++)
            delete array[i];
    }

    inline void Clear()
    {
        for(unsigned int i=0; i<Num(); i++)
            delete array[i];
        List<LocaleStringItem*>::Clear();
    }

    inline void Remove(UINT i)
    {
        if(i < Num())
        {
            delete array[i];
            List<LocaleStringItem*>::Remove(i);
        }
    }

    inline void SetSize(UINT newSize)
    {
        if(newSize < Num())
        {
            for(unsigned int i=newSize; i<Num(); i++)
                delete array[i];
        }

        List<LocaleStringItem*>::SetSize(newSize);
    }
};


struct StringLookupNode;


//------------------------------------------------------------------
// Localization String Lookup Class
//------------------------------------------------------------------

class BASE_EXPORT LocaleStringLookup
{
    StringLookupNode *top;
    LocaleStringCache cache;

    void AddLookup(CTSTR lookupVal, LocaleStringItem *item, StringLookupNode *node=NULL);
    void RemoveLookup(CTSTR lookupVal, StringLookupNode *node=NULL);

    StringLookupNode* FindNode(CTSTR lookupVal, StringLookupNode *node=NULL) const;

public:
    LocaleStringLookup();
    ~LocaleStringLookup();

    BOOL LoadStringFile(CTSTR lpFile, bool bClear=false);

    inline BOOL HasLookup(CTSTR lookupVal) const {return (FindNode(lookupVal) != NULL);}

    void AddLookupString(CTSTR lookupVal, CTSTR lpVal);
    inline void RemoveLookupString(CTSTR lookupVal) {RemoveLookup(lookupVal);}

    CTSTR LookupString(CTSTR lookupVal);

    const LocaleStringCache& GetCache() const {return cache;}
};


BASE_EXPORT extern LocaleStringLookup *locale;

#define Str(text) locale->LookupString(TEXT2(text))

inline BOOL  LoadStringFile(CTSTR lpResource)   {return locale->LoadStringFile(lpResource);}

#ifdef UNICODE
struct LocaleNativeName
{
    CTSTR lpCode, lpNative;
};

BASE_EXPORT LocaleNativeName* STDCALL GetLocaleNativeName(CTSTR lpCode);

BASE_EXPORT bool LocaleIsRTL(LocaleStringLookup *l = locale);
#endif
