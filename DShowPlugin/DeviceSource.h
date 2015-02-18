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

#include <memory>

void PackPlanar(LPBYTE convertBuffer, LPBYTE lpPlanar, UINT renderCX, UINT renderCY, UINT pitch, UINT startY, UINT endY, UINT linePitch, UINT lineShift);

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

struct SampleData {
    //IMediaSample *sample;
    LPBYTE lpData;
    long dataLength;

    int cx, cy;

    bool bAudio;
    LONGLONG timestamp;
    volatile long refs;

    inline SampleData() {refs = 1;}
    inline ~SampleData() {Free(lpData);} //sample->Release();}

    inline void AddRef() {++refs;}
    inline void Release()
    {
        if(!InterlockedDecrement(&refs))
            delete this;
    }
};

struct ConvertData
{
    LPBYTE input, output;
    SampleData *sample;
    HANDLE hSignalConvert, hSignalComplete;
    bool   bKillThread;
    UINT   width, height;
    UINT   pitch;
    UINT   startY, endY;
    UINT   linePitch, lineShift;
};

class DeviceSource;

class DeviceAudioSource : public AudioSource
{
    DeviceSource *device;

    UINT sampleSegmentSize, sampleFrameCount;

    HANDLE hAudioMutex;
    List<BYTE> sampleBuffer;
    List<BYTE> outputBuffer;

    int offset;

protected:
    virtual bool GetNextBuffer(void **buffer, UINT *numFrames, QWORD *timestamp);
    virtual void ReleaseBuffer();

    virtual CTSTR GetDeviceName() const;

public:
    bool Initialize(DeviceSource *parent);
    ~DeviceAudioSource();

    void ReceiveAudio(LPBYTE lpData, UINT dataLength);

    void FlushSamples();

    inline void SetAudioOffset(int offset) {this->offset = offset; SetTimeOffset(offset);}
};

class DeviceSource : public ImageSource
{
    friend class DeviceAudioSource;
    friend class CapturePin;

    IGraphBuilder           *graph;
    ICaptureGraphBuilder2   *capture;
    IMediaControl           *control;

    IBaseFilter             *deviceFilter;
    IBaseFilter             *audioDeviceFilter;
    CaptureFilter           *captureFilter;
    IBaseFilter             *audioFilter; // Audio renderer filter

    //---------------------------------

    WAVEFORMATEX            audioFormat;
    DeviceAudioSource       *audioOut;

    bool bRequestVolume;
    float fNewVol;

    UINT enteredSceneCount;

    //---------------------------------

    DeviceColorType colorType;

    String          strDevice, strDeviceName, strDeviceID;
    String          strAudioDevice, strAudioName, strAudioID;
    bool            bFlipVertical, bFlipHorizontal, bDeviceHasAudio, bUsePointFiltering, bUseAudioRender;
    UINT64          frameInterval;
    UINT            renderCX, renderCY;
    UINT            newCX, newCY;
    UINT            imageCX, imageCY;
    UINT            linePitch, lineShift, lineSize;
    BOOL            bUseCustomResolution;
    UINT            preferredOutputType;
    BOOL            fullRange;
    int             colorSpace;
    BOOL            use709;

    struct {
        int                         type; //DeinterlacingType
        char                        fieldOrder; //DeinterlacingFieldOrder
        char                        processor; //DeinterlacingProcessor
        bool                        curField, bNewFrame;
        bool                        doublesFramerate;
        bool                        needsPreviousFrame;
        bool                        isReady;
        std::unique_ptr<Texture>    texture;
        UINT                        imageCX, imageCY;
        std::unique_ptr<Shader>     vertexShader;
        FuturePixelShader           pixelShader;
    } deinterlacer;

    bool            bFirstFrame;
    bool            bUseThreadedConversion;
    bool            bReadyToDraw;

    int             soundOutputType;
    bool            bOutputAudioToDesktop;

    Texture         *texture, *previousTexture;
    XElement        *data;
    UINT            texturePitch;
    bool            bCapturing, bFiltersLoaded;
    Shader          *colorConvertShader;
    Shader          *drawShader;

    bool            bUseBuffering;
    HANDLE          hStopSampleEvent;
    HANDLE          hSampleMutex;
    HANDLE          hSampleThread;
    UINT            bufferTime;				// 100-nsec units (same as REFERENCE_TIME)
    SampleData      *latestVideoSample;
    List<SampleData*> samples;

    UINT            opacity;

    int             gamma;

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

    void ChangeSize(bool bSucceeded = true, bool bForce = false);
    void KillThreads();

    String ChooseShader();
    String ChooseDeinterlacingShader();

    void Convert422To444(LPBYTE convertBuffer, LPBYTE lp422, UINT pitch, bool bLeadingY);

    void FlushSamples()
    {
        OSEnterMutex(hSampleMutex);
        for (UINT i=0; i<samples.Num(); i++)
            delete samples[i];
        samples.Clear();
        SafeRelease(latestVideoSample);
        OSLeaveMutex(hSampleMutex);
    }

    void SetAudioInfo(AM_MEDIA_TYPE *audioMediaType, GUID &expectedAudioType);

    UINT GetSampleInsertIndex(LONGLONG timestamp);
    void ReceiveMediaSample(IMediaSample *sample, bool bAudio);

    bool LoadFilters();
    void UnloadFilters();

    void Start();
    void Stop();

    static DWORD WINAPI SampleThread(DeviceSource *source);

public:
    bool Init(XElement *data);
    ~DeviceSource();

    void UpdateSettings();

    void Preprocess();
    void Render(const Vect2 &pos, const Vect2 &size);

    void BeginScene();
    void EndScene();

    void GlobalSourceEnterScene();
    void GlobalSourceLeaveScene();

    void SetInt(CTSTR lpName, int iVal);
    void SetFloat(CTSTR lpName, float fValue);

    Vect2 GetSize() const {return Vect2(float(imageCX), float(imageCY));}
};

