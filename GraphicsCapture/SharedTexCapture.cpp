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
    for(UINT i=0; i<2; i++)
    {
        delete sharedTextures[i];
        sharedTextures[i] = NULL;
    }

    bInitialized = false;

    texData = NULL;

    if(sharedMemory)
        UnmapViewOfFile(sharedMemory);

    if(hFileMap)
        CloseHandle(hFileMap);
}

bool SharedTexCapture::Init(HANDLE hProcess, HWND hwndTarget, CaptureInfo &info)
{
    this->hwndTarget = hwndTarget;
    this->hProcess = hProcess;

    String strFileMapName;
    strFileMapName << TEXTURE_MEMORY << UIntString(info.mapID);

    hFileMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, strFileMapName);
    if(hFileMap == NULL)
    {
        AppWarning(TEXT("SharedTexCapture::Init: Could not open file mapping"));
        return false;
    }

    sharedMemory = (LPBYTE)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, info.mapSize);
    if(!sharedMemory)
    {
        AppWarning(TEXT("SharedTexCapture::Init: Could not map view of file"));
        return false;
    }

    Log(TEXT("using shared texture capture"));

    //---------------------------------------

    texData = (SharedTexData*)sharedMemory;
    texData->frameTime = 1000000/API->GetMaxFPS()/2;

    for(UINT i=0; i<2; i++)
    {
        sharedTextures[i] = GS->CreateTextureFromSharedHandle(info.cx, info.cy, (GSColorFormat)info.format, texData->texHandles[i]);
        if(!sharedTextures[i])
        {
            AppWarning(TEXT("SharedTexCapture::Init: Could not create shared texture"));
            return false;
        }
    }

    bInitialized = true;
    return true;
}

Texture* SharedTexCapture::LockTexture()
{
    curTextureID = texData->lastRendered;

    if(curTextureID < 2)
    {
        DWORD nextTexture = (curTextureID == 1) ? 0 : 1;
        bool bSuccess = false;

        if(sharedTextures[curTextureID]->AcquireSync(0, 0) == WAIT_OBJECT_0)
            bSuccess = true;
        else if(sharedTextures[nextTexture]->AcquireSync(0, 0) == WAIT_OBJECT_0)
        {
            bSuccess = true;
            curTextureID = nextTexture;
        }

        if(bSuccess)
        {
            curTexture = sharedTextures[curTextureID];
            return curTexture;
        }
    }

    return NULL;
}

void SharedTexCapture::UnlockTexture()
{
    if(curTexture)
    {
        curTexture->ReleaseSync(0);
        curTexture = NULL;
    }
}
