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
 * @file  Guid.h
 * @brief Guid class declaration
 ***************************************************************************************************
 */

#ifndef __AMFGuid_h__
#define __AMFGuid_h__
#pragma once

#include "Platform.h"

namespace amf
{
    struct AMFGuid
    {
        AMFGuid(amf_uint32 _data1, amf_uint16 _data2, amf_uint16 _data3,
                amf_uint8 _data41, amf_uint8 _data42, amf_uint8 _data43, amf_uint8 _data44,
                amf_uint8 _data45, amf_uint8 _data46, amf_uint8 _data47, amf_uint8 _data48)
            : data1 (_data1),
            data2 (_data2),
            data3 (_data3),
            data41(_data41),
            data42(_data42),
            data43(_data43),
            data44(_data44),
            data45(_data45),
            data46(_data46),
            data47(_data47),
            data48(_data48)
        {}
        amf_uint32 data1;
        amf_uint16 data2;
        amf_uint16 data3;
        amf_uint8 data41;
        amf_uint8 data42;
        amf_uint8 data43;
        amf_uint8 data44;
        amf_uint8 data45;
        amf_uint8 data46;
        amf_uint8 data47;
        amf_uint8 data48;
    };

    AMF_INLINE bool AMFCompareGUIDs(const AMFGuid& guid1, const AMFGuid& guid2)
    {
        return
            guid1.data1 == guid2.data1 &&
            guid1.data2 == guid2.data2 &&
            guid1.data3 == guid2.data3 &&
            guid1.data41 == guid2.data41 &&
            guid1.data42 == guid2.data42 &&
            guid1.data43 == guid2.data43 &&
            guid1.data44 == guid2.data44 &&
            guid1.data45 == guid2.data45 &&
            guid1.data46 == guid2.data46 &&
            guid1.data47 == guid2.data47 &&
            guid1.data48 == guid2.data48;
    }
}

#endif //#ifndef __AMFGuid_h__
