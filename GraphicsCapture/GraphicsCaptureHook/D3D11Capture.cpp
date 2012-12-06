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
ID3D10Device1           *shareDevice = NULL;
ID3D11Resource          *copyTextureGame = NULL;
ID3D10Resource          *copyTextureIntermediary = NULL;
HANDLE                  sharedHandles[2] = {NULL, NULL};
IDXGIKeyedMutex         *keyedMutexes[2] = {NULL, NULL};
ID3D10Resource          *sharedTextures[2] = {NULL, NULL};


extern bool bD3D101Hooked;

void ClearD3D11Data()
{
    bHasTextures = false;
    if(texData)
        texData->lastRendered = -1;
    texData = NULL;

    for(UINT i=0; i<2; i++)
    {
        SafeRelease(keyedMutexes[i]);
        SafeRelease(sharedTextures[i]);
        sharedHandles[i] = NULL;
    }

    SafeRelease(copyTextureGame);
    SafeRelease(copyTextureIntermediary);
    SafeRelease(shareDevice);

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
                d3d11CaptureInfo.hwndCapture = scd.OutputWindow;
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

    bD3D101Hooked = true;
    HMODULE hD3D10_1 = LoadLibrary(TEXT("d3d10_1.dll"));
    if(!hD3D10_1)
    {
        RUNONCE logOutput << "DoD3D11Hook: could not load d3d10.1" << endl;
        return false;
    }

    HMODULE hDXGI = GetModuleHandle(TEXT("dxgi.dll"));
    if(!hDXGI)
    {
        RUNONCE logOutput << "DoD3D11Hook: could not load dxgi" << endl;
        return false;
    }

    CREATEDXGIFACTORY1PROC createDXGIFactory1 = (CREATEDXGIFACTORY1PROC)GetProcAddress(hDXGI, "CreateDXGIFactory1");
    if(!createDXGIFactory1)
    {
        RUNONCE logOutput << "DoD3D11Hook: could not get address of CreateDXGIFactory1" << endl;
        return false;
    }

    PFN_D3D10_CREATE_DEVICE1 d3d10CreateDevice1 = (PFN_D3D10_CREATE_DEVICE1)GetProcAddress(hD3D10_1, "D3D10CreateDevice1");
    if(!d3d10CreateDevice1)
    {
        RUNONCE logOutput << "DoD3D11Hook: could not get address of D3D10CreateDevice1" << endl;
        return false;
    }

    IDXGIFactory1 *factory;
    if(FAILED(hErr = (*createDXGIFactory1)(__uuidof(IDXGIFactory1), (void**)&factory)))
    {
        RUNONCE logOutput << "DoD3D11Hook: CreateDXGIFactory1 failed, result = " << UINT(hErr) << endl;
        return false;
    }

    IDXGIAdapter1 *adapter;
    if(FAILED(hErr = factory->EnumAdapters1(0, &adapter)))
    {
        RUNONCE logOutput << "DoD3D11Hook: factory->EnumAdapters1 failed, result = " << UINT(hErr) << endl;
        factory->Release();
        return false;
    }

    if(FAILED(hErr = (*d3d10CreateDevice1)(adapter, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, &shareDevice)))
    {
        if(FAILED(hErr = (*d3d10CreateDevice1)(adapter, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_FEATURE_LEVEL_9_3, D3D10_1_SDK_VERSION, &shareDevice)))
        {
            RUNONCE logOutput << "DoD3D11Hook: device creation failed, result = " << UINT(hErr) << endl;
            adapter->Release();
            factory->Release();
            return false;
        }
    }

    adapter->Release();
    factory->Release();

    //------------------------------------------------

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

    HANDLE handle;
    if(FAILED(hErr = res->GetSharedHandle(&handle)))
    {
        RUNONCE logOutput << "DoD3D11Hook: res->GetSharedHandle failed, result = " << UINT(hErr) << endl;
        d3d11Tex->Release();
        res->Release();
        return false;
    }

    d3d11Tex->Release();
    res->Release();

    //------------------------------------------------

    if(FAILED(hErr = shareDevice->OpenSharedResource(handle, __uuidof(ID3D10Resource), (void**)&copyTextureIntermediary)))
    {
        RUNONCE logOutput << "DoD3D11Hook: shareDevice->OpenSharedResource failed, result = " << UINT(hErr) << endl;
        return false;
    }

    //------------------------------------------------

    D3D10_TEXTURE2D_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(texDesc));
    texDesc.Width               = d3d11CaptureInfo.cx;
    texDesc.Height              = d3d11CaptureInfo.cy;
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
            RUNONCE logOutput << "DoD3D11Hook: shareDevice->CreateTexture2D " << i << " failed, result = " << UINT(hErr) << endl;
            return false;
        }

        if(FAILED(hErr = d3d10tex->QueryInterface(__uuidof(ID3D10Resource), (void**)&sharedTextures[i])))
        {
            RUNONCE logOutput << "DoD3D11Hook: d3d10tex->QueryInterface(ID3D10Resource) " << i << " failed, result = " << UINT(hErr) << endl;
            d3d10tex->Release();
            return false;
        }

        if(FAILED(hErr = d3d10tex->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)&keyedMutexes[i])))
        {
            RUNONCE logOutput << "DoD3D11Hook: d3d10tex->QueryInterface(IDXGIKeyedMutex) " << i << " failed, result = " << UINT(hErr) << endl;
            d3d10tex->Release();
            return false;
        }

        IDXGIResource *res;
        if(FAILED(hErr = d3d10tex->QueryInterface(__uuidof(IDXGIResource), (void**)&res)))
        {
            RUNONCE logOutput << "DoD3D11Hook: d3d10tex->QueryInterface(IDXGIResource) " << i << " failed, result = " << UINT(hErr) << endl;
            d3d10tex->Release();
            return false;
        }

        if(FAILED(hErr = res->GetSharedHandle(&sharedHandles[i])))
        {
            RUNONCE logOutput << "DoD3D11Hook: res->GetSharedHandle " << i << " failed, result = " << UINT(hErr) << endl;
            res->Release();
            d3d10tex->Release();
            return false;
        }

        res->Release();
        d3d10tex->Release();
    }

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
                            bSuccess = IsWindow(hwndReceiver);

                        if(bSuccess)
                        {
                            bHasTextures = true;
                            d3d11CaptureInfo.captureType = CAPTURETYPE_SHAREDTEX;
                            d3d11CaptureInfo.hwndSender = hwndSender;
                            d3d11CaptureInfo.bFlip = FALSE;
                            texData->texHandles[0] = sharedHandles[0];
                            texData->texHandles[1] = sharedHandles[1];
                            PostMessage(hwndReceiver, RECEIVER_NEWCAPTURE, 0, (LPARAM)&d3d11CaptureInfo);

                            logOutput << "DoD3D11Hook: success";
                        }
                        else
                        {
                            ClearD3D11Data();
                        }
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
