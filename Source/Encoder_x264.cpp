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
    String chi;

    chi << TEXT("x264: ") << String(psz);
    chi.FindReplace(TEXT("%s"), TEXT("%S"));

    OSDebugOutva(chi, argptr);

    chi.FindReplace(TEXT("\r"), TEXT(""));
    chi.FindReplace(TEXT("\n"), TEXT(""));

    if (chi == TEXT("x264: OpenCL: fatal error, aborting encode") || chi == TEXT("x264: OpenCL: Invalid value."))
    {
        if (App->IsRunning())
        {
            // FIXME: currently due to the way OBS handles the stream report, if reconnect is enabled and this error happens
            // outside of the 30 second "no reconnect" window, no error dialog is shown to the user. usually x264 opencl errors
            // will happen immediately though.
            Log(TEXT("Aborting stream due to x264 opencl error"));
            App->SetStreamReport(TEXT("x264 OpenCL Error\r\n\r\nx264 encountered an error attempting to use OpenCL. This may be due to unsupported hardware or outdated drivers.\r\n\r\nIt is recommended that you remove opencl=true from your x264 advanced settings."));
            PostMessage(hwndMain, OBS_REQUESTSTOP, 1, 0);
        }
    }

    Logva(chi.Array(), argptr);
}


struct VideoPacket
{
    List<BYTE> Packet;
    inline void FreeData() {Packet.Clear();}
};

const float baseCRF = 22.0f;

bool valid_x264_string(const String &str, const char **x264StringList)
{
    do
    {
        if(str.CompareI(String(*x264StringList)))
            return true;
    } while (*++x264StringList != 0);

    return false;
}

class X264Encoder : public VideoEncoder
{
    x264_param_t paramData;
    x264_t *x264;

    x264_picture_t picOut;

    int cur_pts_time;
    x264_nal_t *pp_nal;
    int pi_nal;

    int fps_ms;

    bool bRequestKeyframe;

    UINT width, height;

    String curPreset, curTune, curProfile;

    bool bFirstFrameProcessed;

    bool bUseCBR, bUseCFR, bPadCBR;

    List<VideoPacket> CurrentPackets;
    List<BYTE> HeaderPacket, SEIData;

    INT64 delayOffset;

    int frameShift;

    inline void ClearPackets()
    {
        for(UINT i=0; i<CurrentPackets.Num(); i++)
            CurrentPackets[i].FreeData();
        CurrentPackets.Clear();
    }

    inline void SetBitRateParams(DWORD maxBitrate, DWORD bufferSize)
    {
        //-1 means ignore so we don't have to know both settings

        if (maxBitrate != -1)
            paramData.rc.i_vbv_max_bitrate  = maxBitrate; //vbv-maxrate

        if (bufferSize != -1)
            paramData.rc.i_vbv_buffer_size  = bufferSize; //vbv-bufsize

        if(bUseCBR)
            paramData.rc.i_bitrate = maxBitrate;
    }

public:
    X264Encoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitrate, int bufferSize, bool bUseCFR)
    {
        curPreset = preset;

        fps_ms = 1000/fps;

        StringList paramList;

        curProfile = AppConfig->GetString(TEXT("Video Encoding"), TEXT("X264Profile"), TEXT("high"));

        BOOL bUseCustomParams = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseCustomSettings"));
        if(bUseCustomParams)
        {
            String strCustomParams = AppConfig->GetString(TEXT("Video Encoding"), TEXT("CustomSettings"));
            strCustomParams.KillSpaces();

            if(strCustomParams.IsValid())
            {
                Log(TEXT("Using custom x264 settings: \"%s\""), strCustomParams.Array());

                strCustomParams.GetTokenList(paramList, ' ', FALSE);
                for(UINT i=0; i<paramList.Num(); i++)
                {
                    String &strParam = paramList[i];
                    if(!schr(strParam, '='))
                        continue;

                    String strParamName = strParam.GetToken(0, '=');
                    String strParamVal  = strParam.GetTokenOffset(1, '=');

                    if(strParamName.CompareI(TEXT("preset")))
                    {
                        if(valid_x264_string(strParamVal, (const char**)x264_preset_names))
                            curPreset = strParamVal;
                        else
                            Log(TEXT("invalid preset: %s"), strParamVal.Array());

                        paramList.Remove(i--);
                    }
                    else if(strParamName.CompareI(TEXT("tune")))
                    {
                        if(valid_x264_string(strParamVal, (const char**)x264_tune_names))
                            curTune = strParamVal;
                        else
                            Log(TEXT("invalid tune: %s"), strParamVal.Array());

                        paramList.Remove(i--);
                    }
                    else if(strParamName.CompareI(TEXT("profile")))
                    {
                        if(valid_x264_string(strParamVal, (const char **)x264_profile_names))
                            curProfile = strParamVal;
                        else
                            Log(TEXT("invalid profile: %s"), strParamVal.Array());

                        paramList.Remove(i--);
                    }
                }
            }
        }

        zero(&paramData, sizeof(paramData));

        LPSTR lpPreset = curPreset.CreateUTF8String();
        LPSTR lpTune = curTune.CreateUTF8String();

        if (x264_param_default_preset(&paramData, lpPreset, lpTune))
            Log(TEXT("Failed to set x264 defaults: %s/%s"), curPreset.Array(), curTune.Array());

        Free(lpTune);
        Free(lpPreset);

        this->width  = width;
        this->height = height;

        paramData.b_deterministic       = false;

        bUseCBR = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseCBR"), 1) != 0;
        bPadCBR = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("PadCBR"), 1) != 0;
        this->bUseCFR = bUseCFR;

        SetBitRateParams(maxBitrate, bufferSize);

        if(bUseCBR)
        {
            if(bPadCBR) paramData.rc.b_filler = 1;
            //if(bPadCBR) paramData.i_nal_hrd = X264_NAL_HRD_CBR;
            paramData.rc.i_rc_method    = X264_RC_ABR;
            paramData.rc.f_rf_constant  = 0.0f;
        }
        else
        {
            paramData.rc.i_rc_method    = X264_RC_CRF;
            paramData.rc.f_rf_constant  = baseCRF+float(10-quality);
        }

        UINT keyframeInterval = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("KeyframeInterval"), 0);

        paramData.b_vfr_input           = !bUseCFR;
        paramData.i_width               = width;
        paramData.i_height              = height;
        paramData.vui.b_fullrange       = colorDesc.fullRange;
        paramData.vui.i_colorprim       = colorDesc.primaries;
        paramData.vui.i_transfer        = colorDesc.transfer;
        paramData.vui.i_colmatrix       = colorDesc.matrix;

        if (keyframeInterval)
            paramData.i_keyint_max      = fps*keyframeInterval;

        paramData.i_fps_num             = fps;
        paramData.i_fps_den             = 1;

        paramData.i_timebase_num        = 1;
        paramData.i_timebase_den        = 1000;

        paramData.pf_log                = get_x264_log;
        paramData.i_log_level           = X264_LOG_WARNING;

        if (scmpi(curProfile, L"main") == 0)
            paramData.i_level_idc = 41; // to ensure compatibility with portable devices

        for (UINT i = 0; i<paramList.Num(); i++)
        {
            String &strParam = paramList[i];
            if(!schr(strParam, '='))
                continue;

            String strParamName = strParam.GetToken(0, '=');
            String strParamVal  = strParam.GetTokenOffset(1, '=');

            if( strParamName.CompareI(TEXT("fps")) || 
                strParamName.CompareI(TEXT("force-cfr")))
            {
                Log(TEXT("The custom x264 command '%s' is unsupported, use the application settings instead"), strParam.Array());
                continue;
            }
            else
            {
                LPSTR lpParam = strParamName.CreateUTF8String();
                LPSTR lpVal   = strParamVal.CreateUTF8String();

                if(x264_param_parse(&paramData, lpParam, lpVal) != 0)
                    Log(TEXT("The custom x264 command '%s' failed"), strParam.Array());

                Free(lpParam);
                Free(lpVal);
            }
        }

        if(bUse444) paramData.i_csp = X264_CSP_I444;
        else paramData.i_csp = X264_CSP_I420;

        colorDesc.fullRange = paramData.vui.b_fullrange;
        colorDesc.primaries = paramData.vui.i_colorprim;
        colorDesc.transfer  = paramData.vui.i_transfer;
        colorDesc.matrix    = paramData.vui.i_colmatrix;

        if (curProfile)
        {
            LPSTR lpProfile = curProfile.CreateUTF8String();

            if (x264_param_apply_profile (&paramData, lpProfile))
                Log(TEXT("Failed to set x264 profile: %s"), curProfile.Array());

            Free(lpProfile);
        }

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

    bool Encode(LPVOID picInPtr, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD outputTimestamp, DWORD &out_pts)
    {
        x264_picture_t *picIn = (x264_picture_t*)picInPtr;

        x264_nal_t *nalOut;
        int nalNum;

        packets.Clear();
        ClearPackets();

        if(bRequestKeyframe && picIn)
            picIn->i_type = X264_TYPE_IDR;

        if(x264_encoder_encode(x264, &nalOut, &nalNum, picIn, &picOut) < 0)
        {
            AppWarning(TEXT("x264 encode failed"));
            return false;
        }

        if(bRequestKeyframe && picIn)
        {
            picIn->i_type = X264_TYPE_AUTO;
            bRequestKeyframe = false;
        }

        if(!bFirstFrameProcessed && nalNum)
        {
            delayOffset = -picOut.i_dts;
            bFirstFrameProcessed = true;
        }

        INT64 ts = INT64(outputTimestamp);
        int timeOffset;

        //if frame duplication is being used, the shift will be insignificant, so just don't bother adjusting audio
        timeOffset = int(picOut.i_pts-picOut.i_dts);
        timeOffset += frameShift;

        out_pts = (DWORD)picOut.i_pts;

        if(nalNum && timeOffset < 0)
        {
            frameShift -= timeOffset;
            timeOffset = 0;
        }

        //OSDebugOut(TEXT("inpts: %005lld, dts: %005lld, pts: %005lld, timestamp: %005d, offset: %005d, newoffset: %005lld\n"), picIn->i_pts, picOut.i_dts, picOut.i_pts, outputTimestamp, timeOffset, picOut.i_pts-picOut.i_dts);

        timeOffset = htonl(timeOffset);

        BYTE *timeOffsetAddr = ((BYTE*)&timeOffset)+1;

        VideoPacket *newPacket = NULL;

        PacketType bestType = PacketType_VideoDisposable;
        bool bFoundFrame = false;

        for(int i=0; i<nalNum; i++)
        {
            x264_nal_t &nal = nalOut[i];

            if(nal.i_type == NAL_SEI)
            {
                BYTE *skip = nal.p_payload;
                while(*(skip++) != 0x1);
                int skipBytes = (int)(skip-nal.p_payload);

                int newPayloadSize = (nal.i_payload-skipBytes);

                if (nal.p_payload[skipBytes+1] == 0x5) {
                    SEIData.Clear();
                    BufferOutputSerializer packetOut(SEIData);

                    packetOut.OutputDword(htonl(newPayloadSize));
                    packetOut.Serialize(nal.p_payload+skipBytes, newPayloadSize);
                } else {
                    if (!newPacket)
                        newPacket = CurrentPackets.CreateNew();

                    BufferOutputSerializer packetOut(newPacket->Packet);

                    packetOut.OutputDword(htonl(newPayloadSize));
                    packetOut.Serialize(nal.p_payload+skipBytes, newPayloadSize);
                }
            }
            else if(nal.i_type == NAL_FILLER)
            {
                BYTE *skip = nal.p_payload;
                while(*(skip++) != 0x1);
                int skipBytes = (int)(skip-nal.p_payload);

                int newPayloadSize = (nal.i_payload-skipBytes);

                if (!newPacket)
                    newPacket = CurrentPackets.CreateNew();

                BufferOutputSerializer packetOut(newPacket->Packet);

                packetOut.OutputDword(htonl(newPayloadSize));
                packetOut.Serialize(nal.p_payload+skipBytes, newPayloadSize);
            }
            else if(nal.i_type == NAL_SLICE_IDR || nal.i_type == NAL_SLICE)
            {
                BYTE *skip = nal.p_payload;
                while(*(skip++) != 0x1);
                int skipBytes = (int)(skip-nal.p_payload);

                if (!newPacket)
                    newPacket = CurrentPackets.CreateNew();

                if (!bFoundFrame)
                {
                    newPacket->Packet.Insert(0, (nal.i_type == NAL_SLICE_IDR) ? 0x17 : 0x27);
                    newPacket->Packet.Insert(1, 1);
                    newPacket->Packet.InsertArray(2, timeOffsetAddr, 3);

                    bFoundFrame = true;
                }

                int newPayloadSize = (nal.i_payload-skipBytes);
                BufferOutputSerializer packetOut(newPacket->Packet);

                packetOut.OutputDword(htonl(newPayloadSize));
                packetOut.Serialize(nal.p_payload+skipBytes, newPayloadSize);

                switch(nal.i_ref_idc)
                {
                    case NAL_PRIORITY_DISPOSABLE:   bestType = MAX(bestType, PacketType_VideoDisposable);  break;
                    case NAL_PRIORITY_LOW:          bestType = MAX(bestType, PacketType_VideoLow);         break;
                    case NAL_PRIORITY_HIGH:         bestType = MAX(bestType, PacketType_VideoHigh);        break;
                    case NAL_PRIORITY_HIGHEST:      bestType = MAX(bestType, PacketType_VideoHighest);     break;
                }
            }
            /*else if(nal.i_type == NAL_SPS)
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
            }*/
            else
                continue;
        }

        packetTypes << bestType;

	if (CurrentPackets.Num()) {
		packets.SetSize(1);
		packets[0].lpPacket = CurrentPackets[0].Packet.Array();
		packets[0].size     = CurrentPackets[0].Packet.Num();
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

    virtual void GetSEI(DataPacket &packet)
    {
        packet.lpPacket = SEIData.Array();
        packet.size     = SEIData.Num();
    }

    int GetBitRate() const
    {
        if (paramData.rc.i_vbv_max_bitrate)
            return paramData.rc.i_vbv_max_bitrate;
        else
            return paramData.rc.i_bitrate;
    }

    String GetInfoString() const
    {
        String strInfo;

        strInfo << TEXT("Video Encoding: x264")  <<
                   TEXT("\r\n    fps: ")         << IntString(paramData.i_fps_num) <<
                   TEXT("\r\n    width: ")       << IntString(width) << TEXT(", height: ") << IntString(height) <<
                   TEXT("\r\n    preset: ")      << curPreset <<
                   TEXT("\r\n    profile: ")     << curProfile <<
                   TEXT("\r\n    keyint: ")      << paramData.i_keyint_max <<
                   TEXT("\r\n    CBR: ")         << CTSTR((bUseCBR) ? TEXT("yes") : TEXT("no")) <<
                   TEXT("\r\n    CFR: ")         << CTSTR((bUseCFR) ? TEXT("yes") : TEXT("no")) <<
                   TEXT("\r\n    max bitrate: ") << IntString(paramData.rc.i_vbv_max_bitrate) <<
                   TEXT("\r\n    buffer size: ") << IntString(paramData.rc.i_vbv_buffer_size);

        if(!bUseCBR)
        {
            strInfo << TEXT("\r\n    quality: ")     << IntString(10-int(paramData.rc.f_rf_constant-baseCRF));
        }

        return strInfo;
    }

    virtual bool DynamicBitrateSupported() const
    {
        return (paramData.i_nal_hrd != X264_NAL_HRD_CBR);
    }

    virtual bool SetBitRate(DWORD maxBitrate, DWORD bufferSize)
    {
        DWORD old_bitrate = paramData.rc.i_vbv_max_bitrate;
        DWORD old_buffer  = paramData.rc.i_vbv_buffer_size;

        SetBitRateParams(maxBitrate, bufferSize);

        int retVal = x264_encoder_reconfig(x264, &paramData);
        if (retVal < 0)
            Log(TEXT("Could not set new encoder bitrate, error value %u"), retVal);
        else
        {
            String changes;
            if (old_bitrate != maxBitrate)
                changes << FormattedString(L"bitrate %d->%d", old_bitrate, maxBitrate);
            if (old_buffer != bufferSize)
                changes << FormattedString(L"%sbuffer size %d->%d", changes.Length() ? L", " : L"", old_buffer, bufferSize);
            if (changes)
                Log(L"x264: %s", changes.Array());
        }

        return retVal == 0;
    }

    virtual void RequestKeyframe()
    {
        bRequestKeyframe = true;
    }

    virtual int GetBufferedFrames()
    {
        return x264_encoder_delayed_frames(x264);
    }

    virtual bool HasBufferedFrames()
    {
        return x264_encoder_delayed_frames(x264) > 0;
    }
};


VideoEncoder* CreateX264Encoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR)
{
    return new X264Encoder(fps, width, height, quality, preset, bUse444, colorDesc, maxBitRate, bufferSize, bUseCFR);
}

