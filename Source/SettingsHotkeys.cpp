/********************************************************************************
Copyright (C) 2014 Ruwen Hahn <palana@stunned.de>

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
// SettingsHotkeys class

SettingsHotkeys::SettingsHotkeys()
    : SettingsPane()
{
}

SettingsHotkeys::~SettingsHotkeys()
{
}

CTSTR SettingsHotkeys::GetCategory() const
{
    static CTSTR name = Str("Settings.Hotkeys");
    return name;
}

HWND SettingsHotkeys::CreatePane(HWND parentHwnd)
{
    hwnd = OBSCreateDialog(hinstMain, MAKEINTRESOURCE(IDD_SETTINGS_HOTKEYS), parentHwnd, (DLGPROC)DialogProc, (LPARAM)this);
    return hwnd;
}

void SettingsHotkeys::DestroyPane()
{
    DestroyWindow(hwnd);
    hwnd = nullptr;
}

void SettingsHotkeys::ApplySettings()
{
    //------------------------------------

    DWORD hotkey = (DWORD)SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY), HKM_GETHOTKEY, 0, 0);
    DWORD hotkey2 = (DWORD)SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY2), HKM_GETHOTKEY, 0, 0);
    App->bUsingPushToTalk = hotkey || hotkey2;

    AppConfig->SetInt(TEXT("Audio"), TEXT("PushToTalkHotkey"), hotkey);
    AppConfig->SetInt(TEXT("Audio"), TEXT("PushToTalkHotkey2"), hotkey2);

    if (App->pushToTalkHotkeyID)
    {
        API->DeleteHotkey(App->pushToTalkHotkeyID);
        App->pushToTalkHotkeyID = 0;
    }
    if (App->pushToTalkHotkey2ID)
    {
        API->DeleteHotkey(App->pushToTalkHotkey2ID);
        App->pushToTalkHotkey2ID = 0;
    }

    if (App->bUsingPushToTalk && hotkey)
        App->pushToTalkHotkeyID = API->CreateHotkey(hotkey, OBS::PushToTalkHotkey, NULL);
    if (App->bUsingPushToTalk && hotkey2)
        App->pushToTalkHotkey2ID = API->CreateHotkey(hotkey2, OBS::PushToTalkHotkey, NULL);

    //------------------------------------

    hotkey = (DWORD)SendMessage(GetDlgItem(hwnd, IDC_MUTEMICHOTKEY), HKM_GETHOTKEY, 0, 0);
    AppConfig->SetInt(TEXT("Audio"), TEXT("MuteMicHotkey"), hotkey);

    if (App->muteMicHotkeyID)
    {
        API->DeleteHotkey(App->muteMicHotkeyID);
        App->muteDesktopHotkeyID = 0;
    }

    if (hotkey)
        App->muteMicHotkeyID = API->CreateHotkey(hotkey, OBS::MuteMicHotkey, NULL);

    //------------------------------------

    hotkey = (DWORD)SendMessage(GetDlgItem(hwnd, IDC_MUTEDESKTOPHOTKEY), HKM_GETHOTKEY, 0, 0);
    AppConfig->SetInt(TEXT("Audio"), TEXT("MuteDesktopHotkey"), hotkey);

    if (App->muteDesktopHotkeyID)
    {
        API->DeleteHotkey(App->muteDesktopHotkeyID);
        App->muteDesktopHotkeyID = 0;
    }

    if (hotkey)
        App->muteDesktopHotkeyID = API->CreateHotkey(hotkey, OBS::MuteDesktopHotkey, NULL);

    //------------------------------------------

    DWORD stopStreamHotkey = (DWORD)SendMessage(GetDlgItem(hwnd, IDC_STOPSTREAMHOTKEY), HKM_GETHOTKEY, 0, 0);
    AppConfig->SetInt(TEXT("Publish"), TEXT("StopStreamHotkey"), stopStreamHotkey);

    if (App->stopStreamHotkeyID)
    {
        API->DeleteHotkey(App->stopStreamHotkeyID);
        App->stopStreamHotkeyID = 0;
    }

    if (stopStreamHotkey)
        App->stopStreamHotkeyID = API->CreateHotkey(stopStreamHotkey, OBS::StopStreamHotkey, NULL);

    //------------------------------------------

    DWORD startStreamHotkey = (DWORD)SendMessage(GetDlgItem(hwnd, IDC_STARTSTREAMHOTKEY), HKM_GETHOTKEY, 0, 0);
    AppConfig->SetInt(TEXT("Publish"), TEXT("StartStreamHotkey"), startStreamHotkey);

    if (App->startStreamHotkeyID)
    {
        API->DeleteHotkey(App->startStreamHotkeyID);
        App->startStreamHotkeyID = 0;
    }

    if (startStreamHotkey)
        App->startStreamHotkeyID = API->CreateHotkey(startStreamHotkey, OBS::StartStreamHotkey, NULL);

    //------------------------------------------

    DWORD stopRecordingHotkey = (DWORD)SendMessage(GetDlgItem(hwnd, IDC_STOPRECORDINGHOTKEY), HKM_GETHOTKEY, 0, 0);
    AppConfig->SetInt(TEXT("Publish"), TEXT("StopRecordingHotkey"), stopRecordingHotkey);

    if (App->stopRecordingHotkeyID)
    {
        API->DeleteHotkey(App->stopRecordingHotkeyID);
        App->stopRecordingHotkeyID = 0;
    }

    if (stopRecordingHotkey)
        App->stopRecordingHotkeyID = API->CreateHotkey(stopRecordingHotkey, OBS::StopRecordingHotkey, NULL);

    //------------------------------------------

    DWORD startRecordingHotkey = (DWORD)SendMessage(GetDlgItem(hwnd, IDC_STARTRECORDINGHOTKEY), HKM_GETHOTKEY, 0, 0);
    AppConfig->SetInt(TEXT("Publish"), TEXT("StartRecordingHotkey"), startRecordingHotkey);

    if (App->startRecordingHotkeyID)
    {
        API->DeleteHotkey(App->startRecordingHotkeyID);
        App->startRecordingHotkeyID = 0;
    }

    if (startRecordingHotkey)
        App->startRecordingHotkeyID = API->CreateHotkey(startRecordingHotkey, OBS::StartRecordingHotkey, NULL);
}

void SettingsHotkeys::CancelSettings()
{
}

bool SettingsHotkeys::HasDefaults() const
{
    return false;
}

INT_PTR SettingsHotkeys::ProcMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        LocalizeWindow(hwnd);

        //--------------------------------------------

        DWORD hotkey = AppConfig->GetInt(TEXT("Audio"), TEXT("PushToTalkHotkey"));
        SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY), HKM_SETHOTKEY, hotkey, 0);
        DWORD hotkey2 = AppConfig->GetInt(TEXT("Audio"), TEXT("PushToTalkHotkey2"));
        SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY2), HKM_SETHOTKEY, hotkey2, 0);

        //--------------------------------------------

        hotkey = AppConfig->GetInt(TEXT("Audio"), TEXT("MuteMicHotkey"));
        SendMessage(GetDlgItem(hwnd, IDC_MUTEMICHOTKEY), HKM_SETHOTKEY, hotkey, 0);

        //--------------------------------------------

        hotkey = AppConfig->GetInt(TEXT("Audio"), TEXT("MuteDesktopHotkey"));
        SendMessage(GetDlgItem(hwnd, IDC_MUTEDESKTOPHOTKEY), HKM_SETHOTKEY, hotkey, 0);

        //--------------------------------------------

        DWORD startHotkey = AppConfig->GetInt(TEXT("Publish"), TEXT("StartStreamHotkey"));
        SendMessage(GetDlgItem(hwnd, IDC_STARTSTREAMHOTKEY), HKM_SETHOTKEY, startHotkey, 0);

        //--------------------------------------------

        DWORD stopHotkey = AppConfig->GetInt(TEXT("Publish"), TEXT("StopStreamHotkey"));
        SendMessage(GetDlgItem(hwnd, IDC_STOPSTREAMHOTKEY), HKM_SETHOTKEY, stopHotkey, 0);

        //--------------------------------------------

        startHotkey = AppConfig->GetInt(TEXT("Publish"), TEXT("StartRecordingHotkey"));
        SendMessage(GetDlgItem(hwnd, IDC_STARTRECORDINGHOTKEY), HKM_SETHOTKEY, startHotkey, 0);

        //--------------------------------------------

        stopHotkey = AppConfig->GetInt(TEXT("Publish"), TEXT("StopRecordingHotkey"));
        SendMessage(GetDlgItem(hwnd, IDC_STOPRECORDINGHOTKEY), HKM_SETHOTKEY, stopHotkey, 0);

        //--------------------------------------------

        //need this as some of the dialog item sets above trigger the notifications
        SetChangedSettings(false);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_PUSHTOTALKHOTKEY:
        case IDC_PUSHTOTALKHOTKEY2:
        case IDC_MUTEMICHOTKEY:
        case IDC_MUTEDESKTOPHOTKEY:
        case IDC_STARTSTREAMHOTKEY:
        case IDC_STOPSTREAMHOTKEY:
        case IDC_STARTRECORDINGHOTKEY:
        case IDC_STOPRECORDINGHOTKEY:
            if (HIWORD(wParam) == EN_CHANGE)
                SetChangedSettings(true);
            break;

        case IDC_CLEARPUSHTOTALK:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY), HKM_SETHOTKEY, 0, 0);
                SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY2), HKM_SETHOTKEY, 0, 0);
                SetChangedSettings(true);
            }
            break;

        case IDC_CLEARMUTEMIC:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                SendMessage(GetDlgItem(hwnd, IDC_MUTEMICHOTKEY), HKM_SETHOTKEY, 0, 0);
                SetChangedSettings(true);
            }
            break;

        case IDC_CLEARMUTEDESKTOP:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                SendMessage(GetDlgItem(hwnd, IDC_MUTEDESKTOPHOTKEY), HKM_SETHOTKEY, 0, 0);
                SetChangedSettings(true);
            }
            break;

        case IDC_CLEARHOTKEY_STARTSTREAM:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                if (SendMessage(GetDlgItem(hwnd, IDC_STARTSTREAMHOTKEY), HKM_GETHOTKEY, 0, 0))
                {
                    SendMessage(GetDlgItem(hwnd, IDC_STARTSTREAMHOTKEY), HKM_SETHOTKEY, 0, 0);
                    SetChangedSettings(true);
                }
            }
            break;

        case IDC_CLEARHOTKEY:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                if (SendMessage(GetDlgItem(hwnd, IDC_STOPSTREAMHOTKEY), HKM_GETHOTKEY, 0, 0))
                {
                    SendMessage(GetDlgItem(hwnd, IDC_STOPSTREAMHOTKEY), HKM_SETHOTKEY, 0, 0);
                    SetChangedSettings(true);
                }
            }
            break;

        case IDC_CLEARHOTKEY_STARTRECORDING:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                if (SendMessage(GetDlgItem(hwnd, IDC_STARTRECORDINGHOTKEY), HKM_GETHOTKEY, 0, 0))
                {
                    SendMessage(GetDlgItem(hwnd, IDC_STARTRECORDINGHOTKEY), HKM_SETHOTKEY, 0, 0);
                    SetChangedSettings(true);
                }
            }
            break;

        case IDC_CLEARHOTKEY_STOPRECORDING:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                if (SendMessage(GetDlgItem(hwnd, IDC_STOPRECORDINGHOTKEY), HKM_GETHOTKEY, 0, 0))
                {
                    SendMessage(GetDlgItem(hwnd, IDC_STOPRECORDINGHOTKEY), HKM_SETHOTKEY, 0, 0);
                    SetChangedSettings(true);
                }
            }
            break;
        }

    }
    return FALSE;
}
