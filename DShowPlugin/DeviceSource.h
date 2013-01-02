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

void PackPlanar(LPBYTE convertBuffer, LPBYTE lpPlanar, UINT renderCX, UINT renderCY, UINT pitch, UINT startY, UINT endY);

enum DeviceColorType
{
    DeviceOutputType_RGB,

    //planar 4:2:0
    DeviceOutputType_I420,
    DeviceOutputType_YV12,

    //packed 4:2:2
    DeviceOutputType_YVYU,
    DeviceOutputType_YUY2,
    DeviceOutputType_UYVY,
    DeviceOutputType_HDYC,
};

struct ConvertData
{
    LPBYTE input, output;
    IMediaSample *sample;
    HANDLE hSignalConvert, hSignalComplete;
    bool bKillThread;
    UINT width, height;
    UINT pitch;
    UINT startY, endY;
};

class DeviceSource;

class DeviceAudioSource : public AudioSource
{
    DeviceSource *device;

    UINT sampleSegmentSize, sampleFrameCount;

    HANDLE hAudioMutex;
    List<BYTE> sampleBuffer;
    List<BYTE> outputBuffer;

    int soundTimeOffset;

protected:
    virtual bool GetNextBuffer(void **buffer, UINT *numFrames, QWORD *timestamp);
    virtual void ReleaseBuffer();

    virtual CTSTR GetDeviceName() const;

public:
    bool Initialize(DeviceSource *parent, int soundTimeOffset);
    ~DeviceAudioSource();

    void ReceiveAudio(IMediaSample *sample);
};

class DeviceSource : public ImageSource
{
    friend class DeviceAudioSource;
    friend class CapturePin;

    IGraphBuilder           *graph;
    ICaptureGraphBuilder2   *capture;
    IMediaControl           *control;

    IBaseFilter             *deviceFilter;
    CaptureFilter           *captureFilter;
    IBaseFilter             *audioFilter;

    //---------------------------------

    WAVEFORMATEX            audioFormat;
    DeviceAudioSource       *audioOut;

    //---------------------------------

    DeviceColorType colorType;

    String          strDevice, strDeviceName, strDeviceID;
    bool            bFlipVertical, bFlipHorizontal;
    UINT64          frameInterval;
    UINT            renderCX, renderCY;
    BOOL            bUseCustomResolution;
    UINT            preferredOutputType;

    bool            bFirstFrame;
    bool            bUseThreadedConversion;
    bool            bReadyToDraw;

    int             soundOutputType;
    bool            bOutputAudioToDesktop;

    Texture         *texture;
    HANDLE          hSampleMutex;
    XElement        *data;
    UINT            texturePitch;
    bool            bCapturing, bFiltersLoaded;
    IMediaSample    *curSample;
    Shader          *colorConvertShader;

    UINT            opacity;

    //---------------------------------

    LPBYTE          lpImageBuffer;
    ConvertData     *convertData;
    HANDLE          *hConvertThreads;

    //---------------------------------

    bool            bUseChromaKey;
    DWORD           keyColor;
    Color4          keyChroma;
    Color4          keyBaseColor;
    int             keySimilarity;
    int             keyBlend;
    int             keySpillReduction;

    //---------------------------------

    String ChooseShader();

    void Convert422To444(LPBYTE convertBuffer, LPBYTE lp422, UINT pitch, bool bLeadingY);

    void FlushSamples()
    {
        OSEnterMutex(hSampleMutex);
        SafeRelease(curSample);
        OSLeaveMutex(hSampleMutex);
    }

    void ReceiveVideo(IMediaSample *sample);
    void ReceiveAudio(IMediaSample *sample);

    bool LoadFilters();
    void UnloadFilters();

    void Start();
    void Stop();

public:
    bool Init(XElement *data);
    ~DeviceSource();

    void UpdateSettings();

    void Preprocess();
    void Render(const Vect2 &pos, const Vect2 &size);

    void BeginScene();
    void EndScene();

    virtual void SetInt(CTSTR lpName, int iVal);

    Vect2 GetSize() const {return Vect2(float(renderCX), float(renderCY));}
};

