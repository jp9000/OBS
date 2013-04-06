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

    //UINT32 numFramesRead;

    String strDeviceName;

    bool bUseVideoTime;
    QWORD lastVideoTime;
    QWORD curVideoTime;

    UINT sampleWindowSize;
    List<float> inputBuffer;
    UINT inputBufferSize;
    QWORD firstTimestamp;

    bool bUseQPC;

    QWORD GetTimestamp(QWORD qpcTimestamp);

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

    if (scmpi(lpID, TEXT("Default")) == 0)
        err = mmEnumerator->GetDefaultAudioEndpoint(bMic ? eCapture : eRender, bMic ? eCommunications : eConsole, &mmDevice);
    else
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

        bUseQPC = GlobalConfig->GetInt(TEXT("Audio"), TEXT("UseMicQPC")) != 0;
    }
    else
    {
        Log(TEXT("------------------------------------------"));
        Log(TEXT("Using desktop audio input: %s"), GetDeviceName());

        bUseVideoTime = AppConfig->GetInt(TEXT("Audio"), TEXT("SyncToVideoTime")) != 0;
        //SetTimeOffset(GlobalConfig->GetInt(TEXT("Audio"), TEXT("GlobalAudioTimeAdjust")));
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

    InitAudioData(bFloat, inputChannels, inputSamplesPerSec, inputBitsPerSample, inputBlockSize, inputChannelMask, !bMic);

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

QWORD MMDeviceAudioSource::GetTimestamp(QWORD qpcTimestamp)
{
    QWORD newTimestamp;

    if(bIsMic)
    {
        newTimestamp = (bUseQPC) ? qpcTimestamp : App->GetAudioTime();
        newTimestamp += GetTimeOffset();
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
                    Log(TEXT("Syncing audio to video time"));

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

        App->latestAudioTime = newTimestamp;
    }

    return newTimestamp;
}

bool MMDeviceAudioSource::GetNextBuffer(void **buffer, UINT *numFrames, QWORD *timestamp)
{
    UINT captureSize = 0;
    HRESULT hRes;
    
    while (true) {
        if (inputBufferSize >= sampleWindowSize*GetChannelCount())
            break;

        //---------------------------------------------------------

        hRes = mmCapture->GetNextPacketSize(&captureSize);

        if (FAILED(hRes)) {
            RUNONCE AppWarning(TEXT("MMDeviceAudioSource::GetBuffer: GetNextPacketSize failed, result = %08lX"), hRes);
            return false;
        }

        if (!captureSize)
            return false;

        //---------------------------------------------------------

        LPBYTE captureBuffer;
        UINT32 numFramesRead;
        DWORD dwFlags = 0;

        UINT64 devPosition, qpcTimestamp;
        hRes = mmCapture->GetBuffer(&captureBuffer, &numFramesRead, &dwFlags, &devPosition, &qpcTimestamp);

        if (FAILED(hRes)) {
            RUNONCE AppWarning(TEXT("MMDeviceAudioSource::GetBuffer: GetBuffer failed, result = %08lX"), hRes);
            return false;
        }

        if (inputBufferSize) {
            double timeAdjust = double(inputBufferSize/GetChannelCount());
            timeAdjust /= (double(GetSamplesPerSec())*0.0000001);

            qpcTimestamp -= UINT64(timeAdjust);
        }

        qpcTimestamp /= 10000;

        firstTimestamp = GetTimestamp(qpcTimestamp);

        //---------------------------------------------------------

        UINT totalFloatsRead = numFramesRead*GetChannelCount();
        UINT newInputBufferSize = inputBufferSize + totalFloatsRead;
        if (newInputBufferSize > inputBuffer.Num())
            inputBuffer.SetSize(newInputBufferSize);

        SSECopy(inputBuffer.Array()+inputBufferSize, captureBuffer, totalFloatsRead*sizeof(float));
        inputBufferSize = newInputBufferSize;

        mmCapture->ReleaseBuffer(numFramesRead);
    }

    *numFrames = sampleWindowSize;
    *buffer = (void*)inputBuffer.Array();
    *timestamp = firstTimestamp;

    return true;
}

void MMDeviceAudioSource::ReleaseBuffer()
{
    UINT sampleSizeFloats = sampleWindowSize*GetChannelCount();
    if (inputBufferSize > sampleSizeFloats)
        SSECopy(inputBuffer.Array(), inputBuffer.Array()+sampleSizeFloats, (inputBufferSize-sampleSizeFloats)*sizeof(float));

    inputBufferSize -= sampleSizeFloats;
}

/*bool MMDeviceAudioSource::GetNextBuffer(void **buffer, UINT *numFrames, QWORD *timestamp)
{
    UINT captureSize = 0;
    HRESULT err = mmCapture->GetNextPacketSize(&captureSize);
    if(FAILED(err))
    {
        RUNONCE AppWarning(TEXT("MMDeviceAudioSource::GetBuffer: GetNextPacketSize failed, result = %08lX"), err);
        return false;
    }

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
            RUNONCE AppWarning(TEXT("MMDeviceAudioSource::GetBuffer: GetBuffer failed, result = %08lX"), err);
            return false;
        }

        qpcTimestamp /= 10000;

        //-----------------------------------------------------------------
        // timestamp bs

        QWORD newTimestamp = 0;

        if(bIsMic)
        {
            newTimestamp = (bUseQPC) ? qpcTimestamp : App->GetAudioTime();
            newTimestamp += GetTimeOffset();
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
                        Log(TEXT("Syncing audio to video time"));

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
*/