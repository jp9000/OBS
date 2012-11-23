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
LONGLONG                frameTime = 0;
DWORD                   fps = 0;
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
extern ID3D10Device1    *shareDevice;
extern ID3D10Resource   *copyTextureIntermediary;
extern HANDLE           sharedHandles[2];
extern IDXGIKeyedMutex  *keyedMutexes[2];
extern ID3D10Resource   *sharedTextures[2];

extern bool             bD3D101Hooked;

HMODULE                 hD3D9Dll = NULL;

int                     patchType = 0;



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

#ifdef _WIN64

#define NUM_KNOWN_PATCHES 0
#define PATCH_COMPARE_SIZE 1
UPARAM patch_offsets[1] = {0};
UPARAM patch_compare[1][PATCH_COMPARE_SIZE] = {{0}};

#else

#define NUM_KNOWN_PATCHES 3
#define PATCH_COMPARE_SIZE 12
UPARAM patch_offsets[NUM_KNOWN_PATCHES] = {0x79D96, 0x166A08, 0x79C9E};
BYTE patch_compare[NUM_KNOWN_PATCHES][PATCH_COMPARE_SIZE] =
{
    {0x8b, 0x89, 0xe8, 0x29, 0x00, 0x00, 0x39, 0xb9, 0x80, 0x4b, 0x00, 0x00},  //win7 - 6.1.7601.17514
    {0x8b, 0x80, 0xe8, 0x29, 0x00, 0x00, 0x39, 0x90, 0xb0, 0x4b, 0x00, 0x00},  //win8 - 6.2.9200.16384
    {0x8b, 0x89, 0xe8, 0x29, 0x00, 0x00, 0x39, 0xb9, 0x80, 0x4b, 0x00, 0x00},  //win7 - 6.1.7600.16385
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
    if(texData)
        texData->lastRendered = -1;
    if(copyData)
        copyData->lastRendered = -1;

    for(int i=0; i<2; i++)
    {
        SafeRelease(keyedMutexes[i]);
        SafeRelease(sharedTextures[i]);
    }

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
    fps = 0;
    frameTime = 0;
    curCapture = 0;
    curCPUTexture = 0;
    pCopyData = NULL;
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
    BOOL bSuccess = false;

    HRESULT hErr;

    bD3D101Hooked = true;
    HMODULE hD3D10_1 = LoadLibrary(TEXT("d3d10_1.dll"));
    if(!hD3D10_1)
    {
        RUNONCE logOutput << "DoD3D9GPUHook: Could not load D3D10.1" << endl;
        goto finishGPUHook;
    }

    HMODULE hDXGI = LoadLibrary(TEXT("dxgi.dll"));
    if(!hDXGI)
    {
        RUNONCE logOutput << "DoD3D9GPUHook: Could not load dxgi" << endl;
        goto finishGPUHook;
    }

    CREATEDXGIFACTORY1PROC createDXGIFactory1 = (CREATEDXGIFACTORY1PROC)GetProcAddress(hDXGI, "CreateDXGIFactory1");
    if(!createDXGIFactory1)
    {
        RUNONCE logOutput << "DoD3D9GPUHook: Could not load 'CreateDXGIFactory1'" << endl;
        goto finishGPUHook;
    }

    PFN_D3D10_CREATE_DEVICE1 d3d10CreateDevice1 = (PFN_D3D10_CREATE_DEVICE1)GetProcAddress(hD3D10_1, "D3D10CreateDevice1");
    if(!d3d10CreateDevice1)
    {
        RUNONCE logOutput << "DoD3D9GPUHook: Could not load 'D3D10CreateDevice1'" << endl;
        goto finishGPUHook;
    }

    IDXGIFactory1 *factory;
    if(FAILED(hErr = (*createDXGIFactory1)(__uuidof(IDXGIFactory1), (void**)&factory)))
    {
        RUNONCE logOutput << "DoD3D9GPUHook: CreateDXGIFactory1 failed, result = " << (UINT)hErr << endl;
        goto finishGPUHook;
    }

    IDXGIAdapter1 *adapter;
    if(FAILED(hErr = factory->EnumAdapters1(0, &adapter)))
    {
        RUNONCE logOutput << "DoD3D9GPUHook: factory->EnumAdapters1 failed, result = " << (UINT)hErr << endl;
        factory->Release();
        goto finishGPUHook;
    }

    if(FAILED(hErr = (*d3d10CreateDevice1)(adapter, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, &shareDevice)))
    {
        if(FAILED(hErr = (*d3d10CreateDevice1)(adapter, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_FEATURE_LEVEL_9_3, D3D10_1_SDK_VERSION, &shareDevice)))
        {
            RUNONCE logOutput << "DoD3D9GPUHook: Could not create D3D10.1 device, result = " << (UINT)hErr << endl;
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
        RUNONCE logOutput << "DoD3D9GPUHook: shareDevice->CreateTexture2D failed, result = " << (UINT)hErr << endl;
        goto finishGPUHook;
    }

    if(FAILED(hErr = d3d101Tex->QueryInterface(__uuidof(ID3D10Resource), (void**)&copyTextureIntermediary)))
    {
        RUNONCE logOutput << "DoD3D9GPUHook: d3d101Tex->QueryInterface(ID3D10Resource) failed, result = " << (UINT)hErr << endl;
        d3d101Tex->Release();
        goto finishGPUHook;
    }

    IDXGIResource *res;
    if(FAILED(hErr = d3d101Tex->QueryInterface(IID_IDXGIResource, (void**)&res)))
    {
        RUNONCE logOutput << "DoD3D9GPUHook: d3d101Tex->QueryInterface(IDXGIResource) failed, result = " << (UINT)hErr << endl;
        d3d101Tex->Release();
        goto finishGPUHook;
    }

    HANDLE handle;
    if(FAILED(res->GetSharedHandle(&handle)))
    {
        RUNONCE logOutput << "DoD3D9GPUHook: res->GetSharedHandle failed, result = " << (UINT)hErr << endl;
        d3d101Tex->Release();
        res->Release();
        goto finishGPUHook;
    }

    d3d101Tex->Release();
    res->Release();

    //------------------------------------------------

    LPBYTE patchAddress = (patchType != 0) ? GetD3D9PatchAddress() : NULL;
    BYTE savedByte;
    DWORD dwOldProtect;

    if(patchAddress)
    {
        if(VirtualProtect(patchAddress, 1, PAGE_EXECUTE_READWRITE, &dwOldProtect))
        {
            savedByte = *patchAddress;
            *patchAddress = 0xEB;
        }
        else
        {
            RUNONCE logOutput << "DoD3D9GPUHook: unable to change memory protection, result = " << GetLastError() << endl;
            goto finishGPUHook;
        }
    }

    IDirect3DTexture9 *d3d9Tex;
    if(FAILED(hErr = device->CreateTexture(d3d9CaptureInfo.cx, d3d9CaptureInfo.cy, 1, D3DUSAGE_RENDERTARGET, (D3DFORMAT)d3d9Format, D3DPOOL_DEFAULT, &d3d9Tex, &handle)))
    {
        RUNONCE logOutput << "DoD3D9GPUHook: opening intermediary texture failed, result = " << (UINT)hErr << endl;
        goto finishGPUHook;
    }

    if(patchAddress)
    {
        *patchAddress = savedByte;
        VirtualProtect(patchAddress, 1, dwOldProtect, &dwOldProtect);
    }

    if(FAILED(hErr = d3d9Tex->GetSurfaceLevel(0, &copyD3D9TextureGame)))
    {
        RUNONCE logOutput << "DoD3D9GPUHook: d3d9Tex->GetSurfaceLevel failed, result = " << (UINT)hErr << endl;
        d3d9Tex->Release();
        goto finishGPUHook;
    }

    d3d9Tex->Release();

    //------------------------------------------------

    D3D10_TEXTURE2D_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(texDesc));
    texDesc.Width               = d3d9CaptureInfo.cx;
    texDesc.Height              = d3d9CaptureInfo.cy;
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
            RUNONCE logOutput << "DoD3D9GPUHook: creation of share texture " << i << " failed, result = " << (UINT)hErr << endl;
            goto finishGPUHook;
        }

        if(FAILED(hErr = d3d10tex->QueryInterface(__uuidof(ID3D10Resource), (void**)&sharedTextures[i])))
        {
            RUNONCE logOutput << "DoD3D9GPUHook: share texture " << i << " d3d10tex->QueryInterface(ID3D10Resource) failed, result = " << (UINT)hErr << endl;
            d3d10tex->Release();
            goto finishGPUHook;
        }

        if(FAILED(hErr = d3d10tex->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)&keyedMutexes[i])))
        {
            RUNONCE logOutput << "DoD3D9GPUHook: share texture " << i << " d3d10tex->QueryInterface(IDXGIKeyedMutex) failed, result = " << (UINT)hErr << endl;
            d3d10tex->Release();
            goto finishGPUHook;
        }

        IDXGIResource *res;
        if(FAILED(hErr = d3d10tex->QueryInterface(__uuidof(IDXGIResource), (void**)&res)))
        {
            RUNONCE logOutput << "DoD3D9GPUHook: share texture " << i << " d3d10tex->QueryInterface(IDXGIResource) failed, result = " << (UINT)hErr << endl;
            d3d10tex->Release();
            goto finishGPUHook;
        }

        if(FAILED(hErr = res->GetSharedHandle(&sharedHandles[i])))
        {
            RUNONCE logOutput << "DoD3D9GPUHook: share texture " << i << " res->GetSharedHandle failed, result = " << (UINT)hErr << endl;
            res->Release();
            d3d10tex->Release();
            goto finishGPUHook;
        }

        res->Release();
        d3d10tex->Release();
    }

    d3d9CaptureInfo.mapID = InitializeSharedMemoryGPUCapture(&texData);
    if(!d3d9CaptureInfo.mapID)
    {
        RUNONCE logOutput << "DoD3D9GPUHook: failed to initialize shared memory" << endl;
        goto finishGPUHook;
    }

    bSuccess = IsWindow(hwndReceiver);

finishGPUHook:

    if(bSuccess)
    {
        bHasTextures = true;
        d3d9CaptureInfo.captureType = CAPTURETYPE_SHAREDTEX;
        d3d9CaptureInfo.hwndSender = hwndSender;
        d3d9CaptureInfo.bFlip = FALSE;
        texData->texHandles[0] = sharedHandles[0];
        texData->texHandles[1] = sharedHandles[1];
        fps = (DWORD)SendMessage(hwndReceiver, RECEIVER_NEWCAPTURE, 0, (LPARAM)&d3d9CaptureInfo);
        frameTime = 1000000/LONGLONG(fps)/2;

        logOutput << "DoD3D9GPUHook: success" << endl;
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

    for(UINT i=0; i<NUM_BUFFERS; i++)
    {
        if(FAILED(hErr = device->CreateOffscreenPlainSurface(d3d9CaptureInfo.cx, d3d9CaptureInfo.cy, (D3DFORMAT)d3d9Format, D3DPOOL_SYSTEMMEM, &textures[i], NULL)))
        {
            RUNONCE logOutput << "DoD3D9CPUHook: device->CreateOffscreenPlainSurface " << i << " failed, result = " << (UINT)hErr << endl;
            bSuccess = false;
            break;
        }

        if(i == (NUM_BUFFERS-1))
        {
            D3DLOCKED_RECT lr;
            if(FAILED(hErr = textures[i]->LockRect(&lr, NULL, D3DLOCK_READONLY)))
            {
                RUNONCE logOutput << "DoD3D9CPUHook: textures[" << i << "]->LockRect failed, result = " << (UINT)hErr << endl;
                bSuccess = false;
                break;
            }

            pitch = lr.Pitch;
            textures[i]->UnlockRect();
        }
    }

    if(bSuccess)
    {
        for(UINT i=0; i<NUM_BUFFERS; i++)
        {
            if(FAILED(hErr = device->CreateRenderTarget(d3d9CaptureInfo.cx, d3d9CaptureInfo.cy, (D3DFORMAT)d3d9Format, D3DMULTISAMPLE_NONE, 0, FALSE, &copyD3D9Textures[i], NULL)))
            {
                RUNONCE logOutput << "DoD3D9CPUHook: device->CreateTexture " << i << " failed, result = " << (UINT)hErr << endl;
                bSuccess = false;
                break;
            }

            if(FAILED(hErr = device->CreateQuery(D3DQUERYTYPE_EVENT, &queries[i])))
            {
                RUNONCE logOutput << "DoD3D9CPUHook: device->CreateQuery " << i << " failed, result = " << (UINT)hErr << endl;
                bSuccess = false;
                break;
            }

            if(!(dataMutexes[i] = OSCreateMutex()))
            {
                RUNONCE logOutput << "DoD3D9CPUHook: OSCreateMutex " << i << " failed, GetLastError = " << GetLastError() << endl;
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
                RUNONCE logOutput << "DoD3D9CPUHook: CreateEvent failed, GetLastError = " << GetLastError() << endl;
                bSuccess = false;
            }
        }
        else
        {
            RUNONCE logOutput << "DoD3D9CPUHook: CreateThread failed, GetLastError = " << GetLastError() << endl;
            bSuccess = false;
        }
    }

    if(bSuccess)
    {
        d3d9CaptureInfo.mapID = InitializeSharedMemoryCPUCapture(pitch*d3d9CaptureInfo.cy, &d3d9CaptureInfo.mapSize, &copyData, textureBuffers);
        if(!d3d9CaptureInfo.mapID)
        {
            RUNONCE logOutput << "DoD3D9CPUHook: failed to initialize shared memory" << endl;
            bSuccess = false;
        }
    }

    if(bSuccess)
        bSuccess = IsWindow(hwndReceiver);

    if(bSuccess)
    {
        bHasTextures = true;
        d3d9CaptureInfo.captureType = CAPTURETYPE_MEMORY;
        d3d9CaptureInfo.pitch = pitch;
        d3d9CaptureInfo.hwndSender = hwndSender;
        d3d9CaptureInfo.bFlip = FALSE;
        fps = (DWORD)SendMessage(hwndReceiver, RECEIVER_NEWCAPTURE, 0, (LPARAM)&d3d9CaptureInfo);
        frameTime = 1000000/LONGLONG(fps);

        logOutput << "DoD3D9CPUHook: success, fps = " << fps << ", frameTime = " << frameTime << endl;
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
        logOutput << "CopyD3D9CPUTextureThread: couldn't duplicate handle" << endl;
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
                SSECopy(textureBuffers[lastRendered], data, d3d9CaptureInfo.pitch*d3d9CaptureInfo.cy);
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



void DoD3D9DrawStuff(IDirect3DDevice9 *device)
{
    if(bStopRequested)
    {
        ClearD3D9Data();
        bStopRequested = false;
    }

    if(!bHasTextures && bCapturing)
    {
        if(d3d9Format)
        {
            if(!hwndReceiver)
                hwndReceiver = FindWindow(RECEIVER_WINDOWCLASS, NULL);

            if(hwndReceiver)
            {
                if(bD3D9Ex)
                    bUseSharedTextures = true;
                else
                    bUseSharedTextures = (patchType = GetD3D9PatchType()) != 0;

                if(bUseSharedTextures)
                    DoD3D9GPUHook(device);
                else
                    DoD3D9CPUHook(device);
            }
        }
    }

    //device->BeginScene();

    if(bHasTextures)
    {
        if(bCapturing)
        {
            if(bUseSharedTextures) //shared texture support
            {
                LONGLONG timeVal = OSGetTimeMicroseconds();
                LONGLONG timeElapsed = timeVal-lastTime;

                if(timeElapsed >= frameTime)
                {
                    lastTime += frameTime;
                    if(timeElapsed > frameTime*2)
                        lastTime = timeVal;

                    DWORD nextCapture = curCapture == 0 ? 1 : 0;

                    IDirect3DSurface9 *texture = textures[curCapture];
                    IDirect3DSurface9 *backBuffer = NULL;

                    if(SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
                    {
                        if(SUCCEEDED(device->StretchRect(backBuffer, NULL, copyD3D9TextureGame, NULL, D3DTEXF_NONE)))
                        {
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
                        }

                        backBuffer->Release();
                    }

                    curCapture = nextCapture;
                }
            }
            else //slow regular d3d9, no shared textures
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

                LONGLONG timeVal = OSGetTimeMicroseconds();
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

                            HRESULT hErr;
                            if(FAILED(hErr = device->GetRenderTargetData(prevSourceTexture, targetTexture)))
                            {
                                int test = 0;
                            }

                            queries[nextCapture]->Issue(D3DISSUE_END);
                            issuedQueries[nextCapture] = true;
                        }
                    }

                    curCapture = nextCapture;
                }
            }
        }
        else
            ClearD3D9Data();
    }

    //device->EndScene();
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
            ClearD3D9Data();
            lpCurrentDevice = NULL;
            bTargetAcquired = false;
        }
    }
    else if(refVal == 1)
    {
        lpCurrentDevice = NULL;
        bTargetAcquired = false;

        logOutput << "d3d9 capture terminated by the application" << endl;
    }

    return (*(RELEASEPROC)oldD3D9Release)(device);
}

HRESULT STDMETHODCALLTYPE D3D9EndScene(IDirect3DDevice9 *device)
{
    d3d9EndScene.Unhook();

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

    HRESULT hRes = device->EndScene();
    d3d9EndScene.Rehook();

    return hRes;
}

HRESULT STDMETHODCALLTYPE D3D9Present(IDirect3DDevice9 *device, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
{
    d3d9Present.Unhook();

    if(!presentRecurse)
        DoD3D9DrawStuff(device);

    presentRecurse++;

    HRESULT hRes = device->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);

    presentRecurse--;
    d3d9Present.Rehook();

    return hRes;
}

HRESULT STDMETHODCALLTYPE D3D9PresentEx(IDirect3DDevice9Ex *device, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion, DWORD dwFlags)
{
    d3d9PresentEx.Unhook();

    if(!presentRecurse)
        DoD3D9DrawStuff(device);

    presentRecurse++;

    HRESULT hRes = device->PresentEx(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);

    presentRecurse--;
    d3d9PresentEx.Rehook();

    return hRes;
}

HRESULT STDMETHODCALLTYPE D3D9SwapPresent(IDirect3DSwapChain9 *swap, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion, DWORD dwFlags)
{
    d3d9SwapPresent.Unhook();

    if(!presentRecurse)
        DoD3D9DrawStuff((IDirect3DDevice9*)lpCurrentDevice);

    presentRecurse++;

    HRESULT hRes = swap->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);

    presentRecurse--;
    d3d9SwapPresent.Rehook();

    return hRes;
}

HRESULT STDMETHODCALLTYPE D3D9Reset(IDirect3DDevice9 *device, D3DPRESENT_PARAMETERS *params)
{
    d3d9Reset.Unhook();

    ClearD3D9Data();

    HRESULT hRes = device->Reset(params);

    if(lpCurrentDevice == NULL && !bTargetAcquired)
    {
        lpCurrentDevice = device;
        bTargetAcquired = true;
    }

    if(lpCurrentDevice == device)
        SetupD3D9(device);

    d3d9Reset.Rehook();

    return hRes;
}

HRESULT STDMETHODCALLTYPE D3D9ResetEx(IDirect3DDevice9Ex *device, D3DPRESENT_PARAMETERS *params, D3DDISPLAYMODEEX *fullscreenData)
{
    d3d9ResetEx.Unhook();
    d3d9Reset.Unhook();

    ClearD3D9Data();

    HRESULT hRes = device->ResetEx(params, fullscreenData);

    if(lpCurrentDevice == NULL && !bTargetAcquired)
    {
        lpCurrentDevice = device;
        bTargetAcquired = true;
        bD3D9Ex = true;
    }

    if(lpCurrentDevice == device)
        SetupD3D9(device);

    d3d9Reset.Rehook();
    d3d9ResetEx.Rehook();

    return hRes;
}


void SetupD3D9(IDirect3DDevice9 *device)
{
    IDirect3DSwapChain9 *swapChain = NULL;
    if(SUCCEEDED(device->GetSwapChain(0, &swapChain)))
    {
        D3DPRESENT_PARAMETERS pp;
        if(SUCCEEDED(swapChain->GetPresentParameters(&pp)))
        {
            d3d9CaptureInfo.format = ConvertDX9BackBufferFormat(pp.BackBufferFormat);
            dxgiFormat = GetDXGIFormat(pp.BackBufferFormat);

            if(d3d9CaptureInfo.format != GS_UNKNOWNFORMAT)
            {
                if( d3d9Format          != pp.BackBufferFormat ||
                    d3d9CaptureInfo.cx  != pp.BackBufferWidth  ||
                    d3d9CaptureInfo.cy  != pp.BackBufferHeight )
                {
                    d3d9Format = pp.BackBufferFormat;
                    d3d9CaptureInfo.cx = pp.BackBufferWidth;
                    d3d9CaptureInfo.cy = pp.BackBufferHeight;
                    d3d9CaptureInfo.hwndCapture = pp.hDeviceWindow;
                }
            }
        }

        IDirect3D9 *d3d;
        if(SUCCEEDED(device->GetDirect3D(&d3d)))
        {
            IDirect3D9 *d3d9ex;
            if(bD3D9Ex = SUCCEEDED(d3d->QueryInterface(__uuidof(IDirect3D9Ex), (void**)&d3d9ex)))
                d3d9ex->Release();
            d3d->Release();
        }

        FARPROC curRelease = GetVTable(device, (8/4));
        if(curRelease != newD3D9Release)
        {
            oldD3D9Release = curRelease;
            newD3D9Release = (FARPROC)D3D9Release;
            SetVTable(device, (8/4), newD3D9Release);
        }

        FARPROC curPresent = GetVTable(device, (68/4));
        d3d9Present.Hook(curPresent, (FARPROC)D3D9Present);

        if(bD3D9Ex)
        {
            FARPROC curPresentEx = GetVTable(device, (484/4));
            d3d9PresentEx.Hook(curPresentEx, (FARPROC)D3D9PresentEx);
        }

        FARPROC curD3D9SwapPresent = GetVTable(swapChain, (12/4));
        d3d9SwapPresent.Hook(curD3D9SwapPresent, (FARPROC)D3D9SwapPresent);
        d3d9Present.Rehook();
        d3d9SwapPresent.Rehook();

        if(bD3D9Ex)
            d3d9PresentEx.Rehook();

        swapChain->Release();
    }

    lastTime = 0;
    OSInitializeTimer();
}

typedef IDirect3D9* (WINAPI*D3D9CREATEPROC)(UINT);
typedef HRESULT (WINAPI*D3D9CREATEEXPROC)(UINT, IDirect3D9Ex**);

bool InitD3D9Capture()
{
    bool bSuccess = false;

    hD3D9Dll = GetModuleHandle(TEXT("d3d9.dll"));
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
                    d3d9ResetEx.Hook((FARPROC)*(vtable+(528/4)), (FARPROC)D3D9ResetEx);
                    d3d9Reset.Hook((FARPROC)*(vtable+(64/4)), (FARPROC)D3D9Reset);

                    deviceEx->Release();

                    d3d9EndScene.Rehook();
                    d3d9Reset.Rehook();
                    d3d9ResetEx.Rehook();
                }
                else
                {
                    RUNONCE logOutput << "InitD3D9Capture: d3d9ex->CreateDeviceEx failed, result: " << (UINT)hRes << endl;
                }

                d3d9ex->Release();
            }
            else
            {
                RUNONCE logOutput << "InitD3D9Capture: Direct3DCreate9Ex failed, result: " << (UINT)hRes << endl;
            }
        }
        else
        {
            RUNONCE logOutput << "InitD3D9Capture: could not load address of Direct3DCreate9Ex" << endl;
        }
    }

    return bSuccess;
}

void FreeD3D9Capture()
{
    d3d9EndScene.Unhook();
    d3d9ResetEx.Unhook();
    d3d9Reset.Unhook();
}
