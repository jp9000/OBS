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



void SetupD3D11 (IDXGISwapChain *swap);
void SetupD3D101(IDXGISwapChain *swap);
void SetupD3D10 (IDXGISwapChain *swap);

void ClearD3D11Data();
void ClearD3D101Data();
void ClearD3D10Data();

void DoD3D11Capture (IDXGISwapChain *swap);
void DoD3D101Capture(IDXGISwapChain *swap);
void DoD3D10Capture (IDXGISwapChain *swap);

typedef HRESULT (WINAPI *CREATEDXGIFACTORYPROC)(REFIID riid, void **ppFactory);
typedef void (*DOCAPTUREPROC)(IDXGISwapChain*);
typedef void (*DOCLEARPROC)();

HookData giswapResizeBuffers;
HookData giswapPresent;

DOCAPTUREPROC captureProc = NULL;
DOCLEARPROC   clearProc = NULL;

extern LPVOID lpCurrentSwap;
extern LPVOID lpCurrentDevice;

void SetupDXGIStuff(IDXGISwapChain *swap)
{
    IUnknown *deviceUnk, *device;
    if(SUCCEEDED(swap->GetDevice(__uuidof(IUnknown), (void**)&deviceUnk)))
    {
        if(SUCCEEDED(deviceUnk->QueryInterface(__uuidof(ID3D10Device), (void**)&device)))
        {
            logOutput << "DXGI: Found D3D 10" << endl;
            SetupD3D10(swap);
            captureProc = DoD3D10Capture;
            clearProc   = ClearD3D10Data;
        }
        else if(SUCCEEDED(deviceUnk->QueryInterface(__uuidof(ID3D10Device1), (void**)&device)))
        {
            logOutput << "DXGI: Found D3D 10.1" << endl;
            SetupD3D101(swap);
            captureProc = DoD3D101Capture;
            clearProc   = ClearD3D101Data;
        }
        else if(SUCCEEDED(deviceUnk->QueryInterface(__uuidof(ID3D11Device), (void**)&device)))
        {
            logOutput << "DXGI: Found D3D 11" << endl;
            SetupD3D11(swap);
            captureProc = DoD3D11Capture;
            clearProc   = ClearD3D11Data;
        }
        else
        {
            deviceUnk->Release();
            return;
        }

        device->Release();
        deviceUnk->Release();
    }
    else
        return;

    lpCurrentSwap = swap;
    bTargetAcquired = true;
}

HRESULT STDMETHODCALLTYPE DXGISwapResizeBuffersHook(IDXGISwapChain *swap, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT giFormat, UINT flags)
{
    if(clearProc)
        (*clearProc)();

    clearProc       = NULL;
    captureProc     = NULL;
    lpCurrentSwap   = NULL;
    lpCurrentDevice = NULL;
    bTargetAcquired = false;

    giswapResizeBuffers.Unhook();
    HRESULT hRes = swap->ResizeBuffers(bufferCount, width, height, giFormat, flags);
    giswapResizeBuffers.Rehook();

    return hRes;
}

HRESULT STDMETHODCALLTYPE DXGISwapPresentHook(IDXGISwapChain *swap, UINT syncInterval, UINT flags)
{
    if(lpCurrentSwap == NULL && !bTargetAcquired)
        SetupDXGIStuff(swap);

    if(lpCurrentSwap == swap && captureProc)
        (*captureProc)(swap);

    giswapPresent.Unhook();
    HRESULT hRes = swap->Present(syncInterval, flags);
    giswapPresent.Rehook();

    return hRes;
}

typedef HRESULT (WINAPI* D3D10CREATEDEVICEPROC)(IDXGIAdapter*, D3D10_DRIVER_TYPE, HMODULE, UINT, UINT, IUnknown**);
typedef HRESULT (WINAPI* D3D10CREATEDEVICE1PROC)(IDXGIAdapter *, D3D10_DRIVER_TYPE, HMODULE, UINT, D3D10_FEATURE_LEVEL1, UINT, IUnknown**);
typedef HRESULT (WINAPI* D3D11CREATEDEVICEPROC)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, CONST D3D_FEATURE_LEVEL*, UINT FeatureLevels, UINT, IUnknown**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);


IUnknown* CreateDummyDevice()
{
    IUnknown *device = NULL;

    HRESULT hErr;

    //------------------------------------------------------
    // d3d10

    TCHAR lpDllPath[MAX_PATH];
    SHGetFolderPath(NULL, CSIDL_SYSTEM_DIR, NULL, SHGFP_TYPE_CURRENT, lpDllPath);
    wcscat_s(lpDllPath, MAX_PATH, TEXT("\\d3d10.dll"));

    HMODULE hDll = GetModuleHandle(lpDllPath);
    if(hDll)
    {
        D3D10CREATEDEVICEPROC d3d10Create = (D3D10CREATEDEVICEPROC)GetProcAddress(hDll, "D3D10CreateDevice");
        if(d3d10Create)
        {
            hErr = (*d3d10Create)(NULL, D3D10_DRIVER_TYPE_NULL, NULL, 0, D3D10_SDK_VERSION, &device);
            if(SUCCEEDED(hErr))
                return device;

            RUNONCE logOutput << "CreateDummyDevice: D3D10CreateDevice failed, result = " << UINT(hErr) << endl;
        }
        else
        {
            RUNONCE logOutput << "CreateDummyDevice: D3D10CreateDevice not found" << endl;
        }
    }

    //------------------------------------------------------
    // d3d10.1

    SHGetFolderPath(NULL, CSIDL_SYSTEM_DIR, NULL, SHGFP_TYPE_CURRENT, lpDllPath);
    wcscat_s(lpDllPath, MAX_PATH, TEXT("\\d3d10_1.dll"));

    hDll = GetModuleHandle(lpDllPath);
    if(hDll)
    {
        D3D10CREATEDEVICE1PROC d3d101Create = (D3D10CREATEDEVICE1PROC)GetProcAddress(hDll, "D3D10CreateDevice1");
        if(d3d101Create)
        {
            hErr = (*d3d101Create)(NULL, D3D10_DRIVER_TYPE_NULL, NULL, 0, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, &device);
            if(SUCCEEDED(hErr))
                return device;

            RUNONCE logOutput << "CreateDummyDevice: D3D10CreateDevice1 failed, result = " << UINT(hErr) << endl;
        }
        else
        {
            RUNONCE logOutput << "CreateDummyDevice: D3D10CreateDevice1 not found" << endl;
        }
    }

    //------------------------------------------------------
    // d3d11

    SHGetFolderPath(NULL, CSIDL_SYSTEM_DIR, NULL, SHGFP_TYPE_CURRENT, lpDllPath);
    wcscat_s(lpDllPath, MAX_PATH, TEXT("\\d3d11.dll"));

    hDll = GetModuleHandle(lpDllPath);
    if(hDll)
    {
        D3D11CREATEDEVICEPROC d3d11Create = (D3D11CREATEDEVICEPROC)GetProcAddress(hDll, "D3D11CreateDevice");
        if(d3d11Create)
        {
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

            ID3D11DeviceContext *context;

            hErr = (*d3d11Create)(NULL, D3D_DRIVER_TYPE_NULL, NULL, 0, desiredLevels, 6, D3D11_SDK_VERSION, &device, &receivedLevel, &context);
            if(SUCCEEDED(hErr))
            {
                context->Release();
                return device;
            }

            RUNONCE logOutput << "CreateDummyDevice: D3D11CreateDevice failed, result = " << UINT(hErr) << endl;
        }
        else
        {
            RUNONCE logOutput << "CreateDummyDevice: D3D11CreateDevice not found" << endl;
        }
    }

    return NULL;
}

bool InitDXGICapture()
{
    bool bSuccess = false;

    TCHAR lpDXGIPath[MAX_PATH];
    SHGetFolderPath(NULL, CSIDL_SYSTEM_DIR, NULL, SHGFP_TYPE_CURRENT, lpDXGIPath);
    wcscat_s(lpDXGIPath, MAX_PATH, TEXT("\\dxgi.dll"));

    HMODULE hDXGIDll = GetModuleHandle(lpDXGIPath);
    if(hDXGIDll)
    {
        CREATEDXGIFACTORYPROC dxgiCreateFactory = (CREATEDXGIFACTORYPROC)GetProcAddress(hDXGIDll, "CreateDXGIFactory");
        if(dxgiCreateFactory)
        {
            HRESULT hErr;

            IDXGIFactory *factory;
            if(SUCCEEDED(hErr = (*dxgiCreateFactory)(__uuidof(IDXGIFactory), (void**)&factory)))
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

                IUnknown *device = CreateDummyDevice();
                if(device)
                {
                    IDXGISwapChain *swap;
                    if(SUCCEEDED(hErr = factory->CreateSwapChain(device, &swapDesc, &swap)))
                    {
                        bSuccess = true;

                        UPARAM *vtable = *(UPARAM**)swap;
                        giswapPresent.Hook((FARPROC)*(vtable+(32/4)), (FARPROC)DXGISwapPresentHook);
                        giswapResizeBuffers.Hook((FARPROC)*(vtable+(52/4)), (FARPROC)DXGISwapResizeBuffersHook);

                        SafeRelease(swap);

                        giswapPresent.Rehook();
                        giswapResizeBuffers.Rehook();
                    }
                    else
                    {
                        RUNONCE logOutput << "InitDXGICapture: factory->CreateSwapChain failed, result = " << UINT(hErr) << endl;
                    }

                    device->Release();
                }

                factory->Release();
            }
            else
            {
                RUNONCE logOutput << "InitDXGICapture: could not create IDXGIFactory" << endl;
            }
        }
        else
        {
            RUNONCE logOutput << "InitDXGICapture: could not get address of CreateDXGIFactory" << endl;
        }
    }

    return bSuccess;
}

void FreeDXGICapture()
{
    giswapPresent.Unhook();
    giswapResizeBuffers.Unhook();
}
