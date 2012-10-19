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


#include "GraphicsCaptureHook.h"

#include <d3d9.h>


HookData            d3d9EndScene;
HookData            d3d9Reset;
HookData            d3d9Release;

LPVOID              lpCurrentDevice = NULL;
BOOL                bHasTextures = false;
DWORD               d3d9Format = 0;
UINT                captureHeight = 0, captureWidth = 0;
LPBYTE              textureBuffers[2] = {NULL, NULL};
MemoryCopyData      copyData = {0, {{NULL, 0}, {NULL, 0}}};
DWORD               curCapture = 0;
IDirect3DSurface9   *textures[2] = {NULL, NULL};

CaptureInfo         d3d9CaptureInfo;


void ClearD3D9Data()
{
    bHasTextures = false;
    for(UINT i=0; i<2; i++)
    {
        SafeRelease(textures[i]);

        if(textureBuffers[i])
        {
            if(WaitForSingleObject(textureMutexes[i], INFINITE) == WAIT_OBJECT_0)
            {
                copyData.lockData[i].address = NULL;
                free(textureBuffers[i]);
                textureBuffers[i] = NULL;
                ReleaseMutex(textureMutexes[i]);
            }
        }

        textures[i] = NULL;
    }
}

GSColorFormat ConvertDX9BackBufferFormat(D3DFORMAT format)
{
    switch(format)
    {
        case D3DFMT_A2B10G10R10:    return GS_R10G10B10A2;
        case D3DFMT_A8R8G8B8:       return GS_BGRA;
        case D3DFMT_X8R8G8B8:       return GS_BGR;
        case D3DFMT_A1R5G5B5:       return GS_B5G5R5A1;
        case D3DFMT_R5G6B5:         return GS_B5G6R5;
    }

    return GS_UNKNOWNFORMAT;
}

void SetupD3D9(IDirect3DDevice9 *device)
{
    ClearD3D9Data();

    IDirect3DSwapChain9 *swapChain = NULL;
    if(SUCCEEDED(device->GetSwapChain(0, &swapChain)))
    {
        D3DPRESENT_PARAMETERS pp;
        if(SUCCEEDED(swapChain->GetPresentParameters(&pp)))
        {
            d3d9CaptureInfo.format = ConvertDX9BackBufferFormat(pp.BackBufferFormat);
            if(d3d9CaptureInfo.format != GS_UNKNOWNFORMAT)
            {
                if( d3d9Format    != pp.BackBufferFormat ||
                    captureWidth  != pp.BackBufferWidth  ||
                    captureHeight != pp.BackBufferHeight )
                {
                    d3d9Format = pp.BackBufferFormat;
                    captureWidth = pp.BackBufferWidth;
                    captureHeight = pp.BackBufferHeight;
                }
            }
        }

        swapChain->Release();
    }
}

struct D3D9Override
{
    HRESULT STDMETHODCALLTYPE EndScene()
    {
        IDirect3DDevice9 *device = (IDirect3DDevice9*)this;

        if(lpCurrentDevice == NULL && !bTargetAquired)
        {
            lpCurrentDevice = device;
            SetupD3D9(device);
            bTargetAquired = true;
        }

        if(lpCurrentDevice == device)
        {
            if(!bHasTextures && bCapturing)
            {
                if(d3d9Format)
                {
                    if(!hwndReceiver)
                        hwndReceiver = FindWindow(RECEIVER_WINDOWCLASS, NULL);

                    if(hwndReceiver)
                    {
                        bool bSuccess = true;
                        for(UINT i=0; i<2; i++)
                        {
                            HRESULT ching;
                            if(FAILED(ching = device->CreateOffscreenPlainSurface(captureWidth, captureHeight, (D3DFORMAT)d3d9Format, D3DPOOL_SYSTEMMEM, &textures[i], NULL)))
                            {
                                bSuccess = false;
                                break;
                            }
                        }

                        if(bSuccess)
                        {
                            bHasTextures = true;
                            d3d9CaptureInfo.captureType = CAPTURETYPE_MEMORY;
                            d3d9CaptureInfo.cx = captureWidth;
                            d3d9CaptureInfo.cy = captureHeight;
                            d3d9CaptureInfo.hwndSender = hwndSender;
                            d3d9CaptureInfo.data = &copyData;
                            d3d9CaptureInfo.bFlip = FALSE;
                            PostMessage(hwndReceiver, RECEIVER_NEWCAPTURE, 0, (LPARAM)&d3d9CaptureInfo);
                        }
                        else
                        {
                            for(UINT i=0; i<2; i++)
                            {
                                SafeRelease(textures[i]);

                                if(textureBuffers[i])
                                {
                                    free(textureBuffers[i]);
                                    textureBuffers[i] = NULL;
                                }
                            }
                        }
                    }
                }
            }

            if(bHasTextures)
            {
                if(bCapturing)
                {
                    DWORD nextCapture = curCapture == 0 ? 1 : 0;

                    IDirect3DSurface9 *texture = textures[curCapture];
                    IDirect3DSurface9 *backBuffer = NULL;

                    if(SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
                    {
                        device->GetRenderTargetData(backBuffer, texture);
                        backBuffer->Release();

                        IDirect3DSurface9 *lastTexture = textures[nextCapture];

                        D3DLOCKED_RECT lr;
                        if(SUCCEEDED(lastTexture->LockRect(&lr, NULL, D3DLOCK_READONLY)))
                        {
                            LPBYTE *pTextureBuffer = NULL;
                            int lastRendered = -1;

                            //under no circumstances do we -ever- allow a stall
                            if(WaitForSingleObject(textureMutexes[curCapture], 0) == WAIT_OBJECT_0)
                                lastRendered = (int)curCapture;
                            else if(WaitForSingleObject(textureMutexes[nextCapture], 0) == WAIT_OBJECT_0)
                                lastRendered = (int)nextCapture;

                            LPBYTE outData = NULL;
                            if(lastRendered != -1)
                            {
                                LockData *pLockData = &copyData.lockData[lastRendered];
                                pTextureBuffer = &textureBuffers[lastRendered];

                                if(!*pTextureBuffer)
                                    *pTextureBuffer = (LPBYTE)malloc(lr.Pitch*captureHeight);

                                outData = *pTextureBuffer;
                                memcpy(outData, lr.pBits, lr.Pitch*captureHeight);

                                ReleaseMutex(textureMutexes[lastRendered]);

                                pLockData->address = outData;
                                pLockData->pitch = lr.Pitch;
                            }

                            lastTexture->UnlockRect();
                            copyData.lastRendered = (UINT)lastRendered;
                        }
                    }

                    curCapture = nextCapture;
                }
                else
                    ClearD3D9Data();
            }
        }

        d3d9EndScene.Unhook();
        HRESULT hRes = device->EndScene();
        d3d9EndScene.Rehook();

        return hRes;
    }

    HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS *params)
    {
        IDirect3DDevice9 *device = (IDirect3DDevice9*)this;

        d3d9Reset.Unhook();
        HRESULT hRes = device->Reset(params);
        d3d9Reset.Rehook();

        if(lpCurrentDevice == NULL && !bTargetAquired)
        {
            lpCurrentDevice = device;
            bTargetAquired = true;
        }

        if(lpCurrentDevice == device)
            SetupD3D9(device);

        return hRes;
    }

    ULONG STDMETHODCALLTYPE Release()
    {
        IDirect3DDevice9 *device = (IDirect3DDevice9*)this;

        d3d9Release.Unhook();

        if(device == lpCurrentDevice)
        {
            ULONG refVal;
            device->AddRef();
            refVal = device->Release();

            if(bHasTextures)
            {
                if(refVal == 3) //our two textures are holding the reference up, so always clear at 3
                {
                    ClearD3D9Data();
                    lpCurrentDevice = NULL;
                    bTargetAquired = false;
                }
            }
            else if(refVal == 1)
            {
                lpCurrentDevice = NULL;
                bTargetAquired = false;
            }
        }

        ULONG ref = device->Release();
        d3d9Release.Rehook();

        return ref;
    }
};

bool InitD3D9Capture()
{
    bool bSuccess = false;

    IDirect3D9 *d3d9;
    if(d3d9 = Direct3DCreate9(D3D_SDK_VERSION))
    {
        D3DPRESENT_PARAMETERS pp;
        ZeroMemory(&pp, sizeof(pp));
        pp.BackBufferHeight = pp.BackBufferWidth = 2;

        IDirect3DDevice9 *d3d9Device;
        if(SUCCEEDED(d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &d3d9Device)))
        {
            bSuccess = true;

            UPARAM *vtable = *(UPARAM**)d3d9Device;

            d3d9EndScene.Hook((FARPROC)*(vtable+(168/4)), ConvertClassProcToFarproc((CLASSPROC)&D3D9Override::EndScene));
            d3d9Reset.Hook((FARPROC)*(vtable+(64/4)), ConvertClassProcToFarproc((CLASSPROC)&D3D9Override::Reset));
            d3d9Release.Hook((FARPROC)*(vtable+(8/4)), ConvertClassProcToFarproc((CLASSPROC)&D3D9Override::Release));

            d3d9Release.Unhook();
            d3d9Device->Release();
            d3d9Release.Rehook();
        }
    }

    return true;
}

void FreeD3D9Capture()
{
    d3d9EndScene.Unhook();
    d3d9Release.Unhook();
    d3d9Release.Unhook();
}
