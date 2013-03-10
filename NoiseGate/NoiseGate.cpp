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

#include "NoiseGate.h"

//============================================================================
// NoiseGateFilter class

NoiseGateFilter::NoiseGateFilter(NoiseGate *parent)
    : parent(parent)
    , attenuation(0.0f)
    , level(0.0f)
    , heldTime(0.0f)
    , isOpen(false)
{
}

NoiseGateFilter::~NoiseGateFilter()
{
}

AudioSegment *NoiseGateFilter::Process(AudioSegment *segment)
{
    ApplyNoiseGate(segment->audioData.Array(), segment->audioData.Num());
    return segment;
}

void NoiseGateFilter::ApplyNoiseGate(float *buffer, int totalFloats)
{
    // We assume stereo input
    if(totalFloats % 2)
        return; // Odd number of samples

    // OBS is currently hard-coded to 44.1ksps
    const float SAMPLE_RATE_F = 44100.0f;
    const float dtPerSample = 1.0f / SAMPLE_RATE_F;

    // Convert configuration times into per-sample amounts
    const float attackRate = 1.0f / (parent->attackTime * SAMPLE_RATE_F);
    const float releaseRate = 1.0f / (parent->releaseTime * SAMPLE_RATE_F);

    // Determine level decay rate. We don't want human voice (75-300Hz) to cross the close
    // threshold if the previous peak crosses the open threshold. 75Hz at 44.1ksps is ~590
    // samples between peaks.
    const float thresholdDiff = parent->openThreshold - parent->closeThreshold;
    const float minDecayPeriod = (1.0f / 75.0f) * SAMPLE_RATE_F; // ~590 samples
    const float decayRate = thresholdDiff / minDecayPeriod;

    // We can't use SSE as the processing of each sample depends on the processed
    // result of the previous sample.
    for(int i = 0; i < totalFloats; i += 2)
    {
        // Get current input level
        float curLvl = abs(buffer[i] + buffer[i+1]) * 0.5f;

        // Test thresholds
        if(curLvl > parent->openThreshold && !isOpen)
            isOpen = true;
        if(level < parent->closeThreshold && isOpen)
        {
            heldTime = 0.0f;
            isOpen = false;
        }

        // Decay level slowly so human voice (75-300Hz) doesn't cross the close threshold
        // (Essentially a peak detector with very fast decay)
        level = max(level, curLvl) - decayRate;

        // Apply gate state to attenuation
        if(isOpen)
            attenuation = min(1.0f, attenuation + attackRate);
        else
        {
            heldTime += dtPerSample;
            if(heldTime > parent->holdTime)
                attenuation = max(0.0f, attenuation - releaseRate);
        }

        // Multiple input by gate multiplier (0.0f if fully closed, 1.0f if fully open)
        buffer[i] *= attenuation;
        buffer[i+1] *= attenuation;
    }
}

//============================================================================
// NoiseGate class

// Statics
HINSTANCE NoiseGate::s_hinstDLL = NULL;
NoiseGate *NoiseGate::s_instance = NULL;

NoiseGate::NoiseGate()
    : micSource(NULL)
    , filter(NULL)
    , openThreshold(0.05f) // FIXME: Configurable
    , closeThreshold(0.005f)
    , attackTime(0.1f)
    , holdTime(0.2f)
    , releaseTime(0.2f)
{
}

NoiseGate::~NoiseGate()
{
    // Delete our filter if it exists
    StreamStopped();
}

void NoiseGate::StreamStarted()
{
    micSource = OBSGetMicAudioSource();
    if(micSource == NULL)
        return; // No microphone
    filter = new NoiseGateFilter(this);
    micSource->AddAudioFilter(filter);
}

void NoiseGate::StreamStopped()
{
    if(filter) {
        micSource->RemoveAudioFilter(filter);
        delete filter;
        filter = NULL;
    }
    micSource = NULL;
}

//============================================================================
// Plugin entry points

bool LoadPlugin()
{
    if(NoiseGate::s_instance != NULL)
        return false;
    NoiseGate::s_instance = new NoiseGate();
    return true;
}

void UnloadPlugin()
{
    if(NoiseGate::s_instance == NULL)
        return;
    delete NoiseGate::s_instance;
    NoiseGate::s_instance = NULL;
}

void OnStartStream()
{
    if(NoiseGate::s_instance == NULL)
        return;
    NoiseGate::s_instance->StreamStarted();
}

void OnStopStream()
{
    if(NoiseGate::s_instance == NULL)
        return;
    NoiseGate::s_instance->StreamStopped();
}

CTSTR GetPluginName()
{
    return Str("Plugins.NoiseGate.PluginName");
}

CTSTR GetPluginDescription()
{
    return Str("Plugins.NoiseGate.PluginDescription");
}

BOOL CALLBACK DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if(fdwReason == DLL_PROCESS_ATTACH)
        NoiseGate::s_hinstDLL = hinstDLL;
    return TRUE;
}
