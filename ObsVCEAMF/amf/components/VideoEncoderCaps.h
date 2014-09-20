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
 * @file  VideoEncoderCaps.h
 * @brief AMFEncoderCaps class declaration
 ***************************************************************************************************
 */
 
#pragma once

#include "ComponentCaps.h"

namespace amf
{
    /*  Encoder Capabilities Interface
        All codec-specific encoder caps interface should be derived from it.
    */
    class AMFEncoderCaps : public virtual AMFCaps
    {
    public:
        AMF_DECLARE_IID(0x92ea1672, 0x7311, 0x46e6, 0xa6, 0x2, 0x93, 0xa9, 0xc3, 0x6c, 0x6e, 0x69)

        //  Get maximum supported bitrate:
        virtual amf_uint32 GetMaxBitrate() const = 0;
        virtual amf_int32 AMF_STD_CALL GetMaxNumOfStreams() const = 0;

    };
    typedef AMFInterfacePtr_T<AMFEncoderCaps>   AMFEncoderCapsPtr;
}
