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
#include <Avrt.h>

VideoEncoder* CreateX264Encoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, int maxBitRate, int bufferSize, bool bUseCFR);
AudioEncoder* CreateMP3Encoder(UINT bitRate);
AudioEncoder* CreateAACEncoder(UINT bitRate);

AudioSource* CreateAudioSource(bool bMic, CTSTR lpID);

//NetworkStream* CreateRTMPServer();
NetworkStream* CreateRTMPPublisher();
NetworkStream* CreateDelayedPublisher(DWORD delayTime);
NetworkStream* CreateBandwidthAnalyzer();

void StartBlankSoundPlayback(CTSTR lpDevice);
void StopBlankSoundPlayback();

VideoEncoder* CreateNullVideoEncoder();
AudioEncoder* CreateNullAudioEncoder();
NetworkStream* CreateNullNetwork();

VideoFileStream* CreateMP4FileStream(CTSTR lpFile);
VideoFileStream* CreateFLVFileStream(CTSTR lpFile);
//VideoFileStream* CreateAVIFileStream(CTSTR lpFile);


BOOL bLoggedSystemStats = FALSE;
void LogSystemStats();

void OBS::ToggleCapturing()
{
    if(!bRunning)
        Start();
    else
        Stop();
}

void OBS::Start()
{
    if(bRunning) return;

    OSEnterMutex (hStartupShutdownMutex);

    scenesConfig.Save();

    //-------------------------------------------------------------

    fps = AppConfig->GetInt(TEXT("Video"), TEXT("FPS"), 30);
    frameTime = 1000/fps;

    //-------------------------------------------------------------

    if(!bLoggedSystemStats)
    {
        LogSystemStats();
        bLoggedSystemStats = TRUE;
    }

    //-------------------------------------------------------------

    if (OSIncompatibleModulesLoaded())
    {
        OSLeaveMutex (hStartupShutdownMutex);
        MessageBox(hwndMain, Str("IncompatibleModules"), NULL, MB_ICONERROR);
        Log(TEXT("Incompatible modules detected."));
        return;
    }

    String strPatchesError;
    if (OSIncompatiblePatchesLoaded(strPatchesError))
    {
        OSLeaveMutex (hStartupShutdownMutex);
        MessageBox(hwndMain, strPatchesError.Array(), NULL, MB_ICONERROR);
        Log(TEXT("Incompatible patches detected."));
        return;
    }

    //-------------------------------------------------------------

    int networkMode = AppConfig->GetInt(TEXT("Publish"), TEXT("Mode"), 2);
    DWORD delayTime = (DWORD)AppConfig->GetInt(TEXT("Publish"), TEXT("Delay"));

    String strError;

    if(bTestStream)
        network = CreateNullNetwork();
    else
    {
        switch(networkMode)
        {
        case 0: network = (delayTime > 0) ? CreateDelayedPublisher(delayTime) : CreateRTMPPublisher(); break;
        case 1: network = CreateNullNetwork(); break;
        }
    }

    if(!network)
    {
        OSLeaveMutex (hStartupShutdownMutex);

        if(!bReconnecting)
            MessageBox(hwndMain, strError, NULL, MB_ICONERROR);
        else
            DialogBox(hinstMain, MAKEINTRESOURCE(IDD_RECONNECTING), hwndMain, OBS::ReconnectDialogProc);
        return;
    }

    bReconnecting = false;

    //-------------------------------------------------------------

    Log(TEXT("=====Stream Start====================================================================="));

    //-------------------------------------------------------------

    bufferingTime = GlobalConfig->GetInt(TEXT("General"), TEXT("SceneBufferingTime"), 400);
    numRenderBuffers = GlobalConfig->GetInt(TEXT("General"), TEXT("UseTripleBuffering"), FALSE) ? 2 : 3;

    int monitorID = AppConfig->GetInt(TEXT("Video"), TEXT("Monitor"));
    if(monitorID >= (int)monitors.Num())
        monitorID = 0;

    RECT &screenRect = monitors[monitorID].rect;
    int defCX = screenRect.right  - screenRect.left;
    int defCY = screenRect.bottom - screenRect.top;

    downscale = AppConfig->GetFloat(TEXT("Video"), TEXT("Downscale"), 1.0f);
    baseCX = AppConfig->GetInt(TEXT("Video"), TEXT("BaseWidth"),  defCX);
    baseCY = AppConfig->GetInt(TEXT("Video"), TEXT("BaseHeight"), defCY);

    baseCX = MIN(MAX(baseCX, 128), 4096);
    baseCY = MIN(MAX(baseCY, 128), 4096);

    scaleCX = UINT(double(baseCX) / double(downscale));
    scaleCY = UINT(double(baseCY) / double(downscale));

    //align width to 128bit for fast SSE YUV4:2:0 conversion
    outputCX = scaleCX & 0xFFFFFFFC;
    outputCY = scaleCY & 0xFFFFFFFE;

    bUseMultithreadedOptimizations = AppConfig->GetInt(TEXT("General"), TEXT("UseMultithreadedOptimizations"), TRUE) != 0;
    Log(TEXT("  Multithreaded optimizations: %s"), (CTSTR)(bUseMultithreadedOptimizations ? TEXT("On") : TEXT("Off")));

    GlobalConfig->SetInt(TEXT("Audio"), TEXT("GlobalAudioTimeAdjust"), 0);

    //------------------------------------------------------------------

    Log(TEXT("  Base resolution: %ux%u"), baseCX, baseCY);
    Log(TEXT("  Output resolution: %ux%u"), outputCX, outputCY);
    Log(TEXT("------------------------------------------"));

    //------------------------------------------------------------------

    GS = new D3D10System;
    GS->Init();

    //-------------------------------------------------------------

    mainVertexShader    = CreateVertexShaderFromFile(TEXT("shaders/DrawTexture.vShader"));
    mainPixelShader     = CreatePixelShaderFromFile(TEXT("shaders/DrawTexture.pShader"));

    solidVertexShader   = CreateVertexShaderFromFile(TEXT("shaders/DrawSolid.vShader"));
    solidPixelShader    = CreatePixelShaderFromFile(TEXT("shaders/DrawSolid.pShader"));

    //------------------------------------------------------------------

    CTSTR lpShader = NULL;
    if(CloseFloat(downscale, 1.0))
        lpShader = TEXT("shaders/DrawYUVTexture.pShader");
    else if(CloseFloat(downscale, 1.5))
        lpShader = TEXT("shaders/DownscaleYUV1.5.pShader");
    else if(CloseFloat(downscale, 2.0))
        lpShader = TEXT("shaders/DownscaleYUV2.pShader");
    else if(CloseFloat(downscale, 2.25))
        lpShader = TEXT("shaders/DownscaleYUV2.25.pShader");
    else if(CloseFloat(downscale, 3.0))
        lpShader = TEXT("shaders/DownscaleYUV3.pShader");
    else
        CrashError(TEXT("Invalid downscale value (must be either 1.0, 1.5, 2.0, 2.25, or 3.0)"));

    yuvScalePixelShader = CreatePixelShaderFromFile(lpShader);
    if (!yuvScalePixelShader)
        CrashError(TEXT("Unable to create shader from file %s"), lpShader);

    //-------------------------------------------------------------

    for(UINT i=0; i<numRenderBuffers; i++)
    {
        mainRenderTextures[i] = CreateRenderTarget(baseCX, baseCY, GS_BGRA, FALSE);
        yuvRenderTextures[i]  = CreateRenderTarget(outputCX, outputCY, GS_BGRA, FALSE);
    }

    //-------------------------------------------------------------

    D3D10_TEXTURE2D_DESC td;
    zero(&td, sizeof(td));
    td.Width            = outputCX;
    td.Height           = outputCY;
    td.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.SampleDesc.Count = 1;
    td.ArraySize        = 1;
    td.Usage            = D3D10_USAGE_STAGING;
    td.CPUAccessFlags   = D3D10_CPU_ACCESS_READ;

    for(UINT i=0; i<numRenderBuffers; i++)
    {
        HRESULT err = GetD3D()->CreateTexture2D(&td, NULL, &copyTextures[i]);
        if(FAILED(err))
        {
            CrashError(TEXT("Unable to create copy texture"));
            //todo - better error handling
        }
    }

    //-------------------------------------------------------------

    AudioDeviceList playbackDevices;
    GetAudioDevices(playbackDevices, ADT_PLAYBACK);

    String strPlaybackDevice = AppConfig->GetString(TEXT("Audio"), TEXT("PlaybackDevice"), TEXT("Default"));
    if(strPlaybackDevice.IsEmpty() || !playbackDevices.HasID(strPlaybackDevice))
    {
        AppConfig->SetString(TEXT("Audio"), TEXT("PlaybackDevice"), TEXT("Default"));
        strPlaybackDevice = TEXT("Default");
    }

    Log(TEXT("Playback device %s"), strPlaybackDevice.Array());
    playbackDevices.FreeData();

    desktopAudio = CreateAudioSource(false, strPlaybackDevice);

    if(!desktopAudio) {
        CrashError(TEXT("Cannot initialize desktop audio sound, more info in the log file."));
    }

    AudioDeviceList audioDevices;
    GetAudioDevices(audioDevices, ADT_RECORDING);

    String strDevice = AppConfig->GetString(TEXT("Audio"), TEXT("Device"), NULL);
    if(strDevice.IsEmpty() || !audioDevices.HasID(strDevice))
    {
        AppConfig->SetString(TEXT("Audio"), TEXT("Device"), TEXT("Disable"));
        strDevice = TEXT("Disable");
    }

    audioDevices.FreeData();

    String strDefaultMic;
    bool bHasDefault = GetDefaultMicID(strDefaultMic);

    if(strDevice.CompareI(TEXT("Disable")))
        EnableWindow(GetDlgItem(hwndMain, ID_MICVOLUME), FALSE);
    else
    {
        bool bUseDefault = strDevice.CompareI(TEXT("Default")) != 0;
        if(!bUseDefault || bHasDefault)
        {
            if(bUseDefault)
                strDevice = strDefaultMic;

            micAudio = CreateAudioSource(true, strDevice);

            if(!micAudio)
                MessageBox(hwndMain, Str("MicrophoneFailure"), NULL, 0);
            else
                micAudio->SetTimeOffset(AppConfig->GetInt(TEXT("Audio"), TEXT("MicTimeOffset"), 0));

            EnableWindow(GetDlgItem(hwndMain, ID_MICVOLUME), micAudio != NULL);
        }
        else
            EnableWindow(GetDlgItem(hwndMain, ID_MICVOLUME), FALSE);
    }

    //-------------------------------------------------------------

    UINT bitRate = (UINT)AppConfig->GetInt(TEXT("Audio Encoding"), TEXT("Bitrate"), 96);
    String strEncoder = AppConfig->GetString(TEXT("Audio Encoding"), TEXT("Codec"), TEXT("AAC"));

#ifdef USE_AAC
    if(strEncoder.CompareI(TEXT("AAC")) && OSGetVersion() >= 7)
        audioEncoder = CreateAACEncoder(bitRate);
    else
#endif
        audioEncoder = CreateMP3Encoder(bitRate);

    //-------------------------------------------------------------

    desktopVol = AppConfig->GetFloat(TEXT("Audio"), TEXT("DesktopVolume"), 1.0f);
    micVol     = AppConfig->GetFloat(TEXT("Audio"), TEXT("MicVolume"),     1.0f);

    //-------------------------------------------------------------

    bRunning = true;

    if(sceneElement)
    {
        scene = CreateScene(sceneElement->GetString(TEXT("class")), sceneElement->GetElement(TEXT("data")));
        XElement *sources = sceneElement->GetElement(TEXT("sources"));
        if(sources)
        {
            UINT numSources = sources->NumElements();
            for(UINT i=0; i<numSources; i++)
            {
                SceneItem *item = scene->AddImageSource(sources->GetElementByID(i));
                if(item)
                {
                    if(ListView_GetItemState(GetDlgItem(hwndMain, ID_SOURCES), i, LVIS_SELECTED) > 0)
                        item->Select(true);
                }
            }
        }

        scene->BeginScene();
    }

    if(scene && scene->HasMissingSources())
        MessageBox(hwndMain, Str("Scene.MissingSources"), NULL, 0);

    //-------------------------------------------------------------

    int maxBitRate = AppConfig->GetInt   (TEXT("Video Encoding"), TEXT("MaxBitrate"), 1000);
    int bufferSize = AppConfig->GetInt   (TEXT("Video Encoding"), TEXT("BufferSize"), 1000);
    int quality    = AppConfig->GetInt   (TEXT("Video Encoding"), TEXT("Quality"),    8);
    String preset  = AppConfig->GetString(TEXT("Video Encoding"), TEXT("Preset"),     TEXT("veryfast"));
    bUsing444      = false;

    AppConfig->SetInt(TEXT("Video Encoding"), TEXT("Use444"), 0);
    
    if(bUsing444)
        bUseCFR = false;
    else
        bUseCFR = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseCFR"), 0) != 0;

    //-------------------------------------------------------------

    bWriteToFile = networkMode == 1 || AppConfig->GetInt(TEXT("Publish"), TEXT("SaveToFile")) != 0;
    String strOutputFile = AppConfig->GetString(TEXT("Publish"), TEXT("SavePath"));

    strOutputFile.FindReplace(TEXT("\\"), TEXT("/"));

    if (bWriteToFile)
    {
        OSFindData ofd;
        HANDLE hFind = NULL;
        bool bUseDateTimeName = true;

        if(hFind = OSFindFirstFile(strOutputFile, ofd))
        {
            String strFileExtension = GetPathExtension(strOutputFile);
            String strFileWithoutExtension = GetPathWithoutExtension(strOutputFile);
            UINT curFile = 0;

            if(strFileExtension.IsValid() && !ofd.bDirectory)
            {
                String strNewFilePath;
                do 
                {
                    strNewFilePath.Clear() << strFileWithoutExtension << TEXT(" (") << FormattedString(TEXT("%02u"), ++curFile) << TEXT(").") << strFileExtension;
                } while(OSFileExists(strNewFilePath));

                strOutputFile = strNewFilePath;

                bUseDateTimeName = false;
            }

            if(ofd.bDirectory)
                strOutputFile.AppendChar('/');

            OSFindClose(hFind);
        }

        if(bUseDateTimeName)
        {
            String strFileName = GetPathFileName(strOutputFile);

            if(!strFileName.IsValid() || !IsSafeFilename(strFileName))
            {
                SYSTEMTIME st;
                GetLocalTime(&st);

                String strDirectory = GetPathDirectory(strOutputFile);
                strOutputFile = FormattedString(TEXT("%s/%u-%02u-%02u-%02u%02u-%02u.mp4"), strDirectory.Array(), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
            }
        }
    }

    //-------------------------------------------------------------

    bForceMicMono = AppConfig->GetInt(TEXT("Audio"), TEXT("ForceMicMono")) != 0;
    bRecievedFirstAudioFrame = false;

    //hRequestAudioEvent = CreateSemaphore(NULL, 0, 0x7FFFFFFFL, NULL);
    hSoundDataMutex = OSCreateMutex();
    hSoundThread = OSCreateThread((XTHREAD)OBS::MainAudioThread, NULL);

    //-------------------------------------------------------------

    StartBlankSoundPlayback(strPlaybackDevice);

    //-------------------------------------------------------------

    ctsOffset = 0;
    videoEncoder = CreateX264Encoder(fps, outputCX, outputCY, quality, preset, bUsing444, maxBitRate, bufferSize, bUseCFR);

    //-------------------------------------------------------------

    for (UINT i=0; i<plugins.Num(); i++)
    {
        OBS_CALLBACK startStreamProc = plugins[i].startStreamCallback;

        if (startStreamProc)
            (*startStreamProc)();
    }

    //-------------------------------------------------------------

    if(!bTestStream && bWriteToFile && strOutputFile.IsValid())
    {
        String strFileExtension = GetPathExtension(strOutputFile);
        if(strFileExtension.CompareI(TEXT("flv")))
            fileStream = CreateFLVFileStream(strOutputFile);
        else if(strFileExtension.CompareI(TEXT("mp4")))
            fileStream = CreateMP4FileStream(strOutputFile);
    }

    //-------------------------------------------------------------

    hMainThread = OSCreateThread((XTHREAD)OBS::MainCaptureThread, NULL);

    if(bTestStream)
    {
        EnableWindow(GetDlgItem(hwndMain, ID_STARTSTOP), FALSE);
        SetWindowText(GetDlgItem(hwndMain, ID_TESTSTREAM), Str("MainWindow.StopTest"));
    }
    else
    {
        EnableWindow(GetDlgItem(hwndMain, ID_TESTSTREAM), FALSE);
        SetWindowText(GetDlgItem(hwndMain, ID_STARTSTOP), Str("MainWindow.StopStream"));
    }

    EnableWindow(GetDlgItem(hwndMain, ID_SCENEEDITOR), TRUE);

    //-------------------------------------------------------------

    ReportStartStreamTrigger(bTestStream);
    
    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 0, 0, 0);
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED);

    OSLeaveMutex (hStartupShutdownMutex);
}

void OBS::Stop()
{
    if(!bRunning) return;

    OSEnterMutex(hStartupShutdownMutex);

    bRunning = false;
    if(hMainThread)
    {
        OSTerminateThread(hMainThread, 20000);
        hMainThread = NULL;
    }

    for(UINT i=0; i<globalSources.Num(); i++)
        globalSources[i].source->EndScene();

    if(scene)
        scene->EndScene();

    //-------------------------------------------------------------

    if(hSoundThread)
    {
        //ReleaseSemaphore(hRequestAudioEvent, 1, NULL);
        OSTerminateThread(hSoundThread, 20000);
    }

    //if(hRequestAudioEvent)
    //    CloseHandle(hRequestAudioEvent);
    if(hSoundDataMutex)
        OSCloseMutex(hSoundDataMutex);

    hSoundThread = NULL;
    //hRequestAudioEvent = NULL;
    hSoundDataMutex = NULL;

    //-------------------------------------------------------------

    StopBlankSoundPlayback();

    //-------------------------------------------------------------

    for (UINT i=0; i<plugins.Num(); i++)
    {
        OBS_CALLBACK stopStreamCallback = plugins[i].stopStreamCallback;

        if (stopStreamCallback)
            (*stopStreamCallback)();
    }

    //-------------------------------------------------------------

    delete network;
    network = NULL;

    delete micAudio;
    micAudio = NULL;

    delete desktopAudio;
    desktopAudio = NULL;

    delete fileStream;
    fileStream = NULL;

    delete audioEncoder;
    audioEncoder = NULL;

    delete videoEncoder;
    videoEncoder = NULL;

    //-------------------------------------------------------------

    for(UINT i=0; i<pendingAudioFrames.Num(); i++)
        pendingAudioFrames[i].audioData.Clear();
    pendingAudioFrames.Clear();

    //-------------------------------------------------------------

    if(GS)
        GS->UnloadAllData();

    //-------------------------------------------------------------

    delete scene;
    scene = NULL;

    for(UINT i=0; i<globalSources.Num(); i++)
        globalSources[i].FreeData();
    globalSources.Clear();

    //-------------------------------------------------------------

    for(UINT i=0; i<auxAudioSources.Num(); i++)
        delete auxAudioSources[i];
    auxAudioSources.Clear();

    //-------------------------------------------------------------

    for(UINT i=0; i<numRenderBuffers; i++)
    {
        delete mainRenderTextures[i];
        delete yuvRenderTextures[i];

        mainRenderTextures[i] = NULL;
        yuvRenderTextures[i] = NULL;
    }

    for(UINT i=0; i<numRenderBuffers; i++)
    {
        SafeRelease(copyTextures[i]);
    }

    delete transitionTexture;
    transitionTexture = NULL;

    //-------------------------------------------------------------

    delete mainVertexShader;
    delete mainPixelShader;
    delete yuvScalePixelShader;

    delete solidVertexShader;
    delete solidPixelShader;

    mainVertexShader = NULL;
    mainPixelShader = NULL;
    yuvScalePixelShader = NULL;

    solidVertexShader = NULL;
    solidPixelShader = NULL;

    //-------------------------------------------------------------

    delete GS;
    GS = NULL;

    //-------------------------------------------------------------

    ResizeRenderFrame(false);
    RedrawWindow(hwndRenderFrame, NULL, NULL, RDW_INVALIDATE|RDW_UPDATENOW);

    //-------------------------------------------------------------

    AudioDeviceList audioDevices;
    GetAudioDevices(audioDevices, ADT_RECORDING);

    String strDevice = AppConfig->GetString(TEXT("Audio"), TEXT("Device"), NULL);
    if(strDevice.IsEmpty() || !audioDevices.HasID(strDevice))
    {
        AppConfig->SetString(TEXT("Audio"), TEXT("Device"), TEXT("Disable"));
        strDevice = TEXT("Disable");
    }

    audioDevices.FreeData();
    EnableWindow(GetDlgItem(hwndMain, ID_MICVOLUME), !strDevice.CompareI(TEXT("Disable")));

    //-------------------------------------------------------------

    ClearStreamInfo();

    Log(TEXT("=====Stream End======================================================================="));
    
    ReportStopStreamTrigger(bTestStream);

    if(streamReport.IsValid())
    {
        MessageBox(hwndMain, streamReport.Array(), Str("StreamReport"), MB_ICONINFORMATION|MB_OK);
        streamReport.Clear();
    }

    
    SetWindowText(GetDlgItem(hwndMain, ID_TESTSTREAM), Str("MainWindow.TestStream"));
    EnableWindow(GetDlgItem(hwndMain, ID_STARTSTOP), TRUE);
    SetWindowText(GetDlgItem(hwndMain, ID_STARTSTOP), Str("MainWindow.StartStream"));
    EnableWindow(GetDlgItem(hwndMain, ID_TESTSTREAM), TRUE);


    bEditMode = false;
    SendMessage(GetDlgItem(hwndMain, ID_SCENEEDITOR), BM_SETCHECK, BST_UNCHECKED, 0);
    EnableWindow(GetDlgItem(hwndMain, ID_SCENEEDITOR), FALSE);
    ClearStatusBar();

    InvalidateRect(hwndRenderFrame, NULL, TRUE);

    for(UINT i=0; i<bufferedVideo.Num(); i++)
        bufferedVideo[i].Clear();
    bufferedVideo.Clear();

    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 1, 0, 0);
    SetThreadExecutionState(ES_CONTINUOUS);

    bTestStream = false;

    OSLeaveMutex(hStartupShutdownMutex);
}

DWORD STDCALL OBS::MainAudioThread(LPVOID lpUnused)
{
    CoInitialize(0);
    App->MainAudioLoop();
    CoUninitialize();
    return 0;
}

#define INVALID_LL 0xFFFFFFFFFFFFFFFFLL

inline void CalculateVolumeLevels(float *buffer, int totalFloats, float mulVal, float &RMS, float &MAX)
{
    float sum = 0.0f;
    int totalFloatsStore = totalFloats;

    float Max = 0.0f;

    if(App->SSE2Available() && (UPARAM(buffer) & 0xF) == 0)
    {
        UINT alignedFloats = totalFloats & 0xFFFFFFFC;
        __m128 sseMulVal = _mm_set_ps1(mulVal);

        for(UINT i=0; i<alignedFloats; i += 4)
        {
            __m128 sseScaledVals = _mm_mul_ps(_mm_load_ps(buffer+i), sseMulVal);

            /*compute squares and add them to the sum*/
            __m128 sseSquares = _mm_mul_ps(sseScaledVals, sseScaledVals);
            sum += sseSquares.m128_f32[0] + sseSquares.m128_f32[1] + sseSquares.m128_f32[2] + sseSquares.m128_f32[3];

            /* 
                sse maximum of squared floats 
                concept from: http://stackoverflow.com/questions/9795529/how-to-find-the-horizontal-maximum-in-a-256-bit-avx-vector
            */
            __m128 sseSquaresP = _mm_shuffle_ps(sseSquares, sseSquares, _MM_SHUFFLE(1, 0, 3, 2));
            __m128 halfmax = _mm_max_ps(sseSquares, sseSquaresP);
            __m128 halfmaxP = _mm_shuffle_ps(halfmax, halfmax, _MM_SHUFFLE(0,1,2,3));
            __m128 maxs = _mm_max_ps(halfmax, halfmaxP);

            Max = max(Max, maxs.m128_f32[0]);
        }

        buffer      += alignedFloats;
        totalFloats -= alignedFloats;
    }

    for(int i=0; i<totalFloats; i++)
    {
        float val = buffer[i] * mulVal;
        float pow2Val = val * val;
        sum += pow2Val;
        Max = max(Max, pow2Val);
    }

    RMS = sqrt(sum / totalFloatsStore);
    MAX = sqrt(Max);
}

inline float toDB(float RMS)
{
    float db = 20.0f * log10(RMS);
    if(!_finite(db))
        return VOL_MIN;
    return db;
}

bool OBS::QueryNewAudio(QWORD &timestamp)
{
    bool bNewAudio = false;

    UINT audioRet;
    timestamp = INVALID_LL;

    QWORD desktopTimestamp;
    while((audioRet = desktopAudio->QueryAudio(curDesktopVol)) != NoAudioAvailable)
    {
        bNewAudio = true;

        OSEnterMutex(hAuxAudioMutex);
        for(UINT i=0; i<auxAudioSources.Num(); i++)
            auxAudioSources[i]->QueryAudio(curDesktopVol);
        OSLeaveMutex(hAuxAudioMutex);

        if(micAudio != NULL)
            micAudio->QueryAudio(curMicVol);
    }

    if(bNewAudio)
    {
        OSEnterMutex(hAuxAudioMutex);
        for(UINT i=0; i<auxAudioSources.Num(); i++)
            auxAudioSources[i]->QueryAudio(curDesktopVol);
        OSLeaveMutex(hAuxAudioMutex);

        if(micAudio)
        {
            while((audioRet = micAudio->QueryAudio(curMicVol)) != NoAudioAvailable);
        }
    }

    if(desktopAudio->GetEarliestTimestamp(desktopTimestamp))
        timestamp = desktopTimestamp;

    if(desktopAudio->GetBufferedTime() >= App->bufferingTime)
        return true;

    return false;
}

void MixAudio(float *bufferDest, float *bufferSrc, UINT totalFloats, bool bForceMono)
{
    UINT floatsLeft = totalFloats;
    float *destTemp = bufferDest;
    float *srcTemp  = bufferSrc;

    if((UPARAM(destTemp) & 0xF) == 0 && (UPARAM(srcTemp) & 0xF) == 0)
    {
        UINT alignedFloats = floatsLeft & 0xFFFFFFFC;

        if(bForceMono)
        {
            __m128 halfVal = _mm_set_ps1(0.5f);
            for(UINT i=0; i<alignedFloats; i += 4)
            {
                float *micInput = srcTemp+i;
                __m128 val = _mm_load_ps(micInput);
                __m128 shufVal = _mm_shuffle_ps(val, val, _MM_SHUFFLE(2, 3, 0, 1));

                _mm_store_ps(micInput, _mm_mul_ps(_mm_add_ps(val, shufVal), halfVal));
            }
        }

        __m128 maxVal = _mm_set_ps1(1.0f);
        __m128 minVal = _mm_set_ps1(-1.0f);

        for(UINT i=0; i<alignedFloats; i += 4)
        {
            float *pos = destTemp+i;

            __m128 mix;
            mix = _mm_add_ps(_mm_load_ps(pos), _mm_load_ps(srcTemp+i));
            mix = _mm_min_ps(mix, maxVal);
            mix = _mm_max_ps(mix, minVal);

            _mm_store_ps(pos, mix);
        }

        floatsLeft  &= 0x3;
        destTemp    += alignedFloats;
        srcTemp     += alignedFloats;
    }

    if(floatsLeft)
    {
        if(bForceMono)
        {
            for(UINT i=0; i<floatsLeft; i += 2)
            {
                srcTemp[i] += srcTemp[i+1];
                srcTemp[i] *= 0.5f;
                srcTemp[i+1] = srcTemp[i];
            }
        }

        for(UINT i=0; i<floatsLeft; i++)
        {
            float val = destTemp[i]+srcTemp[i];

            if(val < -1.0f)     val = -1.0f;
            else if(val > 1.0f) val = 1.0f;

            destTemp[i] = val;
        }
    }
}

void OBS::MainAudioLoop()
{
    DWORD taskID = 0;
    HANDLE hTask = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskID);

    bPushToTalkOn = false;

    UINT curAudioFrame = 0;

    micMax = desktopMax = VOL_MIN;
    micPeak = desktopPeak = VOL_MIN;

    UINT audioFramesSinceMeterUpdate = 0;
    UINT audioFramesSinceMicMaxUpdate = 0;
    UINT audioFramesSinceDesktopMaxUpdate = 0;

    List<float> mixedLatestDesktopSamples;

    while(TRUE)
    {
        OSSleep(5); //screw it, just run it every 5ms

        if(!bRunning)
            break;

        //-----------------------------------------------

        float *desktopBuffer, *micBuffer;
        UINT desktopAudioFrames = 0, micAudioFrames = 0;
        UINT latestDesktopAudioFrames = 0, latestMicAudioFrames = 0;

        curDesktopVol = desktopVol * desktopBoost;

        if(bUsingPushToTalk)
            curMicVol = bPushToTalkOn ? micVol : 0.0f;
        else
            curMicVol = micVol;

        curMicVol *= micBoost;

        bool bDesktopMuted = (curDesktopVol < EPSILON);
        bool bMicEnabled   = (micAudio != NULL);

        QWORD timestamp;
        while(QueryNewAudio(timestamp))
        {
            //----------------------------------------------------------------------------
            // get latest sample for calculating the volume levels

            float *latestDesktopBuffer = NULL, *latestMicBuffer = NULL;

            desktopAudio->GetBuffer(&desktopBuffer, &desktopAudioFrames, timestamp);
            desktopAudio->GetNewestFrame(&latestDesktopBuffer, &latestDesktopAudioFrames);
            if(micAudio != NULL)
            {
                micAudio->GetBuffer(&micBuffer, &micAudioFrames, timestamp);
                micAudio->GetNewestFrame(&latestMicBuffer, &latestMicAudioFrames);
            }

            //----------------------------------------------------------------------------
            // get latest aux volume level samples and mix

            OSEnterMutex(hAuxAudioMutex);

            mixedLatestDesktopSamples.CopyArray(latestDesktopBuffer, latestDesktopAudioFrames*2);
            for(UINT i=0; i<auxAudioSources.Num(); i++)
            {
                float *latestAuxBuffer;

                if(auxAudioSources[i]->GetNewestFrame(&latestAuxBuffer, &latestDesktopAudioFrames))
                    MixAudio(mixedLatestDesktopSamples.Array(), latestAuxBuffer, latestDesktopAudioFrames*2, false);
            }

            //----------------------------------------------------------------------------
            // mix output aux sound samples with the desktop

            for(UINT i=0; i<auxAudioSources.Num(); i++)
            {
                float *auxBuffer;

                if(auxAudioSources[i]->GetBuffer(&auxBuffer, &desktopAudioFrames, timestamp))
                    MixAudio(desktopBuffer, auxBuffer, desktopAudioFrames*2, false);
            }

            OSLeaveMutex(hAuxAudioMutex);

            //----------------------------------------------------------------------------

            UINT totalFloats = desktopAudioFrames*2;

            //----------------------------------------------------------------------------

            /*multiply samples by volume and compute RMS and max of samples*/
            float desktopRMS = 0, micRMS = 0, desktopMx = 0, micMx = 0;
            if(latestDesktopBuffer)
                CalculateVolumeLevels(mixedLatestDesktopSamples.Array(), latestDesktopAudioFrames*2, curDesktopVol, desktopRMS, desktopMx);
            if(bMicEnabled && latestMicBuffer)
                CalculateVolumeLevels(latestMicBuffer, latestMicAudioFrames*2, curMicVol, micRMS, micMx);

            /*convert RMS and Max of samples to dB*/            
            desktopRMS = toDB(desktopRMS);
            micRMS = toDB(micRMS);
            desktopMx = toDB(desktopMx);
            micMx = toDB(micMx);

            /* update max if sample max is greater or after 1 second */
            float maxAlpha = 0.15f;
            UINT peakMeterDelayFrames = 44100 * 3;
            if(micMx > micMax)
            {
                micMax = micMx;
            }
            else
            {
                micMax = maxAlpha * micMx + (1.0f - maxAlpha) * micMax;
            }

            if(desktopMx > desktopMax)
            {
                desktopMax = desktopMx;
            }
            else
            {
                desktopMax = maxAlpha * desktopMx + (1.0f - maxAlpha) * desktopMax;
            }

            /*update delayed peak meter*/
            if(micMax > micPeak || audioFramesSinceMicMaxUpdate > peakMeterDelayFrames)
            {
                micPeak = micMax;
                audioFramesSinceMicMaxUpdate = 0;
            }
            else
            {
                audioFramesSinceMicMaxUpdate += desktopAudioFrames;
            }

            if(desktopMax > desktopPeak || audioFramesSinceDesktopMaxUpdate > peakMeterDelayFrames)
            {
                desktopPeak = desktopMax;
                audioFramesSinceDesktopMaxUpdate = 0;
            }
            else
            {
                audioFramesSinceDesktopMaxUpdate += desktopAudioFrames;
            }

            /*low pass the level sampling*/
            float rmsAlpha = 0.15f;
            desktopMag = rmsAlpha * desktopRMS + desktopMag * (1.0f - rmsAlpha);
            micMag = rmsAlpha * micRMS + micMag * (1.0f - rmsAlpha);

            /*update the meter about every 50ms*/
            audioFramesSinceMeterUpdate += desktopAudioFrames;
            if(audioFramesSinceMeterUpdate >= 2205)
            {
                PostMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_MICVOLUMEMETER, VOLN_METERED), 0);
                audioFramesSinceMeterUpdate = 0;
            }

            //----------------------------------------------------------------------------
            // mix mic and desktop sound, using SSE2 if available
            // also, it's perfectly fine to just mix into the returned buffer

            if(bDesktopMuted)
                zero(desktopBuffer, sizeof(*desktopBuffer)*totalFloats);

            if(bMicEnabled)
                MixAudio(desktopBuffer, micBuffer, totalFloats, bForceMicMono);

            DataPacket packet;
            if(audioEncoder->Encode(desktopBuffer, totalFloats>>1, packet, timestamp))
            {
                OSEnterMutex(hSoundDataMutex);

                FrameAudio *frameAudio = pendingAudioFrames.CreateNew();
                frameAudio->audioData.CopyArray(packet.lpPacket, packet.size);
                frameAudio->timestamp = timestamp;

                /*DWORD calcTimestamp = DWORD(double(curAudioFrame)*double(GetAudioEncoder()->GetFrameSize())/44.1);
                Log(TEXT("returned timestamp: %u, calculated timestamp: %u"), timestamp, calcTimestamp);*/

                curAudioFrame++;

                OSLeaveMutex(hSoundDataMutex);
            }
        }

        //-----------------------------------------------

        if(!bRecievedFirstAudioFrame && pendingAudioFrames.Num())
            bRecievedFirstAudioFrame = true;
    }

    desktopMag = desktopMax = desktopPeak = VOL_MIN;
    micMag = micMax = micPeak = VOL_MIN;

    PostMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_MICVOLUMEMETER, VOLN_METERED), 0);

    for(UINT i=0; i<pendingAudioFrames.Num(); i++)
        pendingAudioFrames[i].audioData.Clear();

    AvRevertMmThreadCharacteristics(hTask);
}

void OBS::RequestKeyframe(int waitTime)
{
    if(bRequestKeyframe && waitTime > keyframeWait)
        return;

    bRequestKeyframe = true;
    keyframeWait = waitTime;
}

