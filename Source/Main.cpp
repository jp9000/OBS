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

#include <shlobj.h>
#include <dwmapi.h>

#include <intrin.h>
#include <inttypes.h>


//----------------------------

HWND        hwndMain        = NULL;
HWND        hwndRenderFrame = NULL;
HINSTANCE   hinstMain       = NULL;
ConfigFile  *GlobalConfig   = NULL;
ConfigFile  *AppConfig      = NULL;
OBS         *App            = NULL;
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
    Log(TEXT("%s - 64bit (@^ƒÖ^)"), OBS_VERSION_STRING);
#else
    Log(TEXT("%s - 32bit (LEƒÖEM)"), OBS_VERSION_STRING);
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

    Log(TEXT("stepping id: %u, model %u, family %u, type %u, extmodel %u, extfamily %u"), cpuSteppingID, cpuModel, cpuFamily, cpuType, cpuExtModel, cpuExtFamily);

    LogVideoCardStats();
}


void SetupIni()
{
    //first, find out which profile we're using

    String strProfile = GlobalConfig->GetString(TEXT("General"), TEXT("Profile"));
    String strIni;

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


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    //make sure only one instance of the application can be open at a time
    hOBSMutex = CreateMutex(NULL, TRUE, TEXT("OBSMutex"));
    if(GetLastError() == ERROR_ALREADY_EXISTS)
    {
        hwndMain = FindWindow(OBS_WINDOW_CLASS, NULL);
        if(hwndMain)
            SetForegroundWindow(hwndMain);

        CloseHandle(hOBSMutex);
        return 0;
    }

    //------------------------------------------------------------

    hinstMain = hInstance;

    if(InitXT(NULL, TEXT("FastAlloc")))
    {
        traceIn(Main);

        InitSockets();
        //CoInitializeEx(NULL, COINIT_MULTITHREADED);
        CoInitialize(0);
        EnableProfiling(TRUE);

        TSTR lpAllocator = NULL;

        {
            SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, lpAppDataPath);
            scat_n(lpAppDataPath, TEXT("\\OBS"), 4);

            if(!OSFileExists(lpAppDataPath) && !OSCreateDirectory(lpAppDataPath))
                CrashError(TEXT("Couldn't create directory '%s'"), lpAppDataPath);

            String strAppDataPath = lpAppDataPath;
            String strProfilesPath = strAppDataPath + TEXT("\\profiles");
            if(!OSFileExists(strProfilesPath) && !OSCreateDirectory(strProfilesPath))
                CrashError(TEXT("Couldn't create directory '%s'"), strProfilesPath.Array());

            String strLogsPath = strAppDataPath + TEXT("\\logs");
            if(!OSFileExists(strLogsPath) && !OSCreateDirectory(strLogsPath))
                CrashError(TEXT("Couldn't create directory '%s'"), strLogsPath.Array());

            String strPluginDataPath = strAppDataPath + TEXT("\\pluginData");
            if(!OSFileExists(strPluginDataPath) && !OSCreateDirectory(strPluginDataPath))
                CrashError(TEXT("Couldn't create directory '%s'"), strPluginDataPath.Array());

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

        //--------------------------------------------

        String strDirectory;
        UINT dirSize = GetCurrentDirectory(0, 0);
        strDirectory.SetLength(dirSize);
        GetCurrentDirectory(dirSize, strDirectory.Array());

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

        if(bDisableComposition)
        {
            DwmIsCompositionEnabled(&bCompositionEnabled);
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

        traceOutStop;
    }

    TerminateXT();

    //------------------------------------------------------------

    CloseHandle(hOBSMutex);

    return 0;
}
