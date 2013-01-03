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


#include "DShowPlugin.h"

#include <Audioclient.h>


bool DeviceAudioSource::GetNextBuffer(void **buffer, UINT *numFrames, QWORD *timestamp)
{
    if(sampleBuffer.Num() >= sampleSegmentSize)
    {
        OSEnterMutex(hAudioMutex);

        mcpy(outputBuffer.Array(), sampleBuffer.Array(), sampleSegmentSize);
        sampleBuffer.RemoveRange(0, sampleSegmentSize);

        OSLeaveMutex(hAudioMutex);

        *buffer = outputBuffer.Array();
        *numFrames = sampleFrameCount;
        *timestamp = API->GetAudioTime()+timeOffset;

        return true;
    }

    return false;
}

void DeviceAudioSource::ReleaseBuffer()
{
}


CTSTR DeviceAudioSource::GetDeviceName() const
{
    if(device)
        return device->strDeviceName.Array();

    return NULL;
}


bool DeviceAudioSource::Initialize(DeviceSource *parent)
{
    device = parent;
    hAudioMutex = OSCreateMutex();

    //---------------------------------

    if(device->audioFormat.wFormatTag == WAVE_FORMAT_EXTENSIBLE)
    {
        WAVEFORMATEXTENSIBLE *wfext = (WAVEFORMATEXTENSIBLE*)&device->audioFormat;
        if(wfext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
            bFloat = true;
    }
    else if(device->audioFormat.wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
        bFloat = true;

    inputBitsPerSample = device->audioFormat.wBitsPerSample;
    inputBlockSize     = device->audioFormat.nBlockAlign;
    inputChannelMask   = 0;
    inputChannels      = device->audioFormat.nChannels;
    inputSamplesPerSec = device->audioFormat.nSamplesPerSec;

    sampleFrameCount   = inputSamplesPerSec/100;
    sampleSegmentSize  = inputBlockSize*sampleFrameCount;

    outputBuffer.SetSize(sampleSegmentSize);

    InitAudioData();

    return true;
}

DeviceAudioSource::~DeviceAudioSource()
{
    if(hAudioMutex)
        OSCloseMutex(hAudioMutex);
}


void DeviceAudioSource::ReceiveAudio(IMediaSample *sample)
{
    if(sample)
    {
        BYTE *pData;
        if(SUCCEEDED(sample->GetPointer(&pData)))
        {
            OSEnterMutex(hAudioMutex);
            sampleBuffer.AppendArray(pData, sample->GetActualDataLength());
            OSLeaveMutex(hAudioMutex);
        }
    }
}

