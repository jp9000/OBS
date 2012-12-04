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


#define RECEIVER_WINDOWCLASS    TEXT("OBSGraphicsCaptureReceiver")
#define SENDER_WINDOWCLASS      TEXT("OBSGraphicsCaptureSender")

#define TEXTURE_MUTEX1          TEXT("OBSTextureMutex1")
#define TEXTURE_MUTEX2          TEXT("OBSTextureMutex2")

#define TEXTURE_MEMORY          TEXT("Local\\OBSTextureMemory")

#define CAPTURETYPE_MEMORY      1
#define CAPTURETYPE_SHAREDTEX   2

struct MemoryCopyData
{
    UINT        lastRendered;
    LONGLONG    frameTime;
    DWORD       texture1Offset, texture2Offset;
};

struct SharedTexData
{
    UINT        lastRendered;
    LONGLONG    frameTime;
    HANDLE      texHandles[2];
};

struct CaptureInfo
{
    UINT    captureType;
    DWORD   format;
    UINT    cx, cy;
    HWND    hwndSender;
    HWND    hwndCapture;
    BOOL    bFlip;

    UINT    pitch;
    UINT    mapID;
    DWORD   mapSize;
};

enum
{
    RECEIVER_NEWCAPTURE=WM_USER+1,
    RECEIVER_ENDCAPTURE,
};

enum
{
    SENDER_RESTARTCAPTURE=WM_USER+1,
    SENDER_ENDCAPTURE,
};

