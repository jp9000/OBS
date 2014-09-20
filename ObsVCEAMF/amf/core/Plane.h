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
 * @file  Plane.h
 * @brief AMFPlane interface declaration
 ***************************************************************************************************
 */

#ifndef __AMFPlane_h__
#define __AMFPlane_h__
#pragma once

#include "Buffer.h"

namespace amf
{
    //---------------------------------------------------------------------------------------------
    enum AMF_PLANE_TYPE
    {
        AMF_PLANE_UNKNOWN = 0,
        AMF_PLANE_PACKED = 1,             // for all packed formats: BGRA, YUY2
        AMF_PLANE_Y = 2,
        AMF_PLANE_UV = 3,
        AMF_PLANE_U = 4,
        AMF_PLANE_V = 5,
    };
    //---------------------------------------------------------------------------------------------
    class AMFPlane : virtual public AMFInterface
    {
    public:
        AMF_DECLARE_IID(0xbede1aa6, 0xd8fa, 0x4625, 0x94, 0x65, 0x6c, 0x82, 0xc4, 0x37, 0x71, 0x2e)

        virtual ~AMFPlane(){}

        virtual AMF_PLANE_TYPE      AMF_STD_CALL    GetType() = 0;
        virtual void*               AMF_STD_CALL    GetNative() = 0;
        virtual amf_int32           AMF_STD_CALL    GetPixelSizeInBytes() = 0;
        virtual amf_int32           AMF_STD_CALL    GetOffsetX() = 0;
        virtual amf_int32           AMF_STD_CALL    GetOffsetY() = 0;
        virtual amf_int32           AMF_STD_CALL    GetWidth() = 0;
        virtual amf_int32           AMF_STD_CALL    GetHeight() = 0;
        virtual amf_int32           AMF_STD_CALL    GetHPitch() = 0;
        virtual amf_int32           AMF_STD_CALL    GetVPitch() = 0;
    };
    typedef AMFInterfacePtr_T<AMFPlane> AMFPlanePtr;
} // namespace amf

#endif //#ifndef __AMFPlane_h__
