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

//============================================================================
// SettingsEncoding class

SettingsEncoding::SettingsEncoding()
    : SettingsPane()
{
}

SettingsEncoding::~SettingsEncoding()
{
}

CTSTR SettingsEncoding::GetCategory() const
{
    static CTSTR name = Str("Settings.Encoding");
    return name;
}

HWND SettingsEncoding::CreatePane(HWND parentHwnd)
{
    hwnd = CreateDialogParam(hinstMain, MAKEINTRESOURCE(IDD_SETTINGS_ENCODING), parentHwnd, (DLGPROC)DialogProc, (LPARAM)this);
    return hwnd;
}

void SettingsEncoding::DestroyPane()
{
    DestroyWindow(hwnd);
    hwnd = NULL;
}

void SettingsEncoding::ApplySettings()
{
    int quality = (int)SendMessage(GetDlgItem(hwnd, IDC_QUALITY), CB_GETCURSEL, 0, 0);
    if(quality != CB_ERR)
        AppConfig->SetInt(TEXT("Video Encoding"), TEXT("Quality"), quality);

    UINT bitrate = GetEditText(GetDlgItem(hwnd, IDC_MAXBITRATE)).ToInt();
    if(bitrate < 100) bitrate = 100;
    AppConfig->SetInt(TEXT("Video Encoding"), TEXT("MaxBitrate"), bitrate);

    UINT bufSize = GetEditText(GetDlgItem(hwnd, IDC_BUFFERSIZE)).ToInt();
    //if(bufSize < 100) bufSize = bitrate;  //R1CH: Allow users to enter 0 buffer size to disable VBV, its protected by checkbox anyway
    AppConfig->SetInt(TEXT("Video Encoding"), TEXT("BufferSize"), bufSize);

    if(App->GetVideoEncoder() != NULL) {
        if(App->GetVideoEncoder()->DynamicBitrateSupported())
        {
            int oldBitrate = App->GetVideoEncoder()->GetBitRate();
            App->GetVideoEncoder()->SetBitRate(bitrate, bufSize);
            if(oldBitrate != bitrate)
                Log(FormattedString(TEXT("Settings::Encoding: Changing bitrate from %dkb/s to %dkb/s"), oldBitrate, bitrate));
        }
    }

    String strTemp = GetCBText(GetDlgItem(hwnd, IDC_AUDIOCODEC));
    AppConfig->SetString(TEXT("Audio Encoding"), TEXT("Codec"), strTemp);

    strTemp = GetCBText(GetDlgItem(hwnd, IDC_AUDIOBITRATE));
    AppConfig->SetString(TEXT("Audio Encoding"), TEXT("Bitrate"), strTemp);

    int curSel = (int)SendMessage(GetDlgItem(hwnd, IDC_AUDIOFORMAT), CB_GETCURSEL, 0, 0);
    if(curSel != CB_ERR)
        AppConfig->SetInt(TEXT("Audio Encoding"), TEXT("Format"), curSel);

    bool bUseCBR = SendMessage(GetDlgItem(hwnd, IDC_USECBR), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt(TEXT("Video Encoding"), TEXT("UseCBR"), bUseCBR);

    bool bPadCBR = SendMessage(GetDlgItem(hwnd, IDC_PADCBR), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt(TEXT("Video Encoding"), TEXT("PadCBR"), bPadCBR);

    bool bCustomBuffer = SendMessage(GetDlgItem(hwnd, IDC_CUSTOMBUFFER), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt(TEXT("Video Encoding"), TEXT("UseBufferSize"), bCustomBuffer);
}

void SettingsEncoding::CancelSettings()
{
}

bool SettingsEncoding::HasDefaults() const
{
    return false;
}

INT_PTR SettingsEncoding::ProcMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                HWND hwndTemp;
                LocalizeWindow(hwnd);

                //--------------------------------------------

                HWND hwndToolTip = CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL, WS_POPUP|TTS_NOPREFIX|TTS_ALWAYSTIP,
                                                  CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                                  hwnd, NULL, hinstMain, NULL);

                TOOLINFO ti;
                zero(&ti, sizeof(ti));
                ti.cbSize = sizeof(ti);
                ti.uFlags = TTF_SUBCLASS|TTF_IDISHWND;
                ti.hwnd = hwnd;

                SendMessage(hwndToolTip, TTM_SETMAXTIPWIDTH, 0, 500);
                SendMessage(hwndToolTip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 8000);

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_QUALITY);
                for(int i=0; i<=10; i++)
                    SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)IntString(i).Array());

                LoadSettingComboString(hwndTemp, TEXT("Video Encoding"), TEXT("Quality"), TEXT("8"));

                ti.lpszText = (LPWSTR)Str("Settings.Encoding.Video.QualityTooltip");
                ti.uId = (UINT_PTR)hwndTemp;
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                //--------------------------------------------

                bool bUseCBR = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseCBR"), 1) != 0;
                bool bPadCBR = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("PadCBR"), 1) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_USECBR), BM_SETCHECK, bUseCBR ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hwnd, IDC_PADCBR), BM_SETCHECK, bPadCBR ? BST_CHECKED : BST_UNCHECKED, 0);
                EnableWindow(GetDlgItem(hwnd, IDC_QUALITY), !bUseCBR);
                EnableWindow(GetDlgItem(hwnd, IDC_PADCBR), bUseCBR);

                ti.lpszText = (LPWSTR)Str("Settings.Advanced.PadCBRToolTip");
                ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_PADCBR);
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                //--------------------------------------------

                int bitrate    = LoadSettingEditInt(GetDlgItem(hwnd, IDC_MAXBITRATE), TEXT("Video Encoding"), TEXT("MaxBitrate"), 1000);
                int buffersize = LoadSettingEditInt(GetDlgItem(hwnd, IDC_BUFFERSIZE), TEXT("Video Encoding"), TEXT("BufferSize"), 1000);

                ti.lpszText = (LPWSTR)Str("Settings.Encoding.Video.MaxBitRateTooltip");
                ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_MAXBITRATE);
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                ti.lpszText = (LPWSTR)Str("Settings.Encoding.Video.BufferSizeTooltip");
                ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_BUFFERSIZE);
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                //--------------------------------------------

                bool bHasUseBufferSizeValue = AppConfig->HasKey(TEXT("Video Encoding"), TEXT("UseBufferSize")) != 0;

                bool bUseBufferSize;
                if(!bHasUseBufferSizeValue)
                    bUseBufferSize = buffersize != bitrate;
                else
                    bUseBufferSize = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseBufferSize"), 0) != 0;

                SendMessage(GetDlgItem(hwnd, IDC_CUSTOMBUFFER), BM_SETCHECK, bUseBufferSize ? BST_CHECKED : BST_UNCHECKED, 0);
                EnableWindow(GetDlgItem(hwnd, IDC_BUFFERSIZE), bUseBufferSize);

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_AUDIOCODEC);

                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("MP3"));
#ifdef USE_AAC
                if(1)//OSGetVersion() >= 7)
                {
                    SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("AAC"));
                    LoadSettingComboString(hwndTemp, TEXT("Audio Encoding"), TEXT("Codec"), TEXT("AAC"));
                }
                else
                    LoadSettingComboString(hwndTemp, TEXT("Audio Encoding"), TEXT("Codec"), TEXT("MP3"));
#else
                LoadSettingComboString(hwndTemp, TEXT("Audio Encoding"), TEXT("Codec"), TEXT("MP3"));
#endif

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_AUDIOBITRATE);
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("48"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("64"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("80"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("96"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("112"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("128"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("160"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("192"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("256"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("320"));

                LoadSettingComboString(hwndTemp, TEXT("Audio Encoding"), TEXT("Bitrate"), TEXT("96"));

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_AUDIOFORMAT);
                //SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("48khz mono"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("44.1khz stereo"));

                BOOL isAAC = SendMessage(GetDlgItem(hwnd, IDC_AUDIOCODEC), CB_GETCURSEL, 0, 0) == 1;
                if (isAAC)
                    SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("48khz stereo"));

                LoadSettingComboInt(hwndTemp, TEXT("Audio Encoding"), TEXT("Format"), 1, isAAC ? 1 : 0);

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
                    case IDC_QUALITY:
                    case IDC_AUDIOBITRATE:
                    case IDC_AUDIOFORMAT:
                        if(HIWORD(wParam) == CBN_SELCHANGE)
                            bDataChanged = true;
                        break;

                    case IDC_AUDIOCODEC:
                        if(HIWORD(wParam) == CBN_SELCHANGE)
                        {
                            HWND hwndTemp = GetDlgItem(hwnd, IDC_AUDIOFORMAT);
                            SendMessage(hwndTemp, CB_RESETCONTENT, 0, 0);
                            SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("44.1khz stereo"));

                            BOOL isAAC = SendMessage(GetDlgItem(hwnd, IDC_AUDIOCODEC), CB_GETCURSEL, 0, 0) == 1;
                            if (isAAC)
                                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("48khz stereo"));

                            SendMessage(hwndTemp, CB_SETCURSEL, isAAC ? 1 : 0, 0);

                            bDataChanged = true;
                        }
                        break;

                    case IDC_MAXBITRATE:
                        if(HIWORD(wParam) == EN_CHANGE)
                        {
                            bool bCustomBuffer = SendMessage(GetDlgItem(hwnd, IDC_CUSTOMBUFFER), BM_GETCHECK, 0, 0) == BST_CHECKED;
                            if (!bCustomBuffer)
                            {
                                String strText = GetEditText((HWND)lParam);
                                SetWindowText(GetDlgItem(hwnd, IDC_BUFFERSIZE), strText);
                            }

                            bDataChanged = true;
                        }
                        break;

                    case IDC_BUFFERSIZE:
                        if(HIWORD(wParam) == EN_CHANGE)
                            bDataChanged = true;
                        break;

                    case IDC_CUSTOMBUFFER:
                    case IDC_USECBR:
                    case IDC_PADCBR:
                        if (HIWORD(wParam) == BN_CLICKED)
                        {
                            bool bChecked = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;
                            if(LOWORD(wParam) == IDC_CUSTOMBUFFER)
                            {
                                EnableWindow(GetDlgItem(hwnd, IDC_BUFFERSIZE), bChecked);
                                if(!bChecked)
                                    SetWindowText(GetDlgItem(hwnd, IDC_BUFFERSIZE), GetEditText(GetDlgItem(hwnd, IDC_MAXBITRATE)));
                            }
                            else if(LOWORD(wParam) == IDC_USECBR)
                            {
                                EnableWindow(GetDlgItem(hwnd, IDC_QUALITY), !bChecked);
                                EnableWindow(GetDlgItem(hwnd, IDC_PADCBR), bChecked);
                            }

                            bDataChanged = true;
                        }
                        break;
                }

                if(bDataChanged)
                {
                    ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                    SetChangedSettings(true);
                }
                break;
            }
    }
    return FALSE;
}
