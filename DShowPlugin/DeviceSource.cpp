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


bool DeviceSource::Init(XElement *data)
{
    traceIn(DeviceSource::Init);

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
        AppWarning(TEXT("DShowPlugin: could not create sasmple mutex"));
        return false;
    }

    capture->SetFiltergraph(graph);

    this->data = data;
    UpdateSettings();

    //if(!bFiltersLoaded)
    //    return false;

    return true;

    traceOut;
}

DeviceSource::~DeviceSource()
{
    traceIn(DeviceSource::~DeviceSource);

    Stop();
    UnloadFilters();

    SafeRelease(capture);
    SafeRelease(graph);

    if(hSampleMutex)
        OSCloseMutex(hSampleMutex);

    traceOut;
}

bool DeviceSource::LoadFilters()
{
    traceIn(DeviceSource::LoadFilters);

    if(bCapturing || bFiltersLoaded)
        return false;

    bool bSucceeded = false;

    List<MediaOutputInfo> outputList;
    IAMStreamConfig *config = NULL;
    bool bAddedCapture = false, bAddedDevice = false;
    GUID expectedMediaType;
    IPin *devicePin = NULL;
    HRESULT err;

    String strDevice = data->GetString(TEXT("device"));
    UINT cx = data->GetInt(TEXT("resolutionWidth"));
    UINT cy = data->GetInt(TEXT("resolutionHeight"));
    UINT fps = data->GetInt(TEXT("fps"));

    bFlipVertical = data->GetInt(TEXT("flipImage")) != 0;

    renderCX = cx;
    renderCY = cy;

    if(!strDevice.IsValid() || !cx || !cy || !fps)
    {
        AppWarning(TEXT("DShowPlugin: Invalid device/size/fps specified"));
        goto cleanFinish;
    }

    deviceFilter = GetDeviceByName(strDevice);
    if(!deviceFilter)
    {
        AppWarning(TEXT("DShowPlugin: Could not create device filter"));
        goto cleanFinish;
    }

    devicePin = GetOutputPin(deviceFilter);
    if(!devicePin)
    {
        AppWarning(TEXT("DShowPlugin: Could not create device ping"));
        goto cleanFinish;
    }

    GetOutputList(devicePin, outputList);

    MediaOutputInfo *bestOutput = GetBestMediaOutput(outputList, cx, cy, fps);
    if(!bestOutput)
    {
        AppWarning(TEXT("DShowPlugin: Could not find appropriate resolution to create device image source"));
        goto cleanFinish;
    }

    expectedMediaType = bestOutput->mediaType->subtype;

    if(bestOutput->videoType == VideoOutputType_I420)
    {
        colorConvertShader = CreatePixelShaderFromFile(TEXT("plugins/DShowPlugin/shaders/YUVToRGB.pShader"));
        colorType = DeviceOutputType_I420;
    }
    else if(bestOutput->videoType == VideoOutputType_YV12)
    {
        colorConvertShader = CreatePixelShaderFromFile(TEXT("plugins/DShowPlugin/shaders/YVUToRGB.pShader"));
        colorType = DeviceOutputType_YV12;
    }
    else
    {
        colorType = DeviceOutputType_RGB;
        expectedMediaType = MEDIASUBTYPE_RGB32;
    }

    if(colorType != DeviceOutputType_RGB && !colorConvertShader)
    {
        AppWarning(TEXT("DShowPlugin: Could not create color space conversion pixel shader"));
        goto cleanFinish;
    }

    if(FAILED(err = devicePin->QueryInterface(IID_IAMStreamConfig, (void**)&config)))
    {
        AppWarning(TEXT("DShowPlugin: Could not get IAMStreamConfig for device pin, result = %08lX"), err);
        goto cleanFinish;
    }

    AM_MEDIA_TYPE outputMediaType;
    CopyMediaType(&outputMediaType, bestOutput->mediaType);

    VIDEOINFOHEADER *vih = reinterpret_cast<VIDEOINFOHEADER*>(outputMediaType.pbFormat);
    vih->AvgTimePerFrame = QWORD((1000.0/double(fps))*10000.0);
    vih->bmiHeader.biWidth  = renderCX;
    vih->bmiHeader.biHeight = renderCY;
    vih->bmiHeader.biSizeImage = renderCX*renderCY*(vih->bmiHeader.biBitCount>>3);

    if(FAILED(err = config->SetFormat(&outputMediaType)))
    {
        AppWarning(TEXT("DShowPlugin: SetFormat on device pin failed, result = %08lX"), err);
        goto cleanFinish;
    }

    FreeMediaType(outputMediaType);

    captureFilter = new CaptureFilter(this, expectedMediaType);

    if(FAILED(err = graph->AddFilter(captureFilter, NULL)))
    {
        AppWarning(TEXT("DShowPlugin: Failed to add capture filter to graph, result = %08lX"), err);
        goto cleanFinish;
    }

    bAddedCapture = true;

    if(FAILED(err = graph->AddFilter(deviceFilter, NULL)))
    {
        AppWarning(TEXT("DShowPlugin: Failed to add device filter to graph, result = %08lX"), err);
        goto cleanFinish;
    }

    bAddedDevice = true;

    //THANK THE NINE DIVINES I FINALLY GOT IT WORKING
    if(FAILED(err = capture->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, deviceFilter, NULL, captureFilter)))
    {
        if(FAILED(err = graph->Connect(devicePin, captureFilter->GetCapturePin())))
        {
            AppWarning(TEXT("DShowPlugin: Failed to connect the device pin to the capture pin, result = %08lX"), err);
            goto cleanFinish;
        }
    }

    if(FAILED(err = graph->QueryInterface(IID_IMediaControl, (void**)&control)))
    {
        AppWarning(TEXT("DShowPlugin: Failed to get IMediaControl, result = %08lX"), err);
        goto cleanFinish;
    }

    bSucceeded = true;

cleanFinish:
    SafeRelease(config);
    SafeRelease(devicePin);

    for(UINT i=0; i<outputList.Num(); i++)
        outputList[i].FreeData();

    if(!bSucceeded)
    {
        bCapturing = false;

        if(bAddedCapture)
            graph->RemoveFilter(captureFilter);
        if(bAddedDevice)
            graph->RemoveFilter(deviceFilter);

        SafeRelease(deviceFilter);
        SafeRelease(captureFilter);

        if(colorConvertShader)
        {
            delete colorConvertShader;
            colorConvertShader = NULL;
        }

        bReadyToDraw = true;
    }
    else
        bReadyToDraw = false;

    //-----------------------------------------------------
    // create the texture regardless, will just show up as red to indicate failure
    BYTE *textureData = (BYTE*)Allocate(cx*cy*4);

    if(colorType == DeviceOutputType_RGB) //you may be confused, but when directshow outputs RGB, it's actually outputting BGR
    {
        msetd(textureData, 0xFFFF0000, cx*cy*4);
        texture = CreateTexture(cx, cy, GS_BGR, textureData, FALSE, FALSE);
    }
    else //if we're working with planar YUV, we can just use regular RGB textures instead
    {
        msetd(textureData, 0xFF0000FF, cx*cy*4);
        texture = CreateTexture(cx, cy, GS_RGB, textureData, FALSE, FALSE);
    }

    Free(textureData);

    bFiltersLoaded = bSucceeded;
    return bSucceeded;

    traceOut;
}

void DeviceSource::UnloadFilters()
{
    traceIn(DeviceSource::UnloadFilters);

    if(texture)
    {
        delete texture;
        texture = NULL;
    }

    if(bFiltersLoaded)
    {
        graph->RemoveFilter(captureFilter);
        graph->RemoveFilter(deviceFilter);

        SafeRelease(captureFilter);
        SafeRelease(deviceFilter);

        bFiltersLoaded = false;
    }

    if(colorConvertShader)
    {
        delete colorConvertShader;
        colorConvertShader = NULL;
    }

    SafeRelease(control);

    traceOut;
}

void DeviceSource::Start()
{
    traceIn(DeviceSource::Start);

    if(bCapturing)
        return;

    HRESULT err;
    if(FAILED(err = control->Run()))
    {
        AppWarning(TEXT("DShowPlugin: control->Run failed, result = %08lX"), err);
        return;
    }

    bCapturing = true;

    traceOut;
}

void DeviceSource::Stop()
{
    traceIn(DeviceSource::Stop);

    if(!bCapturing)
        return;

    bCapturing = false;
    control->Stop();
    FlushSamples();

    traceOut;
}

void DeviceSource::BeginScene()
{
    traceIn(DeviceSource::BeginScene);

    Start();

    traceOut;
}

void DeviceSource::EndScene()
{
    traceIn(DeviceSource::EndScene);

    Stop();

    traceOut;
}

void DeviceSource::Receive(IMediaSample *sample)
{
    if(bCapturing)
    {
        OSEnterMutex(hSampleMutex);

        SafeRelease(curSample);
        curSample = sample;
        curSample->AddRef();

        OSLeaveMutex(hSampleMutex);
    }
}

void DeviceSource::Preprocess()
{
    traceIn(DeviceSource::Preprocess);

    if(!bCapturing)
        return;

    IMediaSample *lastSample = NULL;

    OSEnterMutex(hSampleMutex);
    if(curSample)
    {
        lastSample = curSample;
        curSample = NULL;
    }
    OSLeaveMutex(hSampleMutex);

    if(lastSample)
    {
        BYTE *lpImage = NULL;
        if(colorType == DeviceOutputType_RGB)
        {
            if(texture)
            {
                if(SUCCEEDED(lastSample->GetPointer(&lpImage)))
                    texture->SetImage(lpImage, GS_IMAGEFORMAT_BGRX, renderCX*4);
            }
        }
        else if(colorType == DeviceOutputType_I420 || colorType == DeviceOutputType_YV12)
        {
            if(SUCCEEDED(lastSample->GetPointer(&lpImage)))
            {
                LPBYTE lpData;
                UINT pitch;

                if(texture->Map(lpData, pitch))
                {
                    PackPlanar(lpData, lpImage);
                    texture->Unmap();
                }
            }
        }

        lastSample->Release();

        bReadyToDraw = true;
    }

    traceOut;
}

void DeviceSource::Render(const Vect2 &pos, const Vect2 &size)
{
    traceIn(DeviceSource::Render);

    if(texture && bReadyToDraw)
    {
        Shader *oldShader = GetCurrentPixelShader();
        if(colorConvertShader)
            LoadPixelShader(colorConvertShader);

        bool bFlip = bFlipVertical;

        if(colorType != DeviceOutputType_RGB)
            bFlip = !bFlip;

        if(bFlip)
            DrawSprite(texture, 0xFFFFFFFF, pos.x, pos.y, pos.x+size.x, pos.y+size.y);
        else
            DrawSprite(texture, 0xFFFFFFFF, pos.x, pos.y+size.y, pos.x+size.x, pos.y);

        if(colorConvertShader)
            LoadPixelShader(oldShader);
    }

    traceOut;
}

void DeviceSource::UpdateSettings()
{
    traceIn(DeviceSource::UpdateSettings);

    bool bWasCapturing = bCapturing;

    if(bWasCapturing) Stop();

    UnloadFilters();
    LoadFilters();

    if(bWasCapturing) Start();

    traceOut;
}


