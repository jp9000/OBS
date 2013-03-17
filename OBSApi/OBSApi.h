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
#define NTDDI_VERSION  NTDDI_VISTASP1

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
BASE_EXPORT String GetLVText(HWND hwndList, UINT id);
BASE_EXPORT String GetCBText(HWND hwndCombo, UINT id=CB_ERR);
BASE_EXPORT String GetEditText(HWND hwndEdit);

BASE_EXPORT LPBYTE GetCursorData(HICON hIcon, ICONINFO &ii, UINT &size);


#define SafeReleaseLogRef(var) if(var) {ULONG chi = var->Release(); OSDebugOut(TEXT("releasing %s, %d refs were left\r\n"), L#var, chi); var = NULL;}
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

//big endian conversion functions
#define QWORD_BE(val) (((val>>56)&0xFF) | (((val>>48)&0xFF)<<8) | (((val>>40)&0xFF)<<16) | (((val>>32)&0xFF)<<24) | \
    (((val>>24)&0xFF)<<32) | (((val>>16)&0xFF)<<40) | (((val>>8)&0xFF)<<48) | ((val&0xFF)<<56))
#define DWORD_BE(val) (((val>>24)&0xFF) | (((val>>16)&0xFF)<<8) | (((val>>8)&0xFF)<<16) | ((val&0xFF)<<24))
#define WORD_BE(val)  (((val>>8)&0xFF) | ((val&0xFF)<<8))

__forceinline QWORD fastHtonll(QWORD qw) {return QWORD_BE(qw);}
__forceinline DWORD fastHtonl (DWORD dw) {return DWORD_BE(dw);}
__forceinline  WORD fastHtons (WORD  w)  {return  WORD_BE(w);}

//arghh I hate defines like this
#define RUNONCE static bool bRunOnce = false; if(!bRunOnce && (bRunOnce = true))

inline BOOL CloseDouble(double f1, double f2, double precision=0.001)
{
    return fabs(f1-f2) <= precision;
}

/* this actually can't work without a 128bit integer, so commenting out for now
inline QWORD GetQPCTime100NS(LONGLONG clockFreq)
{
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);

    QWORD timeVal = currentTime.QuadPart;
    timeVal *= 10000000;
    timeVal /= clockFreq;

    return timeVal;
}*/

inline QWORD GetQPCTimeMS(LONGLONG clockFreq)
{
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);

    QWORD timeVal = currentTime.QuadPart;
    timeVal *= 1000;
    timeVal /= clockFreq;

    return timeVal;
}

//-------------------------------------------

#include "GraphicsSystem.h"
#include "Scene.h"
#include "SettingsPane.h"
#include "APIInterface.h"
#include "AudioFilter.h"
#include "AudioSource.h"
#include "HotkeyControlEx.h"
#include "ColorControl.h"
#include "VolumeControl.h"
#include "VolumeMeter.h"
