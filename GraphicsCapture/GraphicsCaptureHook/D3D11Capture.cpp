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
    keepAliveTime = 0;
    resetCount++;

    logOutput << CurrentTimeString() << "---------------------- Cleared D3D11 Capture ----------------------" << endl;
}

void SetupD3D11(IDXGISwapChain *swapChain)
{
    logOutput << CurrentTimeString() << "setting up d3d11 data" << endl;
    ClearD3D11Data();

    DXGI_SWAP_CHAIN_DESC scd;
    if(SUCCEEDED(swapChain->GetDesc(&scd)))
    {
        d3d11CaptureInfo.format = ConvertGIBackBufferFormat(scd.BufferDesc.Format);
        if(d3d11CaptureInfo.format != GS_UNKNOWNFORMAT)
        {
            if( dxgiFormat                   != scd.BufferDesc.Format ||
                d3d11CaptureInfo.cx          != scd.BufferDesc.Width  ||
                d3d11CaptureInfo.cy          != scd.BufferDesc.Height ||
                d3d11CaptureInfo.hwndCapture != (DWORD)scd.OutputWindow)
            {
                dxgiFormat = FixCopyTextureFormat(scd.BufferDesc.Format);
                d3d11CaptureInfo.cx = scd.BufferDesc.Width;
                d3d11CaptureInfo.cy = scd.BufferDesc.Height;
                d3d11CaptureInfo.hwndCapture = (DWORD)scd.OutputWindow;
                bIsMultisampled = scd.SampleDesc.Count > 1;

                logOutput << CurrentTimeString() << "found dxgi format (dx11) of: " << UINT(dxgiFormat) <<
                    ", size: {" << scd.BufferDesc.Width << ", " << scd.BufferDesc.Height <<
                    "}, multisampled: " << (bIsMultisampled ? "true" : "false") << endl;
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
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D11Hook: creation of intermediary texture failed, result = " << UINT(hErr) << endl;
        return false;
    }

    if(FAILED(hErr = d3d11Tex->QueryInterface(__uuidof(ID3D11Resource), (void**)&copyTextureGame)))
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D11Hook: d3d11Tex->QueryInterface(ID3D11Resource) failed, result = " << UINT(hErr) << endl;
        d3d11Tex->Release();
        return false;
    }

    IDXGIResource *res;
    if(FAILED(hErr = d3d11Tex->QueryInterface(IID_IDXGIResource, (void**)&res)))
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D11Hook: d3d11Tex->QueryInterface(IID_IDXGIResource) failed, result = " << UINT(hErr) << endl;
        d3d11Tex->Release();
        return false;
    }

    if(FAILED(hErr = res->GetSharedHandle(&sharedHandle)))
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D11Hook: res->GetSharedHandle failed, result = " << UINT(hErr) << endl;
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
    ULONG refVal = (*(RELEASEPROC)oldD3D11Release)(device);

    if(bHasTextures)
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

    ULONG refVal = (*(RELEASEPROC)oldD3D11Release)(device);
    return refVal;
}

void DoD3D11Capture(IDXGISwapChain *swap)
{
    HRESULT hRes;

    ID3D11Device *device = NULL;
    if(SUCCEEDED(hRes = swap->GetDevice(__uuidof(ID3D11Device), (void**)&device)))
    {
        if(bCapturing && WaitForSingleObject(hSignalEnd, 0) == WAIT_OBJECT_0)
            bStopRequested = true;

        if(bCapturing && !IsWindow(hwndOBS))
        {
            hwndOBS = NULL;
            bStopRequested = true;
        }

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
            RUNEVERYRESET logOutput << CurrentTimeString() << "stop requested, terminating d3d11 capture" << endl;

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
                        RUNEVERYRESET logOutput << CurrentTimeString() << "SwapPresentHook: creation of shared memory failed" << endl;
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

                    logOutput << CurrentTimeString() << "DoD3D11Hook: success" << endl;
                }
                else
                {
                    ClearD3D11Data();
                }
            }
        }

        LONGLONG timeVal = OSGetTimeMicroseconds();

        //check keep alive state, dumb but effective
        if(bCapturing)
        {
            if (!keepAliveTime)
                keepAliveTime = timeVal;

            if((timeVal-keepAliveTime) > 5000000)
            {
                HANDLE hKeepAlive = OpenEvent(EVENT_ALL_ACCESS, FALSE, strKeepAlive.c_str());
                if (hKeepAlive) {
                    CloseHandle(hKeepAlive);
                } else {
                    ClearD3D11Data();
                    logOutput << CurrentTimeString() << "Keepalive no longer found on d3d11, freeing capture data" << endl;
                    bCapturing = false;
                }

                keepAliveTime = timeVal;
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
                        LONGLONG timeElapsed = timeVal-lastTime;

                        if(timeElapsed >= frameTime)
                        {
                            lastTime += frameTime;
                            if(timeElapsed > frameTime*2)
                                lastTime = timeVal;

                            DWORD nextCapture = curCapture == 0 ? 1 : 0;

                            ID3D11Resource *backBuffer = NULL;

                            if(SUCCEEDED(hRes = swap->GetBuffer(0, IID_ID3D11Resource, (void**)&backBuffer)))
                            {
                                if(bIsMultisampled)
                                    context->ResolveSubresource(copyTextureGame, 0, backBuffer, 0, dxgiFormat);
                                else
                                    context->CopyResource(copyTextureGame, backBuffer);

                                RUNEVERYRESET logOutput << CurrentTimeString() << "successfully capturing d3d11 frames via GPU" << endl;

                                backBuffer->Release();
                            } else {
                                RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D11Capture: swap->GetBuffer failed: result = " << UINT(hRes) << endl;
                            }

                            curCapture = nextCapture;
                        }
                    }
                }
            } else {
                RUNEVERYRESET logOutput << CurrentTimeString() << "no longer capturing, terminating d3d11 capture" << endl;
                ClearD3D11Data();
            }
        }

        device->Release();
        context->Release();
    }
}
