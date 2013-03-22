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


#pragma once


#define OBS_WINDOW_CLASS        TEXT("OBSWindowClass")

//events sent by receiver (OBS)
#define RESTART_CAPTURE_EVENT   TEXT("OBS_RestartCapture")
#define END_CAPTURE_EVENT       TEXT("OBS_EndCapture")

//events sent by sender (graphics app)
#define CAPTURE_READY_EVENT     TEXT("OBS_CaptureReady")
#define APP_EXIT_EVENT          TEXT("OBS_AppExit")

#define TEXTURE_MUTEX1          TEXT("OBSTextureMutex1")
#define TEXTURE_MUTEX2          TEXT("OBSTextureMutex2")

#define INFO_MEMORY             TEXT("Local\\OBSInfoMemory")
#define TEXTURE_MEMORY          TEXT("Local\\OBSTextureMemory")

#define OBS_KEEPALIVE_EVENT     TEXT("OBS_KeepAlive")

#define CAPTURETYPE_MEMORY      1
#define CAPTURETYPE_SHAREDTEX   2

inline HANDLE GetEvent(LPCTSTR lpEvent)
{
    HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, lpEvent);
    if(!hEvent)
        hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, lpEvent);

    return hEvent;
}

#pragma pack(push, 8)

struct MemoryCopyData
{
    UINT        lastRendered;
    LONGLONG    frameTime;
    DWORD       texture1Offset, texture2Offset;
};

struct SharedTexData
{
    LONGLONG    frameTime;
    DWORD       texHandle;
};

struct CaptureInfo
{
    UINT    captureType;
    DWORD   format;
    UINT    cx, cy;
    BOOL    bFlip;

    UINT    pitch;
    UINT    mapID;
    DWORD   mapSize;

    DWORD   hwndCapture;
};

#pragma pack(pop)
