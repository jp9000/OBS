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
#include <time.h>
#include <Avrt.h>

VideoEncoder* CreateX264Encoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR);
VideoEncoder* CreateQSVEncoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR, String &errors);
VideoEncoder* CreateNVENCEncoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR, String &errors);
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

void OBS::ToggleRecording()
{
    if (bRecordingOnly)
        ToggleCapturing();
    else if(!bRecording)
        StartRecording();
    else
        StopRecording();
}

void OBS::ToggleCapturing()
{
    if(!bRunning || (!bStreaming && bRecording))
        Start();
    else
        Stop();
}

void OBS::StartRecording()
{
    if(bRecording) return;
    int networkMode = AppConfig->GetInt(TEXT("Publish"), TEXT("Mode"), 2);

    bWriteToFile = networkMode == 1 || AppConfig->GetInt(TEXT("Publish"), TEXT("SaveToFile")) != 0;
    String strOutputFile = AppConfig->GetString(TEXT("Publish"), TEXT("SavePath"));

    strOutputFile.FindReplace(TEXT("\\"), TEXT("/"));

    // Don't request a keyframe while everything is starting up for the first time
    if(!bStartingUp) videoEncoder->RequestKeyframe();

    if (bWriteToFile)
    {
        OSFindData ofd;
        HANDLE hFind = NULL;
        bool bUseDateTimeName = true;
        bool bOverwrite = GlobalConfig->GetInt(L"General", L"OverwriteRecordings", false) != 0;

        if(!bOverwrite && (hFind = OSFindFirstFile(strOutputFile, ofd)))
        {
            String strFileExtension = GetPathExtension(strOutputFile);
            String strFileWithoutExtension = GetPathWithoutExtension(strOutputFile);

            if(strFileExtension.IsValid() && !ofd.bDirectory)
            {
                String strNewFilePath;
                UINT curFile = 0;

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
                String file = strOutputFile.Right(strOutputFile.Length() - strDirectory.Length());
                String extension;

                if (!file.IsEmpty())
                    extension = GetPathExtension(file.Array());

                if(extension.IsEmpty())
                    extension = TEXT("mp4");
                strOutputFile = FormattedString(TEXT("%s/%u-%02u-%02u-%02u%02u-%02u.%s"), strDirectory.Array(), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, extension.Array());
            }
        }
    }

    if(!bTestStream && bWriteToFile && strOutputFile.IsValid())
    {
        String strFileExtension = GetPathExtension(strOutputFile);
        if(strFileExtension.CompareI(TEXT("flv")))
            fileStream = CreateFLVFileStream(strOutputFile);
        else if(strFileExtension.CompareI(TEXT("mp4")))
            fileStream = CreateMP4FileStream(strOutputFile);

        if(!fileStream)
        {
            Log(TEXT("Warning - OBSCapture::Start: Unable to create the file stream. Check the file path in Broadcast Settings."));
            MessageBox(hwndMain, Str("Capture.Start.FileStream.Warning"), Str("Capture.Start.FileStream.WarningCaption"), MB_OK | MB_ICONWARNING);        
            bRecording = false;
        }
        else {
            EnableWindow(GetDlgItem(hwndMain, ID_TOGGLERECORDING), TRUE);
            SetWindowText(GetDlgItem(hwndMain, ID_TOGGLERECORDING), Str("MainWindow.StopRecording"));
            bRecording = true;
        }
    }
}

void OBS::StopRecording()
{
    if(!bRecording) return;

    VideoFileStream *tempStream = NULL;

    tempStream = fileStream;
    // Prevent the encoder thread from trying to write to fileStream while it's closing
    fileStream = NULL;

    delete tempStream;
    tempStream = NULL;
    bRecording = false;

    SetWindowText(GetDlgItem(hwndMain, ID_TOGGLERECORDING), Str("MainWindow.StartRecording"));

    if(!bStreaming) Stop();
}

void OBS::Start()
{
    if(bRunning && !bRecording) return;

    int networkMode = AppConfig->GetInt(TEXT("Publish"), TEXT("Mode"), 2);
    DWORD delayTime = (DWORD)AppConfig->GetInt(TEXT("Publish"), TEXT("Delay"));

    if(bRecording && bKeepRecording && networkMode == 0 && delayTime == 0) {
        bFirstConnect = !bReconnecting;

        network = NULL;
        network = CreateRTMPPublisher();

        Log(TEXT("=====Stream Start (while recording): %s============================="), CurrentDateTimeString().Array());

        EnableWindow(GetDlgItem(hwndMain, ID_STARTSTOP), TRUE);
        SetWindowText(GetDlgItem(hwndMain, ID_STARTSTOP), Str("MainWindow.StopStream"));

        bSentHeaders = false;
        bStreaming = true;

        return;
    }

    bStartingUp = true;

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

    OSCheckForBuggyDLLs();

    //-------------------------------------------------------------
retryHookTest:
    bool alreadyWarnedAboutModules = false;
    if (OSIncompatibleModulesLoaded())
    {
        Log(TEXT("Incompatible modules (pre-D3D) detected."));
        int ret = MessageBox(hwndMain, Str("IncompatibleModules"), NULL, MB_ICONERROR | MB_ABORTRETRYIGNORE);
        if (ret == IDABORT)
        {
            OSLeaveMutex (hStartupShutdownMutex);
            bStartingUp = false;
            return;
        }
        else if (ret == IDRETRY)
        {
            goto retryHookTest;
        }

        alreadyWarnedAboutModules = true;
    }

    String strPatchesError;
    if (OSIncompatiblePatchesLoaded(strPatchesError))
    {
        OSLeaveMutex (hStartupShutdownMutex);
        MessageBox(hwndMain, strPatchesError.Array(), NULL, MB_ICONERROR);
        Log(TEXT("Incompatible patches detected."));
        bStartingUp = false;
        return;
    }

    //-------------------------------------------------------------

    String processPriority = AppConfig->GetString(TEXT("General"), TEXT("Priority"), TEXT("Normal"));
    if (!scmp(processPriority, TEXT("Idle")))
        SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
    else if (!scmp(processPriority, TEXT("Above Normal")))
        SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
    else if (!scmp(processPriority, TEXT("High")))
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    networkMode = AppConfig->GetInt(TEXT("Publish"), TEXT("Mode"), 2);
    delayTime = (DWORD)AppConfig->GetInt(TEXT("Publish"), TEXT("Delay"));

    String strError;

    bFirstConnect = !bReconnecting;

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
        bStartingUp = false;
        return;
    }

    bReconnecting = false;

    //-------------------------------------------------------------

    Log(TEXT("=====Stream Start: %s==============================================="), CurrentDateTimeString().Array());

    //-------------------------------------------------------------

    bEnableProjectorCursor = GlobalConfig->GetInt(L"General", L"EnableProjectorCursor", 1) != 0;
    bPleaseEnableProjector = bPleaseDisableProjector = false;

    int monitorID = AppConfig->GetInt(TEXT("Video"), TEXT("Monitor"));
    if(monitorID >= (int)monitors.Num())
        monitorID = 0;

    RECT &screenRect = monitors[monitorID].rect;
    int defCX = screenRect.right  - screenRect.left;
    int defCY = screenRect.bottom - screenRect.top;

    downscaleType = AppConfig->GetInt(TEXT("Video"), TEXT("Filter"), 0);
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

    encoderSkipThreshold = GlobalConfig->GetInt(TEXT("Video"), TEXT("EncoderSkipThreshold"), fps/4);

    //------------------------------------------------------------------

    Log(TEXT("  Base resolution: %ux%u"), baseCX, baseCY);
    Log(TEXT("  Output resolution: %ux%u"), outputCX, outputCY);
    Log(TEXT("------------------------------------------"));

    //------------------------------------------------------------------

    GS = new D3D10System;
    GS->Init();

    //Thanks to ASUS OSD hooking the goddamn user mode driver framework (!!!!), we have to re-check for dangerous
    //hooks after initializing D3D.
retryHookTestV2:
    if (!alreadyWarnedAboutModules)
    {
        if (OSIncompatibleModulesLoaded())
        {
            Log(TEXT("Incompatible modules (post-D3D) detected."));
            int ret = MessageBox(hwndMain, Str("IncompatibleModules"), NULL, MB_ICONERROR | MB_ABORTRETRYIGNORE);
            if (ret == IDABORT)
            {
                //FIXME: really need a better way to abort startup than this...
                delete network;
                delete GS;

                OSLeaveMutex (hStartupShutdownMutex);
                bStartingUp = false;
                return;
            }
            else if (ret == IDRETRY)
            {
                goto retryHookTestV2;
            }
        }
    }

    //-------------------------------------------------------------

    mainVertexShader    = CreateVertexShaderFromFile(TEXT("shaders/DrawTexture.vShader"));
    mainPixelShader     = CreatePixelShaderFromFile(TEXT("shaders/DrawTexture.pShader"));

    solidVertexShader   = CreateVertexShaderFromFile(TEXT("shaders/DrawSolid.vShader"));
    solidPixelShader    = CreatePixelShaderFromFile(TEXT("shaders/DrawSolid.pShader"));

    if(!mainVertexShader || !mainPixelShader)
        CrashError(TEXT("Unable to load DrawTexture shaders"));

    if(!solidVertexShader || !solidPixelShader)
        CrashError(TEXT("Unable to load DrawSolid shaders"));

    //------------------------------------------------------------------

    CTSTR lpShader;
    if(CloseFloat(downscale, 1.0))
        lpShader = TEXT("shaders/DrawYUVTexture.pShader");
    else if(downscale < 2.01)
    {
        switch(downscaleType)
        {
            case 0: lpShader = TEXT("shaders/DownscaleBilinear1YUV.pShader"); break;
            case 1: lpShader = TEXT("shaders/DownscaleBicubicYUV.pShader"); break;
            case 2: lpShader = TEXT("shaders/DownscaleLanczos6tapYUV.pShader"); break;
        }
    }
    else if(downscale < 3.01)
        lpShader = TEXT("shaders/DownscaleBilinear9YUV.pShader");
    else
        CrashError(TEXT("Invalid downscale value (must be either 1.0, 1.5, 2.0, 2.25, or 3.0)"));

    yuvScalePixelShader = CreatePixelShaderFromFile(lpShader);
    if (!yuvScalePixelShader)
        CrashError(TEXT("Unable to create shader from file %s"), lpShader);

    //-------------------------------------------------------------

    for(UINT i=0; i<NUM_RENDER_BUFFERS; i++)
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

    for(UINT i=0; i<NUM_RENDER_BUFFERS; i++)
    {
        HRESULT err = GetD3D()->CreateTexture2D(&td, NULL, &copyTextures[i]);
        if(FAILED(err))
        {
            CrashError(TEXT("Unable to create copy texture"));
            //todo - better error handling
        }
    }

    //------------------------------------------------------------------

    String strEncoder = AppConfig->GetString(TEXT("Audio Encoding"), TEXT("Codec"), TEXT("AAC"));
    BOOL isAAC = strEncoder.CompareI(TEXT("AAC"));

    UINT format = AppConfig->GetInt(L"Audio Encoding", L"Format", 1);

    if (!isAAC)
        format = 0;

    switch (format) {
    case 0: sampleRateHz = 44100; break;
    default:
    case 1: sampleRateHz = 48000; break;
    }

    Log(L"------------------------------------------");
    Log(L"Audio Format: %uhz", sampleRateHz);

    //------------------------------------------------------------------

    AudioDeviceList playbackDevices;
    bool useInputDevices = AppConfig->GetInt(L"Audio", L"UseInputDevices", false) != 0;
    GetAudioDevices(playbackDevices, useInputDevices ? ADT_RECORDING : ADT_PLAYBACK);

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
    GetAudioDevices(audioDevices, ADT_RECORDING, false, true);

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

    bool bDisableEncoding = false;

    if (bTestStream)
        bDisableEncoding = GlobalConfig->GetInt(TEXT("General"), TEXT("DisablePreviewEncoding"), false) != 0;

    //-------------------------------------------------------------

    UINT bitRate = (UINT)AppConfig->GetInt(TEXT("Audio Encoding"), TEXT("Bitrate"), 96);

    if (bDisableEncoding)
        audioEncoder = CreateNullAudioEncoder();
    else
#ifdef USE_AAC
    if(isAAC) // && OSGetVersion() >= 7)
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
        unsigned int numSources = scene->sceneItems.Num();
        for(UINT i=0; i<numSources; i++)
        {
            XElement *source = scene->sceneItems[i]->GetElement();

            String className = source->GetString(TEXT("class"));
            if(scene->sceneItems[i]->bRender && className == "GlobalSource") {
                XElement *globalSourceData = source->GetElement(TEXT("data"));
                String globalSourceName = globalSourceData->GetString(TEXT("name"));
                if(App->GetGlobalSource(globalSourceName) != NULL) {
                    App->GetGlobalSource(globalSourceName)->GlobalSourceEnterScene();
                }
            }
        }
    }

    if(scene && scene->HasMissingSources())
        MessageBox(hwndMain, Str("Scene.MissingSources"), NULL, 0);

    //-------------------------------------------------------------

    int maxBitRate = AppConfig->GetInt   (TEXT("Video Encoding"), TEXT("MaxBitrate"), 1000);
    int bufferSize = AppConfig->GetInt   (TEXT("Video Encoding"), TEXT("BufferSize"), 1000);
    int quality    = AppConfig->GetInt   (TEXT("Video Encoding"), TEXT("Quality"),    8);
    String preset  = AppConfig->GetString(TEXT("Video Encoding"), TEXT("Preset"),     TEXT("veryfast"));
    bUsing444      = false;//AppConfig->GetInt   (TEXT("Video Encoding"), TEXT("Use444"),     0) != 0;
    bUseCFR        = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseCFR"), 1) != 0;

    //-------------------------------------------------------------

    bufferingTime = GlobalConfig->GetInt(TEXT("General"), TEXT("SceneBufferingTime"), 700);
    Log(TEXT("Scene buffering time set to %u"), bufferingTime);

    //-------------------------------------------------------------

    bForceMicMono = AppConfig->GetInt(TEXT("Audio"), TEXT("ForceMicMono")) != 0;
    bRecievedFirstAudioFrame = false;

    //hRequestAudioEvent = CreateSemaphore(NULL, 0, 0x7FFFFFFFL, NULL);
    hSoundDataMutex = OSCreateMutex();
    hSoundThread = OSCreateThread((XTHREAD)OBS::MainAudioThread, NULL);

    //-------------------------------------------------------------

    if (!useInputDevices)
        StartBlankSoundPlayback(strPlaybackDevice);

    //-------------------------------------------------------------

    colorDesc.fullRange = false;
    colorDesc.primaries = ColorPrimaries_BT709;
    colorDesc.transfer  = ColorTransfer_IEC6196621;
    colorDesc.matrix    = outputCX >= 1280 || outputCY > 576 ? ColorMatrix_BT709 : ColorMatrix_SMPTE170M;

    videoEncoder = nullptr;
    String videoEncoderErrors;
    if (bDisableEncoding)
        videoEncoder = CreateNullVideoEncoder();
    else if(AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseQSV")) != 0)
        videoEncoder = CreateQSVEncoder(fps, outputCX, outputCY, quality, preset, bUsing444, colorDesc, maxBitRate, bufferSize, bUseCFR, videoEncoderErrors);
    else if(AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseNVENC")) != 0)
        videoEncoder = CreateNVENCEncoder(fps, outputCX, outputCY, quality, preset, bUsing444, colorDesc, maxBitRate, bufferSize, bUseCFR, videoEncoderErrors);
    else
        videoEncoder = CreateX264Encoder(fps, outputCX, outputCY, quality, preset, bUsing444, colorDesc, maxBitRate, bufferSize, bUseCFR);

    if (!videoEncoder)
    {
        Log(L"Couldn't initialize encoder");
        Stop(true);

        if (videoEncoderErrors.IsEmpty())
            videoEncoderErrors = Str("Encoder.InitFailed");
        else
            videoEncoderErrors = String(Str("Encoder.InitFailedWithReason")) + videoEncoderErrors;

        MessageBox(hwndMain, videoEncoderErrors.Array(), nullptr, MB_OK | MB_ICONERROR); //might want to defer localization until here to automatically
                                                                                         //output english localization to logfile
        return;
    }

    bStreaming = true;
    //-------------------------------------------------------------

    // Ensure that the render frame is properly sized
    ResizeRenderFrame(true);

    //-------------------------------------------------------------

    StartRecording();

    //-------------------------------------------------------------

    curFramePic = NULL;
    bShutdownVideoThread = false;
    bShutdownEncodeThread = false;
    //ResetEvent(hVideoThread);
    hEncodeThread = OSCreateThread((XTHREAD)OBS::EncodeThread, NULL);
    hVideoThread = OSCreateThread((XTHREAD)OBS::MainCaptureThread, NULL);

    if(bTestStream)
    {
        EnableWindow(GetDlgItem(hwndMain, ID_STARTSTOP), FALSE);
        EnableWindow(GetDlgItem(hwndMain, ID_TOGGLERECORDING), FALSE);
        SetWindowText(GetDlgItem(hwndMain, ID_TESTSTREAM), Str("MainWindow.StopTest"));
    }
    else
    {
        EnableWindow(GetDlgItem(hwndMain, ID_TESTSTREAM), FALSE);
        SetWindowText(GetDlgItem(hwndMain, ID_STARTSTOP), Str("MainWindow.StopStream"));
    }

    EnableWindow(GetDlgItem(hwndMain, ID_SCENEEDITOR), TRUE);

    //-------------------------------------------------------------

    ReportStartStreamTrigger();
    
    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 0, 0, 0);
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED | ES_DISPLAY_REQUIRED);

    UpdateRenderViewMessage();

    //update notification icon to reflect current status
    UpdateNotificationAreaIcon();

    OSLeaveMutex (hStartupShutdownMutex);

    bStartingUp = false;
}

void OBS::Stop(bool overrideKeepRecording)
{
    if((!bStreaming && !bRecording && !bRunning) && (!bTestStream)) return;

    //ugly hack to prevent hotkeys from being processed while we're stopping otherwise we end up
    //with callbacks from the ProcessEvents call in DelayedPublisher which causes havoc.
    OSEnterMutex(hHotkeyMutex);

    int networkMode = AppConfig->GetInt(TEXT("Publish"), TEXT("Mode"), 2);

    if(!overrideKeepRecording && bRecording && bKeepRecording && networkMode == 0) {
        NetworkStream *tempStream = NULL;
        
        videoEncoder->RequestKeyframe();
        tempStream = network;
        network = NULL;

        Log(TEXT("=====Stream End (recording continues): %s========================="), CurrentDateTimeString().Array());

        delete tempStream;

        EnableWindow(GetDlgItem(hwndMain, ID_STARTSTOP), TRUE);
        SetWindowText(GetDlgItem(hwndMain, ID_STARTSTOP), Str("MainWindow.StartStream"));

        bStreaming = false;
        bSentHeaders = false;

        OSLeaveMutex(hHotkeyMutex);

        return;
    }

    OSEnterMutex(hStartupShutdownMutex);

    //we only want the capture thread to stop first, so we can ensure all packets are flushed
    bShutdownEncodeThread = true;
    ShowWindow(hwndProjector, SW_HIDE);

    if(hEncodeThread)
    {
        OSTerminateThread(hEncodeThread, 30000);
        hEncodeThread = NULL;
    }

    bShutdownVideoThread = true;
    SetEvent(hVideoEvent);

    if(hVideoThread)
    {
        OSTerminateThread(hVideoThread, 30000);
        hVideoThread = NULL;
    }

    bRunning = false;

    ReportStopStreamTrigger();

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

    delete network;
    network = NULL;
    bStreaming = false;
    
    if(bRecording) StopRecording();

    delete micAudio;
    micAudio = NULL;

    delete desktopAudio;
    desktopAudio = NULL;

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

    for(UINT i=0; i<NUM_RENDER_BUFFERS; i++)
    {
        delete mainRenderTextures[i];
        delete yuvRenderTextures[i];

        mainRenderTextures[i] = NULL;
        yuvRenderTextures[i] = NULL;
    }

    for(UINT i=0; i<NUM_RENDER_BUFFERS; i++)
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
    GetAudioDevices(audioDevices, ADT_RECORDING, false, true);

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

    DumpProfileData();
    FreeProfileData();
    Log(TEXT("=====Stream End: %s================================================="), CurrentDateTimeString().Array());

    //update notification icon to reflect current status
    UpdateNotificationAreaIcon();

    if (bRecordingOnly)
    {
        EnableWindow(GetDlgItem(hwndMain, ID_TOGGLERECORDING), TRUE);
        EnableWindow(GetDlgItem(hwndMain, ID_STARTSTOP), FALSE);
        EnableWindow(GetDlgItem(hwndMain, ID_TESTSTREAM), TRUE);
    }
    else
    {
        EnableWindow(GetDlgItem(hwndMain, ID_TOGGLERECORDING), FALSE);
        EnableWindow(GetDlgItem(hwndMain, ID_STARTSTOP), TRUE);
        EnableWindow(GetDlgItem(hwndMain, ID_TESTSTREAM), TRUE);
    }

    SetWindowText(GetDlgItem(hwndMain, ID_TOGGLERECORDING), Str("MainWindow.StartRecording"));
    SetWindowText(GetDlgItem(hwndMain, ID_TESTSTREAM), Str("MainWindow.TestStream"));
    SetWindowText(GetDlgItem(hwndMain, ID_STARTSTOP), Str("MainWindow.StartStream"));


    bEditMode = false;
    SendMessage(GetDlgItem(hwndMain, ID_SCENEEDITOR), BM_SETCHECK, BST_UNCHECKED, 0);
    EnableWindow(GetDlgItem(hwndMain, ID_SCENEEDITOR), FALSE);
    ClearStatusBar();

    InvalidateRect(hwndRenderFrame, NULL, TRUE);

    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 1, 0, 0);
    SetThreadExecutionState(ES_CONTINUOUS);

    String processPriority = AppConfig->GetString(TEXT("General"), TEXT("Priority"), TEXT("Normal"));
    if (scmp(processPriority, TEXT("Normal")))
        SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);

    bTestStream = false;

    ConfigureStreamButtons();

    UpdateRenderViewMessage();

    OSLeaveMutex(hStartupShutdownMutex);

    OSLeaveMutex(hHotkeyMutex);
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

    if((UPARAM(buffer) & 0xF) == 0)
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

bool OBS::QueryAudioBuffers(bool bQueriedDesktopDebugParam)
{
    bool bGotSomeAudio = false;

    if (!latestAudioTime) {
        desktopAudio->GetEarliestTimestamp(latestAudioTime); //will always return true
    } else {
        QWORD latestDesktopTimestamp;
        if (desktopAudio->GetLatestTimestamp(latestDesktopTimestamp)) {
            if ((latestAudioTime+10) > latestDesktopTimestamp)
                return false;
        }
        latestAudioTime += 10;
    }

    bufferedAudioTimes << latestAudioTime;

    OSEnterMutex(hAuxAudioMutex);
    for(UINT i=0; i<auxAudioSources.Num(); i++)
    {
        if (auxAudioSources[i]->QueryAudio2(auxAudioSources[i]->GetVolume(), true) != NoAudioAvailable)
            bGotSomeAudio = true;
    }

    OSLeaveMutex(hAuxAudioMutex);

    if(micAudio != NULL)
    {
        if (micAudio->QueryAudio2(curMicVol, true) != NoAudioAvailable)
            bGotSomeAudio = true;
    }

    return bGotSomeAudio;
}

bool OBS::QueryNewAudio()
{
    bool bAudioBufferFilled = false;

    while (!bAudioBufferFilled) {
        bool bGotAudio = false;

        if ((desktopAudio->QueryAudio2(curDesktopVol)) != NoAudioAvailable) {
            QueryAudioBuffers(true);
            bGotAudio = true;
        }

        bAudioBufferFilled = desktopAudio->GetBufferedTime() >= App->bufferingTime;

        if (!bGotAudio && bAudioBufferFilled)
            QueryAudioBuffers(false);

        if (bAudioBufferFilled || !bGotAudio)
            break;
    }

    /* wait until buffers are completely filled before accounting for burst */
    if (!bAudioBufferFilled)
    {
        QWORD timestamp;
        int burst = 0;

        // No more desktop data, drain auxilary/mic buffers until they're dry to prevent burst data
        OSEnterMutex(hAuxAudioMutex);
        for(UINT i=0; i<auxAudioSources.Num(); i++)
        {
            while (auxAudioSources[i]->QueryAudio2(auxAudioSources[i]->GetVolume(), true) != NoAudioAvailable)
                burst++;

            if (auxAudioSources[i]->GetLatestTimestamp(timestamp))
                auxAudioSources[i]->SortAudio(timestamp);

            /*if (burst > 10)
                Log(L"Burst happened for %s", auxAudioSources[i]->GetDeviceName2());*/
        }

        OSLeaveMutex(hAuxAudioMutex);

        burst = 0;

        if (micAudio)
        {
            while (micAudio->QueryAudio2(curMicVol, true) != NoAudioAvailable)
                burst++;

            /*if (burst > 10)
                Log(L"Burst happened for %s", micAudio->GetDeviceName2());*/

            if (micAudio->GetLatestTimestamp(timestamp))
                micAudio->SortAudio(timestamp);
        }
    }

    return bAudioBufferFilled;
}

void OBS::EncodeAudioSegment(float *buffer, UINT numFrames, QWORD timestamp)
{
    DataPacket packet;
    if(audioEncoder->Encode(buffer, numFrames, packet, timestamp))
    {
        OSEnterMutex(hSoundDataMutex);

        FrameAudio *frameAudio = pendingAudioFrames.CreateNew();
        frameAudio->audioData.CopyArray(packet.lpPacket, packet.size);
        frameAudio->timestamp = timestamp;

        OSLeaveMutex(hSoundDataMutex);
    }
}

void OBS::MainAudioLoop()
{
    const unsigned int audioSamplesPerSec = App->GetSampleRateHz();
    const unsigned int audioSampleSize = audioSamplesPerSec/100;

    DWORD taskID = 0;
    HANDLE hTask = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskID);

    bufferedAudioTimes.Clear();

    bPushToTalkOn = false;

    micMax = desktopMax = VOL_MIN;
    micPeak = desktopPeak = VOL_MIN;

    UINT audioFramesSinceMeterUpdate = 0;
    UINT audioFramesSinceMicMaxUpdate = 0;
    UINT audioFramesSinceDesktopMaxUpdate = 0;

    List<float> mixBuffer, levelsBuffer;
    mixBuffer.SetSize(audioSampleSize*2);
    levelsBuffer.SetSize(audioSampleSize*2);

    latestAudioTime = 0;

    //---------------------------------------------
    // the audio loop of doom

    while (true) {
        OSSleep(5); //screw it, just run it every 5ms

        if (!bRunning)
            break;

        //-----------------------------------------------

        float *desktopBuffer, *micBuffer;

        curDesktopVol = desktopVol * desktopBoost;

        if (bUsingPushToTalk)
            curMicVol = bPushToTalkOn ? micVol : 0.0f;
        else
            curMicVol = micVol;

        curMicVol *= micBoost;

        bool bDesktopMuted = (curDesktopVol < EPSILON);
        bool bMicEnabled   = (micAudio != NULL);

        while (QueryNewAudio()) {
            QWORD timestamp = bufferedAudioTimes[0];
            bufferedAudioTimes.Remove(0);

            zero(mixBuffer.Array(),    audioSampleSize*2*sizeof(float));
            zero(levelsBuffer.Array(), audioSampleSize*2*sizeof(float));

            //----------------------------------------------------------------------------
            // get latest sample for calculating the volume levels

            float *latestDesktopBuffer = NULL, *latestMicBuffer = NULL;

            desktopAudio->GetBuffer(&desktopBuffer, timestamp);
            desktopAudio->GetNewestFrame(&latestDesktopBuffer);

            if (micAudio != NULL) {
                micAudio->GetBuffer(&micBuffer, timestamp);
                micAudio->GetNewestFrame(&latestMicBuffer);
            }

            //----------------------------------------------------------------------------
            // mix desktop samples

            if (desktopBuffer)
                MixAudio(mixBuffer.Array(), desktopBuffer, audioSampleSize*2, false);

            if (latestDesktopBuffer)
                MixAudio(levelsBuffer.Array(), latestDesktopBuffer, audioSampleSize*2, false);

            //----------------------------------------------------------------------------
            // get latest aux volume level samples and mix

            OSEnterMutex(hAuxAudioMutex);

            for (UINT i=0; i<auxAudioSources.Num(); i++) {
                float *latestAuxBuffer;

                if(auxAudioSources[i]->GetNewestFrame(&latestAuxBuffer))
                    MixAudio(levelsBuffer.Array(), latestAuxBuffer, audioSampleSize*2, false);
            }

            //----------------------------------------------------------------------------
            // mix output aux sound samples with the desktop

            for (UINT i=0; i<auxAudioSources.Num(); i++) {
                float *auxBuffer;

                if(auxAudioSources[i]->GetBuffer(&auxBuffer, timestamp))
                    MixAudio(mixBuffer.Array(), auxBuffer, audioSampleSize*2, false);
            }

            OSLeaveMutex(hAuxAudioMutex);

            //----------------------------------------------------------------------------
            // multiply samples by volume and compute RMS and max of samples
            // Use 1.0f instead of curDesktopVol, since aux audio sources already have their volume set, and shouldn't be boosted anyway.

            float desktopRMS = 0, micRMS = 0, desktopMx = 0, micMx = 0;
            if (latestDesktopBuffer)
                CalculateVolumeLevels(levelsBuffer.Array(), audioSampleSize*2, 1.0f, desktopRMS, desktopMx);
            if (bMicEnabled && latestMicBuffer)
                CalculateVolumeLevels(latestMicBuffer, audioSampleSize*2, curMicVol, micRMS, micMx);

            //----------------------------------------------------------------------------
            // convert RMS and Max of samples to dB 

            desktopRMS = toDB(desktopRMS);
            micRMS = toDB(micRMS);
            desktopMx = toDB(desktopMx);
            micMx = toDB(micMx);

            //----------------------------------------------------------------------------
            // update max if sample max is greater or after 1 second

            float maxAlpha = 0.15f;
            UINT peakMeterDelayFrames = audioSamplesPerSec * 3;

            if (micMx > micMax)
                micMax = micMx;
            else
                micMax = maxAlpha * micMx + (1.0f - maxAlpha) * micMax;

            if(desktopMx > desktopMax)
                desktopMax = desktopMx;
            else
                desktopMax = maxAlpha * desktopMx + (1.0f - maxAlpha) * desktopMax;

            //----------------------------------------------------------------------------
            // update delayed peak meter

            if (micMax > micPeak || audioFramesSinceMicMaxUpdate > peakMeterDelayFrames) {
                micPeak = micMax;
                audioFramesSinceMicMaxUpdate = 0;
            } else {
                audioFramesSinceMicMaxUpdate += audioSampleSize;
            }

            if (desktopMax > desktopPeak || audioFramesSinceDesktopMaxUpdate > peakMeterDelayFrames) {
                desktopPeak = desktopMax;
                audioFramesSinceDesktopMaxUpdate = 0;
            } else {
                audioFramesSinceDesktopMaxUpdate += audioSampleSize;
            }

            //----------------------------------------------------------------------------
            // low pass the level sampling

            float rmsAlpha = 0.15f;
            desktopMag = rmsAlpha * desktopRMS + desktopMag * (1.0f - rmsAlpha);
            micMag = rmsAlpha * micRMS + micMag * (1.0f - rmsAlpha);

            //----------------------------------------------------------------------------
            // update the meter about every 50ms

            audioFramesSinceMeterUpdate += audioSampleSize;
            if (audioFramesSinceMeterUpdate >= (audioSampleSize*5)) {
                PostMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_MICVOLUMEMETER, VOLN_METERED), 0);
                audioFramesSinceMeterUpdate = 0;
            }

            //----------------------------------------------------------------------------
            // mix mic and desktop sound
            // also, it's perfectly fine to just mix into the returned buffer

            if (bMicEnabled && micBuffer)
                MixAudio(mixBuffer.Array(), micBuffer, audioSampleSize*2, bForceMicMono);

            EncodeAudioSegment(mixBuffer.Array(), audioSampleSize, timestamp);
        }

        //-----------------------------------------------

        if (!bRecievedFirstAudioFrame && pendingAudioFrames.Num())
            bRecievedFirstAudioFrame = true;
    }

    desktopMag = desktopMax = desktopPeak = VOL_MIN;
    micMag = micMax = micPeak = VOL_MIN;

    PostMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_MICVOLUMEMETER, VOLN_METERED), 0);

    for (UINT i=0; i<pendingAudioFrames.Num(); i++)
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



