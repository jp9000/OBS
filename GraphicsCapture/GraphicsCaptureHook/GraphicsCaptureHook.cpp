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


#include "GraphicsCaptureHook.h"
#include <shlobj.h>


HINSTANCE hinstMain = NULL;
HWND hwndSender = NULL, hwndReceiver = NULL;
HANDLE textureMutexes[2] = {NULL, NULL};
bool bStopRequested = false;
bool bCapturing = true;
bool bTargetAcquired = false;

HANDLE hFileMap = NULL;
LPBYTE lpSharedMemory = NULL;
UINT sharedMemoryIDCounter = 0;


LARGE_INTEGER clockFreq, startTime;
LONGLONG prevElapsedTime;
DWORD startTick;

void WINAPI OSInitializeTimer()
{
    QueryPerformanceFrequency(&clockFreq);
    QueryPerformanceCounter(&startTime);
    startTick = GetTickCount();
    prevElapsedTime = 0;
}

LONGLONG WINAPI OSGetTimeMicroseconds()
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
        LONGLONG msecAdjustment = min(msecOff * 
            clockFreq.QuadPart / 1000, elapsedTime - 
            prevElapsedTime);
        startTime.QuadPart += msecAdjustment;
        elapsedTime -= msecAdjustment;
    }

    // Store the current elapsed time for adjustments next time.
    prevElapsedTime = elapsedTime;

    // Convert to microseconds.
    LONGLONG usecTicks = (1000000 * elapsedTime / 
        clockFreq.QuadPart);

    return usecTicks;
}

HANDLE WINAPI OSCreateMutex()
{
    CRITICAL_SECTION *pSection = (CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION));
    InitializeCriticalSection(pSection);

    return (HANDLE)pSection;
}

void   WINAPI OSEnterMutex(HANDLE hMutex)
{
    if(hMutex)
        EnterCriticalSection((CRITICAL_SECTION*)hMutex);
}

BOOL   WINAPI OSTryEnterMutex(HANDLE hMutex)
{
    if(hMutex)
        return TryEnterCriticalSection((CRITICAL_SECTION*)hMutex);
    return FALSE;
}

void   WINAPI OSLeaveMutex(HANDLE hMutex)
{
    if(hMutex)
        LeaveCriticalSection((CRITICAL_SECTION*)hMutex);
}

void   WINAPI OSCloseMutex(HANDLE hMutex)
{
    if(hMutex)
    {
        DeleteCriticalSection((CRITICAL_SECTION*)hMutex);
        free(hMutex);
    }
}


void QuickLog(LPCSTR lpText)
{
    HANDLE hFile = CreateFile(TEXT("d:\\log.txt"), GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, 0, NULL);
    DWORD bla;
    SetFilePointer(hFile, 0, 0, FILE_END);
    WriteFile(hFile, lpText, strlen(lpText), &bla, NULL);
    FlushFileBuffers(hFile);
    CloseHandle(hFile);
}


UINT InitializeSharedMemoryCPUCapture(UINT textureSize, DWORD *totalSize, MemoryCopyData **copyData, LPBYTE *textureBuffers)
{
    UINT alignedHeaderSize = (sizeof(MemoryCopyData)+15) & 0xFFFFFFF0;
    UINT alignedTexureSize = (textureSize+15) & 0xFFFFFFF0;

    *totalSize = alignedHeaderSize + alignedTexureSize*2;

    wstringstream strName;
    strName << TEXTURE_MEMORY << ++sharedMemoryIDCounter;
    hFileMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, *totalSize, strName.str().c_str());
    if(!hFileMap)
        return 0;

    lpSharedMemory = (LPBYTE)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, *totalSize);
    if(!lpSharedMemory)
    {
        CloseHandle(hFileMap);
        hFileMap = NULL;
        return 0;
    }

    *copyData = (MemoryCopyData*)lpSharedMemory;
    (*copyData)->texture1Offset = alignedHeaderSize;
    (*copyData)->texture2Offset = alignedHeaderSize+alignedTexureSize;

    textureBuffers[0] = lpSharedMemory+alignedHeaderSize;
    textureBuffers[1] = lpSharedMemory+alignedHeaderSize+alignedTexureSize;

    return sharedMemoryIDCounter;
}

UINT InitializeSharedMemoryGPUCapture(SharedTexData **texData)
{
    int totalSize = sizeof(SharedTexData);

    wstringstream strName;
    strName << TEXTURE_MEMORY << ++sharedMemoryIDCounter;
    hFileMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, totalSize, strName.str().c_str());
    if(!hFileMap)
        return 0;

    lpSharedMemory = (LPBYTE)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, totalSize);
    if(!lpSharedMemory)
    {
        CloseHandle(hFileMap);
        hFileMap = NULL;
        return 0;
    }

    *texData = (SharedTexData*)lpSharedMemory;

    return sharedMemoryIDCounter;
}

void DestroySharedMemory()
{
    if(lpSharedMemory && hFileMap)
    {
        UnmapViewOfFile(lpSharedMemory);
        CloseHandle(hFileMap);

        hFileMap = NULL;
        lpSharedMemory = NULL;
    }
}


LRESULT WINAPI SenderWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case SENDER_RESTARTCAPTURE:
            bCapturing = true;
            break;

        case SENDER_ENDCAPTURE:
            bStopRequested = true;
            bCapturing = false;
            hwndReceiver = NULL;
            break;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

bool bD3D9Hooked = false;
bool bD3D10Hooked = false;
bool bD3D101Hooked = false;
bool bD3D11Hooked = false;
bool bGLHooked = false;
bool bDirectDrawHooked = false;

inline bool AttemptToHookSomething()
{
    bool bFoundSomethingToHook = false;
    if(!bD3D11Hooked && InitD3D11Capture())
    {
        logOutput << "D3D11 Present" << endl;
        bFoundSomethingToHook = true;
        bD3D11Hooked = true;
        bD3D101Hooked = true;
        bD3D10Hooked = true;
    }
    if(!bD3D9Hooked && InitD3D9Capture())
    {
        logOutput << "D3D9 Present" << endl;
        bFoundSomethingToHook = true;
        bD3D9Hooked = true;
    }
    if(!bD3D101Hooked && InitD3D101Capture())
    {
        logOutput << "D3D10.1 Present" << endl;
        bFoundSomethingToHook = true;
        bD3D11Hooked = true;
        bD3D101Hooked = true;
        bD3D10Hooked = true;
    }
    if(!bD3D10Hooked && InitD3D10Capture())
    {
        logOutput << "D3D10 Present" << endl;
        bFoundSomethingToHook = true;
        bD3D11Hooked = true;
        bD3D101Hooked = true;
        bD3D10Hooked = true;
    }
    if(!bGLHooked && InitGLCapture())
    {
        logOutput << "GL Present" << endl;
        bFoundSomethingToHook = true;
        bGLHooked = true;
    }
    /*
    if(!bDirectDrawHooked && InitDDrawCapture())
    {
        OutputDebugString(TEXT("DirectDraw Present\r\n"));
        bFoundSomethingToHook = true;
        bDirectDrawfHooked = true;
    }*/

    return bFoundSomethingToHook;
}

fstream logOutput;


DWORD WINAPI CaptureThread(HANDLE hDllMainThread)
{
    bool bSuccess = false;

    //wait for dll initialization to finish before executing any initialization code
    if(hDllMainThread)
    {
        WaitForSingleObject(hDllMainThread, INFINITE);
        CloseHandle(hDllMainThread);
    }

    TCHAR lpLogPath[MAX_PATH];
    SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, lpLogPath);
    wcscat_s(lpLogPath, MAX_PATH, TEXT("\\OBS\\pluginData\\captureHookLog.txt"));

    if(!logOutput.is_open())
        logOutput.open(lpLogPath, ios_base::in | ios_base::out | ios_base::trunc);

    WNDCLASS wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.hInstance = hinstMain;
    wc.lpszClassName = SENDER_WINDOWCLASS;
    wc.lpfnWndProc = (WNDPROC)SenderWindowProc;

    if(RegisterClass(&wc))
    {
        hwndSender = CreateWindow(SENDER_WINDOWCLASS, NULL, 0, 0, 0, 0, 0, NULL, 0, hinstMain, 0);
        if(hwndSender)
        {
            textureMutexes[0] = OpenMutex(MUTEX_ALL_ACCESS, FALSE, TEXTURE_MUTEX1);
            if(textureMutexes[0])
            {
                textureMutexes[1] = OpenMutex(MUTEX_ALL_ACCESS, FALSE, TEXTURE_MUTEX2);
                if(textureMutexes[1])
                {
                    while(!AttemptToHookSomething())
                        Sleep(50);

                    MSG msg;
                    while(GetMessage(&msg, NULL, 0, 0))
                    {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);

                        AttemptToHookSomething();
                    }

                    CloseHandle(textureMutexes[1]);
                    textureMutexes[1] = NULL;
                }

                CloseHandle(textureMutexes[0]);
                textureMutexes[0] = NULL;
            }

            DestroyWindow(hwndSender);
        }
    }

    return 0;
}


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpBlah)
{
    if(dwReason == DLL_PROCESS_ATTACH)
    {
        HANDLE hThread = NULL;
        hinstMain = hinstDLL;

        HANDLE hDllMainThread = OpenThread(THREAD_ALL_ACCESS, NULL, GetCurrentThreadId());

        if(!(hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CaptureThread, (LPVOID)hDllMainThread, 0, 0)))
        {
            CloseHandle(hDllMainThread);
            return FALSE;
        }

        CloseHandle(hThread);
    }
    else if(dwReason == DLL_PROCESS_DETACH)
    {
        /*FreeGLCapture();
        FreeD3D9Capture();
        FreeD3D10Capture();
        FreeD3D101Capture();
        FreeD3D11Capture();*/

        if(hwndReceiver)
            PostMessage(hwndReceiver, RECEIVER_ENDCAPTURE, 0, 0);

        if(hwndSender)
            DestroyWindow(hwndSender);

        for(UINT i=0; i<2; i++)
        {
            if(textureMutexes[i])
                CloseHandle(textureMutexes[i]);
        }

        if(logOutput.is_open())
            logOutput.close();
    }

    return TRUE;
}
