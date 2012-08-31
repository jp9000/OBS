
#include "Main.h"

#include <MMReg.h>
#include <Audioclient.h>
#include <dsound.h>


//this class is obsolete, use MMDeviceAudioSource instead
class DSoundAudioSource : public AudioSource
{
    IDirectSoundCapture8        *capture;
    IDirectSoundCaptureBuffer8  *captureBuffer;

    bool bCapturing;
    UINT capturePos;
    UINT bufferSize;
    UINT blockSize;

    List<float> readBuffer;

public:
    DSoundAudioSource()
    {
        HRESULT err;
        
        err = DirectSoundCaptureCreate8(&DSDEVID_DefaultVoiceCapture, &capture, NULL);
        if(FAILED(err))
            CrashError(TEXT("Could not create directsound capture interface"));

        WAVEFORMATEXTENSIBLE wfext;
        wfext.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX);
        wfext.Format.nSamplesPerSec = 48000;
        wfext.Format.nChannels = 2;
        wfext.Format.nBlockAlign = sizeof(float)*wfext.Format.nChannels;
        wfext.Format.nAvgBytesPerSec = wfext.Format.nBlockAlign*wfext.Format.nSamplesPerSec;
        wfext.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wfext.Format.wBitsPerSample = 32;
        wfext.Samples.wValidBitsPerSample = 32;
        wfext.dwChannelMask = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT;
        wfext.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

        blockSize = wfext.Format.nBlockAlign;

        bufferSize = 5*wfext.Format.nAvgBytesPerSec;

        DSCBUFFERDESC bd;
        zero(&bd, sizeof(bd));
        bd.dwSize = sizeof(bd);
        bd.dwBufferBytes = 5*wfext.Format.nAvgBytesPerSec;
        bd.lpwfxFormat = (WAVEFORMATEX*)&wfext;

        IDirectSoundCaptureBuffer *capBuf;
        err = capture->CreateCaptureBuffer(&bd, &capBuf, NULL);
        if(FAILED(err))
            CrashError(TEXT("Could not create directsound capture buffer interface"));

        if(FAILED(capBuf->QueryInterface(IID_IDirectSoundCaptureBuffer8, (void**)&captureBuffer)))
            CrashError(TEXT("What the..  couldn't query interface for directsound capture buffer?  This shouldn't even be possible"));
        capBuf->Release();
    }

    virtual UINT GetSamplesPerSec() const {return 48000;}

    virtual void StartCapture()
    {
        if(captureBuffer && !bCapturing)
        {
            captureBuffer->Start(DSCBSTART_LOOPING);
            bCapturing = true;
        }
    }

    virtual void StopCapture()
    {
        if(captureBuffer && bCapturing)
        {
            captureBuffer->Stop();
            bCapturing = false;
        }
    }

    virtual bool GetBuffer(float **buffer, UINT *numFrames)
    {
        if(!captureBuffer)
        {
            AppWarning(TEXT("No directsound capture buffer to get data from"));
            return false;
        }

        DWORD maxPos;
        captureBuffer->GetCurrentPosition(NULL, &maxPos);

        if(capturePos == maxPos)
            return false;

        if(capturePos > maxPos)
            maxPos += bufferSize;

        DWORD lockSize = maxPos-capturePos;

        float *buf1, *buf2 = NULL;
        DWORD buf1Size, buf2Size = 0;

        if(FAILED(captureBuffer->Lock(capturePos, lockSize, (void**)&buf1, &buf1Size, (void**)&buf2, &buf2Size, 0)))
        {
            AppWarning(TEXT("Failed to lock directsound capture buffer"));
            return false;
        }

        DWORD totalSize = buf1Size+buf2Size;
        if(readBuffer.Num() < totalSize)
            readBuffer.SetSize(totalSize);

        mcpy(readBuffer.Array(), buf1, buf1Size);

        if(buf2Size)
            mcpy(((LPBYTE)readBuffer.Array())+buf1Size, buf2, buf2Size);

        captureBuffer->Unlock((LPVOID)buf1, buf1Size, (LPVOID)buf2, buf2Size);

        *buffer = readBuffer.Array();
        *numFrames = totalSize/blockSize;

        capturePos += totalSize;
        if(capturePos > bufferSize)
            capturePos -= bufferSize;

        return totalSize != 0;
    }
};


AudioSource* CreateDSoundAudioSource()
{
    return new DSoundAudioSource;
}
