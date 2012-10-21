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


HookData                gi11swapResizeBuffers;
HookData                gi11swapPresent;
extern FARPROC          oldD3D10Release;
extern FARPROC          newD3D10Release;

CaptureInfo             d3d101CaptureInfo;

extern LPVOID           lpCurrentSwap;
extern LPVOID           lpCurrentDevice;
extern LPBYTE           textureBuffers[2];
extern MemoryCopyData   *copyData;
extern DWORD            curCapture;
extern BOOL             bHasTextures;
extern BOOL             bIsMultisampled;

extern DXGI_FORMAT      dxgiFormat;
extern ID3D10Texture2D  *d3d10Textures[2];


void ClearD3D101Data()
{
    bHasTextures = false;
    for(UINT i=0; i<2; i++)
    {
        SafeRelease(d3d10Textures[i]);
    }

    DestroySharedMemory();
}

void SetupD3D101(IDXGISwapChain *swapChain)
{
    ClearD3D101Data();

    DXGI_SWAP_CHAIN_DESC scd;
    if(SUCCEEDED(swapChain->GetDesc(&scd)))
    {
        d3d101CaptureInfo.format = ConvertGIBackBufferFormat(scd.BufferDesc.Format);
        if(d3d101CaptureInfo.format != GS_UNKNOWNFORMAT)
        {
            if( dxgiFormat          != scd.BufferDesc.Format ||
                d3d101CaptureInfo.cx != scd.BufferDesc.Width  ||
                d3d101CaptureInfo.cy != scd.BufferDesc.Height )
            {
                dxgiFormat = scd.BufferDesc.Format;
                d3d101CaptureInfo.cx = scd.BufferDesc.Width;
                d3d101CaptureInfo.cy = scd.BufferDesc.Height;
                bIsMultisampled = scd.SampleDesc.Count > 1;
            }
        }
    }
}

struct D3D101Override
{
    UINT STDMETHODCALLTYPE DeviceReleaseHook()
    {
        ID3D10Device *device = (ID3D10Device*)this;

        device->AddRef();
        ULONG refVal = (*(RELEASEPROC)oldD3D10Release)(device);

        if(bHasTextures)
        {
            if(refVal == 5) //our two textures are holding the reference up, so always clear at 3
            {
                ClearD3D101Data();
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
            SetupD3D101(swap);

        return hRes;
    }

    HRESULT STDMETHODCALLTYPE SwapPresentHook(UINT syncInterval, UINT flags)
    {
        IDXGISwapChain *swap = (IDXGISwapChain*)this;

        if(lpCurrentSwap == NULL && !bTargetAcquired)
        {
            lpCurrentSwap = swap;
            SetupD3D101(swap);
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

                    oldD3D10Release = GetVTable(device, (8/4));
                    newD3D10Release = ConvertClassProcToFarproc((CLASSPROC)&D3D101Override::DeviceReleaseHook);
                    SetVTable(device, (8/4), newD3D10Release);
                }

                if(!bHasTextures && bCapturing)
                {
                    if(dxgiFormat)
                    {
                        if(!hwndReceiver)
                            hwndReceiver = FindWindow(RECEIVER_WINDOWCLASS, NULL);

                        if(hwndReceiver)
                        {
                            D3D10_TEXTURE2D_DESC texDesc;
                            ZeroMemory(&texDesc, sizeof(texDesc));
                            texDesc.Width  = d3d101CaptureInfo.cx;
                            texDesc.Height = d3d101CaptureInfo.cy;
                            texDesc.MipLevels = 1;
                            texDesc.ArraySize = 1;
                            texDesc.Format = dxgiFormat;
                            texDesc.SampleDesc.Count = 1;
                            texDesc.Usage = D3D10_USAGE_STAGING;
                            texDesc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;

                            bool bSuccess = true;
                            UINT pitch;

                            for(UINT i=0; i<2; i++)
                            {
                                HRESULT ching;
                                if(FAILED(ching = device->CreateTexture2D(&texDesc, NULL, &d3d10Textures[i])))
                                {
                                    bSuccess = false;
                                    break;
                                }

                                if(i == 0)
                                {
                                    D3D10_MAPPED_TEXTURE2D map;
                                    if(FAILED(d3d10Textures[i]->Map(0, D3D10_MAP_READ, 0, &map)))
                                    {
                                        bSuccess = false;
                                        break;
                                    }

                                    pitch = map.RowPitch;
                                    d3d10Textures[i]->Unmap(0);
                                }
                            }

                            if(bSuccess)
                            {
                                d3d101CaptureInfo.mapID = InitializeSharedMemory(pitch*d3d101CaptureInfo.cy, &d3d101CaptureInfo.mapSize, &copyData, textureBuffers);
                                if(!d3d101CaptureInfo.mapID)
                                    bSuccess = false;
                            }

                            if(bSuccess)
                            {
                                bHasTextures = true;
                                d3d101CaptureInfo.captureType = CAPTURETYPE_MEMORY;
                                d3d101CaptureInfo.hwndSender = hwndSender;
                                d3d101CaptureInfo.pitch = pitch;
                                d3d101CaptureInfo.bFlip = FALSE;
                                PostMessage(hwndReceiver, RECEIVER_NEWCAPTURE, 0, (LPARAM)&d3d101CaptureInfo);
                            }
                            else
                            {
                                for(UINT i=0; i<2; i++)
                                {
                                    SafeRelease(d3d10Textures[i]);

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

                        ID3D10Texture2D *texture = d3d10Textures[curCapture];
                        ID3D10Resource *backBuffer = NULL;

                        if(SUCCEEDED(swap->GetBuffer(0, IID_ID3D10Resource, (void**)&backBuffer)))
                        {
                            if(bIsMultisampled)
                                device->ResolveSubresource(texture, 0, backBuffer, 0, dxgiFormat);
                            else
                                device->CopyResource(texture, backBuffer);
                            backBuffer->Release();

                            ID3D10Texture2D *lastTexture = d3d10Textures[nextCapture];

                            D3D10_MAPPED_TEXTURE2D map;
                            if(SUCCEEDED(lastTexture->Map(0, D3D10_MAP_READ, 0, &map)))
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
                                    SSECopy(textureBuffers[lastRendered], map.pData, map.RowPitch*d3d101CaptureInfo.cy);
                                    ReleaseMutex(textureMutexes[lastRendered]);
                                }

                                lastTexture->Unmap(0);
                                copyData->lastRendered = (UINT)lastRendered;
                            }
                        }

                        curCapture = nextCapture;
                    }
                    else
                        ClearD3D101Data();
                }
            }

            device->Release();
        }

        gi11swapPresent.Unhook();
        HRESULT hRes = swap->Present(syncInterval, flags);
        gi11swapPresent.Rehook();

        return hRes;
    }
};

typedef HRESULT (WINAPI*D3D101CREATEPROC)(IDXGIAdapter*, D3D10_DRIVER_TYPE, HMODULE, UINT, D3D10_FEATURE_LEVEL1, UINT, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D10Device1**);

bool InitD3D101Capture()
{
    bool bSuccess = false;

    HMODULE hD3D101Dll = GetModuleHandle(TEXT("d3d10_1.dll"));
    if(hD3D101Dll)
    {
        D3D101CREATEPROC d3d101Create = (D3D101CREATEPROC)GetProcAddress(hD3D101Dll, "D3D10CreateDeviceAndSwapChain1");
        if(d3d101Create)
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
            ID3D10Device1 *device;

            HRESULT hErr;
            if(SUCCEEDED(hErr = (*d3d101Create)(NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, &swapDesc, &swap, &device)))
            {
                bSuccess = true;

                UPARAM *vtable = *(UPARAM**)swap;
                gi11swapPresent.Hook((FARPROC)*(vtable+(32/4)), ConvertClassProcToFarproc((CLASSPROC)&D3D101Override::SwapPresentHook));
                gi11swapResizeBuffers.Hook((FARPROC)*(vtable+(52/4)), ConvertClassProcToFarproc((CLASSPROC)&D3D101Override::SwapResizeBuffersHook));

                SafeRelease(swap);
                SafeRelease(device);

                gi11swapPresent.Rehook();
                gi11swapResizeBuffers.Rehook();
            }
        }
    }

    return bSuccess;
}

void FreeD3D101Capture()
{
    gi11swapPresent.Unhook();
    gi11swapResizeBuffers.Unhook();
}
