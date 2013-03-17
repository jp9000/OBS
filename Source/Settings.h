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
#include "Main.h"

//============================================================================
// Helpers

int LoadSettingEditInt(HWND hwnd, CTSTR lpConfigSection, CTSTR lpConfigName, int defVal);
String LoadSettingEditString(HWND hwnd, CTSTR lpConfigSection, CTSTR lpConfigName, CTSTR lpDefault);
int LoadSettingComboInt(HWND hwnd, CTSTR lpConfigSection, CTSTR lpConfigName, int defVal, int maxVal);
String LoadSettingComboString(HWND hwnd, CTSTR lpConfigSection, CTSTR lpConfigName, CTSTR lpDefault);
String LoadSettingTextComboString(HWND hwnd, CTSTR lpConfigSection, CTSTR lpConfigName, CTSTR lpDefault);

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
    virtual HWND CreatePane(HWND parentHwnd);
    virtual void DestroyPane();
    virtual INT_PTR ProcMessage(UINT message, WPARAM wParam, LPARAM lParam);
    virtual void ApplySettings();
    virtual void CancelSettings();
};

//============================================================================
// SettingsEncoding class

class SettingsEncoding : public SettingsPane
{
    //-----------------------------------------------------------------------
    // Constructor/destructor

public:
    SettingsEncoding();
    virtual ~SettingsEncoding();

    //-----------------------------------------------------------------------
    // Methods

public:
    // Interface
    virtual CTSTR GetCategory() const;
    virtual HWND CreatePane(HWND parentHwnd);
    virtual void DestroyPane();
    virtual INT_PTR ProcMessage(UINT message, WPARAM wParam, LPARAM lParam);
    virtual void ApplySettings();
    virtual void CancelSettings();
};

//============================================================================
// SettingsPublish class

class SettingsPublish : public SettingsPane
{
    //-----------------------------------------------------------------------
    // Private data structures

private:
    // This should be converted to class members
    struct PublishDialogData
    {
        UINT mode;
        LONG fileControlOffset;
    };

    //-----------------------------------------------------------------------
    // Private members

private:
    PublishDialogData * data;

    //-----------------------------------------------------------------------
    // Constructor/destructor

public:
    SettingsPublish();
    virtual ~SettingsPublish();

    //-----------------------------------------------------------------------
    // Methods

public:
    // Interface
    virtual CTSTR GetCategory() const;
    virtual HWND CreatePane(HWND parentHwnd);
    virtual void DestroyPane();
    virtual INT_PTR ProcMessage(UINT message, WPARAM wParam, LPARAM lParam);
    virtual void ApplySettings();
    virtual void CancelSettings();
};

//============================================================================
// SettingsVideo class

class SettingsVideo : public SettingsPane
{
    //-----------------------------------------------------------------------
    // Constructor/destructor

public:
    SettingsVideo();
    virtual ~SettingsVideo();

    //-----------------------------------------------------------------------
    // Methods

private:
    void RefreshDownscales(HWND hwnd, int cx, int cy);

public:
    // Interface
    virtual CTSTR GetCategory() const;
    virtual HWND CreatePane(HWND parentHwnd);
    virtual void DestroyPane();
    virtual INT_PTR ProcMessage(UINT message, WPARAM wParam, LPARAM lParam);
    virtual void ApplySettings();
    virtual void CancelSettings();
};

//============================================================================
// SettingsAudio class

class SettingsAudio : public SettingsPane
{
    //-----------------------------------------------------------------------
    // Private data structures

private:
    // This should be converted to class members
    struct AudioDeviceStorage
    {
        AudioDeviceList *playbackDevices;
        AudioDeviceList *recordingDevices;
    };

    //-----------------------------------------------------------------------
    // Private members

private:
    AudioDeviceStorage * storage;

    //-----------------------------------------------------------------------
    // Constructor/destructor

public:
    SettingsAudio();
    virtual ~SettingsAudio();

    //-----------------------------------------------------------------------
    // Methods

public:
    // Interface
    virtual CTSTR GetCategory() const;
    virtual HWND CreatePane(HWND parentHwnd);
    virtual void DestroyPane();
    virtual INT_PTR ProcMessage(UINT message, WPARAM wParam, LPARAM lParam);
    virtual void ApplySettings();
    virtual void CancelSettings();
};

//============================================================================
// SettingsAdvanced class

class SettingsAdvanced : public SettingsPane
{
    //-----------------------------------------------------------------------
    // Constructor/destructor

public:
    SettingsAdvanced();
    virtual ~SettingsAdvanced();

    //-----------------------------------------------------------------------
    // Methods

public:
    // Interface
    virtual CTSTR GetCategory() const;
    virtual HWND CreatePane(HWND parentHwnd);
    virtual void DestroyPane();
    virtual INT_PTR ProcMessage(UINT message, WPARAM wParam, LPARAM lParam);
    virtual void ApplySettings();
    virtual void CancelSettings();
};
