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

const float baseCRF = 15.0f;

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

    List<VideoPacket> CurrentPackets;
    List<BYTE> HeaderPacket;

    inline void ClearPackets()
    {
        for(UINT i=0; i<CurrentPackets.Num(); i++)
            CurrentPackets[i].FreeData();
        CurrentPackets.Clear();
    }

public:
    X264Encoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, int maxBitRate, int bufferSize)
    {
        traceIn(X264Encoder::X264Encoder);

        fps_ms = 1000/fps;

        zero(&paramData, sizeof(paramData));

        curPreset = preset;
        LPSTR lpPreset = curPreset.CreateUTF8String();
        x264_param_default_preset(&paramData, lpPreset, "fastdecode");

        Free(lpPreset);

        this->width  = width;
        this->height = height;

        //warning: messing with x264 settings without knowing what they do can seriously screw things up

        //x264_param_parse(&paramData, "ratetol", "0.1");
        //x264_param_parse(&paramData, "qcomp", "0.0");

        //paramData.i_threads             = 4;
        paramData.b_vfr_input           = 1;
        paramData.i_keyint_max          = fps*5;      //keyframe every 5 sec, should make this an option
        paramData.i_width               = width;
        paramData.i_height              = height;
        paramData.rc.i_vbv_max_bitrate  = maxBitRate; //vbv-maxrate
        paramData.rc.i_vbv_buffer_size  = bufferSize; //vbv-bufsize
        paramData.rc.i_rc_method        = X264_RC_CRF;
        paramData.rc.f_rf_constant      = baseCRF+float(10-quality);

        //paramData.i_nal_hrd = 1;

        paramData.i_fps_num = fps;
        paramData.i_fps_den = 1;

        paramData.i_timebase_num = 1;
        paramData.i_timebase_den = 1000;

        //paramData.pf_log                = get_x264_log;
        //paramData.i_log_level           = X264_LOG_INFO;

        if(bUse444) paramData.i_csp = X264_CSP_I444;

        x264 = x264_encoder_open(&paramData);
        if(!x264)
            CrashError(TEXT("Could not initialize x264"));

        Log(TEXT("------------------------------------------"));
        Log(TEXT("%s"), GetInfoString().Array());
        Log(TEXT("------------------------------------------"));

        DataPacket packet;
        GetHeaders(packet);

        traceOut;
    }

    ~X264Encoder()
    {
        traceIn(X264Encoder::~X264Encoder);

        ClearPackets();
        x264_encoder_close(x264);

        traceOut;
    }

    bool Encode(LPVOID picInPtr, List<DataPacket> &packets, DWORD &outputTimestamp)
    {
        traceIn(X264Encoder::Encode);

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

        int timeOffset = int(INT64(picOut.i_pts)-INT64(outputTimestamp));
        timeOffset = htonl(timeOffset);

        BYTE *timeOffsetAddr = ((BYTE*)&timeOffset)+1;

        for(int i=0; i<nalNum; i++)
        {
            x264_nal_t &nal = nalOut[i];

            if(nal.i_type == NAL_SLICE_IDR || nal.i_type == NAL_SLICE)
            {
                VideoPacket *newPacket = CurrentPackets.CreateNew();

                BYTE *skip = nal.p_payload;
                while(*(skip++) != 0x1);
                int skipBytes = (int)(skip-nal.p_payload);

                int newPayloadSize = (nal.i_payload-skipBytes);
                newPacket->Packet.SetSize(9+newPayloadSize);

                newPacket->Packet[0] = ((nal.i_type == NAL_SLICE_IDR) ? 0x17 : 0x27);
                newPacket->Packet[1] = 1;
                mcpy(newPacket->Packet+2, timeOffsetAddr, 3);
                *(DWORD*)(newPacket->Packet+5) = htonl(nal.i_payload-skipBytes);
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
        }

        packets.SetSize(CurrentPackets.Num());
        for(UINT i=0; i<packets.Num(); i++)
        {
            packets[i].lpPacket = CurrentPackets[i].Packet.Array();
            packets[i].size     = CurrentPackets[i].Packet.Num();
        }

        return true;

        traceOut;
    }

    void GetHeaders(DataPacket &packet)
    {
        traceIn(X264Encoder::GetHeaders);

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

        traceOut;
    }

    int GetBitRate() const {return paramData.rc.i_vbv_max_bitrate;}

    String GetInfoString() const
    {
        String strInfo;
        strInfo << TEXT("Video Encoding: x264")  <<
                   TEXT("\r\n    fps: ")         << IntString(paramData.i_fps_num) <<
                   TEXT("\r\n    width: ")       << IntString(width) << TEXT(", height: ") << IntString(height) <<
                   TEXT("\r\n    quality: ")     << IntString(10-int(paramData.rc.f_rf_constant-baseCRF)) <<
                   TEXT("\r\n    preset: ")      << curPreset <<
                   TEXT("\r\n    i444: ")        << CTSTR((paramData.i_csp == X264_CSP_I444) ? TEXT("yes") : TEXT("no")) <<
                   TEXT("\r\n    max bitrate: ") << IntString(paramData.rc.i_vbv_max_bitrate) <<
                   TEXT("\r\n    buffer size: ") << IntString(paramData.rc.i_vbv_buffer_size);

        return strInfo;
    }
};


VideoEncoder* CreateX264Encoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, int maxBitRate, int bufferSize)
{
    return new X264Encoder(fps, width, height, quality, preset, bUse444, maxBitRate, bufferSize);
}

