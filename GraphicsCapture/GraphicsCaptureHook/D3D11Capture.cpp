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

#include <D3D11.h>
#include "DXGIStuff.h"


extern HookData         gi11swapResizeBuffers;
extern HookData         gi11swapPresent;
FARPROC                 oldD3D11Release = NULL;
FARPROC                 newD3D11Release = NULL;

CaptureInfo             d3d11CaptureInfo;

extern LPVOID           lpCurrentSwap;
extern LPVOID           lpCurrentDevice;
extern LPBYTE           textureBuffers[2];
extern MemoryCopyData   *copyData;
extern DWORD            curCapture;
extern BOOL             bHasTextures;
extern BOOL             bIsMultisampled;

extern DXGI_FORMAT      dxgiFormat;
ID3D11Texture2D         *d3d11Textures[2];


void ClearD3D11Data()
{
    bHasTextures = false;
    for(UINT i=0; i<2; i++)
    {
        SafeRelease(d3d11Textures[i]);
    }

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
                bIsMultisampled = scd.SampleDesc.Count > 1;
            }
        }
    }
}

struct D3D11Override
{
    UINT STDMETHODCALLTYPE DeviceReleaseHook()
    {
        ID3D10Device *device = (ID3D10Device*)this;

        device->AddRef();
        ULONG refVal = (*(RELEASEPROC)oldD3D11Release)(device);

        if(bHasTextures)
        {
            if(refVal == 5) //our two textures are holding the reference up, so always clear at 3
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
        }

        return (*(RELEASEPROC)oldD3D11Release)(device);
    }

    HRESULT STDMETHODCALLTYPE SwapResizeBuffersHook(UINT bufferCount, UINT width, UINT height, DXGI_FORMAT giFormat, UINT flags)
    {
        IDXGISwapChain *swap = (IDXGISwapChain*)this;

        gi11swapResizeBuffers.Unhook();
        HRESULT hRes = swap->ResizeBuffers(bufferCount, width, height, giFormat, flags);
        gi11swapResizeBuffers.Rehook();

        if(lpCurrentSwap == NULL && !bTargetAcquired)
        {
            lpCurrentSwap = swap;
            bTargetAcquired = true;
        }

        if(lpCurrentSwap == swap)
            SetupD3D11(swap);

        return hRes;
    }

    HRESULT STDMETHODCALLTYPE SwapPresentHook(UINT syncInterval, UINT flags)
    {
        IDXGISwapChain *swap = (IDXGISwapChain*)this;

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

                    oldD3D11Release = GetVTable(device, (8/4));
                    newD3D11Release = ConvertClassProcToFarproc((CLASSPROC)&D3D11Override::DeviceReleaseHook);
                    SetVTable(device, (8/4), newD3D11Release);
                }

                ID3D11DeviceContext *context;
                device->GetImmediateContext(&context);

                if(!bHasTextures && bCapturing)
                {
                    if(dxgiFormat)
                    {
                        if(!hwndReceiver)
                            hwndReceiver = FindWindow(RECEIVER_WINDOWCLASS, NULL);

                        if(hwndReceiver)
                        {
                            D3D11_TEXTURE2D_DESC texDesc;
                            ZeroMemory(&texDesc, sizeof(texDesc));
                            texDesc.Width  = d3d11CaptureInfo.cx;
                            texDesc.Height = d3d11CaptureInfo.cy;
                            texDesc.MipLevels = 1;
                            texDesc.ArraySize = 1;
                            texDesc.Format = dxgiFormat;
                            texDesc.SampleDesc.Count = 1;
                            texDesc.Usage = D3D11_USAGE_STAGING;
                            texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

                            bool bSuccess = true;
                            UINT pitch;

                            for(UINT i=0; i<2; i++)
                            {
                                HRESULT ching;
                                if(FAILED(ching = device->CreateTexture2D(&texDesc, NULL, &d3d11Textures[i])))
                                {
                                    bSuccess = false;
                                    break;
                                }

                                if(i == 0)
                                {
                                    ID3D11Resource *resource;
                                    if(FAILED(d3d11Textures[i]->QueryInterface(__uuidof(ID3D11Resource), (void**)&resource)))
                                    {
                                        bSuccess = false;
                                        break;
                                    }

                                    D3D11_MAPPED_SUBRESOURCE map;
                                    if(FAILED(context->Map(resource, 0, D3D11_MAP_READ, 0, &map)))
                                    {
                                        bSuccess = false;
                                        break;
                                    }

                                    pitch = map.RowPitch;
                                    context->Unmap(resource, 0);
                                    resource->Release();
                                }
                            }

                            if(bSuccess)
                            {
                                d3d11CaptureInfo.mapID = InitializeSharedMemory(pitch*d3d11CaptureInfo.cy, &d3d11CaptureInfo.mapSize, &copyData, textureBuffers);
                                if(!d3d11CaptureInfo.mapID)
                                    bSuccess = false;
                            }

                            if(bSuccess)
                            {
                                bHasTextures = true;
                                d3d11CaptureInfo.captureType = CAPTURETYPE_MEMORY;
                                d3d11CaptureInfo.hwndSender = hwndSender;
                                d3d11CaptureInfo.pitch = pitch;
                                d3d11CaptureInfo.bFlip = FALSE;
                                PostMessage(hwndReceiver, RECEIVER_NEWCAPTURE, 0, (LPARAM)&d3d11CaptureInfo);
                            }
                            else
                            {
                                for(UINT i=0; i<2; i++)
                                {
                                    SafeRelease(d3d11Textures[i]);

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

                        ID3D11Texture2D *texture = d3d11Textures[curCapture];
                        ID3D11Resource *backBuffer = NULL;

                        if(SUCCEEDED(swap->GetBuffer(0, IID_ID3D11Resource, (void**)&backBuffer)))
                        {
                            if(bIsMultisampled)
                                context->ResolveSubresource(texture, 0, backBuffer, 0, dxgiFormat);
                            else
                                context->CopyResource(texture, backBuffer);
                            backBuffer->Release();

                            ID3D11Texture2D *lastTexture = d3d11Textures[nextCapture];
                            ID3D11Resource *resource;

                            if(SUCCEEDED(lastTexture->QueryInterface(__uuidof(ID3D11Resource), (void**)&resource)))
                            {
                                D3D11_MAPPED_SUBRESOURCE map;
                                if(SUCCEEDED(context->Map(resource, 0, D3D11_MAP_READ, 0, &map)))
                                {
                                    LPBYTE *pTextureBuffer = NULL;
                                    int lastRendered = -1;

                                    //under no circumstances do we -ever- allow a stall
                                    if(WaitForSingleObject(textureMutexes[curCapture], 0) == WAIT_OBJECT_0)
                                        lastRendered = (int)curCapture;
                                    else if(WaitForSingleObject(textureMutexes[nextCapture], 0) == WAIT_OBJECT_0)
                                        lastRendered = (int)nextCapture;

                                    if(lastRendered != -1)
                                    {
                                        SSECopy(textureBuffers[lastRendered], map.pData, map.RowPitch*d3d11CaptureInfo.cy);
                                        ReleaseMutex(textureMutexes[lastRendered]);
                                    }

                                    context->Unmap(resource, 0);
                                    copyData->lastRendered = (UINT)lastRendered;
                                }

                                resource->Release();
                            }
                        }

                        curCapture = nextCapture;
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
};

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
            swapDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            swapDesc.BufferDesc.Width  = 2;
            swapDesc.BufferDesc.Height = 2;
            swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swapDesc.OutputWindow = hwndSender;
            swapDesc.SampleDesc.Count = 1;
            swapDesc.Windowed = TRUE;

            IDXGISwapChain *swap;
            ID3D11Device *device;
            ID3D11DeviceContext *context;

            D3D_FEATURE_LEVEL desiredLevel = D3D_FEATURE_LEVEL_11_0;
            D3D_FEATURE_LEVEL receivedLevel;

            HRESULT hErr;
            if(SUCCEEDED(hErr = (*d3d11Create)(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &desiredLevel, 1, D3D11_SDK_VERSION, &swapDesc, &swap, &device, &receivedLevel, &context)))
            {
                bSuccess = true;

                UPARAM *vtable = *(UPARAM**)swap;
                gi11swapPresent.Hook((FARPROC)*(vtable+(32/4)), ConvertClassProcToFarproc((CLASSPROC)&D3D11Override::SwapPresentHook));
                gi11swapResizeBuffers.Hook((FARPROC)*(vtable+(52/4)), ConvertClassProcToFarproc((CLASSPROC)&D3D11Override::SwapResizeBuffersHook));

                SafeRelease(swap);
                SafeRelease(device);
                SafeRelease(context);

                gi11swapPresent.Rehook();
                gi11swapResizeBuffers.Rehook();
            }
        }
    }

    return bSuccess;
}

void FreeD3D11Capture()
{
    gi11swapPresent.Unhook();
    gi11swapResizeBuffers.Unhook();
}
