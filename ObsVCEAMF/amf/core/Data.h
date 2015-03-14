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
 * @file  Data.h
 * @brief AMFData interface declaration
 ***************************************************************************************************
 */

#ifndef __AMFData_h__
#define __AMFData_h__
#pragma once

#include "PropertyStorage.h"

namespace amf
{
    //----------------------------------------------------------------------------------------------
    enum AMF_DATA_TYPE
    {
        AMF_DATA_BUFFER = 0,
        AMF_DATA_SURFACE = 1,
        AMF_DATA_AUDIO_BUFFER = 2,
        AMF_DATA_USER = 1000,
        // all extensions will be AMF_DATA_USER+i
    };
    //----------------------------------------------------------------------------------------------
    enum AMF_MEMORY_TYPE
    {
        AMF_MEMORY_UNKNOWN = 0,
        AMF_MEMORY_HOST = 1,
        AMF_MEMORY_DX9 = 2,
        AMF_MEMORY_DX11 = 3,
        AMF_MEMORY_OPENCL = 4,
        AMF_MEMORY_OPENGL = 5,
        AMF_MEMORY_XV = 6,
        AMF_MEMORY_GRALLOC = 7,
    };

    AMF_CORE_LINK const wchar_t* const  AMF_STD_CALL AMFGetMemoryTypeName(const AMF_MEMORY_TYPE memoryType);
    AMF_CORE_LINK AMF_MEMORY_TYPE       AMF_STD_CALL AMFGetMemoryTypeByName(const wchar_t* name);


    //----------------------------------------------------------------------------------------------
    enum AMF_DX_VERSION
    {
        AMF_DX9 = 90,
        AMF_DX9_EX = 91,
        AMF_DX11_0 = 110,
        AMF_DX11_1 = 111 
    };
    //----------------------------------------------------------------------------------------------
    // generic data interface
    //----------------------------------------------------------------------------------------------
    class AMFData : virtual public AMFPropertyStorage
    {
    public:
        AMF_DECLARE_IID(0xa1159bf6, 0x9104, 0x4107, 0x8e, 0xaa, 0xc5, 0x3d, 0x5d, 0xba, 0xc5, 0x11)
        virtual ~AMFData(){}

        virtual AMF_MEMORY_TYPE     AMF_STD_CALL    GetMemoryType() = 0;
        virtual AMF_RESULT          AMF_STD_CALL    Duplicate(AMF_MEMORY_TYPE type, AMFData** ppData) = 0;
        virtual AMF_RESULT          AMF_STD_CALL    Convert(AMF_MEMORY_TYPE type) = 0;
        virtual AMF_DATA_TYPE       AMF_STD_CALL    GetDataType() = 0;

        virtual bool                AMF_STD_CALL    IsReusable() = 0;

        virtual void                AMF_STD_CALL    SetPts(amf_pts pts) = 0;
        virtual amf_pts             AMF_STD_CALL    GetPts() = 0;
        virtual void                AMF_STD_CALL    SetDuration(amf_pts duration) = 0;
        virtual amf_pts             AMF_STD_CALL    GetDuration() = 0;
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFData> AMFDataPtr;
} // namespace

#endif //#ifndef __AMFData_h__
