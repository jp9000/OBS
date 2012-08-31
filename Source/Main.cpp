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


#include "Main.h"

#include <dwmapi.h>


//----------------------------

HWND        hwndMain        = NULL;
HWND        hwndRenderFrame = NULL;
HINSTANCE   hinstMain       = NULL;
ConfigFile  *AppConfig      = NULL;
OBS         *App            = NULL;

//----------------------------


void InitSockets();
void TerminateSockets();

BOOL bCompositionEnabled = TRUE;
BOOL bDisableComposition = FALSE;

HANDLE hOBSMutex = NULL;


void LogSystemStats()
{
    HKEY key;
    TCHAR data[1024];
    DWORD dwSize, dwSpeed;

    zero(data, 1024);

    if(RegOpenKey(HKEY_LOCAL_MACHINE, TEXT("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"), &key) != ERROR_SUCCESS)
    {
        AppWarning(TEXT("Could not open system information registry key"));
        return;
    }

#ifdef _WIN64
    Log(TEXT("%s - 64bit"), OBS_VERSION_STRING);
#else
    Log(TEXT("%s - 32bit"), OBS_VERSION_STRING);
#endif

    Log(TEXT("-------------------------------"));

    dwSize = 1024;
    RegQueryValueEx(key, TEXT("ProcessorNameString"), NULL, NULL, (LPBYTE)data, &dwSize);
    Log(TEXT("CPU Name: %s"), sfix(data));

    dwSize = 4;
    RegQueryValueEx(key, TEXT("~MHz"), NULL, NULL, (LPBYTE)&dwSpeed, &dwSize);
    Log(TEXT("CPU Speed: %dMHz"), dwSpeed);

    RegCloseKey(key);

    MEMORYSTATUS ms;

    GlobalMemoryStatus(&ms);

    Log(TEXT("Physical Memory:  %ldMB Total, %ldMB Free"), (ms.dwTotalPhys/1048576), (ms.dwAvailPhys/1048576));
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    //make sure only one instance of the application can be open at a time
    hOBSMutex = CreateMutex(NULL, TRUE, TEXT("OBSMutex"));
    if(GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(hOBSMutex);
        return 0;
    }

    //------------------------------------------------------------

    hinstMain = hInstance;

    //------------------------------------------------------------
    // get the allocator from the config.  problem is we need an allocator to load the config

    CTSTR lpAllocatorString = TEXT("FastAlloc");

    MainAllocator = new DefaultAlloc;

    AppConfig = new ConfigFile;
    if(AppConfig->Open(TEXT("OBS.ini")))
        lpAllocatorString = AppConfig->GetStringPtr(TEXT("General"), TEXT("Allocator"), TEXT("FastAlloc"));

    UINT stringSize = ssize(lpAllocatorString);
    TSTR lpAllocator = (TSTR)malloc(stringSize);
    mcpy(lpAllocator, lpAllocatorString, stringSize);

    delete AppConfig;
    delete MainAllocator;

    //------------------------------------------------------------

    if(InitXT(TEXT("OBS.log"), lpAllocator))
    {
        traceIn(Main);

        EnableProfiling(TRUE);

        LogSystemStats();

        //CoInitializeEx(NULL, COINIT_MULTITHREADED);
        CoInitialize(0);

        InitSockets();

        AppConfig = new ConfigFile;
        if(!AppConfig->Open(TEXT("OBS.ini")))
        {
            if(!AppConfig->Create(TEXT("OBS.ini")))
                CrashError(TEXT("Could not open OBS.ini"));

            AppConfig->SetString(TEXT("General"),        TEXT("Language"),      TEXT("en"));

            AppConfig->SetString(TEXT("Audio"),          TEXT("Device"),        TEXT("Default"));
            AppConfig->SetFloat (TEXT("Audio"),          TEXT("MicVolume"),     1.0f);
            AppConfig->SetFloat (TEXT("Audio"),          TEXT("DesktopVolume"), 1.0f);

            AppConfig->SetInt   (TEXT("Video"),          TEXT("Monitor"),       0);
            AppConfig->SetInt   (TEXT("Video"),          TEXT("FPS"),           25);
            AppConfig->SetFloat (TEXT("Video"),          TEXT("Downscale"),     1.0f);
            AppConfig->SetInt   (TEXT("Video"),          TEXT("DisableAero"),   0);

            AppConfig->SetInt   (TEXT("Video Encoding"), TEXT("BufferSize"),    1000);
            AppConfig->SetInt   (TEXT("Video Encoding"), TEXT("MaxBitrate"),    1000);
            AppConfig->SetString(TEXT("Video Encoding"), TEXT("Preset"),        TEXT("veryfast"));
            AppConfig->SetInt   (TEXT("Video Encoding"), TEXT("Quality"),       8);

            AppConfig->SetInt   (TEXT("Audio Encoding"), TEXT("Format"),        1);
            AppConfig->SetString(TEXT("Audio Encoding"), TEXT("Bitrate"),       TEXT("128"));
            AppConfig->SetString(TEXT("Audio Encoding"), TEXT("Codec"),         TEXT("AAC"));

            AppConfig->SetInt   (TEXT("Publish"),        TEXT("Service"),       0);
            AppConfig->SetInt   (TEXT("Publish"),        TEXT("Mode"),          0);
        }

        bDisableComposition = AppConfig->GetInt(TEXT("Video"), TEXT("DisableAero"), 0);

        if(bDisableComposition)
        {
            DwmIsCompositionEnabled(&bCompositionEnabled);
            if(bCompositionEnabled)
                DwmEnableComposition(DWM_EC_DISABLECOMPOSITION);
        }

        App = new OBS;

        HACCEL hAccel = LoadAccelerators(hinstMain, MAKEINTRESOURCE(IDR_ACCELERATOR1));

        MSG msg;
        while(GetMessage(&msg, NULL, 0, 0))
        {
            if(!TranslateAccelerator(hwndMain, hAccel, &msg) && !IsDialogMessage(hwndMain, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        delete App;
        delete AppConfig;

        if(bDisableComposition && bCompositionEnabled)
            DwmEnableComposition(DWM_EC_ENABLECOMPOSITION);

        TerminateSockets();

        traceOutStop;

        TerminateXT();
    }

    free(lpAllocator);

    //------------------------------------------------------------

    CloseHandle(hOBSMutex);

    return 0;
}
