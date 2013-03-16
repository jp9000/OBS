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

enum SettingsSelection
{
    Settings_Advanced = 5
};

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
    AddSettingsPane(new SettingsVideo());
    AddSettingsPane(new SettingsAudio());
}

CTSTR preset_names[7] = {TEXT("ultrafast"), TEXT("superfast"), TEXT("veryfast"), TEXT("faster"), TEXT("fast"), TEXT("medium"), TEXT("slow")};

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
