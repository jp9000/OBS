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
    auto update_hotkey = [&](decltype(App->pushToTalkHotkeyID) &hotkey, int dlg_id, CTSTR section, CTSTR key, OBSHOTKEYPROC proc, bool enabled)
    {
        if (hotkey)
        {
            API->DeleteHotkey(hotkey);
            hotkey = 0;
        }

        DWORD new_hotkey = (DWORD)SendMessage(GetDlgItem(hwnd, dlg_id), HKM_GETHOTKEY, 0, 0);
        if (enabled && new_hotkey)
            hotkey = API->CreateHotkey(new_hotkey, proc, NULL);

        AppConfig->SetInt(section, key, new_hotkey);
        
        return new_hotkey;
    };

    //------------------------------------

    App->bUsingPushToTalk = SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALK), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt(L"Audio", L"UsePushToTalk", App->bUsingPushToTalk);

    //------------------------------------

    update_hotkey(App->pushToTalkHotkeyID, IDC_PUSHTOTALKHOTKEY, L"Audio", L"PushToTalkHotkey", OBS::PushToTalkHotkey, App->bUsingPushToTalk);
    update_hotkey(App->pushToTalkHotkey2ID, IDC_PUSHTOTALKHOTKEY2, L"Audio", L"PushToTalkHotkey2", OBS::PushToTalkHotkey, App->bUsingPushToTalk);

    //------------------------------------

    update_hotkey(App->muteMicHotkeyID, IDC_MUTEMICHOTKEY, L"Audio", L"MuteMicHotkey", OBS::MuteMicHotkey, true);

    //------------------------------------

    update_hotkey(App->muteDesktopHotkeyID, IDC_MUTEDESKTOPHOTKEY, L"Audio", L"MuteDesktopHotkey", OBS::MuteDesktopHotkey, true);

    //------------------------------------------

    update_hotkey(App->stopStreamHotkeyID, IDC_STOPSTREAMHOTKEY, L"Publish", L"StopStreamHotkey", OBS::StopStreamHotkey, true);

    //------------------------------------------

    update_hotkey(App->startStreamHotkeyID, IDC_STARTSTREAMHOTKEY, L"Publish", L"StartStreamHotkey", OBS::StartStreamHotkey, true);

    //------------------------------------------

    update_hotkey(App->stopRecordingHotkeyID, IDC_STOPRECORDINGHOTKEY, L"Publish", L"StopRecordingHotkey", OBS::StopRecordingHotkey, true);

    //------------------------------------------

    update_hotkey(App->startRecordingHotkeyID, IDC_STARTRECORDINGHOTKEY, L"Publish", L"StartRecordingHotkey", OBS::StartRecordingHotkey, true);

    //------------------------------------------

    update_hotkey(App->stopReplayBufferHotkeyID, IDC_STOPREPLAYBUFFERHOTKEY, L"Publish", L"StopReplayBufferHotkey", OBS::StopReplayBufferHotkey, true);

    //------------------------------------------

    update_hotkey(App->startReplayBufferHotkeyID, IDC_STARTREPLAYBUFFERHOTKEY, L"Publish", L"StartReplayBufferHotkey", OBS::StartReplayBufferHotkey, true);

    //------------------------------------------

    update_hotkey(App->saveReplayBufferHotkeyID, IDC_SAVEREPLAYBUFFERHOTKEY, L"Publish", L"SaveReplayBufferHotkey", OBS::SaveReplayBufferHotkey, true);

    //------------------------------------------

    update_hotkey(App->recordFromReplayBufferHotkeyID, IDC_RECORDFROMREPLAYBUFFERHOTKEY, L"Publish", L"RecordFromReplayBufferHotkey", OBS::RecordFromReplayBufferHotkey, true);

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
        
        bool pushToTalk = !!AppConfig->GetInt(L"Audio", L"UsePushToTalk");
        SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALK), BM_SETCHECK, pushToTalk ? BST_CHECKED : BST_UNCHECKED, 0);

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

        startHotkey = AppConfig->GetInt(L"Publish", L"StartReplayBufferHotkey");
        SendMessage(GetDlgItem(hwnd, IDC_STARTREPLAYBUFFERHOTKEY), HKM_SETHOTKEY, startHotkey, 0);

        //--------------------------------------------

        stopHotkey = AppConfig->GetInt(L"Publish", L"StopReplayBufferHotkey");
        SendMessage(GetDlgItem(hwnd, IDC_STOPREPLAYBUFFERHOTKEY), HKM_SETHOTKEY, stopHotkey, 0);

        //--------------------------------------------

        DWORD saveReplayBufferHotkey = AppConfig->GetInt(L"Publish", L"SaveReplayBufferHotkey");
        SendMessage(GetDlgItem(hwnd, IDC_SAVEREPLAYBUFFERHOTKEY), HKM_SETHOTKEY, saveReplayBufferHotkey, 0);

        //--------------------------------------------

        DWORD recordFromReplayBufferHotkey = AppConfig->GetInt(L"Publish", L"RecordFromReplayBufferHotkey");
        SendMessage(GetDlgItem(hwnd, IDC_RECORDFROMREPLAYBUFFERHOTKEY), HKM_SETHOTKEY, recordFromReplayBufferHotkey, 0);

        //--------------------------------------------

        //need this as some of the dialog item sets above trigger the notifications
        SetChangedSettings(false);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_PUSHTOTALK:
            if (HIWORD(wParam) == BN_CLICKED)
                SetChangedSettings(true);
            break;

        case IDC_PUSHTOTALKHOTKEY:
        case IDC_PUSHTOTALKHOTKEY2:
        case IDC_MUTEMICHOTKEY:
        case IDC_MUTEDESKTOPHOTKEY:
        case IDC_STARTSTREAMHOTKEY:
        case IDC_STOPSTREAMHOTKEY:
        case IDC_STARTRECORDINGHOTKEY:
        case IDC_STOPRECORDINGHOTKEY:
        case IDC_STARTREPLAYBUFFERHOTKEY:
        case IDC_STOPREPLAYBUFFERHOTKEY:
        case IDC_SAVEREPLAYBUFFERHOTKEY:
        case IDC_RECORDFROMREPLAYBUFFERHOTKEY:
            if (HIWORD(wParam) == EN_CHANGE)
                SetChangedSettings(true);
            break;

        case IDC_CLEARPUSHTOTALK:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY), HKM_SETHOTKEY, 0, 0);
                SetChangedSettings(true);
            }
            break;

        case IDC_CLEARPUSHTOTALK2:
            if (HIWORD(wParam) == BN_CLICKED)
            {
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

        case IDC_CLEARHOTKEY_STARTREPLAYBUFFER:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                if (SendMessage(GetDlgItem(hwnd, IDC_STARTREPLAYBUFFERHOTKEY), HKM_GETHOTKEY, 0, 0))
                {
                    SendMessage(GetDlgItem(hwnd, IDC_STARTREPLAYBUFFERHOTKEY), HKM_SETHOTKEY, 0, 0);
                    SetChangedSettings(true);
                }
            }
            break;

        case IDC_CLEARHOTKEY_STOPREPLAYBUFFER:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                if (SendMessage(GetDlgItem(hwnd, IDC_STOPREPLAYBUFFERHOTKEY), HKM_GETHOTKEY, 0, 0))
                {
                    SendMessage(GetDlgItem(hwnd, IDC_STOPREPLAYBUFFERHOTKEY), HKM_SETHOTKEY, 0, 0);
                    SetChangedSettings(true);
                }
            }
            break;

        case IDC_CLEARHOTKEY_SAVEREPLAYBUFFER:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                if (SendMessage(GetDlgItem(hwnd, IDC_SAVEREPLAYBUFFERHOTKEY), HKM_GETHOTKEY, 0, 0))
                {
                    SendMessage(GetDlgItem(hwnd, IDC_SAVEREPLAYBUFFERHOTKEY), HKM_SETHOTKEY, 0, 0);
                    SetChangedSettings(true);
                }
            }
            break;

        case IDC_CLEARHOTKEY_RECORDFROMREPLAYBUFFER:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                if (SendMessage(GetDlgItem(hwnd, IDC_RECORDFROMREPLAYBUFFERHOTKEY), HKM_GETHOTKEY, 0, 0))
                {
                    SendMessage(GetDlgItem(hwnd, IDC_RECORDFROMREPLAYBUFFERHOTKEY), HKM_SETHOTKEY, 0, 0);
                    SetChangedSettings(true);
                }
            }
            break;
        }

    }
    return FALSE;
}
