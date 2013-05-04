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

//disable this warning just for this file
#pragma warning(disable : 4035)



#ifndef USE_CUSTOM_MEMORY_FUNCTIONS

#define mcpy memcpy
/*inline void STDCALL mcpy(void *pDest, const void *pSrc, size_t iLen)
{
    memcpy(pDest, pSrc, iLen);
}*/

#endif


#ifdef C64


#ifdef USE_CUSTOM_MEMORY_FUNCTIONS

inline void STDCALL mcpy(void *pDest, const void *pSrc, size_t iLen)
{
    assert(pDest);
    assert(pSrc);

    register size_t iLenMod8 = iLen&7;
    register size_t iLenDiv8 = iLen>>3;

    register QWORD *srcQW = (QWORD*)pSrc, *destQW = (QWORD*)pDest;
    while(iLenDiv8--)
        *(destQW++) = *(srcQW++);

    register BYTE *srcB = (BYTE*)srcQW, *destB = (BYTE*)destQW;
    while(iLenMod8--)
        *(destB++) = *(srcB++);
}

#endif

inline BOOL STDCALL mcmp(const void *pDest, const void *pSrc, size_t iLen)
{
    assert(pDest);
    assert(pSrc);

    register size_t iLenMod8 = iLen&7;
    register size_t iLenDiv8 = iLen>>3;

    register QWORD *srcQW = (QWORD*)pSrc, *destQW = (QWORD*)pDest;
    while(iLenDiv8--)
    {
        if(*(srcQW++) != *(destQW++))
            return FALSE;
    }

    register BYTE *srcB = (BYTE*)srcQW, *destB = (BYTE*)destQW;
    while(iLenMod8--)
    {
        if(*(srcB++) != *(destB++))
            return FALSE;
    }

    return TRUE;
}

inline void STDCALL zero(void *pDest, size_t iLen)
{
    register size_t iLenMod8 = iLen&7;
    register size_t iLenDiv8 = iLen>>3;

    register QWORD *destQW = (QWORD*)pDest;
    while(iLenDiv8--)
        *(destQW++) = 0;

    register BYTE *destB = (BYTE*)destQW;
    while(iLenMod8--)
        *(destB++) = 0;
}

inline void STDCALL msetd(void *pDest, DWORD val, size_t iLen)
{
    assert(pDest);

    QWORD newVal = ((QWORD)val)|(((QWORD)val)<<32);

    register size_t iLenMod8 = iLen&7;
    register size_t iLenDiv8 = iLen>>3;

    register QWORD *destQW = (QWORD*)pDest;
    while(iLenDiv8--)
        *(destQW++) = newVal;

    register BYTE *destB = (BYTE*)destQW;
    register BYTE *pVal = (BYTE*)&newVal;
    while(iLenMod8--)
        *(destB++) = *(pVal++);
}

inline void STDCALL mswap(void *pDest, void *pSrc, size_t iLen)
{
    assert(pDest);
    assert(pSrc);

    register size_t iLenMod8 = iLen&7;
    register size_t iLenDiv8 = iLen>>3;

    register QWORD *srcQW = (QWORD*)pSrc, *destQW = (QWORD*)pDest;
    while(iLenDiv8--)
    {
        QWORD qw = *destQW;
        *(destQW++) = *srcQW;
        *(srcQW++) = qw;
    }

    register BYTE *srcB = (BYTE*)srcQW, *destB = (BYTE*)destQW;
    while(iLenMod8--)
    {
        BYTE b = *destB;
        *(destB++) = *srcB;
        *(srcB++) = b;
    }
}

inline void STDCALL mcpyrev(void *pDest, const void *pSrc, size_t iLen)
{
    assert(pDest);
    assert(pSrc);

    register size_t iLenMod8 = iLen&7;
    register size_t iLenDiv8 = iLen>>3;

    register BYTE *srcB = (BYTE*)pSrc, *destB = (BYTE*)pDest;
    register QWORD *srcQW = (QWORD*)(srcB+iLen), *destQW = (QWORD*)(destB+iLen);
    while(iLenDiv8--)
        *(--destQW) = *(--srcQW);

    srcB = (BYTE*)srcQW;
    destB = (BYTE*)destQW;
    while(iLenMod8--)
        *(--destB) = *(--srcB);
}

inline unsigned int PtrTo32(void *ptr)
{
    return ((unsigned int)((unsigned long long)ptr));
}

#else

#pragma warning(disable : 4311)

inline unsigned int PtrTo32(void *ptr)
{
    return ((unsigned int)ptr);
}

#pragma warning(default : 4311)

#ifdef USE_CUSTOM_MEMORY_FUNCTIONS
inline void STDCALL mcpy(void *pDest, const void *pSrc, size_t iLen)
{
    assert(pDest);
    assert(pSrc);

    register size_t iLenMod4 = iLen&3;
    register size_t iLenDiv4 = iLen>>2;

    register DWORD *srcDW = (DWORD*)pSrc, *destDW = (DWORD*)pDest;
    while(iLenDiv4--)
        *(destDW++) = *(srcDW++);

    register BYTE *srcB = (BYTE*)srcDW, *destB = (BYTE*)destDW;
    while(iLenMod4--)
        *(destB++) = *(srcB++);
}
#endif

inline void STDCALL zero(void *pDest, size_t iLen)
{
    assert(pDest);

    register size_t iLenMod4 = iLen&3;
    register size_t iLenDiv4 = iLen>>2;

    register DWORD *destDW = (DWORD*)pDest;
    while(iLenDiv4--)
        *(destDW++) = 0;

    register BYTE *destB = (BYTE*)destDW;
    while(iLenMod4--)
        *(destB++) = 0;
}

inline BOOL STDCALL mcmp(const void *pDest, const void *pSrc, size_t iLen)
{
    assert(pDest);
    assert(pSrc);

    register size_t iLenMod4 = iLen&3;
    register size_t iLenDiv4 = iLen>>2;

    register DWORD *srcDW = (DWORD*)pSrc, *destDW = (DWORD*)pDest;
    while(iLenDiv4--)
    {
        if(*(srcDW++) != *(destDW++))
            return FALSE;
    }

    register BYTE *srcB = (BYTE*)srcDW, *destB = (BYTE*)destDW;
    while(iLenMod4--)
    {
        if(*(srcB++) != *(destB++))
            return FALSE;
    }

    return TRUE;
}

inline void STDCALL msetd(void *pDest, DWORD val, size_t iLen)
{
    assert(pDest);

    register size_t iLenMod4 = iLen&3;
    register size_t iLenDiv4 = iLen>>2;

    register DWORD *destDW = (DWORD*)pDest;
    while(iLenDiv4--)
        *(destDW++) = val;

    register BYTE *destB = (BYTE*)destDW;
    register BYTE *pVal = (BYTE*)&val;
    while(iLenMod4--)
        *(destB++) = *(pVal++);
}

inline void STDCALL mswap(void *pDest, const void *pSrc, size_t iLen)
{
    assert(pDest);
    assert(pSrc);

    register size_t iLenMod4 = iLen&3;
    register size_t iLenDiv4 = iLen>>2;

    register DWORD *srcDW = (DWORD*)pSrc, *destDW = (DWORD*)pDest;
    while(iLenDiv4--)
    {
        DWORD dw = *destDW;
        *(destDW++) = *srcDW;
        *(srcDW++) = dw;
    }

    register BYTE *srcB = (BYTE*)srcDW, *destB = (BYTE*)destDW;
    while(iLenMod4--)
    {
        BYTE b = *destB;
        *(destB++) = *srcB;
        *(srcB++) = b;
    }
}

inline void STDCALL mcpyrev(void *pDest, const void *pSrc, size_t iLen)
{
    assert(pDest);
    assert(pSrc);

    register size_t iLenMod4 = iLen&3;
    register size_t iLenDiv4 = iLen>>2;

    register BYTE *srcB = (BYTE*)pSrc, *destB = (BYTE*)pDest;
    register DWORD *srcDW = (DWORD*)(srcB+iLen), *destDW = (DWORD*)(destB+iLen);
    while(iLenDiv4--)
        *(--destDW) = *(--srcDW);

    srcB = (BYTE*)srcDW;
    destB = (BYTE*)destDW;
    while(iLenMod4--)
        *(--destB) = *(--srcB);
}

#endif

inline void STDCALL mset(void *pDest, unsigned char val, size_t iLen)
{
    msetd(pDest, (val)|(val<<8)|(val<<16)|(val<<24), iLen);
}

inline void nop()
{
}

#pragma warning(default : 4035)
