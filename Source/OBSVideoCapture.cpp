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
extern "C"
{
#include "../x264/x264.h"
}


void Convert444to420(LPBYTE input, int width, int pitch, int height, int startY, int endY, LPBYTE *output, bool bSSE2Available);


DWORD STDCALL OBS::MainCaptureThread(LPVOID lpUnused)
{
    App->MainCaptureLoop();
    return 0;
}


struct Convert444Data
{
    LPBYTE input;
    LPBYTE output[3];
    bool bKillThread;
    HANDLE hSignalConvert, hSignalComplete;
    int width, height, pitch, startY, endY;
};

DWORD STDCALL Convert444Thread(Convert444Data *data)
{
    do
    {
        WaitForSingleObject(data->hSignalConvert, INFINITE);
        if(data->bKillThread) break;

        Convert444to420(data->input, data->width, data->pitch, data->height, data->startY, data->endY, data->output, App->SSE2Available());

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

    if((bufferedVideo.Last().timestamp-bufferedVideo[0].timestamp) >= OUTPUT_BUFFER_TIME)
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

struct FrameProcessInfo
{
    x264_picture_t *picOut;
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

    VideoSegment curSegment;
    bool bSendFrame;
    bool bProcessedFrame;

    profileIn("call to encoder");

    videoEncoder->Encode(frameInfo.picOut, videoPackets, videoPacketTypes, frameInfo.frameTimestamp, ctsOffset);
    if(bUsing444) frameInfo.prevTexture->Unmap(0);

    bProcessedFrame = (videoPackets.Num() != 0);

    //buffer video data before sending out
    bSendFrame = BufferVideoData(videoPackets, videoPacketTypes, frameInfo.frameTimestamp, curSegment);

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

                    if(bFirstAudioPacket)
                    {
                        audioTimestamp = 0;
                        bFirstAudioPacket = false;
                    }
                    else
                        audioTimestamp += curSegment.ctsOffset;

                    //stop sending audio packets when we reach an audio timestamp greater than the video timestamp
                    if(audioTimestamp > curSegment.timestamp)
                        break;

                    if(audioTimestamp == 0 || audioTimestamp > lastAudioTimestamp)
                    {
                        List<BYTE> &audioData = pendingAudioFrames[0].audioData;
                        if(audioData.Num())
                        {
                            //Log(TEXT("a:%u, %u"), audioTimestamp, firstFrameTime+audioTimestamp-curSegment.ctsOffset);

                            network->SendPacket(audioData.Array(), audioData.Num(), audioTimestamp, PacketType_Audio);
                            if(fileStream)
                                fileStream->AddPacket(audioData.Array(), audioData.Num(), audioTimestamp, PacketType_Audio);

                            audioData.Clear();

                            lastAudioTimestamp = audioTimestamp;
                        }
                    }
                }

                pendingAudioFrames[0].audioData.Clear();
                pendingAudioFrames.Remove(0);
            }
        }

        OSLeaveMutex(hSoundDataMutex);

        for(UINT i=0; i<curSegment.packets.Num(); i++)
        {
            VideoPacketData &packet = curSegment.packets[i];

            //Log(TEXT("v:%u, %u"), curSegment.timestamp, firstFrameTime+curSegment.timestamp);

            network->SendPacket(packet.data.Array(), packet.data.Num(), curSegment.timestamp, packet.type);
            if(fileStream)
                fileStream->AddPacket(packet.data.Array(), packet.data.Num(), curSegment.timestamp, packet.type);
        }
    }

    profileOut;

    return bProcessedFrame;
}


void STDCALL SleepTo(LONGLONG clockFreq, QWORD qw100NSTime)
{
    QWORD t = GetQPCTime100NS(clockFreq);

    unsigned int milliseconds = (unsigned int)((qw100NSTime - t)/10000);
    if (milliseconds > 1) //also accounts for windows 8 sleep problem
        Sleep(--milliseconds);

    for (;;)
    {
        t = GetQPCTime100NS(clockFreq);
        if (t >= qw100NSTime)
            return;
        Sleep(0);
    }
}


//#define USE_100NS_TIME 1

//todo: this function is an abomination, this is just disgusting.  fix it.
//...seriously, this is really, really horrible.  I mean this is amazingly bad.
void OBS::MainCaptureLoop()
{
    int curRenderTarget = 0, curYUVTexture = 0, curCopyTexture = 0;
    int copyWait = NUM_RENDER_BUFFERS-1;

    bSentHeaders = false;
    bFirstAudioPacket = false;

    Vect2 baseSize    = Vect2(float(baseCX), float(baseCY));
    Vect2 outputSize  = Vect2(float(outputCX), float(outputCY));
    Vect2 scaleSize   = Vect2(float(scaleCX), float(scaleCY));

    HANDLE hScaleVal = yuvScalePixelShader->GetParameterByName(TEXT("baseDimensionI"));

    LPVOID nullBuff = NULL;

    //----------------------------------------
    // x264 input buffers

    int curOutBuffer = 0;

    x264_picture_t *lastPic = NULL;
    x264_picture_t outPics[NUM_OUT_BUFFERS];

    for(int i=0; i<NUM_OUT_BUFFERS; i++)
        x264_picture_init(&outPics[i]);

    if(bUsing444)
    {
        for(int i=0; i<NUM_OUT_BUFFERS; i++)
        {
            outPics[i].img.i_csp   = X264_CSP_BGRA; //although the x264 input says BGR, x264 actually will expect packed UYV
            outPics[i].img.i_plane = 1;
        }
    }
    else
    {
        for(int i=0; i<NUM_OUT_BUFFERS; i++)
            x264_picture_alloc(&outPics[i], X264_CSP_I420, outputCX, outputCY);
    }

    //----------------------------------------
    // time/timestamp stuff

    LARGE_INTEGER clockFreq;
    QueryPerformanceFrequency(&clockFreq);

    /*LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);

    QWORD timeVal = currentTime.QuadPart;
    QWORD chi1 = timeVal * 1000 / clockFreq.QuadPart;
    QWORD chi2 = timeVal * 10000000 / clockFreq.QuadPart;
    Log(TEXT("qpc %llu, clockFreq: %llu, ms: %llu, 100ns: %llu"), timeVal, clockFreq.QuadPart, chi1, chi2);*/

    bufferedTimes.Clear();

#ifdef USE_100NS_TIME
    QWORD streamTimeStart = GetQPCTime100NS(clockFreq.QuadPart);
    QWORD frameTime100ns = 10000000/fps;

    QWORD sleepTargetTime = 0;
    bool bWasLaggedFrame = false;
#else
    DWORD streamTimeStart = OSGetTime();
#endif
    totalStreamTime = 0;

    curPTS = 0;
    lastAudioTimestamp = 0;

    latestVideoTime = firstSceneTimestamp = GetQPCTime100NS(clockFreq.QuadPart)/10000;

    DWORD fpsTimeNumerator = 1000-(frameTime*fps);
    DWORD fpsTimeDenominator = fps;
    DWORD fpsTimeAdjust = 0;

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

    QWORD curStreamTime = 0, lastStreamTime, firstFrameTime = GetQPCTimeMS(clockFreq.QuadPart);
    lastStreamTime = 0;

    bool bFirstAudioPacket = true;

    while(bRunning)
    {
        QueryPerformanceFrequency(&clockFreq);

#ifdef USE_100NS_TIME
        QWORD renderStartTime = GetQPCTime100NS(clockFreq.QuadPart);

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
        QWORD qwTime = GetQPCTime100NS(clockFreq.QuadPart)/10000;
        latestVideoTime = qwTime;

        QWORD frameDelta = qwTime-lastStreamTime;
        float fSeconds = float(frameDelta)*0.001f;

        lastStreamTime = qwTime;
#endif

        curStreamTime = qwTime-firstFrameTime;

        bufferedTimes << UINT(curStreamTime);

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
            Vect2 renderFrameSize = Vect2(float(renderFrameWidth), float(renderFrameHeight));

            SetRenderTarget(NULL);

            LoadVertexShader(mainVertexShader);
            LoadPixelShader(mainPixelShader);

            Ortho(0.0f, renderFrameSize.x, renderFrameSize.y, 0.0f, -100.0f, 100.0f);
            SetViewport(0.0f, 0.0f, renderFrameSize.x, renderFrameSize.y);

            if(bTransitioning)
            {
                BlendFunction(GS_BLEND_ONE, GS_BLEND_ZERO);
                DrawSprite(transitionTexture, 0xFFFFFFFF, 0.0f, 0.0f, renderFrameSize.x, renderFrameSize.y);
                BlendFunction(GS_BLEND_FACTOR, GS_BLEND_INVFACTOR, transitionAlpha);
            }

            DrawSprite(mainRenderTextures[curRenderTarget], 0xFFFFFFFF, 0.0f, 0.0f, renderFrameSize.x, renderFrameSize.y);

            Ortho(0.0f, renderFrameSize.x, renderFrameSize.y, 0.0f, -100.0f, 100.0f);

            //draw selections if in edit mode
            if(bEditMode && !bSizeChanging)
            {
                LoadVertexShader(solidVertexShader);
                LoadPixelShader(solidPixelShader);
                solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0xFFFF0000);

                Ortho(0.0f, baseSize.x, baseSize.y, 0.0f, -100.0f, 100.0f);

                if(scene)
                    scene->RenderSelections();
            }
        }

        //------------------------------------
        // actual stream output

        LoadVertexShader(mainVertexShader);
        LoadPixelShader(yuvScalePixelShader);

        Texture *yuvRenderTexture = yuvRenderTextures[curRenderTarget];
        SetRenderTarget(yuvRenderTexture);

        yuvScalePixelShader->SetVector2(hScaleVal, 1.0f/baseSize);

        Ortho(0.0f, outputSize.x, outputSize.y, 0.0f, -100.0f, 100.0f);
        SetViewport(0.0f, 0.0f, outputSize.x, outputSize.y);

        //why am I using scaleSize instead of outputSize for the texture?
        //because outputSize can be trimmed by up to three pixels due to 128-bit alignment.
        //using the scale function with outputSize can cause slightly inaccurate scaled images
        if(bTransitioning)
        {
            BlendFunction(GS_BLEND_ONE, GS_BLEND_ZERO);
            DrawSpriteEx(transitionTexture, 0xFFFFFFFF, 0.0f, 0.0f, scaleSize.x, scaleSize.y, 0.0f, 0.0f, scaleSize.x, scaleSize.y);
            BlendFunction(GS_BLEND_FACTOR, GS_BLEND_INVFACTOR, transitionAlpha);
        }

        DrawSpriteEx(mainRenderTextures[curRenderTarget], 0xFFFFFFFF, 0.0f, 0.0f, outputSize.x, outputSize.y, 0.0f, 0.0f, outputSize.x, outputSize.y);

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
                if(bufferedTimes.Num() > 1)
                    bufferedTimes.RemoveRange(0, bufferedTimes.Num()-1);
                lastStreamTime -= bufferedTimes[0];
                firstFrameTime += bufferedTimes[0];
                bufferedTimes[0] = 0;

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
            UINT prevCopyTexture = (curCopyTexture+1) & 1;

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

            D3D10_MAPPED_TEXTURE2D map;
            if(SUCCEEDED(prevTexture->Map(0, D3D10_MAP_READ, 0, &map)))
            {
                int prevOutBuffer = (curOutBuffer == 0) ? NUM_OUT_BUFFERS-1 : curOutBuffer-1;
                int nextOutBuffer = (curOutBuffer == NUM_OUT_BUFFERS-1) ? 0 : curOutBuffer+1;

                x264_picture_t &prevPicOut = outPics[prevOutBuffer];
                x264_picture_t &picOut = outPics[curOutBuffer];
                x264_picture_t &nextPicOut = outPics[nextOutBuffer];

                curOutBuffer = nextOutBuffer;

                if(!bUsing444)
                {
                    profileIn("conversion to 4:2:0");

                    if(bUseThreaded420)
                    {
                        for(int i=0; i<numThreads; i++)
                        {
                            convertInfo[i].input     = (LPBYTE)map.pData;
                            convertInfo[i].pitch     = map.RowPitch;
                            convertInfo[i].output[0] = nextPicOut.img.plane[0];
                            convertInfo[i].output[1] = nextPicOut.img.plane[1];
                            convertInfo[i].output[2] = nextPicOut.img.plane[2];
                            SetEvent(convertInfo[i].hSignalConvert);
                        }

                        if(bFirst420Encode)
                            bFirst420Encode = bEncode = false;
                    }
                    else
                    {
                        Convert444to420((LPBYTE)map.pData, outputCX, map.RowPitch, outputCY, 0, outputCY, picOut.img.plane, SSE2Available());
                        prevTexture->Unmap(0);
                    }

                    profileOut;
                }
                else
                {
                    picOut.img.i_stride[0] = map.RowPitch;
                    picOut.img.plane[0]    = (uint8_t*)map.pData;
                }

                if(bEncode && bFirstImage)
                    bFirstImage = bEncode = false;

                if(bEncode)
                {
                    DWORD curFrameTimestamp = bufferedTimes[curPTS++];

                    //------------------------------------

                    FrameProcessInfo frameInfo;
                    frameInfo.firstFrameTime = firstFrameTime;
                    frameInfo.prevTexture = prevTexture;

                    if(bUseCFR)
                    {
                        bool bFrameProcessed = false;

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

                            x264_picture_t *nextPic = (curFrameTimestamp-cfrTime <= halfTime) ? &picOut : &prevPicOut;

                            //these lines are just for counting duped frames
                            if(nextPic == lastPic)
                                ++numTotalDuplicatedFrames;
                            else
                                lastPic = nextPic;

                            frameInfo.picOut = nextPic;
                            frameInfo.picOut->i_pts = cfrTime;
                            frameInfo.frameTimestamp = cfrTime;
                            bFrameProcessed |= ProcessFrame(frameInfo);

                            cfrTime += frameTimeAdjust;

                            //Log(TEXT("cfrTime: %u, chi frame: %u"), cfrTime, (curFrameTimestamp-cfrTime <= halfTime));
                        }

                        if(bFrameProcessed)
                        {
                            curPTS--;
                            bufferedTimes.Remove(0);
                        }
                    }
                    else
                    {
                        picOut.i_pts = curFrameTimestamp;

                        frameInfo.picOut = &picOut;
                        frameInfo.frameTimestamp = curFrameTimestamp;

                        if(ProcessFrame(frameInfo))
                        {
                            curPTS--;
                            bufferedTimes.Remove(0);
                        }
                    }
                }
            }

            curCopyTexture = prevCopyTexture;

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
        // get audio while sleeping or capturing
        ReleaseSemaphore(hRequestAudioEvent, 1, NULL);

        //------------------------------------
        // frame sync

#ifdef USE_100NS_TIME
        QWORD renderStopTime = GetQPCTime100NS(clockFreq.QuadPart);
        sleepTargetTime += frameTime100ns;

        if(bWasLaggedFrame = (sleepTargetTime <= renderStopTime))
            numLongFrames++;
        else
            SleepTo(clockFreq.QuadPart, sleepTargetTime);
#else
        DWORD renderStopTime = OSGetTime();
        DWORD totalTime = renderStopTime-renderStartTime;

        if(totalTime > frameTimeAdjust)
            numLongFrames++;
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

        for(int i=0; i<NUM_OUT_BUFFERS; i++)
            x264_picture_clean(&outPics[i]);
    }

    Free(h420Threads);
    Free(convertInfo);

    Log(TEXT("Total frames rendered: %d, number of frames that lagged: %d (%0.2f%%) (it's okay for some frames to lag)"), numTotalFrames, numLongFrames, (double(numLongFrames)/double(numTotalFrames))*100.0);
    if(bUseCFR)
        Log(TEXT("Total duplicated CFR frames: %d"), numTotalDuplicatedFrames);
}
