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

#include <cstdint>

#include "IPCStructs.h"
#include "WindowsStuff.h"


#define INIT_REQUEST                                L"init_request"
typedef IPCSignalledType<init_request>              ipc_init_request;

#define INIT_RESPONSE                               L"init_response"
typedef IPCSignalledType<init_response>             ipc_init_response;

#define BITSTREAM_BUFF                              L"bitstream_buff"
typedef NamedSharedMemory                           ipc_bitstream_buff;

#define FILLED_BITSTREAM                            L"filled_bitstream"
typedef IPCLockedSignalledType<int32_t>             ipc_filled_bitstream;

#define BITSTREAM_INFO                              L"bitstream_info"
typedef IPCArray<bitstream_info>                    ipc_bitstream_info;

#define FRAME_BUFF                                  L"frame_buff"
typedef NamedSharedMemory                           ipc_frame_buff;

#define FRAME_BUFF_STATUS                           L"frame_buff_status"
typedef IPCLockedSignalledArray<uint32_t>           ipc_frame_buff_status;

#define FRAME_QUEUE                                 L"frame_queue"
typedef IPCLockedSignalledArray<queued_frame>       ipc_frame_queue;

#define SPS_BUFF                                    L"sps_buff"
typedef IPCArray<mfxU8>                             ipc_sps_buff;

#define PPS_BUFF                                    L"pps_buff"
typedef IPCArray<mfxU8>                             ipc_pps_buff;

#define SPSPPS_SIZES                                L"spspps_size"
typedef IPCSignalledType<spspps_size>               ipc_spspps_size;

#define STOP_REQUEST                                L"stop"
typedef IPCSignal<true>                             ipc_stop;

#define ENCODER_FLUSHED                             L"encoder_flushed"
typedef IPCSignal<true>                             ipc_encoder_flushed;



#define EXIT_INIT_IPC_FAILED 1
#define EXIT_NO_INTEL_GRAPHICS 2
#define EXIT_INIT_QUERY_FAILED 3
#define EXIT_IPC_OBS_HANDLE_FAILED 4
#define EXIT_ENCODER_INIT_FAILED 6
#define EXIT_NO_VALID_CONFIGURATION 5
#define EXIT_INCOMPATIBLE_CONFIGURATION 10
#define EXIT_D3D11_UNKNOWN_DEVICE 11
#define EXIT_LOG_FILE_OPEN_FAILED 100
