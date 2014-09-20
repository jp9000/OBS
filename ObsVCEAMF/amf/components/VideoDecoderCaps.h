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
 * @file  VideoDecoderCaps.h
 * @brief VideoDecoderCaps class declaration
 ***************************************************************************************************
 */

#pragma once

#include "ComponentCaps.h"

namespace amf
{
    /*  Decoder Codec Capabilities Interface
    */

    class AMFDecoderCaps : public virtual AMFCaps
    {
    public:
        AMF_DECLARE_IID(0x367247f4, 0x25a0, 0x47d1,  0x9d, 0x5b, 0x90, 0x1, 0x78, 0x97, 0xea, 0xdc)

        virtual amf_int32 AMF_STD_CALL GetMaxNumOfStreams() const = 0;
    };

    typedef AMFInterfacePtr_T<AMFDecoderCaps>   AMFDecoderCapsPtr;
}

