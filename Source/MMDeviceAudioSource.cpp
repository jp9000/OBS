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


#include "Main.h"

#include <Mmdeviceapi.h>
#include <Audioclient.h>
#include <propsys.h>
#include <Functiondiscoverykeys_devpkey.h>


class MMDeviceAudioSource : public AudioSource
{
    IMMDeviceEnumerator *mmEnumerator;

    IMMDevice           *mmDevice;
    IAudioClient        *mmClient;
    IAudioCaptureClient *mmCapture;
    IAudioClock         *mmClock;

    bool bIsMic;
    bool bFirstFrameReceived;

    UINT32 numFramesRead;

    String strDeviceName;

protected:
    virtual bool GetNextBuffer(void **buffer, UINT *numFrames, QWORD *timestamp);
    virtual void ReleaseBuffer();

    virtual CTSTR GetDeviceName() const {return strDeviceName.Array();}

public:
    bool Initialize(bool bMic, CTSTR lpID);

    ~MMDeviceAudioSource()
    {
        StopCapture();

        SafeRelease(mmCapture);
        SafeRelease(mmClient);
        SafeRelease(mmDevice);
        SafeRelease(mmEnumerator);
        SafeRelease(mmClock);
    }

    virtual void StartCapture();
    virtual void StopCapture();
};

AudioSource* CreateAudioSource(bool bMic, CTSTR lpID)
{
    MMDeviceAudioSource *source = new MMDeviceAudioSource;
    if(source->Initialize(bMic, lpID))
        return source;
    else
    {
        delete source;
        return NULL;
    }
}

//==============================================================================================================================

bool MMDeviceAudioSource::Initialize(bool bMic, CTSTR lpID)
{
    const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
    const IID IID_IMMDeviceEnumerator    = __uuidof(IMMDeviceEnumerator);
    const IID IID_IAudioClient           = __uuidof(IAudioClient);
    const IID IID_IAudioCaptureClient    = __uuidof(IAudioCaptureClient);

    HRESULT err;
    err = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&mmEnumerator);
    if(FAILED(err))
    {
        AppWarning(TEXT("MMDeviceAudioSource::Initialize(%d): Could not create IMMDeviceEnumerator = %08lX"), (BOOL)bMic, err);
        return false;
    }

    bIsMic = bMic;

    if(bMic)
        err = mmEnumerator->GetDevice(lpID, &mmDevice);
    else
        err = mmEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &mmDevice);

    if(FAILED(err))
    {
        AppWarning(TEXT("MMDeviceAudioSource::Initialize(%d): Could not create IMMDevice = %08lX"), (BOOL)bMic, err);
        return false;
    }

    err = mmDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&mmClient);
    if(FAILED(err))
    {
        AppWarning(TEXT("MMDeviceAudioSource::Initialize(%d): Could not create IAudioClient = %08lX"), (BOOL)bMic, err);
        return false;
    }

    //-----------------------------------------------------------------
    // get name

    IPropertyStore *store;
    if(SUCCEEDED(mmDevice->OpenPropertyStore(STGM_READ, &store)))
    {
        PROPVARIANT varName;

        PropVariantInit(&varName);
        if(SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &varName)))
        {
            CWSTR wstrName = varName.pwszVal;
            strDeviceName = wstrName;
        }

        store->Release();
    }

    if(bMic)
    {
        Log(TEXT("------------------------------------------"));
        Log(TEXT("Using auxilary audio input: %s"), GetDeviceName());
    }

    //-----------------------------------------------------------------
    // get format

    WAVEFORMATEX *pwfx;
    err = mmClient->GetMixFormat(&pwfx);
    if(FAILED(err))
    {
        AppWarning(TEXT("MMDeviceAudioSource::Initialize(%d): Could not get mix format from audio client = %08lX"), (BOOL)bMic, err);
        return false;
    }

    //the internal audio engine should always use floats (or so I read), but I suppose just to be safe better check
    if(pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
    {
        WAVEFORMATEXTENSIBLE *wfext = (WAVEFORMATEXTENSIBLE*)pwfx;
        inputChannelMask = wfext->dwChannelMask;

        if(wfext->SubFormat != KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
        {
            AppWarning(TEXT("MMDeviceAudioSource::Initialize(%d): Unsupported wave format"), (BOOL)bMic);
            return false;
        }
    }
    else if(pwfx->wFormatTag != WAVE_FORMAT_IEEE_FLOAT)
    {
        AppWarning(TEXT("MMDeviceAudioSource::Initialize(%d): Unsupported wave format"), (BOOL)bMic);
        return false;
    }

    bFloat              = true;
    inputChannels       = pwfx->nChannels;
    inputBitsPerSample  = 32;
    inputBlockSize      = pwfx->nBlockAlign;
    inputSamplesPerSec  = pwfx->nSamplesPerSec;

    DWORD flags = bMic ? 0 : AUDCLNT_STREAMFLAGS_LOOPBACK;
    err = mmClient->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, ConvertMSTo100NanoSec(5000), 0, pwfx, NULL);
    if(FAILED(err))
    {
        AppWarning(TEXT("MMDeviceAudioSource::Initialize(%d): Could not initialize audio client, result = %08lX"), (BOOL)bMic, err);
        return false;
    }

    //-----------------------------------------------------------------
    // acquire services

    err = mmClient->GetService(IID_IAudioCaptureClient, (void**)&mmCapture);
    if(FAILED(err))
    {
        AppWarning(TEXT("MMDeviceAudioSource::Initialize(%d): Could not get audio capture client, result = %08lX"), (BOOL)bMic, err);
        return false;
    }

    err = mmClient->GetService(__uuidof(IAudioClock), (void**)&mmClock);
    if(FAILED(err))
    {
        AppWarning(TEXT("MMDeviceAudioSource::Initialize(%d): Could not get audio capture clock, result = %08lX"), (BOOL)bMic, err);
        return false;
    }

    CoTaskMemFree(pwfx);

    //-----------------------------------------------------------------

    InitAudioData();

    return true;
}

void MMDeviceAudioSource::StartCapture()
{
    if(mmClient)
        mmClient->Start();
}

void MMDeviceAudioSource::StopCapture()
{
    if(mmClient)
        mmClient->Stop();
}

bool MMDeviceAudioSource::GetNextBuffer(void **buffer, UINT *numFrames, QWORD *timestamp)
{
    UINT captureSize = 0;
    HRESULT err = mmCapture->GetNextPacketSize(&captureSize);
    if(FAILED(err))
        return false;

    numFramesRead = 0;

    if(captureSize)
    {
        LPBYTE captureBuffer;
        DWORD dwFlags = 0;

        UINT64 devPosition;
        UINT64 qpcTimestamp;
        err = mmCapture->GetBuffer(&captureBuffer, &numFramesRead, &dwFlags, &devPosition, &qpcTimestamp);
        if(FAILED(err))
        {
            RUNONCE AppWarning(TEXT("MMDeviceAudioSource::GetBuffer: GetBuffer failed"));
            return false;
        }

        //-----------------------------------------------------------------
        // timestamp bs

        QWORD newTimestamp = 0;

        if(bIsMic)
        {
            newTimestamp = App->GetAudioTime();
            //Log(TEXT("newTimestamp: %llu"), newTimestamp);
        }
        else
        {
            if(dwFlags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)
            {
                RUNONCE AppWarning(TEXT("MMDeviceAudioSource::GetBuffer: woa woa woa, getting timestamp errors from the audio subsystem.  device = %s"), GetDeviceName());
                if(!bCalculateTimestamp)
                    newTimestamp = lastUsedTimestamp + numFramesRead*1000/inputSamplesPerSec;
            }
            else
            {
                if(!bCalculateTimestamp)
                    newTimestamp = qpcTimestamp/10000;

                /*if(bIsMic)
                {
                    UINT64 freq;
                    mmClock->GetFrequency(&freq);

                    QWORD micTime = devPosition*8000/freq;

                    static QWORD startTimestamp = newTimestamp;
                    static QWORD startTime = micTime;

                    LONGLONG timestampOffset = LONGLONG(newTimestamp-startTimestamp);
                    LONGLONG micTimeOffset = LONGLONG(micTime-startTime);

                    Log(TEXT("position: %llu, numAudioFrames: %u, freq: %llu, newTimestamp: %llu, micTime: %llu, timestampOffset: %llu, micTimeOffset: %llu, diff: %lld"),
                        devPosition, numAudioFrames, freq, newTimestamp, micTime, timestampOffset, micTimeOffset, micTimeOffset-timestampOffset);
                }*/
            }

            //have to do this crap to account for broken devices or device drivers.  absolutely unbelievable.
            if(!bFirstFrameReceived)
            {
                LARGE_INTEGER clockFreq;
                QueryPerformanceFrequency(&clockFreq);
                QWORD curTime = GetQPCTimeMS(clockFreq.QuadPart);

                if(newTimestamp < (curTime-OUTPUT_BUFFER_TIME) || newTimestamp > (curTime+OUTPUT_BUFFER_TIME))
                {
                    bCalculateTimestamp = true;

                    Log(TEXT("MMDeviceAudioSource::GetNextBuffer: Got bad audio timestamp offset %lld from device: '%s', timestamps for this device will be calculated.  curTime: %llu, newTimestamp: %llu"), (LONGLONG)(newTimestamp - curTime), GetDeviceName(), curTime, newTimestamp);
                    lastUsedTimestamp = newTimestamp = curTime;
                }
                else
                    lastUsedTimestamp = newTimestamp;

                bFirstFrameReceived = true;
            }

            //desktop audio is best audio!  use it for the base audio
            App->latestAudioTime = newTimestamp;
        }

        //-----------------------------------------------------------------
        //save data

        *numFrames = numFramesRead;
        *buffer = (void*)captureBuffer;
        *timestamp = newTimestamp;

        return true;
    }

    return false;
}

void MMDeviceAudioSource::ReleaseBuffer()
{
    mmCapture->ReleaseBuffer(numFramesRead);
}