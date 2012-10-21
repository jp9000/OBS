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
/*#include <ddraw.h>


HookData ddrawSurfaceFlip;
HookData ddrawSurfaceLock;
HookData ddrawSurfaceUnlock;
HookData ddrawSurfacePalette;
HookData ddrawSurfaceRelease;

CaptureInfo             ddrawCaptureInfo;

extern LPBYTE           textureBuffers[2];
extern MemoryCopyData   copyData;
extern DWORD            curCapture;

bool                    bGotSurfaceDesc = false;
bool                    bLockingSurface = false;
DDSURFACEDESC           surfaceDesc;
IDirectDrawSurface      *frontSurface = NULL;
PALETTEENTRY            curPalette[256];


void CleanUpDDraw()
{
    for(UINT i=0; i<2; i++)
    {
        if(textureBuffers[i])
        {
            if(WaitForSingleObject(textureMutexes[i], INFINITE) == WAIT_OBJECT_0)
            {
                copyData.lockData[i].address = NULL;
                free(textureBuffers[i]);
                textureBuffers[i] = NULL;
                ReleaseMutex(textureMutexes[i]);
            }
        }
    }
}

bool AquiredDDrawSurface(IDirectDrawSurface *surface)
{
    if(!frontSurface && !bTargetAcquired)
    {
        DDSCAPS caps;
        if(SUCCEEDED(surface->GetCaps(&caps)))
        {
            if(caps.dwCaps & DDSCAPS_PRIMARYSURFACE)
            {
                frontSurface = surface;
                bTargetAcquired = true;
                return true;
            }
        }
    }

    return frontSurface == surface;
}

void ChangeDDrawSurface()
{
    textureBuffers[0] = (LPBYTE)malloc(surfaceDesc.dwWidth*surfaceDesc.dwHeight*4);
    textureBuffers[1] = (LPBYTE)malloc(surfaceDesc.dwWidth*surfaceDesc.dwHeight*4);

    ddrawCaptureInfo.captureType = CAPTURETYPE_MEMORY;
    ddrawCaptureInfo.cx = surfaceDesc.dwWidth;
    ddrawCaptureInfo.cy = surfaceDesc.dwHeight;
    ddrawCaptureInfo.hwndSender = hwndSender;
    ddrawCaptureInfo.data = &copyData;
    ddrawCaptureInfo.bFlip = FALSE;
    PostMessage(hwndReceiver, RECEIVER_NEWCAPTURE, 0, (LPARAM)&ddrawCaptureInfo);
}

void CopyDDrawSurface(LPBYTE lpData)
{
    if(surfaceDesc.ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED8)
    {
        
    }
    else if(surfaceDesc.ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED4)
    {
    }
    else
    {
        DWORD bitDepth = surfaceDesc.ddpfPixelFormat.dwRGBBitCount;
        if(surfaceDesc.ddpfPixelFormat.dwFlags & DDPF_ALPHAPIXELS)
        {
            
        }
    }
}

void CopyPalette(LPDIRECTDRAWPALETTE lpPalette)
{
    if(surfaceDesc.ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED8)
        lpPalette->GetEntries(0, 0, 256, curPalette);
    else if(surfaceDesc.ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED4)
        lpPalette->GetEntries(0, 0, 16, curPalette);
}

struct DDrawOverride
{
    HRESULT STDMETHODCALLTYPE SurfaceFlip(LPDIRECTDRAWSURFACE lpDDSurface, DWORD flags)
    {
        IDirectDrawSurface *surface = (IDirectDrawSurface*)this;

        if(AquiredDDrawSurface(surface))
        {
        }

        ddrawSurfaceFlip.Unhook();
        HRESULT hRes = surface->Flip(lpDDSurface, flags);
        ddrawSurfaceFlip.Rehook();

        return hRes;
    }

    HRESULT STDMETHODCALLTYPE SurfaceLock(LPRECT pRect, LPDDSURFACEDESC lpSurfDesc, DWORD flags, HANDLE hEvent)
    {
        IDirectDrawSurface *surface = (IDirectDrawSurface*)this;

        if(AquiredDDrawSurface(surface) && !pRect)
        {
            bool bSurfaceChanged = true;
            if(!bGotSurfaceDesc)
                memcpy(&surfaceDesc, lpSurfDesc, sizeof(surfaceDesc));
            else if(lpSurfDesc->dwWidth != surfaceDesc.dwWidth || lpSurfDesc->dwHeight != surfaceDesc.dwHeight)
            {
                CleanUpDDraw();
                memcpy(&surfaceDesc, lpSurfDesc, sizeof(surfaceDesc));
            }
            else
                bSurfaceChanged = false;

            if(bSurfaceChanged)
                ChangeDDrawSurface();

            bLockingSurface = true;
        }

        ddrawSurfaceLock.Unhook();
        HRESULT hRes = surface->Lock(pRect, lpSurfDesc, flags, hEvent);
        ddrawSurfaceLock.Rehook();

        return hRes;
    }

    HRESULT STDMETHODCALLTYPE SurfaceUnlock(LPVOID lpSurfaceData)
    {
        IDirectDrawSurface *surface = (IDirectDrawSurface*)this;

        if(AquiredDDrawSurface(surface))
        {
            if(bLockingSurface)
            {
                
                bLockingSurface = false;
            }
        }

        ddrawSurfaceUnlock.Unhook();
        HRESULT hRes = surface->Unlock(lpSurfaceData);
        ddrawSurfaceUnlock.Rehook();

        return hRes;
    }

    HRESULT STDMETHODCALLTYPE SurfaceSetPalette(LPDIRECTDRAWPALETTE lpPalette)
    {
        IDirectDrawSurface *surface = (IDirectDrawSurface*)this;

        if(AquiredDDrawSurface(surface))
        {
            if(lpPalette)
                CopyPalette(lpPalette);
        }

        ddrawSurfaceUnlock.Unhook();
        HRESULT hRes = surface->SetPalette(lpPalette);
        ddrawSurfaceUnlock.Rehook();

        return hRes;
    }

    HRESULT STDMETHODCALLTYPE SurfaceRelease()
    {
        IDirectDrawSurface *surface = (IDirectDrawSurface*)this;

        ddrawSurfaceUnlock.Unhook();

        if(frontSurface == surface)
        {
            surface->AddRef();
            UINT refCount = surface->Release();

            if(refCount == 1)
            {
                frontSurface = NULL;
                bTargetAcquired = false;
                CleanUpDDraw();
            }
        }

        UINT ref = surface->Release();
        ddrawSurfaceUnlock.Rehook();

        return ref;
    }
};

typedef HRESULT (WINAPI*DDRAWCREATEPROC)(GUID*, LPDIRECTDRAW*, IUnknown*);

bool InitDDrawCapture()
{
    bool bSuccess = false;

    HMODULE hDDrawLib = NULL;
    if(hDDrawLib = GetModuleHandle(TEXT("ddraw.dll")))
    {
        DDRAWCREATEPROC pDDrawCreate = (DDRAWCREATEPROC)GetProcAddress(hDDrawLib, "DirectDrawCreate");
        if(pDDrawCreate)
        {
            IDirectDraw *ddraw;
            if(SUCCEEDED((*pDDrawCreate)(NULL, &ddraw, NULL)))
            {
                DDSURFACEDESC ddsd;
                ZeroMemory(&ddsd, sizeof(ddsd));

                HRESULT whatever;
                if(SUCCEEDED(whatever = ddraw->SetCooperativeLevel(hwndSender, DDSCL_NORMAL)))
                {
                    ddsd.dwSize = sizeof(ddsd);
                    ddsd.dwFlags = DDSD_CAPS;
                    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

                    IDirectDrawSurface *surface;
                    if(SUCCEEDED(whatever = ddraw->CreateSurface(&ddsd, &surface, NULL)))
                    {
                        UPARAM *vtable = *(UPARAM**)surface;

                        ddrawSurfaceFlip.Hook((FARPROC)*(vtable+(44/4)), ConvertClassProcToFarproc((CLASSPROC)&DDrawOverride::SurfaceFlip));
                        ddrawSurfaceLock.Hook((FARPROC)*(vtable+(100/4)), ConvertClassProcToFarproc((CLASSPROC)&DDrawOverride::SurfaceLock));
                        ddrawSurfaceUnlock.Hook((FARPROC)*(vtable+(128/4)), ConvertClassProcToFarproc((CLASSPROC)&DDrawOverride::SurfaceUnlock));
                        ddrawSurfacePalette.Hook((FARPROC)*(vtable+(124/4)), ConvertClassProcToFarproc((CLASSPROC)&DDrawOverride::SurfaceSetPalette));
                        ddrawSurfaceRelease.Hook((FARPROC)*(vtable+(8/4)), ConvertClassProcToFarproc((CLASSPROC)&DDrawOverride::SurfaceRelease));

                        ddrawSurfaceRelease.Unhook();
                        surface->Release();
                        ddrawSurfaceRelease.Rehook();

                        bSuccess = true;
                    }
                }
                ddraw->Release();
            }
        }
    }

    return bSuccess;
}
*/