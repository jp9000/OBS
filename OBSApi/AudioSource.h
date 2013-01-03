/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

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


enum
{
    NoAudioAvailable,
    AudioAvailable,
};

struct AudioSegment
{
    List<float> audioData;
    QWORD timestamp;

    inline void ClearData()
    {
        audioData.Clear();
    }
};

class BASE_EXPORT AudioSource
{
    bool bResample;
    LPVOID resampler;
    double resampleRatio;

    //-----------------------------------------

    List<AudioSegment> audioSegments;

    bool bFirstBaseFrameReceived;
    QWORD lastSentTimestamp;

    //-----------------------------------------

    List<float> storageBuffer;

    //-----------------------------------------

    List<float> outputBuffer;
    List<float> convertBuffer;
    List<float> tempBuffer;
    List<float> tempResampleBuffer;

protected:

    int timeOffset;

    //-----------------------------------------

    bool bFloat;
    UINT inputChannels;
    UINT inputSamplesPerSec;
    UINT inputBitsPerSample;
    UINT inputBlockSize;
    DWORD inputChannelMask;

    QWORD lastUsedTimestamp;

    void InitAudioData();

    //-----------------------------------------

    virtual CTSTR GetDeviceName() const=0;

    //-----------------------------------------

    virtual bool GetNextBuffer(void **buffer, UINT *numFrames, QWORD *timestamp)=0;
    virtual void ReleaseBuffer()=0;

public:

    //-----------------------------------------

    virtual ~AudioSource();

    virtual UINT QueryAudio(float curVolume);
    virtual bool GetEarliestTimestamp(QWORD &timestamp);
    virtual bool GetBuffer(float **buffer, UINT *numFrames, QWORD targetTimestamp);

    virtual bool GetNewestFrame(float **buffer, UINT *numFrames);

    virtual QWORD GetBufferedTime();

    virtual void StartCapture() {}
    virtual void StopCapture() {}

    inline void SetTimeOffset(int newOffset) {timeOffset = newOffset;}
};

