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
    hwnd = OBSCreateDialog(hinstMain, MAKEINTRESOURCE(IDD_SETTINGS_ADVANCED), parentHwnd, (DLGPROC)DialogProc, (LPARAM)this);
    return hwnd;
}

void SettingsAdvanced::DestroyPane()
{
    DestroyWindow(hwnd);
    hwnd = NULL;
}

void SettingsAdvanced::SelectPresetDialog(bool useQSV, bool useNVENC)
{
    bool usex264 = !useQSV && !useNVENC;

    HWND hwndTemp = GetDlgItem(hwnd, IDC_PRESET);
    ShowWindow(hwndTemp, usex264 ? SW_SHOW : SW_HIDE);
    EnableWindow(hwndTemp, true);

    hwndTemp = GetDlgItem(hwnd, IDC_X264PRESET_LABEL);
    ShowWindow(hwndTemp, usex264 ? SW_SHOW : SW_HIDE);
    EnableWindow(hwndTemp, true);

    hwndTemp = GetDlgItem(hwnd, IDC_USEVIDEOENCODERSETTINGS);
    ShowWindow(hwndTemp, !useQSV ? SW_SHOW : SW_HIDE);
    EnableWindow(hwndTemp, usex264);

    hwndTemp = GetDlgItem(hwnd, IDC_VIDEOENCODERSETTINGS);
    ShowWindow(hwndTemp, !useQSV ? SW_SHOW : SW_HIDE);

    hwndTemp = GetDlgItem(hwnd, IDC_NVENCPRESET);
    ShowWindow(hwndTemp, useNVENC ? SW_SHOW : SW_HIDE);
    EnableWindow(hwndTemp, true);

    hwndTemp = GetDlgItem(hwnd, IDC_NVENCPRESET_LABEL);
    ShowWindow(hwndTemp, useNVENC ? SW_SHOW : SW_HIDE);
    EnableWindow(hwndTemp, true);

    hwndTemp = GetDlgItem(hwnd, IDC_QSVPRESET);
    ShowWindow(hwndTemp, useQSV ? SW_SHOW : SW_HIDE);
    EnableWindow(hwndTemp, true);

    hwndTemp = GetDlgItem(hwnd, IDC_QSVPRESET_LABEL);
    ShowWindow(hwndTemp, useQSV ? SW_SHOW : SW_HIDE);
    EnableWindow(hwndTemp, true);

    hwndTemp = GetDlgItem(hwnd, IDC_QSVUSEVIDEOENCODERSETTINGS);
    ShowWindow(hwndTemp, useQSV ? SW_SHOW : SW_HIDE);
    EnableWindow(hwndTemp, true);

    hwndTemp = GetDlgItem(hwnd, IDC_QSVVIDEOENCODERSETTINGS);
    ShowWindow(hwndTemp, useQSV ? SW_SHOW : SW_HIDE);
}

void SettingsAdvanced::ApplySettings()
{
    //--------------------------------------------------

    String strTemp = GetCBText(GetDlgItem(hwnd, IDC_PRESET));
    AppConfig->SetString(TEXT("Video Encoding"), TEXT("Preset"), strTemp);

    //------------------------------------

    strTemp = GetCBText(GetDlgItem(hwnd, IDC_X264PROFILE));
    AppConfig->SetString(TEXT("Video Encoding"), TEXT("X264Profile"), strTemp);

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

    bool bAllowOtherHotkeyModifiers = SendMessage(GetDlgItem(hwnd, IDC_ALLOWOTHERHOTKEYMODIFIERS), BM_GETCHECK, 0, 0) == BST_CHECKED;
    GlobalConfig->SetInt(TEXT("General"), TEXT("AllowOtherHotkeyModifiers"), bAllowOtherHotkeyModifiers);
    
    //--------------------------------------------------

    UINT keyframeInt = (UINT)SendMessage(GetDlgItem(hwnd, IDC_KEYFRAMEINTERVAL), UDM_GETPOS32, 0, 0);
    AppConfig->SetInt(TEXT("Video Encoding"), TEXT("KeyframeInterval"), keyframeInt);

    //--------------------------------------------------

    bool bUseCFR = SendMessage(GetDlgItem(hwnd, IDC_USECFR), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt   (TEXT("Video Encoding"), TEXT("UseCFR"),            bUseCFR);

    //--------------------------------------------------

    BOOL bUseCustomX264Settings = SendMessage(GetDlgItem(hwnd, IDC_USEVIDEOENCODERSETTINGS), BM_GETCHECK, 0, 0) == BST_CHECKED;
    String strCustomX264Settings = GetEditText(GetDlgItem(hwnd, IDC_VIDEOENCODERSETTINGS));

    AppConfig->SetInt   (TEXT("Video Encoding"), TEXT("UseCustomSettings"), bUseCustomX264Settings);
    AppConfig->SetString(TEXT("Video Encoding"), TEXT("CustomSettings"), strCustomX264Settings);

    //--------------------------------------------------

    AppConfig->SetString(L"Video Encoding", L"CustomQSVSettings", GetEditText(GetDlgItem(hwnd, IDC_QSVVIDEOENCODERSETTINGS)));

    //--------------------------------------------------

    BOOL bUnlockFPS = SendMessage(GetDlgItem(hwnd, IDC_UNLOCKHIGHFPS), BM_GETCHECK, 0, 0) == BST_CHECKED;
    BOOL bFullRange = SendMessage(GetDlgItem(hwnd, IDC_ENCODEFULLRANGE), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt(TEXT("Video"), TEXT("UnlockFPS"), bUnlockFPS);
    AppConfig->SetInt(TEXT("Video"), TEXT("FullRange"), bFullRange);

    //------------------------------------

    BOOL bQSVUseVideoEncoderSettings = SendMessage(GetDlgItem(hwnd, IDC_QSVUSEVIDEOENCODERSETTINGS), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt(TEXT("Video Encoding"), TEXT("QSVUseVideoEncoderSettings"), bQSVUseVideoEncoderSettings);

    //------------------------------------

    int qsvPreset = (int)SendMessage(GetDlgItem(hwnd, IDC_QSVPRESET), CB_GETCURSEL, 0, 0);
    if (qsvPreset != CB_ERR)
    {
        qsvPreset = (int)SendMessage(GetDlgItem(hwnd, IDC_QSVPRESET), CB_GETITEMDATA, qsvPreset, 0);
        AppConfig->SetInt(TEXT("Video Encoding"), TEXT("QSVPreset"), qsvPreset);
    }

    //------------------------------------

    strTemp = GetCBText(GetDlgItem(hwnd, IDC_NVENCPRESET));
    AppConfig->SetString(TEXT("Video Encoding"), TEXT("NVENCPreset"), strTemp);

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

    BOOL bUseMicSyncFixHack = SendMessage(GetDlgItem(hwnd, IDC_MICSYNCFIX), BM_GETCHECK, 0, 0) == BST_CHECKED;
    GlobalConfig->SetInt(TEXT("Audio"), TEXT("UseMicSyncFixHack"), bUseMicSyncFixHack);

    //--------------------------------------------------

    BOOL bLowLatencyAutoMode = SendMessage(GetDlgItem(hwnd, IDC_LATENCYMETHOD), BM_GETCHECK, 0, 0) == BST_CHECKED;
    int latencyFactor = GetDlgItemInt(hwnd, IDC_LATENCYTUNE, NULL, TRUE);

    AppConfig->SetInt   (TEXT("Publish"),        TEXT("LatencyFactor"),     latencyFactor);
    AppConfig->SetInt   (TEXT("Publish"),        TEXT("LowLatencyMethod"),  bLowLatencyAutoMode);

    BOOL bDisableTCPOptimizations = SendDlgItemMessage(hwnd, IDC_DISABLETCPOPTIMIZATIONS, BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt(TEXT("Publish"), TEXT("DisableSendWindowOptimization"), bDisableTCPOptimizations);

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

    String vencoder = AppConfig->GetString(L"Video Encoding", L"Encoder");
    bool useQSV = !!(vencoder == L"QSV");
    bool useNVENC = !!(vencoder == L"NVENC");
    SelectPresetDialog(useQSV, useNVENC);

    SendMessage(GetDlgItem(hwnd, IDC_SCENEBUFFERTIME), UDM_SETPOS32, 0, 700);
    SendMessage(GetDlgItem(hwnd, IDC_USEMULTITHREADEDOPTIMIZATIONS), BM_SETCHECK, BST_CHECKED, 0);
    SendMessage(GetDlgItem(hwnd, IDC_PRIORITY), CB_SETCURSEL, 2, 0);
    SendMessage(GetDlgItem(hwnd, IDC_PRESET), CB_SETCURSEL, 2, 0);
    SendMessage(GetDlgItem(hwnd, IDC_X264PROFILE), CB_SETCURSEL, 1, 0);
    SendMessage(GetDlgItem(hwnd, IDC_KEYFRAMEINTERVAL), UDM_SETPOS32, 0, 0);
    SendMessage(GetDlgItem(hwnd, IDC_USECFR), BM_SETCHECK, BST_CHECKED, 0);
    SendMessage(GetDlgItem(hwnd, IDC_USEVIDEOENCODERSETTINGS), BM_SETCHECK, BST_UNCHECKED, 0);
    EnableWindow(GetDlgItem(hwnd, IDC_VIDEOENCODERSETTINGS), FALSE);
    SendMessage(GetDlgItem(hwnd, IDC_UNLOCKHIGHFPS), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwnd, IDC_ENCODEFULLRANGE), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwnd, IDC_QSVUSEVIDEOENCODERSETTINGS), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwnd, IDC_NVENCPRESET), CB_SETCURSEL, 0, 0);
    SendMessage(GetDlgItem(hwnd, IDC_QSVPRESET), CB_SETCURSEL, 0, 0);
    EnableWindow(GetDlgItem(hwnd, IDC_QSVVIDEOENCODERSETTINGS), FALSE);
    SendMessage(GetDlgItem(hwnd, IDC_SYNCTOVIDEOTIME), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwnd, IDC_USEMICQPC), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwnd, IDC_MICSYNCFIX), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwnd, IDC_AUDIOTIMEADJUST), UDM_SETPOS32, 0, 0);
    SendMessage(GetDlgItem(hwnd, IDC_LATENCYTUNE), UDM_SETPOS32, 0, 20);
    SendMessage(GetDlgItem(hwnd, IDC_LATENCYMETHOD), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwnd, IDC_BINDIP), CB_SETCURSEL, 0, 0);
    SendMessage(GetDlgItem(hwnd, IDC_DISABLEPREVIEWENCODING), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwnd, IDC_ALLOWOTHERHOTKEYMODIFIERS), BM_SETCHECK, BST_CHECKED, 0);
    SendMessage(GetDlgItem(hwnd, IDC_DISABLETCPOPTIMIZATIONS), BM_SETCHECK, BST_UNCHECKED, 0);

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

                bool invalidSettings = false;

                //--------------------------------------------

                HWND hwndToolTip = CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL, WS_POPUP|TTS_NOPREFIX|TTS_ALWAYSTIP,
                    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                    hwnd, NULL, hinstMain, NULL);

                TOOLINFO ti;
                zero(&ti, sizeof(ti));
                ti.cbSize = sizeof(ti);
                ti.uFlags = TTF_SUBCLASS|TTF_IDISHWND;
                ti.hwnd = hwnd;

                if (LocaleIsRTL())
                    ti.uFlags |= TTF_RTLREADING;

                SendMessage(hwndToolTip, TTM_SETMAXTIPWIDTH, 0, 500);
                SendMessage(hwndToolTip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 14000);

                //------------------------------------

                UINT sceneBufferingTime = GlobalConfig->GetInt(TEXT("General"), TEXT("SceneBufferingTime"), 700);
                SendMessage(GetDlgItem(hwnd, IDC_SCENEBUFFERTIME), UDM_SETRANGE32, 60, 20000);
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

                //------------------------------------

                bool bAllowOtherHotkeyModifiers = GlobalConfig->GetInt(TEXT("General"), TEXT("AllowOtherHotkeyModifiers"), true) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_ALLOWOTHERHOTKEYMODIFIERS), BM_SETCHECK, bAllowOtherHotkeyModifiers ? BST_CHECKED : BST_UNCHECKED, 0);

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_X264PROFILE);
                static const CTSTR profile_names[3] = {TEXT("main"), TEXT("high")};
                for(int i=0; i<2; i++)
                    SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)profile_names[i]);

                LoadSettingComboString(hwndTemp, TEXT("Video Encoding"), TEXT("X264Profile"), TEXT("high"));

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_PRESET);
                static const CTSTR preset_names[8] = {TEXT("ultrafast"), TEXT("superfast"), TEXT("veryfast"), TEXT("faster"), TEXT("fast"), TEXT("medium"), TEXT("slow"), TEXT("slower")};
                for(int i=0; i<8; i++)
                    SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)preset_names[i]);

                LoadSettingComboString(hwndTemp, TEXT("Video Encoding"), TEXT("Preset"), TEXT("veryfast"));

                ti.lpszText = (LPWSTR)Str("Settings.Advanced.VideoEncoderCPUTradeoffTooltip");
                ti.uId = (UINT_PTR)hwndTemp;
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                //------------------------------------

                bool bUseCFR = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseCFR"), 1) != 0;
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

                UINT keyframeInt = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("KeyframeInterval"), 0);
                SendMessage(GetDlgItem(hwnd, IDC_KEYFRAMEINTERVAL), UDM_SETRANGE32, 0, 20);
                SendMessage(GetDlgItem(hwnd, IDC_KEYFRAMEINTERVAL), UDM_SETPOS32, 0, keyframeInt);

                //--------------------------------------------

                bool bUnlockFPS = AppConfig->GetInt(TEXT("Video"), TEXT("UnlockFPS")) != 0;
                bool bFullRange = AppConfig->GetInt(TEXT("Video"), TEXT("FullRange")) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_UNLOCKHIGHFPS), BM_SETCHECK, bUnlockFPS ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hwnd, IDC_ENCODEFULLRANGE), BM_SETCHECK, bFullRange ? BST_CHECKED : BST_UNCHECKED, 0);

                //------------------------------------

                String vencoder = AppConfig->GetString(L"Video Encoding", L"Encoder");

                bool bUseQSV = !!(vencoder == L"QSV");

                bool bUseNVENC = !!(vencoder == L"NVENC");

                SelectPresetDialog(bUseQSV, bUseNVENC);

                bool bQSVUseVideoEncoderSettings = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("QSVUseVideoEncoderSettings")) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_QSVUSEVIDEOENCODERSETTINGS), BM_SETCHECK, bQSVUseVideoEncoderSettings ? BST_CHECKED : BST_UNCHECKED, 0);

                String qsvSettings = AppConfig->GetString(TEXT("Video Encoding"), TEXT("CustomQSVSettings"));
                SetWindowText(GetDlgItem(hwnd, IDC_QSVVIDEOENCODERSETTINGS), qsvSettings);
                EnableWindow(GetDlgItem(hwnd, IDC_QSVVIDEOENCODERSETTINGS), bQSVUseVideoEncoderSettings);
                
                ti.lpszText = (LPWSTR)Str("Settings.Advanced.QSVUseVideoEncoderSettingsTooltip");
                ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_QSVUSEVIDEOENCODERSETTINGS);
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
                ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_QSVVIDEOENCODERSETTINGS);
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
                
                hwndTemp = GetDlgItem(hwnd, IDC_NVENCPRESET);
                static const CTSTR nv_preset_names[16] = {
                    TEXT("Automatic"),
                    TEXT("Streaming"),
                    TEXT("Streaming (2pass)"),
                    TEXT("High Quality"),
                    TEXT("High Performance"),
                    TEXT("Bluray Disk"),
                    TEXT("Low Latency"),
                    TEXT("High Performance Low Latency"),
                    TEXT("High Quality Low Latency"),
                    TEXT("Low Latency (2pass)"),
                    TEXT("High Performance Low Latency (2pass)"),
                    TEXT("High Quality Low Latency (2pass)"),
                    TEXT("Lossless"),
                    TEXT("High Performance Lossless"),
                    TEXT("NVDefault"),
                    NULL
                };
                for (int i = 0; nv_preset_names[i]; i++)
                    SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)nv_preset_names[i]);

                LoadSettingComboString(hwndTemp, TEXT("Video Encoding"), TEXT("NVENCPreset"), nv_preset_names[0]);

                hwndTemp = GetDlgItem(hwnd, IDC_QSVPRESET);
                static const struct {
                    CTSTR name;
                    int id;
                } qsv_presets[] = {
                    { L"1 (Best Quality)", 1 },
                    { L"2", 2 },
                    { L"3", 3 },
                    { L"4 (Balanced)", 4 },
                    { L"5", 5 },
                    { L"6", 6 },
                    { L"7 (Best Speed)", 7 }
                };
                for (int i = 0; i < _countof(qsv_presets); i++)
                {
                    SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)qsv_presets[i].name);
                    SendMessage(hwndTemp, CB_SETITEMDATA, i, qsv_presets[i].id);
                }
                int qsvPreset = AppConfig->GetInt(L"Video Encoding", L"QSVPreset", 1);
                if (qsvPreset < 1 || qsvPreset > 7) qsvPreset = 1;
                SendMessage(hwndTemp, CB_SETCURSEL, qsvPreset - 1, 0);

                //------------------------------------

                bool bSyncToVideoTime = AppConfig->GetInt(TEXT("Audio"), TEXT("SyncToVideoTime")) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_SYNCTOVIDEOTIME), BM_SETCHECK, bSyncToVideoTime ? BST_CHECKED : BST_UNCHECKED, 0);

                //------------------------------------

                bool bUseMicQPC = GlobalConfig->GetInt(TEXT("Audio"), TEXT("UseMicQPC")) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_USEMICQPC), BM_SETCHECK, bUseMicQPC ? BST_CHECKED : BST_UNCHECKED, 0);

                //------------------------------------

                BOOL bMicSyncFixHack = GlobalConfig->GetInt(TEXT("Audio"), TEXT("UseMicSyncFixHack"));
                SendMessage(GetDlgItem(hwnd, IDC_MICSYNCFIX), BM_SETCHECK, bMicSyncFixHack ? BST_CHECKED : BST_UNCHECKED, 0);

                //------------------------------------

                int bufferTime = GlobalConfig->GetInt(TEXT("General"), TEXT("SceneBufferingTime"), 700);

                int globalAudioTimeAdjust = GlobalConfig->GetInt(TEXT("Audio"), TEXT("GlobalAudioTimeAdjust"));
                SendMessage(GetDlgItem(hwnd, IDC_AUDIOTIMEADJUST), UDM_SETRANGE32, -bufferTime, 5000);
                SendMessage(GetDlgItem(hwnd, IDC_AUDIOTIMEADJUST), UDM_SETPOS32, 0, globalAudioTimeAdjust);

                //------------------------------------

                int lowLatencyFactor = AppConfig->GetInt(TEXT("Publish"), TEXT("LatencyFactor"), 20);
                SetDlgItemInt(hwnd, IDC_LATENCYTUNE, lowLatencyFactor, TRUE);

                int bLowLatencyAutoMethod = AppConfig->GetInt(TEXT("Publish"), TEXT("LowLatencyMethod"), 0);
                SendMessage(GetDlgItem(hwnd, IDC_LATENCYMETHOD), BM_SETCHECK, bLowLatencyAutoMethod ? BST_CHECKED : BST_UNCHECKED, 0);

                BOOL bDisableTCPOptimizations = AppConfig->GetInt(TEXT("Publish"), TEXT("DisableSendWindowOptimization"), 0);
                SendMessage(GetDlgItem(hwnd, IDC_DISABLETCPOPTIMIZATIONS), BM_SETCHECK, bDisableTCPOptimizations ? BST_CHECKED : BST_UNCHECKED, 0);

                //------------------------------------

                IP_ADAPTER_ADDRESSES *ipTable;

                DWORD dwSize = 65536;
                ipTable = (IP_ADAPTER_ADDRESSES *)Allocate(dwSize);

                DWORD flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;

                hwndTemp = GetDlgItem(hwnd, IDC_BINDIP);
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("Default"));

                if (!GetAdaptersAddresses(AF_UNSPEC, flags, NULL, ipTable, &dwSize))
                {
                    IP_ADAPTER_ADDRESSES *pCurrAddresses = ipTable;

                    while (pCurrAddresses)
                    {
                        if (pCurrAddresses->OperStatus == IfOperStatusUp && pCurrAddresses->IfType != IF_TYPE_SOFTWARE_LOOPBACK)
                        {
                            PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress;
                            while (pUnicast)
                            {
                                if (pUnicast->Address.lpSockaddr->sa_family == AF_INET || pUnicast->Address.lpSockaddr->sa_family == AF_INET6)
                                {
                                    TCHAR friendlyAddress[256];
                                    DWORD friendlyAddressLen = _countof(friendlyAddress);
                                    if (!WSAAddressToString(pUnicast->Address.lpSockaddr, pUnicast->Address.iSockaddrLength, NULL, friendlyAddress, &friendlyAddressLen))
                                    {
                                        SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)friendlyAddress);
                                    }
                                }
                                pUnicast = pUnicast->Next;
                            }
                        }

                        pCurrAddresses = pCurrAddresses->Next;
                    }

                    Free(ipTable);
                }

                LoadSettingComboString(hwndTemp, TEXT("Publish"), TEXT("BindToIP"), TEXT("Default"));

                //need this as some of the dialog item sets above trigger the notifications
                ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_HIDE);
                SetChangedSettings(invalidSettings);
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

                        if (App->GetVideoEncoder())
                            ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                        SetChangedSettings(true);
                    }
                    break;

                case IDC_QSVUSEVIDEOENCODERSETTINGS:
                    if (HIWORD(wParam) == BN_CLICKED)
                    {
                        bool useSettings = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;
                        EnableWindow(GetDlgItem(hwnd, IDC_QSVVIDEOENCODERSETTINGS), useSettings);

                        if (App->GetVideoEncoder())
                            ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                        SetChangedSettings(true);
                    }
                    break;

                case IDC_KEYFRAMEINTERVAL_EDIT:
                case IDC_SCENEBUFFERTIME_EDIT:
                case IDC_AUDIOTIMEADJUST_EDIT:
                case IDC_VIDEOENCODERSETTINGS:
                case IDC_QSVVIDEOENCODERSETTINGS:
                case IDC_LATENCYTUNE:
                    if(HIWORD(wParam) == EN_CHANGE)
                    {
                        if (App->GetVideoEncoder())
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

                        if (App->GetVideoEncoder())
                            ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                        SetChangedSettings(true);
                    }
                    break;

                case IDC_PRESET:
                    if(HIWORD(wParam) == CBN_SELCHANGE)
                    {
                        HWND hwndTemp = (HWND)lParam;

                        String strNewPreset = GetCBText(hwndTemp);
                        if (scmp(strNewPreset.Array(), TEXT("veryfast")))
                        {
                            static BOOL bHasWarned = FALSE;
                            if (!bHasWarned && OBSMessageBox(hwnd, Str("Settings.Advanced.PresetWarning"), NULL, MB_ICONEXCLAMATION | MB_YESNO) == IDNO)
                                LoadSettingComboString(hwndTemp, TEXT("Video Encoding"), TEXT("Preset"), TEXT("veryfast"));
                            else
                                bHasWarned = TRUE;
                        }

                        SetChangedSettings(true);
                        if (App->GetVideoEncoder())
                            ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                    }
                    break;

                case IDC_X264PROFILE:
                case IDC_SENDBUFFERSIZE:
                case IDC_PRIORITY:
                case IDC_BINDIP:
                case IDC_NVENCPRESET:
                case IDC_QSVPRESET:
                    if(HIWORD(wParam) == CBN_SELCHANGE || HIWORD(wParam) == CBN_EDITCHANGE)
                    {
                        if (App->GetVideoEncoder())
                            ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                        SetChangedSettings(true);
                    }
                    break;

                case IDC_DISABLEPREVIEWENCODING:
                case IDC_ALLOWOTHERHOTKEYMODIFIERS:
                case IDC_MICSYNCFIX:
                case IDC_USEMICQPC:
                case IDC_SYNCTOVIDEOTIME:
                case IDC_USECFR:
                case IDC_ENCODEFULLRANGE:
                case IDC_USEMULTITHREADEDOPTIMIZATIONS:
                case IDC_UNLOCKHIGHFPS:
                case IDC_LATENCYMETHOD:
                case IDC_DISABLETCPOPTIMIZATIONS:
                    if(HIWORD(wParam) == BN_CLICKED)
                    {
                        if (App->GetVideoEncoder())
                            ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                        SetChangedSettings(true);
                    }
                    break;
            }

    }
    return FALSE;
}
