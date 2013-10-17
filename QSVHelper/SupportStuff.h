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

std::vector<mfxU8> InitSEIUserData(bool use_cbr, const mfxVideoParam& params, const mfxVersion& ver)
{
    using namespace std;
    vector<mfxU8> data;

    const mfxU8 UUID[] = { 0x6d, 0x1a, 0x26, 0xa0, 0xbd, 0xdc, 0x11, 0xe2,   //ISO-11578 UUID
                           0x90, 0x24, 0x00, 0x50, 0xc2, 0x49, 0x00, 0x48 }; //6d1a26a0-bddc-11e2-9024-0050c2490048
    data.insert(end(data), begin(UUID), end(UUID));

    ostringstream str;
    str << "QSV hardware encoder options:"
        << " rate control: " << (use_cbr ? "cbr" : "vbr")
        << "; target bitrate: " << params.mfx.TargetKbps
        << "; max bitrate: " << params.mfx.MaxKbps
        << "; buffersize: " << params.mfx.BufferSizeInKB*8
        << "; API level: " << ver.Major << "." << ver.Minor;
    string str_(str.str());

    data.insert(end(data), begin(str_), end(str_));

    return data;
}