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
    : m_parent(parent)
    , m_attenuation(0.0f)
    , m_level(0.0f)
    , m_heldTime(0.0f)
    , m_isOpen(false)
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
    const float attackRate = 1.0f / (m_parent->m_attackTime * SAMPLE_RATE_F);
    const float releaseRate = 1.0f / (m_parent->m_releaseTime * SAMPLE_RATE_F);

    // Determine level decay rate. We don't want human voice (75-300Hz) to cross the close
    // threshold if the previous peak crosses the open threshold. 75Hz at 44.1ksps is ~590
    // samples between peaks.
    const float thresholdDiff = m_parent->m_openThreshold - m_parent->m_closeThreshold;
    const float minDecayPeriod = (1.0f / 75.0f) * SAMPLE_RATE_F; // ~590 samples
    const float decayRate = thresholdDiff / minDecayPeriod;

    // We can't use SSE as the processing of each sample depends on the processed
    // result of the previous sample.
    for(int i = 0; i < totalFloats; i += 2)
    {
        // Get current input level
        float curLvl = abs(buffer[i] + buffer[i+1]) * 0.5f;

        // Test thresholds
        if(curLvl > m_parent->m_openThreshold && !m_isOpen)
            m_isOpen = true;
        if(m_level < m_parent->m_closeThreshold && m_isOpen)
        {
            m_heldTime = 0.0f;
            m_isOpen = false;
        }

        // Decay level slowly so human voice (75-300Hz) doesn't cross the close threshold
        // (Essentially a peak detector with very fast decay)
        m_level = max(m_level, curLvl) - decayRate;

        // Apply gate state to attenuation
        if(m_isOpen)
            m_attenuation = min(1.0f, m_attenuation + attackRate);
        else
        {
            m_heldTime += dtPerSample;
            if(m_heldTime > m_parent->m_holdTime)
                m_attenuation = max(0.0f, m_attenuation - releaseRate);
        }

        // Multiple input by gate multiplier (0.0f if fully closed, 1.0f if fully open)
        buffer[i] *= m_attenuation;
        buffer[i+1] *= m_attenuation;
    }
}

//============================================================================
// NoiseGate class

// Statics
HINSTANCE NoiseGate::s_hinstDLL = NULL;
NoiseGate *NoiseGate::s_instance = NULL;

NoiseGate::NoiseGate()
    : m_micSource(NULL)
    , m_filter(NULL)
    , m_openThreshold(0.05f) // FIXME: Configurable
    , m_closeThreshold(0.005f)
    , m_attackTime(0.1f)
    , m_holdTime(0.2f)
    , m_releaseTime(0.2f)
{
}

NoiseGate::~NoiseGate()
{
    // Delete our filter if it exists
    StreamStopped();
}

void NoiseGate::StreamStarted()
{
    m_micSource = OBSGetMicAudioSource();
    if(m_micSource == NULL)
        return; // No microphone
    m_filter = new NoiseGateFilter(this);
    m_micSource->AddAudioFilter(m_filter);
}

void NoiseGate::StreamStopped()
{
    if(m_filter) {
        m_micSource->RemoveAudioFilter(m_filter);
        delete m_filter;
        m_filter = NULL;
    }
    m_micSource = NULL;
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
