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

#include "SettingsPane.h"

#pragma once

#define CONFIGFILENAME TEXT("\\scenesw.ini")
#define TTS_DEFAULT 300
#define TTS_MIN 50
#define TTS_MAX 5000
#define TTS_CLAMP(x) x = (x < TTS_MIN ? TTS_MIN : (x > TTS_MAX ? TTS_MAX : x))

#define PluginStr(text) thePlugin->pluginLocale->LookupString(TEXT2(text))

class SceneSwitcher;

class SceneSwitcherSettings : public SettingsPane {
private:
    SceneSwitcher * thePlugin;
    OBSHOTKEYPROC ToggleHotkeyProc;
    bool pChange;

    //-----------------------------------------------------------------------
    // Constructor/destructor
public:
    SceneSwitcherSettings(SceneSwitcher *thePlugin, OBSHOTKEYPROC ToggleHotkeyProc);
    virtual ~SceneSwitcherSettings();

    //-----------------------------------------------------------------------
    // Fields
public:
    UINT hotkeyID;
    DWORD hotkey;

    //-----------------------------------------------------------------------
    // Methods
private:
    void RefreshConfig();
    void ApplyConfig(HWND hWnd);
    void EditItem(bool selChange);

    // Message processing
    void MsgInitDialog();
    void MsgDestroy();
    INT_PTR MsgClicked(int controlId, int code, HWND controlHwnd);

public:
    // Interface
    virtual CTSTR GetCategory() const;
    virtual HWND CreatePane(HWND parentHwnd);
    virtual void DestroyPane();
    virtual INT_PTR ProcMessage(UINT message, WPARAM wParam, LPARAM lParam);
    virtual void ApplySettings();
    virtual void CancelSettings();
    virtual bool HasDefaults() const;
};


class SceneSwitcher
{
    friend class SceneSwitcherSettings;

	HANDLE thread;
	HANDLE timer;
	HANDLE stopReq;
	DWORD timeToSleep;
	String mainSceneName;
	String altSceneName;
	String mainWndName;
	int startAuto;
	bool bKillThread;
	ConfigFile config;
	int altDoSwitch;
	DWORD toggleHotkey;

    SceneSwitcherSettings * settings;

	int nWindowsDefined;
	StringList windows;
	StringList scenes;

	inline bool CheckSettings() const {return (!altSceneName.IsEmpty());}
	void ReadSettings();
	void WriteSettings();

	DWORD Run();
	static DWORD WINAPI ThreadProc(LPVOID pParam){return static_cast<SceneSwitcher*>(pParam)->Run();}

public:
    LocaleStringLookup *pluginLocale;

    HINSTANCE hinstDll;

	inline CTSTR GetmainSceneName() const {return mainSceneName;}
	inline CTSTR GetaltSceneName()  const {return altSceneName;}
	inline CTSTR GetmainWndName()   const {return mainWndName;}
	inline DWORD GettimeToSleep()   const {return timeToSleep;}
	inline bool IsRunning()		    const {return thread!=0;}
	inline bool IsStartAuto()	    const {return startAuto!=0;}
	inline bool IsAltDoSwitch()     const {return altDoSwitch!=0;}
	inline int GetToggleHotkey()	const {return toggleHotkey;}

	inline int GetnWindowsDefined() const {return nWindowsDefined;}
	inline String GetWindow(int i) const {return windows[i];}
	inline String GetScene(int i) const {return scenes[i];}

    inline SceneSwitcherSettings* GetSettings() const { return settings; }

	SceneSwitcher(HINSTANCE hinstMain, OBSHOTKEYPROC ToggleHotkeyProc);
	~SceneSwitcher();

	void StartThread(HWND hDialog = NULL);
	void StopThread (HWND hDialog = NULL);
	void ApplyConfig(HWND hDialog);
};