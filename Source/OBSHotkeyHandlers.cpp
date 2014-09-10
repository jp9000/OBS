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


void STDCALL OBS::StartStreamHotkey(DWORD hotkey, UPARAM param, bool bDown)
{
    if(App->bStopStreamHotkeyDown)
        return;

    if(App->bStartStreamHotkeyDown && !bDown)
        App->bStartStreamHotkeyDown = false;
    else if(!App->bRunning || !App->bStreaming)
    {
        if(App->bStartStreamHotkeyDown = bDown)
            App->Start();
    }
}

void STDCALL OBS::StopStreamHotkey(DWORD hotkey, UPARAM param, bool bDown)
{
    if(App->bStartStreamHotkeyDown)
        return;

    if(App->bStopStreamHotkeyDown && !bDown)
        App->bStopStreamHotkeyDown = false;
    else if(App->bRunning)
    {
        if(App->bStopStreamHotkeyDown = bDown)
            App->Stop();
    }
}

void STDCALL OBS::StartRecordingHotkey(DWORD hotkey, UPARAM param, bool bDown)
{
    if (App->bStopRecordingHotkeyDown)
        return;

    if (App->bStartRecordingHotkeyDown && !bDown)
        App->bStartRecordingHotkeyDown = false;
    else if (!App->bRecording && App->canRecord)
    {
        if (!(App->bStartRecordingHotkeyDown = bDown))
            return;

        if (!App->bRunning && !App->bStreaming)
            App->Start(true);

        App->StartRecording(true);
    }
}

void STDCALL OBS::StopRecordingHotkey(DWORD hotkey, UPARAM param, bool bDown)
{
    if (App->bStartRecordingHotkeyDown)
        return;

    if (App->bStopRecordingHotkeyDown && !bDown)
        App->bStopRecordingHotkeyDown = false;
    else if (App->bRunning)
    {
        if (App->bStopRecordingHotkeyDown = bDown)
            App->StopRecording();
    }
}

void STDCALL OBS::StartReplayBufferHotkey(DWORD hotkey, UPARAM param, bool bDown)
{
    if (App->bStopReplayBufferHotkeyDown)
        return;

    if (App->bStartReplayBufferHotkeyDown && !bDown)
        App->bStartReplayBufferHotkeyDown = false;
    else if (!App->bRecordingReplayBuffer && App->canRecord)
    {
        if (!(App->bStartReplayBufferHotkeyDown = bDown))
            return;

        App->StartReplayBuffer();
    }
}

void STDCALL OBS::StopReplayBufferHotkey(DWORD hotkey, UPARAM param, bool bDown)
{
    if (App->bStartReplayBufferHotkeyDown)
        return;

    if (App->bStopReplayBufferHotkeyDown && !bDown)
        App->bStopReplayBufferHotkeyDown = false;
    else if (App->bRunning)
    {
        if (App->bStopReplayBufferHotkeyDown = bDown)
            App->StopReplayBuffer();
    }
}

void STDCALL OBS::SaveReplayBufferHotkey(DWORD hotkey, UPARAM param, bool bDown)
{
    if (App->bSaveReplayBufferHotkeyDown && !bDown)
        App->bSaveReplayBufferHotkeyDown = false;
    else if (App->bRunning)
    {
        if (App->bSaveReplayBufferHotkeyDown = bDown)
            SaveReplayBuffer(App->replayBuffer, (DWORD)(App->GetVideoTime() - App->firstFrameTimestamp));
    }
}

void StartRecordingFromReplayBuffer(ReplayBuffer *rb);

void STDCALL OBS::RecordFromReplayBufferHotkey(DWORD hotkey, UPARAM param, bool bDown)
{
    if (App->bRecordFromReplayBufferHotkeyDown && !bDown)
        App->bRecordFromReplayBufferHotkeyDown = false;
    else if (App->bRunning)
    {
        if (App->bRecordFromReplayBufferHotkeyDown = bDown)
            StartRecordingFromReplayBuffer(App->replayBuffer);
    }
}

void STDCALL OBS::PushToTalkHotkey(DWORD hotkey, UPARAM param, bool bDown)
{
    if(bDown)
    {
        App->pushToTalkDown++;
        App->bPushToTalkOn = true;
    }
    else
    {
        App->pushToTalkDown--;
        if(!App->pushToTalkDown && App->pushToTalkDelay <= 0)
            App->bPushToTalkOn = false;
    }

    App->pushToTalkTimeLeft = App->pushToTalkDelay*1000000;
    OSDebugOut(TEXT("Actual delay: %d"), App->pushToTalkDelay);
}


void STDCALL OBS::MuteMicHotkey(DWORD hotkey, UPARAM param, bool bDown)
{
    if(!bDown) return;

    if(App->micAudio)
    {
        App->micVol = ToggleVolumeControlMute(GetDlgItem(hwndMain, ID_MICVOLUME));
        App->ReportMicVolumeChange(App->micVol, App->micVol < VOLN_MUTELEVEL, true);
    }
}

void STDCALL OBS::MuteDesktopHotkey(DWORD hotkey, UPARAM param, bool bDown)
{
    if(!bDown) return;

    App->desktopVol = ToggleVolumeControlMute(GetDlgItem(hwndMain, ID_DESKTOPVOLUME));
    App->ReportDesktopVolumeChange(App->desktopVol, App->desktopVol < VOLN_MUTELEVEL, true);
}
