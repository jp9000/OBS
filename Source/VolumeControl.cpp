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


#include "Main.h"

struct VolumeControlData
{
    float curVolume;
    float lastUnmutedVol;

    DWORD drawColor;
    bool  bDisabled, bHasCapture;
    long  cx,cy;

    HICON hiconPlay, hiconMute;
    bool bDrawIcon;

    void DrawVolumeControl(HDC hDC);
};

inline VolumeControlData* GetVolumeControlData(HWND hwnd)
{
    return (VolumeControlData*)GetWindowLongPtr(hwnd, 0);
}

float ToggleVolumeControlMute(HWND hwnd)
{
    VolumeControlData *control = GetVolumeControlData(hwnd);
    if(!control)
        CrashError(TEXT("ToggleVolumeControlMute called on a control that's not a volume control"));

    if(control->curVolume < 0.05f)
    {
        if(control->lastUnmutedVol < 0.05f)
            control->lastUnmutedVol = 1.0f;
        control->curVolume = control->lastUnmutedVol;
    }
    else
    {
        control->lastUnmutedVol = control->curVolume;
        control->curVolume = 0.0f;
    }

    HDC hDC = GetDC(hwnd);
    control->DrawVolumeControl(hDC);
    ReleaseDC(hwnd, hDC);

    return control->curVolume;
}

float SetVolumeControlValue(HWND hwnd, float fVal)
{
    VolumeControlData *control = GetVolumeControlData(hwnd);
    if(!control)
        CrashError(TEXT("SetVolumeControlValue called on a control that's not a volume control"));

    float lastVal = control->curVolume;

    /* 
        convert back to linear, assuming logarithmic scale input
        based on article at http://www.dr-lex.be/info-stuff/volumecontrols.html
    */
    if(fVal == 0)
        control->curVolume = 0;
    else
        control->curVolume = log(fVal / VOL_ALPHA) / VOL_BETA;
    

    HDC hDC = GetDC(hwnd);
    control->DrawVolumeControl(hDC);
    ReleaseDC(hwnd, hDC);

    return lastVal;
}

float GetVolumeControlValue(HWND hwnd)
{
    VolumeControlData *control = GetVolumeControlData(hwnd);
    if(!control)
        CrashError(TEXT("GetVolumeControlValue called on a control that's not a volume control"));
    
    /* 
        conversion to logarithmic scale 
        based on article at http://www.dr-lex.be/info-stuff/volumecontrols.html
    */
    if(control->curVolume > 0.05)
        return VOL_ALPHA * exp(VOL_BETA * control->curVolume);
    else 
        return 0;

}

void SetVolumeControlIcons(HWND hwnd, HICON hiconPlay, HICON hiconMute)
{
    VolumeControlData *control = GetVolumeControlData(hwnd);
    if(!control)
        CrashError(TEXT("GetVolumeControlValue called on a control that's not a volume control"));

    control->hiconPlay = hiconPlay;
    control->hiconMute = hiconMute;
    control->bDrawIcon = (control->cy == 32) && control->hiconPlay;

    HDC hDC = GetDC(hwnd);
    control->DrawVolumeControl(hDC);
    ReleaseDC(hwnd, hDC);
}


LRESULT CALLBACK VolumeControlProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    VolumeControlData *control;

    switch(message)
    {
        case WM_NCCREATE:
            {
                CREATESTRUCT *pCreateData = (CREATESTRUCT*)lParam;

                control = (VolumeControlData*)malloc(sizeof(VolumeControlData));
                zero(control, sizeof(VolumeControlData));
                SetWindowLongPtr(hwnd, 0, (LONG_PTR)control);

                control->curVolume = 1.0f;
                control->bDisabled = ((pCreateData->style & WS_DISABLED) != 0);

                control->cx = pCreateData->cx;
                control->cy = pCreateData->cy;

                return TRUE;
            }

        case WM_DESTROY:
            {
                control = GetVolumeControlData(hwnd);
                if(control)
                    free(control);

                break;
            }

        case WM_PAINT:
            {
                control = GetVolumeControlData(hwnd);

                PAINTSTRUCT ps;

                HDC hDC = BeginPaint(hwnd, &ps);
                control->DrawVolumeControl(hDC);
                EndPaint(hwnd, &ps);

                break;
            }

        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            {
                control = GetVolumeControlData(hwnd);

                short x = short(LOWORD(lParam));
                short y = short(HIWORD(lParam));

                UINT id = (UINT)GetWindowLongPtr(hwnd, GWLP_ID);

                if(message == WM_LBUTTONDOWN && !control->bDisabled)
                {
                    if(control->cy == 32 && x >= (control->cx-32))
                    {
                        if(control->curVolume < 0.05f)
                        {
                            if(control->lastUnmutedVol < 0.05f)
                                control->lastUnmutedVol = 1.0f;
                            control->curVolume = control->lastUnmutedVol;
                        }
                        else
                        {
                            control->lastUnmutedVol = control->curVolume;
                            control->curVolume = 0.0f;
                        }

                        SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(id, VOLN_FINALVALUE), (LPARAM)hwnd);
                    }
                    else
                    {
                        SetCapture(hwnd);
                        control->bHasCapture = true;

                        if(control->curVolume > 0.05f)
                            control->lastUnmutedVol = control->curVolume;

                        int cxAdjust = control->cx;
                        if(control->bDrawIcon) cxAdjust -= 32;
                        control->curVolume = float(x) / cxAdjust;

                        SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(id, VOLN_ADJUSTING), (LPARAM)hwnd);
                    }

                    HDC hDC = GetDC(hwnd);
                    control->DrawVolumeControl(hDC);
                    ReleaseDC(hwnd, hDC);
                }
                else if(control->bHasCapture)
                {
                    UINT id = (UINT)GetWindowLongPtr(hwnd, GWLP_ID);
                    SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(id, VOLN_FINALVALUE), (LPARAM)hwnd);

                    ReleaseCapture();
                    control->bHasCapture = false;
                }
                
                break;
            }

        case WM_MOUSEMOVE:
            {
                control = GetVolumeControlData(hwnd);
                if(control->bHasCapture)
                {
                    int cxAdjust = control->cx;
                    if(control->bDrawIcon) cxAdjust -= 32;
                    control->curVolume = float(short(LOWORD(lParam))) / cxAdjust;

                    if(control->curVolume < 0.0f)
                        control->curVolume = 0.0f;
                    else if(control->curVolume > 1.0f)
                        control->curVolume = 1.0f;

                    HDC hDC = GetDC(hwnd);
                    control->DrawVolumeControl(hDC);
                    ReleaseDC(hwnd, hDC);

                    UINT id = (UINT)GetWindowLongPtr(hwnd, GWLP_ID);
                    SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(id, VOLN_ADJUSTING), (LPARAM)hwnd);
                }
                break;
            }

        case WM_ENABLE:
            {
                UINT id = (UINT)GetWindowLongPtr(hwnd, GWLP_ID);
                control = GetVolumeControlData(hwnd);

                if(control->bDisabled == !wParam)
                    break;

                control->bDisabled = !control->bDisabled;

                if(control->bDisabled)
                {
                    if(control->curVolume > 0.05f)
                    {
                        control->lastUnmutedVol = control->curVolume;
                        control->curVolume = 0.0f;
                        SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(id, VOLN_ADJUSTING), (LPARAM)hwnd);
                    }
                }
                else
                {
                    if(control->curVolume < 0.05f)
                    {
                        control->curVolume = control->lastUnmutedVol;
                        SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(id, VOLN_ADJUSTING), (LPARAM)hwnd);
                    }
                }

                HDC hDC = GetDC(hwnd);
                control->DrawVolumeControl(hDC);
                ReleaseDC(hwnd, hDC);
                break;
            }

        case WM_SIZE:
            {
                control = GetVolumeControlData(hwnd);
                control->cx = LOWORD(lParam);
                control->cy = HIWORD(lParam);

                control->bDrawIcon = (control->cy == 32) && control->hiconPlay;

                HDC hDC = GetDC(hwnd);
                control->DrawVolumeControl(hDC);
                ReleaseDC(hwnd, hDC);
                break;
            }

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

void VolumeControlData::DrawVolumeControl(HDC hDC)
{
    HDC hdcTemp = CreateCompatibleDC(hDC);
    HBITMAP hbmpTemp = CreateCompatibleBitmap(hDC, cx, cy);
    SelectObject(hdcTemp, hbmpTemp);

    RECT clientRect = {0, 0, cx, cy};
    FillRect(hdcTemp, &clientRect, (HBRUSH)COLOR_WINDOW);

    HBRUSH hGray = CreateSolidBrush(REVERSE_COLOR(Color_Gray));
    HBRUSH hRed  = CreateSolidBrush(0x2020bf);//0xff4040);

    float visualVolume = (bDisabled) ? 0.0f : curVolume;

    int cxAdjust = cx;
    if(bDrawIcon) cxAdjust -= 32;

    const int padding = 1;
    const float volSliceSize = float(cxAdjust-(padding*9))/10.0f;

    int volPixelPos = int(visualVolume*float(cxAdjust));

    if(volSliceSize > 1.0f)
    {
        float ySliceStart, ySliceSize;
        if(bDrawIcon)
        {
            ySliceStart = 4.0f;
            ySliceSize = float((cy-ySliceStart)/10);
        }
        else
        {
            ySliceStart = 0.0f;
            ySliceSize = float(cy/10);
        }

        for(int i=0; i<10; i++)
        {
            int pos = int(volSliceSize*float(i)) + (padding*i);

            RECT sliceRect = {pos, int(ySliceStart+((9.0f-float(i))*ySliceSize)), pos+int(volSliceSize), cy};

            if(sliceRect.right < volPixelPos) //full
                FillRect(hdcTemp, &sliceRect, hRed);
            else if(sliceRect.left < volPixelPos) //half
            {
                RECT leftHalf, rightHalf;
                mcpy(&leftHalf,  &sliceRect, sizeof(sliceRect));
                mcpy(&rightHalf, &sliceRect, sizeof(sliceRect));

                rightHalf.left = leftHalf.right = volPixelPos;
                FillRect(hdcTemp, &leftHalf, hRed);
                FillRect(hdcTemp, &rightHalf, hGray);
            }
            else //empty
                FillRect(hdcTemp, &sliceRect, hGray);
        }

        if(bDrawIcon)
            DrawIcon(hdcTemp, cx-32, 0, (visualVolume > 0.05f) ? hiconPlay : hiconMute);
    }

    BitBlt(hDC, 0, 0, cx, cy, hdcTemp, 0, 0, SRCCOPY);

    DeleteObject(hdcTemp);
    DeleteObject(hbmpTemp);

    DeleteObject(hGray);
    DeleteObject(hRed);
}

void InitVolumeControl()
{
    WNDCLASS wnd;

    wnd.cbClsExtra = 0;
    wnd.cbWndExtra = sizeof(LPVOID);
    wnd.hbrBackground = NULL;
    wnd.hCursor = LoadCursor(NULL, IDC_ARROW);
    wnd.hIcon = NULL;
    wnd.hInstance = hinstMain;
    wnd.lpfnWndProc = VolumeControlProc;
    wnd.lpszClassName = VOLUME_CONTROL_CLASS;
    wnd.lpszMenuName = NULL;
    wnd.style = CS_PARENTDC | CS_VREDRAW | CS_HREDRAW;

    if(!RegisterClass(&wnd))
        CrashError(TEXT("Could not register volume control class"));
}

