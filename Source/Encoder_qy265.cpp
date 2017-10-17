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

#include <fstream>
#include <inttypes.h>
#include <ws2tcpip.h>

extern "C"
{
#include "../x264/x264.h"
#include "../qy265/qy265def.h"
#include "../qy265/qy265enc.h"

}

#define DUMP_BS
#define ENCODE_LENGTH
#if !defined(ENCODE_LENGTH)
#define  ENCODE_START_CODE 1
#endif

void get_qy265_log(void *param, int i_level, const char *psz, va_list argptr)
{
    String chi;

    chi << TEXT("qy265: ") << String(psz);
    chi.FindReplace(TEXT("%s"), TEXT("%S"));

    OSDebugOutva(chi, argptr);

    chi.FindReplace(TEXT("\r"), TEXT(""));
    chi.FindReplace(TEXT("\n"), TEXT(""));

    if (chi == TEXT("qy265: OpenCL: fatal error, aborting encode") || chi == TEXT("qy265: OpenCL: Invalid value."))
    {
        if (App->IsRunning())
        {
            // FIXME: currently due to the way OBS handles the stream report, if reconnect is enabled and this error happens
            // outside of the 30 second "no reconnect" window, no error dialog is shown to the user. usually qy265 opencl errors
            // will happen immediately though.
            Log(TEXT("Aborting stream due to qy265 opencl error"));
            App->SetStreamReport(TEXT("qy265 OpenCL Error\r\n\r\nqy265 encountered an error attempting to use OpenCL. This may be due to unsupported hardware or outdated drivers.\r\n\r\nIt is recommended that you remove opencl=true from your qy265 advanced settings."));
            PostMessage(hwndMain, OBS_REQUESTSTOP, 1, 0);
        }
    }

    Logva(chi.Array(), argptr);
}


struct VideoPacket
{
    List<BYTE> Packet;
    inline void FreeData() { Packet.Clear(); }
};

const float baseCRF = 22.0f;

bool valid_qy265_string(const String &str, const char **x264StringList)
{
    do
    {
        if (str.CompareI(String(*x264StringList)))
            return true;
    } while (*++x264StringList != 0);

    return false;
}

class QY265Encoder : public VideoEncoder
{
    QY265EncConfig paramData;
    void* qy265;

    QY265Picture picOut;

    int cur_pts_time;
    QY265Nal *pp_nal;
    int pi_nal;

    int fps_ms;

    bool bRequestKeyframe;

    UINT width, height;

    String curPreset, curTune, curLatency,curProfile;

    bool bFirstFrameProcessed;

    bool bUseCBR, bUseCFR, bPadCBR;

    List<VideoPacket> CurrentPackets;
    List<BYTE> HeaderPacket, SEIData;

    INT64 delayOffset;

    int frameShift;
#ifdef DUMP_BS
    std::fstream m_bs;
#endif
    inline void ClearPackets()
    {
        for (UINT i = 0; i<CurrentPackets.Num(); i++)
            CurrentPackets[i].FreeData();
        CurrentPackets.Clear();
    }

    inline void SetBitRateParams(DWORD maxBitrate, DWORD bufferSize)
    {
        //-1 means ignore so we don't have to know both settings

//         if (maxBitrate != -1)
//             paramData.rc.i_vbv_max_bitrate = maxBitrate; //vbv-maxrate
// 
//         if (bufferSize != -1)
//             paramData.rc.i_vbv_buffer_size = bufferSize; //vbv-bufsize

        if (bUseCBR)
            paramData.bitrateInkbps = maxBitrate;
    }

public:
    QY265Encoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitrate, int bufferSize, bool bUseCFR)
    {
#ifdef DUMP_BS
        m_bs.open("bs.265",std::ios::binary|std::ios::out);
#endif
        curPreset = preset;

        fps_ms = 1000 / fps;
        paramData.frameRate = fps;
        StringList paramList;

        curTune = AppConfig->GetString(TEXT("Video Encoding"), TEXT("QY265Tune"), TEXT("default"));

        BOOL bUseCustomParams = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseCustomQY265Settings"));
        if (bUseCustomParams)
        {
            String strCustomParams = AppConfig->GetString(TEXT("Video Encoding"), TEXT("CustomQY265Settings"));
            strCustomParams.KillSpaces();

            if (strCustomParams.IsValid())
            {
                Log(TEXT("Using custom qy265 settings: \"%s\""), strCustomParams.Array());

                strCustomParams.GetTokenList(paramList, ' ', FALSE);
                for (UINT i = 0; i<paramList.Num(); i++)
                {
                    String &strParam = paramList[i];
                    if (!schr(strParam, '='))
                        continue;

                    String strParamName = strParam.GetToken(0, '=');
                    String strParamVal = strParam.GetTokenOffset(1, '=');

                    if (strParamName.CompareI(TEXT("preset")))
                    {
                        if (valid_qy265_string(strParamVal, (const char**)qy265_preset_names))
                            curPreset = strParamVal;
                        else
                            Log(TEXT("invalid preset: %s"), strParamVal.Array());

                        paramList.Remove(i--);
                    }
                    else if (strParamName.CompareI(TEXT("tune")))
                    {
                        if (valid_qy265_string(strParamVal, (const char**)qy265_tunes_names))
                            curTune = strParamVal;
                        else
                            Log(TEXT("invalid tune: %s"), strParamVal.Array());

                        paramList.Remove(i--);
                    }
                    else if (strParamName.CompareI(TEXT("latency")))
                    {
                        if (valid_qy265_string(strParamVal, (const char**)qy265_latency_names))
                            curLatency = strParamVal;
                        else
                            Log(TEXT("invalid latency: %s"), strParamVal.Array());

                        paramList.Remove(i--);
                    }
                }
            }
        }

        zero(&paramData, sizeof(paramData));

        LPSTR lpPreset = curPreset.CreateUTF8String();
        LPSTR lpTune = curTune.CreateUTF8String();
        LPSTR lpLatency = curLatency.CreateUTF8String();
        if (QY265ConfigDefaultPreset(&paramData, lpPreset, "default", "zerolatency"))
            Log(TEXT("Failed to set qy265 defaults: %s/%s/%s"), curPreset.Array(), curTune.Array(), curLatency.Array());

        Free(lpTune);
        Free(lpPreset);

        this->width = width;
        this->height = height;

//        paramData.b_deterministic = false;
        bUseCBR = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseCBR"), 1) != 0;
        bPadCBR = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("PadCBR"), 1) != 0;
        this->bUseCFR = bUseCFR;

        SetBitRateParams(maxBitrate, bufferSize);

        paramData.bHeaderBeforeKeyframe = true;
        if (bUseCBR)
        {
            paramData.rc = 2;
            paramData.bitrateInkbps = maxBitrate;
        }
        else
        {
            paramData.rc = 3;
            paramData.crf = (UINT)(baseCRF + float(10 - quality));
            //paramData.crf = 30;
        }

        UINT keyframeInterval = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("KeyframeInterval"), 0);

        //paramData.b_vfr_input = !bUseCFR;
        paramData.picWidth = width;
        paramData.picHeight = height;
        if (paramData.picHeight*paramData.picWidth >= 1920*1080)
        {
            paramData.threads = 6;
        }
        else if (paramData.picHeight*paramData.picWidth >= 1280 * 720)
        {
            paramData.threads = 4;
        }
        else if (paramData.picHeight*paramData.picWidth >= 640 * 360)
        {
            paramData.threads = 2;
        }
        else{
            paramData.threads = 1;
        }
        
        if (paramData.threads >1)
        {
            paramData.enWavefront = 1;
            paramData.enFrameParallel = 1;
        }
        
        if (keyframeInterval)
            paramData.iIntraPeriod = fps*keyframeInterval;
        else
        {
            paramData.iIntraPeriod = 256;
        }

        //paramData.pf_log = get_qy265_log;
        paramData.logLevel = 0;

//         if (scmpi(curProfile, L"main") == 0)
//             paramData.i_level_idc = 41; // to ensure compatibility with portable devices

        for (UINT i = 0; i<paramList.Num(); i++)
        {
            String &strParam = paramList[i];
            if (!schr(strParam, '='))
                continue;

            String strParamName = strParam.GetToken(0, '=');
            String strParamVal = strParam.GetTokenOffset(1, '=');

            if (strParamName.CompareI(TEXT("fps")) ||
                strParamName.CompareI(TEXT("force-cfr")))
            {
                Log(TEXT("The custom qy265 command '%s' is unsupported, use the application settings instead"), strParam.Array());
                continue;
            }
            else
            {
                LPSTR lpParam = strParamName.CreateUTF8String();
                LPSTR lpVal = strParamVal.CreateUTF8String();

                if (QY265ConfigParse(&paramData, lpParam, lpVal) != 0)
                    Log(TEXT("The custom qy265 command '%s' failed"), strParam.Array());

                Free(lpParam);
                Free(lpVal);
            }
        }

        if (bUse444){
            Log(TEXT("The colorspace 444 is not surportted"));
        }

        int32_t errcode = 0;
        qy265 = QY265EncoderOpen(&paramData,&errcode);
        if (!qy265)
            CrashError(TEXT("Could not initialize qy265"));

        Log(TEXT("------------------------------------------"));
        Log(TEXT("%s"), GetInfoString().Array());
        Log(TEXT("------------------------------------------"));

        DataPacket packet;
        GetHeaders(packet);
    }

    ~QY265Encoder()
    {
        ClearPackets();
        QY265EncoderClose(qy265);
#ifdef DUMP_BS
        m_bs.close();
#endif
    }
    virtual bool isQY265() { return true; }

    bool Encode(LPVOID picInPtr, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD outputTimestamp, DWORD &out_pts)
    {
        x264_picture_t *x264picIn = (x264_picture_t*)picInPtr;
        QY265Picture picIn_;
        QY265YUV yuv_;
        picIn_.yuv = &yuv_;
        QY265Picture* picIn;
        if (x264picIn)
        {
            picIn = &picIn_;
            picIn->pts = x264picIn->i_pts;
            picIn->yuv->iWidth = width;
            picIn->yuv->iHeight = height;
            picIn->yuv->iStride[0] = x264picIn->img.i_stride[0];
            picIn->yuv->iStride[1] = x264picIn->img.i_stride[1];
            picIn->yuv->iStride[2] = x264picIn->img.i_stride[2];
            picIn->yuv->pData[0] = x264picIn->img.plane[0];
            picIn->yuv->pData[1] = x264picIn->img.plane[1];
            picIn->yuv->pData[2] = x264picIn->img.plane[2];
        }
        else{
            picIn = NULL;
        }
        QY265Nal *nalOut;
        int nalNum;

        packets.Clear();
        ClearPackets();

        if (bRequestKeyframe)
            QY265EncoderKeyFrameRequest(qy265);

        if (QY265EncoderEncodeFrame(qy265, &nalOut, &nalNum, picIn, &picOut,1) < 0)
        {
            AppWarning(TEXT("qy265 encode failed"));
            return false;
        }

        if (bRequestKeyframe)
        {
            bRequestKeyframe = false;
        }

        if (!bFirstFrameProcessed && nalNum)
        {
            delayOffset = -picOut.dts;
            bFirstFrameProcessed = true;
        }

        INT64 ts = INT64(outputTimestamp);
        int timeOffset;

        //if frame duplication is being used, the shift will be insignificant, so just don't bother adjusting audio
        timeOffset = int(picOut.pts - picOut.dts);
        timeOffset += frameShift;

        out_pts = (DWORD)picOut.pts;

        if (nalNum && timeOffset < 0)
        {
            frameShift -= timeOffset;
            timeOffset = 0;
        }

        //OSDebugOut(TEXT("inpts: %005lld, dts: %005lld, pts: %005lld, timestamp: %005d, offset: %005d, newoffset: %005lld\n"), picIn->i_pts, picOut.i_dts, picOut.i_pts, outputTimestamp, timeOffset, picOut.i_pts-picOut.i_dts);

        timeOffset = htonl(timeOffset);

        BYTE *timeOffsetAddr = ((BYTE*)&timeOffset) + 1;

        VideoPacket *newPacket = NULL;

        PacketType bestType = PacketType_VideoDisposable;
        bool bFoundFrame = false;

        INT64 iTotalLen = 0;
        for (int i = 0; i < nalNum; i++)
        {
            iTotalLen += nalOut[i].iSize;
        }

        for (int i = 0; i<nalNum; i++)
        {
            QY265Nal &nal = nalOut[i];
#ifdef DUMP_BS
            m_bs.write((char*)nal.pPayload, nal.iSize);
#endif
//             if (nal.naltype == NAL_SEI)
//             {
//                 BYTE *skip = nal.pPayload;
//                 while (*(skip++) != 0x1);
//                 int skipBytes = (int)(skip - nal.pPayload);
// 
//                 int newPayloadSize = (nal.iSize - skipBytes);
// 
//                 if (nal.pPayload[skipBytes + 1] == 0x5) {
//                     SEIData.Clear();
//                     BufferOutputSerializer packetOut(SEIData);
// 
//                     packetOut.OutputDword(htonl(newPayloadSize));
//                     packetOut.Serialize(nal.pPayload + skipBytes, newPayloadSize);
//                 }
//                 else {
//                     if (!newPacket)
//                         newPacket = CurrentPackets.CreateNew();
// 
//                     BufferOutputSerializer packetOut(newPacket->Packet);
// 
//                     packetOut.OutputDword(htonl(newPayloadSize));
//                     packetOut.Serialize(nal.pPayload + skipBytes, newPayloadSize);
//                 }
//             }
//             else
//             if (nal.naltype == NAL_UNIT_TYPE_RASL_R)
//             {
//                 BYTE *skip = nal.pPayload;
//                 while (*(skip++) != 0x1);
//                 int skipBytes = (int)(skip - nal.pPayload);
// 
//                 int newPayloadSize = (nal.iSize - skipBytes);
// 
//                 if (!newPacket)
//                     newPacket = CurrentPackets.CreateNew();
// 
//                 BufferOutputSerializer packetOut(newPacket->Packet);
// 
//                 packetOut.OutputDword(htonl(newPayloadSize));
//                 packetOut.Serialize(nal.pPayload + skipBytes, newPayloadSize);
//             }
//             else 
            if (nal.naltype == NAL_UNIT_TYPE_VPS_NUT || nal.naltype == NAL_UNIT_TYPE_SPS_NUT || nal.naltype == NAL_UNIT_TYPE_PPS_NUT
                || nal.naltype == NAL_UNIT_TYPE_IDR_W_RADL || nal.naltype == NAL_UNIT_TYPE_CRA_NUT
                || nal.naltype == NAL_UNIT_TYPE_RASL_R || nal.naltype == NAL_UNIT_TYPE_RASL_R
                || nal.naltype == NAL_UNIT_TYPE_TRAIL_R || nal.naltype == NAL_UNIT_TYPE_TRAIL_N)
            {
                BYTE *skip = nal.pPayload;
#ifdef ENCODE_LENGTH
                while (*(skip++) != 0x1){
                };
                int skipBytes = (int)(skip - nal.pPayload);
#elif ENCODE_START_CODE
                int skipBytes = 0;
#endif
                if (!newPacket)
                    newPacket = CurrentPackets.CreateNew();

                if (!bFoundFrame)
                {
                    newPacket->Packet.Insert(0, (nal.naltype == NAL_UNIT_TYPE_IDR_W_RADL || nal.naltype == NAL_UNIT_TYPE_CRA_NUT 
                        || nal.naltype == NAL_UNIT_TYPE_VPS_NUT || nal.naltype == NAL_UNIT_TYPE_SPS_NUT || nal.naltype == NAL_UNIT_TYPE_PPS_NUT) ? 0x1c : 0x2c);
                    newPacket->Packet.Insert(1, 1);
                    newPacket->Packet.InsertArray(2, timeOffsetAddr, 3);

                    bFoundFrame = true;
                }

                int newPayloadSize = nal.iSize - skipBytes;
                BufferOutputSerializer packetOut(newPacket->Packet);
#ifdef ENCODE_LENGTH
                packetOut.OutputDword(htonl(newPayloadSize));
#endif
                packetOut.Serialize(nal.pPayload + skipBytes, newPayloadSize);

                switch (nal.naltype)
                {
                case NAL_UNIT_TYPE_TRAIL_N:         bestType = MAX(bestType, PacketType_VideoDisposable);  break;
                case NAL_UNIT_TYPE_RASL_N:          bestType = MAX(bestType, PacketType_VideoDisposable);  break;
                case NAL_UNIT_TYPE_RASL_R:          bestType = MAX(bestType, PacketType_VideoLow);         break;
                case NAL_UNIT_TYPE_TRAIL_R:         bestType = MAX(bestType, PacketType_VideoHigh);        break;
                case NAL_UNIT_TYPE_IDR_W_RADL:
                case NAL_UNIT_TYPE_CRA_NUT:
                case NAL_UNIT_TYPE_VPS_NUT:         bestType = MAX(bestType, PacketType_VideoHighest);     break;
                }
            }
            else
                continue;
        }

        packetTypes << bestType;

        if (CurrentPackets.Num()) {
            packets.SetSize(1);
            packets[0].lpPacket = CurrentPackets[0].Packet.Array();
            packets[0].size = CurrentPackets[0].Packet.Num();
        }

        return true;
    }

    void GetHeaders(DataPacket &packet)
    {
        if (!HeaderPacket.Num())
        {
            QY265Nal *nalOut;
            int nalNum;

            QY265EncoderEncodeHeaders(qy265, &nalOut, &nalNum);

            BufferOutputSerializer headerOut(HeaderPacket);
#ifdef ENCODE_LENGTH
            headerOut.OutputByte(0x1c);
            headerOut.OutputByte(0);
            headerOut.OutputByte(0);
            headerOut.OutputByte(0);
            headerOut.OutputByte(0);
            headerOut.OutputByte(1); //configurationVersion
            headerOut.OutputByte(0x01); //  unsigned int(2) general_profile_space;*unsigned int(1)general_tier_flag;*unsigned int(5) general_profile_idc;
            headerOut.OutputDword(0x60);//general_profile_compatibility_flags
            headerOut.OutputDword(0);//general_constraint_indicator_flags>>16
            headerOut.OutputShort(0);//general_constraint_indicator_flags
            headerOut.OutputByte(0x00); //general_level_idc
            headerOut.OutputShort(0x00F0);//min_spatial_segmentation_idc | 0xf000
            headerOut.OutputByte(0xFc); //parallelismType | 0xfc
            headerOut.OutputByte(0xFd); //chromaFormat | 0xfc
            headerOut.OutputByte(0xF8); //bitDepthLumaMinus8 | 0xf8
            headerOut.OutputByte(0xF8); //bitDepthChromaMinus8 | 0xf8
            headerOut.OutputShort(0); //avgFrameRate
            headerOut.OutputByte(0x0F);// bit(2) constantFrameRate;* bit(3) numTemporalLayers;* bit(1) temporalIdNested;* unsigned int(2) lengthSizeMinusOne;
            int32_t numOfArrays = nalNum;
            headerOut.OutputByte(numOfArrays); //numOfArrays
            for (int32_t i = 0; i < numOfArrays; ++i)
            {
                headerOut.OutputByte(nalOut[i].naltype); //hvcc->array[i].array_completeness << 7 |hvcc->array[i].NAL_unit_type & 0x3f
                headerOut.OutputWord(0x0100);
                headerOut.OutputWord(htons(nalOut[i].iSize - 4));
                headerOut.Serialize(nalOut[i].pPayload + 4, nalOut[i].iSize - 4);
            }

#elif ENCODE_START_CODE
            headerOut.OutputByte(0x1c);
            headerOut.OutputByte(0);
            headerOut.OutputByte(0);
            headerOut.OutputByte(0);
            headerOut.OutputByte(0);
            headerOut.OutputByte(0); //configurationVersion
            headerOut.Serialize(nalOut[0].pPayload, nalOut[0].iSize + nalOut[1].iSize + nalOut[2].iSize);
#endif
        }
        packet.lpPacket = HeaderPacket.Array();
        packet.size = HeaderPacket.Num();
    }

    virtual void GetSEI(DataPacket &packet)
    {
        packet.lpPacket = SEIData.Array();
        packet.size = SEIData.Num();
    }

    int GetBitRate() const
    {
        return paramData.bitrateInkbps;
    }

    String GetInfoString() const
    {
        String strInfo;

        strInfo << TEXT("Video Encoding: qy265") <<
            TEXT("\r\n    fps: ") << IntString((int32_t)(paramData.frameRate)) <<
            TEXT("\r\n    width: ") << IntString(width) << TEXT(", height: ") << IntString(height) <<
            TEXT("\r\n    preset: ") << curPreset <<
            TEXT("\r\n    profile: ") << curProfile <<
            TEXT("\r\n    keyint: ") << paramData.iIntraPeriod <<
            TEXT("\r\n    CBR: ") << CTSTR((bUseCBR) ? TEXT("yes") : TEXT("no")) <<
            TEXT("\r\n    CFR: ") << CTSTR((bUseCFR) ? TEXT("yes") : TEXT("no")) <<
            TEXT("\r\n    max bitrate: ") << IntString(paramData.bitrateInkbps);

        if (!bUseCBR)
        {
            strInfo << TEXT("\r\n    quality: ") << IntString(10 - int(paramData.crf - baseCRF));
        }

        return strInfo;
    }

    virtual bool DynamicBitrateSupported() const
    {
//        return (paramData.i_nal_hrd != X264_NAL_HRD_CBR);
        return false;
    }

    virtual bool SetBitRate(DWORD maxBitrate, DWORD bufferSize)
    {
        DWORD old_bitrate = paramData.bitrateInkbps;
        //DWORD old_buffer = paramData.rc.i_vbv_buffer_size;
        SetBitRateParams(maxBitrate, bufferSize);

        int retVal = 0;
        QY265EncoderReconfig(qy265, &paramData);
        if (retVal < 0)
            Log(TEXT("Could not set new encoder bitrate, error value %u"), retVal);
        else
        {
            String changes;
            if (old_bitrate != maxBitrate)
                changes << FormattedString(L"bitrate %d->%d", old_bitrate, maxBitrate);
//             if (old_buffer != bufferSize)
//                 changes << FormattedString(L"%sbuffer size %d->%d", changes.Length() ? L", " : L"", old_buffer, bufferSize);
            if (changes)
                Log(L"qy265: %s", changes.Array());
        }

        return retVal == 0;
    }

    virtual void RequestKeyframe()
    {
        bRequestKeyframe = true;
    }

    virtual int GetBufferedFrames()
    {
        return QY265EncoderDelayedFrames(qy265);
    }

    virtual bool HasBufferedFrames()
    {
        return QY265EncoderDelayedFrames(qy265) > 0;
    }
};


VideoEncoder* CreateQY265Encoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR)
{
    return new QY265Encoder(fps, width, height, quality, preset, bUse444, colorDesc, maxBitRate, bufferSize, bUseCFR);
}

