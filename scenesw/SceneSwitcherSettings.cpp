/********************************************************************************
 Copyright (C) 2013 Christophe Jeannin <chris.j84@free.fr>
                    Eric Bataille <e.c.p.bataille@gmail.com>

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

#include "scenesw.h"

SceneSwitcherSettings::SceneSwitcherSettings(SceneSwitcher *thePlugin, OBSHOTKEYPROC ToggleHotkeyProc):
                            SettingsPane(),
                            thePlugin(thePlugin),
                            ToggleHotkeyProc(ToggleHotkeyProc)
{
    pChange = false;
}

SceneSwitcherSettings::~SceneSwitcherSettings()
{
}

CTSTR SceneSwitcherSettings::GetCategory() const
{
	return PluginStr("Plugin.SettingsName");
}

HWND SceneSwitcherSettings::CreatePane(HWND parentHwnd)
{
    hwnd = CreateDialogParam(thePlugin->hinstDll, MAKEINTRESOURCE(IDD_SETTINGS_SCENESW),
                             parentHwnd, (DLGPROC)DialogProc, (LPARAM)this);

	//EM_SETCUEBANNER


	HWND hwndAppList = GetDlgItem(hwnd, IDC_APPLIST);
	HWND hwndMainScn = GetDlgItem(hwnd, IDC_MAINSCN);

	ComboBox_SetCueBannerText(hwndMainScn, PluginStr("Settings.Scene"));
	ComboBox_SetCueBannerText(hwndAppList, PluginStr("Settings.WindowTitle"));
    return hwnd;
}

void SceneSwitcherSettings::DestroyPane()
{
    DestroyWindow(hwnd);
    hwnd = NULL;
}

INT_PTR SceneSwitcherSettings::ProcMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
    case WM_INITDIALOG:
        MsgInitDialog();
        return TRUE;
    case WM_COMMAND:
        return MsgClicked(LOWORD(wParam), HIWORD(wParam), (HWND)lParam);
    case WM_CTLCOLORSTATIC:
		if(GetWindowLong((HWND)lParam, GWL_ID) == IDC_RUN)
		{
			HDC hdc = (HDC)wParam;
			SetTextColor(hdc, thePlugin->IsRunning() ? RGB(0,255,0) : RGB(255,0,0));
			SetBkColor(hdc, GetSysColor(COLOR_3DFACE));
			return (INT_PTR)GetSysColorBrush(COLOR_3DFACE);
		}
        break;
    case WM_NOTIFY:
        switch (LOWORD(wParam)) {
        case IDC_WSMAP:
            {
		        const NMITEMACTIVATE* lpnmitem = (LPNMITEMACTIVATE)lParam;
		        if(lpnmitem->hdr.idFrom == IDC_WSMAP && lpnmitem->hdr.code == NM_CLICK)
		        {
			        const int sel = lpnmitem->iItem;
			        if(sel >= 0)
			        {
				        HWND wsMap = GetDlgItem(hwnd, IDC_WSMAP);
				        HWND hwndAppList = GetDlgItem(hwnd, IDC_APPLIST);
				        HWND hwndMainScn = GetDlgItem(hwnd, IDC_MAINSCN);

				        // Get the text from the item
				        String wnd;
				        wnd.SetLength(256);
				        ListView_GetItemText(wsMap, sel, 0, wnd, 256);
				        String scn;
				        scn.SetLength(256);
				        ListView_GetItemText(wsMap, sel, 1, scn, 256);

				        // Set the combos
				        SetWindowText(hwndAppList, wnd);
				        SendMessage(hwndMainScn, CB_SETCURSEL, SendMessage(hwndMainScn, CB_FINDSTRINGEXACT, -1, (LPARAM)scn.Array()), 0);
			        }
                }
                return TRUE;
            }
            break;
        }
    }
    return FALSE;
}

void SceneSwitcherSettings::MsgInitDialog()
{
    LocalizeWindow(hwnd, thePlugin->pluginLocale);

    HWND hwndAppList = GetDlgItem(hwnd, IDC_APPLIST);
	HWND hwndMainScn = GetDlgItem(hwnd, IDC_MAINSCN);
	HWND hwndAltScn  = GetDlgItem(hwnd, IDC_ALTSCN);
	HWND hwndWSMap = GetDlgItem(hwnd, IDC_WSMAP);
	HWND hwndSwButton = GetDlgItem(hwnd, IDC_ALTSWITCH);
	HWND hwndNoswButton = GetDlgItem(hwnd, IDC_ALTNOSWITCH);
	HWND hwndCurrent = GetWindow(GetDesktopWindow(), GW_CHILD); // The top child of the desktop

	// let's fill the listcontrol
	SendMessage(hwndWSMap, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP, LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);
	LVCOLUMN col1;
	col1.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	col1.fmt = LVCFMT_RIGHT | LVCFMT_FIXED_WIDTH;
	col1.cx = 204+205;
	col1.pszText = (LPWSTR)PluginStr("Settings.WindowTitle");
	LVCOLUMN col2;
	col2.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	col2.fmt = LVCFMT_LEFT | LVCFMT_FIXED_WIDTH;
	col2.cx = 205;
	col2.pszText = (LPWSTR)PluginStr("Settings.Scene");
	SendMessage(hwndWSMap, LVM_INSERTCOLUMN, 0, (LPARAM) (LPLVCOLUMN) &col1);
	SendMessage(hwndWSMap, LVM_INSERTCOLUMN, 1, (LPARAM) (LPLVCOLUMN) &col2);
	// Add the items
	for (int i = 0; i < thePlugin->GetnWindowsDefined(); i++)
	{
		String window = thePlugin->GetWindow(i); 
		String scene = thePlugin->GetScene(i);
		LVITEM item;
		item.iItem = 0;
		item.iSubItem = 0;
		item.mask = LVIF_TEXT;
		item.pszText = window;

		LVITEM subItem;
		subItem.iItem = 0;
		subItem.iSubItem = 1;
		subItem.mask = LVIF_TEXT;
		subItem.pszText = scene;

		SendMessage(hwndWSMap, LVM_INSERTITEM, 0, (LPARAM) &item);
		SendMessage(hwndWSMap, LVM_SETITEM, 0, (LPARAM) &subItem);
	}
	// Set the radio buttons and alt scene combo state
	SendMessage(hwndSwButton, BM_SETCHECK, (thePlugin->IsAltDoSwitch() ? BST_CHECKED : BST_UNCHECKED), 0);
	SendMessage(hwndNoswButton, BM_SETCHECK, (thePlugin->IsAltDoSwitch() ? BST_UNCHECKED : BST_CHECKED), 0);
	EnableWindow(hwndAltScn, thePlugin->IsAltDoSwitch());

	do
	{
		if(IsWindowVisible(hwndCurrent))
		{
			// Get the styles for the window
			DWORD exStyles = (DWORD)GetWindowLongPtr(hwndCurrent, GWL_EXSTYLE);
			DWORD styles = (DWORD)GetWindowLongPtr(hwndCurrent, GWL_STYLE);

			if( (exStyles & WS_EX_TOOLWINDOW) == 0 && (styles & WS_CHILD) == 0)
			{
				// The window is not a toolwindow, and not a child window
				String strWindowName;

				// Get the name of the window
				strWindowName.SetLength(GetWindowTextLength(hwndCurrent));
				GetWindowText(hwndCurrent, strWindowName, strWindowName.Length()+1);
				// Add the Name of the window to the appList
				const int id = (int)SendMessage(hwndAppList, CB_ADDSTRING, 0, (LPARAM)strWindowName.Array());
				// Set the data for the added item to be the window handle
				SendMessage(hwndAppList, CB_SETITEMDATA, id, (LPARAM)hwndCurrent);
			}
		}
	} // Move down the windows in z-order
	while (hwndCurrent = GetNextWindow(hwndCurrent, GW_HWNDNEXT));

	// Get the list of scenes from OBS
	XElement* scnList = OBSGetSceneListElement();
	if(scnList)
	{
		const DWORD numScn = scnList->NumElements();
		for(DWORD i=0; i<numScn; i++)
		{
			CTSTR sceneName = (scnList->GetElementByID(i))->GetName();
			// Add the scene name to both scene lists
			SendMessage(hwndMainScn, CB_ADDSTRING, 0, (LPARAM)sceneName);
			SendMessage(hwndAltScn,  CB_ADDSTRING, 0, (LPARAM)sceneName);
		}
	}

	// Set the selected list items to be the ones from settings
	SendMessage(hwndMainScn, CB_SETCURSEL, SendMessage(hwndMainScn, CB_FINDSTRINGEXACT, -1, (LPARAM)thePlugin->GetmainSceneName()), 0);
	SendMessage(hwndAltScn,  CB_SETCURSEL, SendMessage(hwndAltScn,  CB_FINDSTRINGEXACT, -1, (LPARAM)thePlugin->GetaltSceneName()), 0);
	SendMessage(hwndAppList, CB_SETCURSEL, SendMessage(hwndAppList, CB_FINDSTRINGEXACT, -1, (LPARAM)thePlugin->GetmainWndName()), 0);

	// Set the frequency from the settings
	SetDlgItemInt(hwnd, IDC_FREQ, thePlugin->GettimeToSleep(), FALSE);

	// Set the autostart checkbox value from the settings
	if(thePlugin->IsStartAuto())
		CheckDlgButton(hwnd, IDC_STARTAUTO, BST_CHECKED);

	// Set the toggle hotkey control
	SendMessage(GetDlgItem(hwnd, IDC_TOGGLEHOTKEY), HKM_SETHOTKEY, thePlugin->GetToggleHotkey(), 0);

	// If the plugin is running, update the text values
	if(thePlugin->IsRunning())
	{
		SetWindowText(GetDlgItem(hwnd, IDC_RUN), PluginStr("Settings.Running"));
		SetWindowText(GetDlgItem(hwnd, IDC_STOP), PluginStr("Settings.Stop"));
	}
}

INT_PTR SceneSwitcherSettings::MsgClicked(int controlId, int code, HWND controlHwnd)
{
    switch(controlId)
	{
	case IDC_CLEAR_HOTKEY:
		if(code == BN_CLICKED) {
			SendMessage(GetDlgItem(hwnd, IDC_TOGGLEHOTKEY), HKM_SETHOTKEY, 0, 0);
            SetChangedSettings(pChange = true);
            return TRUE;
        }
        break;

	case IDC_STOP:
		if(code == BN_CLICKED) // Stop button clicked
		{
			if(thePlugin->IsRunning())
				thePlugin->StopThread(hwnd);
			else
			{
				ApplyConfig(hwnd);
                pChange = false;
				thePlugin->StartThread(hwnd);
			}
            SetChangedSettings(pChange);
            return TRUE;
		}
        break;

	case IDUP:
		{
			HWND wsMap = GetDlgItem(hwnd, IDC_WSMAP);
			const int sel = SendMessage(wsMap, LVM_GETSELECTIONMARK, 0, 0);
			if (sel > 0)
			{
				// Get the text from the item
				String wnd;
				wnd.SetLength(256);
				ListView_GetItemText(wsMap, sel, 0, wnd, 256);
				String scn;
				scn.SetLength(256);
				ListView_GetItemText(wsMap, sel, 1, scn, 256);

				// Delete it
				SendMessage(wsMap, LVM_DELETEITEM, sel, 0);

				// Add it above
				LVITEM lv1;
				lv1.mask = LVIF_TEXT;
				lv1.iItem = sel - 1;
				lv1.iSubItem = 0;
				lv1.pszText = wnd;
				LVITEM lv2;
				lv2.mask = LVIF_TEXT;
				lv2.iItem = sel - 1;
				lv2.iSubItem = 1;
				lv2.pszText = scn;
				SendMessage(wsMap, LVM_INSERTITEM, sel - 1, (LPARAM) &lv1);
				SendMessage(wsMap, LVM_SETITEM, sel - 1, (LPARAM) &lv2);

				// Update the selection mark
				SendMessage(wsMap, LVM_SETSELECTIONMARK, 0, sel - 1);
                SetChangedSettings(pChange = true);
                return TRUE;
			}
            break;
		}
		

	case IDDOWN:
		{
			HWND wsMap = GetDlgItem(hwnd, IDC_WSMAP);
			const int sel = SendMessage(wsMap, LVM_GETSELECTIONMARK, 0, 0);
			const int max = SendMessage(wsMap, LVM_GETITEMCOUNT, 0, 0) - 1;
			if (sel > -1 && sel < max)
			{
				// Get the text from the item
				String wnd;
				wnd.SetLength(256);
				ListView_GetItemText(wsMap, sel, 0, wnd, 256);
				String scn;
				scn.SetLength(256);
				ListView_GetItemText(wsMap, sel, 1, scn, 256);

				// Delete it
				SendMessage(wsMap, LVM_DELETEITEM, sel, 0);

				// Add it below
				LVITEM lv1;
				lv1.mask = LVIF_TEXT;
				lv1.iItem = sel + 1;
				lv1.iSubItem = 0;
				lv1.pszText = wnd;
				LVITEM lv2;
				lv2.mask = LVIF_TEXT;
				lv2.iItem = sel + 1;
				lv2.iSubItem = 1;
				lv2.pszText = scn;
				SendMessage(wsMap, LVM_INSERTITEM, sel + 1, (LPARAM) &lv1);
				SendMessage(wsMap, LVM_SETITEM, sel + 1, (LPARAM) &lv2);

				// Update the selection mark
				SendMessage(wsMap, LVM_SETSELECTIONMARK, 0, sel + 1);
                SetChangedSettings(pChange = true);
                return TRUE;
			}
            break;
		}
		
	case IDADD:
		{
			HWND wsMap = GetDlgItem(hwnd, IDC_WSMAP);
			HWND appList = GetDlgItem(hwnd, IDC_APPLIST);
			HWND scnList = GetDlgItem(hwnd, IDC_MAINSCN);
				
			String wnd = GetEditText(appList);
			// First column
			LVITEM lv1;
			lv1.mask = LVIF_TEXT;
			lv1.iItem = 0;
			lv1.iSubItem = 0;
			lv1.pszText = wnd;
			// Second column
			String scn = GetCBText(scnList, CB_ERR);
			LVITEM lv2;
			lv2.mask = LVIF_TEXT;
			lv2.iItem = 0;
			lv2.iSubItem = 1;
			lv2.pszText = scn;
			// Add first column then set second
			SendMessage(wsMap, LVM_INSERTITEM, 0, (LPARAM)&lv1);
			SendMessage(wsMap, LVM_SETITEM, 0, (LPARAM)&lv2);
            SetChangedSettings(pChange = true);
            return TRUE;
		}
        break;

	case IDREM:
		{
			// Remove the item
			HWND wsMap = GetDlgItem(hwnd, IDC_WSMAP);
			const int sel = SendMessage(wsMap, LVM_GETSELECTIONMARK, 0, 0);
			if (sel > -1)
				SendMessage(wsMap, LVM_DELETEITEM, sel, 0);

            SetChangedSettings(pChange = true);
		    return TRUE;
        }
        break;

	case IDC_ALTSWITCH:
	case IDC_ALTNOSWITCH:
		if (code == BN_CLICKED)
		{
			HWND swButton = GetDlgItem(hwnd, IDC_ALTSWITCH);
			HWND altCombo = GetDlgItem(hwnd, IDC_ALTSCN);
			const bool swChecked = (SendMessage(swButton, BM_GETSTATE, 0, 0) & BST_CHECKED) != 0;
			EnableWindow(altCombo, swChecked);
            pChange = pChange || (swChecked != thePlugin->IsAltDoSwitch());
            SetChangedSettings(pChange);
            return TRUE;
		}
		break;

	case IDC_STARTAUTO:
        {
            HWND control = GetDlgItem(hwnd, IDC_STARTAUTO);
            bool newState = (SendMessage(control, BM_GETSTATE, 0, 0) & BST_CHECKED) != 0;
            pChange = pChange || (newState != thePlugin->IsStartAuto());
            SetChangedSettings(pChange);
            return TRUE;
        }
        break;
    case IDC_TOGGLEHOTKEY:
        if (code == EN_CHANGE) {
            SetChangedSettings(pChange = true);
            return TRUE;
        }
        break;
    case IDC_FREQ:
        if(code == EN_CHANGE)
        {
            DWORD newFreq = GetDlgItemInt(hwnd, IDC_FREQ, NULL, FALSE);
            DWORD oldFreq = thePlugin->GettimeToSleep();
            pChange = pChange || newFreq != oldFreq;
            SetChangedSettings(pChange);
            return TRUE;
        }
        break;
        case IDC_APPLIST:
        case IDC_MAINSCN:
            if (code == CBN_SELCHANGE || code == CBN_EDITCHANGE) {
                EditItem(code == CBN_SELCHANGE && controlId == IDC_APPLIST);
                return TRUE;
            }
            break;
        case IDC_ALTSCN:
            if (code == CBN_SELCHANGE) {
                SetChangedSettings(pChange = true);
                return TRUE;
            }
            break;
	}
    return FALSE;
}

void SceneSwitcherSettings::EditItem(bool selChange) {
    HWND wsMap = GetDlgItem(hwnd, IDC_WSMAP);
	HWND appList = GetDlgItem(hwnd, IDC_APPLIST);
	HWND scnList = GetDlgItem(hwnd, IDC_MAINSCN);
				
	const int sel = SendMessage(wsMap, LVM_GETSELECTIONMARK, 0, 0);
	if (sel < 0) return;

	String wnd = selChange ? GetCBText(appList, CB_ERR) : GetEditText(appList);
	// First column
	LVITEM lv1;
	lv1.mask = LVIF_TEXT;
	lv1.iItem = sel;
	lv1.iSubItem = 0;
	lv1.pszText = wnd;
	String scn = GetCBText(scnList, CB_ERR);
	// Second column
	LVITEM lv2;
	lv2.mask = LVIF_TEXT;
	lv2.iItem = sel;
	lv2.iSubItem = 1;
	lv2.pszText = scn;

	// Set the text
	SendMessage(wsMap, LVM_SETITEM, 0, (LPARAM)&lv1);
	SendMessage(wsMap, LVM_SETITEM, 0, (LPARAM)&lv2);
    SetChangedSettings(pChange = true);
}

void SceneSwitcherSettings::ApplyConfig(HWND hWnd) {
	thePlugin->ApplyConfig(hWnd);

	// Redefine the hotkey
	if (hotkeyID)
		OBSDeleteHotkey(hotkeyID);

	if (thePlugin->GetToggleHotkey() != 0)
		hotkeyID = OBSCreateHotkey((DWORD)thePlugin->GetToggleHotkey(), ToggleHotkeyProc, 0);
}

void SceneSwitcherSettings::ApplySettings()
{
    pChange = false;
    ApplyConfig(hwnd);
}

void SceneSwitcherSettings::CancelSettings()
{
    pChange = false;
}

bool SceneSwitcherSettings::HasDefaults() const
{
    return false;
}