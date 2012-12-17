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

float maxLinear;
float redThresh;
float yellowThresh;
float minLinear;

struct VolumeMeterData
{
    float curVolume, curMax;
    float graduations[16];

    DWORD drawColor;
    long  cx,cy;
    HBRUSH  hRed,hRedDark,hYellow,hYellowDark,hGreen,hGreenDark, hBlack, hGray;

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

float SetVolumeMeterValue(HWND hwnd, float fVal, float fMax)
{
    VolumeMeterData *meter = GetVolumeMeterData(hwnd);
    if(!meter)
        CrashError(TEXT("SetVolumeMeterValue called on a control that's not a volume control"));

    float lastVal = meter->curVolume;

    meter->curVolume = fVal;
    meter->curMax = max(VOL_MIN, min(VOL_MAX, fMax));
    
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
                
                meter->cx = pCreateData->cx;
                meter->cy = pCreateData->cy;

                for(int i = 0; i < 16; i++)
                {
                    meter->graduations[i] = (DBtoLog(-96.0f + 6.0f * (i+1)) - minLinear) / (maxLinear - minLinear);
                }

                /*create color brushes*/
                meter->hRed  = CreateSolidBrush(0x2929f4);
                meter->hRedDark  = CreateSolidBrush(0x15157d);
                meter->hYellow  = CreateSolidBrush(0x2beee7);
                meter->hYellowDark  = CreateSolidBrush(0x167c78);
                meter->hGreen  = CreateSolidBrush(0x2bf13e);
                meter->hGreenDark  = CreateSolidBrush(0x177d20);
                meter->hBlack = CreateSolidBrush(0x000000);
                meter->hGray = CreateSolidBrush(0x777777);
                
                return TRUE;
            }

        case WM_DESTROY:
            {
                meter = GetVolumeMeterData(hwnd);
                
                DeleteObject(meter->hRed);
                DeleteObject(meter->hRedDark);
                DeleteObject(meter->hYellow);
                DeleteObject(meter->hYellowDark);
                DeleteObject(meter->hGreen);
                DeleteObject(meter->hGreenDark);
                DeleteObject(meter->hBlack);
                DeleteObject(meter->hGray);
                
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

    /* convert dB to logarithmic then to linear scale [0, 1] */
    float scale = (DBtoLog(workingVol) - minLinear) / (maxLinear - minLinear);
    
    LONG yStart = 0, yEnd = cy - 5;

    /* draw active and inactive part of volume meter */
    if(scale < yellowThresh)
    {
        /*only green meter*/
        RECT meterGreen = {0, yStart, (LONG)(cx * scale), yEnd};
        RECT meterGreenDark = {(LONG)(cx * scale), yStart, (LONG)(cx * yellowThresh), yEnd};
        RECT meterYellowDark = {(LONG)(cx * yellowThresh), yStart, (LONG)(cx * redThresh), yEnd};
        RECT meterRedDark = {(LONG)(cx * redThresh), yStart, cx, yEnd};
        
        FillRect(hdcTemp, &meterGreen, hGreen);
        FillRect(hdcTemp, &meterGreenDark, hGreenDark);
        FillRect(hdcTemp, &meterYellowDark, hYellowDark);
        FillRect(hdcTemp, &meterRedDark, hRedDark);
    }
    else if(scale < redThresh)
    {
        /*yellow meter*/
        RECT meterGreen = {0, yStart, (LONG)(cx * yellowThresh), yEnd};
        RECT meterYellow = {(LONG)(cx * yellowThresh), yStart, (LONG)(cx * scale), yEnd};
        RECT meterYellowDark = {(LONG)(cx * scale), yStart, (LONG)(cx * redThresh), yEnd};
        RECT meterRedDark = {(LONG)(cx * redThresh), yStart, cx, yEnd};
        
        FillRect(hdcTemp, &meterGreen, hGreen);
        FillRect(hdcTemp, &meterYellow, hYellow);
        FillRect(hdcTemp, &meterYellowDark, hYellowDark);
        FillRect(hdcTemp, &meterRedDark, hRedDark);
    }
    else
    {
        /*red meter (signal will be clippled)*/
        RECT meterGreen = {0, yStart, (LONG)(cx * yellowThresh), yEnd};
        RECT meterYellow = {(LONG)(cx * yellowThresh), yStart, (LONG)(cx * redThresh), yEnd};
        RECT meterRed = {(LONG)(cx * redThresh), yStart, (LONG)(cx * scale), yEnd};
        RECT meterRedDark = {(LONG)(cx * scale), yStart, cx, yEnd};

        FillRect(hdcTemp, &meterGreen, hGreen);
        FillRect(hdcTemp, &meterYellow, hYellow);
        FillRect(hdcTemp, &meterRed, hRed);
        FillRect(hdcTemp, &meterRedDark, hRedDark);

    }
    
    /* draw 6dB graduations */
    for(int i = 0; i < 16; i++)
    {
        float scale = graduations[i];
        RECT graduation = {(LONG)(cx * scale), cy - 4, ((LONG)(cx * scale)) + 1, cy};
        FillRect(hdcTemp, &graduation, hGray);
    }
    
    /* draw max sample indicator */
    scale = (DBtoLog(curMax) - minLinear) / (maxLinear - minLinear);
    bool maxxed = VOL_MAX - curMax < 0.1f;
    RECT graduation = {(LONG)(cx * scale) - ((maxxed)?1:0), 0, ((LONG)(cx * scale)) + 1, cy};
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
    
    /* -2dB red zone, -10.0dB yellow zone */
    redThresh = (DBtoLog(-2.0f) - minLinear) / (maxLinear - minLinear);
    yellowThresh = (DBtoLog(-10.0f) - minLinear) / (maxLinear - minLinear);

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
