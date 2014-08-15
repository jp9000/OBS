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
{
}

SettingsAudio::~SettingsAudio()
{
}

CTSTR SettingsAudio::GetCategory() const
{
    static CTSTR name = Str("Settings.Audio");
    return name;
}

HWND SettingsAudio::CreatePane(HWND parentHwnd)
{
    hwnd = OBSCreateDialog(hinstMain, MAKEINTRESOURCE(IDD_SETTINGS_AUDIO), parentHwnd, (DLGPROC)DialogProc, (LPARAM)this);
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
        strPlaybackDevice = storage.playbackDevices.devices[iPlaybackDevice].strID;
    }

    AppConfig->SetString(TEXT("Audio"), TEXT("PlaybackDevice"), strPlaybackDevice);

    UINT iDevice = (UINT)SendMessage(GetDlgItem(hwnd, IDC_MICDEVICES), CB_GETCURSEL, 0, 0);

    String strDevice;

    if(iDevice == CB_ERR)
        strDevice = TEXT("Disable");
    else
        strDevice = storage.recordingDevices.devices[iDevice].strID;


    AppConfig->SetString(TEXT("Audio"), TEXT("Device"), strDevice);


    if(strDevice.CompareI(TEXT("Disable")))
        EnableWindow(GetDlgItem(hwndMain, ID_MICVOLUME), FALSE);
    else
        EnableWindow(GetDlgItem(hwndMain, ID_MICVOLUME), TRUE);

    //------------------------------------

    App->pushToTalkDelay = (int)SendMessage(GetDlgItem(hwnd, IDC_PTTDELAY), UDM_GETPOS32, 0, 0);
    if(App->pushToTalkDelay < 0)
        App->pushToTalkDelay = 0;
    else if(App->pushToTalkDelay > 2000)
        App->pushToTalkDelay = 2000;
    AppConfig->SetInt(TEXT("Audio"), TEXT("PushToTalkDelay"), (int)App->pushToTalkDelay);

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
    storage.playbackDevices.FreeData();
    storage.recordingDevices.FreeData();

    HWND hwndTemp = GetDlgItem(hwnd, IDC_MICDEVICES);
    HWND hwndPlayback = GetDlgItem(hwnd, IDC_PLAYBACKDEVICES);

    SendMessage(hwndTemp, CB_RESETCONTENT, 0, 0);
    SendMessage(hwndPlayback, CB_RESETCONTENT, 0, 0);

    GetAudioDevices(storage.playbackDevices, desktopDeviceType, bDisplayConnectedOnly, false);

    GetAudioDevices(storage.recordingDevices, ADT_RECORDING, bDisplayConnectedOnly, true);

    for(UINT i=0; i<storage.playbackDevices.devices.Num(); i++)
        SendMessage(hwndPlayback, CB_ADDSTRING, 0, (LPARAM)storage.playbackDevices.devices[i].strName.Array());

    for(UINT i=0; i<storage.recordingDevices.devices.Num(); i++)
        SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)storage.recordingDevices.devices[i].strName.Array());

    String strPlaybackID;
    String strDeviceID;

    if (storage.playbackDevices.devices.Num())
        strPlaybackID = AppConfig->GetString(TEXT("Audio"), TEXT("PlaybackDevice"), storage.playbackDevices.devices[0].strID);
    else
        strPlaybackID = AppConfig->GetString(TEXT("Audio"), TEXT("PlaybackDevice"));

    if (storage.recordingDevices.devices.Num())
        strDeviceID = AppConfig->GetString(TEXT("Audio"), TEXT("Device"), storage.recordingDevices.devices[0].strID);
    else
        strDeviceID = AppConfig->GetString(TEXT("Audio"), TEXT("Device"));

    UINT iPlaybackDevice;
    for(iPlaybackDevice=0; iPlaybackDevice<storage.playbackDevices.devices.Num(); iPlaybackDevice++)
    {
        if(storage.playbackDevices.devices[iPlaybackDevice].strID == strPlaybackID)
        {
            SendMessage(hwndPlayback, CB_SETCURSEL, iPlaybackDevice, 0);
            break;
        }
    }

    UINT iDevice;
    for(iDevice=0; iDevice<storage.recordingDevices.devices.Num(); iDevice++)
    {
        if(storage.recordingDevices.devices[iDevice].strID == strDeviceID)
        {
            SendMessage(hwndTemp, CB_SETCURSEL, iDevice, 0);
            break;
        }
    }

    if (iPlaybackDevice && iPlaybackDevice == storage.playbackDevices.devices.Num())
    {
        AppConfig->SetString(TEXT("Audio"), TEXT("PlaybackDevice"), storage.playbackDevices.devices[0].strID);
        SendMessage(hwndPlayback, CB_SETCURSEL, 0, 0);

        SetChangedSettings(true);
    }

    if (iDevice && iDevice == storage.recordingDevices.devices.Num())
    {
        AppConfig->SetString(TEXT("Audio"), TEXT("Device"), storage.recordingDevices.devices[0].strID);
        SendMessage(hwndTemp, CB_SETCURSEL, 0, 0);

        SetChangedSettings(true);
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

                useInputDevices = AppConfig->GetInt(L"Audio", L"InputDevicesForDesktopSound", false) != 0;

                //--------------------------------------------

                int pttDelay = AppConfig->GetInt(TEXT("Audio"), TEXT("PushToTalkDelay"), 200);
                SendMessage(GetDlgItem(hwnd, IDC_PTTDELAY), UDM_SETRANGE32, 0, 2000);
                SendMessage(GetDlgItem(hwnd, IDC_PTTDELAY), UDM_SETPOS32, 0, pttDelay);

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

                RefreshDevices(useInputDevices ? ADT_RECORDING : ADT_PLAYBACK);

                return TRUE;
            }

        case WM_DESTROY:
            {
            }

        case WM_COMMAND:
            {
                bool bDataChanged = false;

                switch(LOWORD(wParam))
                {
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

                    case IDC_MICTIMEOFFSET_EDIT:
                    case IDC_DESKTOPBOOST_EDIT:
                    case IDC_MICBOOST_EDIT:
                    case IDC_PTTDELAY_EDIT:
                        if(HIWORD(wParam) == EN_CHANGE)
                            SetChangedSettings(true);
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
                    if (App->GetVideoEncoder())
                        ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                    SetChangedSettings(true);
                }
                break;
            }
    }
    return FALSE;
}
