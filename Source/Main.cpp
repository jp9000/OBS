﻿/********************************************************************************
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

#include <shellapi.h>
#include <shlobj.h>
#include <dwmapi.h>

#include <intrin.h>
#include <inttypes.h>

#include <gdiplus.h>


//----------------------------

HWND        hwndMain        = NULL;
HWND        hwndRenderFrame = NULL;
HINSTANCE   hinstMain       = NULL;
ConfigFile  *GlobalConfig   = NULL;
ConfigFile  *AppConfig      = NULL;
OBS         *App            = NULL;
bool        bIsPortable     = false;
TCHAR       lpAppPath[MAX_PATH];
TCHAR       lpAppDataPath[MAX_PATH];

//----------------------------


void InitSockets();
void TerminateSockets();

void LogVideoCardStats();

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
    Log(TEXT("%s - 64bit (　^ω^)"), OBS_VERSION_STRING);
#else
    Log(TEXT("%s - 32bit (´・ω・｀)"), OBS_VERSION_STRING);
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

    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    BYTE cpuSteppingID  = cpuInfo[0] & 0xF;
    BYTE cpuModel       = (cpuInfo[0]>>4) & 0xF;
    BYTE cpuFamily      = (cpuInfo[0]>>8) & 0xF;
    BYTE cpuType        = (cpuInfo[0]>>12) & 0x3;
    BYTE cpuExtModel    = (cpuInfo[0]>>17) & 0xF;
    BYTE cpuExtFamily   = (cpuInfo[0]>>21) & 0xFF;

    BYTE cpuHTT         = (cpuInfo[3]>>28) & 1;

    Log(TEXT("stepping id: %u, model %u, family %u, type %u, extmodel %u, extfamily %u, HTT %u, logical cores %u, total cores %u"), cpuSteppingID, cpuModel, cpuFamily, cpuType, cpuExtModel, cpuExtFamily, cpuHTT, OSGetLogicalCores(), OSGetTotalCores());

    for(UINT i=0; i<App->NumMonitors(); i++)
    {
        const MonitorInfo &info = App->GetMonitor(i);
        Log(TEXT("monitor %u: pos={%d, %d}, size={%d, %d}"), i+1, info.rect.left, info.rect.top, info.rect.right-info.rect.left, info.rect.bottom-info.rect.top);
    }

    OSVERSIONINFO osvi;
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    GetVersionEx(&osvi);
    Log(TEXT("Windows Version: %u.%u Build %u %S"), osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber, osvi.szCSDVersion);

    BOOL bComposition;
    DwmIsCompositionEnabled(&bComposition);
    Log(TEXT("Aero is %s"), bComposition ? TEXT("Enabled") : TEXT("Disabled"));

    LogVideoCardStats();
}

void InvertPre47Scenes()
{
    String strScenesPath;
    strScenesPath << lpAppDataPath << TEXT("\\scenes.xconfig");

    XConfig scenesConfig;
    if(scenesConfig.Open(strScenesPath))
    {
        XElement *scenes = scenesConfig.GetElement(TEXT("scenes"));
        if(!scenes)
            return;

        UINT numScenes = scenes->NumElements();
        for(UINT i=0; i<numScenes; i++)
        {
            XElement *scene = scenes->GetElementByID(i);
            XElement *sources = scene->GetElement(TEXT("sources"));
            if(!sources)
                continue;

            sources->ReverseOrder();
        }

        scenesConfig.Close(true);
    }
}

void SetupIni()
{
    //first, find out which profile we're using

    String strProfile = GlobalConfig->GetString(TEXT("General"), TEXT("Profile"));
    DWORD lastVersion = GlobalConfig->GetInt(TEXT("General"), TEXT("LastAppVersion"));
    String strIni;

    //--------------------------------------------
    // 0.47a fix (invert sources in all scenes)

    if(lastVersion < 0x470)
        InvertPre47Scenes();

    //--------------------------------------------
    // try to find and open the file, otherwise use the first one available

    bool bFoundProfile = false;

    if(strProfile.IsValid())
    {
        strIni << lpAppDataPath << TEXT("\\profiles\\") << strProfile << TEXT(".ini");
        bFoundProfile = OSFileExists(strIni) != 0;
    }

    if(!bFoundProfile)
    {
        OSFindData ofd;

        strIni.Clear() << lpAppDataPath << TEXT("\\profiles\\*.ini");
        HANDLE hFind = OSFindFirstFile(strIni, ofd);
        if(hFind)
        {
            do
            {
                if(ofd.bDirectory) continue;

                strProfile = GetPathWithoutExtension(ofd.fileName);
                GlobalConfig->SetString(TEXT("General"), TEXT("Profile"), strProfile);
                bFoundProfile = true;

                break;

            } while(OSFindNextFile(hFind, ofd));

            OSFindClose(hFind);
        }
    }

    //--------------------------------------------
    // open, or if no profile found, create one

    if(bFoundProfile)
    {
        strIni.Clear() << lpAppDataPath << TEXT("\\profiles\\") << strProfile << TEXT(".ini");

        if(AppConfig->Open(strIni))
            return;
    }

    strProfile = TEXT("Untitled");
    GlobalConfig->SetString(TEXT("General"), TEXT("Profile"), strProfile);

    strIni.Clear() << lpAppDataPath << TEXT("\\profiles\\") << strProfile << TEXT(".ini");

    if(!AppConfig->Create(strIni))
        CrashError(TEXT("Could not create '%s'"), strIni);

    AppConfig->SetString(TEXT("Audio"),          TEXT("Device"),        TEXT("Default"));
    AppConfig->SetFloat (TEXT("Audio"),          TEXT("MicVolume"),     1.0f);
    AppConfig->SetFloat (TEXT("Audio"),          TEXT("DesktopVolume"), 1.0f);

    AppConfig->SetInt   (TEXT("Video"),          TEXT("Monitor"),       0);
    AppConfig->SetInt   (TEXT("Video"),          TEXT("FPS"),           30);
    AppConfig->SetFloat (TEXT("Video"),          TEXT("Downscale"),     1.0f);
    AppConfig->SetInt   (TEXT("Video"),          TEXT("DisableAero"),   0);

    AppConfig->SetInt   (TEXT("Video Encoding"), TEXT("BufferSize"),    1000);
    AppConfig->SetInt   (TEXT("Video Encoding"), TEXT("MaxBitrate"),    1000);
    AppConfig->SetString(TEXT("Video Encoding"), TEXT("Preset"),        TEXT("veryfast"));
    AppConfig->SetInt   (TEXT("Video Encoding"), TEXT("Quality"),       8);

    AppConfig->SetInt   (TEXT("Audio Encoding"), TEXT("Format"),        1);
    AppConfig->SetString(TEXT("Audio Encoding"), TEXT("Bitrate"),       TEXT("128"));

    AppConfig->SetInt   (TEXT("Publish"),        TEXT("Service"),       0);
    AppConfig->SetInt   (TEXT("Publish"),        TEXT("Mode"),          0);
};

void LoadGlobalIni()
{
    GlobalConfig = new ConfigFile;

    String strGlobalIni;
    strGlobalIni << lpAppDataPath << TEXT("\\global.ini");

    if(!GlobalConfig->Open(strGlobalIni))
    {
        if(!GlobalConfig->Create(strGlobalIni))
            CrashError(TEXT("Could not create '%s'"), strGlobalIni.Array());

        //----------------------
        // first, try to set the app to the system language, defaulting to english if the language doesn't exist

        DWORD bufSize = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SISO639LANGNAME, NULL, 0);

        String str639Lang;
        str639Lang.SetLength(bufSize);

        GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SISO639LANGNAME, str639Lang.Array(), bufSize+1);

        String strLangFile;
        strLangFile << TEXT("locale/") << str639Lang << TEXT(".txt");

        if(!OSFileExists(strLangFile))
            str639Lang = TEXT("en");

        //----------------------

        GlobalConfig->SetString(TEXT("General"), TEXT("Language"), str639Lang);
        GlobalConfig->SetInt(TEXT("General"), TEXT("MaxLogs"), 20);
    }
}

void WINAPI ProcessEvents()
{
    MSG msg;
    while(PeekMessage(&msg, NULL, 0, 0, 1))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

typedef BOOL (WINAPI *getUserModeExceptionProc)(LPDWORD);
typedef BOOL (WINAPI *setUserModeExceptionProc)(DWORD);

void InitializeExceptionHandler()
{
    HMODULE k32;

    //standard app-wide unhandled exception filter
    SetUnhandledExceptionFilter(OBSExceptionHandler);

    //fix for exceptions being swallowed inside callbacks (see KB976038)
    k32 = GetModuleHandle(TEXT("KERNEL32"));
    if (k32)
    {
        DWORD dwFlags;
        getUserModeExceptionProc procGetProcessUserModeExceptionPolicy;
        setUserModeExceptionProc procSetProcessUserModeExceptionPolicy;

        procGetProcessUserModeExceptionPolicy = (getUserModeExceptionProc)GetProcAddress(k32, "GetProcessUserModeExceptionPolicy");
        procSetProcessUserModeExceptionPolicy = (setUserModeExceptionProc)GetProcAddress(k32, "SetProcessUserModeExceptionPolicy");

        if (procGetProcessUserModeExceptionPolicy && procSetProcessUserModeExceptionPolicy)
        {
            if (procGetProcessUserModeExceptionPolicy(&dwFlags))
                procSetProcessUserModeExceptionPolicy(dwFlags & ~1);
        }
    }
}

void SetWorkingFolder(void)
{
    String modulePath;

    if (GetFileAttributes(TEXT("locale\\en.txt")) != INVALID_FILE_ATTRIBUTES)
        return;

    modulePath.SetLength(MAX_PATH);

    if (GetModuleFileName(NULL, modulePath, modulePath.Length()-1))
    {
        TCHAR *p;

        p = srchr(modulePath, '\\');
        if (p)
            *p = 0;
        
        SetCurrentDirectory(modulePath);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    int numArgs;
    LPWSTR *args = CommandLineToArgvW(GetCommandLineW(), &numArgs);

    bool bDisableMutex = false;

    for(int i=1; i<numArgs; i++)
    {
        if(scmpi(args[i], TEXT("-multi")) == 0)
            bDisableMutex = true;
        else if(scmpi(args[i], TEXT("-portable")) == 0)
            bIsPortable = true;
    }

    LocalFree(args);

    //------------------------------------------------------------

    //make sure only one instance of the application can be open at a time
    hOBSMutex = CreateMutex(NULL, TRUE, TEXT("OBSMutex"));
    if(!bDisableMutex && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        hwndMain = FindWindow(OBS_WINDOW_CLASS, NULL);
        if(hwndMain)
            SetForegroundWindow(hwndMain);

        CloseHandle(hOBSMutex);
        return 0;
    }

    //------------------------------------------------------------

    hinstMain = hInstance;
    
    SetProcessDEPPolicy(PROCESS_DEP_ENABLE | PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION);
    InitializeExceptionHandler();

    ULONG_PTR gdipToken;
    const Gdiplus::GdiplusStartupInput gdipInput;
    Gdiplus::GdiplusStartup(&gdipToken, &gdipInput, NULL);

    if(InitXT(NULL, TEXT("DefaultAlloc")))
    {
        InitSockets();
        //CoInitializeEx(NULL, COINIT_MULTITHREADED);
        CoInitialize(0);
        EnableProfiling(TRUE);

        //always make sure we're running inside our app folder so that locale files and plugins work
        SetWorkingFolder();

        //get current working dir
        String strDirectory;
        UINT dirSize = GetCurrentDirectory(0, 0);
        strDirectory.SetLength(dirSize);
        GetCurrentDirectory(dirSize, strDirectory.Array());

        scpy(lpAppPath, strDirectory);

        //if -portable isn't specified in command line check if there's a file named "obs_portable_mode" in current working dir, if so, obs goes into portable mode
        if(!bIsPortable)
        {
            String strPMFileName = lpAppPath;
            strPMFileName += TEXT("\\obs_portable_mode");
            if(OSFileExists(strPMFileName))
                bIsPortable = true;
        }

        TSTR lpAllocator = NULL;

        {
            if(bIsPortable)
                scpy(lpAppDataPath, lpAppPath);
            else
            {
                SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, lpAppDataPath);
                scat_n(lpAppDataPath, TEXT("\\OBS"), 4);
            }

            if(!OSFileExists(lpAppDataPath) && !OSCreateDirectory(lpAppDataPath))
                CrashError(TEXT("Couldn't create directory '%s'"), lpAppDataPath);

            String strAppDataPath = lpAppDataPath;
            String strProfilesPath = strAppDataPath + TEXT("\\profiles");
            if(!OSFileExists(strProfilesPath) && !OSCreateDirectory(strProfilesPath))
                CrashError(TEXT("Couldn't create directory '%s'"), strProfilesPath.Array());

            String strLogsPath = strAppDataPath + TEXT("\\logs");
            if(!OSFileExists(strLogsPath) && !OSCreateDirectory(strLogsPath))
                CrashError(TEXT("Couldn't create directory '%s'"), strLogsPath.Array());

            String strCrashPath = strAppDataPath + TEXT("\\crashDumps");
            if(!OSFileExists(strCrashPath) && !OSCreateDirectory(strCrashPath))
                CrashError(TEXT("Couldn't create directory '%s'"), strCrashPath.Array());

            String strPluginDataPath = strAppDataPath + TEXT("\\pluginData");
            if(!OSFileExists(strPluginDataPath) && !OSCreateDirectory(strPluginDataPath))
                CrashError(TEXT("Couldn't create directory '%s'"), strPluginDataPath.Array());

            String strUpdatePath = strAppDataPath + TEXT("\\updates");
            if(!OSFileExists(strUpdatePath) && !OSCreateDirectory(strUpdatePath))
                CrashError(TEXT("Couldn't create directory '%s'"), strUpdatePath.Array());

            LoadGlobalIni();

            String strAllocator = GlobalConfig->GetString(TEXT("General"), TEXT("Allocator"));
            if(strAllocator.IsValid())
            {
                UINT size = strAllocator.DataLength();
                lpAllocator = (TSTR)malloc(size);
                mcpy(lpAllocator, strAllocator.Array(), size);
            }
        }

        if(lpAllocator)
        {
            delete GlobalConfig;

            ResetXTAllocator(lpAllocator);
            free(lpAllocator);

            LoadGlobalIni();
        }

        //EnableMemoryTracking(true, 8961);

        //--------------------------------------------

        GlobalConfig->SetString(TEXT("General"), TEXT("LastAppDirectory"), strDirectory.Array());

        //--------------------------------------------

        AppConfig = new ConfigFile;
        SetupIni();

        //--------------------------------------------

        String strLogFileWildcard;
        strLogFileWildcard << lpAppDataPath << TEXT("\\logs\\*.log");

        OSFindData ofd;
        HANDLE hFindLogs = OSFindFirstFile(strLogFileWildcard, ofd);
        if(hFindLogs)
        {
            int numLogs = 0;
            String strFirstLog;

            do
            {
                if(ofd.bDirectory) continue;
                if(!numLogs++)
                    strFirstLog << lpAppDataPath << TEXT("\\logs\\") << ofd.fileName;
            } while(OSFindNextFile(hFindLogs, ofd));

            OSFindClose(hFindLogs);

            if(numLogs >= GlobalConfig->GetInt(TEXT("General"), TEXT("MaxLogs"), 20))
                OSDeleteFile(strFirstLog);
        }

        SYSTEMTIME st;
        GetLocalTime(&st);

        String strLog;
        strLog << lpAppDataPath << FormattedString(TEXT("\\logs\\%u-%02u-%02u-%02u%02u-%02u"), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond) << TEXT(".log");

        InitXTLog(strLog);

        //--------------------------------------------

        bDisableComposition = AppConfig->GetInt(TEXT("Video"), TEXT("DisableAero"), 0);

        DwmIsCompositionEnabled(&bCompositionEnabled);
        if(bDisableComposition)
        {
            if(bCompositionEnabled)
                DwmEnableComposition(DWM_EC_DISABLECOMPOSITION);
        }

        //--------------------------------------------

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

        //--------------------------------------------

        GlobalConfig->SetInt(TEXT("General"), TEXT("LastAppVersion"), OBS_VERSION);

        delete AppConfig;
        delete GlobalConfig;

        if(bDisableComposition && bCompositionEnabled)
            DwmEnableComposition(DWM_EC_ENABLECOMPOSITION);

        TerminateSockets();
    }

    Gdiplus::GdiplusShutdown(gdipToken);

    TerminateXT();

    //------------------------------------------------------------

    CloseHandle(hOBSMutex);

    return 0;
}
