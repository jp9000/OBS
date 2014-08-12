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

#pragma once

#include <mfxvideo++.h>

#include "Utilities.h"
#include "QSVStuff.h"

struct encode_task
{
    mfxFrameSurface1 *surf;
    mfxBitstream bs;
    mfxSyncPoint sp;
    mfxEncodeCtrl *ctrl;
    size_t frame_index;

    void Init(mfxU8 *bs_start, mfxU32 bs_size)
    {
        sp = nullptr;
        ctrl = nullptr;
        surf = nullptr;

        zero(bs);
        bs.Data = bs_start;
        bs.MaxLength = bs_size;
    }
};

void InitFrame(mfxFrameData &frame, mfxU8 *Y, mfxU8 *UV, mfxU8 *V, mfxU16 pitch)
{
    zero(frame);
    frame.Y = Y;
    frame.UV = UV;
    frame.V = V;
    frame.Pitch = pitch;
}

std::vector<mfxU8> InitSEIUserData(bool use_cbr, const Parameters& params, const mfxVersion& ver)
{
#define TO_STR(x) #x
    static const char *usage_str[] = {
        TO_STR(MFX_TARGETUSAGE_UNKNOWN),
        TO_STR(MFX_TARGETUSAGE_BEST_QUALITY),
        TO_STR(MFX_TARGETUSAGE_2),
        TO_STR(MFX_TARGETUSAGE_3),
        TO_STR(MFX_TARGETUSAGE_BALANCED),
        TO_STR(MFX_TARGETUSAGE_5),
        TO_STR(MFX_TARGETUSAGE_6),
        TO_STR(MFX_TARGETUSAGE_BEST_SPEED)
    };
#undef TO_STR

#define RATE_CONTROL(x) {MFX_RATECONTROL_##x, #x}
    struct
    {
        decltype(mfxInfoMFX::RateControlMethod) method;
        const char* name;
    } rate_control_str[] = {
        RATE_CONTROL(CBR),
        RATE_CONTROL(VBR),
        RATE_CONTROL(CQP),
        RATE_CONTROL(AVBR),
        RATE_CONTROL(LA),
        RATE_CONTROL(ICQ),
        RATE_CONTROL(VCM),
        RATE_CONTROL(LA_ICQ),
    };
#undef RATE_CONTROL

    using namespace std;
    vector<mfxU8> data;

    const mfxU8 UUID[] = { 0x6d, 0x1a, 0x26, 0xa0, 0xbd, 0xdc, 0x11, 0xe2,   //ISO-11578 UUID
                           0x90, 0x24, 0x00, 0x50, 0xc2, 0x49, 0x00, 0x48 }; //6d1a26a0-bddc-11e2-9024-0050c2490048
    data.insert(end(data), begin(UUID), end(UUID));

    auto method = params->mfx.RateControlMethod;

    const char *name = "UKNOWN";
    for (const auto &names : rate_control_str)
    {
        if (names.method != method)
            continue;

        name = names.name;
        break;
    }

    ostringstream str;
    str << "QSV hardware encoder options:"
        << " rate control: "        << name;
    switch (method)
    {
    case MFX_RATECONTROL_CBR:
    case MFX_RATECONTROL_VBR:
    case MFX_RATECONTROL_VCM:
        str << "; target bitrate: "  << params->mfx.TargetKbps;
        if (method != MFX_RATECONTROL_CBR)
            str << "; max bitrate: " << params->mfx.MaxKbps;
        str << "; buffersize: "      << params->mfx.BufferSizeInKB * 8;
        break;

    case MFX_RATECONTROL_AVBR:
        str << "; target bitrate: "  << params->mfx.TargetKbps
            << "; accuracy: "        << params->mfx.Accuracy
            << "; convergence: "     << params->mfx.Convergence;
        break;

    case MFX_RATECONTROL_CQP:
        str << "; QPI: "             << params->mfx.QPI
            << "; QPP: "             << params->mfx.QPP
            << "; QPB: "             << params->mfx.QPB;
        break;

    case MFX_RATECONTROL_LA:
        str << "; target bitrate: "  << params->mfx.TargetKbps
            << "; look ahead: "      << params.co2.LookAheadDepth;
        break;

    case MFX_RATECONTROL_ICQ:
    case MFX_RATECONTROL_LA_ICQ:
        str << "; ICQQuality: "      << params->mfx.ICQQuality;
        if (method == MFX_RATECONTROL_LA_ICQ)
            str << "; look ahead: "  << params.co2.LookAheadDepth;
        break;
    }
    str << "; API level: "           << ver.Major << "." << ver.Minor
        << "; Target Usage: "        << usage_str[params->mfx.TargetUsage];

    string str_(str.str());

    data.insert(end(data), begin(str_), end(str_));

    return data;
}