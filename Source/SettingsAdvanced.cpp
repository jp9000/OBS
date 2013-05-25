/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>
 Copyright (C) 2013 Lucas Murray <lmurray@undefinedfire.com>

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

#include "Settings.h"

#include <Winsock2.h>
#include <iphlpapi.h>

//============================================================================
// SettingsAdvanced class

SettingsAdvanced::SettingsAdvanced()
    : SettingsPane()
{
}

SettingsAdvanced::~SettingsAdvanced()
{
}

CTSTR SettingsAdvanced::GetCategory() const
{
    static CTSTR name = Str("Settings.Advanced");
    return name;
}

HWND SettingsAdvanced::CreatePane(HWND parentHwnd)
{
    hwnd = CreateDialogParam(hinstMain, MAKEINTRESOURCE(IDD_SETTINGS_ADVANCED), parentHwnd, (DLGPROC)DialogProc, (LPARAM)this);
    return hwnd;
}

void SettingsAdvanced::DestroyPane()
{
    DestroyWindow(hwnd);
    hwnd = NULL;
}

void SettingsAdvanced::ApplySettings()
{
    String strTemp = GetCBText(GetDlgItem(hwnd, IDC_PRESET));
    AppConfig->SetString(TEXT("Video Encoding"), TEXT("Preset"), strTemp);

    //--------------------------------------------------

    bool bUseMTOptimizations = SendMessage(GetDlgItem(hwnd, IDC_USEMULTITHREADEDOPTIMIZATIONS), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt(TEXT("General"), TEXT("UseMultithreadedOptimizations"), bUseMTOptimizations);

    int priority = (int)SendMessage(GetDlgItem(hwnd, IDC_PRIORITY), CB_GETCURSEL, 0, 0);
    switch (priority) {
    case 0: strTemp = TEXT("High"); break;
    case 1: strTemp = TEXT("Above Normal"); break;
    case 2: strTemp = TEXT("Normal"); break;
    case 3: strTemp = TEXT("Idle"); break;
    }

    AppConfig->SetString(TEXT("General"), TEXT("Priority"), strTemp);

    //--------------------------------------------------

    UINT sceneBufferTime = (UINT)SendMessage(GetDlgItem(hwnd, IDC_SCENEBUFFERTIME), UDM_GETPOS32, 0, 0);
    GlobalConfig->SetInt(TEXT("General"), TEXT("SceneBufferingTime"), sceneBufferTime);

    //--------------------------------------------------

    bool bDisablePreviewEncoding = SendMessage(GetDlgItem(hwnd, IDC_DISABLEPREVIEWENCODING), BM_GETCHECK, 0, 0) == BST_CHECKED;
    GlobalConfig->SetInt(TEXT("General"), TEXT("DisablePreviewEncoding"), bDisablePreviewEncoding);

    //--------------------------------------------------

    bool bUseCFR = SendMessage(GetDlgItem(hwnd, IDC_USECFR), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt   (TEXT("Video Encoding"), TEXT("UseCFR"),            bUseCFR);

    //--------------------------------------------------

    BOOL bUseCustomX264Settings = SendMessage(GetDlgItem(hwnd, IDC_USEVIDEOENCODERSETTINGS), BM_GETCHECK, 0, 0) == BST_CHECKED;
    String strCustomX264Settings = GetEditText(GetDlgItem(hwnd, IDC_VIDEOENCODERSETTINGS));

    AppConfig->SetInt   (TEXT("Video Encoding"), TEXT("UseCustomSettings"), bUseCustomX264Settings);
    AppConfig->SetString(TEXT("Video Encoding"), TEXT("CustomSettings"),    strCustomX264Settings);

    //--------------------------------------------------

    BOOL bUnlockFPS = SendMessage(GetDlgItem(hwnd, IDC_UNLOCKHIGHFPS), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt   (TEXT("Video"), TEXT("UnlockFPS"), bUnlockFPS);

    //------------------------------------

    BOOL bUseQSV = SendMessage(GetDlgItem(hwnd, IDC_USEQSV), BM_GETCHECK, 0, 0) == BST_CHECKED;
    GlobalConfig->SetInt(TEXT("Video Encoding"), TEXT("UseQSV"), bUseQSV);

    //------------------------------------

    BOOL bSyncToVideoTime = SendMessage(GetDlgItem(hwnd, IDC_SYNCTOVIDEOTIME), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt   (TEXT("Audio"), TEXT("SyncToVideoTime"), bSyncToVideoTime);

    //--------------------------------------------------

    BOOL bUseMicQPC = SendMessage(GetDlgItem(hwnd, IDC_USEMICQPC), BM_GETCHECK, 0, 0) == BST_CHECKED;
    GlobalConfig->SetInt(TEXT("Audio"), TEXT("UseMicQPC"), bUseMicQPC);

    //--------------------------------------------------

    int globalAudioTimeAdjust = (int)SendMessage(GetDlgItem(hwnd, IDC_AUDIOTIMEADJUST), UDM_GETPOS32, 0, 0);
    GlobalConfig->SetInt(TEXT("Audio"), TEXT("GlobalAudioTimeAdjust"), globalAudioTimeAdjust);

    //--------------------------------------------------

    BOOL bLowLatencyAutoMode = SendMessage(GetDlgItem(hwnd, IDC_LATENCYMETHOD), BM_GETCHECK, 0, 0) == BST_CHECKED;
    int latencyFactor = GetDlgItemInt(hwnd, IDC_LATENCYTUNE, NULL, TRUE);

    AppConfig->SetInt   (TEXT("Publish"),        TEXT("LatencyFactor"),     latencyFactor);
    AppConfig->SetInt   (TEXT("Publish"),        TEXT("LowLatencyMethod"),  bLowLatencyAutoMode);

    //--------------------------------------------------

    strTemp = GetCBText(GetDlgItem(hwnd, IDC_BINDIP));
    AppConfig->SetString(TEXT("Publish"), TEXT("BindToIP"), strTemp);
}

void SettingsAdvanced::CancelSettings()
{
}

bool SettingsAdvanced::HasDefaults() const
{
    return true;
}

void SettingsAdvanced::SetDefaults()
{
    SendMessage(GetDlgItem(hwnd, IDC_SCENEBUFFERTIME), UDM_SETPOS32, 0, 400);
    SendMessage(GetDlgItem(hwnd, IDC_USEMULTITHREADEDOPTIMIZATIONS), BM_SETCHECK, BST_CHECKED, 0);
    SendMessage(GetDlgItem(hwnd, IDC_PRIORITY), CB_SETCURSEL, 2, 0);
    SendMessage(GetDlgItem(hwnd, IDC_PRESET), CB_SETCURSEL, 2, 0);
    SendMessage(GetDlgItem(hwnd, IDC_USECFR), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwnd, IDC_USEVIDEOENCODERSETTINGS), BM_SETCHECK, BST_UNCHECKED, 0);
    EnableWindow(GetDlgItem(hwnd, IDC_VIDEOENCODERSETTINGS), FALSE);
    SendMessage(GetDlgItem(hwnd, IDC_UNLOCKHIGHFPS), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwnd, IDC_USEQSV), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwnd, IDC_SYNCTOVIDEOTIME), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwnd, IDC_USEMICQPC), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwnd, IDC_AUDIOTIMEADJUST), UDM_SETPOS32, 0, 0);
    SendMessage(GetDlgItem(hwnd, IDC_LATENCYTUNE), UDM_SETPOS32, 0, 20);
    SendMessage(GetDlgItem(hwnd, IDC_LATENCYMETHOD), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwnd, IDC_BINDIP), CB_SETCURSEL, 0, 0);
    SendMessage(GetDlgItem(hwnd, IDC_DISABLEPREVIEWENCODING), BM_SETCHECK, BST_UNCHECKED, 0);

    ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
    SetChangedSettings(true);
}

INT_PTR SettingsAdvanced::ProcMessage(UINT message, WPARAM wParam, LPARAM lParam)
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
                SendMessage(GetDlgItem(hwnd, IDC_SCENEBUFFERTIME), UDM_SETRANGE32, 60, 3000);
                SendMessage(GetDlgItem(hwnd, IDC_SCENEBUFFERTIME), UDM_SETPOS32, 0, sceneBufferingTime);

                //------------------------------------

                bool bUseMTOptimizations = AppConfig->GetInt(TEXT("General"), TEXT("UseMultithreadedOptimizations"), TRUE) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_USEMULTITHREADEDOPTIMIZATIONS), BM_SETCHECK, bUseMTOptimizations ? BST_CHECKED : BST_UNCHECKED, 0);

                HWND hwndTemp = GetDlgItem(hwnd, IDC_PRIORITY);
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)Str("Settings.Advanced.Priority.High"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)Str("Settings.Advanced.Priority.AboveNormal"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)Str("Settings.Advanced.Priority.Normal"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)Str("Settings.Advanced.Priority.Idle"));

                CTSTR pStr = AppConfig->GetStringPtr(TEXT("General"), TEXT("Priority"), TEXT("Normal"));
                if (scmpi(pStr, TEXT("Idle")) == 0)
                    SendMessage(hwndTemp, CB_SETCURSEL, 3, 0);
                else if (scmpi(pStr, TEXT("Above Normal")) == 0)
                    SendMessage(hwndTemp, CB_SETCURSEL, 1, 0);
                else if (scmpi(pStr, TEXT("High")) == 0)
                    SendMessage(hwndTemp, CB_SETCURSEL, 0, 0);
                else //Normal
                    SendMessage(hwndTemp, CB_SETCURSEL, 2, 0);

                //------------------------------------

                bool bDisablePreviewEncoding = GlobalConfig->GetInt(TEXT("General"), TEXT("DisablePreviewEncoding"), false) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_DISABLEPREVIEWENCODING), BM_SETCHECK, bDisablePreviewEncoding ? BST_CHECKED : BST_UNCHECKED, 0);

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_PRESET);
                static const CTSTR preset_names[7] = {TEXT("ultrafast"), TEXT("superfast"), TEXT("veryfast"), TEXT("faster"), TEXT("fast"), TEXT("medium"), TEXT("slow")};
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

                bool bUseQSV = GlobalConfig->GetInt(TEXT("Video Encoding"), TEXT("UseQSV")) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_USEQSV), BM_SETCHECK, bUseQSV ? BST_CHECKED : BST_UNCHECKED, 0);

                //------------------------------------

                bool bSyncToVideoTime = AppConfig->GetInt(TEXT("Audio"), TEXT("SyncToVideoTime")) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_SYNCTOVIDEOTIME), BM_SETCHECK, bSyncToVideoTime ? BST_CHECKED : BST_UNCHECKED, 0);

                //------------------------------------

                bool bUseMicQPC = GlobalConfig->GetInt(TEXT("Audio"), TEXT("UseMicQPC")) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_USEMICQPC), BM_SETCHECK, bUseMicQPC ? BST_CHECKED : BST_UNCHECKED, 0);

                //------------------------------------

                int bufferTime = GlobalConfig->GetInt(TEXT("General"), TEXT("SceneBufferingTime"), 400);

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
                SetChangedSettings(false);
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
                        SetChangedSettings(true);
                    }
                    break;

                case IDC_SCENEBUFFERTIME_EDIT:
                case IDC_AUDIOTIMEADJUST_EDIT:
                case IDC_VIDEOENCODERSETTINGS:
                case IDC_LATENCYTUNE:
                    if(HIWORD(wParam) == EN_CHANGE)
                    {
                        ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                        SetChangedSettings(true);
                    }
                    break;

                /*case IDC_TIMER1:
                case IDC_TIMER2:
                case IDC_TIMER3:
                case IDC_DISABLED3DCOMPATIBILITY:
                    if(HIWORD(wParam) == BN_CLICKED)
                    {
                        ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                        SetChangedSettings(true);
                    }
                    break;*/

                case IDC_USESENDBUFFER:
                    if(HIWORD(wParam) == BN_CLICKED)
                    {
                        BOOL bUseSendBuffer = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;
                        EnableWindow(GetDlgItem(hwnd, IDC_SENDBUFFERSIZE), bUseSendBuffer);

                        ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                        SetChangedSettings(true);
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
                                SetChangedSettings(true);
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
                        SetChangedSettings(true);
                    }
                    break;

                case IDC_DISABLEPREVIEWENCODING:
                case IDC_USEMICQPC:
                case IDC_SYNCTOVIDEOTIME:
                case IDC_USECFR:
                case IDC_USEMULTITHREADEDOPTIMIZATIONS:
                case IDC_UNLOCKHIGHFPS:
                case IDC_LATENCYMETHOD:
                case IDC_USEQSV:
                    if(HIWORD(wParam) == BN_CLICKED)
                    {
                        ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                        SetChangedSettings(true);
                    }
                    break;
            }

    }
    return FALSE;
}
