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


FARPROC                 oldD3D10Release;
FARPROC                 newD3D10Release;

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
    keepAliveTime = 0;
    resetCount++;

    logOutput << CurrentTimeString() << "---------------------- Cleared D3D10 Capture ----------------------" << endl;
}

void SetupD3D10(IDXGISwapChain *swapChain)
{
    logOutput << CurrentTimeString() << "setting up d3d10 data" << endl;
    ClearD3D10Data();

    DXGI_SWAP_CHAIN_DESC scd;
    if(SUCCEEDED(swapChain->GetDesc(&scd)))
    {
        d3d10CaptureInfo.format = ConvertGIBackBufferFormat(scd.BufferDesc.Format);
        if(d3d10CaptureInfo.format != GS_UNKNOWNFORMAT)
        {
            if( dxgiFormat                   != scd.BufferDesc.Format ||
                d3d10CaptureInfo.cx          != scd.BufferDesc.Width  ||
                d3d10CaptureInfo.cy          != scd.BufferDesc.Height ||
                d3d10CaptureInfo.hwndCapture != (DWORD)scd.OutputWindow)
            {
                dxgiFormat = FixCopyTextureFormat(scd.BufferDesc.Format);
                d3d10CaptureInfo.cx = scd.BufferDesc.Width;
                d3d10CaptureInfo.cy = scd.BufferDesc.Height;
                d3d10CaptureInfo.hwndCapture = (DWORD)scd.OutputWindow;
                bIsMultisampled = scd.SampleDesc.Count > 1;

                logOutput << CurrentTimeString() << "found dxgi format (dx10) of: " << UINT(dxgiFormat) <<
                    ", size: {" << scd.BufferDesc.Width << ", " << scd.BufferDesc.Height <<
                    "}, multisampled: " << (bIsMultisampled ? "true" : "false") << endl;
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
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D10Hook: failed to create intermediary texture, result = " << UINT(hErr) << endl;
        return false;
    }

    if(FAILED(hErr = d3d10Tex->QueryInterface(__uuidof(ID3D10Resource), (void**)&copyD3D10TextureGame)))
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D10Hook: d3d10Tex->QueryInterface(ID3D10Resource) failed, result = " << UINT(hErr) << endl;
        d3d10Tex->Release();
        return false;
    }

    IDXGIResource *res;
    if(FAILED(hErr = d3d10Tex->QueryInterface(IID_IDXGIResource, (void**)&res)))
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D10Hook: d3d10Tex->QueryInterface(IDXGIResource) failed, result = " << UINT(hErr) << endl;
        d3d10Tex->Release();
        return false;
    }

    if(FAILED(res->GetSharedHandle(&sharedHandle)))
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D10Hook: res->GetSharedHandle failed, result = " << UINT(hErr) << endl;
        d3d10Tex->Release();
        res->Release();
        return false;
    }

    d3d10Tex->Release();
    res->Release();

    return true;
}

void DoD3D10Capture(IDXGISwapChain *swap)
{
    HRESULT hRes;

    ID3D10Device *device = NULL;
    if(SUCCEEDED(swap->GetDevice(__uuidof(ID3D10Device), (void**)&device)))
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
            RUNEVERYRESET logOutput << CurrentTimeString() << "stop requested, terminating d3d10 capture" << endl;

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

                    logOutput << CurrentTimeString() << "DoD3D10Hook: success" << endl;
                }
                else
                {
                    ClearD3D10Data();
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
                    ClearD3D10Data();
                    logOutput << CurrentTimeString() << "Keepalive no longer found on d3d10, freeing capture data" << endl;
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

                            ID3D10Resource *backBuffer = NULL;
                            if(SUCCEEDED(hRes = swap->GetBuffer(0, IID_ID3D10Resource, (void**)&backBuffer)))
                            {
                                if(bIsMultisampled)
                                    device->ResolveSubresource(copyD3D10TextureGame, 0, backBuffer, 0, dxgiFormat);
                                else
                                    device->CopyResource(copyD3D10TextureGame, backBuffer);

                                RUNEVERYRESET logOutput << CurrentTimeString() << "successfully capturing d3d10 frames via GPU" << endl;

                                backBuffer->Release();
                            } else {
                                RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D10Capture: swap->GetBuffer failed: result = " << UINT(hRes) << endl;
                            }

                            curCapture = nextCapture;
                        }
                    }
                }
            }
            else {
                RUNEVERYRESET logOutput << CurrentTimeString() << "no longer capturing, terminating d3d10 capture" << endl;
                ClearD3D10Data();
            }
        }
    }

    SafeRelease(device);
}
