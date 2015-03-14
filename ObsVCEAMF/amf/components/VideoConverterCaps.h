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
 * @file  VideoConverterCaps.h
 * @brief AMFVideoConverterCaps class declaration
 ***************************************************************************************************
 */
 
#pragma once

#include "ComponentCaps.h"

namespace amf
{
    /*  Video Converter Capabilities Interface
    */

    class AMFVideoConverterCaps : public virtual AMFCaps
    {
    public:
        AMF_DECLARE_IID(0xc2cf7c7d, 0x5c9a, 0x4060, 0xbc, 0x67, 0xef, 0xed, 0x10, 0xb1, 0xcb, 0x47)
    };

    typedef AMFInterfacePtr_T<AMFVideoConverterCaps>   AMFVideoConverterCapsPtr;
}

