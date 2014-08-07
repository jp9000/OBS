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


//  So you came in here wondering, "...why are you playing a blank looping sound?".  You see, WASAPI, the new primary windows
//sound interface for vista+, will disable the main audio mixer if there is nothing playing.  What this means is that all output
//to our audio stream completely stops, and the timestamps on our audio packets become unreliable.  So, to enable the mixer by
//force, we play a muted 1 second sound loop to ensure that the audio stream continues seamlessly.  A simple but effective hack.

struct BlankAudioPlayback
{
    IMMDeviceEnumerator *mmEnumerator;
    IMMDevice           *mmDevice;
    IAudioClient        *mmClient;
    IAudioRenderClient  *mmRender;

    BlankAudioPlayback(CTSTR lpDevice)
    {
        const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
        const IID IID_IMMDeviceEnumerator    = __uuidof(IMMDeviceEnumerator);
        const IID IID_IAudioClient           = __uuidof(IAudioClient);
        const IID IID_IAudioRenderClient     = __uuidof(IAudioRenderClient);

        HRESULT err;
        err = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&mmEnumerator);
        if(FAILED(err))
            CrashError(TEXT("Could not create IMMDeviceEnumerator: 0x%08lx"), err);

        if (scmpi(lpDevice, TEXT("Default")) == 0)
            err = mmEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &mmDevice);
        else
            err = mmEnumerator->GetDevice(lpDevice, &mmDevice);
        if(FAILED(err))
            CrashError(TEXT("Could not create IMMDevice"));

        err = mmDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&mmClient);
        if(FAILED(err))
            CrashError(TEXT("Could not create IAudioClient"));

        WAVEFORMATEX *pwfx;
        err = mmClient->GetMixFormat(&pwfx);
        if(FAILED(err))
            CrashError(TEXT("Could not get mix format from audio client"));

        UINT inputBlockSize = pwfx->nBlockAlign;

        err = mmClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, ConvertMSTo100NanoSec(1000), 0, pwfx, NULL);
        if(FAILED(err))
            CrashError(TEXT("Could not initialize audio client, error = %08lX"), err);

        err = mmClient->GetService(IID_IAudioRenderClient, (void**)&mmRender);
        if(FAILED(err))
            CrashError(TEXT("Could not get audio render client"));

        //----------------------------------------------------------------

        UINT bufferFrameCount;
        err = mmClient->GetBufferSize(&bufferFrameCount);
        if(FAILED(err))
            CrashError(TEXT("Could not get audio buffer size"));

        BYTE *lpData;
        err = mmRender->GetBuffer(bufferFrameCount, &lpData);
        if(FAILED(err))
            CrashError(TEXT("Could not get audio buffer"));

        zero(lpData, bufferFrameCount*inputBlockSize);

        mmRender->ReleaseBuffer(bufferFrameCount, 0);//AUDCLNT_BUFFERFLAGS_SILENT); //probably better if it doesn't know

        if(FAILED(mmClient->Start()))
            CrashError(TEXT("Could not start audio source"));
    }

    ~BlankAudioPlayback()
    {
        mmClient->Stop();

        SafeRelease(mmRender);
        SafeRelease(mmClient);
        SafeRelease(mmDevice);
        SafeRelease(mmEnumerator);
    }
};


static BlankAudioPlayback *curBlankPlaybackThingy = NULL;

void StartBlankSoundPlayback(CTSTR lpDevice)
{
    if(!curBlankPlaybackThingy)
        curBlankPlaybackThingy = new BlankAudioPlayback(lpDevice);
}

void StopBlankSoundPlayback()
{
    if(curBlankPlaybackThingy)
    {
        delete curBlankPlaybackThingy;
        curBlankPlaybackThingy = NULL;
    }
}
