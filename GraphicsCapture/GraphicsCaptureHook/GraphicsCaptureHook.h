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
#include <windows.h>

#pragma intrinsic(memcpy, memset, memcmp)

#include <xmmintrin.h>
#include <emmintrin.h>

#include <objbase.h>

#include <string>
#include <sstream>
using namespace std;

#ifdef _WIN64
typedef unsigned __int64 UPARAM;
#else
typedef unsigned long UPARAM;
#endif


#define SafeRelease(var) if(var) {var->Release(); var = NULL;}


struct DummyClass {};
typedef HRESULT (DummyClass::*CLASSPROC)();

inline FARPROC ConvertClassProcToFarproc(CLASSPROC val)
{
    return *reinterpret_cast<FARPROC*>(&val);
}


class HookData
{
    BYTE data[5];
    FARPROC func;
    FARPROC hookFunc;
    bool bHooked;

public:
    inline HookData() : bHooked(false), func(NULL), hookFunc(NULL) {}

    inline bool Hook(FARPROC funcIn, FARPROC hookFuncIn)
    {
        if(bHooked)
        {
            if(funcIn == func)
            {
                if(hookFunc != hookFuncIn)
                {
                    hookFunc = hookFuncIn;
                    Rehook();
                    return true;
                }
            }
            else
                Unhook();
        }

        func = funcIn;
        hookFunc = hookFuncIn;

        DWORD oldProtect;
        if(!VirtualProtect((LPVOID)func, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;

        memcpy(data, (const void*)func, 5);
        VirtualProtect((LPVOID)func, 5, oldProtect, &oldProtect);

        return true;
    }

    inline void Rehook()
    {
        if(bHooked)
            return;

        DWORD oldProtect;
        VirtualProtect((LPVOID)func, 5, PAGE_READWRITE, &oldProtect);

        DWORD offset = DWORD(UPARAM(hookFunc) - (UPARAM(func)+5));

        LPBYTE addrData = (LPBYTE)func;
        *addrData = 0xE9;
        *(DWORD*)(addrData+1) = offset;
        VirtualProtect((LPVOID)func, 5, oldProtect, &oldProtect);

        bHooked = true;
    }

    inline void Unhook()
    {
        if(!bHooked)
            return;

        DWORD oldProtect;
        VirtualProtect((LPVOID)func, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy((void*)func, data, 5);
        VirtualProtect((LPVOID)func, 5, oldProtect, &oldProtect);

        bHooked = false;
    }
};

inline FARPROC GetVTable(LPVOID ptr, UINT funcOffset)
{
    UPARAM *vtable = *(UPARAM**)ptr;
    return (FARPROC)(*(vtable+funcOffset));
}

inline void SetVTable(LPVOID ptr, UINT funcOffset, FARPROC funcAddress)
{
    UPARAM *vtable = *(UPARAM**)ptr;

    DWORD oldProtect;
    if(!VirtualProtect((LPVOID)(vtable+funcOffset), sizeof(UPARAM), PAGE_EXECUTE_READWRITE, &oldProtect))
        return;

    *(vtable+funcOffset) = (UPARAM)funcAddress;

    VirtualProtect((LPVOID)(vtable+funcOffset), sizeof(UPARAM), oldProtect, &oldProtect);
}

inline void SSECopy(void *lpDest, void *lpSource, UINT size)
{
    UINT alignedSize = size&0xFFFFFFF0;

    if(UPARAM(lpDest)&0xF || UPARAM(lpSource)&0xF) //if unaligned revert to normal copy
    {
        memcpy(lpDest, lpSource, size);
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
            memcpy(mDest, mSrc, sizeTemp);
    }
}

typedef ULONG (WINAPI *RELEASEPROC)(LPVOID);

#include "../GlobalCaptureStuff.h"

enum GSColorFormat {GS_UNKNOWNFORMAT, GS_ALPHA, GS_GRAYSCALE, GS_RGB, GS_RGBA, GS_BGR, GS_BGRA, GS_RGBA16F, GS_RGBA32F, GS_B5G5R5A1, GS_B5G6R5, GS_R10G10B10A2, GS_DXT1, GS_DXT3, GS_DXT5};

extern HWND hwndSender, hwndReceiver;
extern HANDLE textureMutexes[2];
extern bool bCapturing;
extern bool bTargetAcquired;

extern HANDLE hFileMap;
extern LPBYTE lpSharedMemory;

UINT InitializeSharedMemory(UINT textureSize, DWORD *totalSize, MemoryCopyData **copyData, LPBYTE *textureBuffers);
void DestroySharedMemory();

//memory capture APIs
bool InitD3D9Capture();
bool InitD3D10Capture();
bool InitGLCapture();
bool InitDDrawCapture();

void FreeD3D9Capture();
void FreeD3D10Capture();
void FreeGLCapture();
void FreeDDrawCapture();

//shared texture APIs
bool InitD3D9ExCapture();
bool InitD3D101Capture();
bool InitD3D11Capture();

void FreeD3D9ExCapture();
void FreeD3D101Capture();
void FreeD3D11Capture();

