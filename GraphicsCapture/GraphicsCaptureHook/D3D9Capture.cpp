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
HookData            d3d9ResetEx;
FARPROC             oldD3D9Release = NULL;
FARPROC             newD3D9Release = NULL;

LPVOID              lpCurrentDevice = NULL;
DWORD               d3d9Format = 0;
UINT                captureHeight = 0, captureWidth = 0;
IDirect3DSurface9   *textures[2] = {NULL, NULL};

MemoryCopyData      *copyData = NULL;
LPBYTE              textureBuffers[2] = {NULL, NULL};
DWORD               curCapture = 0;
BOOL                bHasTextures = FALSE;

CaptureInfo         d3d9CaptureInfo;


void ClearD3D9Data()
{
    bHasTextures = false;
    for(UINT i=0; i<2; i++)
    {
        SafeRelease(textures[i]);
    }

    DestroySharedMemory();
}

GSColorFormat ConvertDX9BackBufferFormat(D3DFORMAT format)
{
    switch(format)
    {
        case D3DFMT_A2B10G10R10:    return GS_R10G10B10A2;
        case D3DFMT_A8R8G8B8:       return GS_BGR;
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
    //you may be like "wtf are you doing here, hooking the virtual function table?",
    //but it's the only stable method of hooking this function.  do -not- mess with
    //machine code here, just hook the vtable and call the function directly when 
    //needed, this function can be called a zillion times a minute in tons of
    //different threads.  do NOT do anything critical here.
    ULONG STDMETHODCALLTYPE Release()
    {
        IDirect3DDevice9 *device = (IDirect3DDevice9*)this;
        device->AddRef();
        ULONG refVal = (*(RELEASEPROC)oldD3D9Release)(device);

        if(bHasTextures)
        {
            if(refVal == 5) //clearing at 5ish is the safest bet.
            {
                ClearD3D9Data();
                lpCurrentDevice = NULL;
                bTargetAcquired = false;
            }
        }
        else if(refVal == 1)
        {
            lpCurrentDevice = NULL;
            bTargetAcquired = false;
        }

        return (*(RELEASEPROC)oldD3D9Release)(device);
    }

    HRESULT STDMETHODCALLTYPE EndScene()
    {
        d3d9EndScene.Unhook();

        IDirect3DDevice9 *device = (IDirect3DDevice9*)this;

        if(lpCurrentDevice == NULL && !bTargetAcquired)
        {
            lpCurrentDevice = device;
            SetupD3D9(device);
            bTargetAcquired = true;

            oldD3D9Release = GetVTable(device, (8/4));
            newD3D9Release = ConvertClassProcToFarproc((CLASSPROC)&D3D9Override::Release);
            SetVTable(device, (8/4), newD3D9Release);
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
                        UINT pitch;

                        for(UINT i=0; i<2; i++)
                        {
                            HRESULT ching;
                            if(FAILED(ching = device->CreateOffscreenPlainSurface(captureWidth, captureHeight, (D3DFORMAT)d3d9Format, D3DPOOL_SYSTEMMEM, &textures[i], NULL)))
                            {
                                bSuccess = false;
                                break;
                            }

                            if(i == 0)
                            {
                                D3DLOCKED_RECT lr;
                                if(FAILED(textures[i]->LockRect(&lr, NULL, D3DLOCK_READONLY)))
                                {
                                    bSuccess = false;
                                    break;
                                }

                                pitch = lr.Pitch;
                                textures[i]->UnlockRect();
                            }
                        }

                        if(bSuccess)
                        {
                            d3d9CaptureInfo.mapID = InitializeSharedMemory(pitch*captureHeight, &d3d9CaptureInfo.mapSize, &copyData, textureBuffers);
                            if(!d3d9CaptureInfo.mapID)
                                bSuccess = false;
                        }

                        if(bSuccess)
                        {
                            bHasTextures = true;
                            d3d9CaptureInfo.captureType = CAPTURETYPE_MEMORY;
                            d3d9CaptureInfo.cx = captureWidth;
                            d3d9CaptureInfo.cy = captureHeight;
                            d3d9CaptureInfo.pitch = pitch;
                            d3d9CaptureInfo.hwndSender = hwndSender;
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
                            int lastRendered = -1;

                            //under no circumstances do we -ever- allow a stall
                            if(WaitForSingleObject(textureMutexes[curCapture], 0) == WAIT_OBJECT_0)
                                lastRendered = (int)curCapture;
                            else if(WaitForSingleObject(textureMutexes[nextCapture], 0) == WAIT_OBJECT_0)
                                lastRendered = (int)nextCapture;

                            if(lastRendered != -1)
                            {
                                memcpy(textureBuffers[lastRendered], lr.pBits, lr.Pitch*captureHeight);
                                ReleaseMutex(textureMutexes[lastRendered]);
                            }

                            lastTexture->UnlockRect();
                            copyData->lastRendered = (UINT)lastRendered;
                        }
                    }

                    curCapture = nextCapture;
                }
                else
                    ClearD3D9Data();
            }
        }

        HRESULT hRes = device->EndScene();
        d3d9EndScene.Rehook();

        return hRes;
    }

    HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS *params)
    {
        d3d9Reset.Unhook();

        IDirect3DDevice9 *device = (IDirect3DDevice9*)this;

        HRESULT hRes = device->Reset(params);

        if(lpCurrentDevice == NULL && !bTargetAcquired)
        {
            lpCurrentDevice = device;
            bTargetAcquired = true;
        }

        if(lpCurrentDevice == device)
            SetupD3D9(device);

        d3d9Reset.Rehook();

        return hRes;
    }

    HRESULT STDMETHODCALLTYPE ResetEx(D3DPRESENT_PARAMETERS *params, D3DDISPLAYMODEEX *fullscreenData)
    {
        d3d9ResetEx.Unhook();
        d3d9Reset.Unhook();

        IDirect3DDevice9Ex *device = (IDirect3DDevice9Ex*)this;

        HRESULT hRes = device->ResetEx(params, fullscreenData);

        if(lpCurrentDevice == NULL && !bTargetAcquired)
        {
            lpCurrentDevice = device;
            bTargetAcquired = true;
        }

        if(lpCurrentDevice == device)
            SetupD3D9(device);

        d3d9Reset.Rehook();
        d3d9ResetEx.Rehook();

        return hRes;
    }
};

typedef IDirect3D9* (WINAPI*D3D9CREATEPROC)(UINT);
typedef HRESULT (WINAPI*D3D9CREATEEXPROC)(UINT, IDirect3D9Ex**);

bool InitD3D9Capture()
{
    bool bSuccess = false;

    HMODULE hD3D9Dll = GetModuleHandle(TEXT("d3d9.dll"));
    if(hD3D9Dll)
    {
        D3D9CREATEEXPROC d3d9CreateEx = (D3D9CREATEEXPROC)GetProcAddress(hD3D9Dll, "Direct3DCreate9Ex");

        IDirect3D9Ex *d3d9ex;
        if(SUCCEEDED((*d3d9CreateEx)(D3D_SDK_VERSION, &d3d9ex)))
        {
            D3DPRESENT_PARAMETERS pp;
            ZeroMemory(&pp, sizeof(pp));
            pp.Windowed                 = 1;
            pp.SwapEffect               = D3DSWAPEFFECT_FLIP;
            pp.BackBufferFormat         = D3DFMT_A8R8G8B8;
            pp.BackBufferCount          = 1;
            pp.hDeviceWindow            = (HWND)hwndSender;
            pp.PresentationInterval     = D3DPRESENT_INTERVAL_IMMEDIATE;

            HRESULT hRes;

            IDirect3DDevice9Ex *deviceEx;
            if(SUCCEEDED(hRes = d3d9ex->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwndSender, D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_NOWINDOWCHANGES, &pp, NULL, &deviceEx)))
            {
                bSuccess = true;

                UPARAM *vtable = *(UPARAM**)deviceEx;

                d3d9EndScene.Hook((FARPROC)*(vtable+(168/4)), ConvertClassProcToFarproc((CLASSPROC)&D3D9Override::EndScene));
                d3d9Reset.Hook((FARPROC)*(vtable+(64/4)), ConvertClassProcToFarproc((CLASSPROC)&D3D9Override::Reset));
                d3d9ResetEx.Hook((FARPROC)*(vtable+(528/4)), ConvertClassProcToFarproc((CLASSPROC)&D3D9Override::ResetEx));

                deviceEx->Release();

                d3d9EndScene.Rehook();
                d3d9Reset.Rehook();
                d3d9ResetEx.Rehook();
            }

            d3d9ex->Release();
        }
    }

    return bSuccess;
}

void FreeD3D9Capture()
{
    d3d9EndScene.Unhook();
    d3d9Reset.Unhook();
    d3d9ResetEx.Unhook();
}
