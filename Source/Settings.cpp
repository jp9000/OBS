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
#include "Settings.h"

//============================================================================
// Helpers

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

//============================================================================

void OBS::AddSettingsPane(SettingsPane *pane)
{
    settingsPanes.Add(pane);
}

void OBS::RemoveSettingsPane(SettingsPane *pane)
{
    settingsPanes.RemoveItem(pane);
}

void OBS::AddBuiltInSettingsPanes()
{
    AddSettingsPane(new SettingsGeneral());
    AddSettingsPane(new SettingsEncoding());
    AddSettingsPane(new SettingsPublish());
    AddSettingsPane(new SettingsVideo());
    AddSettingsPane(new SettingsAudio());
    AddSettingsPane(new SettingsAdvanced());
    numberOfBuiltInSettingsPanes = 6;
}

void OBS::SetChangedSettings(bool bChanged)
{
    if(hwndSettings == NULL)
        return;
    EnableWindow(GetDlgItem(hwndSettings, IDC_APPLY), (bSettingsChanged = bChanged));
}

void OBS::SetAbortApplySettings(bool abort)
{
    bApplySettingsAborted = abort;
}

void OBS::CancelSettings()
{
    if(App->currentSettingsPane != NULL)
        App->currentSettingsPane->CancelSettings();
}

void OBS::ApplySettings()
{
    bApplySettingsAborted = false;
    if(App->currentSettingsPane != NULL)
        App->currentSettingsPane->ApplySettings();
    if (bApplySettingsAborted)
        return;

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

                // Add setting categories from the pane list
                for(unsigned int i = 0; i < App->settingsPanes.Num(); i++)
                {
                    SettingsPane *pane = App->settingsPanes[i];
                    if(pane == NULL)
                        continue;
                    SendMessage(GetDlgItem(hwnd, IDC_SETTINGSLIST), LB_ADDSTRING, 0, (LPARAM)pane->GetCategory());
                }

                RECT subDialogRect;
                GetWindowRect(GetDlgItem(hwnd, IDC_SUBDIALOG), &subDialogRect);
                ScreenToClient(hwnd, (LPPOINT)&subDialogRect.left);

                SendMessage(GetDlgItem(hwnd, IDC_SETTINGSLIST), LB_SETCURSEL, 0, 0);

                // Load the first settings pane from the list
                App->curSettingsSelection = 0;
                App->hwndCurrentSettings = NULL;
                App->currentSettingsPane = NULL;

                if(App->settingsPanes.Num() >= 1)
                    App->currentSettingsPane = App->settingsPanes[0];

                if(App->currentSettingsPane != NULL)
                    App->hwndCurrentSettings = App->currentSettingsPane->CreatePane(hwnd);

                if(App->hwndCurrentSettings != NULL)
                {
                    SetWindowPos(App->hwndCurrentSettings, NULL, subDialogRect.left, subDialogRect.top, 0, 0, SWP_NOSIZE);
                    ShowWindow(App->hwndCurrentSettings, SW_SHOW);
                    ShowWindow(GetDlgItem(hwnd, IDC_DEFAULTS), App->currentSettingsPane->HasDefaults());
                }

                return TRUE;
            }

        case WM_DRAWITEM: 
            PDRAWITEMSTRUCT pdis;
            pdis = (PDRAWITEMSTRUCT) lParam;

            if(pdis->CtlID != IDC_SETTINGSLIST || pdis->itemID == -1)
                break;

            switch(pdis->itemAction)
            {
                case ODA_SELECT:
                case ODA_DRAWENTIRE:
                    {
                        int cy, bkMode;
                        TEXTMETRIC tm;
                        COLORREF oldTextColor;
                        TCHAR itemText[MAX_PATH];

                        oldTextColor = GetTextColor(pdis->hDC);

                        if(pdis->itemState & ODS_SELECTED)
                        {
                            FillRect(pdis->hDC, &pdis->rcItem, (HBRUSH)(COLOR_HIGHLIGHT + 1));
                            SetTextColor(pdis->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
                        }
                        else
                        {
                            FillRect(pdis->hDC, &pdis->rcItem, (HBRUSH)(COLOR_WINDOW + 1));
                            SetTextColor(pdis->hDC, GetSysColor(COLOR_WINDOWTEXT));
                        }

                        SendMessage(pdis->hwndItem, LB_GETTEXT, pdis->itemID, (LPARAM)itemText);

                        GetTextMetrics(pdis->hDC, &tm);
                        cy = (pdis->rcItem.bottom + pdis->rcItem.top - tm.tmHeight) / 2;

                        bkMode = SetBkMode(pdis->hDC, TRANSPARENT);

                        if(slen(itemText) > 0)
                            TextOut(pdis->hDC, 6, cy, itemText, slen(itemText));

                        SetBkMode(pdis->hDC, bkMode);
                        SetTextColor(pdis->hDC, oldTextColor);

                        if(App->settingsPanes.Num() > (UINT)App->numberOfBuiltInSettingsPanes)
                        {
                            if(pdis->itemID == App->numberOfBuiltInSettingsPanes)
                            {
                                HGDIOBJ origPen;
                                origPen = SelectObject(pdis->hDC, GetStockObject(DC_PEN));
                                SetDCPenColor(pdis->hDC, GetSysColor(COLOR_BTNSHADOW));

                                MoveToEx(pdis->hDC, pdis->rcItem.left, pdis->rcItem.top, NULL);
                                LineTo(pdis->hDC, pdis->rcItem.right, pdis->rcItem.top);

                                SelectObject(pdis->hDC, origPen);
                            }
                        }

                        break;
                    }

                case ODA_FOCUS:
                    break;
            }

            break;

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_SETTINGSLIST:
                    {
                        if(HIWORD(wParam) != LBN_SELCHANGE)
                            break;

                        int sel = (int)SendMessage((HWND)lParam, LB_GETCURSEL, 0, 0);

                        // No need to continue if we're on the same panel
                        if(sel == App->curSettingsSelection)
                            break;

                        if(App->bSettingsChanged)
                        {
                            int id = MessageBox(hwnd, Str("Settings.SaveChangesPrompt"), Str("Settings.SaveChangesTitle"), MB_YESNOCANCEL);

                            if (id == IDNO)
                                App->CancelSettings();

                            if(id == IDCANCEL)
                            {
                                SendMessage((HWND)lParam, LB_SETCURSEL, App->curSettingsSelection, 0);
                                break;
                            }
                            else if(id == IDYES)
                                App->ApplySettings();
                        }

                        App->curSettingsSelection = sel;

                        if(App->currentSettingsPane != NULL)
                            App->currentSettingsPane->DestroyPane();
                        App->currentSettingsPane = NULL;
                        App->hwndCurrentSettings = NULL;

                        RECT subDialogRect;
                        GetWindowRect(GetDlgItem(hwnd, IDC_SUBDIALOG), &subDialogRect);
                        ScreenToClient(hwnd, (LPPOINT)&subDialogRect.left);

                        if(sel >= 0 && sel < (int)App->settingsPanes.Num())
                            App->currentSettingsPane = App->settingsPanes[sel];
                        if(App->currentSettingsPane != NULL)
                            App->hwndCurrentSettings = App->currentSettingsPane->CreatePane(hwnd);
                        if(App->hwndCurrentSettings)
                        {
                            SetWindowPos(App->hwndCurrentSettings, NULL, subDialogRect.left, subDialogRect.top, 0, 0, SWP_NOSIZE);
                            ShowWindow(App->hwndCurrentSettings, SW_SHOW);

                            ShowWindow(GetDlgItem(hwnd, IDC_DEFAULTS), App->currentSettingsPane->HasDefaults());
                            SetFocus(GetDlgItem(hwnd, IDC_SETTINGSLIST));
                        }

                        break;
                    }

                case IDC_DEFAULTS:
                    App->currentSettingsPane->SetDefaults();
                    break;

                case IDOK:
                    if(App->bSettingsChanged)
                        App->ApplySettings();
                    if (App->bApplySettingsAborted)
                        break;
                    EndDialog(hwnd, IDOK);
                    App->hwndSettings = NULL;
                    break;

                case IDCANCEL:
                    if(App->bSettingsChanged)
                        App->CancelSettings();
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
