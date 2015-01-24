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

#include <d3d9.h>
#include <D3D10_1.h>

typedef HRESULT (WINAPI *PRESENTPROC)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
typedef HRESULT (WINAPI *PRESENTEXPROC)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD);
typedef HRESULT (WINAPI *SWAPPRESENTPROC)(IDirect3DSwapChain9*, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD);

HookData                d3d9EndScene;
HookData                d3d9Reset;
HookData                d3d9ResetEx;
HookData                d3d9Present;
HookData                d3d9PresentEx;
HookData                d3d9SwapPresent;
FARPROC                 oldD3D9Release = NULL;
FARPROC                 newD3D9Release = NULL;

extern CRITICAL_SECTION d3d9EndMutex;

LPVOID                  lpCurrentDevice = NULL;
DWORD                   d3d9Format = 0;

CaptureInfo             d3d9CaptureInfo;

//-----------------------------------------------------------------
// CPU copy stuff (f*in ridiculous what you have to do to get it cpu copy working as smoothly as humanly possible)

#define                 NUM_BUFFERS 3
#define                 ZERO_ARRAY {0, 0, 0}

HANDLE                  hCopyThread = NULL;
HANDLE                  hCopyEvent = NULL;
bool                    bKillThread = false;
HANDLE                  dataMutexes[NUM_BUFFERS] = ZERO_ARRAY;
void                    *pCopyData = NULL;
DWORD                   curCPUTexture = 0;

bool                    lockedTextures[NUM_BUFFERS] = ZERO_ARRAY;
bool                    issuedQueries[NUM_BUFFERS] = ZERO_ARRAY;
MemoryCopyData          *copyData = NULL;
LPBYTE                  textureBuffers[2] = {NULL, NULL};
DWORD                   curCapture = 0;
BOOL                    bHasTextures = FALSE;
LONGLONG                lastTime = 0;
DWORD                   copyWait = 0;

IDirect3DQuery9         *queries[NUM_BUFFERS] = ZERO_ARRAY;
IDirect3DSurface9       *textures[NUM_BUFFERS] = ZERO_ARRAY;
IDirect3DSurface9       *copyD3D9Textures[NUM_BUFFERS] = ZERO_ARRAY;

//-----------------------------------------------------------------
// GPU copy stuff (on top of being perfect and amazing, is also easy)

BOOL                    bD3D9Ex = FALSE;
BOOL                    bUseSharedTextures = FALSE;
IDirect3DSurface9       *copyD3D9TextureGame = NULL;
extern SharedTexData    *texData;
extern DXGI_FORMAT      dxgiFormat;
ID3D10Device1           *shareDevice = NULL;
ID3D10Resource          *copyTextureIntermediary = NULL;
extern HANDLE           sharedHandle;

HMODULE                 hD3D9Dll = NULL;

int                     patchType = 0;

extern bool             bDXGIHooked;



bool CompareMemory(const LPVOID lpVal1, const LPVOID lpVal2, UINT nBytes)
{
    __try
    {
        return memcmp(lpVal1, lpVal2, nBytes) == 0;
    }
    __except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
    {
        return false;
    }

    return false;
}

struct PatchInfo {
    size_t patchSize;
    const BYTE *patchData;
};

#define NewPatch(x) {sizeof(x), (x)}

#ifdef _WIN64

#define NUM_KNOWN_PATCHES 10
#define PATCH_COMPARE_SIZE 13
UPARAM patch_offsets[NUM_KNOWN_PATCHES] = {/*0x4B55F,*/ 0x54FE6, 0x55095, 0x550C5, 0x8BDB5, 0x8E635, 0x90352, 0x9038A, 0x93AFA, 0x93B8A, 0x1841E5};
BYTE patch_compare[NUM_KNOWN_PATCHES][PATCH_COMPARE_SIZE] =
{
    //{0x48, 0x8b, 0x81, 0xc8, 0x38, 0x00, 0x00, 0x39, 0x98, 0x68, 0x50, 0x00, 0x00},  //winvis - 6.0.6002.18005
    {0x48, 0x8b, 0x81, 0xb8, 0x3d, 0x00, 0x00, 0x39, 0x98, 0x68, 0x50, 0x00, 0x00},  //win7   - 6.1.7600.16385
    {0x48, 0x8b, 0x81, 0xb8, 0x3d, 0x00, 0x00, 0x39, 0x98, 0x68, 0x50, 0x00, 0x00},  //win7   - 6.1.7601.16562
    {0x48, 0x8b, 0x81, 0xb8, 0x3d, 0x00, 0x00, 0x39, 0x98, 0x68, 0x50, 0x00, 0x00},  //win7   - 6.1.7601.17514
    {0x48, 0x8b, 0x81, 0xb8, 0x3d, 0x00, 0x00, 0x39, 0xB0, 0x28, 0x51, 0x00, 0x00},  //win8.1 - 6.3.9431.00000
    {0x48, 0x8b, 0x81, 0xb8, 0x3d, 0x00, 0x00, 0x39, 0xA8, 0x28, 0x51, 0x00, 0x00},  //win8.1 - 6.3.9600.17415
    {0x8b, 0x81, 0xb8, 0x3d, 0x00, 0x00, 0x44, 0x39, 0xA0, 0x28, 0x51, 0x00, 0x00},  //win8.1 - 6.3.9600.17085
    {0x8b, 0x81, 0xb8, 0x3d, 0x00, 0x00, 0x44, 0x39, 0xA0, 0x28, 0x51, 0x00, 0x00},  //win8.1 - 6.3.9600.17095
    {0x8b, 0x81, 0xb8, 0x3d, 0x00, 0x00, 0x44, 0x39, 0xA0, 0x28, 0x51, 0x00, 0x00},  //win8.1 - 6.3.9600.16384
    {0x8b, 0x81, 0xb8, 0x3d, 0x00, 0x00, 0x44, 0x39, 0xA0, 0x28, 0x51, 0x00, 0x00},  //win8.1 - 6.3.9600.16404
    {0x49, 0x8b, 0x85, 0xb8, 0x3d, 0x00, 0x00, 0x39, 0x88, 0xc8, 0x50, 0x00, 0x00},  //win8   - 6.2.9200.16384
};

static const BYTE forceJump[] = {0xEB};
static const BYTE ignoreJump[] = {0x90, 0x90};

PatchInfo patch[NUM_KNOWN_PATCHES] =
{
    //{0xEB, 0x12},
    NewPatch(forceJump),
    NewPatch(forceJump),
    NewPatch(forceJump),
    NewPatch(ignoreJump),
    NewPatch(ignoreJump),
    NewPatch(ignoreJump),
    NewPatch(ignoreJump),
    NewPatch(ignoreJump),
    NewPatch(ignoreJump),
    NewPatch(ignoreJump),
};

#else

#define NUM_KNOWN_PATCHES 10
#define PATCH_COMPARE_SIZE 12
UPARAM patch_offsets[NUM_KNOWN_PATCHES] = {/*0x4BDA1,*/ 0x79AA6, 0x79C9E, 0x79D96, 0x7F9BD, 0x8A3F4, 0x8E9F7, 0x8F00F, 0x8FBB1, 0x90264, 0x166A08};
BYTE patch_compare[NUM_KNOWN_PATCHES][PATCH_COMPARE_SIZE] =
{
    //{0x8b, 0x89, 0x6c, 0x27, 0x00, 0x00, 0x39, 0xb9, 0x80, 0x4b, 0x00, 0x00},  //winvis - 6.0.6002.18005
    {0x8b, 0x89, 0xe8, 0x29, 0x00, 0x00, 0x39, 0xb9, 0x80, 0x4b, 0x00, 0x00},  //win7   - 6.1.7601.16562
    {0x8b, 0x89, 0xe8, 0x29, 0x00, 0x00, 0x39, 0xb9, 0x80, 0x4b, 0x00, 0x00},  //win7   - 6.1.7600.16385
    {0x8b, 0x89, 0xe8, 0x29, 0x00, 0x00, 0x39, 0xb9, 0x80, 0x4b, 0x00, 0x00},  //win7   - 6.1.7601.17514
    {0x8b, 0x80, 0xe8, 0x29, 0x00, 0x00, 0x39, 0xb0, 0x40, 0x4c, 0x00, 0x00},  //win8.1 - 6.3.9431.00000
    {0x80, 0xe8, 0x29, 0x00, 0x00, 0x83, 0xb8, 0x40, 0x4c, 0x00, 0x00, 0x00},  //win8.1 - 6.3.9600.16404
    {0x80, 0xe8, 0x29, 0x00, 0x00, 0x83, 0xb8, 0x40, 0x4c, 0x00, 0x00, 0x00},  //win8.1 - 6.3.9600.17095
    {0x80, 0xe8, 0x29, 0x00, 0x00, 0x83, 0xb8, 0x40, 0x4c, 0x00, 0x00, 0x00},  //win8.1 - 6.3.9600.17085
    {0x80, 0xe8, 0x29, 0x00, 0x00, 0x83, 0xb8, 0x40, 0x4c, 0x00, 0x00, 0x00},  //win8.1 - 6.3.9600.16384
    {0x87, 0xe8, 0x29, 0x00, 0x00, 0x83, 0xb8, 0x40, 0x4c, 0x00, 0x00, 0x00},  //win8.1 - 6.3.9600.17415
    {0x8b, 0x80, 0xe8, 0x29, 0x00, 0x00, 0x39, 0x90, 0xb0, 0x4b, 0x00, 0x00},  //win8   - 6.2.9200.16384
};

static const BYTE forceJump[] = {0xEB};
static const BYTE ignoreJump[] = {0x90, 0x90};

PatchInfo patch[NUM_KNOWN_PATCHES] =
{
    //{0xEB, 0x02},
    NewPatch(forceJump),
    NewPatch(forceJump),
    NewPatch(forceJump),
    NewPatch(forceJump),
    NewPatch(forceJump),
    NewPatch(forceJump),
    NewPatch(forceJump),
    NewPatch(forceJump),
    NewPatch(ignoreJump),
    NewPatch(forceJump),
};

#endif


int GetD3D9PatchType()
{
    LPBYTE lpBaseAddress = (LPBYTE)hD3D9Dll;
    for(int i=0; i<NUM_KNOWN_PATCHES; i++)
    {
        if(CompareMemory(lpBaseAddress+patch_offsets[i], patch_compare[i], PATCH_COMPARE_SIZE))
            return i+1;
    }

    return 0;
}

LPBYTE GetD3D9PatchAddress()
{
    if(patchType)
    {
        LPBYTE lpBaseAddress = (LPBYTE)hD3D9Dll;
        return lpBaseAddress+patch_offsets[patchType-1]+PATCH_COMPARE_SIZE;
    }

    return NULL;
}


void ClearD3D9Data()
{
    bHasTextures = false;
    if(copyData)
        copyData->lastRendered = -1;

    if(hCopyThread)
    {
        bKillThread = true;
        SetEvent(hCopyEvent);
        if(WaitForSingleObject(hCopyThread, 500) != WAIT_OBJECT_0)
            TerminateThread(hCopyThread, -1);

        CloseHandle(hCopyThread);
        CloseHandle(hCopyEvent);

        hCopyThread = NULL;
        hCopyEvent = NULL;
    }

    for(int i=0; i<NUM_BUFFERS; i++)
    {
        if(lockedTextures[i])
        {
            OSEnterMutex(dataMutexes[i]);

            textures[i]->UnlockRect();
            lockedTextures[i] = false;

            OSLeaveMutex(dataMutexes[i]);
        }

        issuedQueries[i] = false;
        SafeRelease(textures[i]);
        SafeRelease(copyD3D9Textures[i]);
        SafeRelease(queries[i]);
    }

    for(int i=0; i<NUM_BUFFERS; i++)
    {
        if(dataMutexes[i])
        {
            OSCloseMutex(dataMutexes[i]);
            dataMutexes[i] = NULL;
        }
    }

    SafeRelease(copyD3D9TextureGame);
    SafeRelease(copyTextureIntermediary);
    SafeRelease(shareDevice);

    DestroySharedMemory();
    texData = NULL;
    copyData = NULL;
    copyWait = 0;
    lastTime = 0;
    curCapture = 0;
    curCPUTexture = 0;
    keepAliveTime = 0;
    resetCount++;
    pCopyData = NULL;

    logOutput << CurrentTimeString() << "---------------------- Cleared D3D9 Capture ----------------------" << endl;
}

GSColorFormat ConvertDX9BackBufferFormat(D3DFORMAT format)
{
    switch(format)
    {
    case D3DFMT_A2B10G10R10:    return GS_R10G10B10A2;
    case D3DFMT_A8R8G8B8:       return GS_BGRA;
    case D3DFMT_X8R8G8B8:       return GS_BGR;
    case D3DFMT_A1R5G5B5:       return GS_B5G5R5A1;
    case D3DFMT_R5G6B5:         return GS_B5G6R5;
    }

    return GS_UNKNOWNFORMAT;
}

DXGI_FORMAT GetDXGIFormat(D3DFORMAT format)
{
    switch(format)
    {
    case D3DFMT_A2B10G10R10:    return DXGI_FORMAT_R10G10B10A2_UNORM;
    case D3DFMT_A8R8G8B8:       return DXGI_FORMAT_B8G8R8A8_UNORM;
    case D3DFMT_X8R8G8B8:       return DXGI_FORMAT_B8G8R8X8_UNORM;
    case D3DFMT_A1R5G5B5:       return DXGI_FORMAT_B5G5R5A1_UNORM;
    case D3DFMT_R5G6B5:         return DXGI_FORMAT_B5G6R5_UNORM;
    }

    return DXGI_FORMAT_UNKNOWN;
}

void SetupD3D9(IDirect3DDevice9 *device);

typedef HRESULT (WINAPI *CREATEDXGIFACTORY1PROC)(REFIID riid, void **ppFactory);


void DoD3D9GPUHook(IDirect3DDevice9 *device)
{
    BYTE *savedData = nullptr;
    BOOL bSuccess = false;

    bDXGIHooked = true;

    HRESULT hErr;

    HMODULE hD3D10_1 = LoadLibrary(TEXT("d3d10_1.dll"));
    if(!hD3D10_1)
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9GPUHook: Could not load D3D10.1" << endl;
        goto finishGPUHook;
    }

    HMODULE hDXGI = LoadLibrary(TEXT("dxgi.dll"));
    if(!hDXGI)
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9GPUHook: Could not load dxgi" << endl;
        goto finishGPUHook;
    }

    CREATEDXGIFACTORY1PROC createDXGIFactory1 = (CREATEDXGIFACTORY1PROC)GetProcAddress(hDXGI, "CreateDXGIFactory1");
    if(!createDXGIFactory1)
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9GPUHook: Could not load 'CreateDXGIFactory1'" << endl;
        goto finishGPUHook;
    }

    PFN_D3D10_CREATE_DEVICE1 d3d10CreateDevice1 = (PFN_D3D10_CREATE_DEVICE1)GetProcAddress(hD3D10_1, "D3D10CreateDevice1");
    if(!d3d10CreateDevice1)
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9GPUHook: Could not load 'D3D10CreateDevice1'" << endl;
        goto finishGPUHook;
    }

    IDXGIFactory1 *factory;
    if(FAILED(hErr = (*createDXGIFactory1)(__uuidof(IDXGIFactory1), (void**)&factory)))
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9GPUHook: CreateDXGIFactory1 failed, result = " << (UINT)hErr << endl;
        goto finishGPUHook;
    }

    IDXGIAdapter1 *adapter;
    if(FAILED(hErr = factory->EnumAdapters1(0, &adapter)))
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9GPUHook: factory->EnumAdapters1 failed, result = " << (UINT)hErr << endl;
        factory->Release();
        goto finishGPUHook;
    }

    if(FAILED(hErr = (*d3d10CreateDevice1)(adapter, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, &shareDevice)))
    {
        if(FAILED(hErr = (*d3d10CreateDevice1)(adapter, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_FEATURE_LEVEL_9_3, D3D10_1_SDK_VERSION, &shareDevice)))
        {
            RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9GPUHook: Could not create D3D10.1 device, result = " << (UINT)hErr << endl;
            adapter->Release();
            factory->Release();
            goto finishGPUHook;
        }
    }

    adapter->Release();
    factory->Release();

    //------------------------------------------------

    D3D10_TEXTURE2D_DESC texGameDesc;
    ZeroMemory(&texGameDesc, sizeof(texGameDesc));
    texGameDesc.Width               = d3d9CaptureInfo.cx;
    texGameDesc.Height              = d3d9CaptureInfo.cy;
    texGameDesc.MipLevels           = 1;
    texGameDesc.ArraySize           = 1;
    texGameDesc.Format              = dxgiFormat;
    texGameDesc.SampleDesc.Count    = 1;
    texGameDesc.BindFlags           = D3D10_BIND_RENDER_TARGET|D3D10_BIND_SHADER_RESOURCE;
    texGameDesc.Usage               = D3D10_USAGE_DEFAULT;
    texGameDesc.MiscFlags           = D3D10_RESOURCE_MISC_SHARED;

    ID3D10Texture2D *d3d101Tex;
    if(FAILED(hErr = shareDevice->CreateTexture2D(&texGameDesc, NULL, &d3d101Tex)))
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9GPUHook: shareDevice->CreateTexture2D failed, result = " << (UINT)hErr << endl;
        goto finishGPUHook;
    }

    if(FAILED(hErr = d3d101Tex->QueryInterface(__uuidof(ID3D10Resource), (void**)&copyTextureIntermediary)))
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9GPUHook: d3d101Tex->QueryInterface(ID3D10Resource) failed, result = " << (UINT)hErr << endl;
        d3d101Tex->Release();
        goto finishGPUHook;
    }

    IDXGIResource *res;
    if(FAILED(hErr = d3d101Tex->QueryInterface(IID_IDXGIResource, (void**)&res)))
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9GPUHook: d3d101Tex->QueryInterface(IDXGIResource) failed, result = " << (UINT)hErr << endl;
        d3d101Tex->Release();
        goto finishGPUHook;
    }

    if(FAILED(res->GetSharedHandle(&sharedHandle)))
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9GPUHook: res->GetSharedHandle failed, result = " << (UINT)hErr << endl;
        d3d101Tex->Release();
        res->Release();
        goto finishGPUHook;
    }

    d3d101Tex->Release();
    res->Release();
    res = NULL;

    //------------------------------------------------

    LPBYTE patchAddress = (patchType != 0) ? GetD3D9PatchAddress() : NULL;
    DWORD dwOldProtect;
    size_t patch_size;

    if(patchAddress)
    {
        patch_size = patch[patchType-1].patchSize;
        savedData = (BYTE*)malloc(patch_size);
        if(VirtualProtect(patchAddress, patch_size, PAGE_EXECUTE_READWRITE, &dwOldProtect))
        {
            memcpy(savedData, patchAddress, patch_size);
            memcpy(patchAddress, patch[patchType-1].patchData, patch_size);
        }
        else
        {
            RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9GPUHook: unable to change memory protection, result = " << GetLastError() << endl;
            goto finishGPUHook;
        }
    }

    IDirect3DTexture9 *d3d9Tex;
    if(FAILED(hErr = device->CreateTexture(d3d9CaptureInfo.cx, d3d9CaptureInfo.cy, 1, D3DUSAGE_RENDERTARGET, (D3DFORMAT)d3d9Format, D3DPOOL_DEFAULT, &d3d9Tex, &sharedHandle)))
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9GPUHook: opening intermediary texture failed, result = " << (UINT)hErr << endl;
        goto finishGPUHook;
    }

    if(patchAddress)
    {
        memcpy(patchAddress, savedData, patch_size);
        VirtualProtect(patchAddress, patch_size, dwOldProtect, &dwOldProtect);
    }

    if(FAILED(hErr = d3d9Tex->GetSurfaceLevel(0, &copyD3D9TextureGame)))
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9GPUHook: d3d9Tex->GetSurfaceLevel failed, result = " << (UINT)hErr << endl;
        d3d9Tex->Release();
        goto finishGPUHook;
    }

    d3d9Tex->Release();

    d3d9CaptureInfo.mapID = InitializeSharedMemoryGPUCapture(&texData);
    if(!d3d9CaptureInfo.mapID)
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9GPUHook: failed to initialize shared memory" << endl;
        goto finishGPUHook;
    }

    bSuccess = IsWindow(hwndOBS);

finishGPUHook:

    if (savedData)
        free(savedData);

    if(bSuccess)
    {
        bHasTextures = true;
        d3d9CaptureInfo.captureType = CAPTURETYPE_SHAREDTEX;
        d3d9CaptureInfo.bFlip = FALSE;
        texData->texHandle = (DWORD)sharedHandle;

        memcpy(infoMem, &d3d9CaptureInfo, sizeof(CaptureInfo));
        if (!SetEvent(hSignalReady))
            logOutput << CurrentTimeString() << "SetEvent(hSignalReady) failed, GetLastError = " << UINT(GetLastError()) << endl;

        logOutput << CurrentTimeString() << "DoD3D9GPUHook: success";

        if (bD3D9Ex)
            logOutput << " - d3d9ex";

        logOutput << endl;
    }
    else
        ClearD3D9Data();
}

DWORD CopyD3D9CPUTextureThread(LPVOID lpUseless);

void DoD3D9CPUHook(IDirect3DDevice9 *device)
{
    BOOL bSuccess = true;
    HRESULT hErr;
    UINT pitch;

    //------------------------------------------------

    if (bSuccess)
    {
        for(UINT i=0; i<NUM_BUFFERS; i++)
        {
            if(FAILED(hErr = device->CreateOffscreenPlainSurface(d3d9CaptureInfo.cx, d3d9CaptureInfo.cy, (D3DFORMAT)d3d9Format, D3DPOOL_SYSTEMMEM, &textures[i], NULL)))
            {
                RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9CPUHook: device->CreateOffscreenPlainSurface " << i << " failed, result = " << (UINT)hErr << endl;
                bSuccess = false;
                break;
            }

            if(i == (NUM_BUFFERS-1))
            {
                D3DLOCKED_RECT lr;
                if(FAILED(hErr = textures[i]->LockRect(&lr, NULL, D3DLOCK_READONLY)))
                {
                    RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9CPUHook: textures[" << i << "]->LockRect failed, result = " << (UINT)hErr << endl;
                    bSuccess = false;
                    break;
                }

                pitch = lr.Pitch;
                textures[i]->UnlockRect();
            }
        }
    }

    //------------------------------------------------

    if (bSuccess)
    {
        for(UINT i=0; i<NUM_BUFFERS; i++)
        {
            if(FAILED(hErr = device->CreateRenderTarget(d3d9CaptureInfo.cx, d3d9CaptureInfo.cy, (D3DFORMAT)d3d9Format, D3DMULTISAMPLE_NONE, 0, FALSE, &copyD3D9Textures[i], NULL)))
            {
                RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9CPUHook: device->CreateTexture " << i << " failed, result = " << (UINT)hErr << endl;
                bSuccess = false;
                break;
            }

            if(FAILED(hErr = device->CreateQuery(D3DQUERYTYPE_EVENT, &queries[i])))
            {
                RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9CPUHook: device->CreateQuery " << i << " failed, result = " << (UINT)hErr << endl;
                bSuccess = false;
                break;
            }

            if(!(dataMutexes[i] = OSCreateMutex()))
            {
                RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9CPUHook: OSCreateMutex " << i << " failed, GetLastError = " << GetLastError() << endl;
                bSuccess = false;
                break;
            }
        }
    }

    if(bSuccess)
    {
        bKillThread = false;

        if(hCopyThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CopyD3D9CPUTextureThread, NULL, 0, NULL))
        {
            if(!(hCopyEvent = CreateEvent(NULL, FALSE, FALSE, NULL)))
            {
                RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9CPUHook: CreateEvent failed, GetLastError = " << GetLastError() << endl;
                bSuccess = false;
            }
        }
        else
        {
            RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9CPUHook: CreateThread failed, GetLastError = " << GetLastError() << endl;
            bSuccess = false;
        }
    }

    if(bSuccess)
    {
        d3d9CaptureInfo.mapID = InitializeSharedMemoryCPUCapture(pitch*d3d9CaptureInfo.cy, &d3d9CaptureInfo.mapSize, &copyData, textureBuffers);
        if(!d3d9CaptureInfo.mapID)
        {
            RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9CPUHook: failed to initialize shared memory" << endl;
            bSuccess = false;
        }
    }

    if(bSuccess)
        bSuccess = IsWindow(hwndOBS);

    if(bSuccess)
    {
        bHasTextures = true;
        d3d9CaptureInfo.captureType = CAPTURETYPE_MEMORY;
        d3d9CaptureInfo.pitch = pitch;
        d3d9CaptureInfo.bFlip = FALSE;

        memcpy(infoMem, &d3d9CaptureInfo, sizeof(CaptureInfo));
        SetEvent(hSignalReady);

        logOutput << CurrentTimeString() << "DoD3D9CPUHook: success" << endl;
    }
    else
        ClearD3D9Data();
}

DWORD CopyD3D9CPUTextureThread(LPVOID lpUseless)
{
    int sharedMemID = 0;

    HANDLE hEvent = NULL;
    if(!DuplicateHandle(GetCurrentProcess(), hCopyEvent, GetCurrentProcess(), &hEvent, NULL, FALSE, DUPLICATE_SAME_ACCESS))
    {
        logOutput << CurrentTimeString() << "CopyD3D9CPUTextureThread: couldn't duplicate handle" << endl;
        return 0;
    }

    while(WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0)
    {
        if(bKillThread)
            break;

        int nextSharedMemID = sharedMemID == 0 ? 1 : 0;

        DWORD copyTex = curCPUTexture;
        LPVOID data = pCopyData;
        if(copyTex < NUM_BUFFERS && data != NULL)
        {
            OSEnterMutex(dataMutexes[copyTex]);

            int lastRendered = -1;

            //copy to whichever is available
            if(WaitForSingleObject(textureMutexes[sharedMemID], 0) == WAIT_OBJECT_0)
                lastRendered = (int)sharedMemID;
            else if(WaitForSingleObject(textureMutexes[nextSharedMemID], 0) == WAIT_OBJECT_0)
                lastRendered = (int)nextSharedMemID;

            if(lastRendered != -1)
            {
                memcpy(textureBuffers[lastRendered], data, d3d9CaptureInfo.pitch*d3d9CaptureInfo.cy);
                ReleaseMutex(textureMutexes[lastRendered]);
                copyData->lastRendered = (UINT)lastRendered;
            }

            OSLeaveMutex(dataMutexes[copyTex]);
        }

        sharedMemID = nextSharedMemID;
    }

    CloseHandle(hEvent);
    return 0;
}

void LogD3D9SurfaceInfo(IDirect3DSurface9 *surf);

void DoD3D9DrawStuff(IDirect3DDevice9 *device)
{
    HRESULT hErr;

    if(bCapturing && WaitForSingleObject(hSignalEnd, 0) == WAIT_OBJECT_0)
        bStopRequested = true;

    if(bCapturing && !IsWindow(hwndOBS))
    {
        hwndOBS = NULL;
        bStopRequested = true;
    }

    if(bStopRequested)
    {
        ClearD3D9Data();
        bCapturing = false;
        bStopRequested = false;
    }

    if(!bCapturing && WaitForSingleObject(hSignalRestart, 0) == WAIT_OBJECT_0)
    {
        hwndOBS = FindWindow(OBS_WINDOW_CLASS, NULL);
        if(hwndOBS) {
            logOutput << CurrentTimeString() << "received restart event, capturing" << endl;
            bCapturing = true;
        } else {
            logOutput << CurrentTimeString() << "received restart event, but couldn't find window" << endl;
        }
    }

    if(!bHasTextures && bCapturing)
    {
        if(d3d9Format && hwndOBS)
        {
            if(bD3D9Ex)
                bUseSharedTextures = true;
            else
                bUseSharedTextures = (patchType = GetD3D9PatchType()) != 0;

            //fix for when backbuffers aren't actually being properly used, instead get the
            //size/format of the actual current render target at time of present
            IDirect3DSurface9 *backBuffer = NULL;
            if (SUCCEEDED(hErr = device->GetRenderTarget(0, &backBuffer))) {
                D3DSURFACE_DESC sd;
                ZeroMemory(&sd, sizeof(sd));

                if (SUCCEEDED(backBuffer->GetDesc(&sd))) {
                    d3d9Format = sd.Format;
                    d3d9CaptureInfo.format = ConvertDX9BackBufferFormat(sd.Format);
                    dxgiFormat = GetDXGIFormat(sd.Format);

                    d3d9CaptureInfo.cx = sd.Width;
                    d3d9CaptureInfo.cy = sd.Height;
                }

                backBuffer->Release();
            }

            if(bUseSharedTextures)
                DoD3D9GPUHook(device);
            else
                DoD3D9CPUHook(device);
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
                logOutput << CurrentTimeString() << "Keepalive no longer found on d3d9, freeing capture data" << endl;
                ClearD3D9Data();
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
            if(bUseSharedTextures) //shared texture support
            {
                if(texData)
                {
                    if(frameTime = texData->frameTime)
                    {
                        LONGLONG timeElapsed = timeVal-lastTime;

                        if(timeElapsed >= frameTime)
                        {
                            if(!IsWindow(hwndOBS))
                            {
                                hwndOBS = NULL;
                                bStopRequested = true;
                            }

                            if(WaitForSingleObject(hSignalEnd, 0) == WAIT_OBJECT_0)
                            {
                                bStopRequested = true;
                            }

                            lastTime += frameTime;
                            if(timeElapsed > frameTime*2)
                                lastTime = timeVal;

                            DWORD nextCapture = curCapture == 0 ? 1 : 0;

                            IDirect3DSurface9 *texture = textures[curCapture];
                            IDirect3DSurface9 *backBuffer = NULL;

                            static bool isTypingOfTheDead = false;
                            static bool checkedExceptions = false;
			    
                            if (!checkedExceptions) {
                                if (_strcmpi(processName,"HOTD_NG.exe") == 0)
                                    isTypingOfTheDead = true;
                            }

                            if (!isTypingOfTheDead && FAILED(hErr = device->GetRenderTarget(0, &backBuffer))) {
                                RUNEVERYRESET logOutput << CurrentTimeString() << "D3D9DrawStuff: GetRenderTarget failed, result = " << unsigned int(hErr) << endl;
                            }

                            if (!backBuffer) {
                                if (FAILED(hErr = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer))) {
                                    RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9DrawStuff: device->GetBackBuffer failed, result = " << unsigned int(hErr) << endl;
                                }
                            }

                            if (backBuffer) {
                                {RUNEVERYRESET LogD3D9SurfaceInfo(backBuffer);}

                                if (FAILED(hErr = device->StretchRect(backBuffer, NULL, copyD3D9TextureGame, NULL, D3DTEXF_NONE))) {
                                    RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9DrawStuff: device->StretchRect failed, result = " << unsigned int(hErr) << endl;
                                } else {
                                    RUNEVERYRESET logOutput << CurrentTimeString() << "successfully capturing d3d9 frames via GPU" << endl;
                                }

                                backBuffer->Release();
                            }

                            curCapture = nextCapture;
                        }
                    }
                }
            }
            else if(copyData)//slow regular d3d9, no shared textures
            {
                if(frameTime = copyData->frameTime)
                {
                    //copy texture data only when GetRenderTargetData completes
                    for(UINT i=0; i<NUM_BUFFERS; i++)
                    {
                        if(issuedQueries[i])
                        {
                            if(queries[i]->GetData(0, 0, 0) == S_OK)
                            {
                                issuedQueries[i] = false;

                                IDirect3DSurface9 *targetTexture = textures[i];
                                D3DLOCKED_RECT lockedRect;
                                if(SUCCEEDED(targetTexture->LockRect(&lockedRect, NULL, D3DLOCK_READONLY)))
                                {
                                    pCopyData = lockedRect.pBits;
                                    curCPUTexture = i;
                                    lockedTextures[i] = true;

                                    SetEvent(hCopyEvent);
                                }
                            }
                        }
                    }

                    //--------------------------------------------------------
                    // copy from backbuffer to GPU texture first to prevent locks, then call GetRenderTargetData when safe

                    LONGLONG timeElapsed = timeVal-lastTime;

                    if(timeElapsed >= frameTime)
                    {
                        lastTime += frameTime;
                        if(timeElapsed > frameTime*2)
                            lastTime = timeVal;

                        DWORD nextCapture = (curCapture == NUM_BUFFERS-1) ? 0 : (curCapture+1);

                        IDirect3DSurface9 *sourceTexture = copyD3D9Textures[curCapture];
                        IDirect3DSurface9 *backBuffer = NULL;

                        if(SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
                        {
                            device->StretchRect(backBuffer, NULL, sourceTexture, NULL, D3DTEXF_NONE);
                            backBuffer->Release();

                            if(copyWait < (NUM_BUFFERS-1))
                                copyWait++;
                            else
                            {
                                IDirect3DSurface9 *prevSourceTexture = copyD3D9Textures[nextCapture];
                                IDirect3DSurface9 *targetTexture = textures[nextCapture];

                                if(lockedTextures[nextCapture])
                                {
                                    OSEnterMutex(dataMutexes[nextCapture]);

                                    targetTexture->UnlockRect();
                                    lockedTextures[nextCapture] = false;

                                    OSLeaveMutex(dataMutexes[nextCapture]);
                                }

                                if(FAILED(hErr = device->GetRenderTargetData(prevSourceTexture, targetTexture)))
                                {
                                    int test = 0;
                                } else {
                                    RUNEVERYRESET logOutput << CurrentTimeString() << "successfully capturing d3d9 frames via CPU" << endl;
                                }

                                queries[nextCapture]->Issue(D3DISSUE_END);
                                issuedQueries[nextCapture] = true;
                            }
                        }

                        curCapture = nextCapture;
                    }
                }
            }
        } else {
            RUNEVERYRESET logOutput << CurrentTimeString() << "no longer capturing, terminating d3d9 capture" << endl;
            ClearD3D9Data();
        }
    }
}


static int presentRecurse = 0;

ULONG STDMETHODCALLTYPE D3D9Release(IDirect3DDevice9 *device)
{
    device->AddRef();
    ULONG refVal = (*(RELEASEPROC)oldD3D9Release)(device);

    if(bHasTextures)
    {
        if(refVal == 15)
        {
            logOutput << CurrentTimeString() << "d3d9 capture terminated by the application" << endl;

            ClearD3D9Data();
            lpCurrentDevice = NULL;
            bTargetAcquired = false;
        }
    }
    else if(refVal == 1)
    {
        lpCurrentDevice = NULL;
        bTargetAcquired = false;
    }

    return (*(RELEASEPROC)oldD3D9Release)(device);
}

typedef HRESULT(STDMETHODCALLTYPE *D3D9EndScenePROC)(IDirect3DDevice9 *device);

HRESULT STDMETHODCALLTYPE D3D9EndScene(IDirect3DDevice9 *device)
{
    EnterCriticalSection(&d3d9EndMutex);

#if OLDHOOKS
    d3d9EndScene.Unhook();
#endif

    RUNEVERYRESET logOutput << CurrentTimeString() << "D3D9EndScene called" << endl;

    if(lpCurrentDevice == NULL)
    {
        IDirect3D9 *d3d;
        if(SUCCEEDED(device->GetDirect3D(&d3d)))
        {
            IDirect3D9 *d3d9ex;
            if(bD3D9Ex = SUCCEEDED(d3d->QueryInterface(__uuidof(IDirect3D9Ex), (void**)&d3d9ex)))
                d3d9ex->Release();
            d3d->Release();
        }

        if(!bTargetAcquired)
        {
            lpCurrentDevice = device;
            SetupD3D9(device);
            bTargetAcquired = true;
        }
    }

#if OLDHOOKS
    HRESULT hRes = device->EndScene();
    d3d9EndScene.Rehook();
#else
    HRESULT hRes = ((D3D9EndScenePROC)d3d9EndScene.origFunc)(device);
#endif

    LeaveCriticalSection(&d3d9EndMutex);

    return hRes;
}

HRESULT STDMETHODCALLTYPE D3D9Present(IDirect3DDevice9 *device, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
{
#if OLDHOOKS
    d3d9Present.Unhook();
#endif

    RUNEVERYRESET logOutput << CurrentTimeString() << "D3D9Present called" << endl;

    if(!presentRecurse)
        DoD3D9DrawStuff(device);

    presentRecurse++;

#if OLDHOOKS
    HRESULT hRes = device->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
#else
    HRESULT hRes = ((PRESENTPROC)d3d9Present.origFunc)(device, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
#endif

    presentRecurse--;
#if OLDHOOKS
    d3d9Present.Rehook();
#endif

    return hRes;
}

HRESULT STDMETHODCALLTYPE D3D9PresentEx(IDirect3DDevice9Ex *device, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion, DWORD dwFlags)
{
#if OLDHOOKS
    d3d9PresentEx.Unhook();
#endif

    RUNEVERYRESET logOutput << CurrentTimeString() << "D3D9PresentEx called" << endl;

    if(!presentRecurse)
        DoD3D9DrawStuff(device);

    presentRecurse++;

#if OLDHOOKS
    HRESULT hRes = device->PresentEx(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
#else
    HRESULT hRes = ((PRESENTEXPROC)d3d9PresentEx.origFunc)(device, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
#endif

    presentRecurse--;
#if OLDHOOKS
    d3d9PresentEx.Rehook();
#endif

    return hRes;
}

HRESULT STDMETHODCALLTYPE D3D9SwapPresent(IDirect3DSwapChain9 *swap, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion, DWORD dwFlags)
{
#if OLDHOOKS
    d3d9SwapPresent.Unhook();
#endif

    RUNEVERYRESET logOutput << CurrentTimeString() << "D3D9SwapPresent called" << endl;

    if(!presentRecurse)
        DoD3D9DrawStuff((IDirect3DDevice9*)lpCurrentDevice);

    presentRecurse++;

#if OLDHOOKS
    HRESULT hRes = swap->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
#else
    HRESULT hRes = ((SWAPPRESENTPROC)d3d9SwapPresent.origFunc)(swap, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
#endif

    presentRecurse--;
#if OLDHOOKS
    d3d9SwapPresent.Rehook();
#endif

    return hRes;
}

typedef HRESULT(STDMETHODCALLTYPE *D3D9ResetPROC)(IDirect3DDevice9 *device, D3DPRESENT_PARAMETERS *params);

HRESULT STDMETHODCALLTYPE D3D9Reset(IDirect3DDevice9 *device, D3DPRESENT_PARAMETERS *params)
{
#if OLDHOOKS
    d3d9Reset.Unhook();
#endif

    RUNEVERYRESET logOutput << CurrentTimeString() << "D3D9Reset called" << endl;

    ClearD3D9Data();

#if OLDHOOKS
    HRESULT hRes = device->Reset(params);
#else
    HRESULT hRes = ((D3D9ResetPROC)d3d9Reset.origFunc)(device, params);
#endif

    if(lpCurrentDevice == NULL && !bTargetAcquired)
    {
        lpCurrentDevice = device;
        bTargetAcquired = true;
    }

    if(lpCurrentDevice == device)
        SetupD3D9(device);

#if OLDHOOKS
    d3d9Reset.Rehook();
#endif

    return hRes;
}

typedef HRESULT(STDMETHODCALLTYPE *D3D9ResetExPROC)(IDirect3DDevice9Ex *device, D3DPRESENT_PARAMETERS *params, D3DDISPLAYMODEEX *fullscreenData);

HRESULT STDMETHODCALLTYPE D3D9ResetEx(IDirect3DDevice9Ex *device, D3DPRESENT_PARAMETERS *params, D3DDISPLAYMODEEX *fullscreenData)
{
#if OLDHOOKS
    d3d9ResetEx.Unhook();
    d3d9Reset.Unhook();
#endif

    RUNEVERYRESET logOutput << CurrentTimeString() << "D3D9ResetEx called" << endl;

    ClearD3D9Data();

#if OLDHOOKS
    HRESULT hRes = device->ResetEx(params, fullscreenData);
#else
    HRESULT hRes = ((D3D9ResetExPROC)d3d9ResetEx.origFunc)(device, params, fullscreenData);
#endif

    if(lpCurrentDevice == NULL && !bTargetAcquired)
    {
        lpCurrentDevice = device;
        bTargetAcquired = true;
        bD3D9Ex = true;
    }

    if(lpCurrentDevice == device)
        SetupD3D9(device);

#if OLDHOOKS
    d3d9Reset.Rehook();
    d3d9ResetEx.Rehook();
#endif

    return hRes;
}

void LogPresentParams(D3DPRESENT_PARAMETERS &pp);

void SetupD3D9(IDirect3DDevice9 *device)
{
    IDirect3DSwapChain9 *swapChain = NULL;
    if (SUCCEEDED(device->GetSwapChain(0, &swapChain))) {
        D3DPRESENT_PARAMETERS pp;
        if (SUCCEEDED(swapChain->GetPresentParameters(&pp))) {
            d3d9CaptureInfo.format = ConvertDX9BackBufferFormat(pp.BackBufferFormat);
            dxgiFormat = GetDXGIFormat(pp.BackBufferFormat);

            if(d3d9CaptureInfo.format != GS_UNKNOWNFORMAT)
            {
                if( d3d9Format                  != pp.BackBufferFormat ||
                    d3d9CaptureInfo.cx          != pp.BackBufferWidth  ||
                    d3d9CaptureInfo.cy          != pp.BackBufferHeight ||
                    d3d9CaptureInfo.hwndCapture != (DWORD)pp.hDeviceWindow)
                {
                    LogPresentParams(pp);

                    d3d9Format = pp.BackBufferFormat;
                    d3d9CaptureInfo.cx = pp.BackBufferWidth;
                    d3d9CaptureInfo.cy = pp.BackBufferHeight;
                    d3d9CaptureInfo.hwndCapture = (DWORD)pp.hDeviceWindow;
                }
            }
        } else {
            RUNEVERYRESET logOutput << CurrentTimeString() << "failed to get d3d9 present params while initializing hooks" << endl;
        }

        IDirect3D9 *d3d;
        if(SUCCEEDED(device->GetDirect3D(&d3d)))
        {
            IDirect3D9 *d3d9ex;
            if(bD3D9Ex = SUCCEEDED(d3d->QueryInterface(__uuidof(IDirect3D9Ex), (void**)&d3d9ex)))
                d3d9ex->Release();
            d3d->Release();
        }

        /*FARPROC curRelease = GetVTable(device, (8/4));
        if(curRelease != newD3D9Release)
        {
            oldD3D9Release = curRelease;
            newD3D9Release = (FARPROC)D3D9Release;
            SetVTable(device, (8/4), newD3D9Release);
        }*/

        d3d9Present.Hook(GetVTable(device, (68/4)), (FARPROC)D3D9Present);

        if(bD3D9Ex)
        {
            FARPROC curPresentEx = GetVTable(device, (484/4));
            d3d9PresentEx.Hook(curPresentEx, (FARPROC)D3D9PresentEx);
            d3d9ResetEx.Hook(GetVTable(device, (528/4)), (FARPROC)D3D9ResetEx);
        }

        d3d9Reset.Hook(GetVTable(device, (64/4)), (FARPROC)D3D9Reset);

        d3d9SwapPresent.Hook(GetVTable(swapChain, (12/4)), (FARPROC)D3D9SwapPresent);
        d3d9Present.Rehook();
        d3d9SwapPresent.Rehook();
        d3d9Reset.Rehook();

        if(bD3D9Ex) {
            d3d9PresentEx.Rehook();
            d3d9ResetEx.Rehook();
        }

        logOutput << CurrentTimeString() << "successfully set up d3d9 hooks" << endl;

        swapChain->Release();
    } else {
        RUNEVERYRESET logOutput << CurrentTimeString() << "failed to get d3d9 swap chain to initialize hooks" << endl;
    }

    lastTime = 0;
    OSInitializeTimer();
}

typedef IDirect3D9* (WINAPI*D3D9CREATEPROC)(UINT);
typedef HRESULT (WINAPI*D3D9CREATEEXPROC)(UINT, IDirect3D9Ex**);

bool InitD3D9Capture()
{
    bool bSuccess = false;

    TCHAR lpD3D9Path[MAX_PATH];
    SHGetFolderPath(NULL, CSIDL_SYSTEM, NULL, SHGFP_TYPE_CURRENT, lpD3D9Path);
    wcscat_s(lpD3D9Path, MAX_PATH, TEXT("\\d3d9.dll"));

    hD3D9Dll = GetModuleHandle(lpD3D9Path);
    if(hD3D9Dll)
    {
        D3D9CREATEEXPROC d3d9CreateEx = (D3D9CREATEEXPROC)GetProcAddress(hD3D9Dll, "Direct3DCreate9Ex");
        if(d3d9CreateEx)
        {
            HRESULT hRes;

            IDirect3D9Ex *d3d9ex;
            if(SUCCEEDED(hRes = (*d3d9CreateEx)(D3D_SDK_VERSION, &d3d9ex)))
            {
                D3DPRESENT_PARAMETERS pp;
                ZeroMemory(&pp, sizeof(pp));
                pp.Windowed                 = 1;
                pp.SwapEffect               = D3DSWAPEFFECT_FLIP;
                pp.BackBufferFormat         = D3DFMT_A8R8G8B8;
                pp.BackBufferCount          = 1;
                pp.hDeviceWindow            = (HWND)hwndSender;
                pp.PresentationInterval     = D3DPRESENT_INTERVAL_IMMEDIATE;

                IDirect3DDevice9Ex *deviceEx;//D3DDEVTYPE_HAL -- HAL causes tabbing issues, NULLREF seems to fix the issue
                if(SUCCEEDED(hRes = d3d9ex->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, hwndSender, D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_NOWINDOWCHANGES, &pp, NULL, &deviceEx)))
                {
                    bSuccess = true;

                    UPARAM *vtable = *(UPARAM**)deviceEx;

                    d3d9EndScene.Hook((FARPROC)*(vtable+(168/4)), (FARPROC)D3D9EndScene);
                    /*d3d9ResetEx.Hook((FARPROC)*(vtable+(528/4)), (FARPROC)D3D9ResetEx);
                    d3d9Reset.Hook((FARPROC)*(vtable+(64/4)), (FARPROC)D3D9Reset);*/

                    deviceEx->Release();

                    d3d9EndScene.Rehook();
                    /*d3d9Reset.Rehook();
                    d3d9ResetEx.Rehook();*/
                }
                else
                {
                    RUNEVERYRESET logOutput << CurrentTimeString() << "InitD3D9Capture: d3d9ex->CreateDeviceEx failed, result: " << (UINT)hRes << endl;
                }

                d3d9ex->Release();
            }
            else
            {
                RUNEVERYRESET logOutput << CurrentTimeString() << "InitD3D9Capture: Direct3DCreate9Ex failed, result: " << (UINT)hRes << endl;
            }
        }
        else
        {
            RUNEVERYRESET logOutput << CurrentTimeString() << "InitD3D9Capture: could not load address of Direct3DCreate9Ex" << endl;
        }
    }

    return bSuccess;
}

void CheckD3D9Capture()
{
    EnterCriticalSection(&d3d9EndMutex);
#if OLDHOOKS
    d3d9EndScene.Rehook(true);
#endif
    LeaveCriticalSection(&d3d9EndMutex);
}

void FreeD3D9Capture()
{
    d3d9EndScene.Unhook();
    d3d9ResetEx.Unhook();
    d3d9Reset.Unhook();

    ClearD3D9Data();
}
