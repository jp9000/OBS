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
#include <ddraw.h>
#include <VersionHelpers.h>

HookData ddrawSurfaceCreate;
HookData ddrawSurfaceFlip;
HookData ddrawSurfaceRelease;
HookData ddrawSurfaceRestore;
HookData ddrawSurfaceSetPalette;
HookData ddrawSurfaceBlt;
HookData ddrawSurfaceBltFast;
HookData ddrawSurfaceLock;
HookData ddrawSurfaceUnlock;
HookData ddrawPaletteSetEntries;

CaptureInfo ddrawCaptureInfo;

#define NUM_BUFFERS 3
#define ZERO_ARRAY {0, 0, 0}

extern CRITICAL_SECTION ddrawMutex;
extern LPBYTE           textureBuffers[2];
extern MemoryCopyData   *copyData;
extern DWORD            curCapture;
extern HANDLE           hCopyThread;
extern HANDLE           hCopyEvent;
extern BOOL             bHasTextures;
extern bool             bKillThread;
extern DWORD            curCPUTexture;
extern DWORD            copyWait;
extern LONGLONG         lastTime;

bool bGotSurfaceDesc = false;
bool bLockingSurface = false;

DWORD g_dwSize = 0; // bytesize of buffers
DWORD g_dwCaptureSize = 0; // bytesize of memory capture buffer

LPDIRECTDRAWSURFACE7    ddCaptures[NUM_BUFFERS] = ZERO_ARRAY;
HANDLE                  ddUnlockFctMutex = 0;
LPDIRECTDRAWSURFACE7    g_frontSurface = NULL;
LPDDSURFACEDESC2        g_surfaceDesc;
LPDIRECTDRAW7           g_ddInterface = NULL;
LPCTSTR                 mutexName = TEXT("Global\\ddUnlockFunctionMutex");

BOOL g_bUseFlipMethod = false;
BOOL g_bUse32bitCapture = false;
BOOL g_bConvert16to32 = false;
BOOL g_bUsePalette = false;

HRESULT STDMETHODCALLTYPE Blt(LPDIRECTDRAWSURFACE7, LPRECT, LPDIRECTDRAWSURFACE7, LPRECT, DWORD, LPDDBLTFX);
ULONG STDMETHODCALLTYPE Release(LPDIRECTDRAWSURFACE7 surface);
void HookAll();
void UnhookAll();

unsigned int getCodeFromHRESULT(HRESULT hr)
{
    return (unsigned int)(((unsigned long)0xFFFF) & hr);
}

void printDDrawError(HRESULT err, const char* callerName = "")
{
    if (err == DD_OK)
        return;

    std::string s;
    switch (err)
    {
    case DDERR_CANTCREATEDC: s = "can't create dc"; break;
    case DDERR_CANTLOCKSURFACE: s = "can't lock surface"; break;
    case DDERR_CLIPPERISUSINGHWND: s = "clipper is using hwnd"; break;
    case DDERR_COLORKEYNOTSET: s = "color key not set"; break;
    case DDERR_CURRENTLYNOTAVAIL: s = "currently not available"; break;
    case DDERR_DCALREADYCREATED: s = "dc already created"; break;
    case DDERR_DEVICEDOESNTOWNSURFACE: s = "device does not own surface"; break;
    case DDERR_DIRECTDRAWALREADYCREATED: s = "direct draw already created"; break;
    case DDERR_EXCLUSIVEMODEALREADYSET: s = "exclusive mode already set"; break;
    case DDERR_GENERIC: s = "undefined error"; break;
    case DDERR_HEIGHTALIGN: s = "height alignment is not a multiple of required alignment"; break;
    case DDERR_IMPLICITLYCREATED: s = "cannot restore surface, implicitly created"; break;
    case DDERR_INCOMPATIBLEPRIMARY: s = "incompatible primary"; break;
    case DDERR_INVALIDCAPS: s = "invalid caps"; break;
    case DDERR_INVALIDCLIPLIST: s = "invalid cliplist"; break;
    case DDERR_INVALIDMODE: s = "invalid mode"; break;
    case DDERR_INVALIDOBJECT: s = "invalid object"; break;
    case DDERR_INVALIDPARAMS: s = "invalid params"; break;
    case DDERR_INVALIDPIXELFORMAT: s = "invalid pixel format"; break;
    case DDERR_INVALIDPOSITION: s = "invalid position"; break;
    case DDERR_INVALIDRECT: s = "invalid rect"; break;
    case DDERR_LOCKEDSURFACES: s = "surface(s) locked"; break;
    case DDERR_MOREDATA: s = "data size exceeds buffer size"; break;
    case DDERR_NOALPHAHW: s = "no alpha acceleration hw available"; break;
    case DDERR_NOBLTHW: s = "no hw blit available"; break;
    case DDERR_NOCLIPLIST: s = "no cliplist available"; break;
    case DDERR_NOCLIPPERATTACHED: s = "no clipper attached"; break;
    case DDERR_NOCOLORCONVHW: s = "no color conversion hw available"; break;
    case DDERR_NOCOLORKEYHW: s = "no hw support for dest color key"; break;
    case DDERR_NOCOOPERATIVELEVELSET: s = "no cooperative level set"; break;
    case DDERR_NODC: s = "no dc available"; break;
    case DDERR_NOEXCLUSIVEMODE: s = "application does not have exclusive mode"; break;
    case DDERR_NOFLIPHW: s = "no hw flip available"; break;
    case DDERR_NOOVERLAYDEST: s = "GetOverlayPosition() is called but UpdateOverlay() has not been called"; break;
    case DDERR_NOOVERLAYHW: s = "no hw overlay available"; break;
    case DDERR_NOPALETTEATTACHED: s = "no palette attached"; break;
    case DDERR_NORASTEROPHW: s = "no raster operation hw available"; break;
    case DDERR_NOSTRETCHHW: s = "no hw support for stretching available"; break;
    case DDERR_NOTAOVERLAYSURFACE: s = "not an overlay component"; break;
    case DDERR_NOTFLIPPABLE: s = "surface not flippable"; break;
    case DDERR_NOTFOUND: s = "item not found"; break;
    case DDERR_NOTLOCKED: s = "surface not locked"; break;
    case DDERR_NOTPALETTIZED: s = "surface is not palette-based"; break;
    case DDERR_NOVSYNCHW: s = "no vsync hw available"; break;
    case DDERR_NOZOVERLAYHW: s = "not supported (NOZOVERLAYHW)"; break;
    case DDERR_OUTOFCAPS: s = "hw needed has already been allocated"; break;
    case DDERR_OUTOFMEMORY: s = "out of memory"; break;
    case DDERR_OUTOFVIDEOMEMORY: s = "out of video memory"; break;
    case DDERR_OVERLAPPINGRECTS: s = "overlapping rects on surface"; break;
    case DDERR_OVERLAYNOTVISIBLE: s = "GetOverlayPosition() is called on a hidden overlay"; break;
    case DDERR_PALETTEBUSY: s = "palette is locked by another thread"; break;
    case DDERR_PRIMARYSURFACEALREADYEXISTS: s = "primary surface already exists"; break;
    case DDERR_REGIONTOOSMALL: s = "region too small"; break;
    case DDERR_SURFACEBUSY: s = "surface is locked by another thread"; break;
    case DDERR_SURFACELOST: s = "surface lost"; break;
    case DDERR_TOOBIGHEIGHT: s = "requested height too large"; break;
    case DDERR_TOOBIGSIZE: s = "requested size too large"; break;
    case DDERR_TOOBIGWIDTH: s = "requested width too large"; break;
    case DDERR_UNSUPPORTED: s = "operation unsupported"; break;
    case DDERR_UNSUPPORTEDFORMAT: s = "unsupported pixel format"; break;
    case DDERR_VERTICALBLANKINPROGRESS: s = "vertical blank in progress"; break;
    case DDERR_VIDEONOTACTIVE: s = "video port not active"; break;
    case DDERR_WASSTILLDRAWING: s = "blit operation incomplete"; break;
    case DDERR_WRONGMODE: s = "surface cannot be restored because it was created in a different mode"; break;
    default: logOutput << "unknown error: " << getCodeFromHRESULT(err) << endl; return;
    }

    std::stringstream msg;
    msg << CurrentTimeString() << callerName << " DDraw Error: " << s << "\n";
    logOutput << msg.str();
}

class Palette
{
    DWORD numEntries;

    void destroy()
    {
        if (entries)
            delete[] entries;

        if (primary_surface_palette_ref)
            primary_surface_palette_ref->Release();

        entries = NULL;
        numEntries = 0;
        bInitialized = false;
        primary_surface_palette_ref = NULL;
    }

public:
    LPDIRECTDRAWPALETTE primary_surface_palette_ref;
    LPPALETTEENTRY entries;
    BOOL bInitialized;

    Palette()
        : numEntries(0), entries(NULL), primary_surface_palette_ref(NULL)
    {
    }

    ~Palette()
    {
        destroy();
    }

    void Reallocate(DWORD size)
    {
        Free();

        numEntries = size;
        entries = new PALETTEENTRY[size];
        bInitialized = true;
    }

    void Free()
    {
        bool bWait = true;

        // try locking mutexes so we don't accidently free memory used by copythread
        HANDLE mutexes[2];
        for (int i = 0; i < 2; i++)
        {
            if (!DuplicateHandle(GetCurrentProcess(), textureMutexes[i], GetCurrentProcess(), &mutexes[i], NULL, FALSE, DUPLICATE_SAME_ACCESS))
            {
                logOutput << CurrentTimeString() << "Palette::Free(): could not duplicate textureMutex handles, assuming OBS was closed and copy thread finished" << endl;
                bWait = false;
                break;
            }
        }

        // wait 2 seconds before deleting memory
        DWORD ret = WaitForMultipleObjects(2, mutexes, TRUE, 2000);

        destroy();

        if (bWait)
        {
            if (ret == WAIT_OBJECT_0)
            {
                ReleaseMutex(mutexes[0]);
                ReleaseMutex(mutexes[1]);
            }

            CloseHandle(mutexes[0]);
            CloseHandle(mutexes[1]);
        }
    }

    void ReloadPrimarySurfacePaletteEntries()
    {
        if (!primary_surface_palette_ref)
            return;

        HRESULT err;
        ddrawPaletteSetEntries.Unhook();
        if (FAILED(err = primary_surface_palette_ref->SetEntries(0, 0, numEntries, entries)))
        {
            logOutput << CurrentTimeString() << "ReloadPrimarySurfacePaletteEntries(): could not set entires" << endl;
            printDDrawError(err);
        }
        ddrawPaletteSetEntries.Rehook();
    }

} g_CurrentPalette;

bool SetupPalette(LPDIRECTDRAWPALETTE lpDDPalette)
{
    if (!lpDDPalette)
    {
        //logOutput << CurrentTimeString() << "Detaching palette" << endl;
        g_CurrentPalette.Free();
        g_bUsePalette = false;
        return false;
    }

    logOutput << CurrentTimeString() << "initializing palette" << endl;

    DWORD caps;
    HRESULT hr;
    if (FAILED(hr = lpDDPalette->GetCaps(&caps)))
    {
        logOutput << CurrentTimeString() << "Failed to get palette caps" << endl;
        printDDrawError(hr, "SetupPalette");
        return false;
    }

    if (caps & DDPCAPS_8BITENTRIES)
    {
        logOutput << CurrentTimeString() << "8-bit index-palette used (lookup color is an index to another lookup table), not implemented" << endl;
        return false;
    }

    DWORD numEntries = 0;
    if (caps & DDPCAPS_8BIT)
        numEntries = 0x100;
    else if (caps & DDPCAPS_4BIT)
        numEntries = 0x10;
    else if (caps & DDPCAPS_2BIT)
        numEntries = 0x04;
    else if (caps & DDPCAPS_1BIT)
        numEntries = 0x02;
    else
    {
        logOutput << CurrentTimeString() << "Unrecognized palette format" << endl;
        return false;
    }

    //logOutput << CurrentTimeString() << "Trying to retrieve " << numEntries << " palette entries" << endl;

    g_CurrentPalette.Reallocate(numEntries);
    hr = lpDDPalette->GetEntries(0, 0, numEntries, g_CurrentPalette.entries);
    if (FAILED(hr))
    {
        logOutput << CurrentTimeString() << "Failed to retrieve palette entries" << endl;
        printDDrawError(hr, "SetupPalette");
        g_CurrentPalette.Free();
        return false;
    }

    g_CurrentPalette.primary_surface_palette_ref = lpDDPalette;
    g_CurrentPalette.bInitialized = true;
    g_bUsePalette = true;

    logOutput << CurrentTimeString() << "Initialized palette with " << numEntries << " color entries" << endl;

    return true;
}

void CleanUpDDraw()
{
    logOutput << CurrentTimeString() << "Cleaning up" << endl;
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

    ddrawSurfaceRelease.Unhook();
    for (int i = 0; i < NUM_BUFFERS; i++)
    {
        if (ddCaptures[i])
        {
            ddCaptures[i]->Release();
            ddCaptures[i] = NULL;
        }
    }
    ddrawSurfaceRelease.Rehook();

    DestroySharedMemory();

    bHasTextures = false;
    curCapture = 0;
    curCPUTexture = 0;
    keepAliveTime = 0;
    resetCount++;
    copyWait = 0;
    lastTime = 0;
    g_frontSurface = NULL;
    g_bUseFlipMethod = false;
    bTargetAcquired = false;
    g_dwSize = 0;
    g_bUse32bitCapture = false;
    g_bConvert16to32 = false;
    g_bUsePalette = false;
    g_dwCaptureSize = 0;

    if (g_ddInterface)
    {
        g_ddInterface->Release();
        g_ddInterface = NULL;
    }

    g_CurrentPalette.Free();

    if (g_surfaceDesc)
        delete g_surfaceDesc;
    g_surfaceDesc = NULL;

    if (ddUnlockFctMutex)
    {
        CloseHandle(ddUnlockFctMutex);
        ddUnlockFctMutex = 0;
    }

    //UnhookAll();

    logOutput << CurrentTimeString() << "---------------------- Cleared DirectDraw Capture ----------------------" << endl;
}

void handleBufferConversion(LPDWORD dst, LPBYTE src, LONG pitch)
{
    //logOutput << CurrentTimeString() << "trying to download buffer" << endl;

    DWORD advance = 0;

    if (g_bUsePalette)
    {
        if (!g_CurrentPalette.bInitialized)
        {
            logOutput << "current palette not initialized" << endl;
            return;
        }

        //logOutput << "Using palette" << endl;

        advance = pitch - g_surfaceDesc->dwWidth;

        for (UINT y = 0; y < ddrawCaptureInfo.cy; y++)
        {
            for (UINT x = 0; x < ddrawCaptureInfo.cx; x++)
            {
                // use color lookup table
                const PALETTEENTRY& entry = g_CurrentPalette.entries[*src];
                *dst = 0xFF000000 + (entry.peRed << 16) + (entry.peGreen << 8) + entry.peBlue;

                src++;
                dst++;
            }
            src += advance;
        }
    }
    else if (g_bConvert16to32)
    {
        //logOutput << "Converting 16bit R5G6B5 to 32bit ARGB" << endl;

        advance = pitch / 2 - g_surfaceDesc->dwWidth;

        LPWORD buf = (LPWORD)src;
        for (UINT y = 0; y < ddrawCaptureInfo.cy; y++)
        {
            for (UINT x = 0; x < ddrawCaptureInfo.cx; x++)
            {
                // R5G6B5 format
                WORD pxl = *buf;
                *dst = 0xFF000000 + ((0xF800 & pxl) << 8) + ((0x07E0 & pxl) << 5) + ((0x001F & pxl) << 3);

                buf++;
                dst++;
            }
            buf += advance;
        }
    }
    else if (g_bUse32bitCapture)
    {
        // no conversion needed
        UINT width = 4 * g_surfaceDesc->dwWidth;
        for (UINT y = 0; y < ddrawCaptureInfo.cy; y++)
        {
            memcpy(dst, src, width);
            dst += g_surfaceDesc->dwWidth;
            src += g_surfaceDesc->lPitch;
        }
    }
}

inline HRESULT STDMETHODCALLTYPE UnlockSurface(LPDIRECTDRAWSURFACE7 surface, LPRECT data)
{
    //logOutput << CurrentTimeString() << "Called UnlockSurface" << endl;

    // standard handler
    if (!bTargetAcquired)
    {
        ddrawSurfaceUnlock.Unhook();
        HRESULT hr = surface->Unlock(data);
        ddrawSurfaceUnlock.Rehook();
        return hr;
    }

    // use mutex lock to prevent memory corruption when calling Unhook/Rehook from multiple threads
    HANDLE mutex = OpenMutex(SYNCHRONIZE, FALSE, mutexName);
    if (!mutex)
    {
        logOutput << CurrentTimeString() << "Could not open mutex - Error(" << GetLastError() << ')' << endl;
        return DDERR_GENERIC;
    }

    DWORD ret = WaitForSingleObject(mutex, INFINITE);
    if (ret == WAIT_OBJECT_0)
    {
        ddrawSurfaceUnlock.Unhook();
        HRESULT hr = surface->Unlock(data);
        ddrawSurfaceUnlock.Rehook();

        ReleaseMutex(mutex);
        CloseHandle(mutex);
        return hr;
    }
    else
    {
        logOutput << CurrentTimeString() << "error while waiting for unlock-mutex to get signaled" << endl;
        logOutput << CurrentTimeString() << "GetLastError: " << GetLastError() << endl;
        CloseHandle(mutex);
        return DDERR_GENERIC;
    }
}

/*
void CopyTextures(LPDIRECTDRAWSURFACE7 src)
{
static int sharedMemID = 0;
int nextSharedMemID = sharedMemID == 0 ? 1 : 0;

DWORD copyTex = curCPUTexture;
if(copyTex < NUM_BUFFERS)
{
//logOutput << CurrentTimeString() << "CopyDDrawTextureThread: processing data" << endl;
int lastRendered = -1;

//copy to whichever is available
if(WaitForSingleObject(textureMutexes[sharedMemID], 0) == WAIT_OBJECT_0)
lastRendered = (int)sharedMemID;
else if(WaitForSingleObject(textureMutexes[nextSharedMemID], 0) == WAIT_OBJECT_0)
lastRendered = (int)nextSharedMemID;

if(lastRendered != -1)
{
//logOutput << CurrentTimeString() << "CopyDDrawTextureThread: copying to memfile" << endl;
HRESULT err;
DDSURFACEDESC2 desc;
desc.dwSize = sizeof(desc);
DWORD flags = DDLOCK_WAIT | DDLOCK_READONLY;
logOutput << CurrentTimeString() << "CopyTexture: locking surface" << endl;
if(SUCCEEDED(err = src->Lock(NULL, &desc, flags, NULL)))
{
logOutput << CurrentTimeString() << "CopyTexture: converting buffer" << endl;
handleBufferConversion((LPDWORD)textureBuffers[lastRendered], (LPBYTE)desc.lpSurface, desc.lPitch);
logOutput << CurrentTimeString() << "CopyTexture: unlocking surface" << endl;
UnlockSurface(src, NULL);
logOutput << CurrentTimeString() << "CopyTexture: done" << endl;
}
else
{
RUNEVERYRESET logOutput << CurrentTimeString() << "CopyDDrawTextureThread: failed to lock capture surface" << endl;
printDDrawError(err);
}
ReleaseMutex(textureMutexes[lastRendered]);
copyData->lastRendered = (UINT)lastRendered;
}
}

sharedMemID = nextSharedMemID;
//logOutput << CurrentTimeString() << "CopyDDrawTextureThread: waiting for event" << endl;
}
*/

void CaptureDDraw()
{
    //RUNEVERYRESET logOutput << CurrentTimeString() << "called CaptureDDraw()" << endl;

    if (bCapturing && WaitForSingleObject(hSignalEnd, 0) == WAIT_OBJECT_0)
    {
        //logOutput << CurrentTimeString() << "not capturing or received hSignalEnd" << endl;
        bStopRequested = true;
    }

    if (bCapturing && !IsWindow(hwndOBS))
    {
        //logOutput << CurrentTimeString() << "not capturing or OBS window not found" << endl;
        hwndOBS = NULL;
        bStopRequested = true;
    }

    if (bStopRequested)
    {
        CleanUpDDraw();
        bCapturing = false;
        bStopRequested = false;
    }

    if (!bCapturing && WaitForSingleObject(hSignalRestart, 0) == WAIT_OBJECT_0)
    {
        //logOutput << CurrentTimeString() << "capturing and received hSignalRestart" << endl;
        hwndOBS = FindWindow(OBS_WINDOW_CLASS, NULL);
        if (hwndOBS) {
            logOutput << CurrentTimeString() << "received restart event, capturing" << endl;
            bCapturing = true;
        }
        else {
            logOutput << CurrentTimeString() << "received restart event, but couldn't find window" << endl;
        }
    }

    LONGLONG timeVal = OSGetTimeMicroseconds();

    //check keep alive state, dumb but effective
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
                logOutput << CurrentTimeString() << "Keepalive no longer found on ddraw, freeing capture data" << endl;
                CleanUpDDraw();
                bCapturing = false;
            }

            keepAliveTime = timeVal;
        }
    }

    if (bHasTextures)
    {
        LONGLONG frameTime;
        if (bCapturing)
        {
            if (copyData)
            {
                if (frameTime = copyData->frameTime)
                {
                    LONGLONG timeElapsed = timeVal - lastTime;

                    if (timeElapsed >= frameTime)
                    {
                        lastTime += frameTime;
                        if (timeElapsed > frameTime * 2)
                            lastTime = timeVal;


                        //logOutput << CurrentTimeString() << "CaptureDDraw: capturing screen from 0x" << g_frontSurface << endl;

                        HRESULT hr;
                        ddrawSurfaceBlt.Unhook();
                        hr = ddCaptures[curCapture]->Blt(NULL, g_frontSurface, NULL, DDBLT_ASYNC, NULL);
                        ddrawSurfaceBlt.Rehook();
                        if (SUCCEEDED(hr))
                        {
                            DWORD nextCapture = (curCapture == NUM_BUFFERS - 1) ? 0 : (curCapture + 1);
                            curCPUTexture = curCapture;
                            curCapture = nextCapture;

                            SetEvent(hCopyEvent);
                        }
                        else
                        {
                            printDDrawError(hr, "CaptureDDraw");
                        }
                        //logOutput << CurrentTimeString() << "CaptureDDraw: finished capturing" << endl;
                    }
                }
            }
        }
    }
}

DWORD CopyDDrawTextureThread(LPVOID lpUseless)
{
    int sharedMemID = 0;

    HANDLE hEvent = NULL;
    if (!DuplicateHandle(GetCurrentProcess(), hCopyEvent, GetCurrentProcess(), &hEvent, NULL, FALSE, DUPLICATE_SAME_ACCESS))
    {
        logOutput << CurrentTimeString() << "CopyDDrawTextureThread: couldn't duplicate event handle - " << GetLastError() << endl;
        return 0;
    }

    std::stringstream msg;
    msg << CurrentTimeString() << "CopyDDrawTextureThread: waiting for copyEvents\n";
    logOutput << msg.str();
    msg.str("");
    while (WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0)
    {
        //logOutput << CurrentTimeString() << "CopyDDrawTextureThread: received copyEvent" << endl;
        if (bKillThread)
            break;

        int nextSharedMemID = sharedMemID == 0 ? 1 : 0;

        DWORD copyTex = curCPUTexture;
        if (copyTex < NUM_BUFFERS)
        {
            //logOutput << CurrentTimeString() << "CopyDDrawTextureThread: processing data" << endl;
            int lastRendered = -1;

            //copy to whichever is available
            if (WaitForSingleObject(textureMutexes[sharedMemID], 0) == WAIT_OBJECT_0)
                lastRendered = (int)sharedMemID;
            else if (WaitForSingleObject(textureMutexes[nextSharedMemID], 0) == WAIT_OBJECT_0)
                lastRendered = (int)nextSharedMemID;

            if (lastRendered != -1)
            {
                //logOutput << CurrentTimeString() << "CopyDDrawTextureThread: copying to memfile" << endl;
                HRESULT err;
                DDSURFACEDESC2 desc;
                desc.dwSize = sizeof(desc);
                DWORD flags = DDLOCK_WAIT | DDLOCK_READONLY | DDLOCK_NOSYSLOCK;
                //logOutput << CurrentTimeString() << "CopyDDrawTextureThread: locking buffer" << endl;
                if (SUCCEEDED(err = ddCaptures[copyTex]->Lock(NULL, &desc, flags, NULL)))
                {
                    //logOutput << CurrentTimeString() << "CopyDDrawTextureThread: converting buffer" << endl;
                    handleBufferConversion((LPDWORD)textureBuffers[lastRendered], (LPBYTE)desc.lpSurface, desc.lPitch);
                    //logOutput << CurrentTimeString() << "CopyDDrawTextureThread: unlocking buffer" << endl;
                    if (FAILED(err = ddCaptures[copyTex]->Unlock(NULL)))
                    {
                        printDDrawError(err, "CopyDDrawTextureThread");
                    }
                    /*
                    else
                    logOutput << CurrentTimeString() << "CopyDDrawTextureThread: done" << endl;
                    */
                    //logOutput << CurrentTimeString() << "CopyDDrawTextureThread: finished uploading" << endl;
                }
                else
                {
                    RUNEVERYRESET logOutput << CurrentTimeString() << "CopyDDrawTextureThread: failed to lock capture surface" << endl;
                    printDDrawError(err, "CopyDDrawTextureThread");
                }
                ReleaseMutex(textureMutexes[lastRendered]);
                copyData->lastRendered = (UINT)lastRendered;
            }
        }

        sharedMemID = nextSharedMemID;
        //logOutput << CurrentTimeString() << "CopyDDrawTextureThread: waiting for event" << endl;
    }

    msg << CurrentTimeString() << "CopyDDrawTextureThread: killed\n";
    logOutput << msg.str();

    CloseHandle(hEvent);
    return 0;
}

bool SetupDDraw()
{
    logOutput << CurrentTimeString() << "called SetupDDraw()" << endl;

    if (!g_ddInterface)
    {
        logOutput << CurrentTimeString() << "SetupDDraw: DirectDraw interface not set, returning" << endl;
        return false;
    }

    bool bSuccess = true;

    bKillThread = false;

    if (hCopyThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CopyDDrawTextureThread, NULL, 0, NULL))
    {
        if (!(hCopyEvent = CreateEvent(NULL, FALSE, FALSE, NULL)))
        {
            logOutput << CurrentTimeString() << "SetupDDraw: CreateEvent failed, GetLastError = " << GetLastError() << endl;
            bSuccess = false;
        }
    }
    else
    {
        logOutput << CurrentTimeString() << "SetupDDraw: CreateThread failed, GetLastError = " << GetLastError() << endl;
        bSuccess = false;
    }

    if (bSuccess)
    {
        if (!ddUnlockFctMutex)
        {
            ddUnlockFctMutex = CreateMutex(NULL, FALSE, mutexName);
            if (!ddUnlockFctMutex)
            {
                RUNEVERYRESET logOutput << CurrentTimeString() << "SetupDDraw: CreateMutex failed, GetLastError = " << GetLastError() << endl;
                bSuccess = false;
            }
        }
    }

    if (bSuccess && !g_frontSurface)
    {
        RUNEVERYRESET logOutput << "SetupDDraw: frontSurface and surface descriptor not set, returning" << endl;
        CleanUpDDraw();
        return false;
    }
    else if (bSuccess)
    {
        LPDIRECTDRAWPALETTE palette = NULL;
        HRESULT err;
        if (SUCCEEDED(err = g_frontSurface->GetPalette(&palette)))
        {
            if (palette)
                SetupPalette(palette);
        }
        else if (err == DDERR_NOPALETTEATTACHED)
        {
            //logOutput << CurrentTimeString() << "No palette attached to primary surface" << endl;
        }
        else
        {
            logOutput << CurrentTimeString() << "Error retrieving palette" << endl;
            printDDrawError(err, "getFrontSurface");
        }
    }

    if (bSuccess && !g_surfaceDesc)
    {
        logOutput << CurrentTimeString() << "SetupDDraw: no surface descriptor found, creating a new one (not an error)" << endl;

        g_surfaceDesc = new DDSURFACEDESC2;
        g_surfaceDesc->dwSize = sizeof(DDSURFACEDESC);

        HRESULT hr;
        if (FAILED(hr = ((LPDIRECTDRAWSURFACE)g_frontSurface)->GetSurfaceDesc((LPDDSURFACEDESC)g_surfaceDesc)))
        {
            g_surfaceDesc->dwSize = sizeof(DDSURFACEDESC2);
            if (FAILED(g_frontSurface->GetSurfaceDesc(g_surfaceDesc)))
            {
                logOutput << CurrentTimeString() << "SetupDDraw: error getting surface descriptor" << endl;
                printDDrawError(hr, "SetupDDraw");
                bSuccess = false;
            }
        }
    }

    if (bSuccess && g_surfaceDesc)
    {
        const DDPIXELFORMAT& pf = g_surfaceDesc->ddpfPixelFormat;
        if (pf.dwFlags & DDPF_RGB)
        {
            if (pf.dwRGBBitCount == 16)
            {
                logOutput << CurrentTimeString() << "SetupDDraw: found 16bit format (using R5G6B5 conversion)" << endl;
                g_bConvert16to32 = true;
            }
            else if (pf.dwRGBBitCount == 32)
            {
                logOutput << CurrentTimeString() << "SetupDDraw: found 32bit format (using plain copy)" << endl;
                g_bUse32bitCapture = true;
            }
        }
        else if (pf.dwFlags & (DDPF_PALETTEINDEXED8 | DDPF_PALETTEINDEXED4 | DDPF_PALETTEINDEXED2 | DDPF_PALETTEINDEXED1))
        {
            logOutput << CurrentTimeString() << "SetupDDraw: front surface uses palette indices" << endl;
        }
    }

    if (bSuccess)
    {
        logOutput << CurrentTimeString() << "SetupDDraw: primary surface width = " << g_surfaceDesc->dwWidth << ", height = " << g_surfaceDesc->dwHeight << endl;
        g_dwSize = g_surfaceDesc->lPitch*g_surfaceDesc->dwHeight;
        ddrawCaptureInfo.captureType = CAPTURETYPE_MEMORY;
        ddrawCaptureInfo.cx = g_surfaceDesc->dwWidth;
        ddrawCaptureInfo.cy = g_surfaceDesc->dwHeight;
        ddrawCaptureInfo.pitch = 4 * ddrawCaptureInfo.cx;
        ddrawCaptureInfo.hwndCapture = (DWORD)hwndSender;
        ddrawCaptureInfo.format = GS_BGRA;
        DWORD g_dwCaptureSize = ddrawCaptureInfo.pitch*ddrawCaptureInfo.cy;
        ddrawCaptureInfo.bFlip = FALSE;
        ddrawCaptureInfo.mapID = InitializeSharedMemoryCPUCapture(g_dwCaptureSize, &ddrawCaptureInfo.mapSize, &copyData, textureBuffers);

        memcpy(infoMem, &ddrawCaptureInfo, sizeof(CaptureInfo));

        DDSURFACEDESC2 captureDesc;
        ZeroMemory(&captureDesc, sizeof(captureDesc));
        captureDesc.dwSize = sizeof(DDSURFACEDESC2);

        captureDesc.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS | DDSD_PIXELFORMAT | DDSD_PITCH;
        captureDesc.dwWidth = g_surfaceDesc->dwWidth;
        captureDesc.dwHeight = g_surfaceDesc->dwHeight;
        captureDesc.lPitch = g_surfaceDesc->lPitch;
        captureDesc.ddpfPixelFormat = g_surfaceDesc->ddpfPixelFormat;
        captureDesc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;

        HRESULT err;

        ddrawSurfaceCreate.Unhook();
        for (int i = 0; i < NUM_BUFFERS && bSuccess; i++)
        {
            if (FAILED(err = g_ddInterface->CreateSurface(&captureDesc, &ddCaptures[i], NULL)))
            {
                logOutput << CurrentTimeString() << "SetupDDraw: Could not create offscreen capture" << endl;
                printDDrawError(err, "SetupDDraw");
                bSuccess = false;
                break;
            }
        }
        ddrawSurfaceCreate.Rehook();

        if (bSuccess)
        {
            bHasTextures = true;

            SetEvent(hSignalReady);

            OSInitializeTimer();
        }
    }

    if (bSuccess)
    {
        logOutput << CurrentTimeString() << "SetupDDraw successfull" << endl;
        HookAll();
        return true;
    }
    else
    {
        logOutput << CurrentTimeString() << "SetupDDraw failed" << endl;
        CleanUpDDraw();
        return false;
    }
}

// set frontSurface to lpDDSurface if lpDDSurface is primary
// returns true if frontSurface is set
// returns false if frontSurface is NULL and lpDDSurface is not primary
bool getFrontSurface(LPDIRECTDRAWSURFACE7 lpDDSurface)
{
    //logOutput << CurrentTimeString() << "called getFrontSurface" << endl;

    if (!lpDDSurface)
    {
        //logOutput << CurrentTimeString() << "lpDDSurface null" << endl;
        return false;
    }

    if (!g_ddInterface)
    {
        LPDIRECTDRAWSURFACE7 dummy;
        if (lpDDSurface->QueryInterface(IID_IDirectDrawSurface7, (LPVOID*)&dummy) == S_OK)
        {
            IUnknown* Unknown;
            HRESULT err;
            if (FAILED(err = dummy->GetDDInterface((LPVOID*)&Unknown)))
            {
                logOutput << CurrentTimeString() << "getFrontSurface: could not get DirectDraw interface" << endl;
                printDDrawError(err, "getFrontSurface");
            }
            else
            {
                if (Unknown->QueryInterface(IID_IDirectDraw7, (LPVOID*)&g_ddInterface) == S_OK)
                {
                    logOutput << CurrentTimeString() << "Got DirectDraw interface pointer" << endl;
                }
                else
                {
                    logOutput << CurrentTimeString() << "Query of DirectDraw interface failed" << endl;
                }
            }
            ddrawSurfaceRelease.Unhook();
            dummy->Release();
            ddrawSurfaceRelease.Rehook();
        }
    }

    if (!bTargetAcquired)
    {
        DDSCAPS2 caps;
        if (SUCCEEDED(lpDDSurface->GetCaps(&caps)))
        {
            //logOutput << CurrentTimeString() << "checking if surface is primary" << endl;
            if (caps.dwCaps & DDSCAPS_PRIMARYSURFACE)
            {
                logOutput << CurrentTimeString() << "found primary surface" << endl;
                g_frontSurface = lpDDSurface;
                if (!SetupDDraw())
                {
                    return false;
                }
                else
                {
                    bTargetAcquired = true;
                }
            }
        }
        else
        {
            logOutput << CurrentTimeString() << "could not retrieve caps" << endl;
        }
    }

    return lpDDSurface == g_frontSurface;
}

HRESULT STDMETHODCALLTYPE Unlock(LPDIRECTDRAWSURFACE7 surface, LPRECT data)
{
    HRESULT hr;

    //logOutput << CurrentTimeString() << "Hooked Unlock()" << endl;

    EnterCriticalSection(&ddrawMutex);

    //UnlockSurface handles the actual unlocking and un-/rehooks the method (as well as thread synchronisation)
    hr = UnlockSurface(surface, data);

    if (getFrontSurface(surface))
    {
        // palette fix, should be tested on several machines
        if (g_bUsePalette && g_CurrentPalette.bInitialized)
            //g_CurrentPalette.ReloadPrimarySurfacePaletteEntries();
        if (!g_bUseFlipMethod)
            CaptureDDraw();
    }

    LeaveCriticalSection(&ddrawMutex);

    return hr;
}

HRESULT STDMETHODCALLTYPE SetPalette(LPDIRECTDRAWSURFACE7 surface, LPDIRECTDRAWPALETTE lpDDPalette)
{
    //logOutput << CurrentTimeString() << "Hooked SetPalette()" << endl;

    ddrawSurfaceSetPalette.Unhook();
    HRESULT hr = surface->SetPalette(lpDDPalette);
    ddrawSurfaceSetPalette.Rehook();

    if (getFrontSurface(surface))
    {
        if (lpDDPalette)
            lpDDPalette->AddRef();
        SetupPalette(lpDDPalette);
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE PaletteSetEntries(LPDIRECTDRAWPALETTE palette, DWORD dwFlags, DWORD dwStartingEntry, DWORD dwCount, LPPALETTEENTRY lpEntries)
{
    //logOutput << CurrentTimeString() << "Hooked SetEntries()" << endl;

    ddrawPaletteSetEntries.Unhook();
    HRESULT hr = palette->SetEntries(dwFlags, dwStartingEntry, dwCount, lpEntries);
    ddrawPaletteSetEntries.Rehook();

    // update buffer palette
    if (SUCCEEDED(hr))
    {
        if (g_CurrentPalette.bInitialized)
        {
            memcpy(g_CurrentPalette.entries + dwStartingEntry, lpEntries, 4 * dwCount); // each entry is 4 bytes if DDCAPS_8BITENTRIES flag is not set
        }
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE CreateSurface(IDirectDraw7* ddInterface, LPDDSURFACEDESC2 lpDDSurfaceDesc, LPDIRECTDRAWSURFACE7 *lplpDDSurface, IUnknown *pUnkOuter)
{
    //logOutput << CurrentTimeString() << "Hooked CreateSurface()" << endl;

    if (!g_ddInterface)
    {
        if (ddInterface->QueryInterface(IID_IDirectDraw, (LPVOID*)&g_ddInterface) == S_OK)
        {
            logOutput << CurrentTimeString() << "IDirectDraw::CreateSurface: got DDInterface pointer" << endl;
        }
        else
        {
            logOutput << CurrentTimeString() << "IDirectDraw::CreateSurface: could not query DirectDraw interface" << endl;
        }
    }

    ddrawSurfaceCreate.Unhook();
    HRESULT hRes = ddInterface->CreateSurface(lpDDSurfaceDesc, lplpDDSurface, pUnkOuter);
    ddrawSurfaceCreate.Rehook();

    if (hRes == DD_OK)
    {
        if (lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
        {
            logOutput << CurrentTimeString() << "IDirectDraw::CreateSurface: Primary surface created at 0x" << *lplpDDSurface << endl;
            getFrontSurface(*lplpDDSurface);
        }
    }
    else
        printDDrawError(hRes, "CreateSurface");

    return hRes;
}

ULONG STDMETHODCALLTYPE Release(LPDIRECTDRAWSURFACE7 surface)
{
    //logOutput << CurrentTimeString() << "Hooked Release()" << endl;

    ddrawSurfaceRelease.Unhook();
    /*
    if(surface == g_frontSurface)
    {
    logOutput << CurrentTimeString() << "Releasing primary surface 0x" << surface << endl;
    surface->AddRef();
    ULONG refCount = surface->Release();

    if(refCount == 1)
    {
    logOutput << CurrentTimeString() << "Freeing primary surface" << endl;
    g_frontSurface = NULL;
    bTargetAcquired = false;
    CleanUpDDraw();
    }
    }
    */

    ULONG refCount = surface->Release();
    ddrawSurfaceRelease.Rehook();

    if (surface == g_frontSurface)
    {
        if (refCount == 0)
        {
            logOutput << CurrentTimeString() << "Freeing primary surface" << endl;
            CleanUpDDraw();
        }
    }

    return refCount;
}

HRESULT STDMETHODCALLTYPE Restore(LPDIRECTDRAWSURFACE7 surface)
{
    //logOutput << CurrentTimeString() << "Hooked Restore()" << endl;

    ddrawSurfaceRestore.Unhook();
    HRESULT hr = surface->Restore();

    if (bHasTextures)
    {
        if (surface == g_frontSurface)
        {
            logOutput << CurrentTimeString() << "SurfaceRestore: restoring offscreen buffers" << endl;
            bool success = true;
            for (UINT i = 0; i < NUM_BUFFERS; i++)
            {
                HRESULT err;
                if (FAILED(err = ddCaptures[i]->Restore()))
                {
                    logOutput << CurrentTimeString() << "SurfaceRestore: error restoring offscreen buffer" << endl;
                    printDDrawError(err, "Restore");
                    success = false;
                }
            }
            if (!success)
            {
                CleanUpDDraw();
            }
        }
    }
    ddrawSurfaceRestore.Rehook();

    if (!bHasTextures)
    {
        getFrontSurface(surface);
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE Flip(LPDIRECTDRAWSURFACE7 surface, LPDIRECTDRAWSURFACE7 lpDDSurface, DWORD flags)
{
    //logOutput << CurrentTimeString() << "Hooked Flip()" << endl;

    HRESULT hr;

    EnterCriticalSection(&ddrawMutex);

    ddrawSurfaceFlip.Unhook();
    hr = surface->Flip(lpDDSurface, flags);
    ddrawSurfaceFlip.Rehook();

    g_bUseFlipMethod = true;

    if (getFrontSurface(surface))
    {
        CaptureDDraw();
    }

    LeaveCriticalSection(&ddrawMutex);

    return hr;
}

HRESULT STDMETHODCALLTYPE Blt(LPDIRECTDRAWSURFACE7 surface, LPRECT lpDestRect, LPDIRECTDRAWSURFACE7 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx)
{
    //logOutput << CurrentTimeString() << "Hooked Blt()" << endl;

    EnterCriticalSection(&ddrawMutex);

    ddrawSurfaceBlt.Unhook();
    HRESULT hr = surface->Blt(lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    ddrawSurfaceBlt.Rehook();

    if (SUCCEEDED(hr))
    {
        if (!g_bUseFlipMethod)
        {
            if (getFrontSurface(surface))
            {
                CaptureDDraw();
            }
        }
    }

    LeaveCriticalSection(&ddrawMutex);

    return hr;
}

void UnhookAll()
{
    ddrawSurfaceCreate.Unhook();
    ddrawSurfaceRestore.Unhook();
    ddrawSurfaceRelease.Unhook();
    ddrawSurfaceSetPalette.Unhook();
    ddrawPaletteSetEntries.Unhook();
}

typedef HRESULT(WINAPI *CREATEDIRECTDRAWEXPROC)(GUID*, LPVOID*, REFIID, IUnknown*);

bool InitDDrawCapture()
{
    bool versionSupported = false;
    HMODULE hDDrawLib = NULL;
    if (hDDrawLib = GetModuleHandle(TEXT("ddraw.dll")))
    {
        bool isWinVistaMin = IsWindowsVistaOrGreater();
        bool isWin7min = IsWindows7OrGreater();
        bool isWin8min = IsWindows8OrGreater();

        UPARAM libBaseAddr = UPARAM(hDDrawLib);
        UPARAM surfCreateOffset;
        UPARAM surfUnlockOffset;
        UPARAM surfReleaseOffset;
        UPARAM surfRestoreOffset;
        UPARAM surfBltOffset;
        UPARAM surfFlipOffset;
        UPARAM surfSetPaletteOffset;
        UPARAM palSetEntriesOffset;

        if (isWinVistaMin)
        {
            if (!isWin7min)
            {
                RUNEVERYRESET logOutput << CurrentTimeString() << "DirectDraw capture: Windows Vista not supported yet" << endl;
            }
            else if (isWin7min && !isWin8min)
            {
                surfCreateOffset = 0x617E;
                surfUnlockOffset = 0x4C40;
                surfReleaseOffset = 0x3239;
                surfRestoreOffset = 0x3E9CB;
                surfBltOffset = surfCreateOffset + 0x44F63;
                surfFlipOffset = surfCreateOffset + 0x37789;
                surfSetPaletteOffset = surfCreateOffset + 0x4D2D3;
                palSetEntriesOffset = surfCreateOffset + 0x4CE68;
                versionSupported = true;
            }
            else if (isWin8min)
            {
                surfCreateOffset = 0x9530 + 0xC00;
                surfUnlockOffset = surfCreateOffset + 0x2A1D0;
                surfReleaseOffset = surfCreateOffset - 0x1A80;
                surfRestoreOffset = surfCreateOffset + 0x36000;
                surfBltOffset = surfCreateOffset + 0x438DC;
                surfFlipOffset = surfCreateOffset + 0x33EF3;
                surfSetPaletteOffset = surfCreateOffset + 0x4D3B8;
                palSetEntriesOffset = surfCreateOffset + 0x4CF4C;
                versionSupported = false;	// some crash bugs remaining

                RUNEVERYRESET logOutput << CurrentTimeString() << "DirectDraw capture: Windows 8 not supported yet" << endl;
            }
            else
            {
                RUNEVERYRESET logOutput << CurrentTimeString() << "DirectDraw capture: Unknown OS version" << endl;
            }
        }
        else
        {
            RUNEVERYRESET logOutput << CurrentTimeString() << "OS version not supported" << endl;
        }

        if (versionSupported)
        {
            ddrawSurfaceCreate.Hook((FARPROC)(libBaseAddr + surfCreateOffset), (FARPROC)CreateSurface);
            ddrawSurfaceRestore.Hook((FARPROC)(libBaseAddr + surfRestoreOffset), (FARPROC)Restore);
            ddrawSurfaceRelease.Hook((FARPROC)(libBaseAddr + surfReleaseOffset), (FARPROC)Release);
            ddrawSurfaceUnlock.Hook((FARPROC)(libBaseAddr + surfUnlockOffset), (FARPROC)Unlock);
            ddrawSurfaceBlt.Hook((FARPROC)(libBaseAddr + surfBltOffset), (FARPROC)Blt);
            ddrawSurfaceFlip.Hook((FARPROC)(libBaseAddr + surfFlipOffset), (FARPROC)Flip);
            ddrawSurfaceSetPalette.Hook((FARPROC)(libBaseAddr + surfSetPaletteOffset), (FARPROC)SetPalette);
            ddrawPaletteSetEntries.Hook((FARPROC)(libBaseAddr + palSetEntriesOffset), (FARPROC)PaletteSetEntries);

            ddrawSurfaceUnlock.Rehook();
            ddrawSurfaceFlip.Rehook();
            ddrawSurfaceBlt.Rehook();
            /*
            ddrawSurfaceCreate.Rehook();
            ddrawSurfaceRestore.Rehook();
            ddrawSurfaceRelease.Rehook();
            ddrawSurfaceSetPalette.Rehook();
            ddrawPaletteSetEntries.Rehook();
            */
        }
    }

    return versionSupported;
}

void HookAll()
{
    ddrawSurfaceCreate.Rehook();
    ddrawSurfaceRestore.Rehook();
    ddrawSurfaceRelease.Rehook();
    ddrawSurfaceSetPalette.Rehook();
    ddrawPaletteSetEntries.Rehook();
}

void CheckDDrawCapture()
{
    EnterCriticalSection(&ddrawMutex);
    ddrawSurfaceUnlock.Rehook(true);
    ddrawSurfaceFlip.Rehook(true);
    ddrawSurfaceBlt.Rehook(true);
    LeaveCriticalSection(&ddrawMutex);
}