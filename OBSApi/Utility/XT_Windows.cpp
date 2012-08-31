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
#include "XT.h"





LARGE_INTEGER clockFreq, startTime;


void STDCALL InputProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
void STDCALL ResetCursorClip();


BOOL        bHidingCursor = 0;

HWND        hwndMainAppWindow = NULL;


void   STDCALL OSInit()
{
    QueryPerformanceFrequency(&clockFreq);
    QueryPerformanceCounter(&startTime);
}

void   STDCALL OSExit()
{
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
    SYSTEM_INFO SI;
    GetSystemInfo(&SI);
    return SI.dwPageSize;
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
    LARGE_INTEGER curTime;
    QueryPerformanceCounter(&curTime);
    LONGLONG adjustedTime = curTime.QuadPart - startTime.QuadPart;

    return DWORD(1000 * adjustedTime / clockFreq.QuadPart);
}

QWORD STDCALL OSGetTimeMicroseconds()
{
    LARGE_INTEGER curTime;
    QueryPerformanceCounter(&curTime);
    LONGLONG adjustedTime = curTime.QuadPart - startTime.QuadPart;

    return QWORD(1000000 * adjustedTime / clockFreq.QuadPart);
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



#endif
