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

    bool bUseVideoTime;
    QWORD lastVideoTime;
    QWORD curVideoTime;

    UINT32 lastNumFramesRead;
    UINT sampleWindowSize;
    List<float> inputBuffer;

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
    err = mmEnumerator->GetDevice(lpID, &mmDevice);

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
    else
    {
        Log(TEXT("------------------------------------------"));
        Log(TEXT("Using desktop audio input: %s"), GetDeviceName());

        bUseVideoTime = AppConfig->GetInt(TEXT("Audio"), TEXT("SyncToVideoTime")) != 0;
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

    bool  bFloat;
    UINT  inputChannels;
    UINT  inputSamplesPerSec;
    UINT  inputBitsPerSample;
    UINT  inputBlockSize;
    DWORD inputChannelMask;

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

    sampleWindowSize    = (inputSamplesPerSec/100);

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

    InitAudioData(bFloat, inputChannels, inputSamplesPerSec, inputBitsPerSample, inputBlockSize, inputChannelMask);

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
    float *captureBuffer = NULL;

    QWORD qpcTimestamp;

    //all audio data should be sent out in 10ms data sizes.
    bool bNeedMoreFrames = true;
    while(bNeedMoreFrames)
    {
        UINT captureSize = 0;
        HRESULT err = mmCapture->GetNextPacketSize(&captureSize);
        if(FAILED(err))
            return false;

        if(captureSize)
        {
            DWORD dwFlags = 0;

            err = mmCapture->GetBuffer((BYTE**)&captureBuffer, &lastNumFramesRead, &dwFlags, NULL, &qpcTimestamp);
            if(FAILED(err))
            {
                RUNONCE AppWarning(TEXT("MMDeviceAudioSource::GetBuffer: GetBuffer failed"));
                return false;
            }

            qpcTimestamp /= 10000;

            if(inputBuffer.Num() == 0 && lastNumFramesRead == sampleWindowSize)
                break;

            qpcTimestamp -= inputBuffer.Num()/GetChannelCount()*1000/GetSamplesPerSec();
            inputBuffer.AppendArray(captureBuffer, lastNumFramesRead*GetChannelCount());
        }
        else
            return false;

        if(inputBuffer.Num() >= sampleWindowSize*GetChannelCount())
        {
            captureBuffer = inputBuffer.Array();
            bNeedMoreFrames = false;
        }
        else
            mmCapture->ReleaseBuffer(lastNumFramesRead);
    }

    //-----------------------------------------------------------------
    // timestamp bs.  now using video timestamps as an audio base.  kill me.
    QWORD newTimestamp = 0;

    //don't even -bother- trying to get mic or auxilary audio time from timestamps.
    //half the time, they aren't valid.  just use desktop timing and let the user deal with
    //offsetting the time.
    if(bIsMic)
        newTimestamp = App->GetAudioTime()+GetTimeOffset();
    else
    {
        //we're doing all these checks because device timestamps are only reliable "sometimes"
        if(!bFirstFrameReceived)
        {
            LARGE_INTEGER clockFreq;
            QueryPerformanceFrequency(&clockFreq);
            QWORD curTime = GetQPCTimeMS(clockFreq.QuadPart);

            newTimestamp = qpcTimestamp;

            if(bUseVideoTime || newTimestamp < (curTime-OUTPUT_BUFFER_TIME) || newTimestamp > (curTime+2000))
            {
                curVideoTime = lastVideoTime = App->GetVideoTime();

                SetTimeOffset(GetTimeOffset()-int(lastVideoTime-App->GetSceneTimestamp()));
                bUseVideoTime = true;

                newTimestamp = lastVideoTime+GetTimeOffset();
            }

            bFirstFrameReceived = true;
        }
        else
        {
            if(bUseVideoTime)
            {
                QWORD newVideoTime = App->GetVideoTime();

                if(newVideoTime != lastVideoTime)
                {
                    lastVideoTime = newVideoTime;
                    newTimestamp = curVideoTime = newVideoTime+GetTimeOffset();
                }
                else
                    newTimestamp = curVideoTime += 10;
            }
            else
                newTimestamp = qpcTimestamp+GetTimeOffset();
        }

        App->latestAudioTime = newTimestamp;
    }

    //-----------------------------------------------------------------
    //save data

    *numFrames = sampleWindowSize;
    *buffer = (void*)captureBuffer;
    *timestamp = newTimestamp;

    return true;
}

void MMDeviceAudioSource::ReleaseBuffer()
{
    if(inputBuffer.Num() != 0)
        inputBuffer.RemoveRange(0, sampleWindowSize*GetChannelCount());

    mmCapture->ReleaseBuffer(lastNumFramesRead);
}
