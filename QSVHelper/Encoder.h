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
#include <fstream>
#include <queue>
#include <vector>

#include "d3d11_allocator.h"
#include "d3d11_device.h"

#include "IPCInfo.h"
#include "IPCStructs.h"
#include "SupportStuff.h"
#include "WindowsStuff.h"

struct Encoder
{
    bool use_cbr;

    bool first_frame;

    unsigned frame_time_ms;

    int exit_code;

    mfxIMPL requested, actual;
    mfxVersion version;


    Parameters params;
    mfxFrameAllocRequest req;
    mfxFrameAllocResponse alloc_res;


    bool using_d3d11;
    CD3D11Device d3d11;
    D3D11FrameAllocator d3d11_alloc;


    MFXVideoSession session;
    MFXVideoENCODE encoder;


    std::wstring event_prefix;

    ipc_encoder_flushed encoder_flushed;
    bool flushed;

    ipc_bitstream_buff bitstream;
    ipc_filled_bitstream filled_bitstream;
    ipc_bitstream_info bs_info;

    ipc_frame_buff frame_buff;
    ipc_frame_buff_status frame_buff_status;
    ipc_frame_queue frame_queue;

    ipc_sps_buff sps_buffer;
    ipc_pps_buff pps_buffer;
    ipc_spspps_size spspps_queried_size;


    std::vector<encode_task> encode_tasks;
    std::queue<size_t> idle_tasks, queued_tasks, encoded_tasks;

    std::vector<mfxFrameSurface1> surfaces;
    std::queue<mfxFrameSurface1*> idle_surfaces;
    std::vector<std::pair<mfxFrameSurface1*, uint32_t>> msdk_locked_tasks;

    std::vector<mfxFrameData> frames;


    EncodeCtrl keyframe_ctrl, sei_ctrl;


    std::wofstream &log_file;


    operator bool() { return static_cast<mfxSession>(session) != nullptr; }

    Encoder(IPCSignalledType<init_request> &init_req, std::wstring event_prefix, std::wofstream &log_file)
        : use_cbr(init_req->use_cbr), first_frame(true), frame_time_ms(static_cast<unsigned>(1./init_req->fps*1000)), exit_code(0)
        , using_d3d11(false), session(), encoder(session), event_prefix(event_prefix), encoder_flushed(event_prefix + ENCODER_FLUSHED), flushed(false), log_file(log_file)
    {
        params.Init(init_req->target_usage, init_req->profile, init_req->fps, init_req->keyint, init_req->bframes, init_req->width, init_req->height, init_req->max_bitrate,
            init_req->buffer_size, init_req->use_cbr, init_req->use_custom_parameters, init_req->custom_parameters, init_req->la_depth);
        params.SetVideoSignalInfo(init_req->full_range, init_req->primaries, init_req->transfer, init_req->matrix);
    }

    template <class T>
    mfxStatus InitializeMFX(T& impl, bool force=false)
    {
        session.Close();

        version = impl.version;
        requested = impl.type | impl.intf;
        auto result = session.Init(requested, &version);
        if(result < 0) return result;

        session.QueryIMPL(&actual);

        bool d3d11_initialized = using_d3d11;

        if(using_d3d11 = (actual & MFX_IMPL_VIA_D3D11) == MFX_IMPL_VIA_D3D11)
        {
            mfxU32 device = 0;
            switch(MFX_IMPL_BASETYPE(actual))
            {
            case MFX_IMPL_HARDWARE: device = 0; break;
            case MFX_IMPL_HARDWARE2: device = 1; break;
            case MFX_IMPL_HARDWARE3: device = 2; break;
            case MFX_IMPL_HARDWARE4: device = 3; break;
            default: exit_code = EXIT_D3D11_UNKNOWN_DEVICE; return MFX_ERR_DEVICE_FAILED;
            }

            d3d11_alloc.Close();

            result = d3d11.Init(nullptr, 1, device);
            if(result != MFX_ERR_NONE)
                return result;

            mfxHDL hdl = nullptr;
            d3d11.GetHandle(MFX_HANDLE_D3D11_DEVICE, &hdl);
            session.SetHandle(MFX_HANDLE_D3D11_DEVICE, hdl);

            D3D11AllocatorParams alloc_params;
            alloc_params.pDevice = reinterpret_cast<ID3D11Device*>(hdl);
            result = d3d11_alloc.Init(&alloc_params);
            if(result != MFX_ERR_NONE)
                return result;

            session.SetFrameAllocator(&d3d11_alloc);
            params->IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
        }

        if(!using_d3d11 && d3d11_initialized)
        {
            d3d11_alloc.Close();
            params->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
        }


        encoder = MFXVideoENCODE(session);

        zero(req);
        result = encoder.QueryIOSurf(&params, &req);
        return result;
    }

    void InitializeBuffers(ipc_init_response &init_res)
    {
        using namespace std;
        Parameters query = params;
        encoder.GetVideoParam(query);

        init_res->rate_control = query->mfx.RateControlMethod;

        switch (query->mfx.CodecProfile)
        {
        case MFX_PROFILE_AVC_BASELINE:
        case MFX_PROFILE_AVC_CONSTRAINED_HIGH:
        case MFX_PROFILE_AVC_CONSTRAINED_BASELINE:
            init_res->bframe_delay = 0;
            break;
        default:
            init_res->bframe_delay = 1;
        }

        init_res->bframe_delay = min(init_res->bframe_delay,
            min<uint16_t>(query->mfx.GopRefDist > 1 ? (query->mfx.GopRefDist - 1) : 0u,
                          query->mfx.GopPicSize > 2 ? (query->mfx.GopPicSize - 2) : 0u));

        init_res->frame_ticks = (uint64_t)((double)query->mfx.FrameInfo.FrameRateExtD / (double)query->mfx.FrameInfo.FrameRateExtN * 90000.);

        unsigned num_bitstreams = max(6, req.NumFrameSuggested + query->AsyncDepth),
                 num_surf = num_bitstreams * (using_d3d11 ? 2 : 1),
                 num_frames = using_d3d11 ? num_bitstreams : (num_surf + 3), //+NUM_OUT_BUFFERS
                 num_d3d11_frames = num_surf;

        encode_tasks.resize(num_bitstreams);

        const unsigned bs_size = (max(query->mfx.BufferSizeInKB*1000, params->mfx.BufferSizeInKB*1000)+31)/32*32;
        params->mfx.BufferSizeInKB = bs_size/1000;
        init_res->bitstream_size = bs_size;

        bitstream = ipc_bitstream_buff(event_prefix + BITSTREAM_BUFF, encode_tasks.size() * bs_size + 31);
        mfxU8 *bs_start = (mfxU8*)(((size_t)&bitstream + 31)/32*32);
        size_t index = 0;
        for(auto task = begin(encode_tasks); task != end(encode_tasks); task++, index++)
        {
            task->Init(bs_start, bs_size);
            idle_tasks.push(index);
            bs_start += bs_size;
        }

        filled_bitstream = ipc_filled_bitstream(event_prefix + FILLED_BITSTREAM);
        {
            auto lock = lock_mutex(filled_bitstream);
            *filled_bitstream = -1;
        }

        bs_info = ipc_bitstream_info(event_prefix + BITSTREAM_INFO, encode_tasks.size());


        if(using_d3d11)
        {
            req.NumFrameSuggested = num_d3d11_frames;
            d3d11_alloc.AllocFrames(&req, &alloc_res);
        }

        mfxFrameInfo &fi = params->mfx.FrameInfo;

        surfaces.resize(num_surf);
        for(size_t i = 0; i < surfaces.size(); i++)
        {
            idle_surfaces.emplace(&surfaces[i]);
            memcpy(&surfaces[i].Info, &fi, sizeof(fi));
            if(using_d3d11)
                surfaces[i].Data.MemId = alloc_res.mids[i];
        }

        const unsigned lum_channel_size = fi.Width*fi.Height,
                       uv_channel_size = fi.Width*fi.Height,
                       frame_size = lum_channel_size + uv_channel_size;
        init_res->frame_size = frame_size;
        init_res->UV_offset = lum_channel_size;
        init_res->V_offset = lum_channel_size+1;
        init_res->frame_pitch = fi.Width;

        frames.resize(num_frames);
        frame_queue = ipc_frame_queue(event_prefix + FRAME_QUEUE, frames.size());
        {
            auto lock = lock_mutex(frame_queue);
            zero(*static_cast<queued_frame*>(frame_queue), sizeof(queued_frame) * frame_queue.size);
        }

        frame_buff = ipc_frame_buff(event_prefix + FRAME_BUFF, frames.size() * frame_size + 15);
        mfxU8 *frame_start = (mfxU8*)(((size_t)&frame_buff + 15)/16*16);
        zero(*frame_start, frame_size * frames.size());
        for(auto frame = begin(frames); frame != end(frames); frame++)
        {
            InitFrame(*frame, frame_start, frame_start  + init_res->UV_offset, frame_start + init_res->V_offset, fi.Width);
            frame_start += frame_size;
        }

        frame_buff_status = ipc_frame_buff_status(event_prefix + FRAME_BUFF_STATUS, frames.size());
        {
            auto lock = lock_mutex(frame_buff_status);
            zero(frame_buff_status[0], frames.size() * sizeof(uint32_t));
        }

        init_res->target_usage = params->mfx.TargetUsage;
        init_res->profile = params->mfx.CodecProfile;
        init_res->bitstream_num = encode_tasks.size();
        init_res->frame_num = frames.size();

        keyframe_ctrl.ctrl.FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR;
        sei_ctrl.AddSEIData(EncodeCtrl::SEI_USER_DATA_UNREGISTERED, InitSEIUserData(use_cbr, query, init_res->version));
    }

    mfxStatus InitializeEncoder()
    {
        return encoder.Init(params);
    }

    void RequestSPSPPS()
    {
        sps_buffer = ipc_sps_buff(event_prefix + SPS_BUFF, 100);
        pps_buffer = ipc_pps_buff(event_prefix + PPS_BUFF, 100);
        Parameters spspps_query;
        spspps_query.SetCodingOptionSPSPPS(sps_buffer, sps_buffer.size, pps_buffer, pps_buffer.size);
        encoder.GetVideoParam(spspps_query);
        spspps_queried_size = ipc_spspps_size(event_prefix + SPSPPS_SIZES);
        spspps_queried_size->sps_size = spspps_query.cospspps.SPSBufSize;
        spspps_queried_size->pps_size = spspps_query.cospspps.PPSBufSize;
        spspps_queried_size.signal();
    }

    void ProcessEncodedFrame()
    {
        if(encoded_tasks.size())
        {
            encode_task& task = encode_tasks[encoded_tasks.front()];
            auto& sp = task.sp;

            auto result = MFXVideoCORE_SyncOperation(session, sp, 0);
            if(result == MFX_WRN_IN_EXECUTION)
                return;

            if (flushed)
                return;

            bitstream_info &info = bs_info[encoded_tasks.front()];
            info.time_stamp = task.bs.TimeStamp;
            info.data_length = task.bs.DataLength;
            info.data_offset = task.bs.DataOffset;
            info.pic_struct = task.bs.PicStruct;
            info.frame_type = task.bs.FrameType;
            info.decode_time_stamp = task.bs.DecodeTimeStamp;

            {
                auto lock = lock_mutex(filled_bitstream);
                if(*filled_bitstream >= 0)
                    return;
                *filled_bitstream = encoded_tasks.front();
            }
            filled_bitstream.signal();

            idle_tasks.emplace(encoded_tasks.front());
            encoded_tasks.pop();

            if (!task.surf)
                return;

            msdk_locked_tasks.emplace_back(std::make_pair(task.surf, task.frame_index));
            task.surf = nullptr;
        }
    }

    void UnlockSurfaces()
    {
        for(size_t i = 0; i < msdk_locked_tasks.size();)
        {
            auto pair = msdk_locked_tasks[i];
            if(pair.first->Data.Locked)
            {
                i += 1;
                continue;
            }

            msdk_locked_tasks.erase(std::begin(msdk_locked_tasks)+i);

            idle_surfaces.emplace(pair.first);

            if(!using_d3d11)
            {
                auto lock = lock_mutex(frame_buff_status);
                frame_buff_status[pair.second] -= 1;
            }
        }
    }

    void QueueTask()
    {
        using namespace std;

        for(;;)
        {
            if(idle_tasks.empty())
            {
                log_file << "Warning: idle_tasks is empty (" << idle_tasks.size() << " idle, " << queued_tasks.size() << " queued, "
                         << encoded_tasks.size() << " encoded, " << msdk_locked_tasks.size() << " locked)\n";
                return;
            }
            
            if(idle_surfaces.empty())
            {
                log_file << "Warning: idle_surfaces is empty (" << idle_tasks.size() << " idle, " << queued_tasks.size() << " queued, "
                         << encoded_tasks.size() << " encoded, " << msdk_locked_tasks.size() << " locked)\n";
                return;
            }

            auto end = static_cast<queued_frame*>(frame_queue)+frame_queue.size;
            auto lock = lock_mutex(frame_queue);
            auto oldest = min_element(static_cast<queued_frame*>(frame_queue), end, [](const queued_frame &f1, const queued_frame &f2) -> bool
            {
                if(f1.is_new)
                    if(f2.is_new)
                        return f1.timestamp < f2.timestamp;
                    else
                        return true;
                return false;
            });
            if(!oldest || !oldest->is_new)
                return;

            oldest->is_new = false;

            auto index = idle_tasks.front();
            queued_tasks.push(index);
            idle_tasks.pop();

            encode_task &task = encode_tasks[index];
            task.bs.DataLength = 0;
            task.bs.DataOffset = 0;

            if(oldest->request_keyframe)
                task.ctrl = &keyframe_ctrl;
            else
                task.ctrl = nullptr;

            if(first_frame)
                task.ctrl = &sei_ctrl;
            first_frame = false;

            if (oldest->flush)
            {
                task.surf = nullptr;
                return;
            }

            task.surf = idle_surfaces.front();
            idle_surfaces.pop();

            mfxFrameData &frame = frames[oldest->frame_index];
            if(using_d3d11)
            {
                d3d11_alloc.LockFrame(task.surf->Data.MemId, &task.surf->Data);
                for(size_t i = 0; i < task.surf->Info.Height; i++)
                    memcpy(task.surf->Data.Y+i*task.surf->Data.Pitch, frame.Y+i*frame.Pitch, task.surf->Info.Width);
                for(size_t i = 0; i < (task.surf->Info.Height/2u); i++)
                    memcpy(task.surf->Data.UV+i*task.surf->Data.Pitch, frame.UV+i*frame.Pitch, task.surf->Info.Width);
                d3d11_alloc.UnlockFrame(task.surf->Data.MemId, &task.surf->Data);
                auto lock = lock_mutex(frame_buff_status);
                frame_buff_status[oldest->frame_index] -= 1;
            }
            else
            {
                task.surf->Data.Y = frame.Y;
                task.surf->Data.UV = frame.UV;
                task.surf->Data.V = frame.V;
                task.surf->Data.Pitch = frame.Pitch;
            }
            task.surf->Data.TimeStamp = oldest->timestamp;
            task.frame_index = oldest->frame_index;
        }
    }

    void EncodeTasks()
    {
        while(queued_tasks.size())
        {
            encode_task& task = encode_tasks[queued_tasks.front()];
            for(;;)
            {
                auto sts = encoder.EncodeFrameAsync(task.ctrl, task.surf, &task.bs, &task.sp);

                if (sts == MFX_ERR_MORE_DATA && !task.surf)
                {
                    encoder_flushed.signal();
                    flushed = true;
                    idle_tasks.push(queued_tasks.front());
                    queued_tasks.pop();
                    return;
                }

                if(sts == MFX_ERR_NONE || (MFX_ERR_NONE < sts && task.sp))
                    break;
                if(sts == MFX_WRN_DEVICE_BUSY)
                    return;
                if(sts == MFX_ERR_NOT_INITIALIZED) //returned after encoder.Init returns PARTIAL_ACCELERATION?
                {
                    exit_code = EXIT_INCOMPATIBLE_CONFIGURATION;
                    return;
                }
                //if(!sp); //sts == MFX_ERR_MORE_DATA usually; retry the call (see MSDK examples)
                //Log(TEXT("returned status %i, %u"), sts, insert);
            }
            encoded_tasks.push(queued_tasks.front());
            queued_tasks.pop();
        }
    }

    int EncodeLoop(ipc_stop &stop, safe_handle &obs_handle)
    {
        IPCWaiter waiter;
        waiter.push_back(stop.signal_);
        waiter.push_back(obs_handle);
        waiter.push_back(frame_queue.signal_);

        for(;;)
        {
            if(waiter.wait_for_two(0, 1, frame_time_ms/2) || exit_code)
                return exit_code;
            ProcessEncodedFrame();
            UnlockSurfaces();
            QueueTask();
            EncodeTasks();
        }
    }
};