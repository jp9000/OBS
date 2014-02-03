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

    bool deviceLost;
    QWORD reinitTimer;

    //UINT32 numFramesRead;

    UINT32 numTimesInARowNewDataSeen;

    String deviceId;
    String strDeviceName;

    bool bUseVideoTime;
    QWORD lastVideoTime;
    QWORD curVideoTime;

    UINT sampleWindowSize;
    List<float> inputBuffer;
    List<float> convertBuffer;
    UINT inputBufferSize;
    QWORD firstTimestamp;
    QWORD lastQPCTimestamp;

    UINT32 angerThreshold;

    bool bUseQPC;

    QWORD GetTimestamp(QWORD qpcTimestamp);

    bool Reinitialize();

    void FreeData()
    {
        SafeRelease(mmCapture);
        SafeRelease(mmClient);
        SafeRelease(mmDevice);
        SafeRelease(mmClock);
    }

protected:
    virtual bool GetNextBuffer(void **buffer, UINT *numFrames, QWORD *timestamp);
    virtual void ReleaseBuffer();

    virtual CTSTR GetDeviceName() const {return strDeviceName.Array();}

public:
    bool Initialize(bool bMic, CTSTR lpID);

    ~MMDeviceAudioSource()
    {
        StopCapture();
        FreeData();
        SafeRelease(mmEnumerator);
    }

    void Reset()
    {
        Log(L"User purposely reset the device '%s'.  Did it go out, or were there audio issues that made the user want to do this?", GetDeviceName());
        deviceLost = true;
        reinitTimer = GetQPCTimeMS();
        FreeData();
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

bool MMDeviceAudioSource::Reinitialize()
{
    const IID IID_IAudioClient           = __uuidof(IAudioClient);
    const IID IID_IAudioCaptureClient    = __uuidof(IAudioCaptureClient);
    HRESULT err;

    bool useInputDevice = bIsMic || AppConfig->GetInt(L"Audio", L"UseInputDevices", false) != 0;

    if (bIsMic) {
        BOOL bMicSyncFixHack = GlobalConfig->GetInt(TEXT("Audio"), TEXT("UseMicSyncFixHack"));
        angerThreshold = bMicSyncFixHack ? 40 : 1000;
    }

    if (scmpi(deviceId, TEXT("Default")) == 0)
        err = mmEnumerator->GetDefaultAudioEndpoint(useInputDevice ? eCapture : eRender, useInputDevice ? eCommunications : eConsole, &mmDevice);
    else
        err = mmEnumerator->GetDevice(deviceId, &mmDevice);

    if(FAILED(err))
    {
        if (!deviceLost) AppWarning(TEXT("MMDeviceAudioSource::Initialize(%d): Could not create IMMDevice = %08lX"), (BOOL)bIsMic, err);
        return false;
    }

    err = mmDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&mmClient);
    if(FAILED(err))
    {
        if (!deviceLost) AppWarning(TEXT("MMDeviceAudioSource::Initialize(%d): Could not create IAudioClient = %08lX"), (BOOL)bIsMic, err);
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

    if(bIsMic)
    {
        if (!deviceLost) {
            Log(TEXT("------------------------------------------"));
            Log(TEXT("Using auxilary audio input: %s"), GetDeviceName());
        }

        bUseQPC = GlobalConfig->GetInt(TEXT("Audio"), TEXT("UseMicQPC")) != 0;
        if (bUseQPC)
            Log(TEXT("Using Mic QPC timestamps"));
    }
    else
    {
        if (!deviceLost) {
            Log(TEXT("------------------------------------------"));
            Log(TEXT("Using desktop audio input: %s"), GetDeviceName());
        }

        bUseVideoTime = AppConfig->GetInt(TEXT("Audio"), TEXT("SyncToVideoTime")) != 0;
        SetTimeOffset(GlobalConfig->GetInt(TEXT("Audio"), TEXT("GlobalAudioTimeAdjust")));
    }

    //-----------------------------------------------------------------
    // get format

    WAVEFORMATEX *pwfx;
    err = mmClient->GetMixFormat(&pwfx);
    if(FAILED(err))
    {
        if (!deviceLost) AppWarning(TEXT("MMDeviceAudioSource::Initialize(%d): Could not get mix format from audio client = %08lX"), (BOOL)bIsMic, err);
        return false;
    }

    bool  bFloat;
    UINT  inputChannels;
    UINT  inputSamplesPerSec;
    UINT  inputBitsPerSample;
    UINT  inputBlockSize;
    DWORD inputChannelMask = 0;
    WAVEFORMATEXTENSIBLE *wfext = NULL;

    //the internal audio engine should always use floats (or so I read), but I suppose just to be safe better check
    if(pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
    {
        wfext = (WAVEFORMATEXTENSIBLE*)pwfx;
        inputChannelMask = wfext->dwChannelMask;

        if(wfext->SubFormat != KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
        {
            if (!deviceLost) AppWarning(TEXT("MMDeviceAudioSource::Initialize(%d): Unsupported wave format"), (BOOL)bIsMic);
            CoTaskMemFree(pwfx);
            return false;
        }
    }
    else if(pwfx->wFormatTag != WAVE_FORMAT_IEEE_FLOAT)
    {
        if (!deviceLost) AppWarning(TEXT("MMDeviceAudioSource::Initialize(%d): Unsupported wave format"), (BOOL)bIsMic);
        CoTaskMemFree(pwfx);
        return false;
    }

    bFloat                = true;
    inputChannels         = pwfx->nChannels;
    inputBitsPerSample    = 32;
    inputBlockSize        = pwfx->nBlockAlign;
    inputSamplesPerSec    = pwfx->nSamplesPerSec;
    sampleWindowSize      = (inputSamplesPerSec/100);

    DWORD flags = useInputDevice ? 0 : AUDCLNT_STREAMFLAGS_LOOPBACK;

    err = mmClient->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, ConvertMSTo100NanoSec(5000), 0, pwfx, NULL);
    //err = AUDCLNT_E_UNSUPPORTED_FORMAT;

    if(FAILED(err))
    {
        if (!deviceLost) AppWarning(TEXT("MMDeviceAudioSource::Initialize(%d): Could not initialize audio client, result = %08lX"), (BOOL)bIsMic, err);
        CoTaskMemFree(pwfx);
        return false;
    }

    //-----------------------------------------------------------------
    // acquire services

    err = mmClient->GetService(IID_IAudioCaptureClient, (void**)&mmCapture);
    if(FAILED(err))
    {
        if (!deviceLost) AppWarning(TEXT("MMDeviceAudioSource::Initialize(%d): Could not get audio capture client, result = %08lX"), (BOOL)bIsMic, err);
        CoTaskMemFree(pwfx);
        return false;
    }

    err = mmClient->GetService(__uuidof(IAudioClock), (void**)&mmClock);
    if(FAILED(err))
    {
        if (!deviceLost) AppWarning(TEXT("MMDeviceAudioSource::Initialize(%d): Could not get audio capture clock, result = %08lX"), (BOOL)bIsMic, err);
        CoTaskMemFree(pwfx);
        return false;
    }

    CoTaskMemFree(pwfx);

    //-----------------------------------------------------------------

    InitAudioData(bFloat, inputChannels, inputSamplesPerSec, inputBitsPerSample, inputBlockSize, inputChannelMask);

    deviceLost = false;

    return true;
}

bool MMDeviceAudioSource::Initialize(bool bMic, CTSTR lpID)
{
    const IID IID_IMMDeviceEnumerator    = __uuidof(IMMDeviceEnumerator);
    const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);

    bIsMic = bMic;
    deviceId = lpID;

    HRESULT err = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&mmEnumerator);
    if(FAILED(err))
    {
        AppWarning(TEXT("MMDeviceAudioSource::Initialize(%d): Could not create IMMDeviceEnumerator = %08lX"), (BOOL)bIsMic, err);
        return false;
    }

    return Reinitialize();
}

void MMDeviceAudioSource::StartCapture()
{
    if(mmClient) {
        mmClient->Start();

        UINT64 freq;
        mmClock->GetFrequency(&freq);

        //Log(TEXT("MMDeviceAudioSource: Frequency for device '%s' is %llu, samples per sec is %u"), GetDeviceName(), freq, this->GetSamplesPerSec());
    }
}

void MMDeviceAudioSource::StopCapture()
{
    if(mmClient)
        mmClient->Stop();
}

QWORD MMDeviceAudioSource::GetTimestamp(QWORD qpcTimestamp)
{
    QWORD newTimestamp;

    if(bIsMic)
    {
        newTimestamp = (bUseQPC) ? qpcTimestamp : App->GetAudioTime();
        newTimestamp += GetTimeOffset();

        //Log(TEXT("got some mic audio, timestamp: %llu"), newTimestamp);

        return newTimestamp;
    }
    else
    {
        //we're doing all these checks because device timestamps are only reliable "sometimes"
        if(!bFirstFrameReceived)
        {
            QWORD curTime = GetQPCTimeMS();

            newTimestamp = qpcTimestamp;

            curVideoTime = lastVideoTime = App->GetVideoTime();

            if(bUseVideoTime || newTimestamp < (curTime-App->bufferingTime) || newTimestamp > (curTime+2000))
            {
                if(!bUseVideoTime)
                    Log(TEXT("Bad timestamp detected, syncing audio to video time"));
                else
                    Log(TEXT("Syncing audio to video time (WARNING: you should not be doing this if you are just having webcam desync, that's a separate issue)"));

                SetTimeOffset(GetTimeOffset()-int(lastVideoTime-App->GetSceneTimestamp()));
                bUseVideoTime = true;

                newTimestamp = lastVideoTime+GetTimeOffset();
            }

            bFirstFrameReceived = true;
        }
        else
        {
            QWORD newVideoTime = App->GetVideoTime();

            if(newVideoTime != lastVideoTime)
                curVideoTime = lastVideoTime = newVideoTime;
            else
                curVideoTime += 10;

            newTimestamp = (bUseVideoTime) ? curVideoTime : qpcTimestamp;
            newTimestamp += GetTimeOffset();
        }

        //Log(TEXT("qpc timestamp: %llu, lastUsed: %llu, dif: %llu"), newTimestamp, lastUsedTimestamp, difVal);

        return newTimestamp;
    }
}

bool MMDeviceAudioSource::GetNextBuffer(void **buffer, UINT *numFrames, QWORD *timestamp)
{
    UINT captureSize = 0;
    bool bFirstRun = true;
    HRESULT hRes;
    UINT64 devPosition, qpcTimestamp;
    LPBYTE captureBuffer;
    UINT32 numFramesRead;
    DWORD dwFlags = 0;

    if (deviceLost) {
        QWORD timeVal = GetQPCTimeMS();
        QWORD timer = (timeVal - reinitTimer);
        if (timer > 1000) {
            if (Reinitialize()) {
                Log(L"Device '%s' reacquired.", strDeviceName.Array());
                StartCapture();
            }
            reinitTimer = timeVal;
        }

        return false;
    }

    while (true) {
        if (inputBufferSize >= sampleWindowSize*GetChannelCount()) {
            if (bFirstRun)
                lastQPCTimestamp += 10;
            firstTimestamp = GetTimestamp(lastQPCTimestamp);
            break;
        }

        //---------------------------------------------------------

        hRes = mmCapture->GetNextPacketSize(&captureSize);

        if (FAILED(hRes)) {
            if (hRes == AUDCLNT_E_DEVICE_INVALIDATED) {
                FreeData();
                deviceLost = true;
                Log(L"Audio device '%s' has been lost, attempting to reinitialize", strDeviceName.Array());
                reinitTimer = GetQPCTimeMS();
                return false;
            }

            RUNONCE AppWarning(TEXT("MMDeviceAudioSource::GetBuffer: GetNextPacketSize failed, result = %08lX"), hRes);
            return false;
        }

        if (!captureSize)
            return false;

        //---------------------------------------------------------

        hRes = mmCapture->GetBuffer(&captureBuffer, &numFramesRead, &dwFlags, &devPosition, &qpcTimestamp);

        if (FAILED(hRes)) {
            RUNONCE AppWarning(TEXT("MMDeviceAudioSource::GetBuffer: GetBuffer failed, result = %08lX"), hRes);
            return false;
        }

        UINT totalFloatsRead = numFramesRead*GetChannelCount();

        if (inputBufferSize) {
            double timeAdjust = double(inputBufferSize/GetChannelCount());
            timeAdjust /= (double(GetSamplesPerSec())*0.0000001);

            qpcTimestamp -= UINT64(timeAdjust);
        }

        qpcTimestamp /= 10000;
        lastQPCTimestamp = qpcTimestamp;

        //---------------------------------------------------------

        UINT newInputBufferSize = inputBufferSize + totalFloatsRead;
        if (newInputBufferSize > inputBuffer.Num())
            inputBuffer.SetSize(newInputBufferSize);

        mcpy(inputBuffer.Array()+inputBufferSize, captureBuffer, totalFloatsRead*sizeof(float));
        inputBufferSize = newInputBufferSize;

        mmCapture->ReleaseBuffer(numFramesRead);

        bFirstRun = false;
    }

    *numFrames = sampleWindowSize;
    *buffer = (void*)inputBuffer.Array();
    *timestamp = firstTimestamp;

    /*if (bIsMic) {
        static QWORD lastTimestamp = 0;
        if (firstTimestamp != lastTimestamp+10)
            Log(TEXT("A: %llu, difference: %llu"), firstTimestamp, firstTimestamp-lastTimestamp);

        lastTimestamp = firstTimestamp;
    }*/

    return true;
}

void MMDeviceAudioSource::ReleaseBuffer()
{
    UINT sampleSizeFloats = sampleWindowSize*GetChannelCount();
    if (inputBufferSize > sampleSizeFloats)
        mcpy(inputBuffer.Array(), inputBuffer.Array()+sampleSizeFloats, (inputBufferSize-sampleSizeFloats)*sizeof(float));

    inputBufferSize -= sampleSizeFloats;
}

void ResetWASAPIAudioDevice(AudioSource *source)
{
    static_cast<MMDeviceAudioSource*>(source)->Reset();
}
