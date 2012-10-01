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

#define WINVER         0x0600
#define _WIN32_WINDOWS 0x0600
#define _WIN32_WINNT   0x0600
#define WIN32_LEAN_AND_MEAN
#define ISOLATION_AWARE_ENABLED 1
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include <xmmintrin.h>
#include <emmintrin.h>

#define USE_TRACE 1
#include "Utility/XT.h"

//-------------------------------------------

BASE_EXPORT void LocalizeWindow(HWND hwnd, LocaleStringLookup *lookup=NULL);
BASE_EXPORT void LocalizeMenu(HMENU hMenu, LocaleStringLookup *lookup=NULL);

BASE_EXPORT String GetLBText(HWND hwndList, UINT id=LB_ERR);
BASE_EXPORT String GetCBText(HWND hwndCombo, UINT id=CB_ERR);
BASE_EXPORT String GetEditText(HWND hwndEdit);


//#define SafeRelease(var) if(var) {ULONG chi = var->Release(); OSDebugOut(TEXT("releasing %s, %d refs were left\r\n"), L#var, chi); var = NULL;}
#define SafeRelease(var) if(var) {var->Release(); var = NULL;}

inline void SSECopy(void *lpDest, void *lpSource, UINT size)
{
    UINT alignedSize = size&0xFFFFFFF0;

    if(UPARAM(lpDest)&0xF || UPARAM(lpSource)&0xF) //if unaligned revert to normal copy
    {
        mcpy(lpDest, lpSource, size);
        return;
    }

    register __m128i *mDest = (__m128i*)lpDest;
    register __m128i *mSrc  = (__m128i*)lpSource;

    {
        register UINT numCopies = alignedSize>>4;
        while(numCopies--)
        {
            _mm_store_si128(mDest, *mSrc);
            mDest++;
            mSrc++;
        }
    }

    {
        UINT sizeTemp = size-alignedSize;
        if(sizeTemp)
            mcpy(mDest, mSrc, sizeTemp);
    }
}

//-------------------------------------------

#include "GraphicsSystem.h"
#include "Scene.h"
#include "APIInterface.h"
#include "HotkeyControlEx.h"

