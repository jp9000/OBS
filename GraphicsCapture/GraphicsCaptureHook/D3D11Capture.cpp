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
#include <D3D11.h>
#include "DXGIStuff.h"


HookData                gi11swapResizeBuffers;
HookData                gi11swapPresent;
FARPROC                 oldD3D11Release = NULL;
FARPROC                 newD3D11Release = NULL;

CaptureInfo             d3d11CaptureInfo;

extern LPVOID           lpCurrentSwap;
extern LPVOID           lpCurrentDevice;
SharedTexData           *texData;
extern DWORD            curCapture;
extern BOOL             bHasTextures;
extern BOOL             bIsMultisampled;
extern LONGLONG         lastTime;

extern DXGI_FORMAT      dxgiFormat;
ID3D11Resource          *copyTextureGame = NULL;
HANDLE                  sharedHandle = NULL;


extern bool bD3D101Hooked;

void ClearD3D11Data()
{
    bHasTextures = false;
    texData = NULL;
    sharedHandle = NULL;

    SafeRelease(copyTextureGame);

    DestroySharedMemory();
}

void SetupD3D11(IDXGISwapChain *swapChain)
{
    ClearD3D11Data();

    DXGI_SWAP_CHAIN_DESC scd;
    if(SUCCEEDED(swapChain->GetDesc(&scd)))
    {
        d3d11CaptureInfo.format = ConvertGIBackBufferFormat(scd.BufferDesc.Format);
        if(d3d11CaptureInfo.format != GS_UNKNOWNFORMAT)
        {
            if( dxgiFormat          != scd.BufferDesc.Format ||
                d3d11CaptureInfo.cx != scd.BufferDesc.Width  ||
                d3d11CaptureInfo.cy != scd.BufferDesc.Height )
            {
                dxgiFormat = scd.BufferDesc.Format;
                d3d11CaptureInfo.cx = scd.BufferDesc.Width;
                d3d11CaptureInfo.cy = scd.BufferDesc.Height;
                d3d11CaptureInfo.hwndCapture = (DWORD)scd.OutputWindow;
                bIsMultisampled = scd.SampleDesc.Count > 1;
            }
        }
    }

    lastTime = 0;
    OSInitializeTimer();
}

typedef HRESULT (WINAPI *CREATEDXGIFACTORY1PROC)(REFIID riid, void **ppFactory);


bool DoD3D11Hook(ID3D11Device *device)
{
    HRESULT hErr;

    D3D11_TEXTURE2D_DESC texGameDesc;
    ZeroMemory(&texGameDesc, sizeof(texGameDesc));
    texGameDesc.Width               = d3d11CaptureInfo.cx;
    texGameDesc.Height              = d3d11CaptureInfo.cy;
    texGameDesc.MipLevels           = 1;
    texGameDesc.ArraySize           = 1;
    texGameDesc.Format              = dxgiFormat;
    texGameDesc.SampleDesc.Count    = 1;
    texGameDesc.BindFlags           = D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    texGameDesc.Usage               = D3D11_USAGE_DEFAULT;
    texGameDesc.MiscFlags           = D3D11_RESOURCE_MISC_SHARED;

    ID3D11Texture2D *d3d11Tex;
    if(FAILED(hErr = device->CreateTexture2D(&texGameDesc, NULL, &d3d11Tex)))
    {
        RUNONCE logOutput << "DoD3D11Hook: creation of intermediary texture failed, result = " << UINT(hErr) << endl;
        return false;
    }

    if(FAILED(hErr = d3d11Tex->QueryInterface(__uuidof(ID3D11Resource), (void**)&copyTextureGame)))
    {
        RUNONCE logOutput << "DoD3D11Hook: d3d11Tex->QueryInterface(ID3D11Resource) failed, result = " << UINT(hErr) << endl;
        d3d11Tex->Release();
        return false;
    }

    IDXGIResource *res;
    if(FAILED(hErr = d3d11Tex->QueryInterface(IID_IDXGIResource, (void**)&res)))
    {
        RUNONCE logOutput << "DoD3D11Hook: d3d11Tex->QueryInterface(IID_IDXGIResource) failed, result = " << UINT(hErr) << endl;
        d3d11Tex->Release();
        return false;
    }

    if(FAILED(hErr = res->GetSharedHandle(&sharedHandle)))
    {
        RUNONCE logOutput << "DoD3D11Hook: res->GetSharedHandle failed, result = " << UINT(hErr) << endl;
        d3d11Tex->Release();
        res->Release();
        return false;
    }

    d3d11Tex->Release();
    res->Release();

    return true;
}

UINT STDMETHODCALLTYPE D3D11DeviceReleaseHook(ID3D10Device *device)
{
    /*device->AddRef();
    ULONG refVal = (*(RELEASEPROC)oldD3D11Release)(device);*/

    ULONG refVal = (*(RELEASEPROC)oldD3D11Release)(device);

    /*if(bHasTextures)
    {
        if(refVal == 8) //our two textures are holding the reference up, so always clear at 3
        {
            ClearD3D11Data();
            lpCurrentDevice = NULL;
            bTargetAcquired = false;
        }
    }
    else if(refVal == 1)
    {
        lpCurrentDevice = NULL;
        bTargetAcquired = false;
    }*/

    return refVal;
}

HRESULT STDMETHODCALLTYPE D3D11SwapResizeBuffersHook(IDXGISwapChain *swap, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT giFormat, UINT flags)
{
    ClearD3D11Data();
    lpCurrentSwap = NULL;
    lpCurrentDevice = NULL;
    bTargetAcquired = false;

    gi11swapResizeBuffers.Unhook();
    HRESULT hRes = swap->ResizeBuffers(bufferCount, width, height, giFormat, flags);
    gi11swapResizeBuffers.Rehook();

    /*if(lpCurrentSwap == NULL && !bTargetAcquired)
    {
        lpCurrentSwap = swap;
        bTargetAcquired = true;
    }

    if(lpCurrentSwap == swap)
        SetupD3D11(swap);*/

    return hRes;
}

HRESULT STDMETHODCALLTYPE D3D11SwapPresentHook(IDXGISwapChain *swap, UINT syncInterval, UINT flags)
{
    if(lpCurrentSwap == NULL && !bTargetAcquired)
    {
        lpCurrentSwap = swap;
        SetupD3D11(swap);
        bTargetAcquired = true;
    }

    if(lpCurrentSwap == swap)
    {
        ID3D11Device *device = NULL;
        HRESULT chi;
        if(SUCCEEDED(chi = swap->GetDevice(__uuidof(ID3D11Device), (void**)&device)))
        {
            if(!lpCurrentDevice)
            {
                lpCurrentDevice = device;

                /*FARPROC curRelease = GetVTable(device, (8/4));
                if(curRelease != newD3D11Release)
                {
                    oldD3D11Release = curRelease;
                    newD3D11Release = (FARPROC)DeviceReleaseHook;
                    SetVTable(device, (8/4), newD3D11Release);
                }*/
            }

            ID3D11DeviceContext *context;
            device->GetImmediateContext(&context);

            if(bCapturing && bStopRequested)
            {
                ClearD3D11Data();
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
                    BOOL bSuccess = DoD3D11Hook(device);

                    if(bSuccess)
                    {
                        d3d11CaptureInfo.mapID = InitializeSharedMemoryGPUCapture(&texData);
                        if(!d3d11CaptureInfo.mapID)
                        {
                            RUNONCE logOutput << "SwapPresentHook: creation of shared memory failed" << endl;
                            bSuccess = false;
                        }
                    }

                    if(bSuccess)
                    {
                        bHasTextures = true;
                        d3d11CaptureInfo.captureType = CAPTURETYPE_SHAREDTEX;
                        d3d11CaptureInfo.bFlip = FALSE;
                        texData->texHandle = (DWORD)sharedHandle;

                        memcpy(infoMem, &d3d11CaptureInfo, sizeof(CaptureInfo));
                        SetEvent(hSignalReady);

                        logOutput << "DoD3D11Hook: success" << endl;
                    }
                    else
                    {
                        ClearD3D11Data();
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

                                ID3D11Resource *backBuffer = NULL;

                                if(SUCCEEDED(swap->GetBuffer(0, IID_ID3D11Resource, (void**)&backBuffer)))
                                {
                                    if(bIsMultisampled)
                                        context->ResolveSubresource(copyTextureGame, 0, backBuffer, 0, dxgiFormat);
                                    else
                                        context->CopyResource(copyTextureGame, backBuffer);

                                    backBuffer->Release();
                                }

                                curCapture = nextCapture;
                            }
                        }
                    }
                }
                else
                    ClearD3D11Data();
            }

            device->Release();
            context->Release();
        }
    }

    gi11swapPresent.Unhook();
    HRESULT hRes = swap->Present(syncInterval, flags);
    gi11swapPresent.Rehook();

    return hRes;
}

typedef HRESULT (WINAPI*D3D11CREATEPROC)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, D3D_FEATURE_LEVEL*, UINT, UINT, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

bool InitD3D11Capture()
{
    bool bSuccess = false;

    HMODULE hD3D11Dll = GetModuleHandle(TEXT("d3d11.dll"));
    if(hD3D11Dll)
    {
        D3D11CREATEPROC d3d11Create = (D3D11CREATEPROC)GetProcAddress(hD3D11Dll, "D3D11CreateDeviceAndSwapChain");
        if(d3d11Create)
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
            ID3D11Device *device;
            ID3D11DeviceContext *context;

            D3D_FEATURE_LEVEL desiredLevels[6] =
            {
                D3D_FEATURE_LEVEL_11_0,
                D3D_FEATURE_LEVEL_10_1,
                D3D_FEATURE_LEVEL_10_0,
                D3D_FEATURE_LEVEL_9_3,
                D3D_FEATURE_LEVEL_9_2,
                D3D_FEATURE_LEVEL_9_1,
            };
            D3D_FEATURE_LEVEL receivedLevel;

            HRESULT hErr;
            if(SUCCEEDED(hErr = (*d3d11Create)(NULL, D3D_DRIVER_TYPE_NULL, NULL, 0, desiredLevels, 6, D3D11_SDK_VERSION, &swapDesc, &swap, &device, &receivedLevel, &context)))
            {
                bSuccess = true;

                UPARAM *vtable = *(UPARAM**)swap;
                gi11swapPresent.Hook((FARPROC)*(vtable+(32/4)), (FARPROC)D3D11SwapPresentHook);
                gi11swapResizeBuffers.Hook((FARPROC)*(vtable+(52/4)), (FARPROC)D3D11SwapResizeBuffersHook);

                SafeRelease(swap);
                SafeRelease(device);
                SafeRelease(context);

                gi11swapPresent.Rehook();
                gi11swapResizeBuffers.Rehook();
            }
            else
            {
                RUNONCE logOutput << "InitD3D11Capture: D3D11CreateDeviceAndSwapChain definitely failed, result = " << UINT(hErr) << endl;
            }
        }
        else
        {
            RUNONCE logOutput << "InitD3D11Capture: could not get address of D3D11CreateDeviceAndSwapChain" << endl;
        }
    }

    return bSuccess;
}

void FreeD3D11Capture()
{
    gi11swapPresent.Unhook();
    gi11swapResizeBuffers.Unhook();
}
