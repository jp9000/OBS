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

#pragma once

#include "SettingsPane.h"

//============================================================================
// SettingsGeneral class

class SettingsGeneral : public SettingsPane
{
    //-----------------------------------------------------------------------
    // Constructor/destructor
    
public:
    SettingsGeneral();
    virtual ~SettingsGeneral();

    //-----------------------------------------------------------------------
    // Methods

public:
    // Interface
    virtual CTSTR GetCategory() const;
    virtual HWND CreatePane(HINSTANCE hInstance, HWND parentHwnd);
    virtual void DestroyPane();
    virtual INT_PTR ProcMessage(UINT message, WPARAM wParam, LPARAM lParam);
    virtual void ApplySettings();
    virtual void CancelSettings();
};
