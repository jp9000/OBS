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

/*
#include "GraphicsCapture.h"


SharedTexCapture::~SharedTexCapture()
{
    for(UINT i=0; i<2; i++)
        delete sharedTextures[i];
}

bool SharedTexCapture::Init(HANDLE hProcess, HWND hwndTarget, CaptureInfo &info)
{
    this->hwndTarget = hwndTarget;
    this->hProcess = hProcess;
    this->height = info.cy;

    colorFormat = info.format;
    lpDataAddress = info.data;

    SharedTextureData sharedTextureData;
    if(!ReadProcessMemory(hProcess, lpDataAddress, &sharedTextureData, sizeof(sharedTextureData), NULL))
        return false;

    bool bSuccess = true;
    for(UINT i=0; i<2; i++)
    {
        sharedTextures[i] = GS->CreateTextureFromSharedHandle(info.cx, info.cy, (GSColorFormat)colorFormat, sharedTextureData.sharedTextureHandles[i]);
        if(!sharedTextures[i])
        {
            bSuccess = false;
            break;
        }
    }

    if(!bSuccess)
    {
        for(UINT i=0; i<2; i++)
            delete sharedTextures[i];

        return false;
    }

    return true;
}

Texture* SharedTexCapture::LockTexture()
{
    SharedTextureData textureData;
    if(ReadProcessMemory(hProcess, lpDataAddress, &textureData, sizeof(textureData), NULL))
    {
        curTextureID = textureData.lastRendered;

        if(curTextureID < 2)
        {
            DWORD nextTexture = (curTextureID == 1) ? 0 : 1;
            bool bSuccess = false;

            if(sharedTextures[curTextureID]->AcquireSync(1, 0) == WAIT_OBJECT_0)
                bSuccess = true;
            else if(sharedTextures[nextTexture]->AcquireSync(1, 0) == WAIT_OBJECT_0)
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

*/