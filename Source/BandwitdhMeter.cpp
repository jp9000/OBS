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

struct BandwidthMeterData
{
    UINT    bytesPerSec;
    double  strain;

    long    cx,cy;

    void Draw(HDC hDC);
};


inline BandwidthMeterData* GetBandwidthMeterData(HWND hwnd)
{
    return (BandwidthMeterData*)GetWindowLongPtr(hwnd, 0);
}

void SetBandwidthMeterValue(HWND hwnd, UINT bytesPerSec, double strain)
{
    BandwidthMeterData *control = GetBandwidthMeterData(hwnd);
    if(!control)
        return;

    control->bytesPerSec = bytesPerSec;
    control->strain = strain;

    HDC hDC = GetDC(hwnd);
    control->Draw(hDC);
    ReleaseDC(hwnd, hDC);
}

LRESULT CALLBACK BandwidthMeterProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    BandwidthMeterData *control;

    switch(message)
    {
        case WM_NCCREATE:
            {
                CREATESTRUCT *pCreateData = (CREATESTRUCT*)lParam;

                control = (BandwidthMeterData*)malloc(sizeof(BandwidthMeterData));
                zero(control, sizeof(BandwidthMeterData));
                SetWindowLongPtr(hwnd, 0, (LONG_PTR)control);

                control->bytesPerSec = 0;
                control->strain = 0.0;

                control->cx = pCreateData->cx;
                control->cy = pCreateData->cy;

                return TRUE;
            }

        case WM_DESTROY:
            {
                control = GetBandwidthMeterData(hwnd);
                if(control)
                    free(control);

                break;
            }

        case WM_PAINT:
            {
                control = GetBandwidthMeterData(hwnd);

                PAINTSTRUCT ps;

                HDC hDC = BeginPaint(hwnd, &ps);
                control->Draw(hDC);
                EndPaint(hwnd, &ps);

                break;
            }

        case WM_SIZE:
            {
                control = GetBandwidthMeterData(hwnd);
                control->cx = LOWORD(lParam);
                control->cy = HIWORD(lParam);

                HDC hDC = GetDC(hwnd);
                control->Draw(hDC);
                ReleaseDC(hwnd, hDC);
                break;
            }

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

void BandwidthMeterData::Draw(HDC hDC)
{
    HDC hdcTemp = CreateCompatibleDC(hDC);
    HBITMAP hbmpTemp = CreateCompatibleBitmap(hDC, cx, cy);
    SelectObject(hdcTemp, hbmpTemp);

    RECT clientRect = {0, 0, cx, cy};
    FillRect(hdcTemp, &clientRect, (HBRUSH)COLOR_WINDOW);

    if(strain > 100.0)
        strain = 100.0;

    //--------------------------------

    DWORD green = 0xFF, red = 0xFF;

    if(strain > 50.0)
        green = DWORD(((50.0-(strain-50.0))/50.0)*255.0);

    double redStrain = strain/50.0;
    if(redStrain > 1.0)
        redStrain = 1.0;

    red = DWORD(redStrain*255.0);

    //--------------------------------

    HBRUSH hColorBrush  = CreateSolidBrush((green<<8)|red);
    HBRUSH hOldBrush    = (HBRUSH)SelectObject(hdcTemp, hColorBrush);

    HPEN   hColorPen    = CreatePen(PS_NULL, 0, 0);
    HPEN   hOldPen      = (HPEN)SelectObject(hdcTemp, hColorPen);

    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT hOldFont = (HFONT)SelectObject(hdcTemp, hFont);

    String strKBPS;
    strKBPS << IntString((bytesPerSec*8) >> 10) << TEXT("kb/s");

    SetBkMode(hdcTemp, TRANSPARENT);

    clientRect.top += 6;
    clientRect.right -= 24;
    DrawText(hdcTemp, strKBPS, strKBPS.Length(), &clientRect, DT_TOP|DT_RIGHT);
    //Ellipse(hdcTemp, cx-20, 4, cx-4, 20);

    RECT rc = {cx-20, 4, cx-4, 20};
    FillRect(hdcTemp, &rc, hColorBrush);

    BitBlt(hDC, 0, 0, cx, cy, hdcTemp, 0, 0, SRCCOPY);

    SelectObject(hdcTemp, hOldFont);
    SelectObject(hdcTemp, hOldBrush);
    SelectObject(hdcTemp, hOldPen);

    DeleteObject(hdcTemp);
    DeleteObject(hbmpTemp);

    DeleteObject(hColorBrush);
    DeleteObject(hColorPen);
}

void InitBandwidthMeter()
{
    WNDCLASS wnd;

    wnd.cbClsExtra = 0;
    wnd.cbWndExtra = sizeof(LPVOID);
    wnd.hbrBackground = NULL;
    wnd.hCursor = LoadCursor(NULL, IDC_ARROW);
    wnd.hIcon = NULL;
    wnd.hInstance = hinstMain;
    wnd.lpfnWndProc = BandwidthMeterProc;
    wnd.lpszClassName = BANDWIDTH_METER_CLASS;
    wnd.lpszMenuName = NULL;
    wnd.style = CS_PARENTDC | CS_VREDRAW | CS_HREDRAW;

    if(!RegisterClass(&wnd))
        CrashError(TEXT("Could not register volume control class"));
}
