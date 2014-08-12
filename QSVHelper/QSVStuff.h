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

#include <algorithm>
#include <iostream>
#include <vector>

#include "Utilities.h"

template <class T>
inline T align16(T t)
{
    return (((t + 15) >> 4) << 4);
}

struct Parameters
{
private:
    mfxVideoParam params;
    std::vector<const mfxExtBuffer*> ext_buffers;

public:
    mfxExtCodingOptionSPSPPS cospspps;
    mfxExtVideoSignalInfo vsi;
    mfxExtCodingOption co;
    mfxExtCodingOption2 co2;


    operator mfxVideoParam() { return params; }
    operator mfxVideoParam*() { return &params; }
    mfxVideoParam* operator&() { return &params; }
    mfxVideoParam* operator->() { return &params; }
    const mfxVideoParam* operator->() const { return &params; }

    void Init(mfxU16 preset, mfxU16 profile, int fps, int keyframe_interval_frames, int bframes, int width, int height, int max_bitrate,
        int buffer_size, bool use_cbr, bool use_custom_params, mfxInfoMFX custom_params, decltype(mfxExtCodingOption2::LookAheadDepth) la_depth);
    void SetCodingOptionSPSPPS(mfxU8 *sps_buff, mfxU16 sps_size, mfxU8 *pps_buff, mfxU16 pps_size);
    void SetVideoSignalInfo(int full_range, int primaries, int transfer, int matrix);
    void AddCodingOption();
    void AddCodingOption2();
    
    Parameters();
    Parameters(const Parameters&);
    Parameters &operator =(const Parameters&);

    void Dump(std::wostream&);
protected:
    void UpdateExt();
    template <class T>
    bool FindExt(const T& t) const { return std::find(std::begin(ext_buffers), std::end(ext_buffers), reinterpret_cast<const mfxExtBuffer*>(&t)) != std::end(ext_buffers); }
    template <class T>
    void AddExt(T& t) { ext_buffers.emplace_back(reinterpret_cast<mfxExtBuffer*>(&t)); UpdateExt(); }
    template <class T>
    void InitAddExt(T& t, decltype(mfxExtBuffer::BufferId) id) { zero(t); t.Header.BufferId = id; t.Header.BufferSz = sizeof(T); AddExt(t); }
};


struct EncodeCtrl
{
    enum sei_type {SEI_USER_DATA_UNREGISTERED=0x5};
    mfxEncodeCtrl ctrl;
    std::vector<mfxPayload*> payload_list;
    std::vector<mfxPayload> payloads;
    std::vector<std::vector<mfxU8>> data_buffers;

    operator mfxEncodeCtrl() { return ctrl; }
    mfxEncodeCtrl *operator&() { return &ctrl; }

    void AddSEIData(sei_type type, std::vector<mfxU8> payload);

    EncodeCtrl() { zero(ctrl); }
};
