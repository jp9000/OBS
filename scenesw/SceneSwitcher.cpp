/********************************************************************************
 Copyright (C) 2013 Christophe Jeannin <chris.j84@free.fr>
                    Hugh Bailey <obs.jim@gmail.com>
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

SceneSwitcher::SceneSwitcher(HINSTANCE hinstMain, OBSHOTKEYPROC ToggleHotkeyProc)
{
    hinstDll = hinstMain;

    // The localization part
    pluginLocale = new LocaleStringLookup;

    if(!pluginLocale->LoadStringFile(TEXT("plugins/scenesw/locale/en.txt")))
        AppWarning(TEXT("Could not open locale string file '%s'"), TEXT("plugins/scenesw/locale/en.txt"));

    if(scmpi(API->GetLanguage(), TEXT("en")) != 0)
    {
        String pluginStringFile;
        pluginStringFile << TEXT("plugins/scenesw/locale/") << API->GetLanguage() << TEXT(".txt");
        if(!pluginLocale->LoadStringFile(pluginStringFile))
            AppWarning(TEXT("Could not open locale string file '%s'"), pluginStringFile.Array());
    }

	timeToSleep = TTS_DEFAULT;
	thread = 0;
	timer = 0;
	bKillThread = true;
	mainSceneName.Clear();
	altSceneName.Clear();
	mainWndName.Clear();
	altDoSwitch = 1;

	// Start with an empty list of window-scene assignments
	nWindowsDefined = 0;
	scenes.Clear();
	windows.Clear();

	config.Open(OBSGetPluginDataPath() + CONFIGFILENAME, true);
	ReadSettings();

	stopReq = CreateEvent(NULL, FALSE, FALSE, NULL);

	if(startAuto)
	{
		StartThread();
	}
    
    settings = new SceneSwitcherSettings(this, ToggleHotkeyProc);
    OBSAddSettingsPane(settings);
}

SceneSwitcher::~SceneSwitcher()
{
    OBSRemoveSettingsPane(settings);
    delete settings;

    delete pluginLocale;

	StopThread();
	config.Close();
	CloseHandle(stopReq);
}


void SceneSwitcher::ReadSettings()
{
	mainWndName   = config.GetString(TEXT("General"), TEXT("MainWindow"));
	mainSceneName = config.GetString(TEXT("General"), TEXT("MainScene"));
	altSceneName  = config.GetString(TEXT("General"), TEXT("AltScene"));
	startAuto     = config.GetInt(TEXT("General"), TEXT("StartAuto"));
	timeToSleep   = (DWORD)config.GetInt(TEXT("General"), TEXT("CheckFrequency"));
	nWindowsDefined = config.GetInt(TEXT("GENERAL"), TEXT("nWindows"));
	altDoSwitch = config.GetInt(TEXT("GENERAL"), TEXT("AltDoSwitch"));
	toggleHotkey = (DWORD)config.GetInt(TEXT("GENERAL"), TEXT("ToggleHotkey"));
	scenes.Clear();
	windows.Clear();
	config.GetStringList(TEXT("SceneDef"), TEXT("window"), windows);
	config.GetStringList(TEXT("SceneDef"), TEXT("scene"), scenes);
    if ((int)windows.Num() < nWindowsDefined) nWindowsDefined = windows.Num();
    if ((int)scenes.Num() < nWindowsDefined) nWindowsDefined = scenes.Num();

	if(!timeToSleep)
		timeToSleep = TTS_DEFAULT;
	else
		TTS_CLAMP(timeToSleep);
}


void SceneSwitcher::WriteSettings()
{
	config.SetString(TEXT("General"), TEXT("MainWindow"), mainWndName);
	config.SetString(TEXT("General"), TEXT("MainScene"), mainSceneName);
	config.SetString(TEXT("General"), TEXT("AltScene"), altSceneName);
	config.SetInt(TEXT("General"), TEXT("StartAuto"), startAuto);
	config.SetInt(TEXT("General"), TEXT("CheckFrequency"), timeToSleep);
	config.SetInt(TEXT("General"), TEXT("nWindows"), nWindowsDefined);
	config.SetInt(TEXT("General"), TEXT("AltDoSwitch"), altDoSwitch);
	config.SetInt(TEXT("General"), TEXT("ToggleHotkey"), toggleHotkey);
	config.SetStringList(TEXT("SceneDef"), TEXT("window"), windows);
	config.SetStringList(TEXT("SceneDef"), TEXT("scene"), scenes);
}



DWORD SceneSwitcher::Run()
{
	LARGE_INTEGER dueTime;

	if((timer = CreateWaitableTimer(NULL, FALSE, NULL)) == NULL)
		return -1;
	dueTime.QuadPart=0;
	SetWaitableTimer(timer, &dueTime, timeToSleep, NULL, NULL, FALSE);
	HANDLE handles[] = { timer, stopReq };

	String currentWindowText;
	while(!bKillThread)
	{
		HWND hwndCurrent = GetForegroundWindow();
        DWORD_PTR textLength;
        if (!SendMessageTimeout(hwndCurrent, WM_GETTEXTLENGTH, 0, 0, SMTO_ABORTIFHUNG, 1000, &textLength))
            continue;
		currentWindowText.SetLength((UINT)textLength);
		//GetWindowText(hwndCurrent, currentWindowText, currentWindowText.Length() + 1);
        if (!SendMessageTimeout(hwndCurrent, WM_GETTEXT, currentWindowText.Length() + 1, (LPARAM)currentWindowText.Array(), SMTO_ABORTIFHUNG, 1000, NULL))
            continue;

        String currentScene = OBSGetSceneName();
		String sceneToSwitch;

		// See if we  get matches
		bool found = false;
		for (int i = 0; i < nWindowsDefined; i++)
		{
			String toCheck = GetWindow(i);
			BOOL match = currentWindowText == toCheck;

			if (!toCheck.IsEmpty())
			{
				// Check for wildcards.
				BOOL matchStart = toCheck.Left(1) != "*";
				BOOL matchEnd = toCheck.Right(1) != "*";

				// Remove the wildcards.
				if (!matchStart)
					toCheck = toCheck.Right(toCheck.Length() - 1);
				// Single character % toCheck is handled by this.
				if (!matchEnd && !toCheck.IsEmpty())
					toCheck = toCheck.Left(toCheck.Length() - 1);

				if (!matchStart &&
					!toCheck.IsEmpty() &&
					currentWindowText.Length() >= toCheck.Length() &&
					currentWindowText.Right(toCheck.Length()) == toCheck)
					match = TRUE;

				if (!matchEnd &&
					!toCheck.IsEmpty() &&
					currentWindowText.Length() >= toCheck.Length() &&
					currentWindowText.Left(toCheck.Length()) == toCheck)
					match = TRUE;
				
				// Empty string does not match anything, even if one or more wildcards are used.
				if (!matchStart && !matchEnd && !toCheck.IsEmpty() &&
					sstr(currentWindowText.Array(), toCheck.Array()))
					match = TRUE;
			}

			if (match)
			{
				sceneToSwitch = GetScene(i);
				found = true;
				break; // take the first one
			}
		}
		if (!found) // Otherwise, take the alternate scene
			sceneToSwitch = altSceneName;

		// Make the switch if it isn't already selected
		if (sceneToSwitch != currentScene && (found || altDoSwitch != 0))
			OBSSetScene(sceneToSwitch, true);

		WaitForMultipleObjects(2, handles, FALSE, INFINITE);
	}

	CloseHandle(timer);
	return 0;
}


void SceneSwitcher::StartThread(HWND hDialog)
{
	if(!thread)
	{
		bKillThread = false;
		thread = CreateThread(NULL, 0, ThreadProc, (LPVOID)this, 0, NULL);

		if(hDialog && thread)
		{
			SetWindowText(GetDlgItem(hDialog, IDC_RUN), pluginLocale->LookupString(TEXT("Settings.Running")));
			SetWindowText(GetDlgItem(hDialog, IDC_STOP), pluginLocale->LookupString(TEXT("Settings.Stop")));
		}
	}
}


void SceneSwitcher::StopThread(HWND hDialog)
{
	if(thread)
	{
		bKillThread = true;

        SetEvent(stopReq);
        WaitForSingleObject(thread, INFINITE);
        ResetEvent(stopReq);

		if(thread)
		{
			CloseHandle(thread);
			thread = 0;
		}
	}

	if(hDialog)
	{
        SetWindowText(GetDlgItem(hDialog, IDC_RUN), pluginLocale->LookupString(TEXT("Settings.NotRunning")));
		SetWindowText(GetDlgItem(hDialog, IDC_STOP), pluginLocale->LookupString(TEXT("Settings.Start")));
	}
}


void SceneSwitcher::ApplyConfig(HWND hDialog)
{
	if(!hDialog) // Noes.. Dialog don't exist?
		return;

	// Get the listbox control handles
	HWND hwndMainScn = GetDlgItem(hDialog, IDC_MAINSCN);
	HWND hwndAltScn  = GetDlgItem(hDialog, IDC_ALTSCN);
	HWND hwndAppList = GetDlgItem(hDialog, IDC_APPLIST);
	HWND hwndWSMap = GetDlgItem(hDialog, IDC_WSMAP);
	HWND hwndHotkey = GetDlgItem(hDialog, IDC_TOGGLEHOTKEY);

	// Get the options names from the controls
	String newMainSceneName = GetCBText(hwndMainScn, CB_ERR);
	String newAltSceneName  = GetCBText(hwndAltScn,  CB_ERR);
	String newMainWndName   = GetCBText(hwndAppList, CB_ERR);

	// Get the new frequency from the frequency control
	DWORD newTimeToSleep = GetDlgItemInt(hDialog, IDC_FREQ, NULL, FALSE);
	TTS_CLAMP(newTimeToSleep);

	// Get the toggle hotkey if any
	toggleHotkey = (DWORD)SendMessage(hwndHotkey, HKM_GETHOTKEY, 0, 0);

	// Get the checkboxes state
	altDoSwitch = IsDlgButtonChecked(hDialog, IDC_ALTSWITCH);
	startAuto   = IsDlgButtonChecked(hDialog, IDC_STARTAUTO);

	if(newAltSceneName.IsEmpty() && IsAltDoSwitch())
		StopThread(hDialog);	// if we're applying a bad config stop thread first

	if((timeToSleep != newTimeToSleep) && thread && timer)
	{							// reset timer with new value
		LARGE_INTEGER dueTime;
		dueTime.QuadPart=0;
		SetWaitableTimer(timer, &dueTime, newTimeToSleep, NULL, NULL, FALSE);
	}


	nWindowsDefined = SendMessage(hwndWSMap, LVM_GETITEMCOUNT, 0, 0);
	scenes.Clear();
	windows.Clear();
	for (int i = 0; i < nWindowsDefined; i++)
	{
		// Get the window name		
		String wnd;
		wnd.SetLength(256);
		ListView_GetItemText(hwndWSMap, i, 0, wnd, 256);
		wnd = String(wnd.Array());
		windows.Add(wnd);
			
		// Get the scene name
		String scn;
		scn.SetLength(256);
		ListView_GetItemText(hwndWSMap, i, 1, scn, 256);
		scn = String(scn.Array());
		scenes.Add(scn);
	}

	// Set the new global values
	timeToSleep   = newTimeToSleep;
	mainSceneName = newMainSceneName;
	altSceneName  = newAltSceneName;
	mainWndName   = newMainWndName;

	WriteSettings(); // Write the settings to config
}

