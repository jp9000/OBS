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


#include "GraphicsCapture.h"


void SharedTexCapture::Destroy()
{
    delete sharedTexture;
    sharedTexture = NULL;

    delete copyTexture;
    copyTexture = NULL;

    bInitialized = false;

    texData = NULL;

    if(sharedMemory)
        UnmapViewOfFile(sharedMemory);

    if(hFileMap)
        CloseHandle(hFileMap);
}

bool SharedTexCapture::Init(CaptureInfo &info)
{
    String strFileMapName;
    strFileMapName << TEXTURE_MEMORY << UIntString(info.mapID);

    hFileMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, strFileMapName);
    if(hFileMap == NULL)
    {
        AppWarning(TEXT("SharedTexCapture::Init: Could not open file mapping: %d"), GetLastError());
        return false;
    }

    sharedMemory = (LPBYTE)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, info.mapSize);
    if(!sharedMemory)
    {
        AppWarning(TEXT("SharedTexCapture::Init: Could not map view of file"));
        return false;
    }

    //Log(TEXT("using shared texture capture"));

    //---------------------------------------

    texData = (SharedTexData*)sharedMemory;
    texData->frameTime = 1000000/API->GetMaxFPS()/2;

    sharedTexture = GS->CreateTextureFromSharedHandle(info.cx, info.cy, (HANDLE)texData->texHandle);
    if(!sharedTexture)
    {
        AppWarning(TEXT("SharedTexCapture::Init: Could not create shared texture"));
        return false;
    }

    copyTexture = GS->CreateTexture(info.cx, info.cy, (GSColorFormat)info.format, 0, FALSE, TRUE);

    Log(TEXT("SharedTexCapture hooked"));

    bInitialized = true;
    return true;
}

Texture* SharedTexCapture::LockTexture()
{
    GS->CopyTexture(copyTexture, sharedTexture);
    return copyTexture;
}

void SharedTexCapture::UnlockTexture()
{
}
