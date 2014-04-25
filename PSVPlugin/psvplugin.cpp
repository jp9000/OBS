/********************************************************************************
 Copyright (C) 2013 HomeWorld <homeworld at gmail dot com>

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

/*
    TODO:
        - Move config to settings panel
        - Add an option to clear saved volumes for all scenes
        - Show every scene with it's volume levels in the settings panel and add ability to change volumes for active/inactive scene(s).
        - Possibly add a custom dialog triggered by a hotkey as an alternative for the behaviour described previously
*/

#include "psvplugin.h"

HINSTANCE   hInstance;
ConfigFile  config;
LocaleStringLookup *pluginLocale = NULL;

bool bPSVEnabled = FALSE;

void LoadSettings()
{
    bPSVEnabled = config.GetInt(TEXT("General"), TEXT("PSVEnabled")) == 1 ? true : false;
}

void SaveSettings()
{
    config.SetInt(TEXT("General"),TEXT("PSVEnabled"), bPSVEnabled ? 1 : 0);
}

void CleanupPSVSettings()
{
    // TO BE IMPLEMENTED
    // remove all the stuff added to scenes config file by this plugin
}

void SaveInitialVolumes()
{
    float dVol, mVol;
    bool dMuted, mMuted;

    if(!bPSVEnabled)
        return;
    
    dVol = OBSGetDesktopVolume();
    mVol = OBSGetMicVolume();

    dMuted = OBSGetDesktopMuted();
    mMuted = OBSGetMicMuted();

    config.SetFloat(TEXT("General"), TEXT("PrevDesktopVolume"), dVol);
    config.SetFloat(TEXT("General"), TEXT("PrevMicVolume"), mVol);

    config.SetInt(TEXT("General"), TEXT("PrevDesktopMuted"), dMuted ? 1 : 0);
    config.SetInt(TEXT("General"), TEXT("PrevMicMuted"), mMuted ? 1 : 0);
    

    OnDesktopVolumeChanged(dVol, dMuted, true);
    OnMicVolumeChanged(mVol, mMuted, true);
}

void RestoreInitialVolumes()
{
    OBSSetDesktopVolume(config.GetFloat(TEXT("General"), TEXT("PrevDesktopVolume")), true);

    OBSSetMicVolume(config.GetFloat(TEXT("General"), TEXT("PrevMicVolume")), true);

    if(config.GetInt(TEXT("General"), TEXT("PrevDesktopMuted")) == 1)
        OBSToggleDesktopMute();

    if(config.GetInt(TEXT("General"), TEXT("PrevMicMuted")) ==1)
        OBSToggleMicMute();
}

INT_PTR CALLBACK ConfigDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    bool bPrevPSVEnabled;
    switch(message)
    {
    case WM_INITDIALOG:
        LocalizeWindow(hWnd, pluginLocale);

        SendMessage(GetDlgItem(hWnd, IDC_TOGGLEPSV), BM_SETCHECK, bPSVEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

        return TRUE;

    case WM_COMMAND:
        switch(LOWORD(wParam))
        {
        case IDOK:
            bPrevPSVEnabled = bPSVEnabled;

            bPSVEnabled = SendMessage(GetDlgItem(hWnd, IDC_TOGGLEPSV), BM_GETCHECK, 0, 0) == BST_CHECKED;

            if(bPSVEnabled && !bPrevPSVEnabled)
                SaveInitialVolumes();
            else if (!bPSVEnabled && bPrevPSVEnabled)
                RestoreInitialVolumes();

            SaveSettings();

        case IDCANCEL:
            EndDialog(hWnd, LOWORD(wParam));
            break;
        }
    }

    return 0;
}

// Entry points

void OnMicVolumeChanged(float level, bool muted, bool finalValue)
{
    XElement *sceneElement;

    if(!bPSVEnabled)
        return;

    sceneElement = OBSGetSceneElement();

    if(sceneElement)
    {
        if(!muted)
            sceneElement->SetFloat(TEXT("psvMicVolume"), level);

        sceneElement->SetInt(TEXT("psvMicMFV"), muted ? PSV_VOL_MUTED : 0);
    }
}

void OnDesktopVolumeChanged(float level, bool muted, bool finalValue)
{
    XElement *sceneElement;

    if(!bPSVEnabled)
        return;

    sceneElement = OBSGetSceneElement();

    if(sceneElement)
    {
        if(!muted)
            sceneElement->SetFloat(TEXT("psvDesktopVolume"), level);

        sceneElement->SetInt(TEXT("psvDesktopMFV"), muted ? PSV_VOL_MUTED : 0);
    }
}

void OnSceneSwitch(CTSTR scene)
{
    XElement *sceneElement;
    float desktopVolumeLevel, micVolumeLevel;
    int dMFV, mMFV;
    bool desktopMuted, micMuted;

    if(!bPSVEnabled)
        return;

    sceneElement = OBSGetSceneElement();

    if(sceneElement)
    {
        desktopVolumeLevel = sceneElement->GetFloat(TEXT("psvDesktopVolume"), 1.0f);
        micVolumeLevel = sceneElement->GetFloat(TEXT("psvMicVolume"), 1.0f);

        dMFV = sceneElement->GetInt(TEXT("psvDesktopMFV"));
        mMFV = sceneElement->GetInt(TEXT("psvMicMFV"));

        desktopMuted = (dMFV & PSV_VOL_MUTED) == PSV_VOL_MUTED;

        micMuted = (mMFV & PSV_VOL_MUTED) == PSV_VOL_MUTED;

        OBSSetDesktopVolume(desktopVolumeLevel, true);

        if(desktopMuted)
            OBSToggleDesktopMute();

        OBSSetMicVolume(micVolumeLevel, true);

        if(micMuted)
            OBSToggleMicMute();
    }
}

void ConfigPlugin(HWND hWnd)
{
    OBSDialogBox(hInstance, MAKEINTRESOURCE(IDD_CONFIGPSV), hWnd, ConfigDlgProc);
}

bool LoadPlugin()
{
    pluginLocale = new LocaleStringLookup;

    if(!pluginLocale->LoadStringFile(TEXT("plugins/PSVPlugin/locale/en.txt")))
        AppWarning(TEXT("Could not open locale string file '%s'"), TEXT("plugins/PSVPlugin/locale/en.txt"));

    if(scmpi(API->GetLanguage(), TEXT("en")) != 0)
    {
        String pluginStringFile;
        pluginStringFile << TEXT("plugins/PSVPlugin/locale/") << API->GetLanguage() << TEXT(".txt");
        if(!pluginLocale->LoadStringFile(pluginStringFile))
            AppWarning(TEXT("Could not open locale string file '%s'"), pluginStringFile.Array());
    }

    config.Open(OBSGetPluginDataPath() + CONFIG_FILE, true);

    LoadSettings();

    OnSceneSwitch(NULL);

    return true;
}

void UnloadPlugin()
{
    delete pluginLocale;
    config.Close();
}


CTSTR GetPluginName()
{
    return PluginStr("Plugin.Name");
}

CTSTR GetPluginDescription()
{
    return PluginStr("Plugin.Description");
}

BOOL CALLBACK DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
#if defined _M_X64 && _MSC_VER == 1800
        //workaround AVX2 bug in VS2013, http://connect.microsoft.com/VisualStudio/feedback/details/811093
        _set_FMA3_enable(0);
#endif
        hInstance = hinstDLL;
    }
    
    return TRUE;
}
