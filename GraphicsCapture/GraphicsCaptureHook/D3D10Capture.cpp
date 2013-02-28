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

#include <D3D10_1.h>
#include "DXGIStuff.h"


HookData         gi1swapResizeBuffers;
HookData         gi1swapPresent;
FARPROC          oldD3D10Release;
FARPROC          newD3D10Release;

CaptureInfo             d3d10CaptureInfo;

LPVOID                  lpCurrentSwap;
extern LPVOID           lpCurrentDevice;
extern SharedTexData    *texData;
extern DWORD            curCapture;
extern BOOL             bHasTextures;
BOOL                    bIsMultisampled;
extern LONGLONG         lastTime;

DXGI_FORMAT             dxgiFormat;
ID3D10Resource          *copyD3D10TextureGame = NULL;
extern HANDLE           sharedHandle;



void ClearD3D10Data()
{
    bHasTextures = false;
    texData = NULL;
    sharedHandle = NULL;

    SafeRelease(copyD3D10TextureGame);

    DestroySharedMemory();
}

void SetupD3D10(IDXGISwapChain *swapChain)
{
    ClearD3D10Data();

    DXGI_SWAP_CHAIN_DESC scd;
    if(SUCCEEDED(swapChain->GetDesc(&scd)))
    {
        d3d10CaptureInfo.format = ConvertGIBackBufferFormat(scd.BufferDesc.Format);
        if(d3d10CaptureInfo.format != GS_UNKNOWNFORMAT)
        {
            if( dxgiFormat          != scd.BufferDesc.Format ||
                d3d10CaptureInfo.cx != scd.BufferDesc.Width  ||
                d3d10CaptureInfo.cy != scd.BufferDesc.Height )
            {
                dxgiFormat = scd.BufferDesc.Format;
                d3d10CaptureInfo.cx = scd.BufferDesc.Width;
                d3d10CaptureInfo.cy = scd.BufferDesc.Height;
                d3d10CaptureInfo.hwndCapture = (DWORD)scd.OutputWindow;
                bIsMultisampled = scd.SampleDesc.Count > 1;
            }
        }
    }

    lastTime = 0;
    OSInitializeTimer();
}


typedef HRESULT (WINAPI *CREATEDXGIFACTORY1PROC)(REFIID riid, void **ppFactory);

bool DoD3D10Hook(ID3D10Device *device)
{
    HRESULT hErr;

    D3D10_TEXTURE2D_DESC texGameDesc;
    ZeroMemory(&texGameDesc, sizeof(texGameDesc));
    texGameDesc.Width               = d3d10CaptureInfo.cx;
    texGameDesc.Height              = d3d10CaptureInfo.cy;
    texGameDesc.MipLevels           = 1;
    texGameDesc.ArraySize           = 1;
    texGameDesc.Format              = dxgiFormat;
    texGameDesc.SampleDesc.Count    = 1;
    texGameDesc.BindFlags           = D3D10_BIND_RENDER_TARGET|D3D10_BIND_SHADER_RESOURCE;
    texGameDesc.Usage               = D3D10_USAGE_DEFAULT;
    texGameDesc.MiscFlags           = D3D10_RESOURCE_MISC_SHARED;

    ID3D10Texture2D *d3d10Tex;
    if(FAILED(hErr = device->CreateTexture2D(&texGameDesc, NULL, &d3d10Tex)))
    {
        RUNONCE logOutput << "DoD3D10Hook: failed to create intermediary texture, result = " << UINT(hErr) << endl;
        return false;
    }

    if(FAILED(hErr = d3d10Tex->QueryInterface(__uuidof(ID3D10Resource), (void**)&copyD3D10TextureGame)))
    {
        RUNONCE logOutput << "DoD3D10Hook: d3d10Tex->QueryInterface(ID3D10Resource) failed, result = " << UINT(hErr) << endl;
        d3d10Tex->Release();
        return false;
    }

    IDXGIResource *res;
    if(FAILED(hErr = d3d10Tex->QueryInterface(IID_IDXGIResource, (void**)&res)))
    {
        RUNONCE logOutput << "DoD3D10Hook: d3d10Tex->QueryInterface(IDXGIResource) failed, result = " << UINT(hErr) << endl;
        d3d10Tex->Release();
        return false;
    }

    if(FAILED(res->GetSharedHandle(&sharedHandle)))
    {
        RUNONCE logOutput << "DoD3D10Hook: res->GetSharedHandle failed, result = " << UINT(hErr) << endl;
        d3d10Tex->Release();
        res->Release();
        return false;
    }

    d3d10Tex->Release();
    res->Release();

    return true;
}

UINT STDMETHODCALLTYPE D3D10DeviceReleaseHook(ID3D10Device *device)
{
    device->AddRef();
    ULONG refVal = (*(RELEASEPROC)oldD3D10Release)(device);

    if(bHasTextures)
    {
        if(refVal == 5) //our two textures are holding the reference up, so always clear at 3
        {
            ClearD3D10Data();
            lpCurrentDevice = NULL;
            bTargetAcquired = false;
        }
    }
    else if(refVal == 1)
    {
        lpCurrentDevice = NULL;
        bTargetAcquired = false;
    }

    return (*(RELEASEPROC)oldD3D10Release)(device);
}

HRESULT STDMETHODCALLTYPE D3D10SwapResizeBuffersHook(IDXGISwapChain *swap, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT giFormat, UINT flags)
{
    ClearD3D10Data();
    lpCurrentSwap = NULL;
    lpCurrentDevice = NULL;
    bTargetAcquired = false;

    gi1swapResizeBuffers.Unhook();
    HRESULT hRes = swap->ResizeBuffers(bufferCount, width, height, giFormat, flags);
    gi1swapResizeBuffers.Rehook();

    /*if(lpCurrentSwap == NULL && !bTargetAcquired)
    {
        lpCurrentSwap = swap;
        bTargetAcquired = true;
    }

    if(lpCurrentSwap == swap)
        SetupD3D10(swap);*/

    return hRes;
}

HRESULT STDMETHODCALLTYPE D3D10SwapPresentHook(IDXGISwapChain *swap, UINT syncInterval, UINT flags)
{
    if(lpCurrentSwap == NULL && !bTargetAcquired)
    {
        lpCurrentSwap = swap;
        SetupD3D10(swap);
        bTargetAcquired = true;
    }

    if(lpCurrentSwap == swap)
    {
        ID3D10Device *device = NULL;
        if(SUCCEEDED(swap->GetDevice(__uuidof(ID3D10Device), (void**)&device)))
        {
            if(!lpCurrentDevice)
            {
                lpCurrentDevice = device;

                /*FARPROC oldRelease = GetVTable(device, (8/4));
                if(oldRelease != newD3D10Release)
                {
                    oldD3D10Release = oldRelease;
                    newD3D10Release = ConvertClassProcToFarproc((CLASSPROC)&D3D10Override::DeviceReleaseHook);
                    SetVTable(device, (8/4), newD3D10Release);
                }*/
            }

            if(bCapturing && bStopRequested)
            {
                ClearD3D10Data();
                bCapturing = false;
                bStopRequested = false;
            }

            if(!bCapturing && WaitForSingleObject(hSignalRestart, 0) == WAIT_OBJECT_0)
            {
                hwndOBS = FindWindow(OBS_WINDOW_CLASS, NULL);
                if(hwndOBS)
                    bCapturing = true;
            }

            if(!bHasTextures && bCapturing)
            {
                if(dxgiFormat && hwndOBS)
                {
                    BOOL bSuccess = DoD3D10Hook(device);

                    if(bSuccess)
                    {
                        d3d10CaptureInfo.mapID = InitializeSharedMemoryGPUCapture(&texData);
                        if(!d3d10CaptureInfo.mapID)
                            bSuccess = false;
                    }

                    if(bSuccess)
                    {
                        bHasTextures = true;
                        d3d10CaptureInfo.captureType = CAPTURETYPE_SHAREDTEX;
                        d3d10CaptureInfo.bFlip = FALSE;
                        texData->texHandle = (DWORD)sharedHandle;

                        memcpy(infoMem, &d3d10CaptureInfo, sizeof(CaptureInfo));
                        SetEvent(hSignalReady);

                        logOutput << "DoD3D10Hook: success";
                    }
                    else
                    {
                        ClearD3D10Data();
                    }
                }
            }

            if(bHasTextures)
            {
                LONGLONG frameTime;
                if(bCapturing)
                {
                    if(texData)
                    {
                        if(frameTime = texData->frameTime)
                        {
                            LONGLONG timeVal = OSGetTimeMicroseconds();
                            LONGLONG timeElapsed = timeVal-lastTime;

                            if(timeElapsed >= frameTime)
                            {
                                if(!IsWindow(hwndOBS))
                                {
                                    hwndOBS = NULL;
                                    bStopRequested = true;
                                }

                                if(WaitForSingleObject(hSignalEnd, 0) == WAIT_OBJECT_0)
                                    bStopRequested = true;

                                lastTime += frameTime;
                                if(timeElapsed > frameTime*2)
                                    lastTime = timeVal;

                                DWORD nextCapture = curCapture == 0 ? 1 : 0;

                                ID3D10Resource *backBuffer = NULL;
                                if(SUCCEEDED(swap->GetBuffer(0, IID_ID3D10Resource, (void**)&backBuffer)))
                                {
                                    if(bIsMultisampled)
                                        device->ResolveSubresource(copyD3D10TextureGame, 0, backBuffer, 0, dxgiFormat);
                                    else
                                        device->CopyResource(copyD3D10TextureGame, backBuffer);

                                    backBuffer->Release();
                                }

                                curCapture = nextCapture;
                            }
                        }
                    }
                }
                else
                    ClearD3D10Data();
            }
        }

        device->Release();
    }

    gi1swapPresent.Unhook();
    HRESULT hRes = swap->Present(syncInterval, flags);
    gi1swapPresent.Rehook();

    return hRes;
}

typedef HRESULT (WINAPI*D3D10CREATEPROC)(IDXGIAdapter*, D3D10_DRIVER_TYPE, HMODULE, UINT, UINT, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D10Device**);

bool InitD3D10Capture()
{
    bool bSuccess = false;

    HMODULE hD3D10Dll = GetModuleHandle(TEXT("d3d10.dll"));
    if(hD3D10Dll)
    {
        D3D10CREATEPROC d3d10Create = (D3D10CREATEPROC)GetProcAddress(hD3D10Dll, "D3D10CreateDeviceAndSwapChain");
        if(d3d10Create)
        {
            DXGI_SWAP_CHAIN_DESC swapDesc;
            ZeroMemory(&swapDesc, sizeof(swapDesc));
            swapDesc.BufferCount = 2;
            swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            swapDesc.BufferDesc.Width  = 2;
            swapDesc.BufferDesc.Height = 2;
            swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swapDesc.OutputWindow = hwndSender;
            swapDesc.SampleDesc.Count = 1;
            swapDesc.Windowed = TRUE;

            IDXGISwapChain *swap;
            ID3D10Device *device;

            HRESULT hErr;
            if(SUCCEEDED(hErr = (*d3d10Create)(NULL, D3D10_DRIVER_TYPE_NULL, NULL, 0, D3D10_SDK_VERSION, &swapDesc, &swap, &device)))
            {
                bSuccess = true;

                UPARAM *vtable = *(UPARAM**)swap;
                gi1swapPresent.Hook((FARPROC)*(vtable+(32/4)), (FARPROC)D3D10SwapPresentHook);
                gi1swapResizeBuffers.Hook((FARPROC)*(vtable+(52/4)), (FARPROC)D3D10SwapResizeBuffersHook);

                SafeRelease(swap);
                SafeRelease(device);

                gi1swapPresent.Rehook();
                gi1swapResizeBuffers.Rehook();
            }
            else
            {
                RUNONCE logOutput << "InitD3D10Capture: D3D10CreateDeviceAndSwapChain failed, result = " << UINT(hErr) << endl;
            }
        }
        else
        {
            RUNONCE logOutput << "InitD3D10Capture: could not get address of D3D10CreateDeviceAndSwapChain" << endl;
        }
    }

    return bSuccess;
}

void FreeD3D10Capture()
{
    gi1swapPresent.Unhook();
    gi1swapResizeBuffers.Unhook();
}
