/********************************************************************************
 Copyright (C) 2013 Hugh Bailey <obs.jim@gmail.com>

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

#include "d3d8.h"
#include <VersionHelpers.h>

typedef IDirect3D8* (WINAPI*D3D8CREATEPROC)(UINT);

extern CRITICAL_SECTION d3d8EndMutex;
extern MemoryCopyData*  copyData;
extern LPBYTE           textureBuffers[2];
extern DWORD            curCapture;
extern HANDLE           hCopyThread;
extern HANDLE           hCopyEvent;
extern BOOL             bHasTextures;
extern bool             bKillThread;
extern DWORD            curCPUTexture;
extern DWORD            copyWait;
extern BOOL             bHasTextures;
extern LONGLONG         lastTime;

const int NUM_BUFFERS = 3;
#define ZERO_ARRAY { NULL, NULL, NULL }

HookData d3d8EndScene;
HookData d3d8Present;
HookData d3d8Release;
HookData d3d8Reset;
CaptureInfo d3d8CaptureInfo;

IDirect3DSurface8* targetBuffers[NUM_BUFFERS] = ZERO_ARRAY;
HANDLE dataMutex[NUM_BUFFERS] = ZERO_ARRAY;
bool targetLocked[NUM_BUFFERS] = ZERO_ARRAY;
void* pixelData = NULL;
D3DFORMAT pixelFormat = D3DFMT_UNKNOWN;
DWORD sourcePitch = 0;
LPDIRECT3DDEVICE8 lpCurrentDevice = NULL;

DWORD CopyD3D8CPUTextureThread(LPVOID lpUseless);

bool pixelFormatIsSupported(D3DFORMAT format);
void SetupD3D8(IDirect3DDevice8* device);
void CaptureD3D8(IDirect3DDevice8* device);
bool CreateCPUCapture(IDirect3DDevice8* device);
void ClearD3D8Data();

ULONG STDMETHODCALLTYPE D3D8Release(IDirect3DDevice8* device)
{
    d3d8Release.Unhook();
    ULONG refCount = device->Release();
    if (refCount == 0)
    {
        logOutput << CurrentTimeString() << "Device released" << endl;
        ClearD3D8Data();
    }
    d3d8Release.Rehook();

    return refCount;
}

HRESULT STDMETHODCALLTYPE D3D8Reset(IDirect3DDevice8* device, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
    RUNEVERYRESET logOutput << CurrentTimeString() << "D3D8Reset called" << endl;

    ClearD3D8Data();

    d3d8Reset.Unhook();
    HRESULT hr = device->Reset(pPresentationParameters);

    if (lpCurrentDevice == NULL && !bTargetAcquired)
    {
        lpCurrentDevice = device;
        bTargetAcquired = true;
    }

    if (lpCurrentDevice == device)
        SetupD3D8(device);

    d3d8Reset.Rehook();

    return hr;
}

HRESULT STDMETHODCALLTYPE D3D8Present(IDirect3DDevice8* device, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
{
    RUNEVERYRESET logOutput << CurrentTimeString() << "D3D8Present called" << endl;

    CaptureD3D8(device);
    d3d8Present.Unhook();
    HRESULT hr = device->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
    d3d8Present.Rehook();

    /*
    if (!bHasTextures && bCapturing)
    {
    // initialize shared memory and texture buffers for CPU capture
    if (hwndOBS)
    {
    if (!bTargetAcquired)
    {
    SetupD3D8(device);
    bTargetAcquired = true;
    }
    if (!CreateCPUCapture(device) && bTargetAcquired)
    ClearD3D8Data();
    else
    bHasTextures = true;
    }
    }
    */

    return hr;
}

HRESULT STDMETHODCALLTYPE D3D8EndScene(IDirect3DDevice8* device)
{
    //logOutput << CurrentTimeString() << "D3D8EndScene hooked" << endl;

    EnterCriticalSection(&d3d8EndMutex);

    if (!bTargetAcquired && bCapturing)
    {
        SetupD3D8(device);
        bTargetAcquired = true;
    }

    d3d8EndScene.Unhook();
    HRESULT hr = device->EndScene();
    d3d8EndScene.Rehook();

    LeaveCriticalSection(&d3d8EndMutex);

    return hr;
}

void printD3Derror(string section, HRESULT err)
{
    string err_str;
    switch (err)
    {
    case D3D_OK: err_str = "no error"; break;
    case D3DERR_INVALIDCALL: err_str = "invalid call"; break;
    case D3DERR_DEVICELOST: err_str = "device lost"; break;
    default: err_str = "unknown error";
    }

    logOutput << CurrentTimeString() + section + ": " + err_str << endl;
}

void printColorFormat(D3DFORMAT format)
{
    string fmt_str;
    switch (format)
    {
    case D3DFMT_UNKNOWN: fmt_str = "unknown"; break;
    case D3DFMT_R8G8B8: fmt_str = "R8G8B8"; break;
    case D3DFMT_A8R8G8B8: fmt_str = "A8R8G8B8"; break;
    case D3DFMT_X8R8G8B8: fmt_str = "X8R8G8B8"; break;
    case D3DFMT_R5G6B5: fmt_str = "R5G6B5"; break;
    case D3DFMT_A1R5G5B5: fmt_str = "A1R5G5B5"; break;
    case D3DFMT_X1R5G5B5: fmt_str = "X1R5G5B5"; break;
    case D3DFMT_R3G3B2: fmt_str = "R3G3B2"; break;
    default: fmt_str = "undefined format";
    }

    logOutput << CurrentTimeString() + "Color format: " + fmt_str << endl;
}

void printDescriptorInfo(D3DSURFACE_DESC d)
{
    stringstream output;
    output << "surface descriptor:";
    output << "\n\t width: " << d.Width;
    output << "\n\t height: " << d.Height;
    output << "\n\t size: " << d.Size;
    output << "\n\t type: " << d.Type;
    output << "\n\t format: " << d.Format;
    output << "\n\t usage: " << d.Usage;
    output << "\n\t pool: " << d.Pool;
    output << "\n\t multisample type: " << d.MultiSampleType;
    output << "\n";

    logOutput << output.str() << flush;
}

bool pixelFormatIsSupported(D3DFORMAT format)
{
    switch (format)
    {
    case D3DFMT_A8R8G8B8:
    case D3DFMT_X8R8G8B8:
    case D3DFMT_R5G6B5:
    case D3DFMT_A1R5G5B5:
    case D3DFMT_X1R5G5B5:
    case D3DFMT_R3G3B2:
        return true;
    default:
        return false;
    }
}

void convertPixelFormat(LPBYTE in, DWORD inPitch, D3DFORMAT inFormat, DWORD outWidth, DWORD outHeight, LPBYTE out)
{
    bool x8r8g8b8 = false;
    bool x1r5g5b5 = false;
    bool r5g6b5 = false;
    bool r3g3b2 = false;

    switch (inFormat)
    {
    case D3DFMT_X8R8G8B8:
    case D3DFMT_A8R8G8B8:
        x8r8g8b8 = true;
        break;
    case D3DFMT_R5G6B5:
        r5g6b5 = true;
        break;
    case D3DFMT_A1R5G5B5:
    case D3DFMT_X1R5G5B5:
        x1r5g5b5 = true;
        break;
    case D3DFMT_R3G3B2:
        r3g3b2 = true;
        break;
    default:
        return;
    }

    // 32bit fast plaincopy
    if (x8r8g8b8)
    {
        LPBYTE src = in;
        LPBYTE dst = out;
        DWORD size = 4 * outWidth;
        for (unsigned int i = 0; i < outHeight; i++)
        {
            memcpy(dst, src, size);
            src += inPitch;
            dst += size;
        }
    }
    // 16bit slow pixel conversion
    else if (r5g6b5)
    {
        LPDWORD dst = (LPDWORD)out;
        LPWORD src_row_begin = (LPWORD)in;
        for (DWORD j = 0; j < outHeight; j++)
        {
            LPWORD src = src_row_begin;
            for (DWORD i = 0; i < outWidth; i++)
            {
                WORD pxl = *src;
                *dst = 0xFF000000 + ((0xF800 & pxl) << 8) + ((0x07E0 & pxl) << 5) + ((0x001F & pxl) << 3);
                src++;
                dst++;
            }
            src_row_begin += inPitch;
        }
    }
    else if (x1r5g5b5)
    {
        LPDWORD dst = (LPDWORD)out;
        LPWORD src_row_begin = (LPWORD)in;
        for (DWORD j = 0; j < outHeight; j++)
        {
            LPWORD src = src_row_begin;
            for (DWORD i = 0; i < outWidth; i++)
            {
                WORD pxl = *src;
                *dst = 0xFF000000 + ((0x7C00 & pxl) << 9) + ((0x03E0 & pxl) << 6) + ((0x001F & pxl) << 3);
                src++;
                dst++;
            }
            src_row_begin += inPitch;
        }
    }
    // 8bit slow pixel conversion
    else if (r3g3b2)
    {
        LPDWORD dst = (LPDWORD)out;
        LPBYTE src_row_begin = (LPBYTE)in;
        for (DWORD j = 0; j < outHeight; j++)
        {
            LPBYTE src = src_row_begin;
            for (DWORD i = 0; i < outWidth; i++)
            {
                BYTE pxl = *src;
                *dst = 0xFF000000 + ((0xE0 & pxl) << 16) + ((0x1C & pxl) << 11) + ((0x03 & pxl) << 6);
                src++;
                dst++;
            }
            src_row_begin += inPitch;
        }
    }
}

DWORD CopyD3D8CPUTextureThread(LPVOID lpUseless)
{
    int sharedMemID = 0;

    HANDLE hEvent = NULL;
    if (!DuplicateHandle(GetCurrentProcess(), hCopyEvent, GetCurrentProcess(), &hEvent, NULL, FALSE, DUPLICATE_SAME_ACCESS))
    {
        logOutput << CurrentTimeString() << "CopyD3D8CPUTextureThread: couldn't duplicate handle" << endl;
        return 0;
    }

    logOutput << CurrentTimeString() + "CopyD3D8CPUTextureThread: waiting for copyEvents" << endl;
    while (WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0)
    {
        if (bKillThread)
            break;

        int nextSharedMemID = sharedMemID == 0 ? 1 : 0;

        DWORD copyTex = curCPUTexture;
        LPVOID data = pixelData;
        if (copyTex < NUM_BUFFERS && data != NULL)
        {
            OSEnterMutex(dataMutex[copyTex]);

            int lastRendered = -1;

            //copy to whichever is available
            if (WaitForSingleObject(textureMutexes[sharedMemID], 0) == WAIT_OBJECT_0)
                lastRendered = (int)sharedMemID;
            else if (WaitForSingleObject(textureMutexes[nextSharedMemID], 0) == WAIT_OBJECT_0)
                lastRendered = (int)nextSharedMemID;

            if (lastRendered != -1)
            {
                convertPixelFormat((LPBYTE)data, sourcePitch, pixelFormat, d3d8CaptureInfo.cx, d3d8CaptureInfo.cy, textureBuffers[lastRendered]);
                ReleaseMutex(textureMutexes[lastRendered]);
                copyData->lastRendered = (UINT)lastRendered;
            }

            OSLeaveMutex(dataMutex[copyTex]);
        }

        sharedMemID = nextSharedMemID;
    }

    CloseHandle(hEvent);
    return 0;
}

void SetupD3D8(IDirect3DDevice8* device)
{
    HRESULT hr;
    IDirect3DSurface8* backBuffer;

    logOutput << CurrentTimeString() + "Called SetupD3D8" << endl;

    if (FAILED(hr = device->GetRenderTarget(&backBuffer)))
    {
        printD3Derror("SetupD3D8, device->GetRenderTarget", hr);
        return;
    }

    D3DSURFACE_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    if (FAILED(hr = backBuffer->GetDesc(&desc)))
    {
        backBuffer->Release();
        printD3Derror("SetupD3D8, backBuffer->GetDesc", hr);
        return;
    }

    backBuffer->Release();

    printDescriptorInfo(desc);
    printColorFormat(desc.Format);

    if (!pixelFormatIsSupported(desc.Format))
    {
        RUNEVERYRESET logOutput << CurrentTimeString() + "pixel format not supported" << endl;
        return;
    }

    lpCurrentDevice = device;

    pixelFormat = desc.Format;

    d3d8CaptureInfo.cx = desc.Width;
    d3d8CaptureInfo.cy = desc.Height;
    d3d8CaptureInfo.hwndCapture = (DWORD)hwndSender;
    d3d8CaptureInfo.format = GS_BGRA;
    d3d8CaptureInfo.pitch = 4 * d3d8CaptureInfo.cx;

    //d3d8Release.Hook(GetVTable(device, (8 / 4)), (FARPROC)D3D8Release);
    //d3d8Release.Rehook();
    d3d8Reset.Hook(GetVTable(device, (56 / 4)), (FARPROC)D3D8Reset);
    d3d8Reset.Rehook();
    d3d8Present.Hook(GetVTable(device, (60 / 4)), (FARPROC)D3D8Present);
    d3d8Present.Rehook();

    OSInitializeTimer();
}

void CaptureD3D8(IDirect3DDevice8* device)
{
    HRESULT hErr;

    if (bCapturing && WaitForSingleObject(hSignalEnd, 0) == WAIT_OBJECT_0)
        bStopRequested = true;

    if (bCapturing && !IsWindow(hwndOBS))
    {
        hwndOBS = NULL;
        bStopRequested = true;
    }

    if (bStopRequested)
    {
        ClearD3D8Data();
        bCapturing = false;
        bStopRequested = false;
    }

    if (!bCapturing && WaitForSingleObject(hSignalRestart, 0) == WAIT_OBJECT_0)
    {
        hwndOBS = FindWindow(OBS_WINDOW_CLASS, NULL);
        if (hwndOBS) {
            logOutput << CurrentTimeString() << "received restart event, capturing" << endl;
            bCapturing = true;
        }
        else {
            logOutput << CurrentTimeString() << "received restart event, but couldn't find window" << endl;
        }
    }

    if (!bHasTextures && bCapturing)
    {
        // initialize shared memory and texture buffers for CPU capture
        if (hwndOBS && bTargetAcquired)
        {
            if (!CreateCPUCapture(device))
                ClearD3D8Data();
            else
                bHasTextures = true;
        }
    }

    LONGLONG timeVal = OSGetTimeMicroseconds();

    if (bHasTextures)
    {
        //check keep alive state, dumb but effective
        // if we are not capturing, we exit the function
        if (bCapturing)
        {
            if (!keepAliveTime)
                keepAliveTime = timeVal;

            if ((timeVal - keepAliveTime) > 5000000)
            {
                HANDLE hKeepAlive = OpenEvent(EVENT_ALL_ACCESS, FALSE, strKeepAlive.c_str());
                if (hKeepAlive) {
                    CloseHandle(hKeepAlive);
                }
                else {
                    logOutput << CurrentTimeString() << "Keepalive no longer found on d3d8, freeing capture data" << endl;
                    ClearD3D8Data();
                    bCapturing = false;
                }

                keepAliveTime = timeVal;
            }
        }
        else
        {
            RUNEVERYRESET logOutput << CurrentTimeString() + "no longer capturing, terminating d3d8 capture" << endl;
            ClearD3D8Data();
            return;
        }

        // do the CPU capture thing
        if (!copyData)
            return;

        LONGLONG frameTime = copyData->frameTime;
        if (frameTime == 0)
            return;

        // copy backbuffer
        IDirect3DSurface8* backBuffer;
        //if (FAILED(hErr = device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
        if (FAILED(hErr = device->GetRenderTarget(&backBuffer)))
        {
            printD3Derror("CaptureD3D8, device->GetRenderTarget", hErr);
            return;
        }

        for (int i = 0; i < NUM_BUFFERS; i++)
        {
            if (!targetLocked[i])
            {
                if (FAILED(hErr = device->CopyRects(backBuffer, NULL, 0, targetBuffers[i], NULL)))
                {
                    printD3Derror("CaptureD3D8, device->CopyRects", hErr);
                    return;
                }

                D3DLOCKED_RECT lr;
                if (SUCCEEDED(targetBuffers[i]->LockRect(&lr, NULL, D3DLOCK_READONLY)))
                {
                    targetLocked[i] = true;
                    pixelData = lr.pBits;
                    curCPUTexture = i;

                    SetEvent(hCopyEvent);
                }
            }
        }

        backBuffer->Release();

        LONGLONG timeElapsed = timeVal - lastTime;
        if (timeElapsed >= frameTime)
        {
            lastTime += frameTime;
            if (timeElapsed > frameTime * 2)
                lastTime = timeVal;
        }
        else
            return;

        DWORD nextCapture = (curCapture == NUM_BUFFERS - 1) ? 0 : (curCapture + 1);
        if (targetLocked[nextCapture])
        {
            OSEnterMutex(dataMutex[nextCapture]);

            targetBuffers[nextCapture]->UnlockRect();
            targetLocked[nextCapture] = false;

            OSLeaveMutex(dataMutex[nextCapture]);
        }

        curCapture = nextCapture;
    }
}

bool CreateCPUCapture(IDirect3DDevice8* device)
{
    HRESULT hr;

    for (int i = 0; i < NUM_BUFFERS; i++)
    {
        // used for copying and formatting pixel data
        if (FAILED(hr = device->CreateImageSurface(d3d8CaptureInfo.cx, d3d8CaptureInfo.cy, pixelFormat, &targetBuffers[i])))
        {
            RUNEVERYRESET printD3Derror("CreateCPUCapture, device->CreateImageSurface", hr);
            return false;
        }

        if (!(dataMutex[i] = OSCreateMutex()))
        {
            RUNEVERYRESET logOutput << CurrentTimeString() << "CreateCPUCapture: OSCreateMutex " << i << " failed, GetLastError = " << GetLastError() << endl;
            return false;
        }
    }

    D3DLOCKED_RECT lr;
    if (FAILED(hr = targetBuffers[0]->LockRect(&lr, NULL, D3DLOCK_READONLY)))
    {
        RUNEVERYRESET printD3Derror("CreateCPUCapture, targetBuffers[0]->LockRect", hr);
        return false;
    }
    sourcePitch = lr.Pitch;
    targetBuffers[0]->UnlockRect();

    // start copy thread
    bKillThread = false;
    if (hCopyThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CopyD3D8CPUTextureThread, NULL, CREATE_SUSPENDED, NULL))
    {
        if (!(hCopyEvent = CreateEvent(NULL, FALSE, FALSE, NULL)))
        {
            RUNEVERYRESET logOutput << CurrentTimeString() + "CreateCPUCapture: CreateEvent failed, GetLastError = " << GetLastError() << endl;
            return false;
        }

        ResumeThread(hCopyThread);
    }
    else
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "CreateCPUCapture: CreateThread failed, GetLastError = " << GetLastError() << endl;
        return false;
    }

    // initialize OBS shared memory
    d3d8CaptureInfo.mapID = InitializeSharedMemoryCPUCapture(d3d8CaptureInfo.pitch*d3d8CaptureInfo.cy, &d3d8CaptureInfo.mapSize, &copyData, textureBuffers);
    if (!d3d8CaptureInfo.mapID)
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "CreateCPUCapture: failed to initialize shared memory" << endl;
        return false;
    }

    if (IsWindow(hwndOBS))
    {
        // ready for capture
        bHasTextures = true;
        d3d8CaptureInfo.captureType = CAPTURETYPE_MEMORY;
        d3d8CaptureInfo.bFlip = FALSE;

        memcpy(infoMem, &d3d8CaptureInfo, sizeof(CaptureInfo));
        SetEvent(hSignalReady);

        logOutput << CurrentTimeString() << "CreateCPUCapture: success" << endl;

        return true;
    }
    else
        return false;
}

void ClearD3D8Data()
{
    bHasTextures = false;

    if (copyData)
        copyData->lastRendered = -1;

    if (hCopyThread)
    {
        bKillThread = true;
        SetEvent(hCopyEvent);
        if (WaitForSingleObject(hCopyThread, 500) != WAIT_OBJECT_0)
            TerminateThread(hCopyThread, -1);

        CloseHandle(hCopyThread);
        CloseHandle(hCopyEvent);

        hCopyThread = NULL;
        hCopyEvent = NULL;
    }

    for (int i = 0; i < NUM_BUFFERS; i++)
    {
        if (targetBuffers[i])
        {
            if (targetLocked[i])
            {
                OSEnterMutex(dataMutex[i]);

                targetBuffers[i]->UnlockRect();
                targetLocked[i] = false;

                OSLeaveMutex(dataMutex[i]);
            }

            targetBuffers[i]->Release();
            targetBuffers[i] = NULL;
        }
    }

    for (int i = 0; i < NUM_BUFFERS; i++)
    {
        if (dataMutex[i])
        {
            OSCloseMutex(dataMutex[i]);
            dataMutex[i] = NULL;
        }
    }

    DestroySharedMemory();
    copyData = NULL;
    pixelFormat = D3DFMT_UNKNOWN;
    sourcePitch = 0;
    curCPUTexture = 0;
    curCapture = 0;
    keepAliveTime = 0;
    resetCount++;
    pixelData = NULL;
    lastTime = 0L;
    lpCurrentDevice = NULL;

    bTargetAcquired = false;

    logOutput << CurrentTimeString() + "---------------------- Cleared D3D8 Capture ----------------------" << endl;
}

bool InitD3D8Capture()
{
    TCHAR lpD3D8Path[MAX_PATH];
    SHGetFolderPath(NULL, CSIDL_SYSTEM, NULL, SHGFP_TYPE_CURRENT, lpD3D8Path);
    wcscat_s(lpD3D8Path, MAX_PATH, TEXT("\\d3d8.dll"));

    bool versionSupported = false;
    HMODULE hD3D8Dll = NULL;
    if (hD3D8Dll = GetModuleHandle(lpD3D8Path))
    {
        bool isWinVistaMin = IsWindowsVistaOrGreater();
        bool isWin7min = IsWindows7OrGreater();
        bool isWin8min = IsWindows8OrGreater();
        /*
        HRESULT hr;
        OSVERSIONINFO vInfo;
        vInfo.dwOSVersionInfoSize = sizeof(vInfo);
        if (FAILED(hr = GetVersionEx(&vInfo)))
        {
        RUNEVERYRESET logOutput << CurrentTimeString() << "Failed to get OS version, GetLastError = " << GetLastError() << endl;
        return false;
        }
        */

        UPARAM libBaseAddr = UPARAM(hD3D8Dll);
        UPARAM devicePresentOffset;
        UPARAM deviceEndSceneOffset;

        if (isWinVistaMin)
        {
            if (!isWin7min)
            {
                RUNEVERYRESET logOutput << CurrentTimeString() << "Windows Vista not supported yet" << endl;
            }
            else if (isWin7min && !isWin8min)
            {
                deviceEndSceneOffset = 0x44530;
                devicePresentOffset = 0x662e0;
                versionSupported = true;
            }
            else if (isWin8min)
            {
                deviceEndSceneOffset = 0x35540;
                devicePresentOffset = 0x5b520;
                versionSupported = true;
            }
            else
            {
                RUNEVERYRESET logOutput << CurrentTimeString() << "Unknown OS version" << endl;
            }
        }
        else
        {
            RUNEVERYRESET logOutput << CurrentTimeString() << "OS version not supported" << endl;
        }

        if (versionSupported)
        {
            //d3d8Present.Hook((FARPROC)(libBaseAddr + devicePresentOffset), (FARPROC)D3D8Present);
            d3d8EndScene.Hook((FARPROC)(libBaseAddr + deviceEndSceneOffset), (FARPROC)D3D8EndScene);

            //d3d8Present.Rehook();
            d3d8EndScene.Rehook();
        }
    }

    return versionSupported;
}

void CheckD3D8Capture()
{
    EnterCriticalSection(&d3d8EndMutex);
    d3d8EndScene.Rehook(true);
    LeaveCriticalSection(&d3d8EndMutex);
}
