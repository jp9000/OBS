/*
 ***************************************************************************************************
 *
 * Copyright (c) 2014 Advanced Micro Devices, Inc. (unpublished)
 *
 *  All rights reserved.  This notice is intended as a precaution against inadvertent publication and
 *  does not imply publication or any waiver of confidentiality.  The year included in the foregoing
 *  notice is the year of creation of the work.
 *
 ***************************************************************************************************
 */
/**
 ***************************************************************************************************
 * @file  VideoDecoderUVD.h
 * @brief VideoDecoderHW_UVD interface declaration
 ***************************************************************************************************
 */
 
#ifndef __VideoDecoderHW_UVD_h__
#define __VideoDecoderHW_UVD_h__
#pragma once

#include "Component.h"

#define AMFVideoDecoderUVD_MPEG2                     L"AMFVideoDecoderUVD_MPEG2"
#define AMFVideoDecoderUVD_MPEG4                     L"AMFVideoDecoderUVD_MPEG4"
#define AMFVideoDecoderUVD_WMV3                      L"AMFVideoDecoderUVD_WMV3"
#define AMFVideoDecoderUVD_VC1                       L"AMFVideoDecoderUVD_VC1"
#define AMFVideoDecoderUVD_H264_AVC                  L"AMFVideoDecoderUVD_H264_AVC"
#define AMFVideoDecoderUVD_H264_MVC                  L"AMFVideoDecoderUVD_H264_MVC"
#define AMFVideoDecoderUVD_H264_SVC                  L"AMFVideoDecoderUVD_H264_SVC"
#define AMFVideoDecoderUVD_MJPEG                     L"AMFVideoDecoderUVD_MJPEG"

enum AMF_VIDEO_DECODER_MODE_ENUM
{
    AMF_VIDEO_DECODER_MODE_REGULAR = 0,     // DPB delay is based on number of reference frames + 1 (from SPS)
    AMF_VIDEO_DECODER_MODE_COMPLIANT,       // DPB delay is based on profile - up to 16
    AMF_VIDEO_DECODER_MODE_LOW_LATENCY,     // DPB delay is 0. Expect stream with no reordering in P-Frames or B-Frames. B-frames can be present as long as they do not introduce any frame re-ordering 
};
enum AMF_TIMESTAMP_MODE_ENUM
{
    AMF_TS_PRESENTATION = 0, // default. decoder will preserve timestamps from input to output
   	AMF_TS_SORT,             // decoder will resort PTS list 
    AMF_TS_DECODE            // timestamps reflect decode order - decoder will reuse them
};

#define AMF_VIDEO_DECODER_DECODER_SURFACE_COPY         L"SurfaceCopy"           // amf_bool; default = false; return output surfaces as a copy
#define AMF_VIDEO_DECODER_EXTRADATA                    L"ExtraData"             // AMFInterface* -> AMFBuffer* - AVCC - size length + SPS/PPS; Don't set if stream is Annex B.
#define AMF_VIDEO_DECODER_FRAME_RATE                   L"FrameRate"             // amf_double; default = 0.0, optional property to restore duration in the output if needed
#define AMF_TIMESTAMP_MODE                             L"TimestampMode"         // amf_int64(AMF_TIMESTAMP_MODE_ENUM)  - default AMF_TS_PRESENTATION - how input timestamps are treated

// dynamic/adaptive resolution change
#define AMF_VIDEO_DECODER_ADAPTIVE_RESOLUTION_CHANGE   L"AdaptiveResolutionChange" // amf_bool; default = false; reuse allocated surfaces if new resolution is smaller
#define AMF_VIDEO_DECODER_ALLOC_SIZE                   L"AllocSize"             // AMFSize; default (1920,1088); size of allocated surface if AdaptiveResolutionChange is true
#define AMF_VIDEO_DECODER_CURRENT_SIZE                 L"CurrentSize"           // AMFSize; default = (0,0); current size of the video

// reference frame management
#define AMF_VIDEO_DECODER_REORDER_MODE                 L"ReorderMode"           // amf_int64(AMF_VIDEO_DECODER_MODE_ENUM); default = AMF_VIDEO_DECODER_MODE_REGULAR;  defines number of surfaces in DPB list.
#define AMF_VIDEO_DECODER_SURFACE_POOL_SIZE            L"SurfacePoolSize"       // amf_int64; number of surfaces in the decode pool = DPB list size + number of surfaces for presentation
#define AMF_VIDEO_DECODER_DPB_SIZE                     L"DPBSize"               // amf_int64; minimum number of surfaces for reordering

#define AMF_VIDEO_DECODER_DEFAULT_SURFACES_FOR_TRANSIT  5                      // if AMF_VIDEO_DECODER_SURFACE_POOL_SIZE is 0 , AMF_VIDEO_DECODER_SURFACE_POOL_SIZE=AMF_VIDEO_DECODER_DEFAULT_SURFACES_FOR_TRANSIT+AMF_VIDEO_DECODER_DPB_SIZE

#endif //#ifndef __VideoDecoderHW_UVD_h__
