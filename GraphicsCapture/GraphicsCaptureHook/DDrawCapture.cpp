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


HookData ddrawSurfaceFlip;

CaptureInfo ddrawCaptureInfo;

extern LPBYTE           textureBuffers[2];
extern MemoryCopyData   copyData;
extern DWORD            curCapture;

struct DDrawOverride
{
    HRESULT STDMETHODCALLTYPE SurfaceFlip(LPDIRECTDRAWSURFACE lpDDSurface, DWORD flags)
    {
        IDirectDrawSurface *surface = (IDirectDrawSurface*)this;

        ddrawSurfaceFlip.Unhook();
        HRESULT hRes = surface->Flip(lpDDSurface, flags);
        ddrawSurfaceFlip.Rehook();

        return hRes;
    }
};

typedef HRESULT (WINAPI*DDRAWCREATEPROC)(GUID*, LPDIRECTDRAW*, IUnknown*);
DDRAWCREATEPROC pDDrawCreate = NULL;

bool InitDDrawCapture()
{
    bool bSuccess = false;

    HMODULE hDDrawLib = NULL;
    if(hDDrawLib = GetModuleHandle(TEXT("ddraw.dll")))
    {
        pDDrawCreate = (DDRAWCREATEPROC)GetProcAddress(hDDrawLib, "DirectDrawCreate");
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
                        surface->Release();

                        bSuccess = true;
                    }
                }
                ddraw->Release();
            }
        }
    }

    return bSuccess;
}
