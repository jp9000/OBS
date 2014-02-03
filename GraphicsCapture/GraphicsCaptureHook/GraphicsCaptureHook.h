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
#include <shlobj.h>

#pragma intrinsic(memcpy, memset, memcmp)

#include <xmmintrin.h>
#include <emmintrin.h>

#include <objbase.h>

#include <string>
#include <sstream>
#include <fstream>
using namespace std;


//arghh I hate defines like this
#define RUNONCE static bool bRunOnce = false; if(!bRunOnce && (bRunOnce = true)) 
#define RUNEVERY(v) static int __runCount = 0; if(__runCount == 50) __runCount = 0; if(!__runCount++) 
#define RUNEVERYRESET static int __resetCount = 0; if(__resetCount != resetCount && (__resetCount = resetCount)) 


#define SafeRelease(var) if(var) {var->Release(); var = NULL;}


#ifdef _WIN64
typedef unsigned __int64 UPARAM;
#else
typedef unsigned long UPARAM;
#endif

class HookData
{
    BYTE data[14];
    FARPROC func;
    FARPROC hookFunc;
    bool bHooked;
    bool b64bitJump;

public:
    inline HookData() : bHooked(false), func(NULL), hookFunc(NULL), b64bitJump(false) {}

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

            Unhook();
        }

        func = funcIn;
        hookFunc = hookFuncIn;

        DWORD oldProtect;
        if(!VirtualProtect((LPVOID)func, 14, PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;

        memcpy(data, (const void*)func, 14);
        //VirtualProtect((LPVOID)func, 14, oldProtect, &oldProtect);

        return true;
    }

    inline void Rehook(bool bForce=false)
    {
        if((!bForce && bHooked) || !func)
            return;

        UPARAM startAddr = UPARAM(func);
        UPARAM targetAddr = UPARAM(hookFunc);
        UPARAM offset = targetAddr - (startAddr+5);

        DWORD oldProtect;

#ifdef _WIN64
        b64bitJump = (offset > 0x7fff0000);

        if(b64bitJump)
        {
            LPBYTE addrData = (LPBYTE)func;
            VirtualProtect((LPVOID)func, 14, PAGE_EXECUTE_READWRITE, &oldProtect);
            *(addrData++) = 0xFF;
            *(addrData++) = 0x25;
            *((LPDWORD)(addrData)) = 0;
            *((unsigned __int64*)(addrData+4)) = targetAddr;
            //VirtualProtect((LPVOID)func, 14, oldProtect, &oldProtect);
        }
        else
#endif
        {
            VirtualProtect((LPVOID)func, 5, PAGE_EXECUTE_READWRITE, &oldProtect);

            LPBYTE addrData = (LPBYTE)func;
            *addrData = 0xE9;
            *(DWORD*)(addrData+1) = DWORD(offset);
            //VirtualProtect((LPVOID)func, 5, oldProtect, &oldProtect);
        }

        bHooked = true;
    }

    inline void Unhook()
    {
        if(!bHooked || !func)
            return;

        UINT count = b64bitJump ? 14 : 5;
        DWORD oldProtect;
        VirtualProtect((LPVOID)func, count, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy((void*)func, data, count);
        //VirtualProtect((LPVOID)func, count, oldProtect, &oldProtect);

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

    //VirtualProtect((LPVOID)(vtable+funcOffset), sizeof(UPARAM), oldProtect, &oldProtect);
}

inline string IntString(DWORD val)
{
    stringstream ss;
    ss << val;

    return ss.str();
}

typedef ULONG (WINAPI *RELEASEPROC)(LPVOID);

#include "../GlobalCaptureStuff.h"

enum GSColorFormat {GS_UNKNOWNFORMAT, GS_ALPHA, GS_GRAYSCALE, GS_RGB, GS_RGBA, GS_BGR, GS_BGRA, GS_RGBA16F, GS_RGBA32F, GS_B5G5R5A1, GS_B5G6R5, GS_R10G10B10A2, GS_DXT1, GS_DXT3, GS_DXT5};

extern HINSTANCE hinstMain;
extern HANDLE textureMutexes[2];
extern bool bCapturing;
extern int  resetCount;
extern bool bStopRequested;
extern bool bTargetAcquired;

extern HANDLE hFileMap;
extern LPBYTE lpSharedMemory;

extern HANDLE hSignalRestart, hSignalEnd;
extern HANDLE hSignalReady, hSignalExit;

extern HWND hwndSender;
extern HWND hwndOBS;
extern HWND hwndD3DDummyWindow;
extern HWND hwndOpenGLSetupWindow;

extern CaptureInfo *infoMem;

extern char processName[MAX_PATH];

extern fstream logOutput;

extern wstring  strKeepAlive;
extern LONGLONG keepAliveTime;

string CurrentDateTimeString();
string CurrentTimeString();

void WINAPI OSInitializeTimer();
LONGLONG WINAPI OSGetTimeMicroseconds();

HANDLE WINAPI OSCreateMutex();
void   WINAPI OSEnterMutex(HANDLE hMutex);
BOOL   WINAPI OSTryEnterMutex(HANDLE hMutex);
void   WINAPI OSLeaveMutex(HANDLE hMutex);
void   WINAPI OSCloseMutex(HANDLE hMutex);

UINT InitializeSharedMemoryCPUCapture(UINT textureSize, DWORD *totalSize, MemoryCopyData **copyData, LPBYTE *textureBuffers);
UINT InitializeSharedMemoryGPUCapture(SharedTexData **texData);
void DestroySharedMemory();

bool InitD3D9Capture();
bool InitDXGICapture();
bool InitGLCapture();
bool InitDDrawCapture();

void FreeD3D9Capture();
void FreeDXGICapture();
void FreeGLCapture();
void FreeDDrawCapture();

void QuickLog(LPCSTR lpText);
