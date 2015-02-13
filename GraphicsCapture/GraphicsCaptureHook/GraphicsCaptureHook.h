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

#define PSAPI_VERSION 1
#include <psapi.h>

#pragma intrinsic(memcpy, memset, memcmp)

#include <xmmintrin.h>
#include <emmintrin.h>

#include <objbase.h>

#include <string>
#include <sstream>
#include <fstream>
using namespace std;

#define OLDHOOKS 1

#if (!OLDHOOKS)
#include "../../minhook/include/minhook.h"
#endif

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

extern fstream logOutput;
extern string CurrentTimeString();

class HookData
{
    BYTE data[14];
    FARPROC func;
    FARPROC hookFunc;
    bool bHooked;
    bool b64bitJump;
    bool bAttemptedBounce;
    LPVOID bounceAddress;

public:
#if OLDHOOKS
    inline HookData() : bHooked(false), func(NULL), hookFunc(NULL), b64bitJump(false), bAttemptedBounce(false) {}
#else
    inline HookData() : bHooked(false), func(NULL), hookFunc(NULL), b64bitJump(false), origFunc(NULL)
    {
        MH_Initialize();
    }
    FARPROC origFunc;
#endif

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

#ifndef _WIN64
        // FIXME: check 64 bit instructions too
        if (*(BYTE *)func == 0xE9 || *(BYTE *)func == 0xE8)
        {
            CHAR		*modName, *ourName;
            CHAR		szModName[MAX_PATH];
            CHAR		szOurName[MAX_PATH];
            DWORD		memAddress;

            MEMORY_BASIC_INFORMATION mem;

            INT_PTR jumpAddress = *(DWORD *)((BYTE *)func + 1) + (DWORD)func;

            // try to identify target
            if (VirtualQueryEx(GetCurrentProcess(), (LPVOID)jumpAddress, &mem, sizeof(mem)) && mem.State == MEM_COMMIT)
                memAddress = (DWORD)mem.AllocationBase;
            else
                memAddress = jumpAddress;

            if (GetMappedFileNameA(GetCurrentProcess(), (LPVOID)memAddress, szModName, _countof(szModName) - 1))
                modName = szModName;
            else if (GetModuleFileNameA((HMODULE)memAddress, szModName, _countof(szModName) - 1))
                modName = szModName;
            else
                modName = "unknown";

            // and ourselves
            if (VirtualQueryEx(GetCurrentProcess(), (LPVOID)func, &mem, sizeof(mem)) && mem.State == MEM_COMMIT)
                memAddress = (DWORD)mem.AllocationBase;
            else
                memAddress = (DWORD)func;

            if (GetMappedFileNameA(GetCurrentProcess(), (LPVOID)memAddress, szOurName, _countof(szOurName) - 1))
                ourName = szOurName;
            else if (GetModuleFileNameA((HMODULE)memAddress, szOurName, _countof(szOurName) - 1))
                ourName = szOurName;
            else
                ourName = "unknown";

            CHAR *p = strrchr(ourName, '\\');
            if (p)
                ourName = p + 1;

            logOutput << CurrentTimeString() << "WARNING: Another hook is already present while trying to hook " << ourName << ", hook target is " << modName <<
                ". If you experience crashes, try disabling the other hooking application" << endl;
        }
#endif

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
        ULONG64 offset, diff;

        offset = targetAddr - (startAddr + 5);

        // we could be loaded above or below the target function
        if (startAddr + 5 > targetAddr)
            diff = startAddr + 5 - targetAddr;
        else
            diff = targetAddr - startAddr + 5;

#ifdef _WIN64
        // for 64 bit, try to use a shorter instruction sequence if we're too far apart, or we
        // risk overwriting other function prologues due to the 64 bit jump opcode length
        if (diff > 0x7fff0000 && !bAttemptedBounce)
        {
            MEMORY_BASIC_INFORMATION mem;

            // if we fail we don't want to continuously search memory every other call
            bAttemptedBounce = true;

            if (VirtualQueryEx(GetCurrentProcess(), (LPCVOID)startAddr, &mem, sizeof(mem)))
            {
                int i, pagesize;
                ULONG64 address;
                SYSTEM_INFO systemInfo;

                GetSystemInfo(&systemInfo);
                pagesize = systemInfo.dwAllocationGranularity;

                // try to allocate a page somewhere below the target
                for (i = 0, address = (ULONG64)mem.AllocationBase - pagesize; i < 256; i++, address -= pagesize)
                {
                    bounceAddress = VirtualAlloc((LPVOID)address, pagesize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
                    if (bounceAddress)
                        break;
                }

                // if that failed, let's try above
                if (!bounceAddress)
                {
                    for (i = 0, address = (ULONG64)mem.AllocationBase + mem.RegionSize + pagesize; i < 256; i++, address += pagesize)
                    {
                        bounceAddress = VirtualAlloc((LPVOID)address, pagesize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
                        if (bounceAddress)
                            break;
                    }
                }

                if (bounceAddress)
                {
                    // we found some space, let's try to put the 64 bit jump code there and fix up values for the original hook
                    ULONG64 newdiff;

                    if (startAddr + 5 > (UPARAM)bounceAddress)
                        newdiff = startAddr + 5 - (UPARAM)bounceAddress;
                    else
                        newdiff = (UPARAM)bounceAddress - startAddr + 5;

                    // first, see if we can reach it with a 32 bit jump
                    if (newdiff <= 0x7fff0000)
                    {
                        // we can! update values so the shorter hook is written below
                        logOutput << CurrentTimeString() << "Creating bounce hook at " << std::hex << bounceAddress << " for " << std::hex << (LPVOID)startAddr <<
                            " -> " << std::hex << (LPVOID)targetAddr << " (diff " << std::hex << diff << ")\n";
                        FillMemory(bounceAddress, pagesize, 0xCC);

                        // write new jmp
                        LPBYTE addrData = (LPBYTE)bounceAddress;
                        *(addrData++) = 0xFF;
                        *(addrData++) = 0x25;
                        *((LPDWORD)(addrData)) = 0;
                        *((unsigned __int64*)(addrData + 4)) = targetAddr;

                        targetAddr = (UPARAM)bounceAddress;
                        offset = targetAddr - (startAddr + 5);
                        diff = newdiff;
                    }
                }
            }
        }
#endif

#if OLDHOOKS
        DWORD oldProtect;

#ifdef _WIN64
        b64bitJump = (diff > 0x7fff0000);

        if (b64bitJump)
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
#else
		// redirect function at startAddr to targetAddr

        MH_CreateHook(func, hookFunc, (void**)&origFunc);
        MH_EnableHook(func);
#endif

        bHooked = true;
    }

    inline void Unhook()
    {
        if(!bHooked || !func)
            return;

#if OLDHOOKS
        UINT count = b64bitJump ? 14 : 5;
        DWORD oldProtect;
        VirtualProtect((LPVOID)func, count, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy((void*)func, data, count);
        //VirtualProtect((LPVOID)func, count, oldProtect, &oldProtect);
#else
        MH_DisableHook(func);
        MH_RemoveHook(func);
#endif

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

bool InitD3D8Capture();
bool InitD3D9Capture();
bool InitDXGICapture();
bool InitGLCapture();
bool InitDDrawCapture();

void FreeD3D9Capture();
void FreeDXGICapture();
void FreeGLCapture();
void FreeDDrawCapture();

void QuickLog(LPCSTR lpText);
