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
extern LONGLONG         frameTime;
extern DWORD            fps;
extern LONGLONG         lastTime;

DXGI_FORMAT             dxgiFormat;
extern ID3D10Device1    *shareDevice;
ID3D10Resource          *copyD3D10TextureGame = NULL;
extern ID3D10Resource   *copyTextureIntermediary;
extern HANDLE           sharedHandles[2];
extern IDXGIKeyedMutex  *keyedMutexes[2];
extern ID3D10Resource   *sharedTextures[2];


void ClearD3D10Data()
{
    bHasTextures = false;
    if(texData)
        texData->lastRendered = -1;

    for(UINT i=0; i<2; i++)
    {
        SafeRelease(keyedMutexes[i]);
        SafeRelease(sharedTextures[i]);
    }

    SafeRelease(copyD3D10TextureGame);
    SafeRelease(copyTextureIntermediary);
    SafeRelease(shareDevice);

    DestroySharedMemory();
    texData = NULL;
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
                d3d10CaptureInfo.hwndCapture = scd.OutputWindow;
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

    HMODULE hD3D10_1 = LoadLibrary(TEXT("d3d10_1.dll"));
    if(!hD3D10_1)
        return false;

    HMODULE hDXGI = GetModuleHandle(TEXT("dxgi.dll"));
    if(!hDXGI)
        return false;

    CREATEDXGIFACTORY1PROC createDXGIFactory1 = (CREATEDXGIFACTORY1PROC)GetProcAddress(hDXGI, "CreateDXGIFactory1");
    if(!createDXGIFactory1)
        return false;

    PFN_D3D10_CREATE_DEVICE1 d3d10CreateDevice1 = (PFN_D3D10_CREATE_DEVICE1)GetProcAddress(hD3D10_1, "D3D10CreateDevice1");
    if(!d3d10CreateDevice1)
        return false;

    IDXGIFactory1 *factory;
    if(FAILED(hErr = (*createDXGIFactory1)(__uuidof(IDXGIFactory1), (void**)&factory)))
    {
        RUNONCE logOutput << "DoD3D10Hook: CreateDXGIFactory1 failed, result = " << UINT(hErr) << endl;
        return false;
    }

    IDXGIAdapter1 *adapter;
    if(FAILED(hErr = factory->EnumAdapters1(0, &adapter)))
    {
        RUNONCE logOutput << "DoD3D10Hook: factory->EnumAdapters1 failed, result = " << UINT(hErr) << endl;
        factory->Release();
        return false;
    }

    if(FAILED(hErr = (*d3d10CreateDevice1)(adapter, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, &shareDevice)))
    {
        if(FAILED(hErr = (*d3d10CreateDevice1)(adapter, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_FEATURE_LEVEL_9_3, D3D10_1_SDK_VERSION, &shareDevice)))
        {
            RUNONCE logOutput << "DoD3D10Hook: failed to create device, result = " << UINT(hErr) << endl;
            adapter->Release();
            factory->Release();
            return false;
        }
    }

    adapter->Release();
    factory->Release();

    //------------------------------------------------

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

    HANDLE handle;
    if(FAILED(res->GetSharedHandle(&handle)))
    {
        RUNONCE logOutput << "DoD3D10Hook: res->GetSharedHandle failed, result = " << UINT(hErr) << endl;
        d3d10Tex->Release();
        res->Release();
        return false;
    }

    d3d10Tex->Release();
    res->Release();

    //------------------------------------------------

    if(FAILED(hErr = shareDevice->OpenSharedResource(handle, __uuidof(ID3D10Resource), (void**)&copyTextureIntermediary)))
    {
        RUNONCE logOutput << "DoD3D10Hook: shareDevice->OpenSharedResource failed, result = " << UINT(hErr) << endl;
        return false;
    }

    //------------------------------------------------

    D3D10_TEXTURE2D_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(texDesc));
    texDesc.Width               = d3d10CaptureInfo.cx;
    texDesc.Height              = d3d10CaptureInfo.cy;
    texDesc.MipLevels           = 1;
    texDesc.ArraySize           = 1;
    texDesc.Format              = dxgiFormat;
    texDesc.SampleDesc.Count    = 1;
    texDesc.BindFlags           = D3D10_BIND_RENDER_TARGET|D3D10_BIND_SHADER_RESOURCE;
    texDesc.Usage               = D3D10_USAGE_DEFAULT;
    texDesc.MiscFlags           = D3D10_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    for(UINT i=0; i<2; i++)
    {
        ID3D10Texture2D *d3d10tex;
        if(FAILED(hErr = shareDevice->CreateTexture2D(&texDesc, NULL, &d3d10tex)))
        {
            RUNONCE logOutput << "DoD3D10Hook: shareDevice->CreateTexture2D " << i << " failed, result = " << UINT(hErr) << endl;
            return false;
        }

        if(FAILED(hErr = d3d10tex->QueryInterface(__uuidof(ID3D10Resource), (void**)&sharedTextures[i])))
        {
            RUNONCE logOutput << "DoD3D10Hook: d3d10tex->QueryInterface(ID3D10Resource) " << i << " failed, result = " << UINT(hErr) << endl;
            d3d10tex->Release();
            return false;
        }

        if(FAILED(hErr = d3d10tex->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)&keyedMutexes[i])))
        {
            RUNONCE logOutput << "DoD3D10Hook: d3d10tex->QueryInterface(IDXGIKeyedMutex) " << i << " failed, result = " << UINT(hErr) << endl;
            d3d10tex->Release();
            return false;
        }

        IDXGIResource *res;
        if(FAILED(hErr = d3d10tex->QueryInterface(__uuidof(IDXGIResource), (void**)&res)))
        {
            RUNONCE logOutput << "DoD3D10Hook: d3d10tex->QueryInterface(IDXGIResource) " << i << " failed, result = " << UINT(hErr) << endl;
            d3d10tex->Release();
            return false;
        }

        if(FAILED(hErr = res->GetSharedHandle(&sharedHandles[i])))
        {
            RUNONCE logOutput << "DoD3D10Hook: res->GetSharedHandle " << i << " failed, result = " << UINT(hErr) << endl;
            res->Release();
            d3d10tex->Release();
            return false;
        }

        res->Release();
        d3d10tex->Release();
    }

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
                bStopRequested = false;
            }

            if(!bHasTextures && bCapturing)
            {
                if(dxgiFormat)
                {
                    if(!hwndReceiver)
                        hwndReceiver = FindWindow(RECEIVER_WINDOWCLASS, NULL);

                    if(hwndReceiver)
                    {
                        BOOL bSuccess = DoD3D10Hook(device);

                        if(bSuccess)
                        {
                            d3d10CaptureInfo.mapID = InitializeSharedMemoryGPUCapture(&texData);
                            if(!d3d10CaptureInfo.mapID)
                                bSuccess = false;
                        }

                        if(bSuccess)
                            bSuccess = IsWindow(hwndReceiver);

                        if(bSuccess)
                        {
                            bHasTextures = true;
                            d3d10CaptureInfo.captureType = CAPTURETYPE_SHAREDTEX;
                            d3d10CaptureInfo.hwndSender = hwndSender;
                            d3d10CaptureInfo.bFlip = FALSE;
                            texData->texHandles[0] = sharedHandles[0];
                            texData->texHandles[1] = sharedHandles[1];
                            fps = (DWORD)SendMessage(hwndReceiver, RECEIVER_NEWCAPTURE, 0, (LPARAM)&d3d10CaptureInfo);
                            frameTime = 1000000/LONGLONG(fps)/2;

                            logOutput << "DoD3D10Hook: success";
                        }
                        else
                        {
                            ClearD3D10Data();
                        }
                    }
                }
            }

            if(bHasTextures)
            {
                if(bCapturing)
                {
                    LONGLONG timeVal = OSGetTimeMicroseconds();
                    LONGLONG timeElapsed = timeVal-lastTime;

                    if(timeElapsed >= frameTime)
                    {
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

                            ID3D10Texture2D *outputTexture = NULL;
                            int lastRendered = -1;

                            if(keyedMutexes[curCapture]->AcquireSync(0, 0) == WAIT_OBJECT_0)
                                lastRendered = (int)curCapture;
                            else if(keyedMutexes[nextCapture]->AcquireSync(0, 0) == WAIT_OBJECT_0)
                                lastRendered = (int)nextCapture;

                            if(lastRendered != -1)
                            {
                                shareDevice->CopyResource(sharedTextures[lastRendered], copyTextureIntermediary);
                                keyedMutexes[lastRendered]->ReleaseSync(0);
                            }

                            texData->lastRendered = lastRendered;
                            backBuffer->Release();
                        }

                        curCapture = nextCapture;
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
