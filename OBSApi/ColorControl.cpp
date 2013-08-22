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


#include "OBSApi.h"


struct CCStruct
{
    DWORD structSize;       //size of this structure
    DWORD curColor;         //current color stored
    BOOL  bDisabled;        //whether the control is disabled or not

    long  cx,cy;            //size of control

    DWORD custColors[16];   //custom colors in the colors dialog box
};



void WINAPI DrawColorControl(HDC hDC, CCStruct *pCCData);
LRESULT WINAPI ColorControlProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);


static DWORD customColors[16] = {0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF,
                                 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF,
                                 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF,
                                 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF};

void CCSetCustomColors(DWORD *colors)
{
    memcpy(customColors, colors, sizeof(DWORD)*16);
}

void CCGetCustomColors(DWORD *colors)
{
    memcpy(colors, customColors, sizeof(DWORD)*16);
}


LRESULT WINAPI ColorControlProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    CCStruct *pCCData;

    switch(message)
    {
        case WM_NCCREATE:
            {
                CREATESTRUCT *pCreateData = (CREATESTRUCT*)lParam;

                //add custom data to window
                pCCData = (CCStruct*)malloc(sizeof(CCStruct));
                zero(pCCData, sizeof(CCStruct));
                SetWindowLongPtr(hwnd, 0, (LONG_PTR)pCCData);

                pCCData->structSize = sizeof(CCStruct);
                pCCData->curColor = 0xFFFFFF;
                pCCData->bDisabled = ((pCreateData->style & WS_DISABLED) != 0);

                pCCData->cx = pCreateData->cx;
                pCCData->cy = pCreateData->cy;

                for(int i=0; i<16; i++) pCCData->custColors[i] = 0xC0C0C0;

                return TRUE;
            }

        case WM_DESTROY:
            {
                pCCData = (CCStruct*)GetWindowLongPtr(hwnd, 0);

                if(pCCData)
                    free(pCCData);

                break;
            }

        case WM_PAINT:
            {
                pCCData = (CCStruct*)GetWindowLongPtr(hwnd, 0);

                PAINTSTRUCT ps;

                HDC hDC = BeginPaint(hwnd, &ps);
                DrawColorControl(hDC, pCCData);
                EndPaint(hwnd, &ps);

                break;
            }

        case WM_ENABLE:
            {
                pCCData = (CCStruct*)GetWindowLongPtr(hwnd, 0);

                pCCData->bDisabled = !wParam;

                //redraw control
                HDC hDC = GetDC(hwnd);
                DrawColorControl(hDC, pCCData);
                ReleaseDC(hwnd, hDC);

                break;
            }

        case WM_LBUTTONDBLCLK:
            {
                pCCData = (CCStruct*)GetWindowLongPtr(hwnd, 0);

                if(pCCData->bDisabled)
                    break;

                CHOOSECOLOR chooserData;
                zero(&chooserData, sizeof(chooserData));

                chooserData.lStructSize = sizeof(chooserData);
                chooserData.hwndOwner = GetParent(hwnd);
                chooserData.Flags = CC_RGBINIT | CC_FULLOPEN;
                chooserData.rgbResult = pCCData->curColor;
                chooserData.lpCustColors = customColors;

                if(ChooseColor(&chooserData))
                {
                    pCCData->curColor = chooserData.rgbResult;

                    HDC hDC = GetDC(hwnd);
                    DrawColorControl(hDC, pCCData);
                    ReleaseDC(hwnd, hDC);

                    HWND hwndParent = GetParent(hwnd);

                    DWORD controlID = (DWORD)GetWindowLongPtr(hwnd, GWLP_ID);
                    SendMessage(hwndParent, WM_COMMAND, MAKEWPARAM(controlID, CCN_CHANGED), (LPARAM)hwnd);
                }

                break;
            }

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

void WINAPI DrawColorControl(HDC hDC, CCStruct *pCCData)
{
    HDC hdcTemp;
    HBITMAP hBmp, hbmpOld;
    HBRUSH hBrush;
    RECT rect;

    //Create temp draw data
    hdcTemp = CreateCompatibleDC(hDC);
    hBmp = CreateCompatibleBitmap(hDC, pCCData->cx, pCCData->cy);
    hbmpOld = (HBITMAP)SelectObject(hdcTemp, hBmp);


    //draw the outline
    hBrush = CreateSolidBrush(INVALID);

    rect.top    = rect.left = 0;
    rect.right  = pCCData->cx;
    rect.bottom = pCCData->cy;
    FillRect(hdcTemp, &rect, hBrush);

    DeleteObject(hBrush);


    //draw the color
    hBrush = CreateSolidBrush(pCCData->bDisabled ? 0x808080 : pCCData->curColor);

    rect.top    = rect.left = 1;
    rect.right  = pCCData->cx-1;
    rect.bottom = pCCData->cy-1;
    FillRect(hdcTemp, &rect, hBrush);

    DeleteObject(hBrush);


    //Copy drawn data back onto the main device context
    BitBlt(hDC, 0, 0, pCCData->cx, pCCData->cy, hdcTemp, 0, 0, SRCCOPY);

    //Delete temp draw data
    SelectObject(hdcTemp, hbmpOld);
    DeleteObject(hBmp);
    DeleteDC(hdcTemp);
}


void InitColorControl(HINSTANCE hInstance)
{
    WNDCLASS wnd;

    wnd.cbClsExtra = 0;
    wnd.cbWndExtra = sizeof(LPVOID);
    wnd.hbrBackground = NULL;
    wnd.hCursor = LoadCursor(NULL, IDC_ARROW);
    wnd.hIcon = NULL;
    wnd.hInstance = hInstance;
    wnd.lpfnWndProc = ColorControlProc;
    wnd.lpszClassName = COLOR_CONTROL_CLASS;
    wnd.lpszMenuName = NULL;
    wnd.style = CS_PARENTDC | CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;

    RegisterClass(&wnd);
}

DWORD CCGetColor(HWND hwnd)
{
    if(!hwnd)
        return 0;

    CCStruct *pCCData = (CCStruct*)GetWindowLongPtr(hwnd, 0);

    DWORD color = REVERSE_COLOR(pCCData->curColor); //MAKERGB(RGB_B(pCCData->curColor), RGB_G(pCCData->curColor), RGB_R(pCCData->curColor));

    return color | 0xFF000000;
}

void  CCSetColor(HWND hwnd, DWORD color)
{
    if(!hwnd)
        return;

    color &= 0xFFFFFF;

    color = REVERSE_COLOR(color);

    CCStruct *pCCData = (CCStruct*)GetWindowLongPtr(hwnd, 0);

    pCCData->curColor = color;

    HDC hDC = GetDC(hwnd);
    DrawColorControl(hDC, pCCData);
    ReleaseDC(hwnd, hDC);
}

void  CCSetColor(HWND hwnd, const Color3 &color)
{
    CCSetColor(hwnd, (DWORD)Vect_to_RGB(color));
}
