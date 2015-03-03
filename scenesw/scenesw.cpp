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

extern "C" __declspec(dllexport) bool LoadPlugin();
extern "C" __declspec(dllexport) bool LoadPluginEx(UINT apiVer);
extern "C" __declspec(dllexport) void UnloadPlugin();
extern "C" __declspec(dllexport) CTSTR GetPluginName();
extern "C" __declspec(dllexport) CTSTR GetPluginDescription();

HINSTANCE hinstMain = NULL;
SceneSwitcher* thePlugin = NULL;

void STDCALL HotkeyToggle(DWORD hotkey, UPARAM param, bool bDown)
{
	if (!bDown) {
		if (thePlugin->IsRunning()) {
			thePlugin->StopThread();
		} else {
			thePlugin->StartThread();
		}
	}
}

bool LoadPluginEx(UINT apiVer)
{
	return (apiVer == 0x0103) && LoadPlugin();
}

bool LoadPlugin()
{
	InitHotkeyExControl(hinstMain);
	thePlugin = new SceneSwitcher(hinstMain, HotkeyToggle);
	if (thePlugin == 0) return false;

	if (thePlugin->GetToggleHotkey() != 0)
        thePlugin->GetSettings()->hotkeyID = OBSCreateHotkey(thePlugin->GetToggleHotkey(), HotkeyToggle, 0);

	return true;
}

void UnloadPlugin()
{
	if(thePlugin)
	{
        if (thePlugin->GetSettings()->hotkeyID)
            OBSDeleteHotkey(thePlugin->GetSettings()->hotkeyID);
        
        delete thePlugin;
		thePlugin = NULL;
	}
}

CTSTR GetPluginName()
{
	return PluginStr("Plugin.Name");
}

CTSTR GetPluginDescription()
{
	return PluginStr("Plugin.Description");
}

BOOL CALLBACK DllMain(HINSTANCE hInst, DWORD dwReason, LPVOID lpBla)
{
	if(dwReason == DLL_PROCESS_ATTACH)
		hinstMain = hInst;

	return TRUE;
}