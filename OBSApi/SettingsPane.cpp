/********************************************************************************
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

#include "SettingsPane.h"

//============================================================================
// SettingsPane class

SettingsPane::SettingsPane()
    : hwnd(NULL)
{
}

SettingsPane::~SettingsPane()
{
}

/**
 * Should be called whenever the user modifies a setting that has not been saved to disk.
 */
void SettingsPane::SetChangedSettings(bool isModified)
{
    API->SetChangedSettings(isModified);
}

void SettingsPane::SetAbortApplySettings(bool abort)
{
    API->SetAbortApplySettings(abort);
}

void SettingsPane::SetCanOptimizeSettings(bool canOptimize)
{
    if (HasDefaults() && canOptimize)
        AppWarning(L"Defaults button hidden by optimize button in %s", GetCategory());
    API->SetCanOptimizeSettings(canOptimize);
}

void SettingsPane::OptimizeSettings()
{
}

INT_PTR CALLBACK SettingsPane::DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Get the pointer to our class instance
    SettingsPane *instance = (SettingsPane *)GetWindowLongPtr(hwnd, DWLP_USER);

    // Initialize pane if this is the first message
    if(message == WM_INITDIALOG)
    {
        SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)lParam);
        instance = (SettingsPane *)lParam;
        instance->hwnd = hwnd; // Not sure which processes first, this message or returning from CreateWindow()
        instance->ProcMessage(message, wParam, lParam);
        return TRUE; // Always return true
    }

    // Forward message to pane
    if(instance)
        return instance->ProcMessage(message, wParam, lParam);
    return FALSE;
}

bool SettingsPane::HasDefaults() const
{
    return false;
}

void SettingsPane::SetDefaults()
{
}
