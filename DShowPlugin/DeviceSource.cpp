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

struct ResSize
{
    UINT cx;
    UINT cy;
};

enum
{
    COLORSPACE_AUTO,
    COLORSPACE_709,
    COLORSPACE_601
};

#undef DEFINE_GUID
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        EXTERN_C const GUID DECLSPEC_SELECTANY name \
                = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

#include "IVideoCaptureFilter.h" // for Elgato GameCapture

DWORD STDCALL PackPlanarThread(ConvertData *data);

#define NEAR_SILENT  3000
#define NEAR_SILENTf 3000.0


#define ELGATO_FORCE_BUFFERING 1 // Workaround to prevent jerky playback with HD60

#if ELGATO_FORCE_BUFFERING
// FMB NOTE 03-Feb-15: Workaround for Elgato Game Capture HD60 which plays jerky unless we add a little buffering.
// The buffer time for this workaround is so small that it shouldn't affect sync with other sources.
// FMB NOTE 18-Feb-15: Enable buffering for every Elgato device to make sure device timestamps are used.
//                     Should improve sync issues.

// param argBufferTime - 100-nsec unit (same as REFERENCE_TIME)
void ElgatoCheckBuffering(IBaseFilter* deviceFilter, bool& argUseBuffering, UINT& argBufferTime)
{
    const int elgatoMinBufferTime = 1 * 10000;	// 1 msec

    if (!argUseBuffering || argBufferTime < elgatoMinBufferTime)
    {
        argUseBuffering = true;
        argBufferTime = elgatoMinBufferTime;
        Log(TEXT("    Elgato Game Capture: force buffering with %d msec"), elgatoMinBufferTime / 10000);
    }
}
#endif // ELGATO_FORCE_BUFFERING

DeinterlacerConfig deinterlacerConfigs[DEINTERLACING_TYPE_LAST] = {
    {DEINTERLACING_NONE,        FIELD_ORDER_NONE,                   DEINTERLACING_PROCESSOR_CPU},
    {DEINTERLACING_DISCARD,     FIELD_ORDER_TFF | FIELD_ORDER_BFF,  DEINTERLACING_PROCESSOR_CPU},
    {DEINTERLACING_RETRO,       FIELD_ORDER_TFF | FIELD_ORDER_BFF,  DEINTERLACING_PROCESSOR_CPU | DEINTERLACING_PROCESSOR_GPU,  true},
    {DEINTERLACING_BLEND,       FIELD_ORDER_NONE,                   DEINTERLACING_PROCESSOR_GPU},
    {DEINTERLACING_BLEND2x,     FIELD_ORDER_TFF | FIELD_ORDER_BFF,  DEINTERLACING_PROCESSOR_GPU,                                true},
    {DEINTERLACING_LINEAR,      FIELD_ORDER_TFF | FIELD_ORDER_BFF,  DEINTERLACING_PROCESSOR_GPU},
    {DEINTERLACING_LINEAR2x,    FIELD_ORDER_TFF | FIELD_ORDER_BFF,  DEINTERLACING_PROCESSOR_GPU,                                true},
    {DEINTERLACING_YADIF,       FIELD_ORDER_TFF | FIELD_ORDER_BFF,  DEINTERLACING_PROCESSOR_GPU},
    {DEINTERLACING_YADIF2x,     FIELD_ORDER_TFF | FIELD_ORDER_BFF,  DEINTERLACING_PROCESSOR_GPU,                                true},
    {DEINTERLACING__DEBUG,      FIELD_ORDER_TFF | FIELD_ORDER_BFF,  DEINTERLACING_PROCESSOR_GPU},
};

bool DeviceSource::Init(XElement *data)
{
    HRESULT err;
    err = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, (REFIID)IID_IFilterGraph, (void**)&graph);
    if(FAILED(err))
    {
        AppWarning(TEXT("DShowPlugin: Failed to build IGraphBuilder, result = %08lX"), err);
        return false;
    }

    err = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER, (REFIID)IID_ICaptureGraphBuilder2, (void**)&capture);
    if(FAILED(err))
    {
        AppWarning(TEXT("DShowPlugin: Failed to build ICaptureGraphBuilder2, result = %08lX"), err);
        return false;
    }

    hSampleMutex = OSCreateMutex();
    if(!hSampleMutex)
    {
        AppWarning(TEXT("DShowPlugin: could not create sample mutex"));
        return false;
    }

    capture->SetFiltergraph(graph);

    int numThreads = MAX(OSGetTotalCores()-2, 1);
    hConvertThreads = (HANDLE*)Allocate(sizeof(HANDLE)*numThreads);
    convertData = (ConvertData*)Allocate(sizeof(ConvertData)*numThreads);

    zero(hConvertThreads, sizeof(HANDLE)*numThreads);
    zero(convertData, sizeof(ConvertData)*numThreads);

    this->data = data;
    UpdateSettings();

    //if(!bFiltersLoaded)
    //    return false;

    Log(TEXT("Using directshow input"));

    return true;
}

DeviceSource::~DeviceSource()
{
    Stop();
    UnloadFilters();

    FlushSamples();

    SafeReleaseLogRef(capture);
    SafeReleaseLogRef(graph);

    if(hConvertThreads)
        Free(hConvertThreads);

    if(convertData)
        Free(convertData);

    if(hSampleMutex)
        OSCloseMutex(hSampleMutex);
}

#define SHADER_PATH TEXT("plugins/DShowPlugin/shaders/")

String DeviceSource::ChooseShader()
{
    if(colorType == DeviceOutputType_RGB && !bUseChromaKey)
        return String();

    String strShader;
    strShader << SHADER_PATH;

    if(bUseChromaKey)
        strShader << TEXT("ChromaKey_");

    if(colorType == DeviceOutputType_I420)
        strShader << TEXT("YUVToRGB.pShader");
    else if(colorType == DeviceOutputType_YV12)
        strShader << TEXT("YVUToRGB.pShader");
    else if(colorType == DeviceOutputType_YVYU)
        strShader << TEXT("YVXUToRGB.pShader");
    else if(colorType == DeviceOutputType_YUY2)
        strShader << TEXT("YUXVToRGB.pShader");
    else if(colorType == DeviceOutputType_UYVY)
        strShader << TEXT("UYVToRGB.pShader");
    else if(colorType == DeviceOutputType_HDYC)
        strShader << TEXT("HDYCToRGB.pShader");
    else
        strShader << TEXT("RGB.pShader");

    return strShader;
}

String DeviceSource::ChooseDeinterlacingShader()
{
    String shader;
    shader << SHADER_PATH << TEXT("Deinterlace_");

#ifdef _DEBUG
#define DEBUG__ _DEBUG
#undef _DEBUG
#endif
#define SELECT(x) case DEINTERLACING_##x: shader << String(TEXT(#x)).MakeLower(); break;
    switch(deinterlacer.type)
    {
        SELECT(RETRO)
        SELECT(BLEND)
        SELECT(BLEND2x)
        SELECT(LINEAR)
        SELECT(LINEAR2x)
        SELECT(YADIF)
        SELECT(YADIF2x)
        SELECT(_DEBUG)
    }
    return shader << TEXT(".pShader");
#undef SELECT
#ifdef DEBUG__
#define _DEBUG DEBUG__
#undef DEBUG__
#endif
}

const float yuv709Mat[16] = {0.182586f,  0.614231f,  0.062007f, 0.062745f,
                            -0.100644f, -0.338572f,  0.439216f, 0.501961f,
                             0.439216f, -0.398942f, -0.040274f, 0.501961f,
                             0.000000f,  0.000000f,  0.000000f, 1.000000f};

const float yuvMat[16] = {0.256788f,  0.504129f,  0.097906f, 0.062745f,
                         -0.148223f, -0.290993f,  0.439216f, 0.501961f,
                          0.439216f, -0.367788f, -0.071427f, 0.501961f,
                          0.000000f,  0.000000f,  0.000000f, 1.000000f};

const float yuvToRGB601[2][16] =
{
    {
        1.164384f,  0.000000f,  1.596027f, -0.874202f,
        1.164384f, -0.391762f, -0.812968f,  0.531668f,
        1.164384f,  2.017232f,  0.000000f, -1.085631f,
        0.000000f,  0.000000f,  0.000000f,  1.000000f
    },
    {
        1.000000f,  0.000000f,  1.407520f, -0.706520f,
        1.000000f, -0.345491f, -0.716948f,  0.533303f,
        1.000000f,  1.778976f,  0.000000f, -0.892976f,
        0.000000f,  0.000000f,  0.000000f,  1.000000f
    }
};

const float yuvToRGB709[2][16] = {
    {
        1.164384f, 0.000000f, 1.792741f, -0.972945f,
        1.164384f, -0.213249f, -0.532909f, 0.301483f,
        1.164384f, 2.112402f, 0.000000f, -1.133402f,
        0.000000f, 0.000000f, 0.000000f, 1.000000f
    },
    {
        1.000000f, 0.000000f, 1.581000f, -0.793600f,
        1.000000f, -0.188062f, -0.469967f, 0.330305f,
        1.000000f, 1.862906f, 0.000000f, -0.935106f,
        0.000000f, 0.000000f, 0.000000f, 1.000000f
    }
};

void DeviceSource::SetAudioInfo(AM_MEDIA_TYPE *audioMediaType, GUID &expectedAudioType)
{
    expectedAudioType = audioMediaType->subtype;

    if(audioMediaType->formattype == FORMAT_WaveFormatEx)
    {
        WAVEFORMATEX *pFormat = reinterpret_cast<WAVEFORMATEX*>(audioMediaType->pbFormat);
        mcpy(&audioFormat, pFormat, sizeof(audioFormat));

        Log(TEXT("    device audio info - bits per sample: %u, channels: %u, samples per sec: %u, block size: %u"),
            audioFormat.wBitsPerSample, audioFormat.nChannels, audioFormat.nSamplesPerSec, audioFormat.nBlockAlign);

        //avoid local resampling if possible
        /*if(pFormat->nSamplesPerSec != 44100)
        {
            pFormat->nSamplesPerSec = 44100;
            if(SUCCEEDED(audioConfig->SetFormat(audioMediaType)))
            {
                Log(TEXT("    also successfully set samples per sec to 44.1k"));
                audioFormat.nSamplesPerSec = 44100;
            }
        }*/
    }
    else
    {
        AppWarning(TEXT("DShowPlugin: Audio format was not a normal wave format"));
        soundOutputType = 0;
    }

    DeleteMediaType(audioMediaType);
}

bool DeviceSource::LoadFilters()
{
    if(bCapturing || bFiltersLoaded)
        return false;

    bool bSucceeded = false;

    List<MediaOutputInfo> outputList;
    IAMStreamConfig *config = NULL;
    bool bAddedVideoCapture = false, bAddedAudioCapture = false, bAddedDevice = false;
    GUID expectedMediaType;
    IPin *devicePin = NULL, *audioPin = NULL;
    HRESULT err;
    String strShader;

    deinterlacer.isReady = true;

    if(graph == NULL) {
        err = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, (REFIID)IID_IFilterGraph, (void**)&graph);
        if(FAILED(err))
        {
            AppWarning(TEXT("DShowPlugin: Failed to build IGraphBuilder, result = %08lX"), err);
            goto cleanFinish;
        }
    }

    if(capture == NULL) {
        err = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER, (REFIID)IID_ICaptureGraphBuilder2, (void**)&capture);
        if(FAILED(err))
        {
            AppWarning(TEXT("DShowPlugin: Failed to build ICaptureGraphBuilder2, result = %08lX"), err);
            goto cleanFinish;
        }

        capture->SetFiltergraph(graph);
    }

    bUseThreadedConversion = API->UseMultithreadedOptimizations() && (OSGetTotalCores() > 1);

    //------------------------------------------------
    // basic initialization vars

    bool bForceCustomAudio = data->GetInt(TEXT("forceCustomAudioDevice")) != 0;
    bUseAudioRender = data->GetInt(TEXT("useAudioRender")) != 0;

    bUseCustomResolution = data->GetInt(TEXT("customResolution"));
    strDevice = data->GetString(TEXT("device"));
    strDeviceName = data->GetString(TEXT("deviceName"));
    strDeviceID = data->GetString(TEXT("deviceID"));
    strAudioDevice = data->GetString(TEXT("audioDevice"));
    strAudioName = data->GetString(TEXT("audioDeviceName"));
    strAudioID = data->GetString(TEXT("audioDeviceID"));
    fullRange = data->GetInt(TEXT("fullrange")) != 0;
    use709 = false;

    bFlipVertical = data->GetInt(TEXT("flipImage")) != 0;
    bFlipHorizontal = data->GetInt(TEXT("flipImageHorizontal")) != 0;
    bUsePointFiltering = data->GetInt(TEXT("usePointFiltering")) != 0;

    bool elgato = sstri(strDeviceName, L"elgato") != nullptr;

    opacity = data->GetInt(TEXT("opacity"), 100);

    float volume = data->GetFloat(TEXT("volume"), 1.0f);

    bUseBuffering = data->GetInt(TEXT("useBuffering")) != 0;
    bufferTime = data->GetInt(TEXT("bufferTime"))*10000;

    //------------------------------------------------
    // chrom key stuff

    bUseChromaKey = data->GetInt(TEXT("useChromaKey")) != 0;
    keyColor = data->GetInt(TEXT("keyColor"), 0xFFFFFFFF);
    keySimilarity = data->GetInt(TEXT("keySimilarity"));
    keyBlend = data->GetInt(TEXT("keyBlend"), 80);
    keySpillReduction = data->GetInt(TEXT("keySpillReduction"), 50);
    
    deinterlacer.type               = data->GetInt(TEXT("deinterlacingType"), 0);
    deinterlacer.fieldOrder         = data->GetInt(TEXT("deinterlacingFieldOrder"), 0);
    deinterlacer.processor          = data->GetInt(TEXT("deinterlacingProcessor"), 0);
    deinterlacer.doublesFramerate   = data->GetInt(TEXT("deinterlacingDoublesFramerate"), 0) != 0;

    if(keyBaseColor.x < keyBaseColor.y && keyBaseColor.x < keyBaseColor.z)
        keyBaseColor -= keyBaseColor.x;
    else if(keyBaseColor.y < keyBaseColor.x && keyBaseColor.y < keyBaseColor.z)
        keyBaseColor -= keyBaseColor.y;
    else if(keyBaseColor.z < keyBaseColor.x && keyBaseColor.z < keyBaseColor.y)
        keyBaseColor -= keyBaseColor.z;

    //------------------------------------------------
    // get the device filter and pins

    if(strDeviceName.IsValid())
        deviceFilter = GetDeviceByValue(CLSID_VideoInputDeviceCategory, L"FriendlyName", strDeviceName, L"DevicePath", strDeviceID);
    else
    {
        if(!strDevice.IsValid())
        {
            AppWarning(TEXT("DShowPlugin: Invalid device specified"));
            goto cleanFinish;
        }

        deviceFilter = GetDeviceByValue(CLSID_VideoInputDeviceCategory, L"FriendlyName", strDevice);
    }
    
    if(!deviceFilter)
    {
        AppWarning(TEXT("DShowPlugin: Could not create device filter"));
        goto cleanFinish;
    }

    devicePin = GetOutputPin(deviceFilter, &MEDIATYPE_Video);
    if(!devicePin)
    {
        AppWarning(TEXT("DShowPlugin: Could not get device video pin"));
        goto cleanFinish;
    }

    soundOutputType = data->GetInt(TEXT("soundOutputType")); //0 is for backward-compatibility
    if (strAudioID.CompareI(TEXT("Disabled")))
        soundOutputType = 0;

    if(soundOutputType != 0)
    {
        if(!bForceCustomAudio)
        {
            err = capture->FindPin(deviceFilter, PINDIR_OUTPUT, &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, FALSE, 0, &audioPin);
            bDeviceHasAudio = SUCCEEDED(err);
        }
        else
            bDeviceHasAudio = false;

        if(!bDeviceHasAudio)
        {
            if(strDeviceName.IsValid())
            {
                audioDeviceFilter = GetDeviceByValue(CLSID_AudioInputDeviceCategory, L"FriendlyName", strAudioName, L"DevicePath", strAudioID);
                if(!audioDeviceFilter)
                    AppWarning(TEXT("DShowPlugin: Invalid audio device: name '%s', path '%s'"), strAudioName.Array(), strAudioID.Array());
            }
            else if(strAudioDevice.IsValid())
            {
                audioDeviceFilter = GetDeviceByValue(CLSID_AudioInputDeviceCategory, L"FriendlyName", strAudioDevice);
                if(!audioDeviceFilter)
                    AppWarning(TEXT("DShowPlugin: Could not create audio device filter"));
            }

            if(audioDeviceFilter)
                err = capture->FindPin(audioDeviceFilter, PINDIR_OUTPUT, &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, FALSE, 0, &audioPin);
            else
                err = E_FAIL;
        }

        if(FAILED(err) || !audioPin)
        {
            Log(TEXT("DShowPlugin: No audio pin, result = %lX"), err);
            soundOutputType = 0;
        }
    }
    else
        bDeviceHasAudio = bForceCustomAudio = false;

    int soundTimeOffset = data->GetInt(TEXT("soundTimeOffset"));

    GetOutputList(devicePin, outputList);

    //------------------------------------------------
    // initialize the basic video variables and data

    if(FAILED(err = devicePin->QueryInterface(IID_IAMStreamConfig, (void**)&config)))
    {
        AppWarning(TEXT("DShowPlugin: Could not get IAMStreamConfig for device pin, result = %08lX"), err);
        goto cleanFinish;
    }

    renderCX = renderCY = newCX = newCY = 0;
    frameInterval = 0;

    IElgatoVideoCaptureFilter6* elgatoFilterInterface6 = nullptr; // FMB NOTE: IElgatoVideoCaptureFilter6 available since EGC v.2.20
    if (SUCCEEDED(deviceFilter->QueryInterface(IID_IElgatoVideoCaptureFilter6, (void**)&elgatoFilterInterface6)))
         elgatoFilterInterface6->Release();
    bool elgatoSupportsIAMStreamConfig = (nullptr != elgatoFilterInterface6);
    bool elgatoCanRenderFromPin        = (nullptr != elgatoFilterInterface6);

    UINT elgatoCX = 1280;
    UINT elgatoCY = 720;

    if(bUseCustomResolution)
    {
        renderCX = newCX = data->GetInt(TEXT("resolutionWidth"));
        renderCY = newCY = data->GetInt(TEXT("resolutionHeight"));
        frameInterval = data->GetInt(TEXT("frameInterval"));
    }
    else
    {
        SIZE size;
        size.cx = 0;
        size.cy = 0;

        // blackmagic/decklink devices will display a black screen if the resolution/fps doesn't exactly match.
        // they should rename the devices to blackscreen
        if (sstri(strDeviceName, L"blackmagic") != NULL || sstri(strDeviceName, L"decklink") != NULL ||
            !GetClosestResolutionFPS(outputList, size, frameInterval, true))
        {
            AM_MEDIA_TYPE *pmt;
            if (SUCCEEDED(config->GetFormat(&pmt))) {
                VIDEOINFOHEADER *pVih = reinterpret_cast<VIDEOINFOHEADER*>(pmt->pbFormat);

                // Use "preferred" format from the device
                size.cx = pVih->bmiHeader.biWidth;
                size.cy = pVih->bmiHeader.biHeight;
                frameInterval = pVih->AvgTimePerFrame;

                DeleteMediaType(pmt);
            } else {
                if (!outputList.Num()) {
                    AppWarning(L"DShowPlugin: Not even an output list!  What the f***");
                    goto cleanFinish;
                }

                /* ..........elgato */
                _ASSERTE(elgato &&!elgatoSupportsIAMStreamConfig);
                size.cx = outputList[0].maxCX;
                size.cy = outputList[0].maxCY;
                frameInterval = outputList[0].minFrameInterval;
            }
        }

        renderCX = newCX = size.cx;
        renderCY = newCY = size.cy;
    }


    /* elgato always starts off at 720p and changes after. */
    if (elgato && !elgatoSupportsIAMStreamConfig)
    {
        elgatoCX = renderCX;
        elgatoCY = renderCY;

        renderCX = newCX = 1280;
        renderCY = newCY = 720;
    }

    if(!renderCX || !renderCY || !frameInterval)
    {
        AppWarning(TEXT("DShowPlugin: Invalid size/fps specified"));
        goto cleanFinish;
    }

    preferredOutputType = (data->GetInt(TEXT("usePreferredType")) != 0) ? data->GetInt(TEXT("preferredType")) : -1;

    bFirstFrame = true;

    //------------------------------------------------
    // get the closest media output for the settings used

    MediaOutputInfo *bestOutput = GetBestMediaOutput(outputList, renderCX, renderCY, preferredOutputType, frameInterval);
    if(!bestOutput)
    {
        if (!outputList.Num()) {
            AppWarning(TEXT("DShowPlugin: Could not find appropriate resolution to create device image source"));
            goto cleanFinish;
        } else { /* エルガット＝自殺 */
            bestOutput = &outputList[0];
        }
    }

    //------------------------------------------------
    // log video info

    {
        String strTest = FormattedString(TEXT("    device: %s,\r\n    device id %s,\r\n    chosen type: %s, usingFourCC: %s, res: %ux%u - %ux%u, frameIntervals: %llu-%llu\r\n    use buffering: %s - %u"),
            strDevice.Array(), strDeviceID.Array(),
            EnumToName[(int)bestOutput->videoType],
            bestOutput->bUsingFourCC ? TEXT("true") : TEXT("false"),
            bestOutput->minCX, bestOutput->minCY, bestOutput->maxCX, bestOutput->maxCY,
            bestOutput->minFrameInterval, bestOutput->maxFrameInterval,
	    bUseBuffering ? L"true" : L"false", bufferTime);

        BITMAPINFOHEADER *bmiHeader = GetVideoBMIHeader(bestOutput->mediaType);

        char fourcc[5];
        mcpy(fourcc, &bmiHeader->biCompression, 4);
        fourcc[4] = 0;

        if(bmiHeader->biCompression > 1000)
            strTest << FormattedString(TEXT(", fourCC: '%S'\r\n"), fourcc);
        else
            strTest << FormattedString(TEXT(", fourCC: %08lX\r\n"), bmiHeader->biCompression);

        if(!bDeviceHasAudio) strTest << FormattedString(TEXT("    audio device: %s,\r\n    audio device id %s,\r\n    audio time offset %d,\r\n"), strAudioDevice.Array(), strAudioID.Array(), soundTimeOffset);

        Log(TEXT("------------------------------------------"));
        Log(strTest.Array());
    }

    //------------------------------------------------
    // set up shaders and video output data

    expectedMediaType = bestOutput->mediaType->subtype;

    colorType = DeviceOutputType_RGB;
    if(bestOutput->videoType == VideoOutputType_I420)
        colorType = DeviceOutputType_I420;
    else if(bestOutput->videoType == VideoOutputType_YV12)
        colorType = DeviceOutputType_YV12;
    else if(bestOutput->videoType == VideoOutputType_YVYU)
        colorType = DeviceOutputType_YVYU;
    else if(bestOutput->videoType == VideoOutputType_YUY2)
        colorType = DeviceOutputType_YUY2;
    else if(bestOutput->videoType == VideoOutputType_UYVY)
        colorType = DeviceOutputType_UYVY;
    else if(bestOutput->videoType == VideoOutputType_HDYC)
    {
        colorType = DeviceOutputType_HDYC;
        use709 = true;
    }
    else
    {
        colorType = DeviceOutputType_RGB;
        expectedMediaType = MEDIASUBTYPE_RGB32;
    }

    strShader = ChooseShader();
    if(strShader.IsValid())
        colorConvertShader = CreatePixelShaderFromFile(strShader);

    if(colorType != DeviceOutputType_RGB && !colorConvertShader)
    {
        AppWarning(TEXT("DShowPlugin: Could not create color space conversion pixel shader"));
        goto cleanFinish;
    }

    //------------------------------------------------
    // set chroma details

    keyBaseColor = Color4().MakeFromRGBA(keyColor);
    Matrix4x4TransformVect(keyChroma, (colorType == DeviceOutputType_HDYC || colorType == DeviceOutputType_RGB) ? (float*)yuv709Mat : (float*)yuvMat, keyBaseColor);
    keyChroma *= 2.0f;

    //------------------------------------------------
    // configure video pin

    AM_MEDIA_TYPE outputMediaType;
    CopyMediaType(&outputMediaType, bestOutput->mediaType);

    VIDEOINFOHEADER *vih  = reinterpret_cast<VIDEOINFOHEADER*>(outputMediaType.pbFormat);
    BITMAPINFOHEADER *bmi = GetVideoBMIHeader(&outputMediaType);
    vih->AvgTimePerFrame  = frameInterval;
    bmi->biWidth          = renderCX;
    bmi->biHeight         = renderCY;
    bmi->biSizeImage      = renderCX*renderCY*(bmi->biBitCount>>3);

    if(FAILED(err = config->SetFormat(&outputMediaType)))
    {
        if(err != E_NOTIMPL)
        {
            AppWarning(TEXT("DShowPlugin: SetFormat on device pin failed, result = %08lX"), err);
            goto cleanFinish;
        }
    }

    FreeMediaType(outputMediaType);

    //------------------------------------------------
    // get audio pin configuration, optionally configure audio pin to 44100

    GUID expectedAudioType;

    if(soundOutputType == 1)
    {
        IAMStreamConfig *audioConfig;
        if(SUCCEEDED(audioPin->QueryInterface(IID_IAMStreamConfig, (void**)&audioConfig)))
        {
            AM_MEDIA_TYPE *audioMediaType;
            if(SUCCEEDED(err = audioConfig->GetFormat(&audioMediaType)))
            {
                SetAudioInfo(audioMediaType, expectedAudioType);
            }
            else if(err == E_NOTIMPL) //elgato probably
            {
                IEnumMediaTypes *audioMediaTypes;
                if(SUCCEEDED(err = audioPin->EnumMediaTypes(&audioMediaTypes)))
                {
                    ULONG i = 0;
                    if((err = audioMediaTypes->Next(1, &audioMediaType, &i)) == S_OK)
                        SetAudioInfo(audioMediaType, expectedAudioType);
                    else
                    {
                        AppWarning(TEXT("DShowPlugin: audioMediaTypes->Next failed, result = %08lX"), err);
                        soundOutputType = 0;
                    }

                    audioMediaTypes->Release();
                }
                else
                {
                    AppWarning(TEXT("DShowPlugin: audioMediaTypes->Next failed, result = %08lX"), err);
                    soundOutputType = 0;
                }
            }
            else
            {
                AppWarning(TEXT("DShowPlugin: Could not get audio format, result = %08lX"), err);
                soundOutputType = 0;
            }

            audioConfig->Release();
        }
        else {
            soundOutputType = 0;
        }
    }

    //------------------------------------------------
    // add video capture filter if any

    captureFilter = new CaptureFilter(this, MEDIATYPE_Video, expectedMediaType);

    if(FAILED(err = graph->AddFilter(captureFilter, NULL)))
    {
        AppWarning(TEXT("DShowPlugin: Failed to add video capture filter to graph, result = %08lX"), err);
        goto cleanFinish;
    }

    bAddedVideoCapture = true;

    //------------------------------------------------
    // add audio capture filter if any

    if(soundOutputType == 1)
    {
        audioFilter = new CaptureFilter(this, MEDIATYPE_Audio, expectedAudioType);
        if(!audioFilter)
        {
            AppWarning(TEXT("Failed to create audio capture filter"));
            soundOutputType = 0;
        }
    }
    else if(soundOutputType == 2)
    {
        if(bUseAudioRender) {
            if(FAILED(err = CoCreateInstance(CLSID_AudioRender, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&audioFilter)))
            {
                AppWarning(TEXT("DShowPlugin: failed to create WaveOut Audio renderer, result = %08lX"), err);
                soundOutputType = 0;
            }
        }
        else {
            if(FAILED(err = CoCreateInstance(CLSID_DSoundRender, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&audioFilter)))
            {
                AppWarning(TEXT("DShowPlugin: failed to create DirectSound renderer, result = %08lX"), err);
                soundOutputType = 0;
            }
        }

        IBasicAudio *basicAudio;
        if(SUCCEEDED(audioFilter->QueryInterface(IID_IBasicAudio, (void**)&basicAudio)))
        {
            long lVol = long((double(volume)*NEAR_SILENTf)-NEAR_SILENTf);
            if(lVol <= -NEAR_SILENT)
                lVol = -10000;
            basicAudio->put_Volume(lVol);
            basicAudio->Release();
        }
    }

    if(soundOutputType != 0)
    {
        if(FAILED(err = graph->AddFilter(audioFilter, NULL)))
            AppWarning(TEXT("DShowPlugin: Failed to add audio capture filter to graph, result = %08lX"), err);

        bAddedAudioCapture = true;
    }

    //------------------------------------------------
    // add primary device filter

    if(FAILED(err = graph->AddFilter(deviceFilter, NULL)))
    {
        AppWarning(TEXT("DShowPlugin: Failed to add device filter to graph, result = %08lX"), err);
        goto cleanFinish;
    }

    if(soundOutputType != 0 && !bDeviceHasAudio)
    {
        if(FAILED(err = graph->AddFilter(audioDeviceFilter, NULL)))
            AppWarning(TEXT("DShowPlugin: Failed to add audio device filter to graph, result = %08lX"), err);
    }

    bAddedDevice = true;

    //------------------------------------------------
    // change elgato resolution
    if (elgato && !elgatoSupportsIAMStreamConfig)
    {
        /* choose closest matching elgato resolution */
        if (!bUseCustomResolution)
        {
            UINT baseCX, baseCY;
            UINT closest = 0xFFFFFFFF;
            API->GetBaseSize(baseCX, baseCY);

            const ResSize resolutions[] = {{480, 360}, {640, 480}, {1280, 720}, {1920, 1080}};
            for (const ResSize &res : resolutions) {
                UINT val = (UINT)labs((long)res.cy - (long)baseCY);
                if (val < closest) {
                    elgatoCX = res.cx;
                    elgatoCY = res.cy;
                    closest = val;
                }
            }
        }

        IElgatoVideoCaptureFilter3 *elgatoFilter = nullptr;

        if (SUCCEEDED(deviceFilter->QueryInterface(IID_IElgatoVideoCaptureFilter3, (void**)&elgatoFilter)))
        {
            VIDEO_CAPTURE_FILTER_SETTINGS settings;
            if (SUCCEEDED(elgatoFilter->GetSettings(&settings)))
            {
                if (elgatoCY == 1080)
                    settings.profile = VIDEO_CAPTURE_FILTER_VID_ENC_PROFILE_1080;
                else if (elgatoCY == 480)
                    settings.profile = VIDEO_CAPTURE_FILTER_VID_ENC_PROFILE_480;
                else if (elgatoCY == 360)
                    settings.profile = VIDEO_CAPTURE_FILTER_VID_ENC_PROFILE_360;
                else
                    settings.profile = VIDEO_CAPTURE_FILTER_VID_ENC_PROFILE_720;

                elgatoFilter->SetSettings(&settings);
            }

            elgatoFilter->Release();
        }
    }

#if ELGATO_FORCE_BUFFERING
    if (elgato)
        ElgatoCheckBuffering(deviceFilter, bUseBuffering, bufferTime);
#endif

    //------------------------------------------------
    // connect all pins and set up the whole capture thing

    bool bConnected = false;
    if (elgato && !elgatoCanRenderFromPin)
    {
        bConnected = SUCCEEDED(err = graph->ConnectDirect(devicePin, captureFilter->GetCapturePin(), nullptr));
        if (!bConnected)
        {
            AppWarning(TEXT("DShowPlugin: Failed to connect the video device pin to the video capture pin, result = %08lX"), err);
            goto cleanFinish;
        }
    }
    else
    {
        bConnected = SUCCEEDED(err = capture->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, deviceFilter, NULL, captureFilter));
        if(!bConnected)
        {
            if(FAILED(err = graph->Connect(devicePin, captureFilter->GetCapturePin())))
            {
                AppWarning(TEXT("DShowPlugin: Failed to connect the video device pin to the video capture pin, result = %08lX"), err);
                goto cleanFinish;
            }
        }
    }

    if(soundOutputType != 0)
    {
        if (elgato && bDeviceHasAudio && !elgatoCanRenderFromPin)
        {
            bConnected = false;

            IPin *audioPin = GetOutputPin(deviceFilter, &MEDIATYPE_Audio);
            if (audioPin)
            {
                IPin* audioRendererPin = NULL;

                // FMB NOTE: Connect with first (= the only) pin of audio renderer
                IEnumPins* pIEnum = NULL;
                if (SUCCEEDED(err = audioFilter->EnumPins(&pIEnum)))
                {
                    IPin* pIPin = NULL;
                    pIEnum->Next(1, &audioRendererPin, NULL);
                    SafeRelease(pIEnum);
                }

                if (audioRendererPin)
                {
                    bConnected = SUCCEEDED(err = graph->ConnectDirect(audioPin, audioRendererPin, nullptr));
                    audioRendererPin->Release();
                }
                audioPin->Release();
            }
        }
        else
        {
            if(!bDeviceHasAudio)
                bConnected = SUCCEEDED(err = capture->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, audioDeviceFilter, NULL, audioFilter));
            else
                bConnected = SUCCEEDED(err = capture->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, deviceFilter, NULL, audioFilter));
        }

        if(!bConnected)
        {
            AppWarning(TEXT("DShowPlugin: Failed to connect the audio device pin to the audio capture pin, result = %08lX"), err);
            soundOutputType = 0;
        }
    }

    if(FAILED(err = graph->QueryInterface(IID_IMediaControl, (void**)&control)))
    {
        AppWarning(TEXT("DShowPlugin: Failed to get IMediaControl, result = %08lX"), err);
        goto cleanFinish;
    }

    if (bUseBuffering) {
        if (!(hStopSampleEvent = CreateEvent(NULL, FALSE, FALSE, NULL))) {
            AppWarning(TEXT("DShowPlugin: Failed to create stop event"), err);
            goto cleanFinish;
        }

        if (!(hSampleThread = OSCreateThread((XTHREAD)SampleThread, this))) {
            AppWarning(TEXT("DShowPlugin: Failed to create sample thread"), err);
            goto cleanFinish;
        }
    }

    if(soundOutputType == 1)
    {
        audioOut = new DeviceAudioSource;
        audioOut->Initialize(this);
        API->AddAudioSource(audioOut);

        audioOut->SetAudioOffset(soundTimeOffset);
        audioOut->SetVolume(volume);
    }

    bSucceeded = true;

cleanFinish:
    SafeRelease(config);
    SafeRelease(devicePin);
    SafeRelease(audioPin);

    for(UINT i=0; i<outputList.Num(); i++)
        outputList[i].FreeData();

    if(!bSucceeded)
    {
        bCapturing = false;

        if(bAddedVideoCapture)
            graph->RemoveFilter(captureFilter);
        if(bAddedAudioCapture)
            graph->RemoveFilter(audioFilter);

        if(bAddedDevice)
        {
            if(!bDeviceHasAudio && audioDeviceFilter)
                graph->RemoveFilter(audioDeviceFilter);
            graph->RemoveFilter(deviceFilter);
        }

        SafeRelease(audioDeviceFilter);
        SafeRelease(deviceFilter);
        SafeRelease(captureFilter);
        SafeRelease(audioFilter);
        SafeRelease(control);

        if (hSampleThread) {
            SetEvent(hStopSampleEvent);
            WaitForSingleObject(hSampleThread, INFINITE);
            CloseHandle(hSampleThread);
            hSampleThread = NULL;
        }

        if (hStopSampleEvent) {
            CloseHandle(hStopSampleEvent);
            hStopSampleEvent = NULL;
        }

        if(colorConvertShader)
        {
            delete colorConvertShader;
            colorConvertShader = NULL;
        }

        if(audioOut)
        {
            delete audioOut;
            audioOut = NULL;
        }

        soundOutputType = 0;

        if(lpImageBuffer)
        {
            Free(lpImageBuffer);
            lpImageBuffer = NULL;
        }

        bReadyToDraw = true;
    }
    else
        bReadyToDraw = false;

    // Updated check to ensure that the source actually turns red instead of
    // screwing up the size when SetFormat fails.
    if (renderCX <= 0 || renderCX >= 8192) { newCX = renderCX = 32; imageCX = renderCX; }
    if (renderCY <= 0 || renderCY >= 8192) { newCY = renderCY = 32; imageCY = renderCY; }

    ChangeSize(bSucceeded, true);

    return bSucceeded;
}

void DeviceSource::UnloadFilters()
{
    if (hSampleThread) {
        SetEvent(hStopSampleEvent);
        WaitForSingleObject(hSampleThread, INFINITE);
        CloseHandle(hSampleThread);
        CloseHandle(hStopSampleEvent);

        hSampleThread = NULL;
        hStopSampleEvent = NULL;
    }

    if(texture)
    {
        delete texture;
        texture = NULL;
    }
    if(previousTexture)
    {
        delete previousTexture;
        previousTexture = NULL;
    }

    KillThreads();

    if(bFiltersLoaded)
    {
        graph->RemoveFilter(captureFilter);
        graph->RemoveFilter(deviceFilter);
        if(!bDeviceHasAudio) graph->RemoveFilter(audioDeviceFilter);

        if(audioFilter)
            graph->RemoveFilter(audioFilter);

        SafeReleaseLogRef(captureFilter);
        SafeReleaseLogRef(deviceFilter);
        SafeReleaseLogRef(audioDeviceFilter);
        SafeReleaseLogRef(audioFilter);

        bFiltersLoaded = false;
    }

    if(audioOut)
    {
        API->RemoveAudioSource(audioOut);
        delete audioOut;
        audioOut = NULL;
    }

    if(colorConvertShader)
    {
        delete colorConvertShader;
        colorConvertShader = NULL;
    }

    if(lpImageBuffer)
    {
        Free(lpImageBuffer);
        lpImageBuffer = NULL;
    }

    SafeReleaseLogRef(capture);
    SafeReleaseLogRef(graph);

    SafeRelease(control);
}

void DeviceSource::Start()
{
    if(bCapturing || !control)
        return;

    drawShader = CreatePixelShaderFromFile(TEXT("shaders\\DrawTexture_ColorAdjust.pShader"));

    HRESULT err;
    if(FAILED(err = control->Run()))
    {
        AppWarning(TEXT("DShowPlugin: control->Run failed, result = %08lX"), err);
        return;
    }

    /*if (err == S_FALSE)
        AppWarning(L"Ook");*/

    bCapturing = true;
}

void DeviceSource::Stop()
{
    delete drawShader;
    drawShader = NULL;

    if(!bCapturing)
        return;

    bCapturing = false;
    control->Stop();
    FlushSamples();
}

void DeviceSource::BeginScene()
{
    Start();
}

void DeviceSource::EndScene()
{
    Stop();
}

void DeviceSource::GlobalSourceLeaveScene()
{
    if (!enteredSceneCount)
        return;
    if (--enteredSceneCount)
        return;

    if(soundOutputType == 1) {
        audioOut->SetVolume(0.0f);
    }
    if(soundOutputType == 2) {
        IBasicAudio *basicAudio;
        if(SUCCEEDED(audioFilter->QueryInterface(IID_IBasicAudio, (void**)&basicAudio)))
        {
            long lVol = long((double(0.0)*NEAR_SILENTf)-NEAR_SILENTf);
            if(lVol <= -NEAR_SILENT)
                lVol = -10000;
            basicAudio->put_Volume(lVol);
            basicAudio->Release();
        }
    }
}

void DeviceSource::GlobalSourceEnterScene()
{
    if (enteredSceneCount++)
        return;

    float sourceVolume = data->GetFloat(TEXT("volume"), 1.0f);

    if(soundOutputType == 1) {
        audioOut->SetVolume(sourceVolume);
    }
    if(soundOutputType == 2) {
        IBasicAudio *basicAudio;
        if(SUCCEEDED(audioFilter->QueryInterface(IID_IBasicAudio, (void**)&basicAudio)))
        {
            long lVol = long((double(sourceVolume)*NEAR_SILENTf)-NEAR_SILENTf);
            if(lVol <= -NEAR_SILENT)
                lVol = -10000;
            basicAudio->put_Volume(lVol);
            basicAudio->Release();
        }
    }
}

DWORD DeviceSource::SampleThread(DeviceSource *source)
{
    HANDLE hSampleMutex = source->hSampleMutex;
    LONGLONG lastTime = GetQPCTime100NS(), bufferTime = 0, frameWait = 0, curBufferTime = source->bufferTime;
    LONGLONG lastSampleTime = 0;

    bool bFirstFrame = true;
    bool bFirstDelay = true;

    while (WaitForSingleObject(source->hStopSampleEvent, 2) == WAIT_TIMEOUT) {
        LONGLONG t = GetQPCTime100NS();
        LONGLONG delta = t-lastTime;
        lastTime = t;

        OSEnterMutex(hSampleMutex);

        if (source->samples.Num()) {
            if (bFirstFrame) {
                bFirstFrame = false;
                lastSampleTime = source->samples[0]->timestamp;
            }

            //wait until the requested delay has been buffered before processing packets
            if (bufferTime >= source->bufferTime) {
                frameWait += delta;

                //if delay time was adjusted downward, remove packets accordingly
                bool bBufferTimeChanged = (curBufferTime != source->bufferTime);
                if (bBufferTimeChanged) {
                    if (curBufferTime > source->bufferTime) {
                        if (source->audioOut)
                            source->audioOut->FlushSamples();

                        LONGLONG lostTime = curBufferTime - source->bufferTime;
                        bufferTime -= lostTime;

                        if (source->samples.Num()) {
                            LONGLONG startTime = source->samples[0]->timestamp;

                            while (source->samples.Num()) {
                                SampleData *sample = source->samples[0];

                                if ((sample->timestamp - startTime) >= lostTime)
                                    break;

                                lastSampleTime = sample->timestamp;

                                sample->Release();
                                source->samples.Remove(0);
                            }
                        }
                    }

                    curBufferTime = source->bufferTime;
                }

                while (source->samples.Num()) {
                    SampleData *sample = source->samples[0];

                    LONGLONG timestamp = sample->timestamp;
                    LONGLONG sampleTime = timestamp - lastSampleTime;

                    //sometimes timestamps can go to shit with horrible garbage devices.
                    //so, bypass any unusual timestamp offsets.
                    if (sampleTime < -10000000 || sampleTime > 10000000) {
                        //OSDebugOut(TEXT("sample time: %lld\r\n"), sampleTime);
                        sampleTime = 0;
                    }

                    if (frameWait < sampleTime)
                        break;

                    if (sample->bAudio) {
                        if (source->audioOut)
                            source->audioOut->ReceiveAudio(sample->lpData, sample->dataLength);

                        sample->Release();
                    } else {
                        SafeRelease(source->latestVideoSample);
                        source->latestVideoSample = sample;
                    }

                    source->samples.Remove(0);

                    if (sampleTime > 0)
                        frameWait -= sampleTime;

                    lastSampleTime = timestamp;
                }
            }
        }

        OSLeaveMutex(hSampleMutex);

        if (!bFirstFrame && bufferTime < source->bufferTime)
            bufferTime += delta;
    }

    return 0;
}

UINT DeviceSource::GetSampleInsertIndex(LONGLONG timestamp)
{
    UINT index;
    for (index=0; index<samples.Num(); index++) {
        if (samples[index]->timestamp > timestamp)
            return index;
    }

    return index;
}

void DeviceSource::KillThreads()
{
    int numThreads = MAX(OSGetTotalCores()-2, 1);
    for(int i=0; i<numThreads; i++)
    {
        if(hConvertThreads[i])
        {
            convertData[i].bKillThread = true;
            SetEvent(convertData[i].hSignalConvert);

            OSTerminateThread(hConvertThreads[i], 10000);
            hConvertThreads[i] = NULL;
        }

        convertData[i].bKillThread = false;

        if(convertData[i].hSignalConvert)
        {
            CloseHandle(convertData[i].hSignalConvert);
            convertData[i].hSignalConvert = NULL;
        }

        if(convertData[i].hSignalComplete)
        {
            CloseHandle(convertData[i].hSignalComplete);
            convertData[i].hSignalComplete = NULL;
        }
    }
}

void DeviceSource::ChangeSize(bool bSucceeded, bool bForce)
{
    if (!bForce && renderCX == newCX && renderCY == newCY)
        return;

    renderCX = newCX;
    renderCY = newCY;

    switch(colorType) {
    case DeviceOutputType_RGB:
        lineSize = renderCX * 4;
        break;
    case DeviceOutputType_I420:
    case DeviceOutputType_YV12:
        lineSize = renderCX; //per plane
        break;
    case DeviceOutputType_YVYU:
    case DeviceOutputType_YUY2:
    case DeviceOutputType_UYVY:
    case DeviceOutputType_HDYC:
        lineSize = (renderCX * 2);
        break;
    }

    linePitch = lineSize;
    lineShift = 0;
    imageCX = renderCX;
    imageCY = renderCY;

    deinterlacer.imageCX = renderCX;
    deinterlacer.imageCY = renderCY;

    if(deinterlacer.doublesFramerate)
        deinterlacer.imageCX *= 2;

    switch(deinterlacer.type) {
    case DEINTERLACING_DISCARD:
        deinterlacer.imageCY = renderCY/2;
        linePitch = lineSize * 2;
        renderCY /= 2;
        break;

    case DEINTERLACING_RETRO:
        deinterlacer.imageCY = renderCY/2;
        if(deinterlacer.processor != DEINTERLACING_PROCESSOR_GPU)
        {
            lineSize *= 2;
            linePitch = lineSize;
            renderCY /= 2;
            renderCX *= 2;
        }
        break;

    case DEINTERLACING__DEBUG:
        deinterlacer.imageCX *= 2;
        deinterlacer.imageCY *= 2;
    case DEINTERLACING_BLEND2x:
    //case DEINTERLACING_MEAN2x:
    case DEINTERLACING_YADIF:
    case DEINTERLACING_YADIF2x:
        deinterlacer.needsPreviousFrame = true;
        break;
    }

    if(deinterlacer.type != DEINTERLACING_NONE && deinterlacer.processor == DEINTERLACING_PROCESSOR_GPU)
    {
        deinterlacer.vertexShader.reset(CreateVertexShaderFromFile(TEXT("shaders/DrawTexture.vShader")));
        deinterlacer.pixelShader = CreatePixelShaderFromFileAsync(ChooseDeinterlacingShader());
        deinterlacer.isReady = false;
    }

    KillThreads();

    int numThreads = MAX(OSGetTotalCores()-2, 1);
    for(int i=0; i<numThreads; i++)
    {
        convertData[i].width  = lineSize;
        convertData[i].height = renderCY;
        convertData[i].sample = NULL;
        convertData[i].hSignalConvert  = CreateEvent(NULL, FALSE, FALSE, NULL);
        convertData[i].hSignalComplete = CreateEvent(NULL, FALSE, FALSE, NULL);
        convertData[i].linePitch = linePitch;
        convertData[i].lineShift = lineShift;

        if(i == 0)
            convertData[i].startY = 0;
        else
            convertData[i].startY = convertData[i-1].endY;

        if(i == (numThreads-1))
            convertData[i].endY = renderCY;
        else
            convertData[i].endY = ((renderCY/numThreads)*(i+1)) & 0xFFFFFFFE;
    }

    if(colorType == DeviceOutputType_YV12 || colorType == DeviceOutputType_I420)
    {
        for(int i=0; i<numThreads; i++)
            hConvertThreads[i] = OSCreateThread((XTHREAD)PackPlanarThread, convertData+i);
    }

    if(texture)
    {
        delete texture;
        texture = NULL;
    }
    if(previousTexture)
    {
        delete previousTexture;
        previousTexture = NULL;
    }

    //-----------------------------------------------------
    // create the texture regardless, will just show up as red to indicate failure
    BYTE *textureData = (BYTE*)Allocate(renderCX*renderCY*4);

    if(colorType == DeviceOutputType_RGB) //you may be confused, but when directshow outputs RGB, it's actually outputting BGR
    {
        msetd(textureData, 0xFFFF0000, renderCX*renderCY*4);
        texture = CreateTexture(renderCX, renderCY, GS_BGR, textureData, FALSE, FALSE);
        if(bSucceeded && deinterlacer.needsPreviousFrame)
            previousTexture = CreateTexture(renderCX, renderCY, GS_BGR, textureData, FALSE, FALSE);
        if(bSucceeded && deinterlacer.processor == DEINTERLACING_PROCESSOR_GPU)
            deinterlacer.texture.reset(CreateRenderTarget(deinterlacer.imageCX, deinterlacer.imageCY, GS_BGRA, FALSE));
    }
    else //if we're working with planar YUV, we can just use regular RGB textures instead
    {
        msetd(textureData, 0xFF0000FF, renderCX*renderCY*4);
        texture = CreateTexture(renderCX, renderCY, GS_RGB, textureData, FALSE, FALSE);
        if(bSucceeded && deinterlacer.needsPreviousFrame)
            previousTexture = CreateTexture(renderCX, renderCY, GS_RGB, textureData, FALSE, FALSE);
        if(bSucceeded && deinterlacer.processor == DEINTERLACING_PROCESSOR_GPU)
            deinterlacer.texture.reset(CreateRenderTarget(deinterlacer.imageCX, deinterlacer.imageCY, GS_BGRA, FALSE));
    }

    if(bSucceeded && bUseThreadedConversion)
    {
        if(colorType == DeviceOutputType_I420 || colorType == DeviceOutputType_YV12)
        {
            LPBYTE lpData;
            if(texture->Map(lpData, texturePitch))
                texture->Unmap();
            else
                texturePitch = renderCX*4;

            lpImageBuffer = (LPBYTE)Allocate(texturePitch*renderCY);
        }
    }

    Free(textureData);

    bFiltersLoaded = bSucceeded;
}

void DeviceSource::ReceiveMediaSample(IMediaSample *sample, bool bAudio)
{
    if (!sample)
        return;

    if (bCapturing) {
        BYTE *pointer;

        if (!sample->GetActualDataLength())
            return;

        if (SUCCEEDED(sample->GetPointer(&pointer))) {
            SampleData *data = NULL;
            AM_MEDIA_TYPE *mt = nullptr;

            if (sample->GetMediaType(&mt) == S_OK)
            {
                BITMAPINFOHEADER *bih = GetVideoBMIHeader(mt);
                newCX = bih->biWidth;
                newCY = bih->biHeight;
                DeleteMediaType(mt);
            }

            if (bUseBuffering || !bAudio) {
                data = new SampleData;
                data->bAudio = bAudio;
                data->dataLength = sample->GetActualDataLength();
                data->lpData = (LPBYTE)Allocate(data->dataLength);//pointer; //
                data->cx = newCX;
                data->cy = newCY;
                /*data->sample = sample;
                sample->AddRef();*/

                memcpy(data->lpData, pointer, data->dataLength);

                LONGLONG stopTime;
                sample->GetTime(&stopTime, &data->timestamp);
            }

            //Log(TEXT("timestamp: %lld, bAudio - %s"), data->timestamp, bAudio ? TEXT("true") : TEXT("false"));

            OSEnterMutex(hSampleMutex);

            if (bUseBuffering) {
                UINT id = GetSampleInsertIndex(data->timestamp);
                samples.Insert(id, data);
            } else if (bAudio) {
                if (audioOut)
                    audioOut->ReceiveAudio(pointer, sample->GetActualDataLength());
            } else {
                SafeRelease(latestVideoSample);
                latestVideoSample = data;
            }

            OSLeaveMutex(hSampleMutex);
        }
    }
}

static DWORD STDCALL PackPlanarThread(ConvertData *data)
{
    do {
        WaitForSingleObject(data->hSignalConvert, INFINITE);
        if(data->bKillThread) break;

        PackPlanar(data->output, data->input, data->width, data->height, data->pitch, data->startY, data->endY, data->linePitch, data->lineShift);
        data->sample->Release();

        SetEvent(data->hSignalComplete);
    }while(!data->bKillThread);

    return 0;
}

void DeviceSource::Preprocess()
{
    if(!bCapturing)
        return;

    //----------------------------------------

    if(bRequestVolume)
    {
        if(audioOut)
            audioOut->SetVolume(fNewVol);
        else if(audioFilter)
        {
            IBasicAudio *basicAudio;
            if(SUCCEEDED(audioFilter->QueryInterface(IID_IBasicAudio, (void**)&basicAudio)))
            {
                long lVol = long((double(fNewVol)*NEAR_SILENTf)-NEAR_SILENTf);
                if(lVol <= -NEAR_SILENT)
                    lVol = -10000;
                basicAudio->put_Volume(lVol);
                basicAudio->Release();
            }
        }
        bRequestVolume = false;
    }

    //----------------------------------------

    SampleData *lastSample = NULL;

    OSEnterMutex(hSampleMutex);

    lastSample = latestVideoSample;
    latestVideoSample = NULL;

    OSLeaveMutex(hSampleMutex);

    //----------------------------------------

    int numThreads = MAX(OSGetTotalCores()-2, 1);

    if(lastSample)
    {
        /*REFERENCE_TIME refTimeStart, refTimeFinish;
        lastSample->GetTime(&refTimeStart, &refTimeFinish);

        static REFERENCE_TIME lastRefTime = 0;
        Log(TEXT("refTimeStart: %llu, refTimeFinish: %llu, offset = %llu"), refTimeStart, refTimeFinish, refTimeStart-lastRefTime);
        lastRefTime = refTimeStart;*/

        if(previousTexture)
        {
            Texture *tmp = texture;
            texture = previousTexture;
            previousTexture = tmp;
        }
        deinterlacer.curField = deinterlacer.processor == DEINTERLACING_PROCESSOR_GPU ? false : (deinterlacer.fieldOrder == FIELD_ORDER_BFF);
        deinterlacer.bNewFrame = true;
 
        if(colorType == DeviceOutputType_RGB)
        {
            if(texture)
            {
                ChangeSize();

                texture->SetImage(lastSample->lpData, GS_IMAGEFORMAT_BGRX, linePitch);
                bReadyToDraw = true;
            }
        }
        else if(colorType == DeviceOutputType_I420 || colorType == DeviceOutputType_YV12)
        {
            if(bUseThreadedConversion)
            {
                if(!bFirstFrame)
                {
                    List<HANDLE> events;
                    for(int i=0; i<numThreads; i++)
                        events << convertData[i].hSignalComplete;

                    WaitForMultipleObjects(numThreads, events.Array(), TRUE, INFINITE);
                    texture->SetImage(lpImageBuffer, GS_IMAGEFORMAT_RGBX, texturePitch);

                    bReadyToDraw = true;
                }
                else
                    bFirstFrame = false;

                ChangeSize();

                for(int i=0; i<numThreads; i++)
                    lastSample->AddRef();

                for(int i=0; i<numThreads; i++)
                {
                    convertData[i].input     = lastSample->lpData;
                    convertData[i].sample    = lastSample;
                    convertData[i].pitch     = texturePitch;
                    convertData[i].output    = lpImageBuffer;
                    convertData[i].linePitch = linePitch;
                    convertData[i].lineShift = lineShift;
                    SetEvent(convertData[i].hSignalConvert);
                }
            }
            else
            {
                LPBYTE lpData;
                UINT pitch;

                ChangeSize();

                if(texture->Map(lpData, pitch))
                {
                    PackPlanar(lpData, lastSample->lpData, renderCX, renderCY, pitch, 0, renderCY, linePitch, lineShift);
                    texture->Unmap();
                }

                bReadyToDraw = true;
            }
        }
        else if(colorType == DeviceOutputType_YVYU || colorType == DeviceOutputType_YUY2)
        {
            LPBYTE lpData;
            UINT pitch;

            ChangeSize();

            if(texture->Map(lpData, pitch))
            {
                Convert422To444(lpData, lastSample->lpData, pitch, true);
                texture->Unmap();
            }

            bReadyToDraw = true;
        }
        else if(colorType == DeviceOutputType_UYVY || colorType == DeviceOutputType_HDYC)
        {
            LPBYTE lpData;
            UINT pitch;

            ChangeSize();

            if(texture->Map(lpData, pitch))
            {
                Convert422To444(lpData, lastSample->lpData, pitch, false);
                texture->Unmap();
            }

            bReadyToDraw = true;
        }

        lastSample->Release();

        if (bReadyToDraw &&
            deinterlacer.type != DEINTERLACING_NONE &&
            deinterlacer.processor == DEINTERLACING_PROCESSOR_GPU &&
            deinterlacer.texture.get() &&
            deinterlacer.pixelShader.Shader())
        {
            SetRenderTarget(deinterlacer.texture.get());

            Shader *oldVertShader = GetCurrentVertexShader();
            LoadVertexShader(deinterlacer.vertexShader.get());
            
            Shader *oldShader = GetCurrentPixelShader();
            LoadPixelShader(deinterlacer.pixelShader.Shader());

            HANDLE hField = deinterlacer.pixelShader.Shader()->GetParameterByName(TEXT("field_order"));
            if(hField)
                deinterlacer.pixelShader.Shader()->SetBool(hField, deinterlacer.fieldOrder == FIELD_ORDER_BFF);
            
            Ortho(0.0f, float(deinterlacer.imageCX), float(deinterlacer.imageCY), 0.0f, -100.0f, 100.0f);
            SetViewport(0.0f, 0.0f, float(deinterlacer.imageCX), float(deinterlacer.imageCY));

            if(previousTexture)
                LoadTexture(previousTexture, 1);

            DrawSpriteEx(texture, 0xFFFFFFFF, 0.0f, 0.0f, float(deinterlacer.imageCX), float(deinterlacer.imageCY), 0.0f, 0.0f, 1.0f, 1.0f);

            if(previousTexture)
                LoadTexture(nullptr, 1);

            LoadPixelShader(oldShader);
            LoadVertexShader(oldVertShader);
            deinterlacer.isReady = true;
        }
    }
}

void DeviceSource::Render(const Vect2 &pos, const Vect2 &size)
{
    if(texture && bReadyToDraw && deinterlacer.isReady)
    {
        Shader *oldShader = GetCurrentPixelShader();
        SamplerState *sampler = NULL;

        gamma = data->GetInt(TEXT("gamma"), 100);
        float fGamma = float(-(gamma-100) + 100) * 0.01f;

        if(colorConvertShader)
        {
            LoadPixelShader(colorConvertShader);
            if(bUseChromaKey)
            {
                float fSimilarity = float(keySimilarity)/1000.0f;
                float fBlendVal = float(max(keyBlend, 1)/1000.0f);
                float fSpillVal = (float(max(keySpillReduction, 1))/1000.0f);

                Vect2 pixelSize = 1.0f/GetSize();

                colorConvertShader->SetColor  (colorConvertShader->GetParameterByName(TEXT("keyBaseColor")),    Color4(keyBaseColor));
                colorConvertShader->SetColor  (colorConvertShader->GetParameterByName(TEXT("chromaKey")),       Color4(keyChroma));
                colorConvertShader->SetVector2(colorConvertShader->GetParameterByName(TEXT("pixelSize")),       pixelSize);
                colorConvertShader->SetFloat  (colorConvertShader->GetParameterByName(TEXT("keySimilarity")),   fSimilarity);
                colorConvertShader->SetFloat  (colorConvertShader->GetParameterByName(TEXT("keyBlend")),        fBlendVal);
                colorConvertShader->SetFloat  (colorConvertShader->GetParameterByName(TEXT("keySpill")),        fSpillVal);
            }
            colorConvertShader->SetFloat  (colorConvertShader->GetParameterByName(TEXT("gamma")),           fGamma);

            float mat[16];
            bool actuallyUse709 = (colorSpace == COLORSPACE_AUTO) ? !!use709 : (colorSpace == COLORSPACE_709);

            if (actuallyUse709)
                memcpy(mat, yuvToRGB709[fullRange ? 1 : 0], sizeof(float) * 16);
            else
                memcpy(mat, yuvToRGB601[fullRange ? 1 : 0], sizeof(float) * 16);

            colorConvertShader->SetValue  (colorConvertShader->GetParameterByName(TEXT("yuvMat")), mat, sizeof(float) * 16);
        }
        else {
            if(fGamma != 1.0f && bFiltersLoaded) {
                LoadPixelShader(drawShader);
                HANDLE hGamma = drawShader->GetParameterByName(TEXT("gamma"));
                if(hGamma)
                    drawShader->SetFloat(hGamma, fGamma);
            }
        }

        bool bFlip = bFlipVertical;

        if(colorType != DeviceOutputType_RGB)
            bFlip = !bFlip;

        float x, x2;
        if(bFlipHorizontal)
        {
            x2 = pos.x;
            x = x2+size.x;
        }
        else
        {
            x = pos.x;
            x2 = x+size.x;
        }

        float y = pos.y,
              y2 = y+size.y;
        if(!bFlip)
        {
            y2 = pos.y;
            y = y2+size.y;
        }

        float fOpacity = float(opacity)*0.01f;
        DWORD opacity255 = DWORD(fOpacity*255.0f);

        if(bUsePointFiltering) {
            SamplerInfo samplerinfo;
            samplerinfo.filter = GS_FILTER_POINT;
            sampler = CreateSamplerState(samplerinfo);
            LoadSamplerState(sampler, 0);
        }


        Texture *tex = (deinterlacer.processor == DEINTERLACING_PROCESSOR_GPU && deinterlacer.texture.get()) ? deinterlacer.texture.get() : texture;
        if(deinterlacer.doublesFramerate)
        {
            if(!deinterlacer.curField)
                DrawSpriteEx(tex, (opacity255<<24) | 0xFFFFFF, x, y, x2, y2, 0.f, 0.0f, .5f, 1.f);
            else
                DrawSpriteEx(tex, (opacity255<<24) | 0xFFFFFF, x, y, x2, y2, .5f, 0.0f, 1.f, 1.f);
        }
        else
            DrawSprite(tex, (opacity255<<24) | 0xFFFFFF, x, y, x2, y2);
        if(deinterlacer.bNewFrame)
        {
            deinterlacer.curField = !deinterlacer.curField;
            deinterlacer.bNewFrame = false; //prevent switching from the second field to the first field
        }

        if(bUsePointFiltering) delete(sampler);

        if(colorConvertShader || fGamma != 1.0f)
            LoadPixelShader(oldShader);
    }
}

void DeviceSource::UpdateSettings()
{
    String strNewDevice         = data->GetString(TEXT("device"));
    String strNewAudioDevice    = data->GetString(TEXT("audioDevice"));
    UINT64 newFrameInterval     = data->GetInt(TEXT("frameInterval"));
    UINT newCX                  = data->GetInt(TEXT("resolutionWidth"));
    UINT newCY                  = data->GetInt(TEXT("resolutionHeight"));
    BOOL bNewCustom             = data->GetInt(TEXT("customResolution"));
    UINT newPreferredType       = data->GetInt(TEXT("usePreferredType")) != 0 ? data->GetInt(TEXT("preferredType")) : -1;
    UINT newSoundOutputType     = data->GetInt(TEXT("soundOutputType"));
    bool bNewUseBuffering       = data->GetInt(TEXT("useBuffering")) != 0;
    bool bNewUseAudioRender     = data->GetInt(TEXT("useAudioRender")) != 0;
    UINT newGamma               = data->GetInt(TEXT("gamma"), 100);

    int newDeintType            = data->GetInt(TEXT("deinterlacingType"));
    int newDeintFieldOrder      = data->GetInt(TEXT("deinterlacingFieldOrder"));
    int newDeintProcessor       = data->GetInt(TEXT("deinterlacingProcessor"));

    UINT64 frameIntervalDiff = 0;
    bool bCheckSoundOutput = true;

    if(newFrameInterval > frameInterval)
        frameIntervalDiff = newFrameInterval - frameInterval;
    else
        frameIntervalDiff = frameInterval - newFrameInterval;

    fullRange = data->GetInt(TEXT("fullrange")) != 0;
    colorSpace = data->GetInt(TEXT("colorspace"));

    if(strNewAudioDevice == "Disable" && strAudioDevice == "Disable")
        bCheckSoundOutput = false;

    bool elgato = sstri(strNewDevice.Array(), L"elgato") != nullptr;

    if(elgato || (bNewUseAudioRender != bUseAudioRender && bCheckSoundOutput) ||
       (newSoundOutputType != soundOutputType && bCheckSoundOutput) || imageCX != newCX || imageCY != newCY ||
       frameIntervalDiff >= 10 || newPreferredType != preferredOutputType ||
       !strDevice.CompareI(strNewDevice) || !strAudioDevice.CompareI(strNewAudioDevice) ||
       bNewCustom != bUseCustomResolution || bNewUseBuffering != bUseBuffering ||
       newGamma != gamma || newDeintType != deinterlacer.type ||
       newDeintFieldOrder != deinterlacer.fieldOrder || newDeintProcessor != deinterlacer.processor)
    {
        API->EnterSceneMutex();

        bool bWasCapturing = bCapturing;
        if(bWasCapturing) Stop();

        UnloadFilters();
        LoadFilters();

        if(bWasCapturing) Start();

        API->LeaveSceneMutex();
    }
}

void DeviceSource::SetInt(CTSTR lpName, int iVal)
{
    if(bCapturing)
    {
        if(scmpi(lpName, TEXT("useChromaKey")) == 0)
        {
            bool bNewVal = iVal != 0;
            if(bUseChromaKey != bNewVal)
            {
                API->EnterSceneMutex();
                bUseChromaKey = bNewVal;

                if(colorConvertShader)
                {
                    delete colorConvertShader;
                    colorConvertShader = NULL;
                }

                String strShader;
                strShader = ChooseShader();

                if(strShader.IsValid())
                    colorConvertShader = CreatePixelShaderFromFile(strShader);

                API->LeaveSceneMutex();
            }
        }
        else if(scmpi(lpName, TEXT("flipImage")) == 0)
        {
            bFlipVertical = iVal != 0;
        }
        else if(scmpi(lpName, TEXT("flipImageHorizontal")) == 0)
        {
            bFlipHorizontal = iVal != 0;
        }
        else if(scmpi(lpName, TEXT("usePointFiltering")) == 0)
        {
            bUsePointFiltering = iVal != 0;
        }
        else if(scmpi(lpName, TEXT("keyColor")) == 0)
        {
            keyColor = (DWORD)iVal;

            keyBaseColor = Color4().MakeFromRGBA(keyColor);
            Matrix4x4TransformVect(keyChroma, (colorType == DeviceOutputType_HDYC || colorType == DeviceOutputType_RGB) ? (float*)yuv709Mat : (float*)yuvMat, keyBaseColor);
            keyChroma *= 2.0f;

            if(keyBaseColor.x < keyBaseColor.y && keyBaseColor.x < keyBaseColor.z)
                keyBaseColor -= keyBaseColor.x;
            else if(keyBaseColor.y < keyBaseColor.x && keyBaseColor.y < keyBaseColor.z)
                keyBaseColor -= keyBaseColor.y;
            else if(keyBaseColor.z < keyBaseColor.x && keyBaseColor.z < keyBaseColor.y)
                keyBaseColor -= keyBaseColor.z;
        }
        else if(scmpi(lpName, TEXT("keySimilarity")) == 0)
        {
            keySimilarity = iVal;
        }
        else if(scmpi(lpName, TEXT("keyBlend")) == 0)
        {
            keyBlend = iVal;
        }
        else if(scmpi(lpName, TEXT("keySpillReduction")) == 0)
        {
            keySpillReduction = iVal;
        }
        else if(scmpi(lpName, TEXT("opacity")) == 0)
        {
            opacity = iVal;
        }
        else if(scmpi(lpName, TEXT("timeOffset")) == 0)
        {
            if(audioOut)
                audioOut->SetAudioOffset(iVal);
        }
        else if(scmpi(lpName, TEXT("bufferTime")) == 0)
        {
            bufferTime = iVal*10000;
        }
    }
}

void DeviceSource::SetFloat(CTSTR lpName, float fValue)
{
    if(!bCapturing)
        return;

    if(scmpi(lpName, TEXT("volume")) == 0)
    {
        fNewVol = fValue;
        bRequestVolume = true;
    }
}
