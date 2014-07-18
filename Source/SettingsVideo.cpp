/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>
 Copyright (C) 2013 Lucas Murray <lmurray@undefinedfire.com>

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

#include "Settings.h"
#include <Dwmapi.h>

//============================================================================
// Helpers

BOOL CALLBACK MonitorInfoEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, List<MonitorInfo> &monitors);

FARPROC editProc = NULL;

LRESULT WINAPI ResolutionEditSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if( ((message == WM_KEYDOWN) && (wParam == VK_RETURN)) ||
        (message == WM_KILLFOCUS) )
    {
        String strText = GetEditText(hwnd);
        if(ValidIntString(strText))
        {
            int iVal = strText.ToInt();

            if(iVal < 128)
                strText = TEXT("128");
            else if(iVal > 4096)
                strText = TEXT("4096");
            else
                return CallWindowProc((WNDPROC)editProc, hwnd, message, wParam, lParam);
        }
        else
            strText = TEXT("128");

        SetWindowText(hwnd, strText);
    }

    return CallWindowProc((WNDPROC)editProc, hwnd, message, wParam, lParam);
}

const int multiplierCount = 9;
const float downscaleMultipliers[multiplierCount] = {1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 2.25f, 2.5f, 2.75f, 3.0f};

//============================================================================
// SettingsVideo class

SettingsVideo::SettingsVideo()
    : SettingsPane()
{
}

SettingsVideo::~SettingsVideo()
{
}

CTSTR SettingsVideo::GetCategory() const
{
    static CTSTR name = Str("Settings.Video");
    return name;
}

HWND SettingsVideo::CreatePane(HWND parentHwnd)
{
    hwnd = OBSCreateDialog(hinstMain, MAKEINTRESOURCE(IDD_SETTINGS_VIDEO), parentHwnd, (DLGPROC)DialogProc, (LPARAM)this);
    return hwnd;
}

void SettingsVideo::DestroyPane()
{
    DestroyWindow(hwnd);
    hwnd = NULL;
}

static int gcd(int a, int b)
{
    if (!a) return b;
    if (!b) return a;

    int c = a;
    a = max(a, b);
    b = min(c, b);

    int remainder;
    do
    {
        remainder = a % b;
        a = b;
        b = remainder;
    } while (remainder);

    return a;
}

static void RefreshAspect(HWND hwnd, int cx, int cy)
{
    int divisor = gcd(cx, cy);

    String aspect = Str("Settings.Video.AspectRatioFormat");
    aspect.FindReplace(L"$1", UIntString(cx / divisor));
    aspect.FindReplace(L"$2", UIntString(cy / divisor));

    SetWindowText(GetDlgItem(hwnd, IDC_ASPECT), aspect.Array());
}

void SettingsVideo::RefreshDownscales(HWND hwnd, int cx, int cy)
{
    int lastID = (int)SendMessage(hwnd, CB_GETCURSEL, 0, 0);

    SendMessage(hwnd, CB_RESETCONTENT, 0, 0);

    float downscale = AppConfig->GetFloat(TEXT("Video"), TEXT("Downscale"));
    bool bFoundVal = false;

    for(int i=0; i<multiplierCount; i++)
    {
        float multiplier = downscaleMultipliers[i];

        int scaleCX = int(float(cx)/multiplier) & 0xFFFFFFFE;
        int scaleCY = int(float(cy)/multiplier) & 0xFFFFFFFE;

        String strText;
        if(i == 0)
            strText << Str("None") << TEXT("  (") << IntString(scaleCX) << TEXT("x") << IntString(scaleCY) << TEXT(")");
        else
            strText << FormattedString(TEXT("%0.2f"), multiplier) << TEXT("  (") << IntString(scaleCX) << TEXT("x") << IntString(scaleCY) << TEXT(")");

        int id = (int)SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)strText.Array());
        SendMessage(hwnd, CB_SETITEMDATA, id, (LPARAM)*(DWORD*)&multiplier);

        if(CloseFloat(downscale, multiplier))
        {
            if(lastID == CB_ERR)
                SendMessage(hwnd, CB_SETCURSEL, id, 0);
            downscale = multiplier;
            bFoundVal = true;
        }
    }

    if(!bFoundVal)
    {
        AppConfig->SetFloat(TEXT("Video"), TEXT("Downscale"), 1.0f);
        if(lastID == CB_ERR)
            SendMessage(hwnd, CB_SETCURSEL, 0, 0);

        SetChangedSettings(true);
    }

    if(lastID != CB_ERR)
        SendMessage(hwnd, CB_SETCURSEL, lastID, 0);
}

void SettingsVideo::RefreshFilters(HWND hwndParent, bool bGetConfig)
{
    HWND hwndFilter = GetDlgItem(hwndParent, IDC_FILTER);
    HWND hwndDownscale = GetDlgItem(hwndParent, IDC_DOWNSCALE);

    int curFilter;
    if(bGetConfig)
        curFilter = AppConfig->GetInt(TEXT("Video"), TEXT("Filter"), 0);
    else
        curFilter = (int)SendMessage(hwndFilter, CB_GETCURSEL, 0, 0);

    float downscale = 1.0f;

    int curSel = (int)SendMessage(hwndDownscale, CB_GETCURSEL, 0, 0);
    if(curSel != CB_ERR)
        downscale = downscaleMultipliers[curSel];

    SendMessage(hwndFilter, CB_RESETCONTENT, 0, 0);
    if(downscale < 2.01)
    {
        SendMessage(hwndFilter, CB_ADDSTRING, 0, (LPARAM)Str("Settings.Video.Filter.Bilinear"));
        SendMessage(hwndFilter, CB_ADDSTRING, 0, (LPARAM)Str("Settings.Video.Filter.Bicubic"));
        SendMessage(hwndFilter, CB_ADDSTRING, 0, (LPARAM)Str("Settings.Video.Filter.Lanczos"));

        SendMessage(hwndFilter, CB_SETCURSEL, curFilter, 0);
    }
    else
    {
        SendMessage(hwndFilter, CB_ADDSTRING, 0, (LPARAM)Str("Settings.Video.Filter.Bilinear"));
        SendMessage(hwndFilter, CB_SETCURSEL, 0, 0);
    }

    EnableWindow(hwndFilter, (downscale > 1.01));
}

void SettingsVideo::ApplySettings()
{
    UINT adapterID = (UINT)SendMessage(GetDlgItem(hwnd, IDC_DEVICE), CB_GETCURSEL, 0, 0);
    if (adapterID == CB_ERR)
        adapterID = 0;

    GlobalConfig->SetInt(TEXT("Video"), TEXT("Adapter"), adapterID);

    int curSel = (int)SendMessage(GetDlgItem(hwnd, IDC_MONITOR), CB_GETCURSEL, 0, 0);
    if(curSel != CB_ERR)
        AppConfig->SetInt(TEXT("Video"), TEXT("Monitor"), curSel);

    int iVal = GetEditText(GetDlgItem(hwnd, IDC_SIZEX)).ToInt();
    if(iVal >=  128)
        AppConfig->SetInt(TEXT("Video"), TEXT("BaseWidth"), iVal);

    iVal = GetEditText(GetDlgItem(hwnd, IDC_SIZEY)).ToInt();
    if(iVal >= 128)
        AppConfig->SetInt(TEXT("Video"), TEXT("BaseHeight"), iVal);

    BOOL bDisableAero = SendMessage(GetDlgItem(hwnd, IDC_DISABLEAERO), BM_GETCHECK, 0, 0) == BST_CHECKED ? TRUE : FALSE;
    AppConfig->SetInt(TEXT("Video"), TEXT("DisableAero"), bDisableAero);

    BOOL bFailed;
    int fps = (int)SendMessage(GetDlgItem(hwnd, IDC_FPS), UDM_GETPOS32, 0, (LPARAM)&bFailed);
    AppConfig->SetInt(TEXT("Video"), TEXT("FPS"), (bFailed) ? 30 : fps);

    curSel = (int)SendMessage(GetDlgItem(hwnd, IDC_DOWNSCALE), CB_GETCURSEL, 0, 0);
    if(curSel != CB_ERR)
        AppConfig->SetFloat(TEXT("Video"), TEXT("Downscale"), downscaleMultipliers[curSel]);

    curSel = (int)SendMessage(GetDlgItem(hwnd, IDC_FILTER), CB_GETCURSEL, 0, 0);
    if(curSel == CB_ERR) curSel = 0;
    AppConfig->SetInt(TEXT("Video"), TEXT("Filter"), curSel);

    int gammaVal = (int)SendMessage(GetDlgItem(hwnd, IDC_GAMMA), TBM_GETPOS, 0, 0);
    AppConfig->SetInt(TEXT("Video"), TEXT("Gamma"), gammaVal);

    //------------------------------------

    if(!App->bRunning)
        App->ResizeWindow(false);
    
    if(OSGetVersion() < 8)
    {
        if (bDisableAero)
        {
            Log(TEXT("Settings::Video: Disabling Aero"));
            DwmEnableComposition(DWM_EC_DISABLECOMPOSITION);
        }
        else
        {
            Log(TEXT("Settings::Video: Enabling Aero"));
            DwmEnableComposition(DWM_EC_ENABLECOMPOSITION);
        }
    }
}

void SettingsVideo::CancelSettings()
{
}

bool SettingsVideo::HasDefaults() const
{
    return false;
}

/*void SettingsVideo::SetDefaults()
{
    SendMessage(GetDlgItem(hwnd, IDC_DEVICE), CB_SETCURSEL, 0, 0);
    SendMessage(GetDlgItem(hwnd, IDC_DISABLEAERO), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwnd, IDC_FILTER), CB_SETCURSEL, 0, 0);

    ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
    SetChangedSettings(true);
}*/

INT_PTR SettingsVideo::ProcMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND hwndTemp;
    switch(message)
    {
        case WM_INITDIALOG:
            {
                LocalizeWindow(hwnd);

                if (LocaleIsRTL())
                {
                    RECT xRect, yRect;
                    GetWindowRect(GetDlgItem(hwnd, IDC_SIZEX), &xRect);
                    MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&xRect.left, 2);
                    GetWindowRect(GetDlgItem(hwnd, IDC_SIZEY), &yRect);
                    MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&yRect.left, 2);

                    SetWindowPos(GetDlgItem(hwnd, IDC_SIZEX), nullptr, yRect.left, yRect.top, 0, 0, SWP_NOSIZE);
                    SetWindowPos(GetDlgItem(hwnd, IDC_SIZEY), nullptr, xRect.left, xRect.top, 0, 0, SWP_NOSIZE);
                }

                //--------------------------------------------

                HWND hwndToolTip = CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL, WS_POPUP|TTS_NOPREFIX|TTS_ALWAYSTIP,
                    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                    hwnd, NULL, hinstMain, NULL);

                TOOLINFO ti;
                zero(&ti, sizeof(ti));
                ti.cbSize = sizeof(ti);
                ti.uFlags = TTF_SUBCLASS|TTF_IDISHWND;
                ti.hwnd = hwnd;

                if (LocaleIsRTL())
                    ti.uFlags |= TTF_RTLREADING;

                SendMessage(hwndToolTip, TTM_SETMAXTIPWIDTH, 0, 500);
                SendMessage(hwndToolTip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 8000);

                //--------------------------------------------

                DeviceOutputs outputs;
                GetDisplayDevices(outputs);

                hwndTemp = GetDlgItem(hwnd, IDC_DEVICE);
                for (UINT i=0; i<outputs.devices.Num(); i++) {
                    SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)outputs.devices[i].strDevice.Array());
                }

                UINT adapterID = GlobalConfig->GetInt(TEXT("Video"), TEXT("Adapter"), 0);
                if (adapterID >= outputs.devices.Num())
                    adapterID = 0;
                SendMessage(hwndTemp, CB_SETCURSEL, adapterID, 0);

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_MONITOR);

                App->monitors.Clear();
                EnumDisplayMonitors(NULL, NULL, (MONITORENUMPROC)MonitorInfoEnumProc, (LPARAM)&App->monitors);

                for(UINT i=0; i<App->monitors.Num(); i++)
                    SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)IntString(i+1).Array());

                int monitorID = LoadSettingComboInt(hwndTemp, TEXT("Video"), TEXT("Monitor"), 0, App->monitors.Num()-1);
                if(monitorID > (int)App->monitors.Num())
                    monitorID = 0;

                //--------------------------------------------

                SendMessage(GetDlgItem(hwnd, IDC_USECUSTOM), BM_SETCHECK, BST_CHECKED, 0);
                EnableWindow(GetDlgItem(hwnd, IDC_MONITOR), FALSE);

                //--------------------------------------------

                int cx, cy;
                if(!AppConfig->HasKey(TEXT("Video"), TEXT("BaseWidth")) || !AppConfig->HasKey(TEXT("Video"), TEXT("BaseHeight")))
                {
                    cx = App->monitors[monitorID].rect.right  - App->monitors[monitorID].rect.left;
                    cy = App->monitors[monitorID].rect.bottom - App->monitors[monitorID].rect.top;
                    AppConfig->SetInt(TEXT("Video"), TEXT("BaseWidth"),  cx);
                    AppConfig->SetInt(TEXT("Video"), TEXT("BaseHeight"), cy);
                }
                else
                {
                    cx = AppConfig->GetInt(TEXT("Video"), TEXT("BaseWidth"));
                    cy = AppConfig->GetInt(TEXT("Video"), TEXT("BaseHeight"));

                    if(cx < 128)        cx = 128;
                    else if(cx > 4096)  cx = 4096;

                    if(cy < 128)        cy = 128;
                    else if(cy > 4096)  cy = 4096;
                }

                RefreshAspect(hwnd, cx, cy);


                hwndTemp = GetDlgItem(hwnd, IDC_SIZEX);
                editProc = (FARPROC)GetWindowLongPtr(hwndTemp, GWLP_WNDPROC);
                SetWindowLongPtr(hwndTemp, GWLP_WNDPROC, (LONG_PTR)ResolutionEditSubclassProc);
                SetWindowText(hwndTemp, IntString(cx).Array());

                hwndTemp = GetDlgItem(hwnd, IDC_SIZEY);
                SetWindowLongPtr(hwndTemp, GWLP_WNDPROC, (LONG_PTR)ResolutionEditSubclassProc);
                SetWindowText(hwndTemp, IntString(cy).Array());

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_DISABLEAERO);

                if(OSGetVersion() == 8)
                    EnableWindow(hwndTemp, FALSE);

                BOOL bDisableAero = AppConfig->GetInt(TEXT("Video"), TEXT("DisableAero"), 0);
                SendMessage(hwndTemp, BM_SETCHECK, bDisableAero ? BST_CHECKED : 0, 0);

                ti.lpszText = (LPWSTR)Str("Settings.Video.DisableAeroTooltip");
                ti.uId = (UINT_PTR)hwndTemp;
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                //--------------------------------------------

                BOOL bUnlockFPS = AppConfig->GetInt(TEXT("Video"), TEXT("UnlockFPS"));
                int topFPS = bUnlockFPS ? 120 : 60;

                hwndTemp = GetDlgItem(hwnd, IDC_FPS);
                SendMessage(hwndTemp, UDM_SETRANGE32, 1, topFPS);

                int fps = AppConfig->GetInt(TEXT("Video"), TEXT("FPS"), 30);
                if(!AppConfig->HasKey(TEXT("Video"), TEXT("FPS")))
                {
                    AppConfig->SetInt(TEXT("Video"), TEXT("FPS"), 30);
                    fps = 30;
                }
                else if(fps < 1)
                {
                    AppConfig->SetInt(TEXT("Video"), TEXT("FPS"), 1);
                    fps = 1;
                }
                else if(fps > topFPS)
                {
                    AppConfig->SetInt(TEXT("Video"), TEXT("FPS"), topFPS);
                    fps = topFPS;
                }

                SendMessage(hwndTemp, UDM_SETPOS32, 0, fps);

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_DOWNSCALE);
                RefreshDownscales(hwndTemp, cx, cy);

                ti.lpszText = (LPWSTR)Str("Settings.Video.DownscaleTooltip");
                ti.uId = (UINT_PTR)hwndTemp;
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                //--------------------------------------------

                RefreshFilters(hwnd, true);

                //--------------------------------------------

                ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_HIDE);
                SetChangedSettings(false);

                return TRUE;
            }

        case WM_COMMAND:
            {
                bool bDataChanged = false;

                switch(LOWORD(wParam))
                {
                    case IDC_MONITOR:
                        {
                            if(HIWORD(wParam) != CBN_SELCHANGE)
                                break;

                            int sel = (int)SendMessage(GetDlgItem(hwnd, IDC_MONITOR), CB_GETCURSEL, 0, 0);
                            if(sel != CB_ERR)
                            {
                                if(sel >= (int)App->monitors.Num())
                                    sel = 0;

                                MonitorInfo &monitor = App->monitors[sel];

                                int cx, cy;
                                cx = monitor.rect.right  - monitor.rect.left;
                                cy = monitor.rect.bottom - monitor.rect.top;

                                SetWindowText(GetDlgItem(hwnd, IDC_SIZEX), IntString(cx).Array());
                                SetWindowText(GetDlgItem(hwnd, IDC_SIZEY), IntString(cy).Array());

                                RefreshAspect(hwnd, cx, cy);
                            }
                            break;
                        }

                    case IDC_USECUSTOM:
                        SendMessage(GetDlgItem(hwnd, IDC_SIZEX), EM_SETREADONLY, FALSE, 0);
                        SendMessage(GetDlgItem(hwnd, IDC_SIZEY), EM_SETREADONLY, FALSE, 0);
                        EnableWindow(GetDlgItem(hwnd, IDC_MONITOR), FALSE);
                        break;

                    case IDC_USEMONITOR:
                        {
                            SendMessage(GetDlgItem(hwnd, IDC_SIZEX), EM_SETREADONLY, TRUE, 0);
                            SendMessage(GetDlgItem(hwnd, IDC_SIZEY), EM_SETREADONLY, TRUE, 0);
                            EnableWindow(GetDlgItem(hwnd, IDC_MONITOR), TRUE);

                            int sel = (int)SendMessage(GetDlgItem(hwnd, IDC_MONITOR), CB_GETCURSEL, 0, 0);
                            if(sel != CB_ERR)
                            {
                                if(sel >= (int)App->monitors.Num())
                                    sel = 0;

                                MonitorInfo &monitor = App->monitors[sel];

                                int cx, cy;
                                cx = monitor.rect.right  - monitor.rect.left;
                                cy = monitor.rect.bottom - monitor.rect.top;

                                SetWindowText(GetDlgItem(hwnd, IDC_SIZEX), IntString(cx).Array());
                                SetWindowText(GetDlgItem(hwnd, IDC_SIZEY), IntString(cy).Array());

                                RefreshAspect(hwnd, cx, cy);
                            }
                            break;
                        }

                    case IDC_SIZEX:
                    case IDC_SIZEY:
                        {
                            if(HIWORD(wParam) != EN_CHANGE)
                                break;

                            int cx = GetEditText(GetDlgItem(hwnd, IDC_SIZEX)).ToInt();
                            int cy = GetEditText(GetDlgItem(hwnd, IDC_SIZEY)).ToInt();

                            if(cx < 128)        cx = 128;
                            else if(cx > 4096)  cx = 4096;

                            if(cy < 128)        cy = 128;
                            else if(cy > 4096)  cy = 4096;

                            RefreshDownscales(GetDlgItem(hwnd, IDC_DOWNSCALE), cx, cy);

                            RefreshAspect(hwnd, cx, cy);

                            bDataChanged = true;
                            break;
                        }

                    case IDC_DISABLEAERO:
                        if(HIWORD(wParam) == BN_CLICKED)
                            bDataChanged = true;
                        break;

                    case IDC_FPS_EDIT:
                        if(HIWORD(wParam) == EN_CHANGE)
                            bDataChanged = true;
                        break;

                    case IDC_DEVICE:
                    case IDC_FILTER:
                        if(HIWORD(wParam) == CBN_SELCHANGE)
                            bDataChanged = true;
                        break;

                    case IDC_DOWNSCALE:
                        if(HIWORD(wParam) == CBN_SELCHANGE)
                        {
                            bDataChanged = true;
                            RefreshFilters(hwnd, false);
                        }
                        break;
                }

                if(bDataChanged)
                {
                    if (App->GetVideoEncoder())
                        ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                    SetChangedSettings(true);
                }
                break;
            }
    }
    return FALSE;
}
