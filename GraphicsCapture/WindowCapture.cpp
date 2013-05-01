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


#include "GraphicsCapture.h"


void WindowCapture::Destroy()
{
    delete sharedTexture;
    sharedTexture = NULL;
}

bool WindowCapture::Init(CaptureInfo &info)
{
    hwndTarget = (HWND)info.hwndCapture;

    cx = info.cx;
    cy = info.cy;

    if (!cx || !cy)
        return false;

    sharedTexture = CreateGDITexture(info.cx, info.cy);

    return true;
}

Texture* WindowCapture::LockTexture()
{
    HDC hdcTarget = GetDC(hwndTarget);
    if (!hdcTarget) return NULL;

    bool bSuccess = false;
    HDC hDC;
    if (sharedTexture->GetDC(hDC)) {
        BitBlt(hDC, 0, 0, cx, cy, hdcTarget, 0, 0, SRCCOPY);

        sharedTexture->ReleaseDC();

        bSuccess = true;
    }

    ReleaseDC(hwndTarget, hdcTarget);

    return bSuccess ? sharedTexture : NULL;
}

void WindowCapture::UnlockTexture()
{
}
