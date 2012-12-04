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
#include <ws2tcpip.h>

extern "C"
{
#include "../x264/x264.h"
}



void get_x264_log(void *param, int i_level, const char *psz, va_list argptr)
{
    String chi(psz);
    chi.FindReplace(TEXT("%s"), TEXT("%S"));

    OSDebugOutva(chi, argptr);
    Logva(chi, argptr);
}


struct VideoPacket
{
    List<BYTE> Packet;
    inline void FreeData() {Packet.Clear();}
};

const float baseCRF = 20.0f;

class X264Encoder : public VideoEncoder
{
    x264_param_t paramData;
    x264_t *x264;

    x264_picture_t picOut;

    int cur_pts_time;
    x264_nal_t *pp_nal;
    int pi_nal;

    int fps_ms;

    UINT width, height;

    String curPreset;

    bool bFirstFrameProcessed;

    bool bUseCBR;

    List<VideoPacket> CurrentPackets;
    List<BYTE> HeaderPacket;

    int delayOffset;

    inline void ClearPackets()
    {
        for(UINT i=0; i<CurrentPackets.Num(); i++)
            CurrentPackets[i].FreeData();
        CurrentPackets.Clear();
    }

public:
    X264Encoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, int maxBitRate, int bufferSize)
    {
        fps_ms = 1000/fps;

        zero(&paramData, sizeof(paramData));

        curPreset = preset;
        LPSTR lpPreset = curPreset.CreateUTF8String();
        x264_param_default_preset(&paramData, lpPreset, NULL);

        Free(lpPreset);

        this->width  = width;
        this->height = height;

        //warning: messing with x264 settings without knowing what they do can seriously screw things up

        //ratetol
        //qcomp

        //paramData.i_frame_reference     = 1; //ref=1
        //paramData.i_threads             = 4;

        bUseCBR = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseCBR")) != 0;

        if(bUseCBR)
        {
            paramData.rc.i_bitrate          = maxBitRate;
            paramData.rc.i_vbv_max_bitrate  = maxBitRate; //vbv-maxrate
            paramData.rc.i_vbv_buffer_size  = bufferSize; //vbv-bufsize
            paramData.i_nal_hrd             = X264_NAL_HRD_CBR;
            paramData.rc.i_rc_method        = X264_RC_ABR;
            paramData.rc.f_rf_constant      = 0.0f;
        }
        else
        {
            paramData.rc.i_vbv_max_bitrate  = maxBitRate; //vbv-maxrate
            paramData.rc.i_vbv_buffer_size  = bufferSize; //vbv-bufsize
            paramData.rc.i_rc_method        = X264_RC_CRF;
            paramData.rc.f_rf_constant      = baseCRF+float(10-quality);
        }

        paramData.b_vfr_input           = 1;
        paramData.i_keyint_max          = fps*5;      //keyframe every 5 sec, should make this an option
        paramData.i_width               = width;
        paramData.i_height              = height;
        paramData.vui.b_fullrange       = 0;          //specify full range input levels
        //paramData.i_frame_reference
        //paramData.b_in

        paramData.i_fps_num = fps;
        paramData.i_fps_den = 1;

        paramData.i_timebase_num = 1;
        paramData.i_timebase_den = 1000;

        //paramData.pf_log                = get_x264_log;
        //paramData.i_log_level           = X264_LOG_INFO;

        BOOL bUseCustomParams = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseCustomSettings"));
        if(bUseCustomParams)
        {
            String strCustomParams = AppConfig->GetString(TEXT("Video Encoding"), TEXT("CustomSettings"));
            strCustomParams.KillSpaces();

            if(strCustomParams.IsValid())
            {
                Log(TEXT("Using custom x264 settings: \"%s\""), strCustomParams.Array());

                StringList paramList;
                strCustomParams.GetTokenList(paramList, ' ', FALSE);
                for(UINT i=0; i<paramList.Num(); i++)
                {
                    String &strParam = paramList[i];
                    if(!schr(strParam, '='))
                        continue;

                    String strParamName = strParam.GetToken(0, '=');
                    String strParamVal  = strParam.GetTokenOffset(1, '=');

                    if( strParamName.CompareI(TEXT("fps")) /*|| 
                        strParamName.CompareI(TEXT("force-cfr"))*/)
                    {
                        continue;
                    }

                    LPSTR lpParam = strParamName.CreateUTF8String();
                    LPSTR lpVal   = strParamVal.CreateUTF8String();

                    x264_param_parse(&paramData, lpParam, lpVal);

                    Free(lpParam);
                    Free(lpVal);
                }
            }
        }

        if(bUse444) paramData.i_csp = X264_CSP_I444;

        x264 = x264_encoder_open(&paramData);
        if(!x264)
            CrashError(TEXT("Could not initialize x264"));

        Log(TEXT("------------------------------------------"));
        Log(TEXT("%s"), GetInfoString().Array());
        Log(TEXT("------------------------------------------"));

        DataPacket packet;
        GetHeaders(packet);
    }

    ~X264Encoder()
    {
        ClearPackets();
        x264_encoder_close(x264);
    }

    bool Encode(LPVOID picInPtr, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD outputTimestamp, int &ctsOffset)
    {
        x264_picture_t *picIn = (x264_picture_t*)picInPtr;

        x264_nal_t *nalOut;
        int nalNum;

        packets.Clear();
        ClearPackets();

        if(x264_encoder_encode(x264, &nalOut, &nalNum, picIn, &picOut) < 0)
        {
            AppWarning(TEXT("x264 encode failed"));
            return false;
        }

        if(!bFirstFrameProcessed && nalNum)
        {
            if(picOut.i_dts < 0)
                delayOffset = int(-picOut.i_dts);
            //Log(TEXT("cts: %u, timestamp: %u"), ctsOffset, outputTimestamp);
            bFirstFrameProcessed = true;
        }

        INT64 ts = INT64(outputTimestamp);
        int timeOffset = int(picOut.i_pts+INT64(delayOffset)-ts);

        timeOffset += ctsOffset;

        //dynamically adjust the CTS for the stream if it gets lower than the current value
        //(thanks to cyrus for suggesting to do this instead of a single shift)
        if(nalNum && timeOffset < 0)
        {
            ctsOffset -= timeOffset;
            timeOffset = 0;
        }

        //Log(TEXT("dts: %d, pts: %d, timestamp: %d, offset: %d"), picOut.i_dts, picOut.i_pts, outputTimestamp, timeOffset);

        timeOffset = htonl(timeOffset);

        BYTE *timeOffsetAddr = ((BYTE*)&timeOffset)+1;

        for(int i=0; i<nalNum; i++)
        {
            x264_nal_t &nal = nalOut[i];

            if(nal.i_type == NAL_SLICE_IDR || nal.i_type == NAL_SLICE || nal.i_type == NAL_SEI)
            {
                VideoPacket *newPacket = CurrentPackets.CreateNew();

                BYTE *skip = nal.p_payload;
                while(*(skip++) != 0x1);
                int skipBytes = (int)(skip-nal.p_payload);

                int newPayloadSize = (nal.i_payload-skipBytes);
                newPacket->Packet.SetSize(9+newPayloadSize);

                newPacket->Packet[0] = ((nal.i_type == NAL_SLICE_IDR || nal.i_type == NAL_SEI) ? 0x17 : 0x27);
                newPacket->Packet[1] = 1;
                mcpy(newPacket->Packet+2, timeOffsetAddr, 3);
                *(DWORD*)(newPacket->Packet+5) = htonl(newPayloadSize);
                mcpy(newPacket->Packet+9, nal.p_payload+skipBytes, newPayloadSize);
            }
            else if(nal.i_type == NAL_SPS)
            {
                VideoPacket *newPacket = CurrentPackets.CreateNew();
                BufferOutputSerializer headerOut(newPacket->Packet);

                headerOut.OutputByte(0x17);
                headerOut.OutputByte(0);
                headerOut.Serialize(timeOffsetAddr, 3);
                headerOut.OutputByte(1);
                headerOut.Serialize(nal.p_payload+5, 3);
                headerOut.OutputByte(0xff);
                headerOut.OutputByte(0xe1);
                headerOut.OutputWord(htons(nal.i_payload-4));
                headerOut.Serialize(nal.p_payload+4, nal.i_payload-4);

                x264_nal_t &pps = nalOut[i+1]; //the PPS always comes after the SPS

                headerOut.OutputByte(1);
                headerOut.OutputWord(htons(pps.i_payload-4));
                headerOut.Serialize(pps.p_payload+4, pps.i_payload-4);
            }
            else
                continue;

            switch(nal.i_ref_idc)
            {
                case NAL_PRIORITY_DISPOSABLE:   packetTypes << PacketType_VideoDisposable;  break;
                case NAL_PRIORITY_LOW:          packetTypes << PacketType_VideoLow;         break;
                case NAL_PRIORITY_HIGH:         packetTypes << PacketType_VideoHigh;        break;
                case NAL_PRIORITY_HIGHEST:      packetTypes << PacketType_VideoHighest;     break;
            }
        }

        packets.SetSize(CurrentPackets.Num());
        for(UINT i=0; i<packets.Num(); i++)
        {
            packets[i].lpPacket = CurrentPackets[i].Packet.Array();
            packets[i].size     = CurrentPackets[i].Packet.Num();
        }

        return true;
    }

    void GetHeaders(DataPacket &packet)
    {
        if(!HeaderPacket.Num())
        {
            x264_nal_t *nalOut;
            int nalNum;

            x264_encoder_headers(x264, &nalOut, &nalNum);

            for(int i=0; i<nalNum; i++)
            {
                x264_nal_t &nal = nalOut[i];

                if(nal.i_type == NAL_SPS)
                {
                    BufferOutputSerializer headerOut(HeaderPacket);

                    headerOut.OutputByte(0x17);
                    headerOut.OutputByte(0);
                    headerOut.OutputByte(0);
                    headerOut.OutputByte(0);
                    headerOut.OutputByte(0);
                    headerOut.OutputByte(1);
                    headerOut.Serialize(nal.p_payload+5, 3);
                    headerOut.OutputByte(0xff);
                    headerOut.OutputByte(0xe1);
                    headerOut.OutputWord(htons(nal.i_payload-4));
                    headerOut.Serialize(nal.p_payload+4, nal.i_payload-4);

                    x264_nal_t &pps = nalOut[i+1]; //the PPS always comes after the SPS

                    headerOut.OutputByte(1);
                    headerOut.OutputWord(htons(pps.i_payload-4));
                    headerOut.Serialize(pps.p_payload+4, pps.i_payload-4);
                }
            }
        }

        packet.lpPacket = HeaderPacket.Array();
        packet.size     = HeaderPacket.Num();
    }

    int GetBitRate() const {return paramData.rc.i_vbv_max_bitrate;}

    String GetInfoString() const
    {
        String strInfo;

        strInfo << TEXT("Video Encoding: x264")  <<
                   TEXT("\r\n    fps: ")         << IntString(paramData.i_fps_num) <<
                   TEXT("\r\n    width: ")       << IntString(width) << TEXT(", height: ") << IntString(height) <<
                   TEXT("\r\n    preset: ")      << curPreset <<
                   TEXT("\r\n    CBR: ")         << CTSTR((bUseCBR) ? TEXT("yes") : TEXT("no")) <<
                   TEXT("\r\n    max bitrate: ") << IntString(paramData.rc.i_vbv_max_bitrate);

        if(!bUseCBR)
        {
            strInfo << TEXT("\r\n    buffer size: ") << IntString(paramData.rc.i_vbv_buffer_size) << 
                       TEXT("\r\n    quality: ")     << IntString(10-int(paramData.rc.f_rf_constant-baseCRF));
        }

        return strInfo;
    }

    virtual bool DynamicBitrateSupported() const
    {
        return true;
    }

    virtual bool SetBitRate(DWORD maxBitrate, DWORD bufferSize)
    {
        if(bUseCBR)
        {
            paramData.rc.i_bitrate          = maxBitrate;
            paramData.rc.i_vbv_max_bitrate  = maxBitrate; //vbv-maxrate
            paramData.rc.i_vbv_buffer_size  = bufferSize; //vbv-bufsize
        }
        else
        {
            paramData.rc.i_vbv_max_bitrate = maxBitrate;
            paramData.rc.i_vbv_buffer_size = bufferSize;
        }

        int retVal = x264_encoder_reconfig(x264, &paramData);
        if(retVal < 0)
            Log(TEXT("Could not set new encoder bitrate, error value %u"), retVal);

        return retVal == 0;
    }
};


VideoEncoder* CreateX264Encoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, int maxBitRate, int bufferSize)
{
    return new X264Encoder(fps, width, height, quality, preset, bUse444, maxBitRate, bufferSize);
}

