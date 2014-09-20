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
 * @file  Surface.h
 * @brief AMFSurface interface declaration
 ***************************************************************************************************
 */

#ifndef __AMFSurface_h__
#define __AMFSurface_h__
#pragma once

#include "Data.h"
#include "Plane.h"

namespace amf
{
    enum AMF_SURFACE_FORMAT
    {
        AMF_SURFACE_UNKNOWN     = 0,
        AMF_SURFACE_NV12,               ///< 1 - planar Y width x height + packed UV width/2 x height/2 - 8 bit per component
        AMF_SURFACE_YV12,               ///< 2 - planar Y width x height + V width/2 x height/2 + U width/2 x height/2 - 8 bit per component
        AMF_SURFACE_BGRA,               ///< 3 - packed - 8 bit per component
        AMF_SURFACE_ARGB,               ///< 4 - packed - 8 bit per component
        AMF_SURFACE_RGBA,               ///< 5 - packed - 8 bit per component
        AMF_SURFACE_GRAY8,              ///< 6 - single component - 8 bit
        AMF_SURFACE_YUV420P,            ///< 7 - planar Y width x height + U width/2 x height/2 + V width/2 x height/2 - 8 bit per component
        AMF_SURFACE_U8V8,               ///< 8 - double component - 8 bit per component
        AMF_SURFACE_YUY2,               ///< 9 - YUY2: Byte 0=8-bit Y'0; Byte 1=8-bit Cb; Byte 2=8-bit Y'1; Byte 3=8-bit Cr

        AMF_SURFACE_FIRST = AMF_SURFACE_NV12,
        AMF_SURFACE_LAST = AMF_SURFACE_YUY2
    };

    AMF_CORE_LINK const wchar_t*        AMF_STD_CALL AMFSurfaceGetFormatName(const AMF_SURFACE_FORMAT eSurfaceFormat);
    AMF_CORE_LINK AMF_SURFACE_FORMAT    AMF_STD_CALL AMFSurfaceGetFormatByName(const wchar_t* name);

    //----------------------------------------------------------------------------------------------
    // frame type
    //----------------------------------------------------------------------------------------------
    enum AMF_FRAME_TYPE
    {
        // flags
        AMF_FRAME_STEREO_FLAG = 0x10000000,
        AMF_FRAME_LEFT_FLAG = AMF_FRAME_STEREO_FLAG | 0x20000000,
        AMF_FRAME_RIGHT_FLAG = AMF_FRAME_STEREO_FLAG | 0x40000000,
        AMF_FRAME_BOTH_FLAG = AMF_FRAME_LEFT_FLAG | AMF_FRAME_RIGHT_FLAG,
        AMF_FRAME_INTERLEAVED_FLAG = 0x01000000,
        AMF_FRAME_FIELD_FLAG = 0x02000000,
        AMF_FRAME_EVEN_FLAG = 0x04000000,
        AMF_FRAME_ODD_FLAG = 0x08000000,
        // values

        AMF_FRAME_UNKNOWN = -1,

        AMF_FRAME_PROGRESSIVE = 0,

        AMF_FRAME_INTERLEAVED_EVEN_FIRST = AMF_FRAME_INTERLEAVED_FLAG | AMF_FRAME_EVEN_FLAG,
        AMF_FRAME_INTERLEAVED_ODD_FIRST = AMF_FRAME_INTERLEAVED_FLAG | AMF_FRAME_ODD_FLAG,
        AMF_FRAME_FIELD_SINGLE_EVEN = AMF_FRAME_FIELD_FLAG | AMF_FRAME_EVEN_FLAG,
        AMF_FRAME_FIELD_SINGLE_ODD = AMF_FRAME_FIELD_FLAG | AMF_FRAME_ODD_FLAG,

        AMF_FRAME_STEREO_LEFT = AMF_FRAME_LEFT_FLAG,
        AMF_FRAME_STEREO_RIGHT = AMF_FRAME_RIGHT_FLAG,
        AMF_FRAME_STEREO_BOTH = AMF_FRAME_BOTH_FLAG,

        AMF_FRAME_INTERLEAVED_EVEN_FIRST_STEREO_LEFT = AMF_FRAME_INTERLEAVED_FLAG |
            AMF_FRAME_EVEN_FLAG | AMF_FRAME_LEFT_FLAG,
        AMF_FRAME_INTERLEAVED_EVEN_FIRST_STEREO_RIGHT = AMF_FRAME_INTERLEAVED_FLAG |
            AMF_FRAME_EVEN_FLAG | AMF_FRAME_RIGHT_FLAG,
        AMF_FRAME_INTERLEAVED_EVEN_FIRST_STEREO_BOTH = AMF_FRAME_INTERLEAVED_FLAG |
            AMF_FRAME_EVEN_FLAG | AMF_FRAME_BOTH_FLAG,

        AMF_FRAME_INTERLEAVED_ODD_FIRST_STEREO_LEFT = AMF_FRAME_INTERLEAVED_FLAG |
            AMF_FRAME_ODD_FLAG | AMF_FRAME_LEFT_FLAG,
        AMF_FRAME_INTERLEAVED_ODD_FIRST_STEREO_RIGHT = AMF_FRAME_INTERLEAVED_FLAG |
            AMF_FRAME_ODD_FLAG | AMF_FRAME_RIGHT_FLAG,
        AMF_FRAME_INTERLEAVED_ODD_FIRST_STEREO_BOTH = AMF_FRAME_INTERLEAVED_FLAG |
            AMF_FRAME_ODD_FLAG | AMF_FRAME_BOTH_FLAG,
    };

    /**
     ********************************************************************************
     * @brief Interface to get callbacks from AMFSurface instances
     ********************************************************************************
     */
    class AMFSurface;
    class AMFSurfaceObserver
    {
    public:
        /**
         ********************************************************************************
         * @brief ReleaseResources is called before internal release resources.
         * The callback is called once, automatically removed after call.
         * those are done in specific Surface ReleasePlanes
         ********************************************************************************
         */
        virtual void AMF_STD_CALL OnSurfaceDataRelease(AMFSurface* pSurface) = 0;
        virtual ~AMFSurfaceObserver() {}
    };

    class AMFSurface : virtual public AMFData
    {
    public:
        AMF_DECLARE_IID(0x3075dbe3, 0x8718, 0x4cfa, 0x86, 0xfb, 0x21, 0x14, 0xc0, 0xa5, 0xa4, 0x51)
        virtual ~AMFSurface(){}

        virtual AMF_SURFACE_FORMAT  AMF_STD_CALL    GetFormat() = 0;
        virtual amf_size            AMF_STD_CALL    GetPlanesCount() = 0;

        // do not store planes outside. shoul be used together with Surface
        virtual AMFPlane*           AMF_STD_CALL    GetPlaneAt(amf_size index) = 0;
        virtual AMFPlane*           AMF_STD_CALL    GetPlane(AMF_PLANE_TYPE type) = 0;

        virtual AMF_FRAME_TYPE      AMF_STD_CALL    GetFrameType() = 0;
        virtual void                AMF_STD_CALL    SetFrameType(AMF_FRAME_TYPE type) = 0;

        virtual AMF_RESULT          AMF_STD_CALL    SetCrop(amf_int32 x,amf_int32 y, amf_int32 width, amf_int32 height) = 0;
        //AMFPropertyStorage observers
        virtual void                AMF_STD_CALL    AddObserver(AMFPropertyStorageObserver* pObserver) = 0;
        virtual void                AMF_STD_CALL    RemoveObserver(AMFPropertyStorageObserver* pObserver) = 0;

        //AMFSurface observers
        virtual void                AMF_STD_CALL    AddObserver(AMFSurfaceObserver* pObserver) = 0;
        virtual void                AMF_STD_CALL    RemoveObserver(AMFSurfaceObserver* pObserver) = 0;
    };
    typedef AMFInterfacePtr_T<AMFSurface> AMFSurfacePtr;
}

#endif //#ifndef __AMFSurface_h__
