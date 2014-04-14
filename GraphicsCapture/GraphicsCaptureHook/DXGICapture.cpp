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
        bool d3d11only = false;

        /* CoD: ghosts hack because apparently on nvidia GPUs it has some d3d10 context open */
        if (_strcmpi(processName, "iw6sp64_ship.exe") == 0 ||
            _strcmpi(processName, "iw6mp64_ship.exe") == 0)
            d3d11only = true;

        if(!d3d11only && SUCCEEDED(deviceUnk->QueryInterface(__uuidof(ID3D10Device), (void**)&device)))
        {
            logOutput << CurrentTimeString() << "DXGI: Found D3D 10" << endl;
            SetupD3D10(swap);
            captureProc = DoD3D10Capture;
            clearProc   = ClearD3D10Data;
        }
        else if(!d3d11only && SUCCEEDED(deviceUnk->QueryInterface(__uuidof(ID3D10Device1), (void**)&device)))
        {
            logOutput << CurrentTimeString() << "DXGI: Found D3D 10.1" << endl;
            SetupD3D101(swap);
            captureProc = DoD3D101Capture;
            clearProc   = ClearD3D101Data;
        }
        else if(SUCCEEDED(deviceUnk->QueryInterface(__uuidof(ID3D11Device), (void**)&device)))
        {
            logOutput << CurrentTimeString() << "DXGI: Found D3D 11" << endl;
            SetupD3D11(swap);
            captureProc = DoD3D11Capture;
            clearProc   = ClearD3D11Data;
        }
        else
        {
            logOutput << CurrentTimeString() << "DXGI: Unknown interface" << endl;
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

typedef HRESULT(STDMETHODCALLTYPE *DXGISwapResizeBuffersHookPROC)(IDXGISwapChain *swap, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT giFormat, UINT flags);

HRESULT STDMETHODCALLTYPE DXGISwapResizeBuffersHook(IDXGISwapChain *swap, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT giFormat, UINT flags)
{
    if(clearProc)
        (*clearProc)();

    clearProc       = NULL;
    captureProc     = NULL;
    lpCurrentSwap   = NULL;
    lpCurrentDevice = NULL;
    bTargetAcquired = false;

#if OLDHOOKS
    giswapResizeBuffers.Unhook();
    HRESULT hRes = swap->ResizeBuffers(bufferCount, width, height, giFormat, flags);
    giswapResizeBuffers.Rehook();
#else
    HRESULT hRes = ((DXGISwapResizeBuffersHookPROC)giswapResizeBuffers.origFunc)(swap, bufferCount, width, height, giFormat, flags);
#endif

    return hRes;
}

typedef HRESULT(STDMETHODCALLTYPE *DXGISwapPresentHookPROC)(IDXGISwapChain *swap, UINT syncInterval, UINT flags);

HRESULT STDMETHODCALLTYPE DXGISwapPresentHook(IDXGISwapChain *swap, UINT syncInterval, UINT flags)
{
    if(lpCurrentSwap == NULL && !bTargetAcquired)
        SetupDXGIStuff(swap);

    if ((flags & DXGI_PRESENT_TEST) == 0 && lpCurrentSwap == swap && captureProc)
        (*captureProc)(swap);

#if OLDHOOKS
    giswapPresent.Unhook();
    HRESULT hRes = swap->Present(syncInterval, flags);
    giswapPresent.Rehook();
#else
    HRESULT hRes = ((DXGISwapPresentHookPROC)giswapPresent.origFunc)(swap, syncInterval, flags);
#endif

    return hRes;
}

typedef HRESULT (WINAPI *D3D10CREATEPROC)(IDXGIAdapter*, D3D10_DRIVER_TYPE, HMODULE, UINT, UINT, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, IUnknown**);
typedef HRESULT (WINAPI *D3D101CREATEPROC)(IDXGIAdapter*, D3D10_DRIVER_TYPE, HMODULE, UINT, D3D10_FEATURE_LEVEL1, UINT, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, IUnknown**);
typedef HRESULT (WINAPI*D3D11CREATEPROC)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, D3D_FEATURE_LEVEL*, UINT, UINT, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, IUnknown**, D3D_FEATURE_LEVEL*, IUnknown**);


IDXGISwapChain* CreateDummySwap()
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

    IDXGISwapChain *swap = NULL;
    IUnknown *device = NULL;

    HRESULT hErr;

    TCHAR lpDllPath[MAX_PATH];
    HMODULE hDll;

    //------------------------------------------------------
    // d3d10

    /* CoD: ghosts hack because apparently on nvidia GPUs it has some d3d10 context open */
    if (_strcmpi(processName, "iw6sp64_ship.exe") == 0 ||
        _strcmpi(processName, "iw6mp64_ship.exe") == 0)
        goto d3d11_only;

    SHGetFolderPath(NULL, CSIDL_SYSTEM, NULL, SHGFP_TYPE_CURRENT, lpDllPath);
    wcscat_s(lpDllPath, MAX_PATH, TEXT("\\d3d10.dll"));

    hDll = GetModuleHandle(lpDllPath);
    if(hDll)
    {
        D3D10CREATEPROC d3d10Create = (D3D10CREATEPROC)GetProcAddress(hDll, "D3D10CreateDeviceAndSwapChain");

        if(d3d10Create)
        {
            hErr = (*d3d10Create)(NULL, D3D10_DRIVER_TYPE_NULL, NULL, 0, D3D10_SDK_VERSION, &swapDesc, &swap, &device);

            if(SUCCEEDED(hErr))
            {
                device->Release();
                return swap;
            }

            RUNEVERYRESET logOutput << CurrentTimeString() << "CreateDummySwap: D3D10CreateDeviceAndSwapChain failed, result = " << UINT(hErr) << endl;
        }
        else
        {
            RUNEVERYRESET logOutput << CurrentTimeString() << "CreateDummySwap: D3D10CreateDeviceAndSwapChain not found" << endl;
        }
    }

    //------------------------------------------------------
    // d3d10.1 -- actually I think 10.1 will always be extended upon 10, so basically this code may never be called

    SHGetFolderPath(NULL, CSIDL_SYSTEM, NULL, SHGFP_TYPE_CURRENT, lpDllPath);
    wcscat_s(lpDllPath, MAX_PATH, TEXT("\\d3d10_1.dll"));

    hDll = GetModuleHandle(lpDllPath);
    if(hDll)
    {
        D3D101CREATEPROC d3d101Create = (D3D101CREATEPROC)GetProcAddress(hDll, "D3D10CreateDeviceAndSwapChain1");
        if(d3d101Create)
        {
            hErr = (*d3d101Create)(NULL, D3D10_DRIVER_TYPE_NULL, NULL, 0, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, &swapDesc, &swap, &device);

            if(SUCCEEDED(hErr))
            {
                device->Release();
                return swap;
            }

            RUNEVERYRESET logOutput << CurrentTimeString() << "CreateDummyDevice: D3D10CreateDeviceAndSwapChain1 failed, result = " << UINT(hErr) << endl;
        }
        else
        {
            RUNEVERYRESET logOutput << CurrentTimeString() << "CreateDummyDevice: D3D10CreateDeviceAndSwapChain1 not found" << endl;
        }
    }

    //------------------------------------------------------
    // d3d11 - check this first always if possible

d3d11_only:

    SHGetFolderPath(NULL, CSIDL_SYSTEM, NULL, SHGFP_TYPE_CURRENT, lpDllPath);
    wcscat_s(lpDllPath, MAX_PATH, TEXT("\\d3d11.dll"));

    hDll = GetModuleHandle(lpDllPath);
    if(hDll)
    {
        D3D11CREATEPROC d3d11Create = (D3D11CREATEPROC)GetProcAddress(hDll, "D3D11CreateDeviceAndSwapChain");
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

            IUnknown *context;
            hErr = (*d3d11Create)(NULL, D3D_DRIVER_TYPE_NULL, NULL, 0, desiredLevels, 6, D3D11_SDK_VERSION, &swapDesc, &swap, &device, &receivedLevel, &context);
            if(SUCCEEDED(hErr))
            {
                context->Release();
                device->Release();
                return swap;
            }

            RUNEVERYRESET logOutput << CurrentTimeString() << "CreateDummyDevice: D3D11CreateDeviceAndSwapChain failed, result = " << UINT(hErr) << endl;
        }
        else
        {
            RUNEVERYRESET logOutput << CurrentTimeString() << "CreateDummyDevice: D3D11CreateDeviceAndSwapChain not found" << endl;
        }
    }

    return NULL;
}

bool InitDXGICapture()
{
    bool bSuccess = false;

    IDXGISwapChain *swap = CreateDummySwap();
    if(swap)
    {
        bSuccess = true;

        UPARAM *vtable = *(UPARAM**)swap;
        giswapPresent.Hook((FARPROC)*(vtable+(32/4)), (FARPROC)DXGISwapPresentHook);
        giswapResizeBuffers.Hook((FARPROC)*(vtable+(52/4)), (FARPROC)DXGISwapResizeBuffersHook);

        SafeRelease(swap);

        giswapPresent.Rehook();
        giswapResizeBuffers.Rehook();
    }

    return bSuccess;
}

void ClearD3D10Data();
void ClearD3D101Data();
void ClearD3D11Data();

void FreeDXGICapture()
{
    giswapPresent.Unhook();
    giswapResizeBuffers.Unhook();

    ClearD3D10Data();
    ClearD3D101Data();
    ClearD3D11Data();
}
