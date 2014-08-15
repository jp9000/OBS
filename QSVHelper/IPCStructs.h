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

#include <cstdint>

#pragma pack(push)
#pragma pack(1)

struct init_request
{
    enum { MODE_QUERY, MODE_ENCODE } mode;
    uint32_t obs_process_id;
    uint16_t target_usage, profile;
    int32_t fps, keyint, bframes, width, height, max_bitrate, buffer_size;
    bool use_cbr, use_custom_parameters;
    mfxInfoMFX custom_parameters;
    decltype(mfxExtCodingOption2::LookAheadDepth) la_depth;
    int32_t full_range, matrix, primaries, transfer;
    bool use_custom_impl;
    mfxVersion custom_version;
    mfxIMPL custom_impl, custom_intf;
};

struct init_response
{
    mfxU16 target_usage, profile;
    mfxVersion version;
    mfxIMPL requested_impl,
            actual_impl;
    decltype(mfxInfoMFX::RateControlMethod) rate_control;

    bool using_custom_impl;

    uint16_t bframe_delay;
    uint64_t frame_ticks;

    uint16_t bitstream_num,
             frame_num;
    uint32_t bitstream_size,
             frame_size,
             UV_offset,
             V_offset,
             frame_pitch;
};

struct spspps_size
{
    mfxU16 sps_size,
           pps_size;
};

struct queued_frame
{
    bool is_new;
    bool request_keyframe;
    bool flush;
    mfxU64 timestamp;
    uint32_t frame_index;
};

struct bitstream_info
{
    mfxU64 time_stamp;
    mfxI64 decode_time_stamp;
    mfxU32 data_offset, data_length;
    mfxU16 pic_struct, frame_type;
};

#pragma pack(pop)