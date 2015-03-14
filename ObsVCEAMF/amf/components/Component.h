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
 * @file  Component.h
 * @brief AMFComponent interface declaration
 ***************************************************************************************************
 */
#ifndef __AMFComponent_h__
#define __AMFComponent_h__
#pragma once

#include "../core/Data.h"
#include "../core/PropertyStorageEx.h"
#include "../core/Surface.h"
#include "../core/Context.h"

namespace amf
{
    //----------------------------------------------------------------------------------------------
    // AMFDataAllocatorCB interface
    //----------------------------------------------------------------------------------------------
    class AMFDataAllocatorCB : virtual public AMFInterface
    {
    public:
        AMF_DECLARE_IID(0x4bf46198, 0x8b7b, 0x49d0, 0xaa, 0x72, 0x48, 0xd4, 0x7, 0xce, 0x24, 0xc5 )
        virtual ~AMFDataAllocatorCB(){}
        virtual AMF_RESULT AMF_STD_CALL AllocBuffer(AMF_MEMORY_TYPE type, amf_size size, AMFBuffer** ppBuffer) = 0;
        virtual AMF_RESULT AMF_STD_CALL AllocSurface(AMF_MEMORY_TYPE type, AMF_SURFACE_FORMAT format,
            amf_int32 width, amf_int32 height, amf_int32 hPitch, amf_int32 vPitch, AMFSurface** ppSurface) = 0;
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFDataAllocatorCB> AMFDataAllocatorCBPtr;
    //----------------------------------------------------------------------------------------------

    //----------------------------------------------------------------------------------------------
    // AMFComponent interface
    //----------------------------------------------------------------------------------------------
    class AMFComponent : virtual public AMFPropertyStorageEx
    {
    public:
        AMF_DECLARE_IID(0x8b51e5e4, 0x455d, 0x4034, 0xa7, 0x46, 0xde, 0x1b, 0xed, 0xc3, 0xc4, 0x6)

        virtual ~AMFComponent(){}

        virtual AMF_RESULT  AMF_STD_CALL Init(AMF_SURFACE_FORMAT format,amf_int32 width,amf_int32 height) = 0;
        virtual AMF_RESULT  AMF_STD_CALL ReInit(amf_int32 width,amf_int32 height) = 0;
        virtual AMF_RESULT  AMF_STD_CALL Terminate() = 0;
        virtual AMF_RESULT  AMF_STD_CALL Drain() = 0;
        virtual AMF_RESULT  AMF_STD_CALL Flush() = 0;

        virtual AMF_RESULT  AMF_STD_CALL SubmitInput(AMFData* pData) = 0;
        virtual AMF_RESULT  AMF_STD_CALL QueryOutput(AMFData** ppData) = 0;
        virtual AMFContext* AMF_STD_CALL GetContext() = 0;
        virtual AMF_RESULT  AMF_STD_CALL SetOutputDataAllocatorCB(AMFDataAllocatorCB* callback) = 0;
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFComponent> AMFComponentPtr;
} // namespace

extern "C"
{
    // Creates component of specified id on specified context
    AMF_CORE_LINK AMF_RESULT AMF_CDECL_CALL AMFCreateComponent(amf::AMFContext* pContext,
            const wchar_t* id,
            amf::AMFComponent** ppComponent);
}

#endif //#ifndef __AMFComponent_h__
