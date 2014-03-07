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
#include "RTMPStuff.h"

void InitSockets()
{
    WSADATA wsad;
    WSAStartup(MAKEWORD( 2, 2 ), &wsad);
}

void TerminateSockets()
{
    WSACleanup();
}

int SendResultNumber(RTMP *r, double txn, double ID)
{
    RTMPPacket packet;
    char pbuf[256], *pend = pbuf+sizeof(pbuf);

    packet.m_nChannel = 0x03;     // control channel (invoke)
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    char *enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av__result);
    enc = AMF_EncodeNumber(enc, pend, txn);
    *enc++ = AMF_NULL;
    enc = AMF_EncodeNumber(enc, pend, ID);

    packet.m_nBodySize = enc - packet.m_body;

    return RTMP_SendPacket(r, &packet, FALSE);
}

int SendConnectResult(RTMP *r, double txn)
{
    RTMPPacket packet;
    char pbuf[384], *pend = pbuf+sizeof(pbuf);
    AMFObject obj;
    AMFObjectProperty p, op;
    AVal av;

    packet.m_nChannel = 0x03;     // control channel (invoke)
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    char *enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av__result);
    enc = AMF_EncodeNumber(enc, pend, txn);
    *enc++ = AMF_OBJECT;

    STR2AVAL(av, "FMS/3,5,1,525");
    enc = AMF_EncodeNamedString(enc, pend, &av_fmsVer, &av);
    enc = AMF_EncodeNamedNumber(enc, pend, &av_capabilities, 31.0);
    enc = AMF_EncodeNamedNumber(enc, pend, &av_mode, 1.0);
    *enc++ = 0;
    *enc++ = 0;
    *enc++ = AMF_OBJECT_END;

    *enc++ = AMF_OBJECT;

    STR2AVAL(av, "status");
    enc = AMF_EncodeNamedString(enc, pend, &av_level, &av);
    STR2AVAL(av, "NetConnection.Connect.Success");
    enc = AMF_EncodeNamedString(enc, pend, &av_code, &av);
    STR2AVAL(av, "Connection succeeded.");
    enc = AMF_EncodeNamedString(enc, pend, &av_description, &av);
    enc = AMF_EncodeNamedNumber(enc, pend, &av_objectEncoding, r->m_fEncoding);

    STR2AVAL(p.p_name, "version");
    STR2AVAL(p.p_vu.p_aval, "3,5,1,525");
    p.p_type = AMF_STRING;
    obj.o_num = 1;
    obj.o_props = &p;
    op.p_type = AMF_OBJECT;
    STR2AVAL(op.p_name, "data");
    op.p_vu.p_object = obj;
    enc = AMFProp_Encode(&op, enc, pend);
    *enc++ = 0;
    *enc++ = 0;
    *enc++ = AMF_OBJECT_END;

    packet.m_nBodySize = enc - packet.m_body;

    return RTMP_SendPacket(r, &packet, FALSE);
}

void AVreplace(AVal *src, const AVal *orig, const AVal *repl)
{
    char *srcbeg = src->av_val;
    char *srcend = src->av_val + src->av_len;
    char *dest, *sptr, *dptr;
    int n = 0;

    /* count occurrences of orig in src */
    sptr = src->av_val;
    while (sptr < srcend && (sptr = strstr(sptr, orig->av_val)))
    {
        n++;
        sptr += orig->av_len;
    }
    if (!n)
        return;

    dest = (char*)malloc(src->av_len + 1 + (repl->av_len - orig->av_len) * n);

    sptr = src->av_val;
    dptr = dest;
    while (sptr < srcend && (sptr = strstr(sptr, orig->av_val)))
    {
        n = sptr - srcbeg;
        memcpy(dptr, srcbeg, n);
        dptr += n;
        memcpy(dptr, repl->av_val, repl->av_len);
        dptr += repl->av_len;
        sptr += orig->av_len;
        srcbeg = sptr;
    }
    n = srcend - srcbeg;
    memcpy(dptr, srcbeg, n);
    dptr += n;
    *dptr = '\0';
    src->av_val = dest;
    src->av_len = dptr - dest;
}

int SendPlayStart(RTMP *r)
{
    RTMPPacket packet;
    char pbuf[512], *pend = pbuf+sizeof(pbuf);

    packet.m_nChannel = 0x03;     // control channel (invoke)
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    char *enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_onStatus);
    enc = AMF_EncodeNumber(enc, pend, 0);
    *enc++ = AMF_NULL;
    *enc++ = AMF_OBJECT;

    enc = AMF_EncodeNamedString(enc, pend, &av_level, &av_status);
    enc = AMF_EncodeNamedString(enc, pend, &av_code, &av_NetStream_Play_Start);
    enc = AMF_EncodeNamedString(enc, pend, &av_description, &av_Started_playing);
    enc = AMF_EncodeNamedString(enc, pend, &av_details, &r->Link.playpath);
    enc = AMF_EncodeNamedString(enc, pend, &av_clientid, &av_clientid);
    *enc++ = 0;
    *enc++ = 0;
    *enc++ = AMF_OBJECT_END;

    packet.m_nBodySize = enc - packet.m_body;
    return RTMP_SendPacket(r, &packet, FALSE);
}

int SendPlayStop(RTMP *r)
{
    RTMPPacket packet;
    char pbuf[512], *pend = pbuf+sizeof(pbuf);

    packet.m_nChannel = 0x03;     // control channel (invoke)
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    char *enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_onStatus);
    enc = AMF_EncodeNumber(enc, pend, 0);
    *enc++ = AMF_NULL;
    *enc++ = AMF_OBJECT;

    enc = AMF_EncodeNamedString(enc, pend, &av_level, &av_status);
    enc = AMF_EncodeNamedString(enc, pend, &av_code, &av_NetStream_Play_Stop);
    enc = AMF_EncodeNamedString(enc, pend, &av_description, &av_Stopped_playing);
    enc = AMF_EncodeNamedString(enc, pend, &av_details, &r->Link.playpath);
    enc = AMF_EncodeNamedString(enc, pend, &av_clientid, &av_clientid);
    *enc++ = 0;
    *enc++ = 0;
    *enc++ = AMF_OBJECT_END;

    packet.m_nBodySize = enc - packet.m_body;
    return RTMP_SendPacket(r, &packet, FALSE);
}

char* OBS::EncMetaData(char *enc, char *pend, bool bFLVFile)
{
    int    maxBitRate    = GetVideoEncoder()->GetBitRate();
    int    fps           = GetFPS();
    int    audioBitRate  = GetAudioEncoder()->GetBitRate();
    CTSTR  lpAudioCodec  = GetAudioEncoder()->GetCodec();

    //double audioCodecID;
    const AVal *av_codecFourCC;

#ifdef USE_AAC
    if(scmpi(lpAudioCodec, TEXT("AAC")) == 0)
    {
        av_codecFourCC = &av_mp4a;
        //audioCodecID = 10.0;
    }
    else
#endif
    {
        av_codecFourCC = &av_mp3;
        //audioCodecID = 2.0;
    }

    if(bFLVFile)
    {
        *enc++ = AMF_ECMA_ARRAY;
        enc = AMF_EncodeInt32(enc, pend, 14);
    }
    else
        *enc++ = AMF_OBJECT;

    enc = AMF_EncodeNamedNumber(enc, pend, &av_duration,        0.0);
    enc = AMF_EncodeNamedNumber(enc, pend, &av_fileSize,        0.0);
    enc = AMF_EncodeNamedNumber(enc, pend, &av_width,           double(outputCX));
    enc = AMF_EncodeNamedNumber(enc, pend, &av_height,          double(outputCY));

    /*if(bFLVFile)
        enc = AMF_EncodeNamedNumber(enc, pend, &av_videocodecid,    7.0);//&av_avc1);//
    else*/
        enc = AMF_EncodeNamedString(enc, pend, &av_videocodecid,    &av_avc1);//7.0);//

    enc = AMF_EncodeNamedNumber(enc, pend, &av_videodatarate,   double(maxBitRate));
    enc = AMF_EncodeNamedNumber(enc, pend, &av_framerate,       double(fps));

    /*if(bFLVFile)
        enc = AMF_EncodeNamedNumber(enc, pend, &av_audiocodecid,    audioCodecID);//av_codecFourCC);//
    else*/
        enc = AMF_EncodeNamedString(enc, pend, &av_audiocodecid,    av_codecFourCC);//audioCodecID);//

    enc = AMF_EncodeNamedNumber(enc, pend, &av_audiodatarate,   double(audioBitRate)); //ex. 128kb\s
    enc = AMF_EncodeNamedNumber(enc, pend, &av_audiosamplerate, double(App->GetSampleRateHz()));
    enc = AMF_EncodeNamedNumber(enc, pend, &av_audiosamplesize, 16.0);
    enc = AMF_EncodeNamedNumber(enc, pend, &av_audiochannels,   double(App->NumAudioChannels()));
    //enc = AMF_EncodeNamedBoolean(enc, pend, &av_stereo,         true);

    if (App->NumAudioChannels() > 2 || App->NumAudioChannels() <1)
        CrashError(TEXT("bad audio channnel configuration"));
    enc = AMF_EncodeNamedBoolean(enc, pend, &av_stereo,     App->NumAudioChannels()==2);

    enc = AMF_EncodeNamedString(enc, pend, &av_encoder,         &av_OBSVersion);
    *enc++ = 0;
    *enc++ = 0;
    *enc++ = AMF_OBJECT_END;

    return enc;
}
