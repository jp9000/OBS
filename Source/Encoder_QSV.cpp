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

#include "mfxstructures.h"
#include "mfxmvc.h"
#include "mfxvideo.h"
#include "mfxvideo++.h"
#include "mfxplugin++.h"

#include <algorithm>
#include <memory>

extern "C"
{
#include "../x264/x264.h"
}

namespace
{
#define TO_STR(a) TEXT(#a)

    const float baseCRF = 22.0f;
    const mfxVersion version = {4, 1}; //Highest supported version on Sandy Bridge
    const mfxIMPL validImpl[] = {MFX_IMPL_HARDWARE_ANY, MFX_IMPL_HARDWARE};
    const TCHAR* implStr[] = {
        TO_STR(MFX_IMPL_AUTO),
        TO_STR(MFX_IMPL_SOFTWARE),
        TO_STR(MFX_IMPL_HARDWARE),
        TO_STR(MFX_IMPL_AUTO_ANY),
        TO_STR(MFX_IMPL_HARDWARE_ANY)
    };
    const TCHAR* usageStr[] = {
        TO_STR(MFX_TARGETUSAGE_UNKNOWN),
        TO_STR(MFX_TARGETUSAGE_BEST_QUALITY),
        TO_STR(2),
        TO_STR(3),
        TO_STR(MFX_TARGETUSAGE_BALANCED),
        TO_STR(5),
        TO_STR(6),
        TO_STR(MFX_TARGETUSAGE_BEST_SPEED)
    };

    void ConvertFrameRate(mfxF64 dFrameRate, mfxU32& pnFrameRateExtN, mfxU32& pnFrameRateExtD)
    {
        mfxU32 fr;

        fr = (mfxU32)(dFrameRate + .5);

        if (fabs(fr - dFrameRate) < 0.0001)
        {
            pnFrameRateExtN = fr;
            pnFrameRateExtD = 1;
            return;
        }

        fr = (mfxU32)(dFrameRate * 1.001 + .5);

        if (fabs(fr * 1000 - dFrameRate * 1001) < 10)
        {
            pnFrameRateExtN = fr * 1000;
            pnFrameRateExtD = 1001;
            return;
        }

        pnFrameRateExtN = (mfxU32)(dFrameRate * 10000 + .5);
        pnFrameRateExtD = 10000;
    }

    bool CheckQSVHardwareSupport()
    {
        MFXVideoSession test;
        for(int i = 0; i < sizeof(validImpl)/sizeof(validImpl[0]); i++)
        {
            mfxIMPL impl = validImpl[i];
            mfxVersion ver = version;
            auto result = test.Init(impl, &ver);
            if(result != MFX_ERR_NONE)
                continue;
            Log(TEXT("Found QSV hardware support"));
            return true;
        }
        Log(TEXT("Failed to initialize QSV hardware session"));
        return false;
    }
}

struct VideoPacket
{
    List<BYTE> Packet;
    inline void FreeData() {Packet.Clear();}
};


class QSVEncoder : public VideoEncoder
{
    MFXVideoSession session;

    mfxVideoParam params;

    std::unique_ptr<MFXVideoENCODE> enc;

    mfxEncodeCtrl ctrl;
    
    List<mfxU8> bs_buff;
    struct encode_task
    {
        mfxFrameSurface1 surf;
        mfxBitstream bs;
        mfxSyncPoint sp;
        bool keyframe;
        mfxFrameData* frame;
    };
    List<encode_task> encode_tasks;

    unsigned oldest, insert, encode, in_use;

    List<mfxU8> frame_buff;
    List<mfxFrameData> frames;

    int fps;

    bool bRequestKeyframe;

    UINT width, height;

    bool bFirstFrameProcessed;

    bool bUseCBR, bUseCFR, bDupeFrames;
    unsigned deferredFrames;

    CircularList<mfxU64> dts_vals;

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

#define ALIGN16(value)                      (((value + 15) >> 4) << 4) // round up to a multiple of 16
public:
    QSVEncoder(int fps_, int width, int height, int quality, CTSTR preset, bool bUse444, int maxBitrate, int bufferSize, bool bUseCFR_, bool bDupeFrames_)
        : enc(nullptr)
    {
        Log(TEXT("------------------------------------------"));
        for(int i = 0; i < sizeof(validImpl)/sizeof(validImpl[0]); i++)
        {
            mfxIMPL impl = validImpl[i];
            mfxVersion ver = version;
            auto result = session.Init(impl, &ver);
            if(result == MFX_ERR_NONE)
            {
                Log(TEXT("QSV version %u.%u using %s"), ver.Major, ver.Minor, implStr[impl]);
                break;
            }
        }

        session.SetPriority(MFX_PRIORITY_HIGH);

        fps = fps_;

        bUseCBR = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseCBR")) != 0;
        bUseCFR = bUseCFR_;
        bDupeFrames = bDupeFrames_;

        memset(&params, 0, sizeof(params));
        //params.AsyncDepth = 0;
        params.mfx.CodecId = MFX_CODEC_AVC;
        params.mfx.TargetUsage = MFX_TARGETUSAGE_BEST_QUALITY;//SPEED;
        params.mfx.TargetKbps = (mfxU16)(maxBitrate*0.9);
        params.mfx.MaxKbps = maxBitrate;
        //params.mfx.InitialDelayInKB = 1;
        //params.mfx.GopRefDist = 1;
        //params.mfx.NumRefFrame = 0;
        params.mfx.GopPicSize = 61;
        params.mfx.GopRefDist = 3;
        params.mfx.GopOptFlag = 2;
        params.mfx.IdrInterval = 2;
        params.mfx.NumSlice = 1;

        params.mfx.RateControlMethod = bUseCBR ? MFX_RATECONTROL_CBR : MFX_RATECONTROL_VBR;
        params.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

        auto& fi = params.mfx.FrameInfo;
        ConvertFrameRate(fps, fi.FrameRateExtN, fi.FrameRateExtD);

        fi.FourCC = MFX_FOURCC_NV12;
        fi.ChromaFormat = bUse444 ? MFX_CHROMAFORMAT_YUV444 : MFX_CHROMAFORMAT_YUV420;
        fi.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

        fi.Width = ALIGN16(width);
        fi.Height = ALIGN16(height);

        fi.CropX = 0;
        fi.CropY = 0;
        fi.CropW = width;
        fi.CropH = height;

        this->width  = width;
        this->height = height;

        enc.reset(new MFXVideoENCODE(session));
        enc->Close();

        mfxFrameAllocRequest req;
        memset(&req, 0, sizeof(req));
        enc->QueryIOSurf(&params, &req);

        enc->Init(&params);

        decltype(params) query;
        memcpy(&query, &params, sizeof(params));
        enc->GetVideoParam(&query);

        unsigned num_surf = max(6, req.NumFrameSuggested+1);

        encode_tasks.SetSize(num_surf);

        const unsigned bs_size = max(query.mfx.BufferSizeInKB*1000, bufferSize*1024/8);
        bs_buff.SetSize(bs_size * encode_tasks.Num() + 31);
        params.mfx.BufferSizeInKB = bs_size/1000;

        mfxU8* bs_start = (mfxU8*)(((size_t)bs_buff.Array() + 31)/32*32);
        for(unsigned i = 0; i < encode_tasks.Num(); i++)
        {
            encode_tasks[i].sp = nullptr;

            mfxFrameSurface1& surf = encode_tasks[i].surf;
            memset(&surf, 0, sizeof(mfxFrameSurface1));
            memcpy(&surf.Info, &params.mfx.FrameInfo, sizeof(params.mfx.FrameInfo));
            
            mfxBitstream& bs = encode_tasks[i].bs;
            memset(&bs, 0, sizeof(mfxBitstream));
            bs.Data = bs_start + i*bs_size;
            bs.MaxLength = bs_size;
        }

        frames.SetSize(num_surf+3); //+NUM_OUT_BUFFERS

        const unsigned frame_size = width*height*2;
        frame_buff.SetSize(frame_size * frames.Num() + 15);

        mfxU8* frame_start = (mfxU8*)(((size_t)frame_buff.Array() + 15)/16*16);
        for(unsigned i = 0; i < frames.Num(); i++)
        {
            mfxFrameData& frame = frames[i];
            memset(&frame, 0, sizeof(mfxFrameData));
            frame.Y = frame_start + i*frame_size;
            frame.UV = frame_start + i*frame_size + width*height;
        }

        oldest = 0;
        insert = 0;
        encode = 0;
        in_use = 0;

        Log(TEXT("Using %u encode tasks"), encode_tasks.Num());
        Log(TEXT("Buffer size: %u configured, %u suggested by QSV; using %u"),
            bufferSize, query.mfx.BufferSizeInKB*1000*8/1024, params.mfx.BufferSizeInKB*1000*8/1024);

        Log(TEXT("------------------------------------------"));
        Log(TEXT("%s"), GetInfoString().Array());
        Log(TEXT("------------------------------------------"));

        memset(&ctrl, 0, sizeof(ctrl));
        ctrl.FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR;

        deferredFrames = 0;

        DataPacket packet;
        GetHeaders(packet);
    }

    ~QSVEncoder()
    {
        ClearPackets();
    }

    virtual void RequestBuffers(LPVOID buffers)
    {
        if(!buffers)
            return;
        for(unsigned i = 0; i < frames.Num(); i++)
        {
            if(frames[i].Locked || frames[i].FrameOrder)
                continue;
            mfxFrameData& data = frames[i];
            mfxFrameData& buff = *(mfxFrameData*)buffers;
            buff.Y = data.Y;
            buff.UV = data.UV;
            buff.FrameOrder = i+1;
            data.FrameOrder = i+1;
            return;
        }
        Log(TEXT("Error: all frames are in use"));
    }

    void ProcessEncodedFrame(List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD outputTimestamp, int &ctsOffset, mfxU32 wait=0)
    {
        if(!in_use)
            return;
        {
            encode_task& task = encode_tasks[oldest];
            auto& sp = task.sp;
            mfxStatus sts;
            if((sts = MFXVideoCORE_SyncOperation(session, sp, wait)) != MFX_ERR_NONE)
                return;
            sp = nullptr;
            in_use -= 1;

            oldest = (oldest+1)%encode_tasks.Num();

            mfxBitstream& bs = task.bs;

            List<x264_nal_t> nalOut;
            mfxU8 *start = bs.Data + bs.DataOffset,
                  *end = bs.Data + bs.DataOffset + bs.DataLength;
            static mfxU8 start_seq[] = {0, 0, 1};
            start = std::search(start, end, start_seq, start_seq+3);
            while(start != end)
            {
                decltype(start) next = std::search(start+1, end, start_seq, start_seq+3);
                x264_nal_t nal;
                nal.i_ref_idc = start[3]>>5;
                nal.i_type = start[3]&0x1f;
                if(nal.i_type == NAL_SLICE_IDR)
                    nal.i_ref_idc = NAL_PRIORITY_HIGHEST;
                nal.p_payload = start;
                nal.i_payload = int(next-start);
                nalOut << nal;
                start = next;
            }
            size_t nalNum = nalOut.Num();

            packets.Clear();
            ClearPackets();


            if(!bFirstFrameProcessed && nalNum)
            {
                delayOffset = 0;//bs.TimeStamp/90;
                bFirstFrameProcessed = true;
            }

            INT64 ts = INT64(dts_vals[0]);
            int timeOffset = int(bs.TimeStamp/90+delayOffset-ts);//int((picOut.i_pts+delayOffset)-ts);

            dts_vals.Remove(0);

            if(bDupeFrames)
            {
                //if frame duplication is being used, the shift will be insignificant, so just don't bother adjusting audio
                timeOffset += frameShift;

                if(nalNum && timeOffset < 0)
                {
                    frameShift -= timeOffset;
                    timeOffset = 0;
                }
            }
            else
            {
                timeOffset += ctsOffset;

                //dynamically adjust the CTS for the stream if it gets lower than the current value
                //(thanks to cyrus for suggesting to do this instead of a single shift)
                if(nalNum && timeOffset < 0)
                {
                    ctsOffset -= timeOffset;
                    timeOffset = 0;
                }
            }

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
                /*else if(nal.i_type == NAL_FILLER) //QSV does not produce NAL_FILLER
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
                }*/
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

            packets.SetSize(CurrentPackets.Num());
            for(UINT i=0; i<packets.Num(); i++)
            {
                packets[i].lpPacket = CurrentPackets[i].Packet.Array();
                packets[i].size     = CurrentPackets[i].Packet.Num();
            }
            task.frame->FrameOrder = 0;
            task.frame->Locked = false;
        }
    }

    bool Encode(LPVOID picInPtr, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD outputTimestamp, int &ctsOffset)
    {
        profileIn("ProcessEncodedFrame");
        ProcessEncodedFrame(packets, packetTypes, outputTimestamp, ctsOffset);
        if(in_use == encode_tasks.Num())
        {
            Log(TEXT("Error: all encode tasks in use, stalling pipeline"));
            ProcessEncodedFrame(packets, packetTypes, outputTimestamp, ctsOffset, INFINITE);
        }
        profileOut;
        encode_task& task = encode_tasks[insert];
        insert = (insert+1)%encode_tasks.Num();
        in_use += 1;
        task.keyframe = bRequestKeyframe;
        bRequestKeyframe = false;
        mfxBitstream& bs = task.bs;
        mfxFrameSurface1& surf = task.surf;
        mfxFrameSurface1& pic = *(mfxFrameSurface1*)picInPtr;
        profileIn("setup new frame");
        mfxFrameData& frame = frames[pic.Data.FrameOrder-1];
        frame.Locked = true;
        task.frame = &frame;
        bs.DataLength = 0;
        bs.DataOffset = 0;
        surf.Data.Y = frame.Y;
        surf.Data.UV = frame.UV;
        surf.Data.Pitch = pic.Data.Pitch;
        surf.Data.TimeStamp = pic.Data.TimeStamp*90;

        dts_vals << pic.Data.TimeStamp;

        pic.Data.Y = nullptr;
        pic.Data.UV = nullptr;
        pic.Data.FrameOrder = 0;
        profileOut;
        mfxSyncPoint& sp = task.sp;
        mfxStatus sts;
        profileIn("EncodeFrameAsync");
        unsigned limit = encode < insert ? insert : insert + encode_tasks.Num();
        for(unsigned i = encode;i < limit; i++)
        {
            encode_task& task = encode_tasks[encode];
            mfxBitstream& bs = task.bs;
            mfxFrameSurface1& surf = task.surf;
            mfxSyncPoint& sp = task.sp;
            for(;;)
            {
                sts = enc->EncodeFrameAsync(task.keyframe ? &ctrl : nullptr, &surf, &bs, &sp);

                if(sts == MFX_ERR_NONE || sp)
                    break;
                if(sts == MFX_WRN_DEVICE_BUSY)
                {
                    deferredFrames += 1;
                    return false;
                }
                //if(!sp); //sts == MFX_ERR_MORE_DATA usually; retry the call (see MSDK examples)
                //Log(TEXT("returned status %i, %u"), sts, insert);
            }
            encode = (encode+1)%encode_tasks.Num();
        }
        profileOut;

        return true;
    }

    void GetHeaders(DataPacket &packet)
    {
        if(!HeaderPacket.Num())
        {
            mfxVideoParam header_query;
            memset(&header_query, 0, sizeof(header_query));
            mfxU8 sps[100], pps[100];
            mfxExtCodingOptionSPSPPS headers;
            memset(&headers, 0, sizeof(headers));
            headers.Header.BufferId = MFX_EXTBUFF_CODING_OPTION_SPSPPS;
            headers.Header.BufferSz = sizeof(mfxExtCodingOptionSPSPPS);
            headers.SPSBuffer = sps;
            headers.SPSBufSize = 100;
            headers.PPSBuffer = pps;
            headers.PPSBufSize = 100;
            mfxExtBuffer *ext_buff[] = {(mfxExtBuffer*)&headers};
            header_query.ExtParam = ext_buff;
            header_query.NumExtParam = 1;

            auto sts = enc->GetVideoParam(&header_query);

            BufferOutputSerializer headerOut(HeaderPacket);

            headerOut.OutputByte(0x17);
            headerOut.OutputByte(0);
            headerOut.OutputByte(0);
            headerOut.OutputByte(0);
            headerOut.OutputByte(0);
            headerOut.OutputByte(1);
            headerOut.Serialize(sps+5, 3);
            headerOut.OutputByte(0xff);
            headerOut.OutputByte(0xe1);
            headerOut.OutputWord(htons(headers.SPSBufSize-4));
            headerOut.Serialize(sps+4, headers.SPSBufSize-4);


            headerOut.OutputByte(1);
            headerOut.OutputWord(htons(headers.PPSBufSize-4));
            headerOut.Serialize(pps+4, headers.PPSBufSize-4);
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
        if(params.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
            return params.mfx.MaxKbps;
        return params.mfx.TargetKbps;
    }

    String GetInfoString() const
    {
        String strInfo;

        strInfo << TEXT("Video Encoding: QSV")    <<
                   TEXT("\r\n    fps: ")          << IntString(fps) <<
                   TEXT("\r\n    width: ")        << IntString(width) << TEXT(", height: ") << IntString(height) <<
                   TEXT("\r\n    target-usage: ") << usageStr[params.mfx.TargetUsage] <<
                   TEXT("\r\n    CBR: ")          << CTSTR((bUseCBR) ? TEXT("yes") : TEXT("no")) <<
                   TEXT("\r\n    CFR: ")          << CTSTR((bUseCFR) ? TEXT("yes") : TEXT("no")) <<
                   TEXT("\r\n    max bitrate: ")  << IntString(params.mfx.MaxKbps);

        if(!bUseCBR)
        {
            strInfo << TEXT("\r\n    buffer size: ") << IntString(params.mfx.BufferSizeInKB*1000*8/1024);
        }

        return strInfo;
    }

    virtual bool DynamicBitrateSupported() const
    {
        return false;
    }

    virtual bool SetBitRate(DWORD maxBitrate, DWORD bufferSize)
    {
        return false;
    }

    virtual void RequestKeyframe()
    {
        bRequestKeyframe = true;
    }

    virtual bool isQSV() { return true; }
};

VideoEncoder* CreateQSVEncoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, int maxBitRate, int bufferSize, bool bUseCFR, bool bDupeFrames)
{
    if(CheckQSVHardwareSupport())
        return new QSVEncoder(fps, width, height, quality, preset, bUse444, maxBitRate, bufferSize, bUseCFR, bDupeFrames);
    return nullptr;
}
