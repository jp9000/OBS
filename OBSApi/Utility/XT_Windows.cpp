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
#include <ShlObj.h>
#include <MMSystem.h>
#include <Psapi.h>
#include "XT.h"
#include <memory>




extern HANDLE hProfilerMutex;

LARGE_INTEGER clockFreq, startTime;
LONGLONG prevElapsedTime;
DWORD startTick;

int coreCount = 1, logicalCores = 1;

void STDCALL InputProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
void STDCALL ResetCursorClip();

SYSTEM_INFO si;
OSVERSIONINFO osVersionInfo;

BOOL        bHidingCursor = 0;
BOOL        bWindows8 = 0;

HWND        hwndMainAppWindow = NULL;

extern int OBSMessageBox(HWND hwnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT flags);
extern INT_PTR OBSDialogBox(HINSTANCE hInstance, LPCWSTR lpTemplateName, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam = 0);
extern HWND OBSCreateDialog(HINSTANCE hInstance, LPCWSTR lpTemplateName, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam = 0);

struct OSFileChangeData
{
    HANDLE hDirectory;
    OVERLAPPED directoryChange;
    TCHAR strDirectory[MAX_PATH];
    TCHAR targetFileName[MAX_PATH];
    BYTE changeBuffer[2048];
};

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

int    STDCALL OSGetVersion()
{
    if(osVersionInfo.dwMajorVersion > 6)
        return 8;

    if (osVersionInfo.dwMajorVersion == 6)
    {
        //Windows 8
        if (osVersionInfo.dwMinorVersion >= 2)
            return 8;

        //Windows 7
        if (osVersionInfo.dwMinorVersion == 1)
            return 7;

        //Vista
        if (osVersionInfo.dwMinorVersion == 0)
            return 6;
    }

    return 0;
}

void   STDCALL OSInit()
{
    timeBeginPeriod(1);

    TIMECAPS chi;
    timeGetDevCaps(&chi, sizeof(chi));

    GetSystemInfo(&si);

    osVersionInfo.dwOSVersionInfoSize = sizeof(osVersionInfo);
    GetVersionEx(&osVersionInfo);

    if (OSGetVersion() == 8)
        bWindows8 = TRUE;

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

    hProfilerMutex = OSCreateMutex();
}

void   STDCALL OSExit()
{
    timeEndPeriod(1);

    OSCloseMutex(hProfilerMutex);
}



void   STDCALL OSSetMainAppWindow(HANDLE window)
{
    hwndMainAppWindow = (HWND)window;
}

void __cdecl OSMessageBoxva(const TCHAR *format, va_list argptr)
{
    TCHAR blah[4096];
    vtsprintf_s(blah, 4095, format, argptr);

    OBSMessageBox(hwndMainAppWindow, blah, NULL, MB_ICONWARNING);
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

    if (bSuccess)
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
                    bSuccess = FALSE;
                    break;
                }
                bFoundDumbDir = TRUE;
            }
        }while(bFoundDumbDir);
    }

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

BOOL   STDCALL OSRenameFile(CTSTR oldPath, CTSTR newPath)
{
    return MoveFileEx(oldPath, newPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
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
    DWORD attrib = GetFileAttributes(lpFile);
    return (attrib != INVALID_FILE_ATTRIBUTES);
}

bool   STDCALL OSFileIsDirectory(CTSTR file)
{
    return (FILE_ATTRIBUTE_DIRECTORY == (GetFileAttributes(file) & FILE_ATTRIBUTE_DIRECTORY));
}

QWORD STDCALL OSGetFileModificationTime(String path)
{
    HANDLE hFile;
    if((hFile = CreateFile(path.Array(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE)
        return -1;
    FILETIME create, write;
    BOOL res = GetFileTime(hFile, &create, nullptr, &write);
    CloseHandle(hFile);
    if(!res)
        return -1;
    ULARGE_INTEGER create_, write_;
    create_.HighPart = create.dwHighDateTime;
    create_.LowPart = create.dwLowDateTime;
    write_.HighPart = write.dwHighDateTime;
    write_.LowPart = write.dwLowDateTime;
    if(write_.QuadPart > create_.QuadPart)
        return write_.QuadPart;
    return create_.QuadPart;
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

#ifndef _DEBUG
    return;
#endif

    String strDebug = FormattedStringva(format, argptr);

    OutputDebugString(strDebug);
}

void __cdecl OSDebugOut(const TCHAR *format, ...)
{
    va_list arglist;

    va_start(arglist, format);

    OSDebugOutva(format, arglist);
}

CTSTR STDCALL OSGetErrorString(DWORD errorCode)
{
    static TCHAR errorString[2048];

    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, errorCode, 0, errorString, _countof(errorString)-1, NULL);
    errorString[_countof(errorString)-1] = 0;

    return errorString;
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


void   STDCALL OSSleepSubMillisecond(double fMSeconds)
{
    int intPart;

    intPart = (int)fMSeconds;
    if(bWindows8 && intPart > 0)
        intPart--;
    if (intPart > 0)
        Sleep(intPart);

    fMSeconds -= intPart;

    LARGE_INTEGER t1, t2;
    double fElapsedTime;

    QueryPerformanceCounter(&t1);
    for (;;)
    {
        QueryPerformanceCounter(&t2);
        fElapsedTime = (t2.QuadPart - t1.QuadPart) * 1000.0 / clockFreq.QuadPart;
        if (fElapsedTime >= fMSeconds)
            return;
        Sleep(0);
    }
}

void   STDCALL OSSleepMicrosecond(QWORD qwMicroseconds)
{
    unsigned int milliseconds = (unsigned int)(qwMicroseconds/1000);
    if(bWindows8 && milliseconds >= 2)
        milliseconds--;
    if (milliseconds > 0)
        Sleep(milliseconds);

    qwMicroseconds -= milliseconds*1000;

    LARGE_INTEGER t1, t2;
    QWORD qwElapsedTime;

    QueryPerformanceCounter(&t1);
    for (;;)
    {
        QueryPerformanceCounter(&t2);
        qwElapsedTime = (t2.QuadPart - t1.QuadPart) * 1000000 / clockFreq.QuadPart;
        if (qwElapsedTime >= qwMicroseconds)
            return;
        Sleep(0);
    }
}

void   STDCALL OSSleep100NS(QWORD qw100NSTime)
{
    unsigned int milliseconds = (unsigned int)(qw100NSTime/10000);
    if(bWindows8 && milliseconds >= 2)
        milliseconds--;
    if (milliseconds > 0)
        Sleep(milliseconds);

    qw100NSTime -= milliseconds*10000;

    LARGE_INTEGER t1, t2;
    QWORD qwElapsedTime;

    QueryPerformanceCounter(&t1);
    for (;;)
    {
        QueryPerformanceCounter(&t2);
        qwElapsedTime = (t2.QuadPart - t1.QuadPart) * 10000000 / clockFreq.QuadPart;
        if (qwElapsedTime >= qw100NSTime)
            return;
        Sleep(0);
    }
}

void   STDCALL OSSleep(DWORD dwMSeconds)
{
    if(bWindows8)
    {
        if(dwMSeconds > 0)
            Sleep(dwMSeconds-1);
        else
            Sleep(0);
    }
    else
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
    QWORD usecTicks = (QWORD)(1000000 * elapsedTime / 
        clockFreq.QuadPart);

    return usecTicks;
}

double STDCALL OSGetTimeDoubleMS()
{
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    LONGLONG elapsedTime = currentTime.QuadPart - 
        startTime.QuadPart;

    // Store the current elapsed time for adjustments next time.
    prevElapsedTime = elapsedTime;

    return double(elapsedTime) * 1000.0 / double(clockFreq.QuadPart);
}

QWORD STDCALL OSGetThreadTime(HANDLE thread)
{
#define TO_QWORD(t) (((QWORD)t.dwHighDateTime)<<32 | (QWORD)t.dwLowDateTime)
    FILETIME user, create, exit, kernel;
    if(!GetThreadTimes(thread, &create, &exit, &kernel, &user))
        return -1;
    return (TO_QWORD(user) + TO_QWORD(kernel))/10;
#undef TO_QWORD
}


UINT STDCALL OSGetProcessorCount()
{
    return si.dwNumberOfProcessors;
}

HANDLE STDCALL OSCreateThread(XTHREAD lpThreadFunc, LPVOID param)
{
    HANDLE hThread;
    DWORD dummy;

    hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)lpThreadFunc, param, 0, &dummy);
    if (!hThread)
        CrashError (TEXT("CreateThread 0x%p failed: %d"), (LPVOID)lpThreadFunc, GetLastError());
    
    return hThread;
}

HANDLE STDCALL OSGetCurrentThread()
{
	return GetCurrentThread();
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
    assert (hThread);

    CloseHandle(hThread);
    return 1;
}

BOOL   STDCALL OSTerminateThread(HANDLE hThread, DWORD waitMS)
{
    assert (hThread);

    if(WaitForSingleObjectEx(hThread, waitMS, 0) == WAIT_TIMEOUT)
    {
        Log (TEXT("WARNING: Forcibly terminating a thread after %d ms timeout!"), waitMS);

        //force a crash report. if we're terminating threads, something terribly wrong is happening, best
        //to get a report of that rather than let corruption from thread termination continue to propagate
        //and cause harder to debug errors.
        DebugBreak ();
        TerminateThread(hThread, 0);
    }

    CloseHandle(hThread);
    return 1;
}

void OSRaiseException(DWORD code)
{
    RaiseException(code, EXCEPTION_NONCONTINUABLE, 0, NULL);
}

void OSCloseEvent(HANDLE event)
{
    CloseHandle(event);
}

BOOL   STDCALL OSGetLoadedModuleList(HANDLE hProcess, StringList &ModuleList)
{
    HMODULE hMods[1024];
    DWORD count;

    if (EnumProcessModulesEx(hProcess, hMods, sizeof(hMods), &count, LIST_MODULES_ALL))
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
                else
                {
                    p = szFileName;
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

BOOL   STDCALL OSIncompatiblePatchesLoaded(String &errors)
{
    BOOL ret = FALSE;
    StringList moduleList;

    OSGetLoadedModuleList (GetCurrentProcess(), moduleList);

    //known problematic code modification hooks can be checked for here

    //current checks:
    //TeamSpeak 3 Overlay (hooks CreateDXGIFactory1 in such a way that it fails when called by OBS)
    //Webroot Secureanywhere (hooks GDI calls and prevents OBS from screen capturing among other issues)

    bool ts3_hook_present = false;

    //Older plugin version hooks DXGI
    HMODULE dxGI = GetModuleHandle(TEXT("DXGI.DLL"));
    if (dxGI)
    {
        FARPROC createFactory = GetProcAddress(dxGI, "CreateDXGIFactory1");
        if (createFactory)
        {
            if (!IsBadReadPtr(createFactory, 5))
            {
                BYTE opCode = *(BYTE *)createFactory;

                if (opCode == 0xE9)
                {
                    if (moduleList.HasValue(TEXT("ts3overlay_hook_win32.dll")) ||
                        moduleList.HasValue(TEXT("ts3overlay_hook_win64.dll")))
                    {
                        ts3_hook_present = true;
                    }
                }
            }
        }
    }

    //Newer plugin hooks D3D
    HMODULE d3d = GetModuleHandle(TEXT("d3d10_1.dll"));
    if (d3d)
    {
        FARPROC createDevice = GetProcAddress(d3d, "D3D10CreateDeviceAndSwapChain1");
        if (createDevice)
        {
            if (!IsBadReadPtr(createDevice, 5))
            {
                BYTE opCode = *(BYTE *)createDevice;

                if (opCode == 0xE9)
                {
                    if (moduleList.HasValue(TEXT("ts3overlay_hook_win32.dll")) ||
                        moduleList.HasValue(TEXT("ts3overlay_hook_win64.dll")))
                    {
                        ts3_hook_present = true;
                    }
                }
            }
        }
    }

    d3d = GetModuleHandle(TEXT("d3d11.dll"));
    if (d3d)
    {
        FARPROC createDevice = GetProcAddress(d3d, "D3D11CreateDeviceAndSwapChain");
        if (createDevice)
        {
            if (!IsBadReadPtr(createDevice, 5))
            {
                BYTE opCode = *(BYTE *)createDevice;

                if (opCode == 0xE9)
                {
                    if (moduleList.HasValue(TEXT("ts3overlay_hook_win32.dll")) ||
                        moduleList.HasValue(TEXT("ts3overlay_hook_win64.dll")))
                    {
                        ts3_hook_present = true;
                    }
                }
            }
        }
    }

    if (ts3_hook_present)
    {
        errors << TEXT("TeamSpeak 3 overlay has loaded into OBS and will cause crashing and instability. Please set \"Disable Loading\" for OBS.EXE in your TeamSpeak 3 overlay settings or visit http://bit.ly/OBSTS3 for help."); 
        ret = TRUE;
    }

    //I'm just going to make this a warning that pops up when the app starts instead of actually preventing people from using the app
    //People are complaining about this a bit too much and it's just like "whatever, do whatever you want"
    /*HMODULE msIMG = GetModuleHandle(TEXT("MSIMG32"));
    if (msIMG)
    {
        FARPROC alphaBlend = GetProcAddress(msIMG, "AlphaBlend");
        if (alphaBlend)
        {
            if (!IsBadReadPtr(alphaBlend, 5))
            {
                BYTE opCode = *(BYTE *)alphaBlend;

                if (opCode == 0xE9)
                {
                    if (moduleList.HasValue(TEXT("wrusr.dll")))
                    {
                        if (!errors.IsEmpty())
                            errors << TEXT("\r\n\r\n");

                        errors << TEXT("Webroot Secureanywhere appears to be active. This product is incompatible with OBS as the security features block OBS from accessing Windows GDI functions. Please add OBS.exe to the Secureanywhere exceptions list and restart OBS - see http://bit.ly/OBSWR if you need help."); 
                        ret = TRUE;
                    }
                }
            }
        }
    }*/

    return ret;
}

BOOL   STDCALL OSIncompatibleModulesLoaded()
{
    StringList  moduleList;
    
    //Modules that will likely cause OBS to crash because they hooked it.
    //This list is checked on stream start only.

    if (!OSGetLoadedModuleList(GetCurrentProcess(), moduleList))
        return 0;

    for(UINT i=0; i<moduleList.Num(); i++)
    {
        CTSTR moduleName = moduleList[i];

        if (!scmp(moduleName, TEXT("dxtorycore.dll")) ||        //DXTory
            !scmp(moduleName, TEXT("dxtorycore64.dll")) ||      //DXTory
            !scmp(moduleName, TEXT("dxtorymm.dll")) ||          //DXTory
            !scmp(moduleName, TEXT("dxtorymm64.dll")) ||        //DXTory
            !scmp(moduleName, TEXT("rtsshooks.dll")) ||         //EVGA Precision OSD
            !scmp(moduleName, TEXT("axonoverlay.dll")) ||       //Dolby Axon
            !scmp(moduleName, TEXT("action_x86.dll")) ||        //Action!
            !scmp(moduleName, TEXT("action_x64.dll")))          //Action!
        {
            Log(TEXT("Incompatible module detected: %s"), moduleName);
            return 1;
        }
        else if (!scmp(moduleName, TEXT("atkdx11disp.dll")))     //ASUS GamerOSD
        {
            //ASUS GamerOSD is so terrible we can't even cleanly shutdown once it loads, trying to unload D3D crashes too (!)
            CrashError (TEXT("ASUS GamerOSD has been detected. This program installs user mode driver hooks which will corrupt the Direct3D state and crash OBS. Please uninstall ASUS GamerOSD if you wish to use OBS."));
        }
    }

    return 0;
}

//This function checks for DLLs that are known to cause problems, but we need to be more specific than a generic "incompatible modules"
//message. As we expect these to change often, I'm not bothering with localization. These DLLs are generally buggy drivers, malware, etc
VOID STDCALL OSCheckForBuggyDLLs ()
{
    StringList  moduleList;

    if (!OSGetLoadedModuleList(GetCurrentProcess(), moduleList))
        return;

    if (moduleList.HasValue(TEXT("sendori.dll")))
    {
        Log(TEXT("BUGGY DLL DETECTED: sendori.dll"));
        OBSMessageBox (hwndMainAppWindow, TEXT("Your system appears to be infected with the Sendori malware, which can crash OBS and cause other problems. Please run a malware scan."), TEXT("Warning"), MB_ICONEXCLAMATION);
    }

    //disabling this for now, it may have been crashing due to a network code bug that was fixed.
    /*if (moduleList.HasValue(TEXT("qproxy.dll")))
    {
        Log(TEXT("BUGGY DLL DETECTED: qproxy.dll"));
        OBSMessageBox (hwndMainAppWindow, TEXT("Your system appears to have Qustodio installed which can cause OBS crashes / lag and other problems. Please remove qproxy.dll by disabling / uninstalling Qustodio, using a tool such as autoruns, or resetting your TCP/IP settings (search KB299357 for more information)."), TEXT("Warning"), MB_ICONEXCLAMATION);
    }*/

    //FIXME: add a version check for bigfoot networks LSP (bfllr.dll), crashes OBS on old versions.
}

OSFileChangeData * STDCALL OSMonitorFileStart(String path, bool suppressLogging)
{
    HANDLE hDirectory;
    OSFileChangeData *data = (OSFileChangeData *)Allocate(sizeof(*data));

    String strDirectory = path;
    strDirectory.FindReplace(TEXT("\\"), TEXT("/"));
    strDirectory = GetPathDirectory(strDirectory);
    strDirectory.FindReplace(TEXT("/"), TEXT("\\"));
    strDirectory << TEXT("\\");

    scpy_n (data->strDirectory, strDirectory.Array(), _countof(data->strDirectory)-1);

    hDirectory = CreateFile(data->strDirectory, FILE_LIST_DIRECTORY, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED, NULL);
    if(hDirectory != INVALID_HANDLE_VALUE)
    {
        DWORD test;
        zero(&data->directoryChange, sizeof(data->directoryChange));
        
        data->directoryChange.hEvent = CreateEvent (NULL, TRUE, FALSE, NULL);

        if(ReadDirectoryChangesW(hDirectory, data->changeBuffer, 2048, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE, &test, &data->directoryChange, NULL))
        {
            scpy_n (data->targetFileName, path.Array(), _countof(data->targetFileName)-1);
            data->hDirectory = hDirectory;
            return data;
        }
        else
        {
            int err = GetLastError();
            CloseHandle(data->directoryChange.hEvent);
            CloseHandle(hDirectory);
            if(!suppressLogging)
                Log(TEXT("OSMonitorFileStart: Unable to monitor file '%s', error %d"), path.Array(), err);
            Free(data);
            return NULL;
        }
    }
    else
    {
        if(!suppressLogging)
        {
            int err = GetLastError();
            Log(TEXT("OSMonitorFileStart: Unable to open directory '%s', error %d"), data->strDirectory, err);
        }
        Free(data);
        return NULL;
    }
}

BOOL STDCALL OSFileHasChanged (OSFileChangeData *data)
{
    BOOL hasModified = FALSE;

    if (!data->hDirectory)
    {
        //we lost our directory handle for some reason, try to re-acquire it
        data->hDirectory = CreateFile(data->strDirectory, FILE_LIST_DIRECTORY, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED, NULL);
        if(data->hDirectory != INVALID_HANDLE_VALUE)
        {
            DWORD test;
            zero(&data->directoryChange, sizeof(data->directoryChange));
        
            data->directoryChange.hEvent = CreateEvent (NULL, TRUE, FALSE, NULL);

            if(ReadDirectoryChangesW(data->hDirectory, data->changeBuffer, 2048, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE, &test, &data->directoryChange, NULL))
            {
            }
            else
            {
                int i = GetLastError ();
                CloseHandle(data->directoryChange.hEvent);
                CloseHandle(data->hDirectory);

                data->directoryChange.hEvent = NULL;
                data->hDirectory = NULL;

                return false;
            }
        }
    }

    if(HasOverlappedIoCompleted(&data->directoryChange))
    {
        FILE_NOTIFY_INFORMATION *notify = (FILE_NOTIFY_INFORMATION*)data->changeBuffer;

        //change triggered, process the notifications
        for (;;)
        {
            if (notify->Action == FILE_ACTION_ADDED || notify->Action == FILE_ACTION_MODIFIED || notify->Action == FILE_ACTION_RENAMED_NEW_NAME)
            {
                String strFileName;
                strFileName.SetLength(notify->FileNameLength);
                scpy_n(strFileName, notify->FileName, notify->FileNameLength/2);
                strFileName.KillSpaces();

                String strFileChanged;
                strFileChanged << data->strDirectory << strFileName;             

                if(strFileChanged.CompareI(data->targetFileName))
                {
                    hasModified = TRUE;
                    break;
                }
            }

            if (!notify->NextEntryOffset)
                break;

            notify = (FILE_NOTIFY_INFORMATION*)((BYTE *)notify + notify->NextEntryOffset);
        }

        //prepare for next read
        ResetEvent (data->directoryChange.hEvent);
        zero(data->changeBuffer, sizeof(data->changeBuffer));

        DWORD test;
        if(ReadDirectoryChangesW(data->hDirectory, data->changeBuffer, 2048, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE, &test, &data->directoryChange, NULL))
        {
        }
        else
        {
            int i = GetLastError ();
            CloseHandle(data->directoryChange.hEvent);
            CloseHandle(data->hDirectory);

            data->directoryChange.hEvent = NULL;
            data->hDirectory = NULL;

            return hasModified;
        }
    }

    return hasModified;
}

VOID STDCALL OSMonitorFileDestroy (OSFileChangeData *data)
{
    if(!HasOverlappedIoCompleted(&data->directoryChange))
    {
        CancelIoEx(data->hDirectory, &data->directoryChange);
        WaitForSingleObject(data->directoryChange.hEvent, INFINITE);
    }

    CloseHandle(data->directoryChange.hEvent);
    CloseHandle(data->hDirectory);
    Free(data);
}

namespace
{
    struct HandleCloser
    {
        void operator()(HANDLE h) { if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h); }
    };

    struct ChangeNotificationCloser
    {
        void operator()(HANDLE h) { if (h && h != INVALID_HANDLE_VALUE) FindCloseChangeNotification(h); }
    };
}

using namespace std;

struct OSDirectoryMonitorData
{
    String directory_path;
    unique_ptr<void, HandleCloser> directory, change_handle, thread, stop_event;
    OSDirectoryMonitorCallback cb;
    ~OSDirectoryMonitorData()
    {
        if (!stop_event || !thread)
            return;
        SetEvent(stop_event.get());
        WaitForSingleObject(thread.get(), INFINITE);
    }
};

static DWORD WINAPI OSMonitorDirectoryThread(void *arg)
{
    OSDirectoryMonitorData &data = *(OSDirectoryMonitorData*)arg;
    HANDLE handles[] = { data.change_handle.get(), data.stop_event.get() };
    BYTE buffer[2048];
    OVERLAPPED ol;
    zero(&ol, sizeof ol);

    ol.hEvent = data.change_handle.get();
    
    for (;;)
    {
        DWORD len = 0;
        if (!ReadDirectoryChangesW(data.directory.get(), buffer, 2048, false, FILE_NOTIFY_CHANGE_FILE_NAME, &len, &ol, nullptr))
            return 1;

        DWORD result = WaitForMultipleObjects(2, handles, false, INFINITE);
        if (result == WAIT_OBJECT_0 + 1) return 2;

        len = 0;
        if (!GetOverlappedResult(data.directory.get(), &ol, &len, false))
            continue;
        
        data.cb();
    }

    return 0;
}

OSDirectoryMonitorData *OSMonitorDirectoryCallback(String path, OSDirectoryMonitorCallback callback)
{
    if (!callback) return nullptr;

    unique_ptr<OSDirectoryMonitorData> data(new OSDirectoryMonitorData);

    data->cb = callback;

    path.FindReplace(L"\\", L"/");

    data->directory_path = GetPathDirectory(path);
    data->directory_path << "/";

    data->directory.reset(CreateFile(data->directory_path.Array(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL));
    if (data->directory.get() == INVALID_HANDLE_VALUE)
        return nullptr;

    data->stop_event.reset(CreateEvent(nullptr, true, false, nullptr));
    if (!data->stop_event)
        return nullptr;

    data->change_handle.reset(CreateEvent(nullptr, false, false, nullptr));
    if (!data->change_handle)
        return nullptr;

    data->thread.reset(CreateThread(nullptr, 0, OSMonitorDirectoryThread, data.get(), 0, nullptr));
    if (!data->thread)
        return nullptr;
    
    return data.release();
}

void OSMonitorDirectoryCallbackStop(OSDirectoryMonitorData *data)
{
    delete data;
}

String STDCALL OSGetDefaultVideoSavePath(CTSTR append)
{
    PWSTR path;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Videos, 0, nullptr, &path)))
        return String();

    String result(path);
    CoTaskMemFree(path);
    return result + append;
}

#endif
