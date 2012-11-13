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


#ifdef WIN32

#define WINVER         0x0600
#define _WIN32_WINDOWS 0x0600
#define _WIN32_WINNT   0x0600
#define WIN32_LEAN_AND_MEAN
#define ISOLATION_AWARE_ENABLED 1
#include <windows.h>
#include <MMSystem.h>
#include <Psapi.h>
#include "XT.h"





LARGE_INTEGER clockFreq, startTime;
LONGLONG prevElapsedTime;
DWORD startTick;

int coreCount = 1, logicalCores = 1;

void STDCALL InputProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
void STDCALL ResetCursorClip();

SYSTEM_INFO si;

BOOL        bHidingCursor = 0;

HWND        hwndMainAppWindow = NULL;

// Helper function to count set bits in the processor mask.
DWORD CountSetBits(ULONG_PTR bitMask)
{
    DWORD LSHIFT = sizeof(ULONG_PTR)*8 - 1;
    DWORD bitSetCount = 0;
    ULONG_PTR bitTest = (ULONG_PTR)1 << LSHIFT;    
    DWORD i;

    for (i = 0; i <= LSHIFT; ++i)
    {
        bitSetCount += ((bitMask & bitTest)?1:0);
        bitTest/=2;
    }

    return bitSetCount;
}

void   STDCALL OSInit()
{
    timeBeginPeriod(1);

    GetSystemInfo(&si);

    QueryPerformanceFrequency(&clockFreq);
    QueryPerformanceCounter(&startTime);
    startTick = GetTickCount();
    prevElapsedTime = 0;

    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pInfo = NULL, pTemp = NULL;
    DWORD dwLen = 0;
    if(!GetLogicalProcessorInformation(pInfo, &dwLen))
    {
        if(GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            pInfo = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(dwLen);

            if(GetLogicalProcessorInformation(pInfo, &dwLen))
            {
                pTemp = pInfo;
                DWORD dwNum = dwLen/sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);

                coreCount = 0;
                logicalCores = 0;

                for(UINT i=0; i<dwNum; i++)
                {
                    if(pTemp->Relationship == RelationProcessorCore)
                    {
                        coreCount++;
                        logicalCores += CountSetBits(pTemp->ProcessorMask);
                    }

                    pTemp++;
                }
            }

            free(pInfo);
        }
    }
}

void   STDCALL OSExit()
{
    timeEndPeriod(1);
}



void   STDCALL OSSetMainAppWindow(HANDLE window)
{
    hwndMainAppWindow = (HWND)window;
}

void __cdecl OSMessageBoxva(const TCHAR *format, va_list argptr)
{
    TCHAR blah[4096];
    vtsprintf_s(blah, 4095, format, argptr);

    MessageBox(hwndMainAppWindow, blah, NULL, MB_ICONWARNING);
}

void __cdecl OSMessageBox(const TCHAR *format, ...)
{
    va_list arglist;

    va_start(arglist, format);

    OSMessageBoxva(format, arglist);
}


HANDLE STDCALL OSFindFirstFile(CTSTR lpFileName, OSFindData &findData)
{
    WIN32_FIND_DATA wfd;
    HANDLE hFind = FindFirstFile(lpFileName, &wfd);

    if(hFind == INVALID_HANDLE_VALUE)
        hFind = NULL;
    else
    {
        BOOL bFoundDumbDir;
        do
        {
            bFoundDumbDir = FALSE;
            if( (scmp(wfd.cFileName, TEXT("..")) == 0) ||
                (scmp(wfd.cFileName, TEXT(".")) == 0)  )
            {
                if(!FindNextFile(hFind, &wfd))
                {
                    FindClose(hFind);
                    return NULL;
                }
                bFoundDumbDir = TRUE;
            }
        }while(bFoundDumbDir);

        scpy(findData.fileName, wfd.cFileName);
        findData.bDirectory = (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        findData.bHidden = (wfd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
    }

    return hFind;
}

int    STDCALL OSGetTotalCores()
{
    return coreCount;
}

int    STDCALL OSGetLogicalCores()
{
    return logicalCores;
}


BOOL  STDCALL OSFindNextFile(HANDLE hFind, OSFindData &findData)
{
    WIN32_FIND_DATA wfd;
    BOOL bSuccess = FindNextFile(hFind, &wfd);

    BOOL bFoundDumbDir;
    do
    {
        bFoundDumbDir = FALSE;
        if( (scmp(wfd.cFileName, TEXT("..")) == 0) ||
            (scmp(wfd.cFileName, TEXT(".")) == 0)  )
        {
            if(!FindNextFile(hFind, &wfd))
            {
                bSuccess = FALSE;
                break;
            }
            bFoundDumbDir = TRUE;
        }
    }while(bFoundDumbDir);

    if(bSuccess)
    {
        scpy(findData.fileName, wfd.cFileName);
        findData.bDirectory = (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        findData.bHidden = (wfd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
    }
    else
        *findData.fileName = 0;

    return bSuccess;
}

void  STDCALL OSFindClose(HANDLE hFind)
{
    FindClose(hFind);
}


DWORD  STDCALL OSGetSysPageSize()
{
    return si.dwPageSize;
}

LPVOID STDCALL OSVirtualAlloc(size_t dwSize)
{
    return VirtualAlloc(NULL, dwSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
}

void   STDCALL OSVirtualFree(LPVOID lpData)
{
    VirtualFree(lpData, 0, MEM_RELEASE);
}

void   STDCALL OSExitProgram()
{
    PostQuitMessage(0);
}

void   STDCALL OSCriticalExit()
{
    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 1, 0, 0);
    TerminateProcess(GetCurrentProcess(), INVALID);
}


BOOL   STDCALL OSDeleteFile(CTSTR lpFile)
{
    return DeleteFile(lpFile);
}

BOOL   STDCALL OSCopyFile(CTSTR lpFileDest, CTSTR lpFileSrc)
{
    return CopyFile(lpFileSrc, lpFileDest, TRUE);
}

BOOL   STDCALL OSCreateDirectory(CTSTR lpDirectory)
{
    return CreateDirectory(lpDirectory, NULL);
}

BOOL   STDCALL OSSetCurrentDirectory(CTSTR lpDirectory)
{
    return SetCurrentDirectory(lpDirectory);
}

BOOL   STDCALL OSFileExists(CTSTR lpFile)
{
    OSFindData ofd;
    HANDLE hFind = OSFindFirstFile(lpFile, ofd);
    if(hFind)
    {
        OSFindClose(hFind);
        return TRUE;
    }

    return FALSE;
}

int   STDCALL OSProcessEvent()
{
    MSG msg;

    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
            return 0;
        else
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            return 2;
        }
    }
    else
        return 1;
}


void STDCALL OSShowCursor(BOOL bShow)
{
    /*HCURSOR hCursor = bShow ? LoadCursor(NULL, IDC_ARROW) : NULL;
    bHidingCursor = !bShow;
    SetCursor(hCursor);

    ResetCursorClip(hwndMain);*/

    if(bShow)
    {
        int displayCount = ShowCursor(TRUE);
        while(displayCount > 0)
            displayCount = ShowCursor(FALSE);
    }
    else
    {
        int displayCount = ShowCursor(FALSE);
        while(displayCount > -1)
            displayCount = ShowCursor(FALSE);
    }
}

void STDCALL OSSetCursorPos(int x, int y)
{
    POINT point;
    point.x = x;
    point.y = y;
    SetCursorPos(point.x, point.y);
}

void STDCALL OSGetCursorPos(int &x, int &y)
{
    POINT point;

    GetCursorPos(&point);
    x = point.x;
    y = point.y;
}


BOOL STDCALL OSDebuggerPresent()
{
    return IsDebuggerPresent();
}

void __cdecl OSDebugOutva(const TCHAR *format, va_list argptr)
{
    /*TCHAR blah[4096];
    vtsprintf_s(blah, 4095, format, argptr);*/

    String strDebug = FormattedStringva(format, argptr);

    OutputDebugString(strDebug);
}

void __cdecl OSDebugOut(const TCHAR *format, ...)
{
    va_list arglist;

    va_start(arglist, format);

    OSDebugOutva(format, arglist);
}



HANDLE STDCALL OSLoadLibrary(CTSTR lpFile)
{
    TCHAR FullFileName[250];

    scpy(FullFileName, lpFile);
    scat(FullFileName, TEXT(".dll"));
    return (HANDLE)LoadLibrary(FullFileName);
}

DEFPROC STDCALL OSGetProcAddress(HANDLE hLibrary, LPCSTR lpProcedure)
{
    return (DEFPROC)GetProcAddress((HMODULE)hLibrary, lpProcedure);
}

void   STDCALL OSFreeLibrary(HANDLE hLibrary)
{
    FreeLibrary((HMODULE)hLibrary);
}


HANDLE STDCALL OSCreateMutex()
{
    CRITICAL_SECTION *pSection = (CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION));
    InitializeCriticalSection(pSection);

    return (HANDLE)pSection;
}

void   STDCALL OSEnterMutex(HANDLE hMutex)
{
    assert(hMutex);
    EnterCriticalSection((CRITICAL_SECTION*)hMutex);
}

BOOL   STDCALL OSTryEnterMutex(HANDLE hMutex)
{
    assert(hMutex);
    return TryEnterCriticalSection((CRITICAL_SECTION*)hMutex);
}

void   STDCALL OSLeaveMutex(HANDLE hMutex)
{
    assert(hMutex);
    LeaveCriticalSection((CRITICAL_SECTION*)hMutex);
}

void   STDCALL OSCloseMutex(HANDLE hMutex)
{
    assert(hMutex);
    DeleteCriticalSection((CRITICAL_SECTION*)hMutex);
    free(hMutex);
}



void   STDCALL OSSleep(DWORD dwMSeconds)
{
    Sleep(dwMSeconds);
}


DWORD STDCALL OSGetTime()
{
    //NOTE: Credit goes to the amazingly awesome bullet physics library for this time code fix,
    //though I think this was basically copied out of the KB274323

    /*LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    LONGLONG elapsedTime = currentTime.QuadPart - 
        startTime.QuadPart;

    // Compute the number of millisecond ticks elapsed.
    unsigned long msecTicks = (unsigned long)(1000 * elapsedTime / 
        clockFreq.QuadPart);

    // Check for unexpected leaps in the Win32 performance counter.  
    // (This is caused by unexpected data across the PCI to ISA 
    // bridge, aka south bridge.  See Microsoft KB274323.)
    unsigned long elapsedTicks = GetTickCount() - startTick;
    signed long msecOff = (signed long)(msecTicks - elapsedTicks);
    if (msecOff < -100 || msecOff > 100)
    {
        // Adjust the starting time forwards.
        LONGLONG msecAdjustment = MIN(msecOff * 
            clockFreq.QuadPart / 1000, elapsedTime - 
            prevElapsedTime);
        startTime.QuadPart += msecAdjustment;
        elapsedTime -= msecAdjustment;

        // Recompute the number of millisecond ticks elapsed.
        msecTicks = (unsigned long)(1000 * elapsedTime / 
            clockFreq.QuadPart);
    }

    // Store the current elapsed time for adjustments next time.
    prevElapsedTime = elapsedTime;

    return msecTicks;*/

    return timeGetTime();
}

QWORD STDCALL OSGetTimeMicroseconds()
{
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    LONGLONG elapsedTime = currentTime.QuadPart - 
        startTime.QuadPart;

    // Compute the number of millisecond ticks elapsed.
    unsigned long msecTicks = (unsigned long)(1000 * elapsedTime / 
        clockFreq.QuadPart);

    // Check for unexpected leaps in the Win32 performance counter.  
    // (This is caused by unexpected data across the PCI to ISA 
    // bridge, aka south bridge.  See Microsoft KB274323.)
    unsigned long elapsedTicks = GetTickCount() - startTick;
    signed long msecOff = (signed long)(msecTicks - elapsedTicks);
    if (msecOff < -100 || msecOff > 100)
    {
        // Adjust the starting time forwards.
        LONGLONG msecAdjustment = MIN(msecOff * 
            clockFreq.QuadPart / 1000, elapsedTime - 
            prevElapsedTime);
        startTime.QuadPart += msecAdjustment;
        elapsedTime -= msecAdjustment;
    }

    // Store the current elapsed time for adjustments next time.
    prevElapsedTime = elapsedTime;

    // Convert to microseconds.
    unsigned long usecTicks = (unsigned long)(1000000 * elapsedTime / 
        clockFreq.QuadPart);

    return usecTicks;
}


UINT STDCALL OSGetProcessorCount()
{
    return si.dwNumberOfProcessors;
}

HANDLE STDCALL OSCreateThread(XTHREAD lpThreadFunc, LPVOID param)
{
    DWORD dummy;
    return CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)lpThreadFunc, param, 0, &dummy);
}

BOOL   STDCALL OSWaitForThread(HANDLE hThread, LPDWORD ret)
{
    BOOL bRet = (WaitForSingleObjectEx(hThread, INFINITE, 0) == WAIT_OBJECT_0);

    if(ret)
        GetExitCodeThread(hThread, ret);

    return bRet;
}

BOOL   STDCALL OSCloseThread(HANDLE hThread)
{
    CloseHandle(hThread);
    return 1;
}

BOOL   STDCALL OSTerminateThread(HANDLE hThread, DWORD waitMS)
{
    if(WaitForSingleObjectEx(hThread, waitMS, 0) == WAIT_TIMEOUT)
        TerminateThread(hThread, 0);

    CloseHandle(hThread);
    return 1;
}

BOOL   STDCALL OSGetLoadedModuleList(HANDLE hProcess, StringList &ModuleList)
{
    HMODULE hMods[1024];
    DWORD count;

    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &count))
    {
        for (UINT i=0; i<(count / sizeof(HMODULE)); i++)
        {
            TCHAR szFileName[MAX_PATH];

            if (GetModuleFileNameEx(hProcess, hMods[i], szFileName, _countof(szFileName)-1))
            {
                TCHAR *p;
                p = srchr(szFileName, '\\');
                if (p)
                {
                    *p = 0;
                    p++;
                }

                slwr (p);
                ModuleList << p;
            }
        }
    }
    else
        return 0;

    return 1;
}

BOOL   STDCALL OSIncompatibleModulesLoaded()
{
    StringList  moduleList;

    if (!OSGetLoadedModuleList(GetCurrentProcess(), moduleList))
        return 0;

    for(UINT i=0; i<moduleList.Num(); i++)
    {
        CTSTR moduleName = moduleList[i];

        if (!scmp(moduleName, TEXT("dxtorycore.dll")) ||
            !scmp(moduleName, TEXT("dxtorycore64.dll")) ||
            !scmp(moduleName, TEXT("dxtorymm.dll")) ||
            !scmp(moduleName, TEXT("dxtorymm64.dll")))
        {
            return 1;
        }
    }

    return 0;
}


#endif
