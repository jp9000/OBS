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
// NoiseGate class

class NoiseGate
{
    friend class NoiseGateFilter;

    //-----------------------------------------------------------------------
    // Static members

public:
    static HINSTANCE    s_hinstDLL;
    static NoiseGate *  s_instance;
    
    //-----------------------------------------------------------------------
    // Private members

private:
    AudioSource *       micSource;
    NoiseGateFilter *   filter;

    // User configuration
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
    void StreamStarted();
    void StreamStopped();
};

//============================================================================
// Plugin entry points

extern "C" __declspec(dllexport) bool LoadPlugin();
extern "C" __declspec(dllexport) void UnloadPlugin();
extern "C" __declspec(dllexport) void OnStartStream();
extern "C" __declspec(dllexport) void OnStopStream();
extern "C" __declspec(dllexport) CTSTR GetPluginName();
extern "C" __declspec(dllexport) CTSTR GetPluginDescription();
