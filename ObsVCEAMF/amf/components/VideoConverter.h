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
 * @file  VideoConverter.h
 * @brief AMFFVideoConverter interface declaration
 ***************************************************************************************************
 */
 
#ifndef __AMFVideoConverter_h__
#define __AMFVideoConverter_h__
#pragma once

#include "Component.h"

#define AMFVideoConverter L"AMFVideoConverter"

#define AMF_VIDEO_CONVERTER_OUTPUT_FORMAT       L"OutputFormat"             // Values : AMF_SURFACE_NV12 or AMF_SURFACE_BGRA or AMF_SURFACE_YUV420P
#define AMF_VIDEO_CONVERTER_MEMORY_TYPE         L"MemoryType"               // Values : AMF_MEMORY_DX11 or AMF_MEMORY_DX9 or AMF_MEMORY_UNKNOWN (get from input type)

#define AMF_VIDEO_CONVERTER_OUTPUT_SIZE         L"OutputSize"               // AMFSize  (default=0,0) width in pixels. default means no scaling
#define AMF_VIDEO_CONVERTER_OUTPUT_OFFSET       L"OutputOffset"             // AMFPoint (default=0,0) offset in pixels. default means no offset

#define AMF_VIDEO_CONVERTER_KEEP_ASPECT_RATIO   L"KeepAspectRatio"          // bool (default=false) Keep aspect ratio if scaling. 
#define AMF_VIDEO_CONVERTER_FILL                L"Fill"                     // bool (default=false) fill area out of ROI. 
#define AMF_VIDEO_CONVERTER_FILL_COLOR          L"FillColor"                // AMFColor 


#define AMF_VIDEO_CONVERTER_SCALE               L"ScaleType"

enum AMF_VIDEO_CONVERTER_SCALE_ENUM
{
    AMF_VIDEO_CONVERTER_SCALE_INVALID          = -1,
    AMF_VIDEO_CONVERTER_SCALE_BILINEAR          = 0,
    AMF_VIDEO_CONVERTER_SCALE_BICUBIC           = 1
};

#define AMF_VIDEO_CONVERTER_COLOR_PROFILE       L"ColorProfile"

enum AMF_VIDEO_CONVERTER_COLOR_PROFILE_ENUM
{
    AMF_VIDEO_CONVERTER_COLOR_PROFILE_UNKNOWN = -1,
    AMF_VIDEO_CONVERTER_COLOR_PROFILE_601 = 0,
    AMF_VIDEO_CONVERTER_COLOR_PROFILE_709 = 1,
    AMF_VIDEO_CONVERTER_COLOR_PROFILE_COUNT
};
#endif //#ifndef __AMFVideoConverter_h__
