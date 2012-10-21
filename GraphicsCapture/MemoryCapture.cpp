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


MemoryCapture::~MemoryCapture()
{
    if(sharedMemory)
        UnmapViewOfFile(sharedMemory);
    if(hFileMap)
        CloseHandle(hFileMap);

    delete texture;
}

bool MemoryCapture::Init(HANDLE hProcess, HWND hwndTarget, CaptureInfo &info)
{
    this->hwndTarget = hwndTarget;
    this->hProcess = hProcess;
    this->height = info.cy;
    this->pitch = info.pitch;

    String strFileMapName;
    strFileMapName << TEXTURE_MEMORY << UIntString(info.mapID);

    hFileMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, strFileMapName);
    if(hFileMap == NULL)
    {
        AppWarning(TEXT("MemoryCapture::Init: Could not open file mapping"));
        return false;
    }

    sharedMemory = (LPBYTE)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, info.mapSize);
    if(!sharedMemory)
    {
        AppWarning(TEXT("MemoryCapture::Init: Could not map view of file"));
        return false;
    }

    copyData = (MemoryCopyData*)sharedMemory;
    textureBuffers[0] = sharedMemory+copyData->texture1Offset;
    textureBuffers[1] = sharedMemory+copyData->texture2Offset;

    texture = CreateTexture(info.cx, info.cy, (GSColorFormat)info.format, NULL, NULL, FALSE);
    if(!texture)
    {
        AppWarning(TEXT("MemoryCapture::Init: Could not create texture"));
        return false;
    }

    return true;
}

Texture* MemoryCapture::LockTexture()
{
    LPVOID address = NULL;

    curTexture = copyData->lastRendered;

    if(curTexture < 2)
    {
        DWORD nextTexture = (curTexture == 1) ? 0 : 1;
    
        if(WaitForSingleObject(textureMutexes[curTexture], 0) == WAIT_OBJECT_0)
            hMutex = textureMutexes[curTexture];
        else if(WaitForSingleObject(textureMutexes[nextTexture], 0) == WAIT_OBJECT_0)
        {
            hMutex = textureMutexes[nextTexture];
            curTexture = nextTexture;
        }

        if(hMutex)
        {
            BYTE *lpData;
            UINT texPitch;

            if(texture->Map(lpData, texPitch))
            {
                if(pitch == texPitch)
                    SSECopy(lpData, textureBuffers[curTexture], pitch*height);
                else
                {
                    UINT bestPitch = MIN(pitch, texPitch);
                    LPBYTE input = textureBuffers[curTexture];
                    for(UINT y=0; y<height; y++)
                    {
                        LPBYTE curInput  = ((LPBYTE)input)  + (pitch*y);
                        LPBYTE curOutput = ((LPBYTE)lpData) + (texPitch*y);

                        SSECopy(curOutput, curInput, bestPitch);
                    }
                }
                texture->Unmap();
            }
            ReleaseMutex(hMutex);
        }

        hMutex = NULL;
    }

    return texture; 
}

void MemoryCapture::UnlockTexture()
{
}
