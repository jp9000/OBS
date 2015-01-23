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

#include <memory>


void Convert444toI420(LPBYTE input, int width, int pitch, int height, int startY, int endY, LPBYTE *output);
void Convert444toNV12(LPBYTE input, int width, int inPitch, int outPitch, int height, int startY, int endY, LPBYTE *output);


DWORD STDCALL OBS::EncodeThread(LPVOID lpUnused)
{
    App->EncodeLoop();
    return 0;
}

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
    DWORD numThreads;
};

DWORD STDCALL Convert444Thread(Convert444Data *data)
{
    do
    {
        WaitForSingleObject(data->hSignalConvert, INFINITE);
        if(data->bKillThread) break;
        profileParallelSegment("Convert444Thread", "Convert444Threads", data->numThreads);
        if(data->bNV12)
            Convert444toNV12(data->input, data->width, data->inPitch, data->outPitch, data->height, data->startY, data->endY, data->output);
        else
            Convert444toNV12(data->input, data->width, data->inPitch, data->width, data->height, data->startY, data->endY, data->output);

        SetEvent(data->hSignalComplete);
    }while(!data->bKillThread);

    return 0;
}

bool OBS::BufferVideoData(const List<DataPacket> &inputPackets, const List<PacketType> &inputTypes, DWORD timestamp, DWORD out_pts, QWORD firstFrameTime, VideoSegment &segmentOut)
{
    VideoSegment &segmentIn = *bufferedVideo.CreateNew();
    segmentIn.timestamp = timestamp;
    segmentIn.pts = out_pts;

    segmentIn.packets.SetSize(inputPackets.Num());
    for(UINT i=0; i<inputPackets.Num(); i++)
    {
        segmentIn.packets[i].data.CopyArray(inputPackets[i].lpPacket, inputPackets[i].size);
        segmentIn.packets[i].type =  inputTypes[i];
    }

    bool dataReady = false;

    OSEnterMutex(hSoundDataMutex);
    for (UINT i = 0; i < pendingAudioFrames.Num(); i++)
    {
        if (firstFrameTime < pendingAudioFrames[i].timestamp && pendingAudioFrames[i].timestamp - firstFrameTime >= bufferedVideo[0].timestamp)
        {
            dataReady = true;
            break;
        }
    }
    OSLeaveMutex(hSoundDataMutex);

    if (dataReady)
    {
        segmentOut.packets.TransferFrom(bufferedVideo[0].packets);
        segmentOut.timestamp = bufferedVideo[0].timestamp;
        segmentOut.pts = bufferedVideo[0].pts;
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

    DWORD frameTimestamp;
    QWORD firstFrameTime;
};

void OBS::SendFrame(VideoSegment &curSegment, QWORD firstFrameTime)
{
    if(!bSentHeaders)
    {
        if(network && curSegment.packets[0].data[0] == 0x17) {
            network->BeginPublishing();
            bSentHeaders = true;
        }
    }

    OSEnterMutex(hSoundDataMutex);

    if(pendingAudioFrames.Num())
    {
        while(pendingAudioFrames.Num())
        {
            if(firstFrameTime < pendingAudioFrames[0].timestamp)
            {
                UINT audioTimestamp = UINT(pendingAudioFrames[0].timestamp-firstFrameTime);

                //stop sending audio packets when we reach an audio timestamp greater than the video timestamp
                if(audioTimestamp > curSegment.timestamp)
                    break;

                if(audioTimestamp == 0 || audioTimestamp > lastAudioTimestamp)
                {
                    List<BYTE> &audioData = pendingAudioFrames[0].audioData;
                    if(audioData.Num())
                    {
                        //Log(TEXT("a:%u, %llu"), audioTimestamp, frameInfo.firstFrameTime+audioTimestamp);

                        if(network)
                            network->SendPacket(audioData.Array(), audioData.Num(), audioTimestamp, PacketType_Audio);

                        if (fileStream || replayBufferStream)
                        {
                            auto shared_data = std::make_shared<const std::vector<BYTE>>(audioData.Array(), audioData.Array() + audioData.Num());
                            if (fileStream)
                                fileStream->AddPacket(shared_data, audioTimestamp, audioTimestamp, PacketType_Audio);
                            if (replayBufferStream)
                                replayBufferStream->AddPacket(shared_data, audioTimestamp, audioTimestamp, PacketType_Audio);
                        }

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

        if (network)
        {
            if (!HandleStreamStopInfo(networkStop, packet.type, curSegment))
                network->SendPacket(packet.data.Array(), packet.data.Num(), curSegment.timestamp, packet.type);
        }

        if (fileStream || replayBufferStream)
        {
            auto shared_data = std::make_shared<const std::vector<BYTE>>(packet.data.Array(), packet.data.Array() + packet.data.Num());
            if (fileStream)
            {
                if (!HandleStreamStopInfo(fileStreamStop, packet.type, curSegment))
                    fileStream->AddPacket(shared_data, curSegment.timestamp, curSegment.pts, packet.type);
            }
            if (replayBufferStream)
            {
                if (!HandleStreamStopInfo(replayBufferStop, packet.type, curSegment))
                    replayBufferStream->AddPacket(shared_data, curSegment.timestamp, curSegment.pts, packet.type);
            }
        }
    }
}

bool OBS::HandleStreamStopInfo(OBS::StopInfo &info, PacketType type, const VideoSegment& segment)
{
    if (type == PacketType_Audio || !info.func)
        return false;

    if (segment.pts < info.time)
        return false;

    if (!info.timeSeen)
    {
        info.timeSeen = true;
        return false;
    }

    info.func();
    info = {};
    return true;
}

bool OBS::ProcessFrame(FrameProcessInfo &frameInfo)
{
    List<DataPacket> videoPackets;
    List<PacketType> videoPacketTypes;

    //------------------------------------
    // encode

    bufferedTimes << frameInfo.frameTimestamp;

    VideoSegment curSegment;
    bool bProcessedFrame, bSendFrame = false;
    VOID *picIn;

    //profileIn("call to encoder");

    if (bShutdownEncodeThread)
        picIn = NULL;
    else
        picIn = frameInfo.pic->picOut ? (LPVOID)frameInfo.pic->picOut : (LPVOID)frameInfo.pic->mfxOut;

    DWORD out_pts = 0;
    videoEncoder->Encode(picIn, videoPackets, videoPacketTypes, bufferedTimes[0], out_pts);

    bProcessedFrame = (videoPackets.Num() != 0);

    //buffer video data before sending out
    if(bProcessedFrame)
    {
        bSendFrame = BufferVideoData(videoPackets, videoPacketTypes, bufferedTimes[0], out_pts, frameInfo.firstFrameTime, curSegment);
        bufferedTimes.Remove(0);
    }
    else
        nop();

    //profileOut;

    //------------------------------------
    // upload

    profileIn("sending stuff out");

    //send headers before the first frame if not yet sent
    if(bSendFrame)
        SendFrame(curSegment, frameInfo.firstFrameTime);

    profileOut;

    return bProcessedFrame;
}


bool STDCALL SleepToNS(QWORD qwNSTime)
{
    QWORD t = GetQPCTimeNS();

    if (t >= qwNSTime)
        return false;

    unsigned int milliseconds = (unsigned int)((qwNSTime - t)/1000000);
    if (milliseconds > 1) //also accounts for windows 8 sleep problem
    {
        //trap suspicious sleeps that should never happen
        if (milliseconds > 10000)
        {
            Log(TEXT("Tried to sleep for %u seconds, that can't be right! Triggering breakpoint."), milliseconds);
            DebugBreak();
        }
        OSSleep(milliseconds);
    }

    for (;;)
    {
        t = GetQPCTimeNS();
        if (t >= qwNSTime)
            return true;
        Sleep(1);
    }
}

bool STDCALL SleepTo100NS(QWORD qw100NSTime)
{
    QWORD t = GetQPCTime100NS();

    if (t >= qw100NSTime)
        return false;

    unsigned int milliseconds = (unsigned int)((qw100NSTime - t)/10000);
    if (milliseconds > 1) //also accounts for windows 8 sleep problem
        OSSleep(milliseconds);

    for (;;)
    {
        t = GetQPCTime100NS();
        if (t >= qw100NSTime)
            return true;
        Sleep(1);
    }
}

#ifdef OBS_TEST_BUILD
#define LOGLONGFRAMESDEFAULT 1
#else
#define LOGLONGFRAMESDEFAULT 0
#endif

UINT OBS::FlushBufferedVideo()
{
    UINT framesFlushed = 0;

    if (bufferedVideo.Num())
    {
        QWORD startTime = GetQPCTimeMS();
        DWORD baseTimestamp = bufferedVideo[0].timestamp;
        DWORD lastTimestamp = bufferedVideo.Last().timestamp;

        Log(TEXT("FlushBufferedVideo: Flushing %d packets over %d ms"), bufferedVideo.Num(), (lastTimestamp - baseTimestamp));

        for (UINT i = 0; i<bufferedVideo.Num(); i++)
        {
            //we measure our own time rather than sleep between frames due to potential sleep drift
            QWORD curTime;

            curTime = GetQPCTimeMS();
            while (curTime - startTime < bufferedVideo[i].timestamp - baseTimestamp)
            {
                OSSleep(1);
                curTime = GetQPCTimeMS();
            }

            SendFrame(bufferedVideo[i], firstFrameTimestamp);
            bufferedVideo[i].Clear();

            framesFlushed++;
        }

        bufferedVideo.Clear();
    }

    return framesFlushed;
}

void OBS::EncodeLoop()
{
    QWORD streamTimeStart = GetQPCTimeNS();
    QWORD frameTimeNS = 1000000000/fps;
    bool bufferedFrames = true; //to avoid constantly polling number of frames
    int numTotalDuplicatedFrames = 0, numTotalFrames = 0, numFramesSkipped = 0;

    bufferedTimes.Clear();

    bool bUsingQSV = videoEncoder->isQSV();//GlobalConfig->GetInt(TEXT("Video Encoding"), TEXT("UseQSV")) != 0;

    QWORD sleepTargetTime = streamTimeStart+frameTimeNS;
    latestVideoTime = firstSceneTimestamp = streamTimeStart/1000000;
    latestVideoTimeNS = streamTimeStart;

    firstFrameTimestamp = 0;

    UINT encoderInfo = 0;
    QWORD messageTime = 0;

    EncoderPicture *lastPic = NULL;

    UINT skipThreshold = encoderSkipThreshold*2;
    UINT no_sleep_counter = 0;

    CircularList<QWORD> bufferedTimes;

    while(!bShutdownEncodeThread || (bufferedFrames && !bTestStream)) {
        if (!SleepToNS(sleepTargetTime += (frameTimeNS/2)))
            no_sleep_counter++;
        else
            no_sleep_counter = 0;

        latestVideoTime = sleepTargetTime/1000000;
        latestVideoTimeNS = sleepTargetTime;

        if (no_sleep_counter < skipThreshold) {
            SetEvent(hVideoEvent);
            if (encoderInfo) {
                if (messageTime == 0) {
                    messageTime = latestVideoTime+3000;
                } else if (latestVideoTime >= messageTime) {
                    RemoveStreamInfo(encoderInfo);
                    encoderInfo = 0;
                    messageTime = 0;
                }
            }
        } else {
            numFramesSkipped++;
            if (!encoderInfo)
                encoderInfo = AddStreamInfo(Str("EncoderLag"), StreamInfoPriority_Critical);
            messageTime = 0;
        }

        if (!SleepToNS(sleepTargetTime += (frameTimeNS/2)))
            no_sleep_counter++;
        else
            no_sleep_counter = 0;
        bufferedTimes << latestVideoTime;

        if (curFramePic && firstFrameTimestamp) {
            while (bufferedTimes[0] < firstFrameTimestamp)
                bufferedTimes.Remove(0);

            DWORD curFrameTimestamp = DWORD(bufferedTimes[0] - firstFrameTimestamp);
            bufferedTimes.Remove(0);

            profileIn("encoder thread frame");

            FrameProcessInfo frameInfo;
            frameInfo.firstFrameTime = firstFrameTimestamp;
            frameInfo.frameTimestamp = curFrameTimestamp;
            frameInfo.pic = curFramePic;

            if (lastPic == frameInfo.pic)
                numTotalDuplicatedFrames++;

            if(bUsingQSV)
                curFramePic->mfxOut->Data.TimeStamp = curFrameTimestamp;
            else
                curFramePic->picOut->i_pts = curFrameTimestamp;

            ProcessFrame(frameInfo);

            lastPic = frameInfo.pic;

            profileOut;

            numTotalFrames++;
        }

        if (bShutdownEncodeThread)
            bufferedFrames = videoEncoder->HasBufferedFrames();
    }

    //if (bTestStream)
    //    bufferedVideo.Clear();

    //flush all video frames in the "scene buffering time" buffer
    if (firstFrameTimestamp)
        numTotalFrames += FlushBufferedVideo();

    Log(TEXT("Total frames encoded: %d, total frames duplicated: %d (%0.2f%%)"), numTotalFrames, numTotalDuplicatedFrames, (numTotalFrames > 0) ? (double(numTotalDuplicatedFrames)/double(numTotalFrames))*100.0 : 0.0f);
    if (numFramesSkipped)
        Log(TEXT("Number of frames skipped due to encoder lag: %d (%0.2f%%)"), numFramesSkipped, (numTotalFrames > 0) ? (double(numFramesSkipped)/double(numTotalFrames))*100.0 : 0.0f);

    SetEvent(hVideoEvent);
    bShutdownVideoThread = true;
}

void OBS::DrawPreview(const Vect2 &renderFrameSize, const Vect2 &renderFrameOffset, const Vect2 &renderFrameCtrlSize, int curRenderTarget, PreviewDrawType type)
{
    LoadVertexShader(mainVertexShader);
    LoadPixelShader(mainPixelShader);

    Ortho(0.0f, renderFrameCtrlSize.x, renderFrameCtrlSize.y, 0.0f, -100.0f, 100.0f);
    if(type != Preview_Projector
       && (renderFrameCtrlSize.x != oldRenderFrameCtrlWidth
           || renderFrameCtrlSize.y != oldRenderFrameCtrlHeight))
    {
        // User is drag resizing the window. We don't recreate the swap chains so our coordinates are wrong
        SetViewport(0.0f, 0.0f, (float)oldRenderFrameCtrlWidth, (float)oldRenderFrameCtrlHeight);
    }
    else
        SetViewport(0.0f, 0.0f, renderFrameCtrlSize.x, renderFrameCtrlSize.y);

    // Draw background (Black if fullscreen/projector, window colour otherwise)
    if(type == Preview_Fullscreen || type == Preview_Projector)
        ClearColorBuffer(0x000000);
    else
        ClearColorBuffer(GetSysColor(COLOR_BTNFACE));

    if(bTransitioning)
        DrawSprite(transitionTexture, 0xFFFFFFFF,
                renderFrameOffset.x, renderFrameOffset.y,
                renderFrameOffset.x + renderFrameSize.x, renderFrameOffset.y + renderFrameSize.y);
    else
        DrawSprite(mainRenderTextures[curRenderTarget], 0xFFFFFFFF,
                renderFrameOffset.x, renderFrameOffset.y,
                renderFrameOffset.x + renderFrameSize.x, renderFrameOffset.y + renderFrameSize.y);
}

const float yuvFullMat[][16] = {
    {0.000000f,  1.000000f,  0.000000f,  0.000000f,
     0.000000f,  0.000000f,  1.000000f,  0.000000f,
     1.000000f,  0.000000f,  0.000000f,  0.000000f,
     0.000000f,  0.000000f,  0.000000f,  1.000000f},

    {0.250000f,  0.500000f,  0.250000f,  0.000000f,
    -0.249020f,  0.498039f, -0.249020f,  0.501961f,
     0.498039f,  0.000000f, -0.498039f,  0.501961f,
     0.000000f,  0.000000f,  0.000000f,  1.000000f},

    {0.262700f,  0.678000f,  0.059300f,  0.000000f,
    -0.139082f, -0.358957f,  0.498039f,  0.501961f,
     0.498039f, -0.457983f, -0.040057f,  0.501961f,
     0.000000f,  0.000000f,  0.000000f,  1.000000f},

    {0.212600f,  0.715200f,  0.072200f,  0.000000f,
    -0.114123f, -0.383916f,  0.498039f,  0.501961f,
     0.498039f, -0.452372f, -0.045667f,  0.501961f,
     0.000000f,  0.000000f,  0.000000f,  1.000000f},

    {0.212200f,  0.701300f,  0.086500f,  0.000000f,
    -0.115691f, -0.382348f,  0.498039f,  0.501961f,
     0.498039f, -0.443355f, -0.054684f,  0.501961f,
     0.000000f,  0.000000f,  0.000000f,  1.000000f},

    {0.299000f,  0.587000f,  0.114000f,  0.000000f,
    -0.168074f, -0.329965f,  0.498039f,  0.501961f,
     0.498039f, -0.417046f, -0.080994f,  0.501961f,
     0.000000f,  0.000000f,  0.000000f,  1.000000f},
};

const float yuvMat[][16] = {
    {0.000000f,  0.858824f,  0.000000f,  0.062745f,
     0.000000f,  0.000000f,  0.858824f,  0.062745f,
     0.858824f,  0.000000f,  0.000000f,  0.062745f,
     0.000000f,  0.000000f,  0.000000f,  1.000000f},

    {0.214706f,  0.429412f,  0.214706f,  0.062745f,
    -0.219608f,  0.439216f, -0.219608f,  0.501961f,
     0.439216f,  0.000000f, -0.439216f,  0.501961f,
     0.000000f,  0.000000f,  0.000000f,  1.000000f},

    {0.225613f,  0.582282f,  0.050928f,  0.062745f,
    -0.122655f, -0.316560f,  0.439216f,  0.501961f,
     0.439216f, -0.403890f, -0.035325f,  0.501961f,
     0.000000f,  0.000000f,  0.000000f,  1.000000f},

    {0.182586f,  0.614231f,  0.062007f,  0.062745f,
    -0.100644f, -0.338572f,  0.439216f,  0.501961f,
     0.439216f, -0.398942f, -0.040274f,  0.501961f,
     0.000000f,  0.000000f,  0.000000f,  1.000000f},

    {0.182242f,  0.602293f,  0.074288f,  0.062745f,
    -0.102027f, -0.337189f,  0.439216f,  0.501961f,
     0.439216f, -0.390990f, -0.048226f,  0.501961f,
     0.000000f,  0.000000f,  0.000000f,  1.000000f},

    {0.256788f,  0.504129f,  0.097906f,  0.062745f,
    -0.148223f, -0.290993f,  0.439216f,  0.501961f,
     0.439216f, -0.367788f, -0.071427f,  0.501961f,
     0.000000f,  0.000000f,  0.000000f,  1.000000f},
};

//todo: this function is an abomination, this is just disgusting.  fix it.
//...seriously, this is really, really horrible.  I mean this is amazingly bad.
void OBS::MainCaptureLoop()
{
    int curRenderTarget = 0, curYUVTexture = 0, curCopyTexture = 0;
    int copyWait = NUM_RENDER_BUFFERS-1;

    bSentHeaders = false;
    bFirstAudioPacket = true;

    bool bLogLongFramesProfile = GlobalConfig->GetInt(TEXT("General"), TEXT("LogLongFramesProfile"), LOGLONGFRAMESDEFAULT) != 0;
    float logLongFramesProfilePercentage = GlobalConfig->GetFloat(TEXT("General"), TEXT("LogLongFramesProfilePercentage"), 10.f);

    Vect2 baseSize    = Vect2(float(baseCX), float(baseCY));
    Vect2 outputSize  = Vect2(float(outputCX), float(outputCY));
    Vect2 scaleSize   = Vect2(float(scaleCX), float(scaleCY));

    HANDLE hMatrix   = yuvScalePixelShader->GetParameterByName(TEXT("yuvMat"));
    HANDLE hScaleVal = yuvScalePixelShader->GetParameterByName(TEXT("baseDimensionI"));

    HANDLE hTransitionTime = transitionPixelShader->GetParameterByName(TEXT("transitionTime"));

    //----------------------------------------
    // x264 input buffers

    int curOutBuffer = 0;

    bool bUsingQSV = videoEncoder->isQSV();//GlobalConfig->GetInt(TEXT("Video Encoding"), TEXT("UseQSV")) != 0;
    bUsing444 = false;

    EncoderPicture lastPic;
    EncoderPicture outPics[NUM_OUT_BUFFERS];

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
                x264_picture_alloc(outPics[i].picOut, X264_CSP_NV12, outputCX, outputCY);
    }

    int bCongestionControl = AppConfig->GetInt (TEXT("Video Encoding"), TEXT("CongestionControl"), 0);
    bool bDynamicBitrateSupported = App->GetVideoEncoder()->DynamicBitrateSupported();
    int defaultBitRate = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("MaxBitrate"), 1000);
    int currentBitRate = defaultBitRate;
    QWORD lastAdjustmentTime = 0;
    UINT adjustmentStreamId = 0;

    //std::unique_ptr<ProfilerNode> encodeThreadProfiler;

    //----------------------------------------
    // time/timestamp stuff

    bool bWasLaggedFrame = false;

    totalStreamTime = 0;
    lastAudioTimestamp = 0;

    //----------------------------------------
    // start audio capture streams

    desktopAudio->StartCapture();
    if(micAudio) micAudio->StartCapture();

    //----------------------------------------
    // status bar/statistics stuff

    DWORD fpsCounter = 0;

    int numLongFrames = 0;
    int numTotalFrames = 0;

    bytesPerSec = 0;
    captureFPS = 0;
    curFramesDropped = 0;
    curStrain = 0.0;
    PostMessage(hwndMain, OBS_UPDATESTATUSBAR, 0, 0);

    QWORD lastBytesSent[3] = {0, 0, 0};
    DWORD lastFramesDropped = 0;
    double bpsTime = 0.0;

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
        convertInfo[i].numThreads = numThreads;

        if(i == 0)
            convertInfo[i].startY = 0;
        else
            convertInfo[i].startY = convertInfo[i-1].endY;

        if(i == (numThreads-1))
            convertInfo[i].endY = outputCY;
        else
            convertInfo[i].endY = ((outputCY/numThreads)*(i+1)) & 0xFFFFFFFE;
    }

    bool bEncode;
    bool bFirstFrame = true;
    bool bFirstImage = true;
    bool bFirstEncode = true;
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

    QWORD streamTimeStart  = GetQPCTimeNS();
    QWORD lastStreamTime   = 0;
    QWORD firstFrameTimeMS = streamTimeStart/1000000;
    QWORD frameLengthNS    = 1000000000/fps;

    while(WaitForSingleObject(hVideoEvent, INFINITE) == WAIT_OBJECT_0)
    {
        if (bShutdownVideoThread)
            break;

        QWORD renderStartTime = GetQPCTimeNS();
        totalStreamTime = DWORD((renderStartTime-streamTimeStart)/1000000);

        bool bRenderView = !IsIconic(hwndMain) && bRenderViewEnabled;

        QWORD renderStartTimeMS = renderStartTime/1000000;

        QWORD curStreamTime = latestVideoTimeNS;
        if (!lastStreamTime)
            lastStreamTime = curStreamTime-frameLengthNS;
        QWORD frameDelta = curStreamTime-lastStreamTime;
        //if (!lastStreamTime)
        //    lastStreamTime = renderStartTime-frameLengthNS;
        //QWORD frameDelta = renderStartTime-lastStreamTime;
        double fSeconds = double(frameDelta)*0.000000001;
        //lastStreamTime = renderStartTime;

        bool bUpdateBPS = false;

        profileIn("video thread frame");

        //Log(TEXT("Stream Time: %llu"), curStreamTime);
        //Log(TEXT("frameDelta: %lf"), fSeconds);

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

        if(!pushToTalkDown && pushToTalkTimeLeft > 0)
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

        if (bPleaseEnableProjector)
            ActuallyEnableProjector();
        else if(bPleaseDisableProjector)
            DisableProjector();

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

        QWORD curBytesSent = 0;
        
        if (network) {
            curBytesSent = network->GetCurrentSentBytes();
            curFramesDropped = network->NumDroppedFrames();
        } else if (numSecondsWaited) {
            //reset stats if the network disappears
            bytesPerSec = 0;
            bpsTime = 0;
            numSecondsWaited = 0;
            curBytesSent = 0;
            zero(lastBytesSent, sizeof(lastBytesSent));
        }

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

        if(network) curStrain = network->GetPacketStrain();

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
            if(!lastRenderTexture)
            {
                lastRenderTexture = CreateTexture(baseCX, baseCY, GS_BGRA, NULL, FALSE, TRUE);
                if(lastRenderTexture)
                {
                    D3D10Texture *d3dTransitionTex = static_cast<D3D10Texture*>(lastRenderTexture);
                    D3D10Texture *d3dSceneTex = static_cast<D3D10Texture*>(mainRenderTextures[lastRenderTarget]);
                    GetD3D()->CopyResource(d3dTransitionTex->texture, d3dSceneTex->texture);
                }
                else
                    bTransitioning = false;
            }
            else if(transitionAlpha >= 1.0f)
            {
                delete lastRenderTexture;
                lastRenderTexture = NULL;

                bTransitioning = false;
            }
        }

        if(bTransitioning)
        {
            transitionAlpha += float(fSeconds) * 5.0f;
            if(transitionAlpha > 1.0f)
                transitionAlpha = 1.0f;

            SetRenderTarget(transitionTexture);

            Shader *oldPixelShader = GetCurrentPixelShader();
            LoadPixelShader(transitionPixelShader);

            transitionPixelShader->SetFloat(hTransitionTime, transitionAlpha);
            LoadTexture(mainRenderTextures[curRenderTarget], 1U);

            DrawSpriteEx(lastRenderTexture, 0xFFFFFFFF,
                0, 0, baseSize.x, baseSize.y, 0.0f, 0.0f, 1.0f, 1.0f);

            LoadTexture(nullptr, 1U);
            LoadPixelShader(oldPixelShader);
        }

        EnableBlending(FALSE);


        //------------------------------------
        // render the mini view thingy

        if (bProjector) {
            SetRenderTarget(projectorTexture);

            Vect2 renderFrameSize, renderFrameOffset;
            Vect2 projectorSize = Vect2(float(projectorWidth), float(projectorHeight));

            float projectorAspect = (projectorSize.x / projectorSize.y);
            float baseAspect = (baseSize.x / baseSize.y);

            if (projectorAspect < baseAspect) {
                float fProjectorWidth = float(projectorWidth);

                renderFrameSize   = Vect2(fProjectorWidth, fProjectorWidth / baseAspect);
                renderFrameOffset = Vect2(0.0f, (projectorSize.y-renderFrameSize.y) * 0.5f);
            } else {
                float fProjectorHeight = float(projectorHeight);

                renderFrameSize   = Vect2(fProjectorHeight * baseAspect, fProjectorHeight);
                renderFrameOffset = Vect2((projectorSize.x-renderFrameSize.x) * 0.5f, 0.0f);
            }

            DrawPreview(renderFrameSize, renderFrameOffset, projectorSize, curRenderTarget, Preview_Projector);

            SetRenderTarget(NULL);
        }

        if(bRenderView)
        {
            // Cache
            const Vect2 renderFrameSize = GetRenderFrameSize();
            const Vect2 renderFrameOffset = GetRenderFrameOffset();
            const Vect2 renderFrameCtrlSize = GetRenderFrameControlSize();

            SetRenderTarget(NULL);
            DrawPreview(renderFrameSize, renderFrameOffset, renderFrameCtrlSize, curRenderTarget,
                    bFullscreenMode ? Preview_Fullscreen : Preview_Standard);

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

        switch(colorDesc.matrix)
        {
        case ColorMatrix_GBR:
            yuvScalePixelShader->SetMatrix(hMatrix, colorDesc.fullRange ? (float*)yuvFullMat[0] : (float*)yuvMat[0]);
            break;
        case ColorMatrix_YCgCo:
            yuvScalePixelShader->SetMatrix(hMatrix, colorDesc.fullRange ? (float*)yuvFullMat[1] : (float*)yuvMat[1]);
            break;
        case ColorMatrix_BT2020NCL:
            yuvScalePixelShader->SetMatrix(hMatrix, colorDesc.fullRange ? (float*)yuvFullMat[2] : (float*)yuvMat[2]);
            break;
        case ColorMatrix_BT709:
            yuvScalePixelShader->SetMatrix(hMatrix, colorDesc.fullRange ? (float*)yuvFullMat[3] : (float*)yuvMat[3]);
            break;
        case ColorMatrix_SMPTE240M:
            yuvScalePixelShader->SetMatrix(hMatrix, colorDesc.fullRange ? (float*)yuvFullMat[4] : (float*)yuvMat[4]);
            break;
        default:
            yuvScalePixelShader->SetMatrix(hMatrix, colorDesc.fullRange ? (float*)yuvFullMat[5] : (float*)yuvMat[5]);
        }

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
            DrawSpriteEx(transitionTexture, 0xFFFFFFFF, 0.0f, 0.0f, scaleSize.x, scaleSize.y, 0.0f, 0.0f, 1.0f, 1.0f);
        else
            DrawSpriteEx(mainRenderTextures[curRenderTarget], 0xFFFFFFFF, 0.0f, 0.0f, outputSize.x, outputSize.y, 0.0f, 0.0f, 1.0f, 1.0f);

        //------------------------------------

        if (bProjector && !copyWait)
            projectorSwap->Present(0, 0);

        if(bRenderView && !copyWait)
            static_cast<D3D10System*>(GS)->swap->Present(0, 0);

        OSLeaveMutex(hSceneMutex);

        //------------------------------------
        // present/upload

        profileIn("GPU download and conversion");

        bEncode = true;

        if(copyWait)
        {
            copyWait--;
            bEncode = false;
        }
        else
        {
            //audio sometimes takes a bit to start -- do not start processing frames until audio has started capturing
            if(!bRecievedFirstAudioFrame)
            {
                static bool bWarnedAboutNoAudio = false;
                if (renderStartTimeMS-firstFrameTimeMS > 10000 && !bWarnedAboutNoAudio)
                {
                    bWarnedAboutNoAudio = true;
                    //AddStreamInfo (TEXT ("WARNING: OBS is not receiving audio frames. Please check your audio devices."), StreamInfoPriority_Critical); 
                }
                bEncode = false;
            }
            else if(bFirstFrame)
            {
                firstFrameTimestamp = lastStreamTime/1000000;
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

        lastStreamTime = curStreamTime;

        if(bEncode)
        {
            UINT prevCopyTexture = (curCopyTexture == 0) ? NUM_RENDER_BUFFERS-1 : curCopyTexture-1;

            ID3D10Texture2D *copyTexture = copyTextures[curCopyTexture];
            profileIn("CopyResource");

            if(!bFirstEncode && bUseThreaded420)
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
                            for(int i=0; i<numThreads; i++)
                            {
                                convertInfo[i].input     = (LPBYTE)map.pData;
                                convertInfo[i].inPitch   = map.RowPitch;
                                if(bUsingQSV)
                                {
                                    mfxFrameData& data = nextPicOut.mfxOut->Data;
                                    videoEncoder->RequestBuffers(&data);
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
                                SetEvent(convertInfo[i].hSignalConvert);
                            }

                            if(bFirstEncode)
                                bFirstEncode = bEncode = false;
                        }
                        else
                        {
                            if(bUsingQSV)
                            {
                                mfxFrameData& data = picOut.mfxOut->Data;
                                videoEncoder->RequestBuffers(&data);
                                LPBYTE output[] = {data.Y, data.UV};
                                Convert444toNV12((LPBYTE)map.pData, outputCX, map.RowPitch, data.Pitch, outputCY, 0, outputCY, output);
                            }
                            else
                                Convert444toNV12((LPBYTE)map.pData, outputCX, map.RowPitch, outputCX, outputCY, 0, outputCY, picOut.picOut->img.plane);
                            prevTexture->Unmap(0);
                        }

                        profileOut;
                    }

                    if(bEncode)
                    {
                        //encodeThreadProfiler.reset(::new ProfilerNode(TEXT("EncodeThread"), true));
                        //encodeThreadProfiler->MonitorThread(hEncodeThread);
                        InterlockedExchangePointer((volatile PVOID*)&curFramePic, &picOut);
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

                        message << TEXT(" This error can also occur if you have enabled opencl in x264 custom settings.");

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

            if (bCongestionControl && bDynamicBitrateSupported && !bTestStream && totalStreamTime > 15000)
            {
                if (curStrain > 25)
                {
                    if (renderStartTimeMS - lastAdjustmentTime > 1500)
                    {
                        if (currentBitRate > 100)
                        {
                            currentBitRate = (int)(currentBitRate * (1.0 - (curStrain / 400)));
                            App->GetVideoEncoder()->SetBitRate(currentBitRate, -1);
                            if (!adjustmentStreamId)
                                adjustmentStreamId = App->AddStreamInfo (FormattedString(TEXT("Congestion detected, dropping bitrate to %d kbps"), currentBitRate).Array(), StreamInfoPriority_Low);
                            else
                                App->SetStreamInfo(adjustmentStreamId, FormattedString(TEXT("Congestion detected, dropping bitrate to %d kbps"), currentBitRate).Array());

                            bUpdateBPS = true;
                        }

                        lastAdjustmentTime = renderStartTimeMS;
                    }
                }
                else if (currentBitRate < defaultBitRate && curStrain < 5 && lastStrain < 5)
                {
                    if (renderStartTimeMS - lastAdjustmentTime > 5000)
                    {
                        if (currentBitRate < defaultBitRate)
                        {
                            currentBitRate += (int)(defaultBitRate * 0.05);
                            if (currentBitRate > defaultBitRate)
                                currentBitRate = defaultBitRate;
                        }

                        App->GetVideoEncoder()->SetBitRate(currentBitRate, -1);
                        /*if (!adjustmentStreamId)
                            App->AddStreamInfo (FormattedString(TEXT("Congestion clearing, raising bitrate to %d kbps"), currentBitRate).Array(), StreamInfoPriority_Low);
                        else
                            App->SetStreamInfo(adjustmentStreamId, FormattedString(TEXT("Congestion clearing, raising bitrate to %d kbps"), currentBitRate).Array());*/

                        App->RemoveStreamInfo(adjustmentStreamId);
                        adjustmentStreamId = 0;

                        bUpdateBPS = true;

                        lastAdjustmentTime = renderStartTimeMS;
                    }
                }
            }
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

        //------------------------------------
        // we're about to sleep so we should flush the d3d command queue
        profileIn("flush");
        GetD3D()->Flush();
        profileOut;
        profileOut;
        profileOut; //frame

        //------------------------------------
        // frame sync

        //QWORD renderStopTime = GetQPCTimeNS();

        if(bWasLaggedFrame = (frameDelta > frameLengthNS))
        {
            numLongFrames++;
            if(bLogLongFramesProfile && (numLongFrames/float(max(1, numTotalFrames)) * 100.) > logLongFramesProfilePercentage)
                DumpLastProfileData();
        }

        //OSDebugOut(TEXT("Frame adjust time: %d, "), frameTimeAdjust-totalTime);

        numTotalFrames++;
    }

    DisableProjector();

    //encodeThreadProfiler.reset();

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

            if(!bFirstEncode)
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

    Log(TEXT("Total frames rendered: %d, number of late frames: %d (%0.2f%%) (it's okay for some frames to be late)"), numTotalFrames, numLongFrames, (numTotalFrames > 0) ? (double(numLongFrames)/double(numTotalFrames))*100.0 : 0.0f);
}
