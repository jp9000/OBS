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
 * @file  VideoEncoderVCECaps.h
 * @brief AMFH264EncoderCaps class declaration
 ***************************************************************************************************
 */
 
#pragma once

#include "VideoEncoderCaps.h"
#include "VideoEncoderVCE.h"

namespace amf
{
    //  Job priority:
    enum H264EncoderJobPriority
    {
        AMF_H264_ENCODER_JOB_PRIORITY_NONE,
        AMF_H264_ENCODER_JOB_PRIORITY_LEVEL1,
        AMF_H264_ENCODER_JOB_PRIORITY_LEVEL2
    };

    //  H.264 Capabilities:
    class AMFH264EncoderCaps : public AMFEncoderCaps
    {
    public:
        AMF_DECLARE_IID(0x9e2b640c, 0xf5d6, 0x426e,  0x94, 0x9e, 0x2f, 0xae, 0x7a, 0x58, 0x5e, 0xd5)

        virtual amf_int32 GetNumOfSupportedProfiles() const = 0;
        virtual AMF_VIDEO_ENCODER_PROFILE_ENUM GetProfile(amf_int32 index) const = 0;

        //  Levels:
        virtual amf_int32 GetNumOfSupportedLevels() const = 0;
        virtual amf_uint32 GetLevel(amf_int32 index) const = 0;

        //  Rate control methods:
        virtual amf_int32 GetNumOfRateControlMethods() const = 0;
        virtual AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_ENUM GetRateControlMethod(amf_int32 index) const = 0;

        virtual H264EncoderJobPriority GetMaxSupportedJobPriority() const = 0;

        //  B-picture support:
        virtual amf_bool IsBPictureSupported() const = 0;

        //  Reference frames:
        virtual void GetNumOfReferenceFrames(amf_uint32* minNum, amf_uint32* maxNum) const = 0;

        //  3D support:
        virtual amf_bool CanOutput3D() const = 0;

        //  Temporal enhancement layers:
        virtual amf_int32 GetMaxNumOfTemporalLayers() const = 0;
    };

    typedef AMFInterfacePtr_T<AMFH264EncoderCaps>  H264EncoderCapsPtr;
}
