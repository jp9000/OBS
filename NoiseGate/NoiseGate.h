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

class NoiseGate;

#define CONFIG_FILENAME TEXT("\\noisegate.ini")

//============================================================================
// NoiseGateFilter class

class NoiseGateFilter : public AudioFilter
{
    //-----------------------------------------------------------------------
    // Private members

private:
    NoiseGate *     parent;
    
    // State
    float   attenuation; // Current gate multiplier
    float   level;  // Input level with delayed decay
    float   heldTime; // The amount of time we've held the gate open after it we hit the close threshold
    bool    isOpen;

    //-----------------------------------------------------------------------
    // Constructor/destructor
    
public:
    NoiseGateFilter(NoiseGate *parent);
    virtual ~NoiseGateFilter();
    
    //-----------------------------------------------------------------------
    // Methods

public:
    virtual AudioSegment *Process(AudioSegment *segment);

private:
    void ApplyNoiseGate(float *buffer, int totalFloats);
};

//============================================================================
// NoiseGateConfigWindow class

class NoiseGateConfigWindow
{
    //-----------------------------------------------------------------------
    // Constants

private:
    static const int    REPAINT_TIMER_ID = 1;
    static const int    CURVOL_RESOLUTION = 96 * 4 - 1; // 0.25 dB resolution for a (-96..0) dB range

    //-----------------------------------------------------------------------
    // Private members

private:
    NoiseGate *     parent;
    HWND            parentHwnd;
    HWND            hwnd;

    //-----------------------------------------------------------------------
    // Static methods

public:
    static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    //-----------------------------------------------------------------------
    // Constructor/destructor

public:
    NoiseGateConfigWindow(NoiseGate *parent, HWND parentHwnd);
    ~NoiseGateConfigWindow();

    //-----------------------------------------------------------------------
    // Methods

public:
    bool Process();

private:
    void SetTrackbarCaption(int controlId, int db);
    void RepaintVolume();
    void RefreshConfig();

    //-----------------------------------------------------------------------
    // Message processing

private:
    void MsgInitDialog();
    INT_PTR MsgClicked(int controlId, HWND controlHwnd);
    INT_PTR MsgScroll(bool vertical, WPARAM wParam, LPARAM lParam);
    INT_PTR MsgTimer(int timerId);
};

//============================================================================
// NoiseGate class

class NoiseGate
{
    friend class NoiseGateFilter;
    friend class NoiseGateConfigWindow;

    //-----------------------------------------------------------------------
    // Static members

public:
    static HINSTANCE    hinstDLL;
    static NoiseGate *  instance;
    
    //-----------------------------------------------------------------------
    // Private members

private:
    AudioSource *       micSource;
    NoiseGateFilter *   filter;
    ConfigFile          config;
    bool                isDisabledFromConfig;

    // User configuration
    bool    isEnabled;
    float   openThreshold;
    float   closeThreshold;
    float   attackTime;
    float   holdTime;
    float   releaseTime;

    //-----------------------------------------------------------------------
    // Constructor/destructor
    
public:
    NoiseGate();
    ~NoiseGate();

    //-----------------------------------------------------------------------
    // Methods

public:
    void LoadDefaults();
    void LoadSettings();
    void SaveSettings();
    void StreamStarted();
    void StreamStopped();
    void ShowConfigDialog(HWND parentHwnd);
};

//============================================================================
// Plugin entry points

extern "C" __declspec(dllexport) bool LoadPlugin();
extern "C" __declspec(dllexport) void UnloadPlugin();
extern "C" __declspec(dllexport) void ConfigPlugin(HWND parentHwnd);
extern "C" __declspec(dllexport) void OnStartStream();
extern "C" __declspec(dllexport) void OnStopStream();
extern "C" __declspec(dllexport) CTSTR GetPluginName();
extern "C" __declspec(dllexport) CTSTR GetPluginDescription();
