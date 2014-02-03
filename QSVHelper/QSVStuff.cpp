/********************************************************************************
 Copyright (C) 2013 Ruwen Hahn <palana@stunned.de>
 
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

#include "QSVStuff.h"

#include "Utilities.h"

using namespace std;

namespace {
    void ConvertFrameRate(mfxF64 dFrameRate, mfxU32& pnFrameRateExtN, mfxU32& pnFrameRateExtD)
    {
        mfxU32 fr;

        fr = (mfxU32)(dFrameRate + .5);

        if(fabs(fr - dFrameRate) < 0.0001)
        {
            pnFrameRateExtN = fr;
            pnFrameRateExtD = 1;
            return;
        }

        fr = (mfxU32)(dFrameRate * 1.001 + .5);

        if(fabs(fr * 1000 - dFrameRate * 1001) < 10)
        {
            pnFrameRateExtN = fr * 1000;
            pnFrameRateExtD = 1001;
            return;
        }

        pnFrameRateExtN = (mfxU32)(dFrameRate * 10000 + .5);
        pnFrameRateExtD = 10000;
    }
}

Parameters::Parameters()
{
    zero(params);
}

void Parameters::Init(int fps, int keyframe_interval_frames, int bframes, int width, int height, int max_bitrate, int buffer_size, bool use_cbr, bool main_profile)
{
    params.mfx.CodecId = MFX_CODEC_AVC;
    params.mfx.TargetUsage = MFX_TARGETUSAGE_BEST_QUALITY;
    params.mfx.TargetKbps = max_bitrate;
    params.mfx.MaxKbps = max_bitrate;
    params.mfx.BufferSizeInKB = buffer_size/8;
    params.mfx.GopOptFlag = MFX_GOP_CLOSED;
    params.mfx.GopPicSize = keyframe_interval_frames;
    params.mfx.GopRefDist = bframes+1;
    params.mfx.NumSlice = 1;
    if (main_profile)
        params.mfx.CodecProfile = MFX_PROFILE_AVC_MAIN;

    params.mfx.RateControlMethod = use_cbr ? MFX_RATECONTROL_CBR : MFX_RATECONTROL_VBR;
    params.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

    auto& fi = params.mfx.FrameInfo;
    ConvertFrameRate(fps, fi.FrameRateExtN, fi.FrameRateExtD);

    fi.FourCC = MFX_FOURCC_NV12;
    fi.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    fi.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

    fi.Width = align16(width);
    fi.Height = align16(height);

    fi.CropX = 0;
    fi.CropY = 0;
    fi.CropW = width;
    fi.CropH = height;
}

void Parameters::SetCodingOptionSPSPPS(mfxU8 *sps_buff, mfxU16 sps_size, mfxU8 *pps_buff, mfxU16 pps_size)
{
    if(!FindExt(cospspps))
    {
        zero(cospspps);
        cospspps.Header.BufferId = MFX_EXTBUFF_CODING_OPTION_SPSPPS;
        cospspps.Header.BufferSz = sizeof(cospspps);

        AddExt(cospspps);
    }
    cospspps.SPSBuffer = sps_buff;
    cospspps.SPSBufSize = sps_size;
    cospspps.PPSBuffer = pps_buff;
    cospspps.PPSBufSize = pps_size;
}

void Parameters::SetVideoSignalInfo(int full_range, int primaries, int transfer, int matrix)
{
    if(!FindExt(vsi))
    {
        zero(vsi);
        vsi.Header.BufferId = MFX_EXTBUFF_VIDEO_SIGNAL_INFO;
        vsi.Header.BufferSz = sizeof(vsi);

        AddExt(vsi);
    }

    vsi.ColourDescriptionPresent = 1;
    vsi.VideoFullRange = full_range;
    vsi.ColourPrimaries = primaries;
    vsi.TransferCharacteristics = transfer;
    vsi.MatrixCoefficients = matrix;
    vsi.VideoFormat = 5; //unspecified
}

void Parameters::UpdateExt()
{
    params.ExtParam = &ext_buffers.front();
    params.NumExtParam = ext_buffers.size();
}



void EncodeCtrl::AddSEIData(sei_type type, vector<mfxU8> data)
{
    unsigned payload_size = data.size();
    vector<mfxU8> buffer;

    mfxU16 type_ = type;
    while(type_ > 255)
    {
        buffer.emplace_back(0xff);
        type_ -= 255;
    }
    buffer.emplace_back((mfxU8)type_);

    while(payload_size > 255)
    {
        buffer.emplace_back(0xff);
        payload_size -= 255;
    }
    buffer.emplace_back(payload_size);

    buffer.insert(end(buffer), begin(data), end(data));

    data_buffers.emplace_back(buffer);
    payloads.emplace_back(mfxPayload());

    mfxPayload &sei_payload = payloads.back();

    zero(sei_payload);

    sei_payload.Type = type;
    sei_payload.BufSize = buffer.size();
    sei_payload.NumBit = sei_payload.BufSize*8;
    sei_payload.Data = &data_buffers.back().front();

    payload_list.clear();
    for(auto &payload = begin(payloads); payload != end(payloads); payload++)
        payload_list.emplace_back(&*payload);

    ctrl.Payload = &payload_list.front();
    ctrl.NumPayload = payload_list.size();
}
