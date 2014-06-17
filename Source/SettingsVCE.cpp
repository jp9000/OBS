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

bool CheckVCEHardwareSupport(bool log, bool useMFT);

int checkRange(int val, int min, int max, int def)
{
    if (val >= min && val <= max)
        return val;
    return def;
}

//============================================================================
// SettingsVCE class

SettingsVCE::SettingsVCE()
: SettingsPane()
{
}

SettingsVCE::~SettingsVCE()
{
}

CTSTR SettingsVCE::GetCategory() const
{
    static CTSTR name = Str("Settings.Encoding.VCE");
    return name;
}

HWND SettingsVCE::CreatePane(HWND parentHwnd)
{
    hwnd = OBSCreateDialog(hinstMain, MAKEINTRESOURCE(IDD_SETTINGS_VCE), parentHwnd, (DLGPROC)DialogProc, (LPARAM)this);
    return hwnd;
}

void SettingsVCE::DestroyPane()
{
    DestroyWindow(hwnd);
    hwnd = NULL;
}

void SettingsVCE::ApplySettings()
{
    bool bBool = false;
    int iInt = 0;

    bBool = SendMessage(GetDlgItem(hwnd, IDC_VCE_USE_CUSTOM), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("UseCustom"), bBool);

    bBool = SendMessage(GetDlgItem(hwnd, IDC_VCE_CABAC), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("CABAC"), bBool);

    bBool = SendMessage(GetDlgItem(hwnd, IDC_VCE_AMD), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("AMD"), bBool);

    bBool = SendMessage(GetDlgItem(hwnd, IDC_VCE_ME_HALF), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("MEHalf"), bBool);

    bBool = SendMessage(GetDlgItem(hwnd, IDC_VCE_ME_QUARTER), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("MEQuarter"), bBool);

    bBool = SendMessage(GetDlgItem(hwnd, IDC_VCE_FORCE16SKIP), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("Force16x16Skip"), bBool);

    bBool = SendMessage(GetDlgItem(hwnd, IDC_VCE_PMV), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("DisFavorPMV"), bBool);

    bBool = SendMessage(GetDlgItem(hwnd, IDC_VCE_ZPC), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("ForceZPC"), bBool);

    bBool = SendMessage(GetDlgItem(hwnd, IDC_VCE_IME_DEC), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("IMEDecimation"), bBool);

    bBool = SendMessage(GetDlgItem(hwnd, IDC_VCE_CONSTINTRAPRED), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("ConstIntraPred"), bBool);

    bBool = SendMessage(GetDlgItem(hwnd, IDC_VCE_SATD), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("DisSATD"), bBool);

    iInt = GetEditText(GetDlgItem(hwnd, IDC_VCE_GOP)).ToInt();
    iInt = checkRange(iInt, 0, 0xFFFF, 0);
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("GOPSize"), iInt);

    iInt = GetEditText(GetDlgItem(hwnd, IDC_VCE_IDR)).ToInt();
    iInt = checkRange(iInt, 0, 0xFFFF, 0);
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("IDRPeriod"), iInt);

    iInt = GetEditText(GetDlgItem(hwnd, IDC_VCE_IPIC)).ToInt();
    iInt = checkRange(iInt, 0, 0xFFFF, 0);
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("IPicPeriod"), iInt);

    iInt = GetEditText(GetDlgItem(hwnd, IDC_VCE_SRNG0)).ToInt();
    iInt = checkRange(iInt, 1, 36, 16);
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("SearchX"), iInt);

    iInt = GetEditText(GetDlgItem(hwnd, IDC_VCE_SRNG1)).ToInt();
    iInt = checkRange(iInt, 1, 36, 16);
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("SearchY"), iInt);

    iInt = GetEditText(GetDlgItem(hwnd, IDC_VCE_SRNG_IMEX)).ToInt();
    iInt = checkRange(iInt, 0, 4, 4);
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("IME2SearchX"), iInt);

    iInt = GetEditText(GetDlgItem(hwnd, IDC_VCE_SRNG_IMEY)).ToInt();
    iInt = checkRange(iInt, 0, 0xFFFF, 4);
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("IME2SearchY"), iInt);

    iInt = GetEditText(GetDlgItem(hwnd, IDC_VCE_IPRED)).ToInt();
    iInt = checkRange(iInt, 0, 255, 0);
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("IPred"), iInt);

    iInt = GetEditText(GetDlgItem(hwnd, IDC_VCE_PPRED)).ToInt();
    iInt = checkRange(iInt, 0, 255, 0);
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("PPred"), iInt);

    iInt = GetEditText(GetDlgItem(hwnd, IDC_VCE_IME_OVERW)).ToInt();
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("IMEOverwite"), iInt);

    iInt = GetEditText(GetDlgItem(hwnd, IDC_VCE_IME_SUBM)).ToInt();
    iInt = checkRange(iInt, 0, 4, 0);
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("IMEDisSubmNo"), iInt);

    iInt = GetEditText(GetDlgItem(hwnd, IDC_VCE_DISABLE_SUBM)).ToInt();
    iInt = checkRange(iInt, 0, 255, 0);
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("RDODisSub"), iInt);

    iInt = SendMessage(GetDlgItem(hwnd, IDC_VCE_LSM), CB_GETCURSEL, 0, 0);
    AppConfig->SetInt(TEXT("VCE Settings"), TEXT("LSMVert"), iInt);

}

void SettingsVCE::CancelSettings()
{
}

bool SettingsVCE::HasDefaults() const
{
    return false;
}

INT_PTR SettingsVCE::ProcMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
        {
            LocalizeWindow(hwnd);

            //--------------------------------------------

            HWND hwndToolTip = CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                hwnd, NULL, hinstMain, NULL);

            TOOLINFO ti;
            zero(&ti, sizeof(ti));
            ti.cbSize = sizeof(ti);
            ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
            ti.hwnd = hwnd;

            if (LocaleIsRTL())
                ti.uFlags |= TTF_RTLREADING;

            SendMessage(hwndToolTip, TTM_SETMAXTIPWIDTH, 0, 500);
            SendMessage(hwndToolTip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 8000);

            //--------------------------------------------
            int iInt = 0;

            iInt = AppConfig->GetInt(TEXT("VCE Settings"), TEXT("UseCustom"), 0);
            SendMessage(GetDlgItem(hwnd, IDC_VCE_USE_CUSTOM), BM_SETCHECK, iInt, 0);

            iInt = AppConfig->GetInt(TEXT("VCE Settings"), TEXT("CABAC"), 0);
            SendMessage(GetDlgItem(hwnd, IDC_VCE_CABAC), BM_SETCHECK, iInt, 0);

            iInt = AppConfig->GetInt(TEXT("VCE Settings"), TEXT("AMD"), 0);
            SendMessage(GetDlgItem(hwnd, IDC_VCE_AMD), BM_SETCHECK, iInt, 0);

            ti.lpszText = (LPWSTR)Str("Settings.Encoding.VCE.AMDTooltip");
            ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_VCE_AMD);
            SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

            LoadSettingEditInt(GetDlgItem(hwnd, IDC_VCE_GOP), TEXT("VCE Settings"), TEXT("GOPSize"), 30);
            LoadSettingEditInt(GetDlgItem(hwnd, IDC_VCE_IDR), TEXT("VCE Settings"), TEXT("IDRPeriod"), 0);
            LoadSettingEditInt(GetDlgItem(hwnd, IDC_VCE_IPIC), TEXT("VCE Settings"), TEXT("IPicPeriod"), 0);

            ti.lpszText = (LPWSTR)Str("Settings.Encoding.VCE.PeriodTooltip");
            ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_VCE_IDR);
            SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
            ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_VCE_IPIC);
            SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

            SendMessage(GetDlgItem(hwnd, IDC_VCE_LSM), CB_ADDSTRING, 0, (LPARAM)TEXT("5x3 macroblocks"));
            SendMessage(GetDlgItem(hwnd, IDC_VCE_LSM), CB_ADDSTRING, 0, (LPARAM)TEXT("9x5 macroblocks"));
            SendMessage(GetDlgItem(hwnd, IDC_VCE_LSM), CB_ADDSTRING, 0, (LPARAM)TEXT("13x7 macroblocks"));
            LoadSettingComboInt(GetDlgItem(hwnd, IDC_VCE_LSM), TEXT("VCE Settings"), TEXT("LSMVert"), 0, 2);

            LoadSettingEditInt(GetDlgItem(hwnd, IDC_VCE_SRNG0), TEXT("VCE Settings"), TEXT("SearchX"), 16);
            LoadSettingEditInt(GetDlgItem(hwnd, IDC_VCE_SRNG1), TEXT("VCE Settings"), TEXT("SearchY"), 16);

            ti.lpszText = (LPWSTR)Str("Settings.Encoding.VCE.SearchXYTooltip");
            ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_VCE_SRNG0);
            SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
            ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_VCE_SRNG1);
            SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

            LoadSettingEditInt(GetDlgItem(hwnd, IDC_VCE_SRNG_IMEX), TEXT("VCE Settings"), TEXT("IME2SearchX"), 4);
            LoadSettingEditInt(GetDlgItem(hwnd, IDC_VCE_SRNG_IMEY), TEXT("VCE Settings"), TEXT("IME2SearchY"), 4);

            ti.lpszText = (LPWSTR)Str("Settings.Encoding.VCE.IME2SearchXYTooltip");
            ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_VCE_SRNG_IMEX);
            SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
            ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_VCE_SRNG_IMEY);
            SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

            LoadSettingEditInt(GetDlgItem(hwnd, IDC_VCE_IPRED), TEXT("VCE Settings"), TEXT("IPred"), 0);
            LoadSettingEditInt(GetDlgItem(hwnd, IDC_VCE_PPRED), TEXT("VCE Settings"), TEXT("PPred"), 0);

            ti.lpszText = (LPWSTR)Str("Settings.Encoding.VCE.PredTooltip");
            ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_VCE_IPRED);
            SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
            ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_VCE_PPRED);
            SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

            iInt = AppConfig->GetInt(TEXT("VCE Settings"), TEXT("MEHalf"), 1);
            SendMessage(GetDlgItem(hwnd, IDC_VCE_ME_HALF), BM_SETCHECK, iInt, 0);

            iInt = AppConfig->GetInt(TEXT("VCE Settings"), TEXT("MEQuarter"), 1);
            SendMessage(GetDlgItem(hwnd, IDC_VCE_ME_QUARTER), BM_SETCHECK, iInt, 0);

            iInt = AppConfig->GetInt(TEXT("VCE Settings"), TEXT("Force16x16Skip"), 0);
            SendMessage(GetDlgItem(hwnd, IDC_VCE_FORCE16SKIP), BM_SETCHECK, iInt, 0);

            iInt = AppConfig->GetInt(TEXT("VCE Settings"), TEXT("DisFavorPMV"), 0);
            SendMessage(GetDlgItem(hwnd, IDC_VCE_PMV), BM_SETCHECK, iInt, 0);

            iInt = AppConfig->GetInt(TEXT("VCE Settings"), TEXT("ForceZPC"), 0);
            SendMessage(GetDlgItem(hwnd, IDC_VCE_ZPC), BM_SETCHECK, iInt, 0);

            iInt = AppConfig->GetInt(TEXT("VCE Settings"), TEXT("IMEDecimation"), 1);
            SendMessage(GetDlgItem(hwnd, IDC_VCE_IME_DEC), BM_SETCHECK, iInt, 0);

            iInt = AppConfig->GetInt(TEXT("VCE Settings"), TEXT("ConstIntraPred"), 0);
            SendMessage(GetDlgItem(hwnd, IDC_VCE_CONSTINTRAPRED), BM_SETCHECK, iInt, 0);

            iInt = AppConfig->GetInt(TEXT("VCE Settings"), TEXT("DisSATD"), 0);
            SendMessage(GetDlgItem(hwnd, IDC_VCE_SATD), BM_SETCHECK, iInt, 0);

            iInt = AppConfig->GetInt(TEXT("VCE Settings"), TEXT("IMEOverwite"), iInt);
            SendMessage(GetDlgItem(hwnd, IDC_VCE_IME_OVERW), BM_SETCHECK, iInt, 0);

            LoadSettingEditInt(GetDlgItem(hwnd, IDC_VCE_IME_SUBM), TEXT("VCE Settings"), TEXT("IMEDisSubmNo"), 0);
            ti.lpszText = (LPWSTR)Str("Settings.Encoding.VCE.IMEOverwTooltip");
            ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_VCE_IME_SUBM);
            SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

            LoadSettingEditInt(GetDlgItem(hwnd, IDC_VCE_DISABLE_SUBM), TEXT("VCE Settings"), TEXT("RDODisSub"), 120);
            ti.lpszText = (LPWSTR)Str("Settings.Encoding.VCE.DisableSubModeTooltip");
            ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_VCE_DISABLE_SUBM);
            SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

            ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_HIDE);
            SetChangedSettings(false);

            return TRUE;
        }

        case WM_COMMAND:
        {
            bool bDataChanged = false;

            switch (LOWORD(wParam))
            {
            case IDC_VCE_USE_CUSTOM:
            case IDC_VCE_CABAC:
            case IDC_VCE_AMD:
            case IDC_VCE_ME_HALF:
            case IDC_VCE_ME_QUARTER:
            case IDC_VCE_IME_OVERW:
            case IDC_VCE_IME_DEC:
            case IDC_VCE_FORCE16SKIP:
            case IDC_VCE_PMV:
            case IDC_VCE_ZPC:
            case IDC_VCE_CONSTINTRAPRED:
            case IDC_VCE_SATD:
                if (HIWORD(wParam) == BN_CLICKED)
                {
                    bDataChanged = true;
                }
                break;

            case IDC_VCE_GOP:
            case IDC_VCE_IDR:
            case IDC_VCE_IPIC:
            case IDC_VCE_IPRED:
            case IDC_VCE_PPRED:
            case IDC_VCE_SRNG0:
            case IDC_VCE_SRNG1:
            case IDC_VCE_SRNG_IMEX:
            case IDC_VCE_SRNG_IMEY:
            case IDC_VCE_IME_SUBM:
            case IDC_VCE_DISABLE_SUBM:
            case IDC_VCE_LSM:
                if (HIWORD(wParam) == EN_CHANGE || 
                    HIWORD(wParam) == CBN_SELCHANGE)
                {
                    bDataChanged = true;
                }
                break;
            }

            if (bDataChanged)
            {
                ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                SetChangedSettings(true);
            }
            break;
        }
    }
    return FALSE;
}
