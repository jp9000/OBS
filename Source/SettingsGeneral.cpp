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
// Helpers

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

//============================================================================
// SettingsGeneral class

SettingsGeneral::SettingsGeneral()
    : SettingsPane()
{
}

SettingsGeneral::~SettingsGeneral()
{
}

CTSTR SettingsGeneral::GetCategory() const
{
    static CTSTR name = Str("Settings.General");
    return name;
}

HWND SettingsGeneral::CreatePane(HWND parentHwnd)
{
    hwnd = OBSCreateDialog(hinstMain, MAKEINTRESOURCE(IDD_SETTINGS_GENERAL), parentHwnd, (DLGPROC)DialogProc, (LPARAM)this);
    return hwnd;
}

void SettingsGeneral::DestroyPane()
{
    DestroyWindow(hwnd);
    hwnd = NULL;
}

void SettingsGeneral::ApplySettings()
{
    //--------------------------------------------------

    bool bShowNotificationAreaIcon = SendMessage(GetDlgItem(hwnd, IDC_NOTIFICATIONICON), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt(TEXT("General"), TEXT("ShowNotificationAreaIcon"), bShowNotificationAreaIcon);
    if (bShowNotificationAreaIcon)
        App->ShowNotificationAreaIcon();
    else
        App->HideNotificationAreaIcon();

    bool bMinimizeToNotificationArea = SendMessage(GetDlgItem(hwnd, IDC_MINIZENOTIFICATION), BM_GETCHECK, 0, 0) == BST_CHECKED;
    AppConfig->SetInt(TEXT("General"), TEXT("MinimizeToNotificationArea"), bMinimizeToNotificationArea);
    
    App->bEnableProjectorCursor = (SendMessage(GetDlgItem(hwnd, IDC_ENABLEPROJECTORCURSOR), BM_GETCHECK, 0, 0) == BST_CHECKED);
    GlobalConfig->SetInt(L"General", L"EnableProjectorCursor", App->bEnableProjectorCursor);

    bool showLogWindowOnLaunch = SendMessage(GetDlgItem(hwnd, IDC_SHOWLOGWINDOWONLAUNCH), BM_GETCHECK, 0, 0) == BST_CHECKED;
    GlobalConfig->SetInt(L"General", L"ShowLogWindowOnLaunch", showLogWindowOnLaunch);

    HWND hwndTemp = GetDlgItem(hwnd, IDC_LANGUAGE);
    int curSel = (int)SendMessage(hwndTemp, CB_GETCURSEL, 0, 0);
    if(curSel != CB_ERR)
    {
        String strLanguageCode = (CTSTR)SendMessage(hwndTemp, CB_GETITEMDATA, curSel, 0);
        GlobalConfig->SetString(TEXT("General"), TEXT("Language"), strLanguageCode);
    }
}

void SettingsGeneral::CancelSettings()
{
}

bool SettingsGeneral::HasDefaults() const
{
    return false;
}

INT_PTR SettingsGeneral::ProcMessage(UINT message, WPARAM wParam, LPARAM lParam)
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

                StringList profileList;
                App->GetProfiles(profileList);

                for(UINT i=0; i<profileList.Num(); i++)
                {
                    UINT id = (UINT)SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)profileList[i].Array());
                    if(profileList[i].CompareI(strCurProfile))
                        SendMessage(hwndTemp, CB_SETCURSEL, id, 0);
                }

                EnableWindow(hwndTemp, !App->bRunning);

                EnableWindow(GetDlgItem(hwnd, IDC_ADD),     FALSE);
                EnableWindow(GetDlgItem(hwnd, IDC_RENAME),  FALSE);

                UINT numItems = (UINT)SendMessage(hwndTemp, CB_GETCOUNT, 0, 0);
                EnableWindow(GetDlgItem(hwnd, IDC_REMOVE),  (numItems > 1) && !App->bRunning);

                //----------------------------------------------

                bool bShowNotificationAreaIcon = AppConfig->GetInt(TEXT("General"), TEXT("ShowNotificationAreaIcon"), 0) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_NOTIFICATIONICON), BM_SETCHECK, bShowNotificationAreaIcon ? BST_CHECKED : BST_UNCHECKED, 0);

                bool bMinimizeToNotificationArea = AppConfig->GetInt(TEXT("General"), TEXT("MinimizeToNotificationArea"), 0) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_MINIZENOTIFICATION), BM_SETCHECK, bMinimizeToNotificationArea ? BST_CHECKED : BST_UNCHECKED, 0);

                //----------------------------------------------

                App->bEnableProjectorCursor = GlobalConfig->GetInt(L"General", L"EnableProjectorCursor", 1) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_ENABLEPROJECTORCURSOR), BM_SETCHECK,
                        App->bEnableProjectorCursor ? BST_CHECKED : BST_UNCHECKED, 0);

                //----------------------------------------------

                bool showLogWindowOnLaunch = GlobalConfig->GetInt(TEXT("General"), TEXT("ShowLogWindowOnLaunch")) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_SHOWLOGWINDOWONLAUNCH), BM_SETCHECK, showLogWindowOnLaunch ? BST_CHECKED : BST_UNCHECKED, 0);

                //----------------------------------------------

                SetChangedSettings(false);
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
                        SetChangedSettings(true);
                        break;
                    }

                case IDC_PROFILE:
                    if (App->bRunning)
                    {
                        HWND cb = (HWND)lParam;
                        String curProfile = GlobalConfig->GetString(TEXT("General"), TEXT("Profile"));
                        UINT numItems = (UINT)SendMessage(cb, CB_GETCOUNT, 0, 0);
                        for (UINT i = 0; i < numItems; i++)
                        {
                            if (GetCBText(cb, i).Compare(curProfile))
                                SendMessage(cb, CB_SETCURSEL, i, 0);
                        }
                        break;
                    }

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
                            OBSMessageBox(hwnd, TEXT("Error - unable to open ini file"), NULL, 0);
                            break;
                        }

                        App->ReloadIniSettings();

                        SetWindowText(GetDlgItem(hwnd, IDC_INFO), Str("Settings.Info"));
                        ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);

                        GlobalConfig->SetString(TEXT("General"), TEXT("Profile"), strProfile);
                        App->ResetProfileMenu();
                        App->ResetApplicationName();
                        App->UpdateNotificationAreaIcon();

                        UINT numItems = (UINT)SendMessage(GetDlgItem(hwnd, IDC_PROFILE), CB_GETCOUNT, 0, 0);
                        EnableWindow(GetDlgItem(hwnd, IDC_REMOVE),  (numItems > 1));

                        App->ResizeWindow(false);
                    }
                    break;
                case IDC_RENAME:
                case IDC_ADD:
                    if (App->bRunning)
                        break;

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
                            OBSMessageBox(hwnd, Str("Settings.General.ProfileExists"), NULL, 0);
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
                                    OBSMessageBox(hwnd, TEXT("Error - unable to create new profile, could not create file"), NULL, 0);
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

                            App->ResetProfileMenu();
                            App->ResetApplicationName();
                        }
                    }
                    break;

                case IDC_REMOVE:
                    if (App->bRunning)
                        break;

                    {
                        HWND hwndProfileList = GetDlgItem(hwnd, IDC_PROFILE);

                        String strCurProfile = GlobalConfig->GetString(TEXT("General"), TEXT("Profile"));

                        UINT numItems = (UINT)SendMessage(hwndProfileList, CB_GETCOUNT, 0, 0);

                        String strConfirm = Str("Settings.General.ConfirmDelete");
                        strConfirm.FindReplace(TEXT("$1"), strCurProfile);
                        if(OBSMessageBox(hwnd, strConfirm, Str("DeleteConfirm.Title"), MB_YESNO) == IDYES)
                        {
                            UINT id = (UINT)SendMessage(hwndProfileList, CB_FINDSTRINGEXACT, -1, (LPARAM)strCurProfile.Array());
                            if(id != CB_ERR)
                            {
                                SendMessage(hwndProfileList, CB_DELETESTRING, id, 0);
                                if(id == numItems-1)
                                    id--;

                                SendMessage(hwndProfileList, CB_SETCURSEL, id, 0);
                                DialogProc(hwnd, WM_COMMAND, MAKEWPARAM(IDC_PROFILE, CBN_SELCHANGE), (LPARAM)hwndProfileList);

                                String strCurProfilePath;
                                strCurProfilePath << lpAppDataPath << TEXT("\\profiles\\") << strCurProfile << TEXT(".ini");
                                OSDeleteFile(strCurProfilePath);

                                App->ResetProfileMenu();
                            }
                        }

                        break;
                    }
                case IDC_NOTIFICATIONICON:
                    if (SendMessage(GetDlgItem(hwnd, IDC_NOTIFICATIONICON), BM_GETCHECK, 0, 0) == BST_UNCHECKED)
                    {
                        SendMessage(GetDlgItem(hwnd, IDC_MINIZENOTIFICATION), BM_SETCHECK, BST_UNCHECKED, 0);
                    }
                    SetChangedSettings(true);
                    break;

                case IDC_MINIZENOTIFICATION:
                    if (SendMessage(GetDlgItem(hwnd, IDC_MINIZENOTIFICATION), BM_GETCHECK, 0, 0) == BST_CHECKED)
                    {
                        SendMessage(GetDlgItem(hwnd, IDC_NOTIFICATIONICON), BM_SETCHECK, BST_CHECKED, 0);
                    }
                    SetChangedSettings(true);
                    break;

                case IDC_ENABLEPROJECTORCURSOR:
                case IDC_SHOWLOGWINDOWONLAUNCH:
                    SetChangedSettings(true);
                    break;
            }

    }
    return FALSE;
}
