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


#include "OBSAPI.h"

#include <Xinput.h>
#define XINPUT_GAMEPAD_LEFT_TRIGGER    0x0400
#define XINPUT_GAMEPAD_RIGHT_TRIGGER   0x0800
#define IDT_XINPUT_HOTKEY_TIMER 1337

//basically trying to do the same as the windows default hotkey control, but with mouse button support.

struct HotkeyControlExData
{
    DWORD hotkeyVK, modifiers;
    DWORD xinputNum, xinputButton;

    bool  bHasFocus;
    long  cx,cy;

    HFONT hFont;
    int fontHeight;

    void DrawHotkeyControlEx(HWND hwnd, HDC hDC);
};

inline HotkeyControlExData* GetHotkeyControlExData(HWND hwnd)
{
    return (HotkeyControlExData*)GetWindowLongPtr(hwnd, 0);
}


LRESULT CALLBACK HotkeyExProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    HotkeyControlExData *control;

    switch(message)
    {
        case WM_NCCREATE:
            {
                CREATESTRUCT *pCreateData = (CREATESTRUCT*)lParam;

                SetWindowLongPtr(hwnd, GWL_EXSTYLE, WS_EX_STATICEDGE);

                control = (HotkeyControlExData*)malloc(sizeof(HotkeyControlExData));
                zero(control, sizeof(HotkeyControlExData));
                SetWindowLongPtr(hwnd, 0, (LONG_PTR)control);

                control->hotkeyVK = 0;
                control->modifiers = 0;
                control->xinputNum = 0;
                control->xinputButton = 0;

                control->cx = pCreateData->cx;
                control->cy = pCreateData->cy;

                control->hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

                return TRUE;
            }

        case WM_CREATE:
            {
                control = GetHotkeyControlExData(hwnd);
                HDC hDC = GetDC(hwnd);
                SIZE size;

                SelectObject(hDC, control->hFont);
                GetTextExtentPoint32(hDC, TEXT("C"), 1, &size);
                control->fontHeight = size.cy;
                ReleaseDC(hwnd, hDC);
                break;
            }

        case WM_SETFOCUS:
            {
                control = GetHotkeyControlExData(hwnd);
                InvalidateRect(hwnd, NULL, TRUE);
                CreateCaret(hwnd, NULL, 0, control->fontHeight);
                control->bHasFocus = true;
                int x = GetSystemMetrics(SM_CXEDGE);
                int y = GetSystemMetrics(SM_CYEDGE);
                SetCaretPos(x-1, y);
                ShowCaret(hwnd);
                SetTimer(hwnd, IDT_XINPUT_HOTKEY_TIMER, 100, NULL);
                break;
            }

        case WM_KILLFOCUS:
            {
                control = GetHotkeyControlExData(hwnd);
                control->bHasFocus = false;
                DestroyCaret();

                InvalidateRect(hwnd, NULL, TRUE);
                KillTimer(hwnd, IDT_XINPUT_HOTKEY_TIMER);
                break;
            }

        case WM_GETDLGCODE:
            return DLGC_WANTCHARS | DLGC_WANTARROWS;

        case HKM_SETHOTKEY:
            {
                control = GetHotkeyControlExData(hwnd);
                if(HIWORD(wParam))
                {
                    control->hotkeyVK = 0;
                    control->modifiers = 0;
                    control->xinputButton = HIWORD(wParam);
                    control->xinputNum = LOWORD(wParam);
                }
                else
                {
                    control->hotkeyVK = LOBYTE(wParam);
                    control->modifiers = HIBYTE(wParam);
                    control->xinputButton = 0;
                    control->xinputNum = 0;
                }
                InvalidateRect(hwnd, NULL, TRUE);
                break;
            }

        case HKM_GETHOTKEY:
            {
                control = GetHotkeyControlExData(hwnd);
                if(control->xinputButton)
                    return MAKELONG(control->xinputNum, control->xinputButton);
                else
                    return MAKEWORD(control->hotkeyVK, control->modifiers);
            }

        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
            {
                control = GetHotkeyControlExData(hwnd);

                if( wParam == VK_RETURN ||
                    wParam == VK_TAB    ||
                    wParam == VK_DELETE ||
                    wParam == VK_ESCAPE ||
                    wParam == VK_LWIN   ||
                    wParam == VK_RWIN   ||
                    wParam == VK_APPS   )
                {
                    if(control->hotkeyVK != 0 || control->modifiers != 0)
                    {
                        control->hotkeyVK = 0;
                        control->modifiers = 0;
                        control->xinputButton = 0;
                        control->xinputNum = 0;

                        InvalidateRect(hwnd, NULL, TRUE);
                        PostMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hwnd), EN_CHANGE), (LPARAM)hwnd);
                    }

                    return DefWindowProc(hwnd, message, wParam, lParam);
                }

                DWORD hotkeyVK = 0;
                if(wParam != VK_MENU && wParam != VK_SHIFT && wParam != VK_CONTROL)
                    hotkeyVK = (DWORD)wParam;

                DWORD modifiers = 0;
                if((GetKeyState(VK_CONTROL) & 0x8000) != 0)
                    modifiers |= HOTKEYF_CONTROL;
                if((GetKeyState(VK_SHIFT) & 0x8000) != 0)
                    modifiers |= HOTKEYF_SHIFT;
                if((GetKeyState(VK_MENU) & 0x8000) != 0)
                    modifiers |= HOTKEYF_ALT;

                bool bExtendedKey = (lParam & 0x01000000) != 0;
                if(bExtendedKey)
                    modifiers |= HOTKEYF_EXT;

                if((hotkeyVK && control->hotkeyVK != hotkeyVK) || control->modifiers != modifiers)
                {
                    control->hotkeyVK  = hotkeyVK;
                    control->modifiers = modifiers;
                    control->xinputButton = 0;
                    control->xinputNum = 0;

                    InvalidateRect(hwnd, NULL, TRUE);
                    PostMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hwnd), EN_CHANGE), (LPARAM)hwnd);
                }
                break;
            }

        case WM_LBUTTONDOWN:
            {
                control = GetHotkeyControlExData(hwnd);
                if(!control->bHasFocus)
                    SetFocus(hwnd);
                break;;
            }
        case WM_RBUTTONDOWN:
            break;

        case WM_MBUTTONDOWN:
        case WM_XBUTTONDOWN:
            {
                control = GetHotkeyControlExData(hwnd);

                DWORD modifiers = 0;
                if((GetKeyState(VK_CONTROL) & 0x8000) != 0)
                    modifiers |= HOTKEYF_CONTROL;
                if((GetKeyState(VK_SHIFT) & 0x8000) != 0)
                    modifiers |= HOTKEYF_SHIFT;
                if((GetKeyState(VK_MENU) & 0x8000) != 0)
                    modifiers |= HOTKEYF_ALT;

                DWORD hotkeyVK = 0;
                switch(message)
                {
                    case WM_LBUTTONDOWN: hotkeyVK = VK_LBUTTON; break;
                    case WM_RBUTTONDOWN: hotkeyVK = VK_RBUTTON; break;
                    case WM_MBUTTONDOWN: hotkeyVK = VK_MBUTTON; break;
                    case WM_XBUTTONDOWN:
                        if(GET_XBUTTON_WPARAM(wParam) == XBUTTON1)
                            hotkeyVK = VK_XBUTTON1;
                        else if(GET_XBUTTON_WPARAM(wParam) == XBUTTON2)
                            hotkeyVK = VK_XBUTTON2;
                }

                if(control->hotkeyVK != hotkeyVK || control->modifiers != modifiers)
                {
                    control->hotkeyVK = hotkeyVK;
                    control->modifiers = modifiers;
                    control->xinputButton = 0;
                    control->xinputNum = 0;

                    InvalidateRect(hwnd, NULL, TRUE);
                    PostMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hwnd), EN_CHANGE), (LPARAM)hwnd);
                }
                break;
            }

        case WM_TIMER:
            {
                control = GetHotkeyControlExData(hwnd);

                for(int i = 0; i < 4; ++i)
                {
                    XINPUT_STATE state = { 0 };

                    if(XInputGetState(i, &state) != ERROR_SUCCESS)
                        continue;

                    if(state.Gamepad.bLeftTrigger >= 85)
                        state.Gamepad.wButtons |= XINPUT_GAMEPAD_LEFT_TRIGGER;

                    if(state.Gamepad.bRightTrigger >= 85)
                        state.Gamepad.wButtons |= XINPUT_GAMEPAD_RIGHT_TRIGGER;

                    if(state.Gamepad.wButtons == 0)
                        continue;

                    // Continue if more than exactly one button is pressed
                    if(state.Gamepad.wButtons & (state.Gamepad.wButtons - 1))
                        continue;

                    control->hotkeyVK = 0;
                    control->modifiers = 0;
                    control->xinputButton = state.Gamepad.wButtons;
                    control->xinputNum = i;

                    InvalidateRect(hwnd, NULL, TRUE);
                    PostMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hwnd), EN_CHANGE), (LPARAM)hwnd);

                    break;
                }

                break;
            }

        case WM_PAINT:
            {
                control = GetHotkeyControlExData(hwnd);

                PAINTSTRUCT ps;

                HDC hDC = BeginPaint(hwnd, &ps);
                control->DrawHotkeyControlEx(hwnd, hDC);
                EndPaint(hwnd, &ps);
                break;
            }

        case WM_ERASEBKGND:
            {
                control = GetHotkeyControlExData(hwnd);

                HideCaret(hwnd);

                RECT rc;

                HDC hDC = GetDC(hwnd);
                GetClientRect(hwnd, &rc);
                if(IsWindowEnabled(hwnd))
                    FillRect(hDC, &rc, (HBRUSH)(COLOR_WINDOW+1));
                else
                    FillRect(hDC, &rc, (HBRUSH)(COLOR_BTNFACE+1));
                ReleaseDC(hwnd, hDC);

                ShowCaret(hwnd);
                break;
            }

        case WM_GETFONT:
            {
                control = GetHotkeyControlExData(hwnd);
                return (LRESULT)control->hFont;
            }

        case WM_SETFONT:
            {
                control = GetHotkeyControlExData(hwnd);
                control->hFont = (HFONT)wParam;

                HDC hDC = GetDC(hwnd);
                SIZE size;

                SelectObject(hDC, control->hFont);
                GetTextExtentPoint32(hDC, TEXT("C"), 1, &size);
                control->fontHeight = size.cy;

                control->DrawHotkeyControlEx(hwnd, hDC);
                ReleaseDC(hwnd, hDC);
                break;
            }

        case WM_ENABLE:
            InvalidateRect(hwnd, NULL, TRUE);
            //pass through

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}



void HotkeyControlExData::DrawHotkeyControlEx(HWND hwnd, HDC hDC)
{
    TCHAR lpName[128];
    String strText;

    if(xinputButton)
    {
        strText << FormattedString(TEXT("XPad%d "), xinputNum);

        switch(xinputButton)
        {
            case XINPUT_GAMEPAD_DPAD_UP:
            strText << "Up";
            break;
            case XINPUT_GAMEPAD_DPAD_DOWN:
            strText << "Down";
            break;
            case XINPUT_GAMEPAD_DPAD_LEFT:
            strText << "Left";
            break;
            case XINPUT_GAMEPAD_DPAD_RIGHT:
            strText << "Right";
            break;
            case XINPUT_GAMEPAD_START:
            strText << "Start";
            break;
            case XINPUT_GAMEPAD_BACK:
            strText << "Back";
            break;
            case XINPUT_GAMEPAD_LEFT_THUMB:
            strText << "LThumb";
            break;
            case XINPUT_GAMEPAD_RIGHT_THUMB:
            strText << "RThumb";
            break;
            case XINPUT_GAMEPAD_LEFT_SHOULDER:
            strText << "LButton";
            break;
            case XINPUT_GAMEPAD_RIGHT_SHOULDER:
            strText << "RButton";
            break;
            case XINPUT_GAMEPAD_LEFT_TRIGGER:
            strText << "LTrigger";
            break;
            case XINPUT_GAMEPAD_RIGHT_TRIGGER:
            strText << "RTrigger";
            break;
            case XINPUT_GAMEPAD_A:
            strText << "A";
            break;
            case XINPUT_GAMEPAD_B:
            strText << "B";
            break;
            case XINPUT_GAMEPAD_X:
            strText << "X";
            break;
            case XINPUT_GAMEPAD_Y:
            strText << "Y";
            break;
            default:
            strText << "Unknown";
        }
    }
    else if(hotkeyVK || modifiers)
    {
        bool bAdd = false;
        if(modifiers & HOTKEYF_CONTROL)
        {
            GetKeyNameText((LONG)MapVirtualKey(VK_CONTROL, 0) << 16, lpName, 127);
            strText << lpName;

            bAdd = true;
        }
        if(modifiers & HOTKEYF_SHIFT)
        {
            if(bAdd) strText << TEXT(" + ");
            GetKeyNameText((LONG)MapVirtualKey(VK_SHIFT, 0) << 16, lpName, 127);
            strText << lpName;

            bAdd = true;
        }
        if(modifiers & HOTKEYF_ALT)
        {
            if(bAdd) strText << TEXT(" + ");
            GetKeyNameText((LONG)MapVirtualKey(VK_MENU, 0) << 16, lpName, 127);
            strText << lpName;

            bAdd = true;
        }

        if(hotkeyVK)
        {
            if(bAdd) strText << TEXT(" + ");

            if(hotkeyVK <= VK_RBUTTON)
                strText << TEXT("Mouse ") << UIntString(hotkeyVK);
            else if(hotkeyVK > VK_CANCEL && hotkeyVK <= VK_XBUTTON2)
                strText << TEXT("Mouse ") << UIntString(hotkeyVK-1);
            else
            {
                UINT scanCode = MapVirtualKey(hotkeyVK, 0) << 16;
                if(modifiers & HOTKEYF_EXT)
                    scanCode |= 0x01000000;

                GetKeyNameText(scanCode, lpName, 128);
                strText << lpName;
            }
        }
    }
    else
        strText << Str("None");

    if(bHasFocus)
        HideCaret(hwnd);

    int x = GetSystemMetrics(SM_CXEDGE);
    int y = GetSystemMetrics(SM_CYEDGE);

    SelectObject(hDC, hFont);
    if(IsWindowEnabled(hwnd))
    {
        SetBkColor(hDC, GetSysColor(COLOR_WINDOW));
        SetTextColor(hDC, GetSysColor(COLOR_WINDOWTEXT));
    }
    else
    {
        SetBkColor(hDC, GetSysColor(COLOR_BTNFACE));
        SetTextColor(hDC, GetSysColor(COLOR_GRAYTEXT));
    }

    TextOut(hDC, x, y, strText, strText.Length());

    if(bHasFocus)
        ShowCaret(hwnd);
}


void InitHotkeyExControl(HINSTANCE hInstance)
{
    WNDCLASS wnd;

    wnd.cbClsExtra = 0;
    wnd.cbWndExtra = sizeof(LPVOID);
    wnd.hbrBackground = NULL;
    wnd.hCursor = LoadCursor(NULL, IDC_ARROW);
    wnd.hIcon = NULL;
    wnd.hInstance = hInstance;
    wnd.lpfnWndProc = HotkeyExProc;
    wnd.lpszClassName = HOTKEY_CONTROL_EX_CLASS;
    wnd.lpszMenuName = NULL;
    wnd.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;

    if(!RegisterClass(&wnd))
        CrashError(TEXT("Could not register hotkey control class"));
}
