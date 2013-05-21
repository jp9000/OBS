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

#include <inttypes.h>
#include "mfxstructures.h"
extern "C"
{
#include "../x264/x264.h"
}


void Convert444to420(LPBYTE input, int width, int pitch, int height, int startY, int endY, LPBYTE *output);
void Convert444toNV12(LPBYTE input, int width, int inPitch, int outPitch, int height, int startY, int endY, LPBYTE *output);


DWORD STDCALL OBS::MainCaptureThread(LPVOID lpUnused)
{
    App->MainCaptureLoop();
    return 0;
}


struct Convert444Data
{
    LPBYTE input;
    LPBYTE output[3];
    bool bNV12;
    bool bKillThread;
    HANDLE hSignalConvert, hSignalComplete;
    int width, height, inPitch, outPitch, startY, endY;
};

DWORD STDCALL Convert444Thread(Convert444Data *data)
{
    do
    {
        WaitForSingleObject(data->hSignalConvert, INFINITE);
        if(data->bKillThread) break;
        if(data->bNV12)
            Convert444toNV12(data->input, data->width, data->inPitch, data->outPitch, data->height, data->startY, data->endY, data->output);
        else
            Convert444to420(data->input, data->width, data->inPitch, data->height, data->startY, data->endY, data->output);

        SetEvent(data->hSignalComplete);
    }while(!data->bKillThread);

    return 0;
}

bool OBS::BufferVideoData(const List<DataPacket> &inputPackets, const List<PacketType> &inputTypes, DWORD timestamp, VideoSegment &segmentOut)
{
    VideoSegment &segmentIn = *bufferedVideo.CreateNew();
    segmentIn.ctsOffset = ctsOffset;
    segmentIn.timestamp = timestamp;

    segmentIn.packets.SetSize(inputPackets.Num());
    for(UINT i=0; i<inputPackets.Num(); i++)
    {
        segmentIn.packets[i].data.CopyArray(inputPackets[i].lpPacket, inputPackets[i].size);
        segmentIn.packets[i].type =  inputTypes[i];
    }

    if((bufferedVideo.Last().timestamp-bufferedVideo[0].timestamp) >= UINT(App->bufferingTime))
    {
        segmentOut.packets.TransferFrom(bufferedVideo[0].packets);
        segmentOut.ctsOffset = bufferedVideo[0].ctsOffset;
        segmentOut.timestamp = bufferedVideo[0].timestamp;
        bufferedVideo.Remove(0);

        return true;
    }

    return false;
}

#define NUM_OUT_BUFFERS 3

struct EncoderPicture
{
    x264_picture_t *picOut;
    mfxFrameSurface1 *mfxOut;
    EncoderPicture() : picOut(nullptr), mfxOut(nullptr) {}
};

bool operator==(const EncoderPicture& lhs, const EncoderPicture& rhs)
{
    if(lhs.picOut && rhs.picOut)
        return lhs.picOut == rhs.picOut;
    if(lhs.mfxOut && rhs.mfxOut)
        return lhs.mfxOut == rhs.mfxOut;
    return false;
}

struct FrameProcessInfo
{
    EncoderPicture *pic;
    ID3D10Texture2D *prevTexture;

    DWORD frameTimestamp;
    QWORD firstFrameTime;
};

bool OBS::ProcessFrame(FrameProcessInfo &frameInfo)
{
    List<DataPacket> videoPackets;
    List<PacketType> videoPacketTypes;

    //------------------------------------
    // encode

    bufferedTimes << frameInfo.frameTimestamp;

    VideoSegment curSegment;
    bool bProcessedFrame, bSendFrame = false;
    int curCTSOffset = 0;

    profileIn("call to encoder");

    videoEncoder->Encode(frameInfo.pic->picOut ? (LPVOID)frameInfo.pic->picOut : (LPVOID)frameInfo.pic->mfxOut, videoPackets, videoPacketTypes, bufferedTimes[0], ctsOffset);
    if(bUsing444) frameInfo.prevTexture->Unmap(0);
    if(videoEncoder->isQSV())
    {
        mfxFrameData& data = frameInfo.pic->mfxOut->Data;
        videoEncoder->RequestBuffers(&data);
    }

    ctsOffsets << ctsOffset;

    bProcessedFrame = (videoPackets.Num() != 0);

    //buffer video data before sending out
    if(bProcessedFrame)
    {
        bSendFrame = BufferVideoData(videoPackets, videoPacketTypes, bufferedTimes[0], curSegment);
        bufferedTimes.Remove(0);

        curCTSOffset = ctsOffsets[0];
        ctsOffsets.Remove(0);
    }

    profileOut;

    //------------------------------------
    // upload

    profileIn("sending stuff out");

    //send headers before the first frame if not yet sent
    if(bSendFrame)
    {
        if(!bSentHeaders)
        {
            network->BeginPublishing();
            bSentHeaders = true;
        }

        OSEnterMutex(hSoundDataMutex);

        if(pendingAudioFrames.Num())
        {
            while(pendingAudioFrames.Num())
            {
                if(frameInfo.firstFrameTime < pendingAudioFrames[0].timestamp)
                {
                    UINT audioTimestamp = UINT(pendingAudioFrames[0].timestamp-frameInfo.firstFrameTime);

                    /*if(bFirstAudioPacket)
                    {
                        audioTimestamp = 0;
                        bFirstAudioPacket = false;
                    }
                    else*/
                        audioTimestamp += curCTSOffset;

                    //stop sending audio packets when we reach an audio timestamp greater than the video timestamp
                    if(audioTimestamp > curSegment.timestamp)
                        break;

                    if(audioTimestamp == 0 || audioTimestamp > lastAudioTimestamp)
                    {
                        List<BYTE> &audioData = pendingAudioFrames[0].audioData;
                        if(audioData.Num())
                        {
                            //Log(TEXT("a:%u, %llu, cts: %d"), audioTimestamp, frameInfo.firstFrameTime+audioTimestamp-curCTSOffset, curCTSOffset);

                            network->SendPacket(audioData.Array(), audioData.Num(), audioTimestamp, PacketType_Audio);
                            if(fileStream)
                                fileStream->AddPacket(audioData.Array(), audioData.Num(), audioTimestamp, PacketType_Audio);

                            audioData.Clear();

                            lastAudioTimestamp = audioTimestamp;
                        }
                    }
                }
                else
                    nop();

                pendingAudioFrames[0].audioData.Clear();
                pendingAudioFrames.Remove(0);
            }
        }

        OSLeaveMutex(hSoundDataMutex);

        for(UINT i=0; i<curSegment.packets.Num(); i++)
        {
            VideoPacketData &packet = curSegment.packets[i];

            if(packet.type == PacketType_VideoHighest)
                bRequestKeyframe = false;

            //Log(TEXT("v:%u, %llu"), curSegment.timestamp, frameInfo.firstFrameTime+curSegment.timestamp);

            network->SendPacket(packet.data.Array(), packet.data.Num(), curSegment.timestamp, packet.type);
            if(fileStream)
                fileStream->AddPacket(packet.data.Array(), packet.data.Num(), curSegment.timestamp, packet.type);
        }
    }

    profileOut;

    return bProcessedFrame;
}


#define USE_100NS_TIME 1

#ifdef USE_100NS_TIME
void STDCALL SleepTo(QWORD qw100NSTime)
{
    QWORD t = GetQPCTime100NS();

    if (t >= qw100NSTime)
        return;

    unsigned int milliseconds = (unsigned int)((qw100NSTime - t)/10000);
    if (milliseconds > 1) //also accounts for windows 8 sleep problem
        OSSleep(milliseconds);

    for (;;)
    {
        t = GetQPCTime100NS();
        if (t >= qw100NSTime)
            return;
        Sleep(0);
    }
}
#endif


//todo: this function is an abomination, this is just disgusting.  fix it.
//...seriously, this is really, really horrible.  I mean this is amazingly bad.
void OBS::MainCaptureLoop()
{
    int curRenderTarget = 0, curYUVTexture = 0, curCopyTexture = 0;
    int copyWait = NUM_RENDER_BUFFERS-1;

    bSentHeaders = false;
    bFirstAudioPacket = true;

    bool bLogLongFramesProfile = GlobalConfig->GetInt(TEXT("General"), TEXT("LogLongFramesProfile"), 1) != 0;
    float logLongFramesProfilePercentage = GlobalConfig->GetFloat(TEXT("General"), TEXT("LogLongFramesProfilePercentage"), 10.f);

    Vect2 baseSize    = Vect2(float(baseCX), float(baseCY));
    Vect2 outputSize  = Vect2(float(outputCX), float(outputCY));
    Vect2 scaleSize   = Vect2(float(scaleCX), float(scaleCY));

    HANDLE hScaleVal = yuvScalePixelShader->GetParameterByName(TEXT("baseDimensionI"));

    //----------------------------------------
    // x264 input buffers

    int curOutBuffer = 0;

    bool bUsingQSV = videoEncoder->isQSV();//GlobalConfig->GetInt(TEXT("Video Encoding"), TEXT("UseQSV")) != 0;
    if(bUsingQSV)
        bUsing444 = false;

    EncoderPicture lastPic;
    EncoderPicture outPics[NUM_OUT_BUFFERS];
    DWORD outTimes[NUM_OUT_BUFFERS] = {0, 0, 0};

    for(int i=0; i<NUM_OUT_BUFFERS; i++)
    {
        if(bUsingQSV)
        {
            outPics[i].mfxOut = new mfxFrameSurface1;
            memset(outPics[i].mfxOut, 0, sizeof(mfxFrameSurface1));
            mfxFrameData& data = outPics[i].mfxOut->Data;
            videoEncoder->RequestBuffers(&data);
        }
        else
        {
            outPics[i].picOut = new x264_picture_t;
            x264_picture_init(outPics[i].picOut);
        }
    }

    if(bUsing444)
    {
        for(int i=0; i<NUM_OUT_BUFFERS; i++)
        {
            outPics[i].picOut->img.i_csp   = X264_CSP_BGRA; //although the x264 input says BGR, x264 actually will expect packed UYV
            outPics[i].picOut->img.i_plane = 1;
        }
    }
    else
    {
        if(!bUsingQSV)
            for(int i=0; i<NUM_OUT_BUFFERS; i++)
                x264_picture_alloc(outPics[i].picOut, X264_CSP_I420, outputCX, outputCY);
    }

    //----------------------------------------
    // time/timestamp stuff

    bufferedTimes.Clear();
    ctsOffsets.Clear();

#ifdef USE_100NS_TIME
    QWORD streamTimeStart = GetQPCTime100NS();
    QWORD frameTime100ns = 10000000/fps;

    QWORD sleepTargetTime = 0;
    bool bWasLaggedFrame = false;
#else
    DWORD streamTimeStart = OSGetTime();
    DWORD fpsTimeAdjust = 0;
#endif
    totalStreamTime = 0;

    lastAudioTimestamp = 0;

    latestVideoTime = firstSceneTimestamp = GetQPCTimeMS();

    DWORD fpsTimeNumerator = 1000-(frameTime*fps);
    DWORD fpsTimeDenominator = fps;
    DWORD cfrTime = 0;
    DWORD cfrTimeAdjust = 0;

    //----------------------------------------
    // start audio capture streams

    desktopAudio->StartCapture();
    if(micAudio) micAudio->StartCapture();

    //----------------------------------------
    // status bar/statistics stuff

    DWORD fpsCounter = 0;

    int numLongFrames = 0;
    int numTotalFrames = 0;

    int numTotalDuplicatedFrames = 0;

    bytesPerSec = 0;
    captureFPS = 0;
    curFramesDropped = 0;
    curStrain = 0.0;
    PostMessage(hwndMain, OBS_UPDATESTATUSBAR, 0, 0);

    QWORD lastBytesSent[3] = {0, 0, 0};
    DWORD lastFramesDropped = 0;
#ifdef USE_100NS_TIME
    double bpsTime = 0.0;
#else
    float bpsTime = 0.0f;
#endif
    double lastStrain = 0.0f;
    DWORD numSecondsWaited = 0;

    //----------------------------------------
    // 444->420 thread data

    int numThreads = MAX(OSGetTotalCores()-2, 1);
    HANDLE *h420Threads = (HANDLE*)Allocate(sizeof(HANDLE)*numThreads);
    Convert444Data *convertInfo = (Convert444Data*)Allocate(sizeof(Convert444Data)*numThreads);

    zero(h420Threads, sizeof(HANDLE)*numThreads);
    zero(convertInfo, sizeof(Convert444Data)*numThreads);

    for(int i=0; i<numThreads; i++)
    {
        convertInfo[i].width  = outputCX;
        convertInfo[i].height = outputCY;
        convertInfo[i].hSignalConvert  = CreateEvent(NULL, FALSE, FALSE, NULL);
        convertInfo[i].hSignalComplete = CreateEvent(NULL, FALSE, FALSE, NULL);
        convertInfo[i].bNV12 = bUsingQSV;

        if(i == 0)
            convertInfo[i].startY = 0;
        else
            convertInfo[i].startY = convertInfo[i-1].endY;

        if(i == (numThreads-1))
            convertInfo[i].endY = outputCY;
        else
            convertInfo[i].endY = ((outputCY/numThreads)*(i+1)) & 0xFFFFFFFE;
    }

    bool bFirstFrame = true;
    bool bFirstImage = true;
    bool bFirst420Encode = true;
    bool bUseThreaded420 = bUseMultithreadedOptimizations && (OSGetTotalCores() > 1) && !bUsing444;

    List<HANDLE> completeEvents;

    if(bUseThreaded420)
    {
        for(int i=0; i<numThreads; i++)
        {
            h420Threads[i] = OSCreateThread((XTHREAD)Convert444Thread, convertInfo+i);
            completeEvents << convertInfo[i].hSignalComplete;
        }
    }

    //----------------------------------------

    QWORD curStreamTime = 0, lastStreamTime, firstFrameTime = GetQPCTimeMS();

#ifdef USE_100NS_TIME
    lastStreamTime = GetQPCTime100NS()-frameTime100ns;
#else
    lastStreamTime = firstFrameTime-frameTime;
#endif

    //bool bFirstAudioPacket = true;

    List<ProfilerNode> threadedProfilers;
    bool bUsingThreadedProfilers = false;

    while(bRunning)
    {
#ifdef USE_100NS_TIME
        QWORD renderStartTime = GetQPCTime100NS();
        totalStreamTime = DWORD((renderStartTime-streamTimeStart)/10000);

        if(sleepTargetTime == 0 || bWasLaggedFrame)
            sleepTargetTime = renderStartTime;
#else
        DWORD renderStartTime = OSGetTime();
        totalStreamTime = renderStartTime-streamTimeStart;

        DWORD frameTimeAdjust = frameTime;
        fpsTimeAdjust += fpsTimeNumerator;
        if(fpsTimeAdjust > fpsTimeDenominator)
        {
            fpsTimeAdjust -= fpsTimeDenominator;
            ++frameTimeAdjust;
        }
#endif

        bool bRenderView = !IsIconic(hwndMain) && bRenderViewEnabled;

        profileIn("frame");

#ifdef USE_100NS_TIME
        QWORD qwTime = renderStartTime/10000;
        latestVideoTime = qwTime;

        QWORD frameDelta = renderStartTime-lastStreamTime;
        double fSeconds = double(frameDelta)*0.0000001;

        //Log(TEXT("frameDelta: %f"), fSeconds);

        lastStreamTime = renderStartTime;
#else
        QWORD qwTime = GetQPCTimeMS();
        latestVideoTime = qwTime;

        QWORD frameDelta = qwTime-lastStreamTime;
        float fSeconds = float(frameDelta)*0.001f;

        //Log(TEXT("frameDelta: %llu"), frameDelta);

        lastStreamTime = qwTime;
#endif

        //------------------------------------

        if(bRequestKeyframe && keyframeWait > 0)
        {
            keyframeWait -= int(frameDelta);

            if(keyframeWait <= 0)
            {
                GetVideoEncoder()->RequestKeyframe();
                bRequestKeyframe = false;
            }
        }

        if(!bPushToTalkDown && pushToTalkTimeLeft > 0)
        {
            pushToTalkTimeLeft -= int(frameDelta);
            OSDebugOut(TEXT("time left: %d\r\n"), pushToTalkTimeLeft);
            if(pushToTalkTimeLeft <= 0)
            {
                pushToTalkTimeLeft = 0;
                bPushToTalkOn = false;
            }
        }

        //------------------------------------

        OSEnterMutex(hSceneMutex);

        if(bResizeRenderView)
        {
            GS->ResizeView();
            bResizeRenderView = false;
        }

        //------------------------------------

        if(scene)
        {
            profileIn("scene->Preprocess");
            scene->Preprocess();

            for(UINT i=0; i<globalSources.Num(); i++)
                globalSources[i].source->Preprocess();

            profileOut;

            scene->Tick(float(fSeconds));

            for(UINT i=0; i<globalSources.Num(); i++)
                globalSources[i].source->Tick(float(fSeconds));
        }

        //------------------------------------

        QWORD curBytesSent = network->GetCurrentSentBytes();
        curFramesDropped = network->NumDroppedFrames();
        bool bUpdateBPS = false;

        bpsTime += fSeconds;
        if(bpsTime > 1.0f)
        {
            if(numSecondsWaited < 3)
                ++numSecondsWaited;

            //bytesPerSec = DWORD(curBytesSent - lastBytesSent);
            bytesPerSec = DWORD(curBytesSent - lastBytesSent[0]) / numSecondsWaited;

            if(bpsTime > 2.0)
                bpsTime = 0.0f;
            else
                bpsTime -= 1.0;

            if(numSecondsWaited == 3)
            {
                lastBytesSent[0] = lastBytesSent[1];
                lastBytesSent[1] = lastBytesSent[2];
                lastBytesSent[2] = curBytesSent;
            }
            else
                lastBytesSent[numSecondsWaited] = curBytesSent;

            captureFPS = fpsCounter;
            fpsCounter = 0;

            bUpdateBPS = true;
        }

        fpsCounter++;

        curStrain = network->GetPacketStrain();

        EnableBlending(TRUE);
        BlendFunction(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);

        //------------------------------------
        // render the mini render texture

        LoadVertexShader(mainVertexShader);
        LoadPixelShader(mainPixelShader);

        SetRenderTarget(mainRenderTextures[curRenderTarget]);

        Ortho(0.0f, baseSize.x, baseSize.y, 0.0f, -100.0f, 100.0f);
        SetViewport(0, 0, baseSize.x, baseSize.y);

        if(scene)
            scene->Render();

        //------------------------------------

        if(bTransitioning)
        {
            if(!transitionTexture)
            {
                transitionTexture = CreateTexture(baseCX, baseCY, GS_BGRA, NULL, FALSE, TRUE);
                if(transitionTexture)
                {
                    D3D10Texture *d3dTransitionTex = static_cast<D3D10Texture*>(transitionTexture);
                    D3D10Texture *d3dSceneTex = static_cast<D3D10Texture*>(mainRenderTextures[lastRenderTarget]);
                    GetD3D()->CopyResource(d3dTransitionTex->texture, d3dSceneTex->texture);
                }
                else
                    bTransitioning = false;
            }
            else if(transitionAlpha >= 1.0f)
            {
                delete transitionTexture;
                transitionTexture = NULL;

                bTransitioning = false;
            }
        }

        if(bTransitioning)
        {
            EnableBlending(TRUE);
            transitionAlpha += float(fSeconds)*5.0f;
            if(transitionAlpha > 1.0f)
                transitionAlpha = 1.0f;
        }
        else
            EnableBlending(FALSE);

        //------------------------------------
        // render the mini view thingy

        if(bRenderView)
        {
            // Cache
            const Vect2 renderFrameSize = GetRenderFrameSize();
            const Vect2 renderFrameOffset = GetRenderFrameOffset();
            const Vect2 renderFrameCtrlSize = GetRenderFrameControlSize();

            SetRenderTarget(NULL);

            LoadVertexShader(mainVertexShader);
            LoadPixelShader(mainPixelShader);

            Ortho(0.0f, renderFrameCtrlSize.x, renderFrameCtrlSize.y, 0.0f, -100.0f, 100.0f);
            if(renderFrameCtrlSize.x != oldRenderFrameCtrlWidth || renderFrameCtrlSize.y != oldRenderFrameCtrlHeight)
            {
                // User is drag resizing the window. We don't recreate the swap chains so our coordinates are wrong
                SetViewport(0.0f, 0.0f, (float)oldRenderFrameCtrlWidth, (float)oldRenderFrameCtrlHeight);
            }
            else
                SetViewport(0.0f, 0.0f, renderFrameCtrlSize.x, renderFrameCtrlSize.y);

            // Draw background (Black if fullscreen, window colour otherwise)
            if(bFullscreenMode)
                ClearColorBuffer(0x000000);
            else
                ClearColorBuffer(GetSysColor(COLOR_BTNFACE));

            if(bTransitioning)
            {
                BlendFunction(GS_BLEND_ONE, GS_BLEND_ZERO);
                DrawSprite(transitionTexture, 0xFFFFFFFF,
                        renderFrameOffset.x, renderFrameOffset.y,
                        renderFrameOffset.x + renderFrameSize.x, renderFrameOffset.y + renderFrameSize.y);
                BlendFunction(GS_BLEND_FACTOR, GS_BLEND_INVFACTOR, transitionAlpha);
            }

            DrawSprite(mainRenderTextures[curRenderTarget], 0xFFFFFFFF,
                    renderFrameOffset.x, renderFrameOffset.y,
                    renderFrameOffset.x + renderFrameSize.x, renderFrameOffset.y + renderFrameSize.y);

            //draw selections if in edit mode
            if(bEditMode && !bSizeChanging)
            {
                if(scene) {
                    LoadVertexShader(solidVertexShader);
                    LoadPixelShader(solidPixelShader);
                    solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0xFF0000);
                    scene->RenderSelections(solidPixelShader);
                }
            }
        }
        else if(bForceRenderViewErase)
        {
            InvalidateRect(hwndRenderFrame, NULL, TRUE);
            UpdateWindow(hwndRenderFrame);
            bForceRenderViewErase = false;
        }

        //------------------------------------
        // actual stream output

        LoadVertexShader(mainVertexShader);
        LoadPixelShader(yuvScalePixelShader);

        Texture *yuvRenderTexture = yuvRenderTextures[curRenderTarget];
        SetRenderTarget(yuvRenderTexture);

        if(downscale < 2.01)
            yuvScalePixelShader->SetVector2(hScaleVal, 1.0f/baseSize);
        else if(downscale < 3.01)
            yuvScalePixelShader->SetVector2(hScaleVal, 1.0f/(outputSize*3.0f));

        Ortho(0.0f, outputSize.x, outputSize.y, 0.0f, -100.0f, 100.0f);
        SetViewport(0.0f, 0.0f, outputSize.x, outputSize.y);

        //why am I using scaleSize instead of outputSize for the texture?
        //because outputSize can be trimmed by up to three pixels due to 128-bit alignment.
        //using the scale function with outputSize can cause slightly inaccurate scaled images
        if(bTransitioning)
        {
            BlendFunction(GS_BLEND_ONE, GS_BLEND_ZERO);
            DrawSpriteEx(transitionTexture, 0xFFFFFFFF, 0.0f, 0.0f, scaleSize.x, scaleSize.y, 0.0f, 0.0f, 1.0f, 1.0f);
            BlendFunction(GS_BLEND_FACTOR, GS_BLEND_INVFACTOR, transitionAlpha);
        }

        DrawSpriteEx(mainRenderTextures[curRenderTarget], 0xFFFFFFFF, 0.0f, 0.0f, outputSize.x, outputSize.y, 0.0f, 0.0f, 1.0f, 1.0f);

        //------------------------------------

        if(bRenderView && !copyWait)
            static_cast<D3D10System*>(GS)->swap->Present(0, 0);

        OSLeaveMutex(hSceneMutex);

        //------------------------------------
        // present/upload

        profileIn("video encoding and uploading");

        bool bEncode = true;

        if(copyWait)
        {
            copyWait--;
            bEncode = false;
        }
        else
        {
            //audio sometimes takes a bit to start -- do not start processing frames until audio has started capturing
            if(!bRecievedFirstAudioFrame)
                bEncode = false;
            else if(bFirstFrame)
            {
                firstFrameTime = qwTime;
                bFirstFrame = false;
            }

            if(!bEncode)
            {
                if(curYUVTexture == (NUM_RENDER_BUFFERS-1))
                    curYUVTexture = 0;
                else
                    curYUVTexture++;
            }
        }

        if(bEncode)
        {
            curStreamTime = qwTime-firstFrameTime;

            UINT prevCopyTexture = (curCopyTexture == 0) ? NUM_RENDER_BUFFERS-1 : curCopyTexture-1;

            ID3D10Texture2D *copyTexture = copyTextures[curCopyTexture];
            profileIn("CopyResource");

            if(!bFirst420Encode && bUseThreaded420)
            {
                WaitForMultipleObjects(completeEvents.Num(), completeEvents.Array(), TRUE, INFINITE);
                copyTexture->Unmap(0);
            }

            D3D10Texture *d3dYUV = static_cast<D3D10Texture*>(yuvRenderTextures[curYUVTexture]);
            GetD3D()->CopyResource(copyTexture, d3dYUV->texture);
            profileOut;

            ID3D10Texture2D *prevTexture = copyTextures[prevCopyTexture];

            if(bFirstImage) //ignore the first frame
                bFirstImage = false;
            else
            {
                HRESULT result;
                D3D10_MAPPED_TEXTURE2D map;
                if(SUCCEEDED(result = prevTexture->Map(0, D3D10_MAP_READ, 0, &map)))
                {
                    int prevOutBuffer = (curOutBuffer == 0) ? NUM_OUT_BUFFERS-1 : curOutBuffer-1;
                    int nextOutBuffer = (curOutBuffer == NUM_OUT_BUFFERS-1) ? 0 : curOutBuffer+1;

                    EncoderPicture &prevPicOut = outPics[prevOutBuffer];
                    EncoderPicture &picOut = outPics[curOutBuffer];
                    EncoderPicture &nextPicOut = outPics[nextOutBuffer];

                    if(!bUsing444)
                    {
                        profileIn("conversion to 4:2:0");

                        if(bUseThreaded420)
                        {
                            outTimes[nextOutBuffer] = (DWORD)curStreamTime;

                            bool firstRun = threadedProfilers.Num() == 0;
                            if(firstRun)
                                threadedProfilers.SetSize(numThreads);

                            for(int i=0; i<numThreads; i++)
                            {
                                convertInfo[i].input     = (LPBYTE)map.pData;
                                convertInfo[i].inPitch   = map.RowPitch;
                                if(bUsingQSV)
                                {
                                    mfxFrameData& data = nextPicOut.mfxOut->Data;
                                    convertInfo[i].outPitch  = data.Pitch;
                                    convertInfo[i].output[0] = data.Y;
                                    convertInfo[i].output[1] = data.UV;
                                }
                                else
                                {
                                    convertInfo[i].output[0] = nextPicOut.picOut->img.plane[0];
                                    convertInfo[i].output[1] = nextPicOut.picOut->img.plane[1];
                                    convertInfo[i].output[2] = nextPicOut.picOut->img.plane[2];
								}
                                if(!firstRun)
                                    threadedProfilers[i].~ProfilerNode();
                                ::new (&threadedProfilers[i]) ProfilerNode(TEXT("Convert444Threads"), true);
                                threadedProfilers[i].MonitorThread(h420Threads[i]);
                                bUsingThreadedProfilers = true;
                                SetEvent(convertInfo[i].hSignalConvert);
                            }

                            if(bFirst420Encode)
                                bFirst420Encode = bEncode = false;
                        }
                        else
                        {
                            outTimes[curOutBuffer] = (DWORD)curStreamTime;
                            if(bUsingQSV)
                            {
                                mfxFrameData& data = picOut.mfxOut->Data;
                                LPBYTE output[] = {data.Y, data.UV};
                                Convert444toNV12((LPBYTE)map.pData, outputCX, map.RowPitch, data.Pitch, outputCY, 0, outputCY, output);
                            }
                            else
                                Convert444to420((LPBYTE)map.pData, outputCX, map.RowPitch, outputCY, 0, outputCY, picOut.picOut->img.plane);
                            prevTexture->Unmap(0);
                        }

                        profileOut;
                    }
                    else
                    {
                        outTimes[curOutBuffer] = (DWORD)curStreamTime;

                        picOut.picOut->img.i_stride[0] = map.RowPitch;
                        picOut.picOut->img.plane[0]    = (uint8_t*)map.pData;
                    }

                    if(bEncode)
                    {
                        DWORD curFrameTimestamp = outTimes[prevOutBuffer];
                        //Log(TEXT("curFrameTimestamp: %u"), curFrameTimestamp);

                        //------------------------------------

                        FrameProcessInfo frameInfo;
                        frameInfo.firstFrameTime = firstFrameTime;
                        frameInfo.prevTexture = prevTexture;

                        if(bDupeFrames)
                        {
                            while(cfrTime < curFrameTimestamp)
                            {
                                DWORD frameTimeAdjust = frameTime;
                                cfrTimeAdjust += fpsTimeNumerator;
                                if(cfrTimeAdjust > fpsTimeDenominator)
                                {
                                    cfrTimeAdjust -= fpsTimeDenominator;
                                    ++frameTimeAdjust;
                                }

                                DWORD halfTime = (frameTimeAdjust+1)/2;

                                EncoderPicture &nextPic = (curFrameTimestamp-cfrTime <= halfTime) ? picOut : prevPicOut;
                                //Log(TEXT("cfrTime: %u, time: %u"), cfrTime, curFrameTimestamp);

                                //these lines are just for counting duped frames
                                if(nextPic == lastPic)
                                    ++numTotalDuplicatedFrames;
                                else
                                    lastPic = nextPic;

                                frameInfo.pic = &nextPic;
                                if(bUsingQSV)
                                    frameInfo.pic->mfxOut->Data.TimeStamp = cfrTime;
                                else
                                    frameInfo.pic->picOut->i_pts = cfrTime;
                                frameInfo.frameTimestamp = cfrTime;
                                ProcessFrame(frameInfo);

                                cfrTime += frameTimeAdjust;

                                //Log(TEXT("cfrTime: %u, chi frame: %u"), cfrTime, (curFrameTimestamp-cfrTime <= halfTime));
                            }
                        }
                        else
                        {
                            if(bUsingQSV)
                                picOut.mfxOut->Data.TimeStamp = curFrameTimestamp;
                            else
                                picOut.picOut->i_pts = curFrameTimestamp;

                            frameInfo.pic = &picOut;
                            frameInfo.frameTimestamp = curFrameTimestamp;

                            ProcessFrame(frameInfo);
                        }
                    }

                    if(bUsing444)
                    {
                        prevTexture->Unmap(0);
                    }

                    curOutBuffer = nextOutBuffer;
                }
                else
                {
                    //We have to crash, or we end up deadlocking the thread when the convert threads are never signalled
                    if (result == DXGI_ERROR_DEVICE_REMOVED)
                    {
                        String message;

                        HRESULT reason = GetD3D()->GetDeviceRemovedReason();

                        switch (reason)
                        {
                        case DXGI_ERROR_DEVICE_RESET:
                        case DXGI_ERROR_DEVICE_HUNG:
                            message = TEXT("Your video card or driver froze and was reset. Please check for possible hardware / driver issues.");
                            break;
                        case DXGI_ERROR_DEVICE_REMOVED:
                            message = TEXT("Your video card disappeared from the system. Please check for possible hardware / driver issues.");
                            break;
                        case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
                            message = TEXT("Your video driver reported an internal error. Please check for possible hardware / driver issues.");
                            break;
                        case DXGI_ERROR_INVALID_CALL:
                            message = TEXT("Your video driver reported an invalid call. Please check for possible driver issues.");
                            break;
                        default:
                            message = TEXT("DXGI_ERROR_DEVICE_REMOVED");
                            break;
                        }

                        CrashError (TEXT("Texture->Map failed: 0x%08x 0x%08x\r\n\r\n%s"), result, reason, message.Array());
                    }
                    else
                        CrashError (TEXT("Texture->Map failed: 0x%08x"), result);
                }
            }

            if(curCopyTexture == (NUM_RENDER_BUFFERS-1))
                curCopyTexture = 0;
            else
                curCopyTexture++;

            if(curYUVTexture == (NUM_RENDER_BUFFERS-1))
                curYUVTexture = 0;
            else
                curYUVTexture++;
        }

        lastRenderTarget = curRenderTarget;

        if(curRenderTarget == (NUM_RENDER_BUFFERS-1))
            curRenderTarget = 0;
        else
            curRenderTarget++;

        if(bUpdateBPS || !CloseDouble(curStrain, lastStrain) || curFramesDropped != lastFramesDropped)
        {
            PostMessage(hwndMain, OBS_UPDATESTATUSBAR, 0, 0);
            lastStrain = curStrain;

            lastFramesDropped = curFramesDropped;
        }

        profileOut;
        profileOut;

        //------------------------------------
        // we're about to sleep so we should flush the d3d command queue
        GetD3D()->Flush();

        //------------------------------------
        // frame sync

#ifdef USE_100NS_TIME
        QWORD renderStopTime = GetQPCTime100NS();
        sleepTargetTime += frameTime100ns;

        if(bWasLaggedFrame = (sleepTargetTime <= renderStopTime))
        {
            numLongFrames++;
            if(bLogLongFramesProfile && (numLongFrames/float(max(1, numTotalFrames)) * 100.) > logLongFramesProfilePercentage)
                DumpLastProfileData();
        }
        else
            SleepTo(sleepTargetTime);
#else
        DWORD renderStopTime = OSGetTime();
        DWORD totalTime = renderStopTime-renderStartTime;

        if(totalTime > frameTimeAdjust)
        {
            numLongFrames++;
            if(bLogLongFramesProfile && (numLongFrames/float(max(1, numTotalFrames)) * 100.) > logLongFramesProfilePercentage)
                DumpLastProfileData();
        }
        else if(totalTime < frameTimeAdjust)
            OSSleep(frameTimeAdjust-totalTime);
#endif

        //OSDebugOut(TEXT("Frame adjust time: %d, "), frameTimeAdjust-totalTime);

        numTotalFrames++;
    }

    if(!bUsing444)
    {
        if(bUseThreaded420)
        {
            for(int i=0; i<numThreads; i++)
            {
                if(h420Threads[i])
                {
                    convertInfo[i].bKillThread = true;
                    SetEvent(convertInfo[i].hSignalConvert);
                    if(bUsingThreadedProfilers)
                        threadedProfilers[i].~ProfilerNode();

                    OSTerminateThread(h420Threads[i], 10000);
                    h420Threads[i] = NULL;
                }

                if(convertInfo[i].hSignalConvert)
                {
                    CloseHandle(convertInfo[i].hSignalConvert);
                    convertInfo[i].hSignalConvert = NULL;
                }

                if(convertInfo[i].hSignalComplete)
                {
                    CloseHandle(convertInfo[i].hSignalComplete);
                    convertInfo[i].hSignalComplete = NULL;
                }
            }

            if(!bFirst420Encode)
            {
                ID3D10Texture2D *copyTexture = copyTextures[curCopyTexture];
                copyTexture->Unmap(0);
            }
        }

        if(bUsingQSV)
            for(int i = 0; i < NUM_OUT_BUFFERS; i++)
                delete outPics[i].mfxOut;
        else
            for(int i=0; i<NUM_OUT_BUFFERS; i++)
            {
                x264_picture_clean(outPics[i].picOut);
                delete outPics[i].picOut;
            }
    }

    Free(h420Threads);
    Free(convertInfo);

    Log(TEXT("Total frames rendered: %d, number of frames that lagged: %d (%0.2f%%) (it's okay for some frames to lag)"), numTotalFrames, numLongFrames, (double(numLongFrames)/double(numTotalFrames))*100.0);
    if(bDupeFrames)
        Log(TEXT("Total duplicated frames: %d (%0.2f%%)"), numTotalDuplicatedFrames, (double(numTotalDuplicatedFrames)/double(numTotalFrames))*100.0);
}
