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

enum SettingsSelection
{
    Settings_General,
    Settings_Encoding,
    Settings_Publish,
    Settings_Video,
    Settings_Audio,
    Settings_Advanced,
};

BOOL CALLBACK MonitorInfoEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, List<MonitorInfo> &monitors);

BOOL IsValidFileName(CTSTR lpFileName)
{
    if(!lpFileName || !*lpFileName)
        return FALSE;

    CTSTR lpTemp = lpFileName;

    do 
    {
        if( *lpTemp == '\\' ||
            *lpTemp == '/'  ||
            *lpTemp == ':'  ||
            *lpTemp == '*'  ||
            *lpTemp == '?'  ||
            *lpTemp == '"'  ||
            *lpTemp == '<'  ||
            *lpTemp == '>')
        {
            return FALSE;
        }
    } while (*++lpTemp);

    return TRUE;
}

INT_PTR CALLBACK OBS::GeneralSettingsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                LocalizeWindow(hwnd);

                //----------------------------------------------

                HWND hwndTemp = GetDlgItem(hwnd, IDC_LANGUAGE);

                OSFindData ofd;
                HANDLE hFind;
                if(hFind = OSFindFirstFile(TEXT("locale/*.txt"), ofd))
                {
                    do 
                    {
                        if(ofd.bDirectory) continue;

                        String langCode = GetPathFileName(ofd.fileName);

                        LocaleNativeName *langInfo = GetLocaleNativeName(langCode);
                        if(langInfo)
                        {
                            UINT id = (UINT)SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)langInfo->lpNative);
                            SendMessage(hwndTemp, CB_SETITEMDATA, id, (LPARAM)langInfo->lpCode);

                            if(App->strLanguage.CompareI(langCode))
                                SendMessage(hwndTemp, CB_SETCURSEL, id, 0);
                        }
                    } while(OSFindNextFile(hFind, ofd));

                    OSFindClose(hFind);
                }

                //----------------------------------------------

                String strCurProfile = GlobalConfig->GetString(TEXT("General"), TEXT("Profile"));

                hwndTemp = GetDlgItem(hwnd, IDC_PROFILE);

                String strProfilesWildcard = lpAppDataPath;
                strProfilesWildcard << TEXT("\\profiles\\*.ini");

                if(hFind = OSFindFirstFile(strProfilesWildcard, ofd))
                {
                    do 
                    {
                        if(ofd.bDirectory) continue;

                        String strProfile = GetPathWithoutExtension(ofd.fileName);
                        UINT id = (UINT)SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)strProfile.Array());
                        if(strProfile.CompareI(strCurProfile))
                            SendMessage(hwndTemp, CB_SETCURSEL, id, 0);
                    } while(OSFindNextFile(hFind, ofd));

                    OSFindClose(hFind);
                }

                EnableWindow(GetDlgItem(hwnd, IDC_ADD),     FALSE);
                EnableWindow(GetDlgItem(hwnd, IDC_RENAME),  FALSE);

                UINT numItems = (UINT)SendMessage(GetDlgItem(hwnd, IDC_PROFILE), CB_GETCOUNT, 0, 0);
                EnableWindow(GetDlgItem(hwnd, IDC_REMOVE),  (numItems > 1));
                //----------------------------------------------

                App->SetChangedSettings(false);
                return TRUE;
            }

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_LANGUAGE:
                    {
                        if(HIWORD(wParam) != CBN_SELCHANGE)
                            break;

                        HWND hwndTemp = (HWND)lParam;

                        SetWindowText(GetDlgItem(hwnd, IDC_INFO), Str("Settings.General.Restart"));
                        ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);

                        int curSel = (int)SendMessage(hwndTemp, CB_GETCURSEL, 0, 0);
                        if(curSel != CB_ERR)
                        {
                            String strLanguageCode = (CTSTR)SendMessage(hwndTemp, CB_GETITEMDATA, curSel, 0);
                            GlobalConfig->SetString(TEXT("General"), TEXT("Language"), strLanguageCode);
                        }
                        break;
                    }

                case IDC_PROFILE:
                    if(HIWORD(wParam) == CBN_EDITCHANGE)
                    {
                        String strText = GetEditText((HWND)lParam).KillSpaces();

                        EnableWindow(GetDlgItem(hwnd, IDC_REMOVE),  FALSE);

                        if(strText.IsValid())
                        {
                            if(IsValidFileName(strText))
                            {
                                String strCurProfile = GlobalConfig->GetString(TEXT("General"), TEXT("Profile"));

                                UINT id = (UINT)SendMessage((HWND)lParam, CB_FINDSTRINGEXACT, -1, (LPARAM)strText.Array());
                                EnableWindow(GetDlgItem(hwnd, IDC_ADD),     (id == CB_ERR));
                                EnableWindow(GetDlgItem(hwnd, IDC_RENAME),  (id == CB_ERR) || strCurProfile.CompareI(strText));

                                ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_HIDE);
                                break;
                            }

                            SetWindowText(GetDlgItem(hwnd, IDC_INFO), Str("Settings.General.InvalidName"));
                            ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                        }
                        else
                            ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_HIDE);

                        EnableWindow(GetDlgItem(hwnd, IDC_ADD),     FALSE);
                        EnableWindow(GetDlgItem(hwnd, IDC_RENAME),  FALSE);
                    }
                    else if(HIWORD(wParam) == CBN_SELCHANGE)
                    {
                        EnableWindow(GetDlgItem(hwnd, IDC_ADD),     FALSE);
                        EnableWindow(GetDlgItem(hwnd, IDC_RENAME),  FALSE);

                        String strProfile = GetCBText((HWND)lParam);
                        String strProfilePath;
                        strProfilePath << lpAppDataPath << TEXT("\\profiles\\") << strProfile << TEXT(".ini");

                        if(!AppConfig->Open(strProfilePath))
                        {
                            MessageBox(hwnd, TEXT("Error - unable to open ini file"), NULL, 0);
                            break;
                        }

                        SetWindowText(GetDlgItem(hwnd, IDC_INFO), Str("Settings.Info"));
                        ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);

                        GlobalConfig->SetString(TEXT("General"), TEXT("Profile"), strProfile);

                        UINT numItems = (UINT)SendMessage(GetDlgItem(hwnd, IDC_PROFILE), CB_GETCOUNT, 0, 0);
                        EnableWindow(GetDlgItem(hwnd, IDC_REMOVE),  (numItems > 1));

                        if(!App->bRunning)
                            App->ResizeWindow(false);
                    }
                    break;

                case IDC_RENAME:
                case IDC_ADD:
                    if(HIWORD(wParam) == BN_CLICKED)
                    {
                        HWND hwndProfileList = GetDlgItem(hwnd, IDC_PROFILE);
                        String strProfile = GetEditText(hwndProfileList).KillSpaces();
                        SetWindowText(hwndProfileList, strProfile);

                        if(strProfile.IsEmpty())
                            break;

                        bool bRenaming = (LOWORD(wParam) == IDC_RENAME);

                        String strCurProfile = GlobalConfig->GetString(TEXT("General"), TEXT("Profile"));
                        String strCurProfilePath;
                        strCurProfilePath << lpAppDataPath << TEXT("\\profiles\\") << strCurProfile << TEXT(".ini");

                        String strProfilePath;
                        strProfilePath << lpAppDataPath << TEXT("\\profiles\\") << strProfile << TEXT(".ini");

                        if((!bRenaming || !strProfilePath.CompareI(strCurProfilePath)) && OSFileExists(strProfilePath))
                            MessageBox(hwnd, Str("Settings.General.ProfileExists"), NULL, 0);
                        else
                        {
                            if(bRenaming)
                            {
                                if(!MoveFile(strCurProfilePath, strProfilePath))
                                    break;

                                AppConfig->SetFilePath(strProfilePath);

                                UINT curID = (UINT)SendMessage(hwndProfileList, CB_FINDSTRINGEXACT, -1, (LPARAM)strCurProfile.Array());
                                if(curID != CB_ERR)
                                    SendMessage(hwndProfileList, CB_DELETESTRING, curID, 0);
                            }
                            else
                            {
                                if(!AppConfig->SaveAs(strProfilePath))
                                {
                                    MessageBox(hwnd, TEXT("Error - unable to create new profile, could not create file"), NULL, 0);
                                    break;
                                }
                            }

                            UINT id = (UINT)SendMessage(hwndProfileList, CB_ADDSTRING, 0, (LPARAM)strProfile.Array());
                            SendMessage(hwndProfileList, CB_SETCURSEL, id, 0);
                            GlobalConfig->SetString(TEXT("General"), TEXT("Profile"), strProfile);

                            UINT numItems = (UINT)SendMessage(hwndProfileList, CB_GETCOUNT, 0, 0);
                            EnableWindow(GetDlgItem(hwnd, IDC_REMOVE),  (numItems > 1));
                            EnableWindow(GetDlgItem(hwnd, IDC_RENAME),  FALSE);
                            EnableWindow(GetDlgItem(hwnd, IDC_ADD),     FALSE);
                        }
                    }
                    break;

                case IDC_REMOVE:
                    {
                        HWND hwndProfileList = GetDlgItem(hwnd, IDC_PROFILE);

                        String strCurProfile = GlobalConfig->GetString(TEXT("General"), TEXT("Profile"));

                        UINT numItems = (UINT)SendMessage(hwndProfileList, CB_GETCOUNT, 0, 0);

                        String strConfirm = Str("Settings.General.ConfirmDelete");
                        strConfirm.FindReplace(TEXT("$1"), strCurProfile);
                        if(MessageBox(hwnd, strConfirm, Str("DeleteConfirm.Title"), MB_YESNO) == IDYES)
                        {
                            UINT id = (UINT)SendMessage(hwndProfileList, CB_FINDSTRINGEXACT, -1, (LPARAM)strCurProfile.Array());
                            if(id != CB_ERR)
                            {
                                SendMessage(hwndProfileList, CB_DELETESTRING, id, 0);
                                if(id == numItems-1)
                                    id--;

                                SendMessage(hwndProfileList, CB_SETCURSEL, id, 0);
                                GeneralSettingsProc(hwnd, WM_COMMAND, MAKEWPARAM(IDC_PROFILE, CBN_SELCHANGE), (LPARAM)hwndProfileList);

                                String strCurProfilePath;
                                strCurProfilePath << lpAppDataPath << TEXT("\\profiles\\") << strCurProfile << TEXT(".ini");
                                OSDeleteFile(strCurProfilePath);
                            }
                        }

                        break;
                    }
            }

    }
    return FALSE;
}

int LoadSettingEditInt(HWND hwnd, CTSTR lpConfigSection, CTSTR lpConfigName, int defVal)
{
    int iLoadVal;
    if(!AppConfig->HasKey(lpConfigSection, lpConfigName))
    {
        AppConfig->SetInt(lpConfigSection, lpConfigName, defVal);
        iLoadVal = defVal;
    }
    else
        iLoadVal = AppConfig->GetInt(lpConfigSection, lpConfigName, defVal);

    SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)IntString(iLoadVal).Array());

    return iLoadVal;
}

String LoadSettingEditString(HWND hwnd, CTSTR lpConfigSection, CTSTR lpConfigName, CTSTR lpDefault)
{
    String strLoadVal;
    if(!AppConfig->HasKey(lpConfigSection, lpConfigName))
    {
        if(lpDefault)
            AppConfig->SetString(lpConfigSection, lpConfigName, lpDefault);

        strLoadVal = lpDefault;
    }
    else
        strLoadVal = AppConfig->GetString(lpConfigSection, lpConfigName, lpDefault);

    SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)strLoadVal.Array());

    return strLoadVal;
}

int LoadSettingComboInt(HWND hwnd, CTSTR lpConfigSection, CTSTR lpConfigName, int defVal, int maxVal)
{
    int curVal = AppConfig->GetInt(lpConfigSection, lpConfigName, defVal);
    int id = curVal;

    if(!AppConfig->HasKey(lpConfigSection, lpConfigName) || curVal < 0 || (maxVal != 0 && curVal > maxVal))
    {
        AppConfig->SetInt(lpConfigSection, lpConfigName, defVal);
        curVal = defVal;
    }

    SendMessage(hwnd, CB_SETCURSEL, (WPARAM)id, 0);

    return curVal;
}

String LoadSettingComboString(HWND hwnd, CTSTR lpConfigSection, CTSTR lpConfigName, CTSTR lpDefault)
{
    String strVal = AppConfig->GetString(lpConfigSection, lpConfigName, lpDefault);
    int id = (int)SendMessage(hwnd, CB_FINDSTRINGEXACT, 0, (LPARAM)strVal.Array());
    if(!AppConfig->HasKey(lpConfigSection, lpConfigName) || id == CB_ERR)
    {
        if(lpDefault)
        {
            AppConfig->SetString(lpConfigSection, lpConfigName, lpDefault);

            if(id == CB_ERR)
            {
                id = (int)SendMessage(hwnd, CB_FINDSTRINGEXACT, -1, (LPARAM)lpDefault);
                strVal = lpDefault;
            }
        }
        else
            id = 0;
    }

    SendMessage(hwnd, CB_SETCURSEL, (WPARAM)id, 0);

    return strVal;
}

String LoadSettingTextComboString(HWND hwnd, CTSTR lpConfigSection, CTSTR lpConfigName, CTSTR lpDefault)
{
    String strVal = AppConfig->GetString(lpConfigSection, lpConfigName, lpDefault);
    int id = (int)SendMessage(hwnd, CB_FINDSTRINGEXACT, -1, (LPARAM)strVal.Array());
    if(!AppConfig->HasKey(lpConfigSection, lpConfigName))
    {
        if(lpDefault)
            AppConfig->SetString(lpConfigSection, lpConfigName, lpDefault);
    }

    SendMessage(hwnd, CB_SETCURSEL, (WPARAM)id, 0);

    return strVal;
}


CTSTR preset_names[10] = {TEXT("ultrafast"), TEXT("superfast"), TEXT("veryfast"), TEXT("faster"), TEXT("fast"), TEXT("medium"), TEXT("slow"), TEXT("slower"), TEXT("veryslow"), TEXT("placebo")};

INT_PTR CALLBACK OBS::EncoderSettingsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
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

                LoadSettingEditInt(GetDlgItem(hwnd, IDC_MAXBITRATE), TEXT("Video Encoding"), TEXT("MaxBitrate"), 2000);
                LoadSettingEditInt(GetDlgItem(hwnd, IDC_BUFFERSIZE), TEXT("Video Encoding"), TEXT("BufferSize"), 1000);

                ti.lpszText = (LPWSTR)Str("Settings.Encoding.Video.MaxBitRateTooltip");
                ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_MAXBITRATE);
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                ti.lpszText = (LPWSTR)Str("Settings.Encoding.Video.BufferSizeTooltip");
                ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_BUFFERSIZE);
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_AUDIOCODEC);
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("AAC"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("MP3"));

                LoadSettingComboString(hwndTemp, TEXT("Audio Encoding"), TEXT("Codec"), TEXT("AAC"));

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_AUDIOBITRATE);
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("48"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("64"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("96"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("128"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("192"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("256"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("320"));

                LoadSettingComboString(hwndTemp, TEXT("Audio Encoding"), TEXT("Bitrate"), TEXT("96"));

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_AUDIOFORMAT);
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("44.1khz mono"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("44.1khz stereo"));

                LoadSettingComboInt(hwndTemp, TEXT("Audio Encoding"), TEXT("Format"), 1, 1);

                //--------------------------------------------

                ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_HIDE);
                App->SetChangedSettings(false);
                return TRUE;
            }

        case WM_COMMAND:
            {
                bool bDataChanged = false;

                switch(LOWORD(wParam))
                {
                    case IDC_QUALITY:
                    case IDC_AUDIOCODEC:
                    case IDC_AUDIOBITRATE:
                    case IDC_AUDIOFORMAT:
                        if(HIWORD(wParam) == CBN_SELCHANGE)
                            bDataChanged = true;
                        break;

                    case IDC_MAXBITRATE:
                    case IDC_BUFFERSIZE:
                        if(HIWORD(wParam) == EN_CHANGE)
                            bDataChanged = true;
                        break;
                }

                if(bDataChanged)
                {
                    ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                    App->SetChangedSettings(true);
                }
                break;
            }
    }
    return FALSE;
}

enum
{
    PublishMode_LiveStream,
    PublishMode_Serve,
    PublishMode_BandwidthInfo
};

struct PublishDialogData
{
    UINT mode;
    LONG fileControlOffset;
};

void AdjustWindowPos(HWND hwnd, LONG xOffset, LONG yOffset)
{
    RECT rc;

    HWND hwndParent = GetParent(hwnd);
    GetWindowRect(hwnd, &rc);
    ScreenToClient(hwndParent, (LPPOINT)&rc);

    SetWindowPos(hwnd, NULL, rc.left+xOffset, rc.top+yOffset, 0, 0, SWP_NOSIZE|SWP_NOZORDER|SWP_NOOWNERZORDER);
}

INT_PTR CALLBACK OBS::PublishSettingsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND hwndTemp;

    switch(message)
    {
        case WM_INITDIALOG:
            {
                LocalizeWindow(hwnd);

                PublishDialogData *data = new PublishDialogData;
                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)data);

                RECT serviceRect, saveToFileRect;
                GetWindowRect(GetDlgItem(hwnd, IDC_SERVICE), &serviceRect);
                GetWindowRect(GetDlgItem(hwnd, IDC_SAVEPATH), &saveToFileRect);

                data->fileControlOffset = saveToFileRect.top-serviceRect.top;

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_MODE);
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)Str("Settings.Publish.Mode.LiveStream"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)Str("Settings.Publish.Mode.FileOnly"));

                int mode = LoadSettingComboInt(hwndTemp, TEXT("Publish"), TEXT("Mode"), 0, 2);
                data->mode = mode;

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_SERVICE);
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("Custom"));

                UINT numServices = 0;

                XConfig serverData;
                if(serverData.Open(TEXT("services.xconfig")))
                {
                    XElement *services = serverData.GetElement(TEXT("services"));
                    if(services)
                    {
                        numServices = services->NumElements();

                        for(UINT i=0; i<numServices; i++)
                        {
                            XElement *service = services->GetElementByID(i);
                            SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)service->GetName());
                        }
                    }
                }

                int serviceID = LoadSettingComboInt(hwndTemp, TEXT("Publish"), TEXT("Service"), 1, 0);
                if(mode != 0) ShowWindow(hwndTemp, SW_HIDE);

                //--------------------------------------------

                /*hwndTemp = GetDlgItem(hwnd, IDC_USERNAME);
                LoadSettingEditString(hwndTemp, TEXT("Publish"), TEXT("Username"), NULL);
                if(mode != 0) ShowWindow(hwndTemp, SW_HIDE);*/

                hwndTemp = GetDlgItem(hwnd, IDC_CHANNELNAME);
                LoadSettingEditString(hwndTemp, TEXT("Publish"), TEXT("Channel"), NULL);
                if(mode != 0) ShowWindow(hwndTemp, SW_HIDE);

                hwndTemp = GetDlgItem(hwnd, IDC_PLAYPATH);
                LoadSettingEditString(hwndTemp, TEXT("Publish"), TEXT("PlayPath"), NULL);
                if(mode != 0) ShowWindow(hwndTemp, SW_HIDE);

                if(serviceID == 0) //custom
                {
                    ShowWindow(GetDlgItem(hwnd, IDC_SERVERLIST), SW_HIDE);
                    hwndTemp = GetDlgItem(hwnd, IDC_SERVEREDIT);
                    LoadSettingEditString(hwndTemp, TEXT("Publish"), TEXT("Server"), NULL);
                }
                else
                {
                    ShowWindow(GetDlgItem(hwnd, IDC_SERVEREDIT), SW_HIDE);
                    hwndTemp = GetDlgItem(hwnd, IDC_SERVERLIST);

                    XElement *services = serverData.GetElement(TEXT("services"));
                    if(services)
                    {
                        XElement *service = services->GetElementByID(serviceID-1);
                        if(service)
                        {
                            XElement *servers = service->GetElement(TEXT("servers"));
                            if(servers)
                            {
                                UINT numServers = servers->NumDataItems();
                                for(UINT i=0; i<numServers; i++)
                                {
                                    XDataItem *server = servers->GetDataItemByID(i);
                                    SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)server->GetName());
                                }
                            }

                            CTSTR lpDefaultChannel = service->GetString(TEXT("useChannel"));
                            if(lpDefaultChannel)
                            {
                                SendMessage(GetDlgItem(hwnd, IDC_CHANNELNAME), WM_SETTEXT, 0, (LPARAM)lpDefaultChannel);
                                EnableWindow(GetDlgItem(hwnd, IDC_CHANNELNAME), FALSE);
                            }
                            else
                                EnableWindow(GetDlgItem(hwnd, IDC_CHANNELNAME), TRUE);
                        }
                    }

                    LoadSettingComboString(hwndTemp, TEXT("Publish"), TEXT("Server"), NULL);
                }

                if(mode != 0) ShowWindow(hwndTemp, SW_HIDE);

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_AUTORECONNECT);

                BOOL bAutoReconnect = AppConfig->GetInt(TEXT("Publish"), TEXT("AutoReconnect"), 1);
                SendMessage(hwndTemp, BM_SETCHECK, bAutoReconnect ? BST_CHECKED : BST_UNCHECKED, 0);

                if(mode != 0) ShowWindow(hwndTemp, SW_HIDE);

                hwndTemp = GetDlgItem(hwnd, IDC_AUTORECONNECT_TIMEOUT);
                EnableWindow(GetDlgItem(hwnd, IDC_AUTORECONNECT_TIMEOUT_EDIT), bAutoReconnect);
                EnableWindow(hwndTemp, bAutoReconnect);

                int retryTime = AppConfig->GetInt(TEXT("Publish"), TEXT("AutoReconnectTimeout"), 10);
                if(retryTime > 60)      retryTime = 60;
                else if(retryTime < 5)  retryTime = 5;

                SendMessage(hwndTemp, UDM_SETRANGE32, 5, 60);
                SendMessage(hwndTemp, UDM_SETPOS32, 0, retryTime);

                if(mode != 0) ShowWindow(hwndTemp, SW_HIDE);

                //--------------------------------------------

                if(mode != 0)
                {
                    ShowWindow(GetDlgItem(hwnd, IDC_SERVICE_STATIC), SW_HIDE);
                    //ShowWindow(GetDlgItem(hwnd, IDC_USERNAME_STATIC), SW_HIDE);
                    ShowWindow(GetDlgItem(hwnd, IDC_PLAYPATH_STATIC), SW_HIDE);
                    ShowWindow(GetDlgItem(hwnd, IDC_CHANNELNAME_STATIC), SW_HIDE);
                    ShowWindow(GetDlgItem(hwnd, IDC_SERVER_STATIC), SW_HIDE);
                    ShowWindow(GetDlgItem(hwnd, IDC_AUTORECONNECT_TIMEOUT_STATIC), SW_HIDE);
                    ShowWindow(GetDlgItem(hwnd, IDC_AUTORECONNECT_TIMEOUT_EDIT), SW_HIDE);
                    ShowWindow(GetDlgItem(hwnd, IDC_DASHBOARDLINK), SW_HIDE);
                    ShowWindow(GetDlgItem(hwnd, IDC_DASHBOARDLINK_STATIC), SW_HIDE);
                    ShowWindow(GetDlgItem(hwnd, IDC_SAVETOFILE), SW_HIDE);

                    AdjustWindowPos(GetDlgItem(hwnd, IDC_SAVEPATH_STATIC), 0, -data->fileControlOffset);
                    AdjustWindowPos(GetDlgItem(hwnd, IDC_SAVEPATH), 0, -data->fileControlOffset);
                    AdjustWindowPos(GetDlgItem(hwnd, IDC_BROWSE), 0, -data->fileControlOffset);
                    AdjustWindowPos(GetDlgItem(hwnd, IDC_STOPSTREAMHOTKEY_STATIC), 0, -data->fileControlOffset);
                    AdjustWindowPos(GetDlgItem(hwnd, IDC_STOPSTREAMHOTKEY), 0, -data->fileControlOffset);
                    AdjustWindowPos(GetDlgItem(hwnd, IDC_CLEARHOTKEY), 0, -data->fileControlOffset);
                    AdjustWindowPos(GetDlgItem(hwnd, IDC_STARTSTREAMHOTKEY_STATIC), 0, -data->fileControlOffset);
                    AdjustWindowPos(GetDlgItem(hwnd, IDC_STARTSTREAMHOTKEY), 0, -data->fileControlOffset);
                    AdjustWindowPos(GetDlgItem(hwnd, IDC_CLEARHOTKEY_STARTSTREAM), 0, -data->fileControlOffset);
                }

                //--------------------------------------------

                DWORD startHotkey = AppConfig->GetInt(TEXT("Publish"), TEXT("StartStreamHotkey"));
                SendMessage(GetDlgItem(hwnd, IDC_STARTSTREAMHOTKEY), HKM_SETHOTKEY, startHotkey, 0);

                //--------------------------------------------

                DWORD stopHotkey = AppConfig->GetInt(TEXT("Publish"), TEXT("StopStreamHotkey"));
                SendMessage(GetDlgItem(hwnd, IDC_STOPSTREAMHOTKEY), HKM_SETHOTKEY, stopHotkey, 0);

                //--------------------------------------------

                BOOL bSaveToFile = AppConfig->GetInt(TEXT("Publish"), TEXT("SaveToFile"));
                SendMessage(GetDlgItem(hwnd, IDC_SAVETOFILE), BM_SETCHECK, bSaveToFile ? BST_CHECKED : BST_UNCHECKED, 0);

                CTSTR lpSavePath = AppConfig->GetStringPtr(TEXT("Publish"), TEXT("SavePath"));
                SetWindowText(GetDlgItem(hwnd, IDC_SAVEPATH), lpSavePath);

                EnableWindow(GetDlgItem(hwnd, IDC_SAVEPATH), bSaveToFile || (mode != 0));
                EnableWindow(GetDlgItem(hwnd, IDC_BROWSE),   bSaveToFile || (mode != 0));

                //--------------------------------------------

                SetWindowText(GetDlgItem(hwnd, IDC_DASHBOARDLINK), App->strDashboard);

                //--------------------------------------------

                ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_HIDE);
                App->SetChangedSettings(false);

                return TRUE;
            }

        case WM_DESTROY:
            {
                PublishDialogData *data = (PublishDialogData*)GetWindowLongPtr(hwnd, DWLP_USER);
                delete data;
            }
            break;

        case WM_NOTIFY:
            {
                NMHDR *nmHdr = (NMHDR*)lParam;

                if(nmHdr->idFrom == IDC_AUTORECONNECT_TIMEOUT)
                {
                    if(nmHdr->code == UDN_DELTAPOS)
                        App->SetChangedSettings(true);
                }

                break;
            }

        case WM_COMMAND:
            {
                bool bDataChanged = false;

                switch(LOWORD(wParam))
                {
                    case IDC_MODE:
                        {
                            if(HIWORD(wParam) != CBN_SELCHANGE)
                                break;

                            PublishDialogData *data = (PublishDialogData*)GetWindowLongPtr(hwnd, DWLP_USER);

                            int mode = (int)SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
                            int swShowControls = (mode == 0) ? SW_SHOW : SW_HIDE;

                            ShowWindow(GetDlgItem(hwnd, IDC_SERVICE), swShowControls);
                            //ShowWindow(GetDlgItem(hwnd, IDC_USERNAME), swShowControls);
                            ShowWindow(GetDlgItem(hwnd, IDC_PLAYPATH), swShowControls);
                            ShowWindow(GetDlgItem(hwnd, IDC_CHANNELNAME), swShowControls);

                            int serviceID = (int)SendMessage(GetDlgItem(hwnd, IDC_SERVICE), CB_GETCURSEL, 0, 0);
                            if(serviceID == 0)
                            {
                                ShowWindow(GetDlgItem(hwnd, IDC_SERVERLIST), SW_HIDE);
                                ShowWindow(GetDlgItem(hwnd, IDC_SERVEREDIT), swShowControls);
                            }
                            else
                            {
                                ShowWindow(GetDlgItem(hwnd, IDC_SERVERLIST), swShowControls);
                                ShowWindow(GetDlgItem(hwnd, IDC_SERVEREDIT), SW_HIDE);
                            }

                            BOOL bSaveToFile = SendMessage(GetDlgItem(hwnd, IDC_SAVETOFILE), BM_GETCHECK, 0, 0) != BST_UNCHECKED;
                            EnableWindow(GetDlgItem(hwnd, IDC_SAVEPATH), bSaveToFile || (mode != 0));
                            EnableWindow(GetDlgItem(hwnd, IDC_BROWSE),   bSaveToFile || (mode != 0));

                            if(mode == 0 && data->mode == 1)
                            {
                                AdjustWindowPos(GetDlgItem(hwnd, IDC_SAVEPATH_STATIC), 0, data->fileControlOffset);
                                AdjustWindowPos(GetDlgItem(hwnd, IDC_SAVEPATH), 0, data->fileControlOffset);
                                AdjustWindowPos(GetDlgItem(hwnd, IDC_BROWSE), 0, data->fileControlOffset);
                                AdjustWindowPos(GetDlgItem(hwnd, IDC_STOPSTREAMHOTKEY_STATIC), 0, data->fileControlOffset);
                                AdjustWindowPos(GetDlgItem(hwnd, IDC_STOPSTREAMHOTKEY), 0, data->fileControlOffset);
                                AdjustWindowPos(GetDlgItem(hwnd, IDC_CLEARHOTKEY), 0, data->fileControlOffset);
                                AdjustWindowPos(GetDlgItem(hwnd, IDC_STARTSTREAMHOTKEY_STATIC), 0, data->fileControlOffset);
                                AdjustWindowPos(GetDlgItem(hwnd, IDC_STARTSTREAMHOTKEY), 0, data->fileControlOffset);
                                AdjustWindowPos(GetDlgItem(hwnd, IDC_CLEARHOTKEY_STARTSTREAM), 0, data->fileControlOffset);
                            }
                            else if(mode == 1 && data->mode == 0)
                            {
                                AdjustWindowPos(GetDlgItem(hwnd, IDC_SAVEPATH_STATIC), 0, -data->fileControlOffset);
                                AdjustWindowPos(GetDlgItem(hwnd, IDC_SAVEPATH), 0, -data->fileControlOffset);
                                AdjustWindowPos(GetDlgItem(hwnd, IDC_BROWSE), 0, -data->fileControlOffset);
                                AdjustWindowPos(GetDlgItem(hwnd, IDC_STOPSTREAMHOTKEY_STATIC), 0, -data->fileControlOffset);
                                AdjustWindowPos(GetDlgItem(hwnd, IDC_STOPSTREAMHOTKEY), 0, -data->fileControlOffset);
                                AdjustWindowPos(GetDlgItem(hwnd, IDC_CLEARHOTKEY), 0, -data->fileControlOffset);
                                AdjustWindowPos(GetDlgItem(hwnd, IDC_STARTSTREAMHOTKEY_STATIC), 0, -data->fileControlOffset);
                                AdjustWindowPos(GetDlgItem(hwnd, IDC_STARTSTREAMHOTKEY), 0, -data->fileControlOffset);
                                AdjustWindowPos(GetDlgItem(hwnd, IDC_CLEARHOTKEY_STARTSTREAM), 0, -data->fileControlOffset);
                            }

                            data->mode = mode;

                            ShowWindow(GetDlgItem(hwnd, IDC_SERVICE_STATIC), swShowControls);
                            //ShowWindow(GetDlgItem(hwnd, IDC_USERNAME_STATIC), swShowControls);
                            ShowWindow(GetDlgItem(hwnd, IDC_PLAYPATH_STATIC), swShowControls);
                            ShowWindow(GetDlgItem(hwnd, IDC_CHANNELNAME_STATIC), swShowControls);
                            ShowWindow(GetDlgItem(hwnd, IDC_SERVER_STATIC), swShowControls);
                            ShowWindow(GetDlgItem(hwnd, IDC_DASHBOARDLINK), swShowControls);
                            ShowWindow(GetDlgItem(hwnd, IDC_DASHBOARDLINK_STATIC), swShowControls);
                            ShowWindow(GetDlgItem(hwnd, IDC_AUTORECONNECT), swShowControls);
                            ShowWindow(GetDlgItem(hwnd, IDC_AUTORECONNECT_TIMEOUT), swShowControls);
                            ShowWindow(GetDlgItem(hwnd, IDC_AUTORECONNECT_TIMEOUT_STATIC), swShowControls);
                            ShowWindow(GetDlgItem(hwnd, IDC_AUTORECONNECT_TIMEOUT_EDIT), swShowControls);
                            ShowWindow(GetDlgItem(hwnd, IDC_SAVETOFILE), swShowControls);

                            bDataChanged = true;
                            break;
                        }

                    case IDC_SERVICE:
                        if(HIWORD(wParam) == CBN_SELCHANGE)
                        {
                            int serviceID = (int)SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);

                            if(serviceID == 0)
                            {
                                ShowWindow(GetDlgItem(hwnd, IDC_SERVERLIST), SW_HIDE);
                                ShowWindow(GetDlgItem(hwnd, IDC_SERVEREDIT), SW_SHOW);

                                SetWindowText(GetDlgItem(hwnd, IDC_CHANNELNAME), NULL);
                                SetWindowText(GetDlgItem(hwnd, IDC_SERVEREDIT), NULL);
                                SetWindowText(GetDlgItem(hwnd, IDC_DASHBOARDLINK), NULL);
                                EnableWindow(GetDlgItem(hwnd, IDC_CHANNELNAME), TRUE);
                            }
                            else
                            {
                                ShowWindow(GetDlgItem(hwnd, IDC_SERVEREDIT), SW_HIDE);

                                hwndTemp = GetDlgItem(hwnd, IDC_SERVERLIST);
                                ShowWindow(hwndTemp, SW_SHOW);
                                SendMessage(hwndTemp, CB_RESETCONTENT, 0, 0);

                                XConfig serverData;
                                if(serverData.Open(TEXT("services.xconfig")))
                                {
                                    XElement *services = serverData.GetElement(TEXT("services"));
                                    if(services)
                                    {
                                        XElement *service = services->GetElementByID(serviceID-1);
                                        if(service)
                                        {
                                            XElement *servers = service->GetElement(TEXT("servers"));
                                            if(servers)
                                            {
                                                UINT numServers = servers->NumDataItems();
                                                for(UINT i=0; i<numServers; i++)
                                                {
                                                    XDataItem *server = servers->GetDataItemByID(i);
                                                    SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)server->GetName());
                                                }
                                            }

                                            CTSTR lpDefaultChannel = service->GetString(TEXT("useChannel"));
                                            if(lpDefaultChannel)
                                            {
                                                SendMessage(GetDlgItem(hwnd, IDC_CHANNELNAME), WM_SETTEXT, 0, (LPARAM)lpDefaultChannel);
                                                EnableWindow(GetDlgItem(hwnd, IDC_CHANNELNAME), FALSE);
                                            }
                                            else
                                                EnableWindow(GetDlgItem(hwnd, IDC_CHANNELNAME), TRUE);
                                        }
                                    }
                                }

                                SendMessage(hwndTemp, CB_SETCURSEL, 0, 0);
                            }

                            SetWindowText(GetDlgItem(hwnd, IDC_PLAYPATH), NULL);
                            //SetWindowText(GetDlgItem(hwnd, IDC_USERNAME), NULL);

                            bDataChanged = true;
                        }
                        break;

                    case IDC_AUTORECONNECT:
                        if(HIWORD(wParam) == BN_CLICKED)
                        {
                            BOOL bAutoReconnect = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;

                            EnableWindow(GetDlgItem(hwnd, IDC_AUTORECONNECT_TIMEOUT),       bAutoReconnect);
                            EnableWindow(GetDlgItem(hwnd, IDC_AUTORECONNECT_TIMEOUT_EDIT),  bAutoReconnect);

                            App->SetChangedSettings(true);
                        }
                        break;

                    case IDC_AUTORECONNECT_TIMEOUT_EDIT:
                        if(HIWORD(wParam) == EN_CHANGE)
                            App->SetChangedSettings(true);
                        break;

                    case IDC_SAVETOFILE:
                        if(HIWORD(wParam) == BN_CLICKED)
                        {
                            BOOL bSaveToFile = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;

                            EnableWindow(GetDlgItem(hwnd, IDC_SAVEPATH), bSaveToFile);
                            EnableWindow(GetDlgItem(hwnd, IDC_BROWSE),   bSaveToFile);

                            bDataChanged = true;
                        }
                        break;

                    case IDC_BROWSE:
                        {
                            TCHAR lpFile[512];
                            OPENFILENAME ofn;
                            zero(&ofn, sizeof(ofn));
                            ofn.lStructSize = sizeof(ofn);
                            ofn.hwndOwner = hwnd;
                            ofn.lpstrFile = lpFile;
                            ofn.nMaxFile = 511;
                            ofn.lpstrFile[0] = 0;
                            ofn.lpstrFilter = TEXT("MP4 File (*.mp4)\0*.mp4\0Flash Video File (*.flv)\0*.flv\0");
                            ofn.lpstrFileTitle = NULL;
                            ofn.nMaxFileTitle = 0;
                            ofn.nFilterIndex = 1;
                            ofn.lpstrInitialDir = AppConfig->GetStringPtr(TEXT("Publish"), TEXT("LastSaveDir"));

                            ofn.Flags = OFN_PATHMUSTEXIST;

                            TCHAR curDirectory[512];
                            GetCurrentDirectory(511, curDirectory);

                            BOOL bChoseFile = GetSaveFileName(&ofn);
                            SetCurrentDirectory(curDirectory);

                            if(bChoseFile)
                            {
                                String strFile = lpFile;
                                strFile.FindReplace(TEXT("\\"), TEXT("/"));

                                String strExtension = GetPathExtension(strFile);
                                if(strExtension.IsEmpty() || (!strExtension.CompareI(TEXT("flv")) && /*!strExtension.CompareI(TEXT("avi")) &&*/ !strExtension.CompareI(TEXT("mp4"))))
                                {
                                    switch(ofn.nFilterIndex)
                                    {
                                        case 1:
                                            strFile << TEXT(".mp4"); break;
                                        case 2:
                                            strFile << TEXT(".flv"); break;
                                        /*case 3:
                                            strFile << TEXT(".avi"); break;*/
                                    }
                                }

                                String strFilePath = GetPathDirectory(strFile).FindReplace(TEXT("/"), TEXT("\\")) << TEXT("\\");
                                AppConfig->SetString(TEXT("Publish"), TEXT("LastSaveDir"), strFilePath);

                                strFile.FindReplace(TEXT("/"), TEXT("\\"));
                                SetWindowText(GetDlgItem(hwnd, IDC_SAVEPATH), strFile);
                                bDataChanged = true;
                            }

                            break;
                        }

                    case IDC_STARTSTREAMHOTKEY:
                    case IDC_STOPSTREAMHOTKEY:
                    case IDC_DASHBOARDLINK:
                        if(HIWORD(wParam) == EN_CHANGE)
                            App->SetChangedSettings(true);
                        break;

                        //case IDC_USERNAME:
                    case IDC_PLAYPATH:
                    case IDC_CHANNELNAME:
                    case IDC_SERVEREDIT:
                    case IDC_SAVEPATH:
                        if(HIWORD(wParam) == EN_CHANGE)
                            bDataChanged = true;
                        break;

                    case IDC_CLEARHOTKEY_STARTSTREAM:
                        if(HIWORD(wParam) == BN_CLICKED)
                        {
                            if(SendMessage(GetDlgItem(hwnd, IDC_STARTSTREAMHOTKEY), HKM_GETHOTKEY, 0, 0))
                            {
                                SendMessage(GetDlgItem(hwnd, IDC_STARTSTREAMHOTKEY), HKM_SETHOTKEY, 0, 0);
                                App->SetChangedSettings(true);
                            }
                        }
                        break;

                    case IDC_CLEARHOTKEY:
                        if(HIWORD(wParam) == BN_CLICKED)
                        {
                            if(SendMessage(GetDlgItem(hwnd, IDC_STOPSTREAMHOTKEY), HKM_GETHOTKEY, 0, 0))
                            {
                                SendMessage(GetDlgItem(hwnd, IDC_STOPSTREAMHOTKEY), HKM_SETHOTKEY, 0, 0);
                                App->SetChangedSettings(true);
                            }
                        }
                        break;

                    case IDC_SERVERLIST:
                        if(HIWORD(wParam) == CBN_EDITCHANGE || HIWORD(wParam) == CBN_SELCHANGE)
                            bDataChanged = true;
                        break;
                }

                if(bDataChanged)
                {
                    ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                    App->SetChangedSettings(true);
                }
                break;
            }
    }
    return FALSE;
}


const int multiplierCount = 5;
const float downscaleMultipliers[multiplierCount] = {1.0f, 1.5f, 2.0f, 2.25f, 3.0f};

void OBS::RefreshDownscales(HWND hwnd, int cx, int cy)
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

INT_PTR CALLBACK OBS::VideoSettingsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND hwndTemp;
    switch(message)
    {
        case WM_INITDIALOG:
            {
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


                hwndTemp = GetDlgItem(hwnd, IDC_SIZEX);
                editProc = (FARPROC)GetWindowLongPtr(hwndTemp, GWLP_WNDPROC);
                SetWindowLongPtr(hwndTemp, GWLP_WNDPROC, (LONG_PTR)ResolutionEditSubclassProc);
                SetWindowText(hwndTemp, IntString(cx).Array());

                hwndTemp = GetDlgItem(hwnd, IDC_SIZEY);
                SetWindowLongPtr(hwndTemp, GWLP_WNDPROC, (LONG_PTR)ResolutionEditSubclassProc);
                SetWindowText(hwndTemp, IntString(cy).Array());

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_DISABLEAERO);

                BOOL bDisableAero = AppConfig->GetInt(TEXT("Video"), TEXT("DisableAero"), 0);
                SendMessage(hwndTemp, BM_SETCHECK, bDisableAero ? BST_CHECKED : 0, 0);

                ti.lpszText = (LPWSTR)Str("Settings.Video.DisableAeroTooltip");
                ti.uId = (UINT_PTR)hwndTemp;
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_FPS);
                SendMessage(hwndTemp, UDM_SETRANGE32, 10, 60);

                int fps = AppConfig->GetInt(TEXT("Video"), TEXT("FPS"), 30);
                if(!AppConfig->HasKey(TEXT("Video"), TEXT("FPS")) || fps < 10 || fps > 60)
                {
                    AppConfig->SetInt(TEXT("Video"), TEXT("FPS"), 30);
                    fps = 30;
                }

                SendMessage(hwndTemp, UDM_SETPOS32, 0, fps);

                //--------------------------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_DOWNSCALE);
                App->RefreshDownscales(hwndTemp, cx, cy);

                ti.lpszText = (LPWSTR)Str("Settings.Video.DownscaleTooltip");
                ti.uId = (UINT_PTR)hwndTemp;
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                //--------------------------------------------

                ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_HIDE);
                App->SetChangedSettings(false);

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
                            }
                            break;
                        }

                    case IDC_SIZEX:
                    case IDC_SIZEY:
                        {
                            if(HIWORD(wParam) != EN_CHANGE)
                                break;

                            String strInt = GetEditText((HWND)lParam);
                            int iVal = strInt.ToInt();

                            int cx = GetEditText(GetDlgItem(hwnd, IDC_SIZEX)).ToInt();
                            int cy = GetEditText(GetDlgItem(hwnd, IDC_SIZEY)).ToInt();

                            if(cx < 128)        cx = 128;
                            else if(cx > 4096)  cx = 4096;

                            if(cy < 128)        cy = 128;
                            else if(cy > 4096)  cy = 4096;

                            App->RefreshDownscales(GetDlgItem(hwnd, IDC_DOWNSCALE), cx, cy);

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

                    case IDC_DOWNSCALE:
                        if(HIWORD(wParam) == CBN_SELCHANGE)
                            bDataChanged = true;
                        break;
                }

                if(bDataChanged)
                {
                    ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                    App->SetChangedSettings(true);
                }
                break;
            }
    }
    return FALSE;
}

INT_PTR CALLBACK OBS::AudioSettingsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                LocalizeWindow(hwnd);

                //--------------------------------------------

                AudioDeviceList *audioDevices = new AudioDeviceList;
                GetAudioDevices(*audioDevices);

                HWND hwndTemp = GetDlgItem(hwnd, IDC_MICDEVICES);

                for(UINT i=0; i<audioDevices->devices.Num(); i++)
                    SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)audioDevices->devices[i].strName.Array());

                String strDeviceID = AppConfig->GetString(TEXT("Audio"), TEXT("Device"), audioDevices->devices[0].strID);

                UINT iDevice;
                for(iDevice=0; iDevice<audioDevices->devices.Num(); iDevice++)
                {
                    if(audioDevices->devices[iDevice].strID == strDeviceID)
                    {
                        SendMessage(hwndTemp, CB_SETCURSEL, iDevice, 0);
                        break;
                    }
                }

                if(iDevice == audioDevices->devices.Num())
                {
                    AppConfig->SetString(TEXT("Audio"), TEXT("Device"), audioDevices->devices[0].strID);
                    SendMessage(hwndTemp, CB_SETCURSEL, 0, 0);
                }

                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)audioDevices);

                //--------------------------------------------

                BOOL bPushToTalk = AppConfig->GetInt(TEXT("Audio"), TEXT("UsePushToTalk"));
                SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALK), BM_SETCHECK, bPushToTalk ? BST_CHECKED : BST_UNCHECKED, 0);
                EnableWindow(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY), bPushToTalk);
                EnableWindow(GetDlgItem(hwnd, IDC_CLEARPUSHTOTALK), bPushToTalk);

                DWORD hotkey = AppConfig->GetInt(TEXT("Audio"), TEXT("PushToTalkHotkey"));
                SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY), HKM_SETHOTKEY, hotkey, 0);

                //--------------------------------------------

                hotkey = AppConfig->GetInt(TEXT("Audio"), TEXT("MuteMicHotkey"));
                SendMessage(GetDlgItem(hwnd, IDC_MUTEMICHOTKEY), HKM_SETHOTKEY, hotkey, 0);

                //--------------------------------------------

                hotkey = AppConfig->GetInt(TEXT("Audio"), TEXT("MuteDesktopHotkey"));
                SendMessage(GetDlgItem(hwnd, IDC_MUTEDESKTOPHOTKEY), HKM_SETHOTKEY, hotkey, 0);

                //--------------------------------------------

                BOOL bForceMono = AppConfig->GetInt(TEXT("Audio"), TEXT("ForceMicMono"));
                SendMessage(GetDlgItem(hwnd, IDC_FORCEMONO), BM_SETCHECK, bForceMono ? BST_CHECKED : BST_UNCHECKED, 0);

                //--------------------------------------------

                DWORD micBoost = AppConfig->GetInt(TEXT("Audio"), TEXT("MicBoostMultiple"), 1);
                if(micBoost < 1)
                    micBoost = 1;
                else if(micBoost > 20)
                    micBoost = 20;
                SendMessage(GetDlgItem(hwnd, IDC_MICBOOST), UDM_SETRANGE32, 1, 20);
                SendMessage(GetDlgItem(hwnd, IDC_MICBOOST), UDM_SETPOS32, 0, micBoost);

                //--------------------------------------------

                App->SetChangedSettings(false);
                return TRUE;
            }

        case WM_DESTROY:
            {
                AudioDeviceList *audioDevices = (AudioDeviceList*)GetWindowLongPtr(hwnd, DWLP_USER);
                delete audioDevices;
            }

        case WM_COMMAND:
            {
                bool bDataChanged = false;

                switch(LOWORD(wParam))
                {
                    case IDC_PUSHTOTALK:
                        if(HIWORD(wParam) == BN_CLICKED)
                        {
                            BOOL bUsePushToTalk = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;
                            EnableWindow(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY), bUsePushToTalk);
                            EnableWindow(GetDlgItem(hwnd, IDC_CLEARPUSHTOTALK), bUsePushToTalk);
                            App->SetChangedSettings(true);
                        }
                        break;

                    case IDC_MICBOOST_EDIT:
                    case IDC_PUSHTOTALKHOTKEY:
                    case IDC_MUTEMICHOTKEY:
                    case IDC_MUTEDESKTOPHOTKEY:
                        if(HIWORD(wParam) == EN_CHANGE)
                            App->SetChangedSettings(true);
                        break;

                    case IDC_CLEARPUSHTOTALK:
                        if(HIWORD(wParam) == BN_CLICKED)
                        {
                            SendMessage(GetDlgItem(hwnd, IDC_PUSHTOTALKHOTKEY), HKM_SETHOTKEY, 0, 0);
                            App->SetChangedSettings(true);
                        }
                        break;

                    case IDC_CLEARMUTEMIC:
                        if(HIWORD(wParam) == BN_CLICKED)
                        {
                            SendMessage(GetDlgItem(hwnd, IDC_MUTEMICHOTKEY), HKM_SETHOTKEY, 0, 0);
                            App->SetChangedSettings(true);
                        }
                        break;

                    case IDC_CLEARMUTEDESKTOP:
                        if(HIWORD(wParam) == BN_CLICKED)
                        {
                            SendMessage(GetDlgItem(hwnd, IDC_MUTEDESKTOPHOTKEY), HKM_SETHOTKEY, 0, 0);
                            App->SetChangedSettings(true);
                        }
                        break;

                    case IDC_FORCEMONO:
                        if(HIWORD(wParam) == BN_CLICKED)
                            App->SetChangedSettings(true);
                        break;

                    case IDC_MICDEVICES:
                        if(HIWORD(wParam) == CBN_SELCHANGE)
                            bDataChanged = true;
                        break;
                }

                if(bDataChanged)
                {
                    ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                    App->SetChangedSettings(true);
                }
                break;
            }
    }
    return FALSE;
}

INT_PTR CALLBACK OBS::AdvancedSettingsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
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
                SendMessage(hwndToolTip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 14000);

                //------------------------------------

                bool bUseMTOptimizations = AppConfig->GetInt(TEXT("General"), TEXT("UseMultithreadedOptimizations"), TRUE) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_USEMULTITHREADEDOPTIMIZATIONS), BM_SETCHECK, bUseMTOptimizations ? BST_CHECKED : BST_UNCHECKED, 0);

                //--------------------------------------------

                HWND hwndTemp = GetDlgItem(hwnd, IDC_PRESET);
                for(int i=0; i<10; i++)
                    SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)preset_names[i]);

                LoadSettingComboString(hwndTemp, TEXT("Video Encoding"), TEXT("Preset"), TEXT("veryfast"));

                ti.lpszText = (LPWSTR)Str("Settings.Advanced.VideoEncoderCPUTradeoffTooltip");
                ti.uId = (UINT_PTR)hwndTemp;
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                //------------------------------------

                bool bUseCustomX264Settings = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseCustomSettings")) != 0;
                String strX264Settings = AppConfig->GetString(TEXT("Video Encoding"), TEXT("CustomSettings"));

                SendMessage(GetDlgItem(hwnd, IDC_USEVIDEOENCODERSETTINGS), BM_SETCHECK, bUseCustomX264Settings ? BST_CHECKED : BST_UNCHECKED, 0);
                SetWindowText(GetDlgItem(hwnd, IDC_VIDEOENCODERSETTINGS), strX264Settings);

                ti.lpszText = (LPWSTR)Str("Settings.Advanced.VideoEncoderSettingsTooltip");
                ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_VIDEOENCODERSETTINGS);
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_USEVIDEOENCODERSETTINGS);
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                EnableWindow(GetDlgItem(hwnd, IDC_VIDEOENCODERSETTINGS), bUseCustomX264Settings);

                //------------------------------------

                bool bUseVideoSyncFix = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseSyncFix")) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_USESYNCFIX), BM_SETCHECK, bUseVideoSyncFix ? BST_CHECKED : BST_UNCHECKED, 0);

                ti.lpszText = (LPWSTR)Str("Settings.Advanced.UseSyncFixTooltip");
                ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_USESYNCFIX);
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                //------------------------------------

                /*bool bDisableD3DCompat = AppConfig->GetInt(TEXT("Video"), TEXT("DisableD3DCompatibilityMode")) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_DISABLED3DCOMPATIBILITY), BM_SETCHECK, bDisableD3DCompat ? BST_CHECKED : BST_UNCHECKED, 0);

                ti.lpszText = (LPWSTR)Str("Settings.Advanced.DisableD3DCompatibilityModeTooltip");
                ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_DISABLED3DCOMPATIBILITY);
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);*/

                //------------------------------------

                bool bUseHQResampling = AppConfig->GetInt(TEXT("Audio"), TEXT("UseHighQualityResampling"), FALSE) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_USEHIGHQUALITYRESAMPLING), BM_SETCHECK, bUseHQResampling ? BST_CHECKED : BST_UNCHECKED, 0);

                ti.lpszText = (LPWSTR)Str("Settings.Advanced.UseHighQualityResamplingTooltip");
                ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_USEHIGHQUALITYRESAMPLING);
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                //------------------------------------

                BOOL bUseSendBuffer = AppConfig->GetInt(TEXT("Publish"), TEXT("UseSendBuffer"), 1) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_USESENDBUFFER), BM_SETCHECK, bUseSendBuffer ? BST_CHECKED : BST_UNCHECKED, 0);

                hwndTemp = GetDlgItem(hwnd, IDC_SENDBUFFERSIZE);
                EnableWindow(hwndTemp, bUseSendBuffer);
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("32768"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("16384"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("8192"));
                SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)TEXT("4096"));

                LoadSettingTextComboString(hwndTemp, TEXT("Publish"), TEXT("SendBufferSize"), TEXT("8196"));

                ti.lpszText = (LPWSTR)Str("Settings.Advanced.UseSendBufferTooltip");
                ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_USESENDBUFFER);
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                //------------------------------------

                App->SetChangedSettings(false);
                return TRUE;
            }

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_USEVIDEOENCODERSETTINGS:
                    if(HIWORD(wParam) == BN_CLICKED)
                    {
                        BOOL bUseVideoEncoderSettings = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;
                        EnableWindow(GetDlgItem(hwnd, IDC_VIDEOENCODERSETTINGS), bUseVideoEncoderSettings);

                        ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                        App->SetChangedSettings(true);
                    }
                    break;

                case IDC_VIDEOENCODERSETTINGS:
                    if(HIWORD(wParam) == EN_CHANGE)
                    {
                        ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                        App->SetChangedSettings(true);
                    }
                    break;

                /*case IDC_TIMER1:
                case IDC_TIMER2:
                case IDC_TIMER3:
                case IDC_DISABLED3DCOMPATIBILITY:
                    if(HIWORD(wParam) == BN_CLICKED)
                    {
                        ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                        App->SetChangedSettings(true);
                    }
                    break;*/

                case IDC_USESENDBUFFER:
                    if(HIWORD(wParam) == BN_CLICKED)
                    {
                        BOOL bUseSendBuffer = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;
                        EnableWindow(GetDlgItem(hwnd, IDC_SENDBUFFERSIZE), bUseSendBuffer);

                        ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                        App->SetChangedSettings(true);
                    }
                    break;

                case IDC_SENDBUFFERSIZE:
                case IDC_PRESET:
                    if(HIWORD(wParam) == CBN_SELCHANGE)
                    {
                        ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                        App->SetChangedSettings(true);
                    }
                    break;

                case IDC_USEHIGHQUALITYRESAMPLING:
                case IDC_USEMULTITHREADEDOPTIMIZATIONS:
                case IDC_USESYNCFIX:
                    if(HIWORD(wParam) == BN_CLICKED)
                    {
                        ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                        App->SetChangedSettings(true);
                    }
                    break;
            }

    }
    return FALSE;
}


void OBS::ApplySettings()
{
    switch(curSettingsSelection)
    {
        case Settings_Encoding:
            {
                int quality = (int)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_QUALITY), CB_GETCURSEL, 0, 0);
                if(quality != CB_ERR)
                    AppConfig->SetInt(TEXT("Video Encoding"), TEXT("Quality"), quality);

                UINT bitrate = GetEditText(GetDlgItem(hwndCurrentSettings, IDC_MAXBITRATE)).ToInt();
                if(bitrate < 100) bitrate = 100;
                AppConfig->SetInt(TEXT("Video Encoding"), TEXT("MaxBitrate"), bitrate);

                UINT bufSize = GetEditText(GetDlgItem(hwndCurrentSettings, IDC_BUFFERSIZE)).ToInt();
                if(bufSize < 100) bitrate = bufSize;
                AppConfig->SetInt(TEXT("Video Encoding"), TEXT("BufferSize"), bufSize);

                BOOL bUse444 = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_USEI444), BM_GETCHECK, 0, 0) == BST_CHECKED;
                AppConfig->SetInt(TEXT("Video Encoding"), TEXT("Use444"), bUse444);

                String strTemp = GetCBText(GetDlgItem(hwndCurrentSettings, IDC_AUDIOCODEC));
                AppConfig->SetString(TEXT("Audio Encoding"), TEXT("Codec"), strTemp);

                strTemp = GetCBText(GetDlgItem(hwndCurrentSettings, IDC_AUDIOBITRATE));
                AppConfig->SetString(TEXT("Audio Encoding"), TEXT("Bitrate"), strTemp);

                int curSel = (int)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_AUDIOFORMAT), CB_GETCURSEL, 0, 0);
                if(curSel != CB_ERR)
                    AppConfig->SetInt(TEXT("Audio Encoding"), TEXT("Format"), curSel);
                break;
            }

        case Settings_Publish:
            {
                int curSel = (int)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_MODE), CB_GETCURSEL, 0, 0);
                if(curSel != CB_ERR)
                    AppConfig->SetInt(TEXT("Publish"), TEXT("Mode"), curSel);

                int serviceID = (int)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_SERVICE), CB_GETCURSEL, 0, 0);
                if(serviceID != CB_ERR)
                    AppConfig->SetInt(TEXT("Publish"), TEXT("Service"), serviceID);

                /*String strTemp = GetEditText(GetDlgItem(hwndCurrentSettings, IDC_USERNAME));
                AppConfig->SetString(TEXT("Publish"), TEXT("Username"), strTemp);*/

                String strTemp = GetEditText(GetDlgItem(hwndCurrentSettings, IDC_CHANNELNAME));
                AppConfig->SetString(TEXT("Publish"), TEXT("Channel"), strTemp);

                strTemp = GetEditText(GetDlgItem(hwndCurrentSettings, IDC_PLAYPATH));
                AppConfig->SetString(TEXT("Publish"), TEXT("PlayPath"), strTemp);

                if(serviceID == 0)
                {
                    strTemp = GetEditText(GetDlgItem(hwndCurrentSettings, IDC_SERVEREDIT));
                    AppConfig->SetString(TEXT("Publish"), TEXT("Server"), strTemp);
                }
                else
                {
                    strTemp = GetCBText(GetDlgItem(hwndCurrentSettings, IDC_SERVERLIST));
                    AppConfig->SetString(TEXT("Publish"), TEXT("Server"), strTemp);
                }

                //------------------------------------------

                App->bAutoReconnect = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_AUTORECONNECT), BM_GETCHECK, 0, 0) == BST_CHECKED;

                BOOL bError = FALSE;
                App->reconnectTimeout = (UINT)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_AUTORECONNECT_TIMEOUT), UDM_GETPOS32, 0, (LPARAM)&bError);
                if(bError)
                    App->reconnectTimeout = 5;

                AppConfig->SetInt(TEXT("Publish"), TEXT("AutoReconnect"), App->bAutoReconnect);
                AppConfig->SetInt(TEXT("Publish"), TEXT("AutoReconnectTimeout"), App->reconnectTimeout);

                //------------------------------------------

                String strSavePath = GetEditText(GetDlgItem(hwndCurrentSettings, IDC_SAVEPATH));
                BOOL bSaveToFile = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_SAVETOFILE), BM_GETCHECK, 0, 0) != BST_UNCHECKED;

                if(!strSavePath.IsValid())
                    bSaveToFile = FALSE;

                AppConfig->SetInt   (TEXT("Publish"), TEXT("SaveToFile"), bSaveToFile);
                AppConfig->SetString(TEXT("Publish"), TEXT("SavePath"),   strSavePath);

                //------------------------------------------

                DWORD stopStreamHotkey = (DWORD)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_STOPSTREAMHOTKEY), HKM_GETHOTKEY, 0, 0);
                AppConfig->SetInt(TEXT("Publish"), TEXT("StopStreamHotkey"), stopStreamHotkey);

                if(App->stopStreamHotkeyID)
                {
                    API->DeleteHotkey(App->stopStreamHotkeyID);
                    App->stopStreamHotkeyID = 0;
                }

                if(stopStreamHotkey)
                    App->stopStreamHotkeyID = API->CreateHotkey(stopStreamHotkey, OBS::StopStreamHotkey, NULL);

                //------------------------------------------

                DWORD startStreamHotkey = (DWORD)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_STARTSTREAMHOTKEY), HKM_GETHOTKEY, 0, 0);
                AppConfig->SetInt(TEXT("Publish"), TEXT("StartStreamHotkey"), startStreamHotkey);

                if(App->startStreamHotkeyID)
                {
                    API->DeleteHotkey(App->startStreamHotkeyID);
                    App->startStreamHotkeyID = 0;
                }

                if(startStreamHotkey)
                    App->startStreamHotkeyID = API->CreateHotkey(startStreamHotkey, OBS::StartStreamHotkey, NULL);

                //------------------------------------------

                App->strDashboard = GetEditText(GetDlgItem(hwndCurrentSettings, IDC_DASHBOARDLINK)).KillSpaces();
                AppConfig->SetString(TEXT("Publish"), TEXT("Dashboard"), App->strDashboard);
                ShowWindow(GetDlgItem(hwndMain, ID_DASHBOARD), App->strDashboard.IsValid() && !curSel ? SW_SHOW : SW_HIDE);

                break;
            }

        case Settings_Video:
            {
                int curSel = (int)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_MONITOR), CB_GETCURSEL, 0, 0);
                if(curSel != CB_ERR)
                    AppConfig->SetInt(TEXT("Video"), TEXT("Monitor"), curSel);

                int iVal = GetEditText(GetDlgItem(hwndCurrentSettings, IDC_SIZEX)).ToInt();
                if(iVal >=  128)
                    AppConfig->SetInt(TEXT("Video"), TEXT("BaseWidth"), iVal);

                iVal = GetEditText(GetDlgItem(hwndCurrentSettings, IDC_SIZEY)).ToInt();
                if(iVal >= 128)
                    AppConfig->SetInt(TEXT("Video"), TEXT("BaseHeight"), iVal);

                BOOL bDisableAero = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_DISABLEAERO), BM_GETCHECK, 0, 0) == BST_CHECKED ? TRUE : FALSE;
                AppConfig->SetInt(TEXT("Video"), TEXT("DisableAero"), bDisableAero);

                BOOL bFailed;
                int fps = (int)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_FPS), UDM_GETPOS32, 0, (LPARAM)&bFailed);
                AppConfig->SetInt(TEXT("Video"), TEXT("FPS"), (bFailed) ? 30 : fps);

                curSel = (int)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_DOWNSCALE), CB_GETCURSEL, 0, 0);
                if(curSel != CB_ERR)
                    AppConfig->SetFloat(TEXT("Video"), TEXT("Downscale"), downscaleMultipliers[curSel]);

                if(!bRunning)
                    ResizeWindow(false);
                break;
            }

        case Settings_Audio:
            {
                AudioDeviceList *audioDevices = (AudioDeviceList*)GetWindowLongPtr(hwndCurrentSettings, DWLP_USER);

                UINT iDevice = (UINT)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_MICDEVICES), CB_GETCURSEL, 0, 0);

                String strDevice;
                if(iDevice == CB_ERR)
                    strDevice = TEXT("Disable");
                else
                    strDevice = audioDevices->devices[iDevice].strID;

                AppConfig->SetString(TEXT("Audio"), TEXT("Device"), strDevice);

                if(strDevice.CompareI(TEXT("Disable")))
                    EnableWindow(GetDlgItem(hwndMain, ID_MICVOLUME), FALSE);
                else
                    EnableWindow(GetDlgItem(hwndMain, ID_MICVOLUME), TRUE);

                //------------------------------------

                bUsingPushToTalk = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_PUSHTOTALK), BM_GETCHECK, 0, 0) == BST_CHECKED;
                DWORD hotkey = (DWORD)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_PUSHTOTALKHOTKEY), HKM_GETHOTKEY, 0, 0);

                AppConfig->SetInt(TEXT("Audio"), TEXT("UsePushToTalk"), bUsingPushToTalk);
                AppConfig->SetInt(TEXT("Audio"), TEXT("PushToTalkHotkey"), hotkey);

                if(App->pushToTalkHotkeyID)
                {
                    API->DeleteHotkey(App->pushToTalkHotkeyID);
                    App->pushToTalkHotkeyID = 0;
                }

                if(App->bUsingPushToTalk && hotkey)
                    pushToTalkHotkeyID = API->CreateHotkey(hotkey, OBS::PushToTalkHotkey, NULL);

                //------------------------------------

                hotkey = (DWORD)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_MUTEMICHOTKEY), HKM_GETHOTKEY, 0, 0);
                AppConfig->SetInt(TEXT("Audio"), TEXT("MuteMicHotkey"), hotkey);

                if(App->muteMicHotkeyID)
                {
                    API->DeleteHotkey(App->muteMicHotkeyID);
                    App->muteDesktopHotkeyID = 0;
                }

                if(hotkey)
                    muteMicHotkeyID = API->CreateHotkey(hotkey, OBS::MuteMicHotkey, NULL);

                //------------------------------------

                hotkey = (DWORD)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_MUTEDESKTOPHOTKEY), HKM_GETHOTKEY, 0, 0);
                AppConfig->SetInt(TEXT("Audio"), TEXT("MuteDesktopHotkey"), hotkey);

                if(App->muteDesktopHotkeyID)
                {
                    API->DeleteHotkey(App->muteDesktopHotkeyID);
                    App->muteDesktopHotkeyID = 0;
                }

                if(hotkey)
                    muteDesktopHotkeyID = API->CreateHotkey(hotkey, OBS::MuteDesktopHotkey, NULL);

                //------------------------------------

                App->bForceMicMono = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_FORCEMONO), BM_GETCHECK, 0, 0) == BST_CHECKED;
                AppConfig->SetInt(TEXT("Audio"), TEXT("ForceMicMono"), bForceMicMono);

                //------------------------------------

                DWORD micBoostMultiple = (DWORD)SendMessage(GetDlgItem(hwndCurrentSettings, IDC_MICBOOST), UDM_GETPOS32, 0, 0);
                if(micBoostMultiple < 1)
                    micBoostMultiple = 1;
                else if(micBoostMultiple > 20)
                    micBoostMultiple = 20;
                AppConfig->SetInt(TEXT("Audio"), TEXT("MicBoostMultiple"), micBoostMultiple);
                App->micBoost = float(micBoostMultiple);

                break;
            }

        case Settings_Advanced:
            {
                String strTemp = GetCBText(GetDlgItem(hwndCurrentSettings, IDC_PRESET));
                AppConfig->SetString(TEXT("Video Encoding"), TEXT("Preset"), strTemp);

                //--------------------------------------------------

                bool bUseMTOptimizations = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_USEMULTITHREADEDOPTIMIZATIONS), BM_GETCHECK, 0, 0) == BST_CHECKED;
                AppConfig->SetInt(TEXT("General"), TEXT("UseMultithreadedOptimizations"), bUseMTOptimizations);

                //--------------------------------------------------

                BOOL bUseCustomX264Settings = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_USEVIDEOENCODERSETTINGS), BM_GETCHECK, 0, 0) == BST_CHECKED;
                String strCustomX264Settings = GetEditText(GetDlgItem(hwndCurrentSettings, IDC_VIDEOENCODERSETTINGS));

                AppConfig->SetInt   (TEXT("Video Encoding"), TEXT("UseCustomSettings"), bUseCustomX264Settings);
                AppConfig->SetString(TEXT("Video Encoding"), TEXT("CustomSettings"),    strCustomX264Settings);

                //--------------------------------------------------

                BOOL bUseVideoSyncFix = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_USESYNCFIX), BM_GETCHECK, 0, 0) == BST_CHECKED;
                AppConfig->SetInt   (TEXT("Video Encoding"), TEXT("UseSyncFix"),        bUseVideoSyncFix);

                //------------------------------------

                BOOL bUseHQResampling = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_USEHIGHQUALITYRESAMPLING), BM_GETCHECK, 0, 0) == BST_CHECKED;
                AppConfig->SetInt   (TEXT("Audio"), TEXT("UseHighQualityResampling"), bUseHQResampling);

                //--------------------------------------------------

                /*BOOL bDisableD3DCompat = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_DISABLED3DCOMPATIBILITY), BM_GETCHECK, 0, 0) == BST_CHECKED;
                AppConfig->SetInt   (TEXT("Video"), TEXT("DisableD3DCompatibilityMode"), bDisableD3DCompat);*/

                //--------------------------------------------------

                BOOL bUseSendBuffer = SendMessage(GetDlgItem(hwndCurrentSettings, IDC_USESENDBUFFER), BM_GETCHECK, 0, 0) == BST_CHECKED;
                String strSendBufferSize = GetCBText(GetDlgItem(hwndCurrentSettings, IDC_SENDBUFFERSIZE));

                AppConfig->SetInt   (TEXT("Publish"),        TEXT("UseSendBuffer"),     bUseSendBuffer);
                AppConfig->SetString(TEXT("Publish"),        TEXT("SendBufferSize"),    strSendBufferSize);
                break;
            }
    }

    bSettingsChanged = false;
    EnableWindow(GetDlgItem(hwndSettings, IDC_APPLY), FALSE);
}


INT_PTR CALLBACK OBS::SettingsDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                App->hwndSettings = hwnd;

                LocalizeWindow(hwnd);

                SendMessage(GetDlgItem(hwnd, IDC_SETTINGSLIST), LB_ADDSTRING, 0, (LPARAM)Str("Settings.General"));
                SendMessage(GetDlgItem(hwnd, IDC_SETTINGSLIST), LB_ADDSTRING, 0, (LPARAM)Str("Settings.Encoding"));
                SendMessage(GetDlgItem(hwnd, IDC_SETTINGSLIST), LB_ADDSTRING, 0, (LPARAM)Str("Settings.Publish"));
                SendMessage(GetDlgItem(hwnd, IDC_SETTINGSLIST), LB_ADDSTRING, 0, (LPARAM)Str("Settings.Video"));
                SendMessage(GetDlgItem(hwnd, IDC_SETTINGSLIST), LB_ADDSTRING, 0, (LPARAM)Str("Settings.Audio"));
                SendMessage(GetDlgItem(hwnd, IDC_SETTINGSLIST), LB_ADDSTRING, 0, (LPARAM)Str("Settings.Advanced"));

                RECT subDialogRect;
                GetWindowRect(GetDlgItem(hwnd, IDC_SUBDIALOG), &subDialogRect);
                ScreenToClient(hwnd, (LPPOINT)&subDialogRect.left);

                SendMessage(GetDlgItem(hwnd, IDC_SETTINGSLIST), LB_SETCURSEL, 0, 0);

                App->curSettingsSelection = 0;
                App->hwndCurrentSettings = CreateDialog(hinstMain, MAKEINTRESOURCE(IDD_SETTINGS_GENERAL), hwnd, (DLGPROC)OBS::GeneralSettingsProc);
                SetWindowPos(App->hwndCurrentSettings, NULL, subDialogRect.left, subDialogRect.top, 0, 0, SWP_NOSIZE);
                ShowWindow(App->hwndCurrentSettings, SW_SHOW);

                return TRUE;
            }

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_SETTINGSLIST:
                    {
                        if(HIWORD(wParam) != LBN_SELCHANGE)
                            break;

                        int sel = (int)SendMessage((HWND)lParam, LB_GETCURSEL, 0, 0);
                        if(sel == App->curSettingsSelection)
                            break;
                        else if(App->bSettingsChanged)
                        {
                            int id = MessageBox(hwnd, Str("Settings.SaveChangesPrompt"), Str("Settings.SaveChangesTitle"), MB_YESNOCANCEL);

                            if(id == IDCANCEL)
                            {
                                SendMessage((HWND)lParam, LB_SETCURSEL, App->curSettingsSelection, 0);
                                break;
                            }
                            else if(id == IDYES)
                                App->ApplySettings();
                        }

                        App->curSettingsSelection = sel;

                        DestroyWindow(App->hwndCurrentSettings);
                        App->hwndCurrentSettings = NULL;

                        RECT subDialogRect;
                        GetWindowRect(GetDlgItem(hwnd, IDC_SUBDIALOG), &subDialogRect);
                        ScreenToClient(hwnd, (LPPOINT)&subDialogRect.left);

                        switch(sel)
                        {
                            case Settings_General:
                                App->hwndCurrentSettings = CreateDialog(hinstMain, MAKEINTRESOURCE(IDD_SETTINGS_GENERAL), hwnd, (DLGPROC)OBS::GeneralSettingsProc);
                                break;
                            case Settings_Encoding:
                                App->hwndCurrentSettings = CreateDialog(hinstMain, MAKEINTRESOURCE(IDD_SETTINGS_ENCODING), hwnd, (DLGPROC)OBS::EncoderSettingsProc);
                                break;
                            case Settings_Publish:
                                App->hwndCurrentSettings = CreateDialog(hinstMain, MAKEINTRESOURCE(IDD_SETTINGS_PUBLISH), hwnd, (DLGPROC)OBS::PublishSettingsProc);
                                break;
                            case Settings_Video:
                                App->hwndCurrentSettings = CreateDialog(hinstMain, MAKEINTRESOURCE(IDD_SETTINGS_VIDEO), hwnd, (DLGPROC)OBS::VideoSettingsProc);
                                break;
                            case Settings_Audio:
                                App->hwndCurrentSettings = CreateDialog(hinstMain, MAKEINTRESOURCE(IDD_SETTINGS_AUDIO), hwnd, (DLGPROC)OBS::AudioSettingsProc);
                                break;
                            case Settings_Advanced:
                                App->hwndCurrentSettings = CreateDialog(hinstMain, MAKEINTRESOURCE(IDD_SETTINGS_ADVANCED), hwnd, (DLGPROC)OBS::AdvancedSettingsProc);
                                break;
                        }

                        if(App->hwndCurrentSettings)
                        {
                            SetWindowPos(App->hwndCurrentSettings, NULL, subDialogRect.left, subDialogRect.top, 0, 0, SWP_NOSIZE);
                            ShowWindow(App->hwndCurrentSettings, SW_SHOW);
                        }

                        break;
                    }
                case IDOK:
                    if(App->bSettingsChanged)
                        App->ApplySettings();
                    EndDialog(hwnd, IDOK);
                    App->hwndSettings = NULL;
                    break;

                case IDCANCEL:
                    EndDialog(hwnd, IDCANCEL);
                    App->hwndSettings = NULL;
                    break;

                case IDC_APPLY:
                    if(App->bSettingsChanged)
                        App->ApplySettings();
                    break;
            }
            break;
    }

    return FALSE;
}
