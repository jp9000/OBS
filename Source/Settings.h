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
    virtual bool HasDefaults() const;
};

//============================================================================
// SettingsEncoding class

class SettingsEncoding : public SettingsPane
{
    bool hasQSV, hasNVENC;
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
    virtual bool HasDefaults() const;
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
    } data;

    //-----------------------------------------------------------------------
    // Private members
    std::vector<ServiceIdentifier> services;

private:
    void SetWarningInfo();

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
    virtual bool HasDefaults() const;
    virtual void OptimizeSettings() override;
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
    void RefreshFilters(HWND hwndParent, bool bGetConfig);

public:
    // Interface
    virtual CTSTR GetCategory() const;
    virtual HWND CreatePane(HWND parentHwnd);
    virtual void DestroyPane();
    virtual INT_PTR ProcMessage(UINT message, WPARAM wParam, LPARAM lParam);
    virtual void ApplySettings();
    virtual void CancelSettings();
    virtual bool HasDefaults() const;
    //virtual void SetDefaults();
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
        AudioDeviceList playbackDevices;
        AudioDeviceList recordingDevices;
    };

    //-----------------------------------------------------------------------
    // Private members
    bool bDisplayConnectedOnly, useInputDevices;

    void RefreshDevices(AudioDeviceType desktopDeviceType);

private:
    AudioDeviceStorage storage;

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
    virtual bool HasDefaults() const;
    //virtual void SetDefaults();
};

//============================================================================
// SettingsAdvanced class

class SettingsAdvanced : public SettingsPane
{
    bool bHasQSV, bHasNVENC;
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
    virtual bool HasDefaults() const;
    virtual void SetDefaults();
private:
    void SelectPresetDialog(bool useQSV, bool useNVENC);
};

//============================================================================
// SettingsHotkeys class

class SettingsHotkeys : public SettingsPane
{
    //-----------------------------------------------------------------------
    // Constructor/destructor

public:
    SettingsHotkeys();
    virtual ~SettingsHotkeys() override;

    //-----------------------------------------------------------------------
    // Methods

public:
    // Interface
    virtual CTSTR GetCategory() const override;
    virtual HWND CreatePane(HWND parentHwnd) override;
    virtual void DestroyPane() override;
    virtual INT_PTR ProcMessage(UINT message, WPARAM wParam, LPARAM lParam) override;
    virtual void ApplySettings() override;
    virtual void CancelSettings() override;
    virtual bool HasDefaults() const override;
};

//============================================================================
// SettingsQSV class

class SettingsQSV : public SettingsPane
{
    //-----------------------------------------------------------------------
    // Constructor/destructor

public:
    SettingsQSV();
    virtual ~SettingsQSV() override;

    //-----------------------------------------------------------------------
    // Methods

public:
    // Interface
    virtual CTSTR GetCategory() const override;
    virtual HWND CreatePane(HWND parentHwnd) override;
    virtual void DestroyPane() override;
    virtual INT_PTR ProcMessage(UINT message, WPARAM wParam, LPARAM lParam) override;
    virtual void ApplySettings() override;
    virtual void CancelSettings() override;
    virtual bool HasDefaults() const override;

private:
    void RateControlMethodChanged();
};

