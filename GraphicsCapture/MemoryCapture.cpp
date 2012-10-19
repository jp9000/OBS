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
    if(lpTextureBuffer)
        Free(lpTextureBuffer);

    delete texture;
}

bool MemoryCapture::Init(HANDLE hProcess, HWND hwndTarget, CaptureInfo &info)
{
    this->hwndTarget = hwndTarget;
    this->hProcess = hProcess;
    this->height = info.cy;
    lpDataAddress = info.data;

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

    MemoryCopyData copyData;
    if(ReadProcessMemory(hProcess, lpDataAddress, &copyData, sizeof(copyData), NULL))
    {
        curTexture = copyData.lastRendered;

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
                address = copyData.lockData[curTexture].address;
                DWORD targetPitch = copyData.lockData[curTexture].pitch;
                if(address)
                {
                    LPBYTE lpData;
                    UINT pitch;
                    if(texture->Map(lpData, pitch))
                    {
                        if(pitch == targetPitch)
                            ReadProcessMemory(hProcess, address, lpData, pitch*height, NULL);
                        else
                        {
                            if(!lpTextureBuffer)
                                lpTextureBuffer = (LPBYTE)Allocate(targetPitch*height);

                            ReadProcessMemory(hProcess, address, lpTextureBuffer, targetPitch*height, NULL);

                            for(UINT y=0; y<height; y++)
                            {
                                LPBYTE curInput  = ((LPBYTE)lpTextureBuffer) + (targetPitch*y);
                                LPBYTE curOutput = ((LPBYTE)lpData)          + (pitch*y);

                                SSECopy(curOutput, curInput, MIN(targetPitch, pitch));
                            }
                        }

                        texture->Unmap();
                    }
                }
                else
                    nop();

                ReleaseMutex(hMutex);
            }

            hMutex = NULL;
        }
    }
    else
        nop();

    return texture; 
}

void MemoryCapture::UnlockTexture()
{
}
