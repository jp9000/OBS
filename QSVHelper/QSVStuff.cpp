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

Parameters::Parameters(const Parameters& o) : Parameters()
{
    *this = o;
}

Parameters &Parameters::operator =(const Parameters& o)
{
    params = o.params;
    cospspps = o.cospspps;
    vsi = o.vsi;
    co = o.co;
    co2 = o.co2;

    if (o.FindExt(o.cospspps))
        AddExt(cospspps);
    if (o.FindExt(o.vsi))
        AddExt(vsi);
    if (o.FindExt(o.co))
        AddExt(co);
    if (o.FindExt(o.co2))
        AddExt(co2);

    return *this;
}

void Parameters::Init(mfxU16 target_usage, mfxU16 profile, int fps, int keyframe_interval_frames, int bframes, int width, int height, int max_bitrate,
    int buffer_size, bool use_cbr, bool use_custom_params, mfxInfoMFX custom_params, decltype(mfxExtCodingOption2::LookAheadDepth) la_depth)
{
    params.mfx.CodecId = MFX_CODEC_AVC;
    params.mfx.TargetUsage = target_usage;
    params.mfx.GopOptFlag = MFX_GOP_CLOSED;
    params.mfx.GopPicSize = keyframe_interval_frames;
    params.mfx.GopRefDist = bframes+1;
    params.mfx.NumSlice = 1;
    params.mfx.CodecProfile = profile;

    params.mfx.TargetKbps = saturate<mfxU16>(use_custom_params ? custom_params.TargetKbps : max_bitrate);
    params.mfx.BufferSizeInKB = use_custom_params ? custom_params.BufferSizeInKB : buffer_size / 8;

    params.mfx.RateControlMethod = use_custom_params ? custom_params.RateControlMethod : use_cbr ? MFX_RATECONTROL_CBR : MFX_RATECONTROL_VBR;
    switch (params.mfx.RateControlMethod)
    {
    case MFX_RATECONTROL_VBR:
    case MFX_RATECONTROL_VCM:
        params.mfx.MaxKbps = use_custom_params ? custom_params.MaxKbps : 0;
        break;

    case MFX_RATECONTROL_AVBR:
        params.mfx.Accuracy = custom_params.Accuracy;
        params.mfx.Convergence = custom_params.Convergence;
        break;

    case MFX_RATECONTROL_CQP:
        params.mfx.QPI = custom_params.QPI;
        params.mfx.QPP = custom_params.QPP;
        params.mfx.QPB = custom_params.QPB;
        break;

    case MFX_RATECONTROL_ICQ:
    case MFX_RATECONTROL_LA_ICQ:
        params.mfx.ICQQuality = custom_params.ICQQuality;
    case MFX_RATECONTROL_LA:
        AddCodingOption2();
        co2.LookAheadDepth = la_depth;
        break;
    }

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
        InitAddExt(cospspps, MFX_EXTBUFF_CODING_OPTION_SPSPPS);

    cospspps.SPSBuffer = sps_buff;
    cospspps.SPSBufSize = sps_size;
    cospspps.PPSBuffer = pps_buff;
    cospspps.PPSBufSize = pps_size;
}

void Parameters::SetVideoSignalInfo(int full_range, int primaries, int transfer, int matrix)
{
    if (!FindExt(vsi))
        InitAddExt(vsi, MFX_EXTBUFF_VIDEO_SIGNAL_INFO);

    vsi.ColourDescriptionPresent = 1;
    vsi.VideoFullRange = full_range;
    vsi.ColourPrimaries = primaries;
    vsi.TransferCharacteristics = transfer;
    vsi.MatrixCoefficients = matrix;
    vsi.VideoFormat = 5; //unspecified
}

void Parameters::AddCodingOption()
{
    if (!FindExt(co))
        InitAddExt(co, MFX_EXTBUFF_CODING_OPTION);
}

void Parameters::AddCodingOption2()
{
    if (!FindExt(co2))
        InitAddExt(co2, MFX_EXTBUFF_CODING_OPTION2);
}

void Parameters::UpdateExt()
{
    params.ExtParam = const_cast<mfxExtBuffer**>(&ext_buffers.front());
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

void Parameters::Dump(std::wostream &log_file)
{
    auto &mfx = params.mfx;
    
#define OUT(name) log_file << "  mfx." #name " = " << mfx.name << '\n'
    OUT(BRCParamMultiplier);
    OUT(CodecId);
    OUT(CodecProfile);
    OUT(CodecLevel);
    OUT(NumThread);
    OUT(TargetUsage);
    OUT(GopPicSize);
    OUT(GopRefDist);
    OUT(GopOptFlag);
    OUT(IdrInterval);
    OUT(RateControlMethod);
    OUT(InitialDelayInKB);
    OUT(BufferSizeInKB);
    OUT(TargetKbps);
    OUT(MaxKbps);
    OUT(NumSlice);
    OUT(NumRefFrame);
    OUT(EncodedOrder);
#undef OUT

    for (auto i : ext_buffers)
    {
#define OUT(name) log_file << "   " #name " = " << name << '\n'
        switch (i->BufferId)
        {
        case MFX_EXTBUFF_VIDEO_SIGNAL_INFO:
            OUT(vsi.VideoFormat);
            OUT(vsi.VideoFullRange);
            OUT(vsi.ColourDescriptionPresent);
            OUT(vsi.ColourPrimaries);
            OUT(vsi.TransferCharacteristics);
            OUT(vsi.MatrixCoefficients);
            break;

        case MFX_EXTBUFF_CODING_OPTION:
            OUT(co.RateDistortionOpt);
            OUT(co.MECostType);
            OUT(co.MESearchType);
            OUT(co.MVSearchWindow.x);
            OUT(co.MVSearchWindow.y);
            OUT(co.EndOfSequence);
            OUT(co.FramePicture);
            OUT(co.CAVLC);
            OUT(co.RecoveryPointSEI);
            OUT(co.ViewOutput);
            OUT(co.NalHrdConformance);
            OUT(co.SingleSeiNalUnit);
            OUT(co.VuiVclHrdParameters);
            OUT(co.RefPicListReordering);
            OUT(co.ResetRefList);
            OUT(co.RefPicMarkRep);
            OUT(co.FieldOutput);
            OUT(co.IntraPredBlockSize);
            OUT(co.InterPredBlockSize);
            OUT(co.MVPrecision);
            OUT(co.MaxDecFrameBuffering);
            OUT(co.AUDelimiter);
            OUT(co.EndOfStream);
            OUT(co.PicTimingSEI);
            OUT(co.VuiNalHrdParameters);
            break;

        case MFX_EXTBUFF_CODING_OPTION2:
            OUT(co2.IntRefType);
            OUT(co2.IntRefCycleSize);
            OUT(co2.IntRefQPDelta);
            OUT(co2.MaxFrameSize);
            OUT(co2.MaxSliceSize);
            OUT(co2.BitrateLimit);
            OUT(co2.MBBRC);
            OUT(co2.ExtBRC);
            OUT(co2.LookAheadDepth);
            OUT(co2.Trellis);
            OUT(co2.RepeatPPS);
            OUT(co2.BRefType);
            OUT(co2.AdaptiveI);
            OUT(co2.AdaptiveB);
            OUT(co2.LookAheadDS);
            OUT(co2.NumMbPerSlice);
            OUT(co2.SkipFrame);
            OUT(co2.MinQPI);
            OUT(co2.MaxQPI);
            OUT(co2.MinQPP);
            OUT(co2.MaxQPP);
            OUT(co2.MinQPB);
            OUT(co2.MaxQPB);
            OUT(co2.FixedFrameRate);
            OUT(co2.DisableDeblockingIdc);
            break;
        }
    }
    log_file << '\n';
}
