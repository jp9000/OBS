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

#pragma once

#include "OBSApi.h"

//============================================================================
// SettingsPane class

class BASE_EXPORT SettingsPane
{
    //-----------------------------------------------------------------------
    // Private members

protected:
    HWND    hwnd;

    //-----------------------------------------------------------------------
    // Static methods

public:
    static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    //-----------------------------------------------------------------------
    // Constructor/destructor
    
public:
    SettingsPane();
    virtual ~SettingsPane();

    //-----------------------------------------------------------------------
    // Methods

protected:
    void SetChangedSettings(bool isModified);
    void SetAbortApplySettings(bool abort);
    void SetCanOptimizeSettings(bool canOptimize);

public:
    virtual CTSTR GetCategory() const = 0;
    virtual HWND CreatePane(HWND parentHwnd) = 0;
    virtual void DestroyPane() = 0;
    virtual INT_PTR ProcMessage(UINT message, WPARAM wParam, LPARAM lParam) = 0;
    virtual void ApplySettings() = 0;
    virtual void CancelSettings() = 0;
    virtual bool HasDefaults() const;
    virtual void SetDefaults();
    virtual void OptimizeSettings();
};
