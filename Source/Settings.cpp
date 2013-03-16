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
#include "Settings.h"

#include <Winsock2.h>
#include <iphlpapi.h>

//============================================================================
// Helpers

int LoadSettingEditInt(HWND hwnd, CTSTR lpConfigSection, CTSTR lpConfigName, int defVal)
{
    int iLoadVal;
    if(!AppConfig->HasKey(lpConfigSection, lpConfigName))
    {
        AppConfig->SetInt(lpConfigSection, lpConfigName, defVal);
        iLoadVal = defVal;
    }
    else
        iLoadVal = AppConfig->GetInt(lpConfigSection, lpConfigName, defVal);

    SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)IntString(iLoadVal).Array());

    return iLoadVal;
}

String LoadSettingEditString(HWND hwnd, CTSTR lpConfigSection, CTSTR lpConfigName, CTSTR lpDefault)
{
    String strLoadVal;
    if(!AppConfig->HasKey(lpConfigSection, lpConfigName))
    {
        if(lpDefault)
            AppConfig->SetString(lpConfigSection, lpConfigName, lpDefault);

        strLoadVal = lpDefault;
    }
    else
        strLoadVal = AppConfig->GetString(lpConfigSection, lpConfigName, lpDefault);

    SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)strLoadVal.Array());

    return strLoadVal;
}

int LoadSettingComboInt(HWND hwnd, CTSTR lpConfigSection, CTSTR lpConfigName, int defVal, int maxVal)
{
    int curVal = AppConfig->GetInt(lpConfigSection, lpConfigName, defVal);
    int id = curVal;

    if(!AppConfig->HasKey(lpConfigSection, lpConfigName) || curVal < 0 || (maxVal != 0 && curVal > maxVal))
    {
        AppConfig->SetInt(lpConfigSection, lpConfigName, defVal);
        curVal = defVal;
    }

    SendMessage(hwnd, CB_SETCURSEL, (WPARAM)id, 0);

    return curVal;
}

String LoadSettingComboString(HWND hwnd, CTSTR lpConfigSection, CTSTR lpConfigName, CTSTR lpDefault)
{
    String strVal = AppConfig->GetString(lpConfigSection, lpConfigName, lpDefault);
    int id = (int)SendMessage(hwnd, CB_FINDSTRINGEXACT, 0, (LPARAM)strVal.Array());
    if(!AppConfig->HasKey(lpConfigSection, lpConfigName) || id == CB_ERR)
    {
        if(lpDefault)
        {
            AppConfig->SetString(lpConfigSection, lpConfigName, lpDefault);

            if(id == CB_ERR)
            {
                id = (int)SendMessage(hwnd, CB_FINDSTRINGEXACT, -1, (LPARAM)lpDefault);
                strVal = lpDefault;
            }
        }
        else
            id = 0;
    }

    SendMessage(hwnd, CB_SETCURSEL, (WPARAM)id, 0);

    return strVal;
}

String LoadSettingTextComboString(HWND hwnd, CTSTR lpConfigSection, CTSTR lpConfigName, CTSTR lpDefault)
{
    String strVal = AppConfig->GetString(lpConfigSection, lpConfigName, lpDefault);
    int id = (int)SendMessage(hwnd, CB_FINDSTRINGEXACT, -1, (LPARAM)strVal.Array());
    if(!AppConfig->HasKey(lpConfigSection, lpConfigName))
    {
        if(lpDefault)
            AppConfig->SetString(lpConfigSection, lpConfigName, lpDefault);
    }

    SendMessage(hwnd, CB_SETCURSEL, (WPARAM)id, 0);

    return strVal;
}

//============================================================================

struct AudioDeviceStorage {
    AudioDeviceList *playbackDevices;
    AudioDeviceList *recordingDevices;
};

enum SettingsSelection
{
    Settings_Video = 3,
    Settings_Audio,
    Settings_Advanced,
};

BOOL CALLBACK MonitorInfoEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, List<MonitorInfo> &monitors);

void OBS::AddSettingsPane(SettingsPane *pane)
{
    settingsPanes.Add(pane);
}

void OBS::RemoveSettingsPane(SettingsPane *pane)
{
    settingsPanes.RemoveItem(pane);
}

void OBS::AddBuiltInSettingsPanes()
{
    AddSettingsPane(new SettingsGeneral());
    AddSettingsPane(new SettingsEncoding());
    AddSettingsPane(new SettingsPublish());
}

CTSTR preset_names[7] = {TEXT("ultrafast"), TEXT("superfast"), TEXT("veryfast"), TEXT("faster"), TEXT("fast"), TEXT("medium"), TEXT("slow")};

const int multiplierCount = 5;
const float downscaleMultipliers[multiplierCount] = {1.0f, 1.5f, 2.0f, 2.25f, 3.0f};

void OBS::RefreshDownscales(HWND hwnd, int cx, int cy)
{
    int lastID = (int)SendMessage(hwnd, CB_GETCURSEL, 0, 0);

    SendMessage(hwnd, CB_RESETCONTENT, 0, 0);

    float downscale = AppConfig->GetFloat(TEXT("Video"), TEXT("Downscale"));
    bool bFoundVal = false;

    for(int i=0; i<multiplierCount; i++)
    {
        float multiplier = downscaleMultipliers[i];

        int scaleCX = int(float(cx)/multiplier) & 0xFFFFFFFE;
        int scaleCY = int(float(cy)/multiplier) & 0xFFFFFFFE;

        String strText;
        if(i == 0)
            strText << Str("None") << TEXT("  (") << IntString(scaleCX) << TEXT("x") << IntString(scaleCY) << TEXT(")");
        else
            strText << FormattedString(TEXT("%0.2f"), multiplier) << TEXT("  (") << IntString(scaleCX) << TEXT("x") << IntString(scaleCY) << TEXT(")");

        int id = (int)SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)strText.Array());
        SendMessage(hwnd, CB_SETITEMDATA, id, (LPARAM)*(DWORD*)&multiplier);

        if(CloseFloat(downscale, multiplier))
        {
            if(lastID == CB_ERR)
                SendMessage(hwnd, CB_SETCURSEL, id, 0);
            downscale = multiplier;
            bFoundVal = true;
        }
    }

    if(!bFoundVal)
    {
        AppConfig->SetFloat(TEXT("Video"), TEXT("Downscale"), 1.0f);
        if(lastID == CB_ERR)
            SendMessage(hwnd, CB_SETCURSEL, 0, 0);

        SetChangedSettings(true);
    }

    if(lastID != CB_ERR)
        SendMessage(hwnd, CB_SETCURSEL, lastID, 0);
}

FARPROC editProc = NULL;

LRESULT WINAPI ResolutionEditSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if( ((message == WM_KEYDOWN) && (wParam == VK_RETURN)) ||
        (message == WM_KILLFOCUS) )
    {
        String strText = GetEditText(hwnd);
        if(ValidIntString(strText))
        {
            int iVal = strText.ToInt();

            if(iVal < 128)
                strText = TEXT("128");
            else if(iVal > 4096)
                strText = TEXT("4096");
            else
                return CallWindowProc((WNDPROC)editProc, hwnd, message, wParam, lParam);
        }
        else
            strText = TEXT("128");

        SetWindowText(hwnd, strText);
    }

    return CallWindowProc((WNDPROC)editProc, hwnd, message, wParam, lParam);
}

INT_PTR CALLBACK OBS::VideoSettingsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND hwndTemp;
    switch(message)
    {
        case WM_INITDIALOG:
            {
                LocalizeWindow(hwnd);

                //--------------------------------------------

                HWND hwndToolTip = CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL, WS_POPUP|TTS_NOPREFIX|TTS_ALWAYSTIP,
                    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                    hwnd, NULL, hinstMain, NULL);

                TOOLINFO ti;
                zero(&ti, sizeof(ti));
                ti.cbSize = sizeof(ti);
                ti.uFlags = TTF_SUBCLASS|TTF_IDISHWND;
                ti.hwnd = hwnd;

                SendMessage(hwndToolTip, TTM_SETMAXTIPWIDTH, 0, 500);
                SendMessage(hwndToolTip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 8000);

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_MONITOR);

                App->monitors.Clear();
                EnumDisplayMonitors(NULL, NULL, (MONITORENUMPROC)MonitorInfoEnumProc, (LPARAM)&App->monitors);

                for(UINT i=0; i<App->monitors.Num(); i++)
                    SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)IntString(i+1).Array());

                int monitorID = LoadSettingComboInt(hwndTemp, TEXT("Video"), TEXT("Monitor"), 0, App->monitors.Num()-1);
                if(monitorID > (int)App->monitors.Num())
                    monitorID = 0;

                //--------------------------------------------

                SendMessage(GetDlgItem(hwnd, IDC_USECUSTOM), BM_SETCHECK, BST_CHECKED, 0);
                EnableWindow(GetDlgItem(hwnd, IDC_MONITOR), FALSE);

                //--------------------------------------------

                int cx, cy;
                if(!AppConfig->HasKey(TEXT("Video"), TEXT("BaseWidth")) || !AppConfig->HasKey(TEXT("Video"), TEXT("BaseHeight")))
                {
                    cx = App->monitors[monitorID].rect.right  - App->monitors[monitorID].rect.left;
                    cy = App->monitors[monitorID].rect.bottom - App->monitors[monitorID].rect.top;
                    AppConfig->SetInt(TEXT("Video"), TEXT("BaseWidth"),  cx);
                    AppConfig->SetInt(TEXT("Video"), TEXT("BaseHeight"), cy);
                }
                else
                {
                    cx = AppConfig->GetInt(TEXT("Video"), TEXT("BaseWidth"));
                    cy = AppConfig->GetInt(TEXT("Video"), TEXT("BaseHeight"));

                    if(cx < 128)        cx = 128;
                    else if(cx > 4096)  cx = 4096;

                    if(cy < 128)        cy = 128;
                    else if(cy > 4096)  cy = 4096;
                }


                hwndTemp = GetDlgItem(hwnd, IDC_SIZEX);
                editProc = (FARPROC)GetWindowLongPtr(hwndTemp, GWLP_WNDPROC);
                SetWindowLongPtr(hwndTemp, GWLP_WNDPROC, (LONG_PTR)ResolutionEditSubclassProc);
                SetWindowText(hwndTemp, IntString(cx).Array());

                hwndTemp = GetDlgItem(hwnd, IDC_SIZEY);
                SetWindowLongPtr(hwndTemp, GWLP_WNDPROC, (LONG_PTR)ResolutionEditSubclassProc);
                SetWindowText(hwndTemp, IntString(cy).Array());

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_DISABLEAERO);

                if(OSGetVersion() == 8)
                    EnableWindow(hwndTemp, FALSE);

                BOOL bDisableAero = AppConfig->GetInt(TEXT("Video"), TEXT("DisableAero"), 0);
                SendMessage(hwndTemp, BM_SETCHECK, bDisableAero ? BST_CHECKED : 0, 0);

                ti.lpszText = (LPWSTR)Str("Settings.Video.DisableAeroTooltip");
                ti.uId = (UINT_PTR)hwndTemp;
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                //--------------------------------------------

                BOOL bUnlockFPS = AppConfig->GetInt(TEXT("Video"), TEXT("UnlockFPS"));
                int topFPS = bUnlockFPS ? 120 : 60;

                hwndTemp = GetDlgItem(hwnd, IDC_FPS);
                SendMessage(hwndTemp, UDM_SETRANGE32, 10, topFPS);

                int fps = AppConfig->GetInt(TEXT("Video"), TEXT("FPS"), 30);
                if(!AppConfig->HasKey(TEXT("Video"), TEXT("FPS")))
                {
                    AppConfig->SetInt(TEXT("Video"), TEXT("FPS"), 30);
                    fps = 30;
                }
                else if(fps < 10)
                {
                    AppConfig->SetInt(TEXT("Video"), TEXT("FPS"), 10);
                    fps = 10;
                }
                else if(fps > topFPS)
                {
                    AppConfig->SetInt(TEXT("Video"), TEXT("FPS"), topFPS);
                    fps = topFPS;
                }

                SendMessage(hwndTemp, UDM_SETPOS32, 0, fps);

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_DOWNSCALE);
                App->RefreshDownscales(hwndTemp, cx, cy);

                ti.lpszText = (LPWSTR)Str("Settings.Video.DownscaleTooltip");
                ti.uId = (UINT_PTR)hwndTemp;
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                //--------------------------------------------

                ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_HIDE);
                App->SetChangedSettings(false);

                return TRUE;
            }

        case WM_COMMAND:
            {
                bool bDataChanged = false;

                switch(LOWORD(wParam))
                {
                    case IDC_MONITOR:
                        {
                            if(HIWORD(wParam) != CBN_SELCHANGE)
                                break;

                            int sel = (int)SendMessage(GetDlgItem(hwnd, IDC_MONITOR), CB_GETCURSEL, 0, 0);
                            if(sel != CB_ERR)
                            {
                                if(sel >= (int)App->monitors.Num())
                                    sel = 0;

                                MonitorInfo &monitor = App->monitors[sel];

                                int cx, cy;
                                cx = monitor.rect.right  - monitor.rect.left;
                                cy = monitor.rect.bottom - monitor.rect.top;

                                SetWindowText(GetDlgItem(hwnd, IDC_SIZEX), IntString(cx).Array());
                                SetWindowText(GetDlgItem(hwnd, IDC_SIZEY), IntString(cy).Array());
                            }
                            break;
                        }

                    case IDC_USECUSTOM:
                        SendMessage(GetDlgItem(hwnd, IDC_SIZEX), EM_SETREADONLY, FALSE, 0);
                        SendMessage(GetDlgItem(hwnd, IDC_SIZEY), EM_SETREADONLY, FALSE, 0);
                        EnableWindow(GetDlgItem(hwnd, IDC_MONITOR), FALSE);
                        break;

                    case IDC_USEMONITOR:
                        {
                            SendMessage(GetDlgItem(hwnd, IDC_SIZEX), EM_SETREADONLY, TRUE, 0);
                            SendMessage(GetDlgItem(hwnd, IDC_SIZEY), EM_SETREADONLY, TRUE, 0);
                            EnableWindow(GetDlgItem(hwnd, IDC_MONITOR), TRUE);

                            int sel = (int)SendMessage(GetDlgItem(hwnd, IDC_MONITOR), CB_GETCURSEL, 0, 0);
                            if(sel != CB_ERR)
                            {
                                if(sel >= (int)App->monitors.Num())
                                    sel = 0;

                                MonitorInfo &monitor = App->monitors[sel];

                                int cx, cy;
                                cx = monitor.rect.right  - monitor.rect.left;
                                cy = monitor.rect.bottom - monitor.rect.top;

                                SetWindowText(GetDlgItem(hwnd, IDC_SIZEX), IntString(cx).Array());
                                SetWindowText(GetDlgItem(hwnd, IDC_SIZEY), IntString(cy).Array());
                            }
                            break;
                        }

                    case IDC_SIZEX:
                    case IDC_SIZEY:
                        {
                            if(HIWORD(wParam) != EN_CHANGE)
                                break;

                            String strInt = GetEditText((HWND)lParam);
                            int iVal = strInt.ToInt();

                            int cx = GetEditText(GetDlgItem(hwnd, IDC_SIZEX)).ToInt();
                            int cy = GetEditText(GetDlgItem(hwnd, IDC_SIZEY)).ToInt();

                            if(cx < 128)        cx = 128;
                            else if(cx > 4096)  cx = 4096;

                            if(cy < 128)        cy = 128;
                            else if(cy > 4096)  cy = 4096;

                            App->RefreshDownscales(GetDlgItem(hwnd, IDC_DOWNSCALE), cx, cy);

                            bDataChanged = true;
                            break;
                        }

                    case IDC_DISABLEAERO:
                        if(HIWORD(wParam) == BN_CLICKED)
                            bDataChanged = true;
                        break;

                    case IDC_FPS_EDIT:
                        if(HIWORD(wParam) == EN_CHANGE)
                            bDataChanged = true;
                        break;

                    case IDC_DOWNSCALE:
                        if(HIWORD(wParam) == CBN_SELCHANGE)
                            bDataChanged = true;
                        break;
                }

                if(bDataChanged)
                {
                    ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                    App->SetChangedSettings(true);
                }
                break;
            }
    }
    return FALSE;
}

INT_PTR CALLBACK OBS::AudioSettingsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                LocalizeWindow(hwnd);

                //--------------------------------------------

                AudioDeviceStorage *storage = new AudioDeviceStorage;

                storage->playbackDevices = new AudioDeviceList;
                GetAudioDevices((*storage->playbackDevices), ADT_PLAYBACK);

                storage->recordingDevices = new AudioDeviceList;
                GetAudioDevices((*storage->recordingDevices), ADT_RECORDING);

                HWND hwndTemp = GetDlgItem(hwnd, IDC_MICDEVICES);
                HWND hwndPlayback = GetDlgItem(hwnd, IDC_PLAYBACKDEVICES);

                for(UINT i=0; i<storage->playbackDevices->devices.Num(); i++)
                    SendMessage(hwndPlayback, CB_ADDSTRING, 0, (LPARAM)storage->playbackDevices->devices[i].strName.Array());

                for(UINT i=0; i<storage->recordingDevices->devices.Num(); i++)
                    SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)storage->recordingDevices->devices[i].strName.Array());

                String strPlaybackID = AppConfig->GetString(TEXT("Audio"), TEXT("PlaybackDevice"), storage->playbackDevices->devices[0].strID);
                String strDeviceID = AppConfig->GetString(TEXT("Audio"), TEXT("Device"), storage->recordingDevices->devices[0].strID);

                UINT iPlaybackDevice;
                for(iPlaybackDevice=0; iPlaybackDevice<storage->playbackDevices->devices.Num(); iPlaybackDevice++)
                {
                    if(storage->playbackDevices->devices[iPlaybackDevice].strID == strPlaybackID)
                    {
                        SendMessage(hwndPlayback, CB_SETCURSEL, iPlaybackDevice, 0);
                        break;
                    }
                }

                UINT iDevice;
                for(iDevice=0; iDevice<storage->recordingDevices->devices.Num(); iDevice++)
                {
                    if(storage->recordingDevices->devices[iDevice].strID == strDeviceID)
                    {
                        SendMessage(hwndTemp, CB_SETCURSEL, iDevice, 0);
                        break;
                    }
                }

                if(iPlaybackDevice == storage->playbackDevices->devices.Num())
                {
                    AppConfig->SetString(TEXT("Audio"), TEXT("PlaybackDevice"), storage->playbackDevices->devices[0].strID);
                    SendMessage(hwndPlayback, CB_SETCURSEL, 0, 0);
                }

                if(iDevice == storage->recordingDevices->devices.Num())
                {
                    AppConfig->SetString(TEXT("Audio"), TEXT("Device"), storage->recordingDevices->devices[0].strID);
                    SendMessage(hwndTemp, CB_SETCURSEL, 0, 0);
                }

                //--------------------------------------------

                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)storage);

                BOOL bPushToTalk = AppConfig->GetInt(TEXT("Audio"), TEXT("UsePushToTalk"));
                SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALK), BM_SETCHECK, bPushToTalk ? BST_CHECKED : BST_UNCHECKED, 0);
                EnableWindow(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY), bPushToTalk);
                EnableWindow(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY2), bPushToTalk);
                EnableWindow(GetDlgItem(hwnd, IDC_CLEARPUSHTOTALK), bPushToTalk);
                EnableWindow(GetDlgItem(hwnd, IDC_PTTDELAY_EDIT), bPushToTalk);
                EnableWindow(GetDlgItem(hwnd, IDC_PTTDELAY), bPushToTalk);

                DWORD hotkey = AppConfig->GetInt(TEXT("Audio"), TEXT("PushToTalkHotkey"));
                SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY), HKM_SETHOTKEY, hotkey, 0);
                DWORD hotkey2 = AppConfig->GetInt(TEXT("Audio"), TEXT("PushToTalkHotkey2"));
                SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY2), HKM_SETHOTKEY, hotkey2, 0);

                int pttDelay = AppConfig->GetInt(TEXT("Audio"), TEXT("PushToTalkDelay"), 200);
                SendMessage(GetDlgItem(hwnd, IDC_PTTDELAY), UDM_SETRANGE32, 0, 2000);
                SendMessage(GetDlgItem(hwnd, IDC_PTTDELAY), UDM_SETPOS32, 0, pttDelay);

                //--------------------------------------------

                hotkey = AppConfig->GetInt(TEXT("Audio"), TEXT("MuteMicHotkey"));
                SendMessage(GetDlgItem(hwnd, IDC_MUTEMICHOTKEY), HKM_SETHOTKEY, hotkey, 0);

                //--------------------------------------------

                hotkey = AppConfig->GetInt(TEXT("Audio"), TEXT("MuteDesktopHotkey"));
                SendMessage(GetDlgItem(hwnd, IDC_MUTEDESKTOPHOTKEY), HKM_SETHOTKEY, hotkey, 0);

                //--------------------------------------------

                BOOL bForceMono = AppConfig->GetInt(TEXT("Audio"), TEXT("ForceMicMono"));
                SendMessage(GetDlgItem(hwnd, IDC_FORCEMONO), BM_SETCHECK, bForceMono ? BST_CHECKED : BST_UNCHECKED, 0);

                //--------------------------------------------

                DWORD desktopBoost = GlobalConfig->GetInt(TEXT("Audio"), TEXT("DesktopBoostMultiple"), 1);
                if(desktopBoost < 1)
                    desktopBoost = 1;
                else if(desktopBoost > 20)
                    desktopBoost = 20;
                SendMessage(GetDlgItem(hwnd, IDC_DESKTOPBOOST), UDM_SETRANGE32, 1, 20);
                SendMessage(GetDlgItem(hwnd, IDC_DESKTOPBOOST), UDM_SETPOS32, 0, desktopBoost);

                //--------------------------------------------

                DWORD micBoost = AppConfig->GetInt(TEXT("Audio"), TEXT("MicBoostMultiple"), 1);
                if(micBoost < 1)
                    micBoost = 1;
                else if(micBoost > 20)
                    micBoost = 20;
                SendMessage(GetDlgItem(hwnd, IDC_MICBOOST), UDM_SETRANGE32, 1, 20);
                SendMessage(GetDlgItem(hwnd, IDC_MICBOOST), UDM_SETPOS32, 0, micBoost);

                //--------------------------------------------

                int bufferTime = GlobalConfig->GetInt(TEXT("General"), TEXT("SceneBufferingTime"), 400);

                int micTimeOffset = AppConfig->GetInt(TEXT("Audio"), TEXT("MicTimeOffset"), 0);
                if(micTimeOffset < -bufferTime)
                    micTimeOffset = -bufferTime;
                else if(micTimeOffset > 3000)
                    micTimeOffset = 3000;

                SendMessage(GetDlgItem(hwnd, IDC_MICTIMEOFFSET), UDM_SETRANGE32, -bufferTime, 3000);
                SendMessage(GetDlgItem(hwnd, IDC_MICTIMEOFFSET), UDM_SETPOS32, 0, micTimeOffset);

                //--------------------------------------------

                App->SetChangedSettings(false);
                return TRUE;
            }

        case WM_DESTROY:
            {
                AudioDeviceStorage* storage = (AudioDeviceStorage*)GetWindowLongPtr(hwnd, DWLP_USER);
                delete storage->recordingDevices;
                delete storage->playbackDevices;
                delete storage;
            }

        case WM_COMMAND:
            {
                bool bDataChanged = false;

                switch(LOWORD(wParam))
                {
                    case IDC_PUSHTOTALK:
                        if(HIWORD(wParam) == BN_CLICKED)
                        {
                            BOOL bUsePushToTalk = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;
                            EnableWindow(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY), bUsePushToTalk);
                            EnableWindow(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY2), bUsePushToTalk);
                            EnableWindow(GetDlgItem(hwnd, IDC_CLEARPUSHTOTALK), bUsePushToTalk);
                            EnableWindow(GetDlgItem(hwnd, IDC_PTTDELAY_EDIT), bUsePushToTalk);
                            EnableWindow(GetDlgItem(hwnd, IDC_PTTDELAY), bUsePushToTalk);
                            App->SetChangedSettings(true);
                        }
                        break;

                    case IDC_MICTIMEOFFSET_EDIT:
                    case IDC_DESKTOPBOOST_EDIT:
                    case IDC_MICBOOST_EDIT:
                    case IDC_PUSHTOTALKHOTKEY:
                    case IDC_PUSHTOTALKHOTKEY2:
                    case IDC_MUTEMICHOTKEY:
                    case IDC_MUTEDESKTOPHOTKEY:
                    case IDC_PTTDELAY_EDIT:
                        if(HIWORD(wParam) == EN_CHANGE)
                            App->SetChangedSettings(true);
                        break;

                    case IDC_CLEARPUSHTOTALK:
                        if(HIWORD(wParam) == BN_CLICKED)
                        {
                            SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY), HKM_SETHOTKEY, 0, 0);
                            SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY2), HKM_SETHOTKEY, 0, 0);
                            App->SetChangedSettings(true);
                        }
                        break;

                    case IDC_CLEARMUTEMIC:
                        if(HIWORD(wParam) == BN_CLICKED)
                        {
                            SendMessage(GetDlgItem(hwnd, IDC_MUTEMICHOTKEY), HKM_SETHOTKEY, 0, 0);
                            App->SetChangedSettings(true);
                        }
                        break;

                    case IDC_CLEARMUTEDESKTOP:
                        if(HIWORD(wParam) == BN_CLICKED)
                        {
                            SendMessage(GetDlgItem(hwnd, IDC_MUTEDESKTOPHOTKEY), HKM_SETHOTKEY, 0, 0);
                            App->SetChangedSettings(true);
                        }
                        break;

                    case IDC_FORCEMONO:
                        if(HIWORD(wParam) == BN_CLICKED)
                            App->SetChangedSettings(true);
                        break;

                    case IDC_MICDEVICES:
                        if(HIWORD(wParam) == CBN_SELCHANGE)
                            bDataChanged = true;
                        break;

                    case IDC_PLAYBACKDEVICES:
                        if(HIWORD(wParam) == CBN_SELCHANGE)
                            bDataChanged = true;
                        break;
                }

                if(bDataChanged)
                {
                    ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                    App->SetChangedSettings(true);
                }
                break;
            }
    }
    return FALSE;
}

INT_PTR CALLBACK OBS::AdvancedSettingsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                LocalizeWindow(hwnd);

                //--------------------------------------------

                HWND hwndToolTip = CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL, WS_POPUP|TTS_NOPREFIX|TTS_ALWAYSTIP,
                    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                    hwnd, NULL, hinstMain, NULL);

                TOOLINFO ti;
                zero(&ti, sizeof(ti));
                ti.cbSize = sizeof(ti);
                ti.uFlags = TTF_SUBCLASS|TTF_IDISHWND;
                ti.hwnd = hwnd;

                SendMessage(hwndToolTip, TTM_SETMAXTIPWIDTH, 0, 500);
                SendMessage(hwndToolTip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 14000);

                //------------------------------------

                UINT sceneBufferingTime = GlobalConfig->GetInt(TEXT("General"), TEXT("SceneBufferingTime"), 400);
                SendMessage(GetDlgItem(hwnd, IDC_SCENEBUFFERTIME), UDM_SETRANGE32, 100, 1000);
                SendMessage(GetDlgItem(hwnd, IDC_SCENEBUFFERTIME), UDM_SETPOS32, 0, sceneBufferingTime);

                //------------------------------------

                bool bUseMTOptimizations = AppConfig->GetInt(TEXT("General"), TEXT("UseMultithreadedOptimizations"), TRUE) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_USEMULTITHREADEDOPTIMIZATIONS), BM_SETCHECK, bUseMTOptimizations ? BST_CHECKED : BST_UNCHECKED, 0);

                HWND hwndTemp = GetDlgItem(hwnd, IDC_PRIORITY);
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("High"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("Above Normal"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("Normal"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("Idle"));

                LoadSettingComboString(hwndTemp, TEXT("General"), TEXT("Priority"), TEXT("Normal"));

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_PRESET);
                for(int i=0; i<7; i++)
                    SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)preset_names[i]);

                LoadSettingComboString(hwndTemp, TEXT("Video Encoding"), TEXT("Preset"), TEXT("veryfast"));

                ti.lpszText = (LPWSTR)Str("Settings.Advanced.VideoEncoderCPUTradeoffTooltip");
                ti.uId = (UINT_PTR)hwndTemp;
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                //------------------------------------

                bool bUseCFR = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseCFR"), 0) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_USECFR), BM_SETCHECK, bUseCFR ? BST_CHECKED : BST_UNCHECKED, 0);

                //------------------------------------

                bool bUseCustomX264Settings = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseCustomSettings")) != 0;
                String strX264Settings = AppConfig->GetString(TEXT("Video Encoding"), TEXT("CustomSettings"));

                SendMessage(GetDlgItem(hwnd, IDC_USEVIDEOENCODERSETTINGS), BM_SETCHECK, bUseCustomX264Settings ? BST_CHECKED : BST_UNCHECKED, 0);
                SetWindowText(GetDlgItem(hwnd, IDC_VIDEOENCODERSETTINGS), strX264Settings);

                ti.lpszText = (LPWSTR)Str("Settings.Advanced.VideoEncoderSettingsTooltip");
                ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_VIDEOENCODERSETTINGS);
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_USEVIDEOENCODERSETTINGS);
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                EnableWindow(GetDlgItem(hwnd, IDC_VIDEOENCODERSETTINGS), bUseCustomX264Settings);

                //--------------------------------------------

                bool bUnlockFPS = AppConfig->GetInt(TEXT("Video"), TEXT("UnlockFPS")) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_UNLOCKHIGHFPS), BM_SETCHECK, bUnlockFPS ? BST_CHECKED : BST_UNCHECKED, 0);

                //------------------------------------

                /*bool bDisableD3DCompat = AppConfig->GetInt(TEXT("Video"), TEXT("DisableD3DCompatibilityMode")) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_DISABLED3DCOMPATIBILITY), BM_SETCHECK, bDisableD3DCompat ? BST_CHECKED : BST_UNCHECKED, 0);

                ti.lpszText = (LPWSTR)Str("Settings.Advanced.DisableD3DCompatibilityModeTooltip");
                ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_DISABLED3DCOMPATIBILITY);
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);*/

                //------------------------------------

                bool bUseHQResampling = AppConfig->GetInt(TEXT("Audio"), TEXT("UseHighQualityResampling"), FALSE) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_USEHIGHQUALITYRESAMPLING), BM_SETCHECK, bUseHQResampling ? BST_CHECKED : BST_UNCHECKED, 0);

                ti.lpszText = (LPWSTR)Str("Settings.Advanced.UseHighQualityResamplingTooltip");
                ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_USEHIGHQUALITYRESAMPLING);
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                //------------------------------------

                bool bSyncToVideoTime = AppConfig->GetInt(TEXT("Audio"), TEXT("SyncToVideoTime")) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_SYNCTOVIDEOTIME), BM_SETCHECK, bSyncToVideoTime ? BST_CHECKED : BST_UNCHECKED, 0);

                //------------------------------------

                bool bUseMicQPC = GlobalConfig->GetInt(TEXT("Audio"), TEXT("UseMicQPC")) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_USEMICQPC), BM_SETCHECK, bUseMicQPC ? BST_CHECKED : BST_UNCHECKED, 0);

                //------------------------------------

                int bufferTime = GlobalConfig->GetInt(TEXT("General"), TEXT("SceneBufferingTime"), 400);

                //GlobalConfig->SetInt(TEXT("Audio"), TEXT("GlobalAudioTimeAdjust"), 0);

                int globalAudioTimeAdjust = GlobalConfig->GetInt(TEXT("Audio"), TEXT("GlobalAudioTimeAdjust"));
                SendMessage(GetDlgItem(hwnd, IDC_AUDIOTIMEADJUST), UDM_SETRANGE32, -bufferTime, 1000);
                SendMessage(GetDlgItem(hwnd, IDC_AUDIOTIMEADJUST), UDM_SETPOS32, 0, globalAudioTimeAdjust);

                //------------------------------------

                int lowLatencyFactor = AppConfig->GetInt(TEXT("Publish"), TEXT("LatencyFactor"), 20);
                SetDlgItemInt(hwnd, IDC_LATENCYTUNE, lowLatencyFactor, TRUE);

                int bLowLatencyAutoMethod = AppConfig->GetInt(TEXT("Publish"), TEXT("LowLatencyMethod"), 0);
                SendMessage(GetDlgItem(hwnd, IDC_LATENCYMETHOD), BM_SETCHECK, bLowLatencyAutoMethod ? BST_CHECKED : BST_UNCHECKED, 0);

                //------------------------------------

                MIB_IPADDRTABLE tempTable;
                DWORD dwSize = 0;
                if (GetIpAddrTable (&tempTable, &dwSize, TRUE) == ERROR_INSUFFICIENT_BUFFER)
                {
                    PMIB_IPADDRTABLE ipTable;

                    ipTable = (PMIB_IPADDRTABLE)Allocate(dwSize);

                    if (GetIpAddrTable (ipTable, &dwSize, TRUE) == NO_ERROR)
                    {
                        DWORD i;

                        hwndTemp = GetDlgItem(hwnd, IDC_BINDIP);
                        SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("Default"));

                        for (i=0; i < ipTable->dwNumEntries; i++)
                        {
                            String strAddress;
                            DWORD strLength = 32;

                            // don't allow binding to localhost
                            if ((ipTable->table[i].dwAddr & 0xFF) == 127)
                                continue;

                            strAddress.SetLength(strLength);

                            SOCKADDR_IN IP;

                            IP.sin_addr.S_un.S_addr = ipTable->table[i].dwAddr;
                            IP.sin_family = AF_INET;
                            IP.sin_port = 0;
                            zero(&IP.sin_zero, sizeof(IP.sin_zero));

                            WSAAddressToString ((LPSOCKADDR)&IP, sizeof(IP), NULL, strAddress.Array(), &strLength);
                            SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)strAddress.Array());
                        }

                        LoadSettingComboString(hwndTemp, TEXT("Publish"), TEXT("BindToIP"), TEXT("Default"));
                    }

                    Free(ipTable);
                }

                //need this as some of the dialog item sets above trigger the notifications
                ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_HIDE);
                App->SetChangedSettings(false);
                return TRUE;
            }

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_USEVIDEOENCODERSETTINGS:
                    if(HIWORD(wParam) == BN_CLICKED)
                    {
                        BOOL bUseVideoEncoderSettings = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;
                        EnableWindow(GetDlgItem(hwnd, IDC_VIDEOENCODERSETTINGS), bUseVideoEncoderSettings);

                        ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                        App->SetChangedSettings(true);
                    }
                    break;

                case IDC_SCENEBUFFERTIME_EDIT:
                case IDC_AUDIOTIMEADJUST_EDIT:
                case IDC_VIDEOENCODERSETTINGS:
                case IDC_LATENCYTUNE:
                    if(HIWORD(wParam) == EN_CHANGE)
                    {
                        ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                        App->SetChangedSettings(true);
                    }
                    break;

                /*case IDC_TIMER1:
                case IDC_TIMER2:
                case IDC_TIMER3:
                case IDC_DISABLED3DCOMPATIBILITY:
                    if(HIWORD(wParam) == BN_CLICKED)
                    {
                        ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                        App->SetChangedSettings(true);
                    }
                    break;*/

                case IDC_USESENDBUFFER:
                    if(HIWORD(wParam) == BN_CLICKED)
                    {
                        BOOL bUseSendBuffer = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;
                        EnableWindow(GetDlgItem(hwnd, IDC_SENDBUFFERSIZE), bUseSendBuffer);

                        ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                        App->SetChangedSettings(true);
                    }
                    break;

                case IDC_PRESET:
                    if(HIWORD(wParam) == CBN_SELCHANGE)
                    {
                        HWND hwndTemp = (HWND)lParam;

                        String strNewPreset = GetCBText(hwndTemp);
                        if (scmp(strNewPreset.Array(), AppConfig->GetString(TEXT("Video Encoding"), TEXT("Preset"), TEXT("veryfast"))))
                        {
                            static BOOL bHasWarned = FALSE;
                            if (!bHasWarned && MessageBox(hwnd, Str("Settings.Advanced.PresetWarning"), NULL, MB_ICONEXCLAMATION | MB_YESNO) == IDNO)
                            {
                                LoadSettingComboString(hwndTemp, TEXT("Video Encoding"), TEXT("Preset"), TEXT("veryfast"));
                            }
                            else
                            {
                                bHasWarned = TRUE;
                                ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                                App->SetChangedSettings(true);
                            }
                        }
                    }
                    break;

                case IDC_SENDBUFFERSIZE:
                case IDC_PRIORITY:
                case IDC_BINDIP:
                    if(HIWORD(wParam) == CBN_SELCHANGE || HIWORD(wParam) == CBN_EDITCHANGE)
                    {
                        ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                        App->SetChangedSettings(true);
                    }
                    break;

                case IDC_USEMICQPC:
                case IDC_SYNCTOVIDEOTIME:
                case IDC_USECFR:
                case IDC_USEHIGHQUALITYRESAMPLING:
                case IDC_USEMULTITHREADEDOPTIMIZATIONS:
                case IDC_UNLOCKHIGHFPS:
                case IDC_LATENCYMETHOD:
                    if(HIWORD(wParam) == BN_CLICKED)
                    {
                        ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                        App->SetChangedSettings(true);
                    }
                    break;
            }

    }
    return FALSE;
}


void OBS::CancelSettings()
{
    if(App->currentSettingsPane != NULL)
        App->currentSettingsPane->CancelSettings();
}

void OBS::ApplySettings()
{
    if(App->currentSettingsPane != NULL)
    {
        App->currentSettingsPane->ApplySettings();
        bSettingsChanged = false;
        EnableWindow(GetDlgItem(hwndSettings, IDC_APPLY), FALSE);
        return;
    }

    // FIXME: This is temporary until all the built-in panes are ported to the new interface
    switch(curSettingsSelection)
    {
        case Settings_Video:
            {
                int curSel = (int)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_MONITOR), CB_GETCURSEL, 0, 0);
                if(curSel != CB_ERR)
                    AppConfig->SetInt(TEXT("Video"), TEXT("Monitor"), curSel);

                int iVal = GetEditText(GetDlgItem(hwndCurrentSettings, IDC_SIZEX)).ToInt();
                if(iVal >=  128)
                    AppConfig->SetInt(TEXT("Video"), TEXT("BaseWidth"), iVal);

                iVal = GetEditText(GetDlgItem(hwndCurrentSettings, IDC_SIZEY)).ToInt();
                if(iVal >= 128)
                    AppConfig->SetInt(TEXT("Video"), TEXT("BaseHeight"), iVal);

                BOOL bDisableAero = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_DISABLEAERO), BM_GETCHECK, 0, 0) == BST_CHECKED ? TRUE : FALSE;
                AppConfig->SetInt(TEXT("Video"), TEXT("DisableAero"), bDisableAero);

                BOOL bFailed;
                int fps = (int)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_FPS), UDM_GETPOS32, 0, (LPARAM)&bFailed);
                AppConfig->SetInt(TEXT("Video"), TEXT("FPS"), (bFailed) ? 30 : fps);

                curSel = (int)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_DOWNSCALE), CB_GETCURSEL, 0, 0);
                if(curSel != CB_ERR)
                    AppConfig->SetFloat(TEXT("Video"), TEXT("Downscale"), downscaleMultipliers[curSel]);

                int gammaVal = (int)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_GAMMA), TBM_GETPOS, 0, 0);
                AppConfig->SetInt(TEXT("Video"), TEXT("Gamma"), gammaVal);

                //------------------------------------

                if(!bRunning)
                    ResizeWindow(false);

                break;
            }

        case Settings_Audio:
            {
                AudioDeviceStorage *storage = (AudioDeviceStorage*)GetWindowLongPtr(hwndCurrentSettings, DWLP_USER);
                UINT iPlaybackDevice = (UINT)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_PLAYBACKDEVICES), CB_GETCURSEL, 0, 0);
                String strPlaybackDevice;

                if(iPlaybackDevice == CB_ERR) {
                    strPlaybackDevice = TEXT("Default");
                }
                else {
                    strPlaybackDevice = storage->playbackDevices->devices[iPlaybackDevice].strID;
                }

                AppConfig->SetString(TEXT("Audio"), TEXT("PlaybackDevice"), strPlaybackDevice);

                UINT iDevice = (UINT)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_MICDEVICES), CB_GETCURSEL, 0, 0);

                String strDevice;

                if(iDevice == CB_ERR)
                    strDevice = TEXT("Disable");
                else
                    strDevice = storage->recordingDevices->devices[iDevice].strID;


                AppConfig->SetString(TEXT("Audio"), TEXT("Device"), strDevice);


                if(strDevice.CompareI(TEXT("Disable")))
                    EnableWindow(GetDlgItem(hwndMain, ID_MICVOLUME), FALSE);
                else
                    EnableWindow(GetDlgItem(hwndMain, ID_MICVOLUME), TRUE);

                //------------------------------------

                bUsingPushToTalk = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_PUSHTOTALK), BM_GETCHECK, 0, 0) == BST_CHECKED;
                DWORD hotkey = (DWORD)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_PUSHTOTALKHOTKEY), HKM_GETHOTKEY, 0, 0);
                DWORD hotkey2 = (DWORD)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_PUSHTOTALKHOTKEY2), HKM_GETHOTKEY, 0, 0);

                pushToTalkDelay = (int)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_PTTDELAY), UDM_GETPOS32, 0, 0);
                if(pushToTalkDelay < 0)
                    pushToTalkDelay = 0;
                else if(pushToTalkDelay > 2000)
                    pushToTalkDelay = 2000;

                AppConfig->SetInt(TEXT("Audio"), TEXT("UsePushToTalk"), bUsingPushToTalk);
                AppConfig->SetInt(TEXT("Audio"), TEXT("PushToTalkHotkey"), hotkey);
                AppConfig->SetInt(TEXT("Audio"), TEXT("PushToTalkHotkey2"), hotkey2);
                AppConfig->SetInt(TEXT("Audio"), TEXT("PushToTalkDelay"), pushToTalkDelay);

                if(App->pushToTalkHotkeyID)
                {
                    API->DeleteHotkey(App->pushToTalkHotkeyID);
                    App->pushToTalkHotkeyID = 0;
                }

                if(App->bUsingPushToTalk && hotkey)
                    pushToTalkHotkeyID = API->CreateHotkey(hotkey, OBS::PushToTalkHotkey, NULL);
                if(App->bUsingPushToTalk && hotkey2)
                    pushToTalkHotkeyID = API->CreateHotkey(hotkey2, OBS::PushToTalkHotkey, NULL);

                //------------------------------------

                hotkey = (DWORD)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_MUTEMICHOTKEY), HKM_GETHOTKEY, 0, 0);
                AppConfig->SetInt(TEXT("Audio"), TEXT("MuteMicHotkey"), hotkey);

                if(App->muteMicHotkeyID)
                {
                    API->DeleteHotkey(App->muteMicHotkeyID);
                    App->muteDesktopHotkeyID = 0;
                }

                if(hotkey)
                    muteMicHotkeyID = API->CreateHotkey(hotkey, OBS::MuteMicHotkey, NULL);

                //------------------------------------

                hotkey = (DWORD)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_MUTEDESKTOPHOTKEY), HKM_GETHOTKEY, 0, 0);
                AppConfig->SetInt(TEXT("Audio"), TEXT("MuteDesktopHotkey"), hotkey);

                if(App->muteDesktopHotkeyID)
                {
                    API->DeleteHotkey(App->muteDesktopHotkeyID);
                    App->muteDesktopHotkeyID = 0;
                }

                if(hotkey)
                    muteDesktopHotkeyID = API->CreateHotkey(hotkey, OBS::MuteDesktopHotkey, NULL);

                //------------------------------------

                App->bForceMicMono = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_FORCEMONO), BM_GETCHECK, 0, 0) == BST_CHECKED;
                AppConfig->SetInt(TEXT("Audio"), TEXT("ForceMicMono"), bForceMicMono);

                //------------------------------------

                DWORD desktopBoostMultiple = (DWORD)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_DESKTOPBOOST), UDM_GETPOS32, 0, 0);
                if(desktopBoostMultiple < 1)
                    desktopBoostMultiple = 1;
                else if(desktopBoostMultiple > 20)
                    desktopBoostMultiple = 20;
                GlobalConfig->SetInt(TEXT("Audio"), TEXT("DesktopBoostMultiple"), desktopBoostMultiple);
                App->desktopBoost = float(desktopBoostMultiple);

                //------------------------------------

                DWORD micBoostMultiple = (DWORD)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_MICBOOST), UDM_GETPOS32, 0, 0);
                if(micBoostMultiple < 1)
                    micBoostMultiple = 1;
                else if(micBoostMultiple > 20)
                    micBoostMultiple = 20;
                AppConfig->SetInt(TEXT("Audio"), TEXT("MicBoostMultiple"), micBoostMultiple);
                App->micBoost = float(micBoostMultiple);

                //------------------------------------

                int micTimeOffset = (int)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_MICTIMEOFFSET), UDM_GETPOS32, 0, 0);
                if(micTimeOffset < -150)
                    micTimeOffset = -150;
                else if(micTimeOffset > 3000)
                    micTimeOffset = 3000;
                AppConfig->SetInt(TEXT("Audio"), TEXT("MicTimeOffset"), micTimeOffset);

                if(App->bRunning && App->micAudio)
                    App->micAudio->SetTimeOffset(micTimeOffset);

                break;
            }

        case Settings_Advanced:
            {
                String strTemp = GetCBText(GetDlgItem(hwndCurrentSettings, IDC_PRESET));
                AppConfig->SetString(TEXT("Video Encoding"), TEXT("Preset"), strTemp);

                //--------------------------------------------------

                bool bUseMTOptimizations = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_USEMULTITHREADEDOPTIMIZATIONS), BM_GETCHECK, 0, 0) == BST_CHECKED;
                AppConfig->SetInt(TEXT("General"), TEXT("UseMultithreadedOptimizations"), bUseMTOptimizations);

                strTemp = GetCBText(GetDlgItem(hwndCurrentSettings, IDC_PRIORITY));
                AppConfig->SetString(TEXT("General"), TEXT("Priority"), strTemp);

                //--------------------------------------------------

                UINT sceneBufferTime = (UINT)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_SCENEBUFFERTIME), UDM_GETPOS32, 0, 0);
                GlobalConfig->SetInt(TEXT("General"), TEXT("SceneBufferingTime"), sceneBufferTime);

                //--------------------------------------------------

                bool bUseCFR = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_USECFR), BM_GETCHECK, 0, 0) == BST_CHECKED;
                AppConfig->SetInt   (TEXT("Video Encoding"), TEXT("UseCFR"),            bUseCFR);

                //--------------------------------------------------

                BOOL bUseCustomX264Settings = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_USEVIDEOENCODERSETTINGS), BM_GETCHECK, 0, 0) == BST_CHECKED;
                String strCustomX264Settings = GetEditText(GetDlgItem(hwndCurrentSettings, IDC_VIDEOENCODERSETTINGS));

                AppConfig->SetInt   (TEXT("Video Encoding"), TEXT("UseCustomSettings"), bUseCustomX264Settings);
                AppConfig->SetString(TEXT("Video Encoding"), TEXT("CustomSettings"),    strCustomX264Settings);

                //--------------------------------------------------

                BOOL bUnlockFPS = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_UNLOCKHIGHFPS), BM_GETCHECK, 0, 0) == BST_CHECKED;
                AppConfig->SetInt   (TEXT("Video"), TEXT("UnlockFPS"), bUnlockFPS);

                //------------------------------------

                BOOL bUseHQResampling = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_USEHIGHQUALITYRESAMPLING), BM_GETCHECK, 0, 0) == BST_CHECKED;
                AppConfig->SetInt   (TEXT("Audio"), TEXT("UseHighQualityResampling"), bUseHQResampling);

                //--------------------------------------------------

                BOOL bSyncToVideoTime = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_SYNCTOVIDEOTIME), BM_GETCHECK, 0, 0) == BST_CHECKED;
                AppConfig->SetInt   (TEXT("Audio"), TEXT("SyncToVideoTime"), bSyncToVideoTime);

                //--------------------------------------------------

                BOOL bUseMicQPC = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_USEMICQPC), BM_GETCHECK, 0, 0) == BST_CHECKED;
                GlobalConfig->SetInt(TEXT("Audio"), TEXT("UseMicQPC"), bUseMicQPC);

                //--------------------------------------------------

                int globalAudioTimeAdjust = (int)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_AUDIOTIMEADJUST), UDM_GETPOS32, 0, 0);
                GlobalConfig->SetInt(TEXT("Audio"), TEXT("GlobalAudioTimeAdjust"), globalAudioTimeAdjust);

                //--------------------------------------------------

                BOOL bLowLatencyAutoMode = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_LATENCYMETHOD), BM_GETCHECK, 0, 0) == BST_CHECKED;
                int latencyFactor = GetDlgItemInt(hwndCurrentSettings, IDC_LATENCYTUNE, NULL, TRUE);

                AppConfig->SetInt   (TEXT("Publish"),        TEXT("LatencyFactor"),     latencyFactor);
                AppConfig->SetInt   (TEXT("Publish"),        TEXT("LowLatencyMethod"),  bLowLatencyAutoMode);

                //--------------------------------------------------

                strTemp = GetCBText(GetDlgItem(hwndCurrentSettings, IDC_BINDIP));
                AppConfig->SetString(TEXT("Publish"), TEXT("BindToIP"), strTemp);

                break;
            }
    }

    bSettingsChanged = false;
    EnableWindow(GetDlgItem(hwndSettings, IDC_APPLY), FALSE);
}


INT_PTR CALLBACK OBS::SettingsDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                App->hwndSettings = hwnd;

                LocalizeWindow(hwnd);

                // Add setting categories from the pane list
                for(unsigned int i = 0; i < App->settingsPanes.Num(); i++)
                {
                    SettingsPane *pane = App->settingsPanes[i];
                    if(pane == NULL)
                        continue;
                    SendMessage(GetDlgItem(hwnd, IDC_SETTINGSLIST), LB_ADDSTRING, 0, (LPARAM)pane->GetCategory());
                }

                // FIXME: These are temporary until all the built-in panes are ported to the new interface
                SendMessage(GetDlgItem(hwnd, IDC_SETTINGSLIST), LB_ADDSTRING, 0, (LPARAM)Str("Settings.Video"));
                SendMessage(GetDlgItem(hwnd, IDC_SETTINGSLIST), LB_ADDSTRING, 0, (LPARAM)Str("Settings.Audio"));
                SendMessage(GetDlgItem(hwnd, IDC_SETTINGSLIST), LB_ADDSTRING, 0, (LPARAM)Str("Settings.Advanced"));

                RECT subDialogRect;
                GetWindowRect(GetDlgItem(hwnd, IDC_SUBDIALOG), &subDialogRect);
                ScreenToClient(hwnd, (LPPOINT)&subDialogRect.left);

                SendMessage(GetDlgItem(hwnd, IDC_SETTINGSLIST), LB_SETCURSEL, 0, 0);

                // Load the first settings pane from the list
                App->curSettingsSelection = 0;
                App->hwndCurrentSettings = NULL;
                App->currentSettingsPane = NULL;
                if(App->settingsPanes.Num() >= 1)
                    App->currentSettingsPane = App->settingsPanes[0];
                if(App->currentSettingsPane != NULL)
                    App->hwndCurrentSettings = App->currentSettingsPane->CreatePane(hinstMain, hwnd);
                if(App->hwndCurrentSettings != NULL)
                {
                    SetWindowPos(App->hwndCurrentSettings, NULL, subDialogRect.left, subDialogRect.top, 0, 0, SWP_NOSIZE);
                    ShowWindow(App->hwndCurrentSettings, SW_SHOW);
                }

                return TRUE;
            }

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_SETTINGSLIST:
                    {
                        if(HIWORD(wParam) != LBN_SELCHANGE)
                            break;

                        int sel = (int)SendMessage((HWND)lParam, LB_GETCURSEL, 0, 0);
                        if(sel == App->curSettingsSelection)
                            break;
                        else if(App->bSettingsChanged)
                        {
                            int id = MessageBox(hwnd, Str("Settings.SaveChangesPrompt"), Str("Settings.SaveChangesTitle"), MB_YESNOCANCEL);

                            if(id == IDCANCEL)
                            {
                                App->CancelSettings();

                                SendMessage((HWND)lParam, LB_SETCURSEL, App->curSettingsSelection, 0);
                                break;
                            }
                            else if(id == IDYES)
                                App->ApplySettings();
                        }

                        App->curSettingsSelection = sel;

                        if(App->currentSettingsPane != NULL)
                            App->currentSettingsPane->DestroyPane();
                        else
                            DestroyWindow(App->hwndCurrentSettings); // FIXME: This is temporary until all the built-in panes are ported to the new interface
                        App->currentSettingsPane = NULL;
                        App->hwndCurrentSettings = NULL;

                        RECT subDialogRect;
                        GetWindowRect(GetDlgItem(hwnd, IDC_SUBDIALOG), &subDialogRect);
                        ScreenToClient(hwnd, (LPPOINT)&subDialogRect.left);

                        if(sel >= 0 && sel < (int)App->settingsPanes.Num())
                            App->currentSettingsPane = App->settingsPanes[sel];
                        if(App->currentSettingsPane != NULL)
                            App->hwndCurrentSettings = App->currentSettingsPane->CreatePane(hinstMain, hwnd);
                        else
                        {
                            // FIXME: This is all temporary until all the built-in panes are ported to the new interface
                            switch(sel)
                            {
                                case Settings_Video:
                                    App->hwndCurrentSettings = CreateDialog(hinstMain, MAKEINTRESOURCE(IDD_SETTINGS_VIDEO), hwnd, (DLGPROC)OBS::VideoSettingsProc);
                                    break;
                                case Settings_Audio:
                                    App->hwndCurrentSettings = CreateDialog(hinstMain, MAKEINTRESOURCE(IDD_SETTINGS_AUDIO), hwnd, (DLGPROC)OBS::AudioSettingsProc);
                                    break;
                                case Settings_Advanced:
                                    App->hwndCurrentSettings = CreateDialog(hinstMain, MAKEINTRESOURCE(IDD_SETTINGS_ADVANCED), hwnd, (DLGPROC)OBS::AdvancedSettingsProc);
                                    break;
                            }
                        }

                        if(App->hwndCurrentSettings)
                        {
                            SetWindowPos(App->hwndCurrentSettings, NULL, subDialogRect.left, subDialogRect.top, 0, 0, SWP_NOSIZE);
                            ShowWindow(App->hwndCurrentSettings, SW_SHOW);
                        }

                        break;
                    }
                case IDOK:
                    if(App->bSettingsChanged)
                        App->ApplySettings();
                    EndDialog(hwnd, IDOK);
                    App->hwndSettings = NULL;
                    break;

                case IDCANCEL:
                    if(App->bSettingsChanged)
                        App->CancelSettings();
                    EndDialog(hwnd, IDCANCEL);
                    App->hwndSettings = NULL;
                    break;

                case IDC_APPLY:
                    if(App->bSettingsChanged)
                        App->ApplySettings();
                    break;
            }
            break;
    }

    return FALSE;
}
