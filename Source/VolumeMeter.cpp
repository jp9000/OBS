/********************************************************************************
 Copyright (C) 2012 Bill Hamilton
                    Hugh Bailey <obs.jim@gmail.com>

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

float maxLinear;
float minLinear;

struct VolumeMeterData
{
    float curVolume, curMax, curPeak;
    float graduations[16];

    DWORD drawColor;
    long  cx,cy;
    HBRUSH  hRed, hGreen,hGreenDark, hBlack, hGray, hLightGray;

    void DrawVolumeMeter(HDC hDC);
};

inline float DBtoLog(float db)
{
    /* logarithmic scale for audio meter */
    return -log10(0.0f-(db - 6.0f));
}

inline VolumeMeterData* GetVolumeMeterData(HWND hwnd)
{
    return (VolumeMeterData*)GetWindowLongPtr(hwnd, 0);
}

float SetVolumeMeterValue(HWND hwnd, float fVal, float fMax, float fPeak)
{
    VolumeMeterData *meter = GetVolumeMeterData(hwnd);
    if(!meter)
        CrashError(TEXT("SetVolumeMeterValue called on a control that's not a volume control"));

    float lastVal = meter->curVolume;

    meter->curVolume = fVal;
    meter->curMax = max(VOL_MIN, fMax);
    meter->curPeak = fPeak;

    HDC hDC = GetDC(hwnd);
    meter->DrawVolumeMeter(hDC);
    ReleaseDC(hwnd, hDC);

    return lastVal;
}

LRESULT CALLBACK VolumeMeterProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    VolumeMeterData *meter;

    switch(message)
    {
        case WM_NCCREATE:
            {
                CREATESTRUCT *pCreateData = (CREATESTRUCT*)lParam;

                meter = (VolumeMeterData*)malloc(sizeof(VolumeMeterData));
                zero(meter, sizeof(VolumeMeterData));
                SetWindowLongPtr(hwnd, 0, (LONG_PTR)meter);

                meter->curVolume = VOL_MIN;
                meter->curMax = VOL_MIN;
                meter->curPeak = VOL_MIN;
                
                meter->cx = pCreateData->cx;
                meter->cy = pCreateData->cy;

                for(int i = 0; i < 16; i++)
                {
                    meter->graduations[i] = (DBtoLog(-96.0f + 6.0f * (i+1)) - minLinear) / (maxLinear - minLinear);
                }

                /*create color brushes*/
                meter->hRed  = CreateSolidBrush(0x2929f4);
                meter->hGreen  = CreateSolidBrush(0x2bf13e);
                meter->hGreenDark  = CreateSolidBrush(0x177d20);
                meter->hBlack = CreateSolidBrush(0x000000);
                meter->hGray = CreateSolidBrush(0x777777);
                meter->hLightGray = CreateSolidBrush(0xCCCCCC);
                
                return TRUE;
            }

        case WM_DESTROY:
            {
                meter = GetVolumeMeterData(hwnd);
                
                DeleteObject(meter->hRed);
                DeleteObject(meter->hGreen);
                DeleteObject(meter->hGreenDark);
                DeleteObject(meter->hBlack);
                DeleteObject(meter->hGray);
                DeleteObject(meter->hLightGray);
                
                if(meter)
                    free(meter);

                break;
            }

        case WM_PAINT:
            {
                meter = GetVolumeMeterData(hwnd);

                PAINTSTRUCT ps;

                HDC hDC = BeginPaint(hwnd, &ps);
                meter->DrawVolumeMeter(hDC);
                EndPaint(hwnd, &ps);

                break;
            }

        case WM_SIZE:
            {
                meter = GetVolumeMeterData(hwnd);
                meter->cx = LOWORD(lParam);
                meter->cy = HIWORD(lParam);

                HDC hDC = GetDC(hwnd);
                meter->DrawVolumeMeter(hDC);
                ReleaseDC(hwnd, hDC);
                break;
            }

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

void VolumeMeterData::DrawVolumeMeter(HDC hDC)
{
    HDC hdcTemp = CreateCompatibleDC(hDC);
    HBITMAP hbmpTemp = CreateCompatibleBitmap(hDC, cx, cy);
    SelectObject(hdcTemp, hbmpTemp);

    RECT clientRect = {0, 0, cx, cy};
    FillRect(hdcTemp, &clientRect, (HBRUSH)COLOR_WINDOW);

    int cxAdjust = cx;
    
    const int padding = 1;

    /* bound volume values */
    float workingVol = max(VOL_MIN, curVolume);
    workingVol = min(VOL_MAX, workingVol);
    float workingMax = max(VOL_MIN, curMax);
    workingMax = min(VOL_MAX, workingMax);

    /* convert dB to logarithmic then to linear scale [0, 1] */
    float rmsScale = (DBtoLog(workingVol) - minLinear) / (maxLinear - minLinear);
    float maxScale = (DBtoLog(workingMax) - minLinear) / (maxLinear - minLinear);

    LONG yStart = 0, yEnd = cy - 5;
    RECT meterRMS = {0, yStart, (LONG)(cx * rmsScale), yEnd};
    RECT meterGray = {0, 0, cx, yEnd};
    
    FillRect(hdcTemp, &meterGray, hLightGray);
    FillRect(hdcTemp, &meterRMS, hGreenDark);

    /* gradient info setup */
    TRIVERTEX        vert[2] ;
    GRADIENT_RECT    gRect;
    vert [0] .x      = (LONG)(cx * rmsScale);
    vert [0] .y      = 0;
    vert [0] .Red    = 0x2000;
    vert [0] .Green  = 0x7d00;
    vert [0] .Blue   = 0x1700;
    vert [0] .Alpha  = 0x0000;
    vert [1] .x      = (LONG)(cx * maxScale);
    vert [1] .y      = yEnd; 
    vert [1] .Red    = 0x3e00;
    vert [1] .Green  = 0xf100;
    vert [1] .Blue   = 0x2b00;
    vert [1] .Alpha  = 0x0000;
    gRect.UpperLeft  = 0;
    gRect.LowerRight = 1;
    GdiGradientFill(hdcTemp,vert,2,&gRect,1,GRADIENT_FILL_RECT_H);

    
    
    /* draw 6dB graduations */
    for(int i = 0; i < 16; i++)
    {
        float scale = graduations[i];
        RECT graduation = {(LONG)(cx * scale), cy - 4, ((LONG)(cx * scale)) + 1, cy};
        FillRect(hdcTemp, &graduation, hGray);
    }
    
    /* draw peak sample indicator */
    float peakScale = (DBtoLog(curPeak) - minLinear) / (maxLinear - minLinear);
    bool maxxed = curPeak >= 0;
    if(maxxed)
        peakScale = 1.0f;
    RECT graduation = {(LONG)(cx * peakScale) - ((maxxed)?1:0), 0, ((LONG)(cx * peakScale)) + 1, cy};
    FillRect(hdcTemp, &graduation, (maxxed)?hRed:hBlack);

    BitBlt(hDC, 0, 0, cx, cy, hdcTemp, 0, 0, SRCCOPY);

    DeleteObject(hdcTemp);
    DeleteObject(hbmpTemp);
}

void InitVolumeMeter()
{
    /*initiate threshold values */
    maxLinear = DBtoLog(VOL_MAX);
    minLinear = DBtoLog(VOL_MIN);

    WNDCLASS wnd;

    wnd.cbClsExtra = 0;
    wnd.cbWndExtra = sizeof(LPVOID);
    wnd.hbrBackground = NULL;
    wnd.hCursor = LoadCursor(NULL, IDC_ARROW);
    wnd.hIcon = NULL;
    wnd.hInstance = hinstMain;
    wnd.lpfnWndProc = VolumeMeterProc;
    wnd.lpszClassName = VOLUME_METER_CLASS;
    wnd.lpszMenuName = NULL;
    wnd.style = CS_PARENTDC | CS_VREDRAW | CS_HREDRAW;

    if(!RegisterClass(&wnd))
        CrashError(TEXT("Could not register volume meter class"));
}
