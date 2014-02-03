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

//============================================================================
// SettingsAudio class

SettingsAudio::SettingsAudio()
    : SettingsPane()
    , storage(NULL)
{
}

SettingsAudio::~SettingsAudio()
{
    delete storage;
}

CTSTR SettingsAudio::GetCategory() const
{
    static CTSTR name = Str("Settings.Audio");
    return name;
}

HWND SettingsAudio::CreatePane(HWND parentHwnd)
{
    hwnd = CreateDialogParam(hinstMain, MAKEINTRESOURCE(IDD_SETTINGS_AUDIO), parentHwnd, (DLGPROC)DialogProc, (LPARAM)this);
    return hwnd;
}

void SettingsAudio::DestroyPane()
{
    DestroyWindow(hwnd);
    hwnd = NULL;
}

void SettingsAudio::ApplySettings()
{
    UINT iPlaybackDevice = (UINT)SendMessage(GetDlgItem(hwnd, IDC_PLAYBACKDEVICES), CB_GETCURSEL, 0, 0);
    String strPlaybackDevice;

    if(iPlaybackDevice == CB_ERR) {
        strPlaybackDevice = TEXT("Default");
    }
    else {
        strPlaybackDevice = storage->playbackDevices->devices[iPlaybackDevice].strID;
    }

    AppConfig->SetInt(L"Audio", L"UseInputDevices", useInputDevices);

    AppConfig->SetString(TEXT("Audio"), TEXT("PlaybackDevice"), strPlaybackDevice);

    UINT iDevice = (UINT)SendMessage(GetDlgItem(hwnd, IDC_MICDEVICES), CB_GETCURSEL, 0, 0);

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

    App->bUsingPushToTalk = SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALK), BM_GETCHECK, 0, 0) == BST_CHECKED;
    DWORD hotkey = (DWORD)SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY), HKM_GETHOTKEY, 0, 0);
    DWORD hotkey2 = (DWORD)SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY2), HKM_GETHOTKEY, 0, 0);

    App->pushToTalkDelay = (int)SendMessage(GetDlgItem(hwnd, IDC_PTTDELAY), UDM_GETPOS32, 0, 0);
    if(App->pushToTalkDelay < 0)
        App->pushToTalkDelay = 0;
    else if(App->pushToTalkDelay > 2000)
        App->pushToTalkDelay = 2000;

    AppConfig->SetInt(TEXT("Audio"), TEXT("UsePushToTalk"), App->bUsingPushToTalk);
    AppConfig->SetInt(TEXT("Audio"), TEXT("PushToTalkHotkey"), hotkey);
    AppConfig->SetInt(TEXT("Audio"), TEXT("PushToTalkHotkey2"), hotkey2);
    AppConfig->SetInt(TEXT("Audio"), TEXT("PushToTalkDelay"), (int)App->pushToTalkDelay);

    if(App->pushToTalkHotkeyID)
    {
        API->DeleteHotkey(App->pushToTalkHotkeyID);
        App->pushToTalkHotkeyID = 0;
    }

    if(App->bUsingPushToTalk && hotkey)
        App->pushToTalkHotkeyID = API->CreateHotkey(hotkey, OBS::PushToTalkHotkey, NULL);
    if(App->bUsingPushToTalk && hotkey2)
        App->pushToTalkHotkeyID = API->CreateHotkey(hotkey2, OBS::PushToTalkHotkey, NULL);

    //------------------------------------

    hotkey = (DWORD)SendMessage(GetDlgItem(hwnd, IDC_MUTEMICHOTKEY), HKM_GETHOTKEY, 0, 0);
    AppConfig->SetInt(TEXT("Audio"), TEXT("MuteMicHotkey"), hotkey);

    if(App->muteMicHotkeyID)
    {
        API->DeleteHotkey(App->muteMicHotkeyID);
        App->muteDesktopHotkeyID = 0;
    }

    if(hotkey)
        App->muteMicHotkeyID = API->CreateHotkey(hotkey, OBS::MuteMicHotkey, NULL);

    //------------------------------------

    hotkey = (DWORD)SendMessage(GetDlgItem(hwnd, IDC_MUTEDESKTOPHOTKEY), HKM_GETHOTKEY, 0, 0);
    AppConfig->SetInt(TEXT("Audio"), TEXT("MuteDesktopHotkey"), hotkey);

    if(App->muteDesktopHotkeyID)
    {
        API->DeleteHotkey(App->muteDesktopHotkeyID);
        App->muteDesktopHotkeyID = 0;
    }

    if(hotkey)
        App->muteDesktopHotkeyID = API->CreateHotkey(hotkey, OBS::MuteDesktopHotkey, NULL);

    //------------------------------------

    App->bForceMicMono = SendMessage(GetDlgItem(hwnd, IDC_FORCEMONO), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt(TEXT("Audio"), TEXT("ForceMicMono"), App->bForceMicMono);

    //------------------------------------

    DWORD desktopBoostMultiple = (DWORD)SendMessage(GetDlgItem(hwnd, IDC_DESKTOPBOOST), UDM_GETPOS32, 0, 0);
    if(desktopBoostMultiple < 1)
        desktopBoostMultiple = 1;
    else if(desktopBoostMultiple > 20)
        desktopBoostMultiple = 20;
    GlobalConfig->SetInt(TEXT("Audio"), TEXT("DesktopBoostMultiple"), desktopBoostMultiple);
    App->desktopBoost = float(desktopBoostMultiple);

    //------------------------------------

    DWORD micBoostMultiple = (DWORD)SendMessage(GetDlgItem(hwnd, IDC_MICBOOST), UDM_GETPOS32, 0, 0);
    if(micBoostMultiple < 1)
        micBoostMultiple = 1;
    else if(micBoostMultiple > 20)
        micBoostMultiple = 20;
    AppConfig->SetInt(TEXT("Audio"), TEXT("MicBoostMultiple"), micBoostMultiple);
    App->micBoost = float(micBoostMultiple);

    //------------------------------------

    int bufferTime = GlobalConfig->GetInt(TEXT("General"), TEXT("SceneBufferingTime"), 700);

    int micTimeOffset = (int)SendMessage(GetDlgItem(hwnd, IDC_MICTIMEOFFSET), UDM_GETPOS32, 0, 0);
    if(micTimeOffset < -bufferTime)
        micTimeOffset = -bufferTime;
    else if(micTimeOffset > 20000)
        micTimeOffset = 20000;
    AppConfig->SetInt(TEXT("Audio"), TEXT("MicTimeOffset"), micTimeOffset);

    if(App->bRunning && App->micAudio)
        App->micAudio->SetTimeOffset(micTimeOffset);
}

void SettingsAudio::CancelSettings()
{
}

bool SettingsAudio::HasDefaults() const
{
    return false;
}

/*void SettingsAudio::SetDefaults()
{
    SendMessage(GetDlgItem(hwnd, IDC_PLAYBACKDEVICES), CB_SETCURSEL, 0, 0);
    SendMessage(GetDlgItem(hwnd, IDC_MICDEVICES), CB_SETCURSEL, 0, 0);

    SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALK), BM_SETCHECK, BST_UNCHECKED, 0);
    EnableWindow(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY), FALSE);
    EnableWindow(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY2), FALSE);
    EnableWindow(GetDlgItem(hwnd, IDC_CLEARPUSHTOTALK), FALSE);
    EnableWindow(GetDlgItem(hwnd, IDC_PTTDELAY_EDIT), FALSE);
    EnableWindow(GetDlgItem(hwnd, IDC_PTTDELAY), FALSE);

    SendMessage(GetDlgItem(hwnd, IDC_MUTEMICHOTKEY), HKM_SETHOTKEY, 0, 0);
    SendMessage(GetDlgItem(hwnd, IDC_MUTEDESKTOPHOTKEY), HKM_SETHOTKEY, 0, 0);
    SendMessage(GetDlgItem(hwnd, IDC_DESKTOPBOOST), UDM_SETPOS32, 0, 1);
    SendMessage(GetDlgItem(hwnd, IDC_MICBOOST), UDM_SETPOS32, 0, 1);
    SendMessage(GetDlgItem(hwnd, IDC_MICTIMEOFFSET), UDM_SETPOS32, 0, 0);

    ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
    SetChangedSettings(true);
}*/

void SettingsAudio::RefreshDevices(AudioDeviceType desktopDeviceType)
{
    if (storage) {
        delete storage->recordingDevices;
        delete storage->playbackDevices;
    }
    delete storage;

    HWND hwndTemp = GetDlgItem(hwnd, IDC_MICDEVICES);
    HWND hwndPlayback = GetDlgItem(hwnd, IDC_PLAYBACKDEVICES);

    SendMessage(hwndTemp, CB_RESETCONTENT, 0, 0);
    SendMessage(hwndPlayback, CB_RESETCONTENT, 0, 0);

    storage = new AudioDeviceStorage;

    storage->playbackDevices = new AudioDeviceList;
    GetAudioDevices((*storage->playbackDevices), desktopDeviceType, bDisplayConnectedOnly, false);

    storage->recordingDevices = new AudioDeviceList;
    GetAudioDevices((*storage->recordingDevices), ADT_RECORDING, bDisplayConnectedOnly, true);

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
}

INT_PTR SettingsAudio::ProcMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                LocalizeWindow(hwnd);

                //--------------------------------------------

                bDisplayConnectedOnly = GlobalConfig->GetInt(L"Audio", L"DisplayConntectedOnly", true) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_CONNECTEDONLY), BM_SETCHECK, bDisplayConnectedOnly ? BST_CHECKED : BST_UNCHECKED, 0);

                useInputDevices = AppConfig->GetInt(L"Audio", L"UseInputDevices", false) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_USEINPUTDEVICES), BM_SETCHECK, useInputDevices ? BST_CHECKED : BST_UNCHECKED, 0);

                RefreshDevices(useInputDevices ? ADT_RECORDING : ADT_PLAYBACK);

                //--------------------------------------------

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

                int bufferTime = GlobalConfig->GetInt(TEXT("General"), TEXT("SceneBufferingTime"), 700);

                int micTimeOffset = AppConfig->GetInt(TEXT("Audio"), TEXT("MicTimeOffset"), 0);
                if(micTimeOffset < -bufferTime)
                    micTimeOffset = -bufferTime;
                else if(micTimeOffset > 20000)
                    micTimeOffset = 20000;

                SendMessage(GetDlgItem(hwnd, IDC_MICTIMEOFFSET), UDM_SETRANGE32, -bufferTime, 20000);
                SendMessage(GetDlgItem(hwnd, IDC_MICTIMEOFFSET), UDM_SETPOS32, 0, micTimeOffset);

                //--------------------------------------------

                SetChangedSettings(false);
                return TRUE;
            }

        case WM_DESTROY:
            {
                delete storage->recordingDevices;
                delete storage->playbackDevices;
                delete storage;
                storage = NULL;
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
                            SetChangedSettings(true);
                        }
                        break;

                    case IDC_RESETMIC:
                        {
                            App->ResetMic();
                            break;
                        }

                    case IDC_CONNECTEDONLY:
                        {
                            bDisplayConnectedOnly = !bDisplayConnectedOnly;
                            GlobalConfig->SetInt(L"Audio", L"DisplayConntectedOnly", bDisplayConnectedOnly);

                            RefreshDevices(useInputDevices ? ADT_RECORDING : ADT_PLAYBACK);
                            SetChangedSettings(true);
                            break;
                        }

                    case IDC_USEINPUTDEVICES:
                        {
                            useInputDevices = !useInputDevices;
                            RefreshDevices(useInputDevices ? ADT_RECORDING : ADT_PLAYBACK);
                            SetChangedSettings(true);
                            break;
                        }

                    case IDC_MICTIMEOFFSET_EDIT:
                    case IDC_DESKTOPBOOST_EDIT:
                    case IDC_MICBOOST_EDIT:
                    case IDC_PUSHTOTALKHOTKEY:
                    case IDC_PUSHTOTALKHOTKEY2:
                    case IDC_MUTEMICHOTKEY:
                    case IDC_MUTEDESKTOPHOTKEY:
                    case IDC_PTTDELAY_EDIT:
                        if(HIWORD(wParam) == EN_CHANGE)
                            SetChangedSettings(true);
                        break;

                    case IDC_CLEARPUSHTOTALK:
                        if(HIWORD(wParam) == BN_CLICKED)
                        {
                            SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY), HKM_SETHOTKEY, 0, 0);
                            SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY2), HKM_SETHOTKEY, 0, 0);
                            SetChangedSettings(true);
                        }
                        break;

                    case IDC_CLEARMUTEMIC:
                        if(HIWORD(wParam) == BN_CLICKED)
                        {
                            SendMessage(GetDlgItem(hwnd, IDC_MUTEMICHOTKEY), HKM_SETHOTKEY, 0, 0);
                            SetChangedSettings(true);
                        }
                        break;

                    case IDC_CLEARMUTEDESKTOP:
                        if(HIWORD(wParam) == BN_CLICKED)
                        {
                            SendMessage(GetDlgItem(hwnd, IDC_MUTEDESKTOPHOTKEY), HKM_SETHOTKEY, 0, 0);
                            SetChangedSettings(true);
                        }
                        break;

                    case IDC_FORCEMONO:
                        if(HIWORD(wParam) == BN_CLICKED)
                            SetChangedSettings(true);
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
                    SetChangedSettings(true);
                }
                break;
            }
    }
    return FALSE;
}
