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
 * @file  Buffer.h
 * @brief AMFBuffer interface declaration
 ***************************************************************************************************
 */

#ifndef __AMFBuffer_h__
#define __AMFBuffer_h__
#pragma once

#include "Data.h"

namespace amf
{
    /**
     ********************************************************************************
     * @brief Interface to get callbacks from AMFBufferObserver instances
     ********************************************************************************
     */
    class AMFBuffer;
    class AMFBufferObserver
    {
    public:
        /**
         ********************************************************************************
         * @brief Destroy is called before internal release resources if
         * those are done in specific Surface ReleasePlanes
         ********************************************************************************
         */
        virtual void AMF_STD_CALL OnBufferDataRelease(AMFBuffer* pBuffer) = 0;
        virtual ~AMFBufferObserver() {}
    };


    //----------------------------------------------------------------------------------------------
    // AMFBuffer interface
    //----------------------------------------------------------------------------------------------
    class AMFBuffer : virtual public AMFData
    {
    public:
        AMF_DECLARE_IID(0xb04b7248, 0xb6f0, 0x4321, 0xb6, 0x91, 0xba, 0xa4, 0x74, 0xf, 0x9f, 0xcb)

        virtual ~AMFBuffer() {}

        virtual amf_size            AMF_STD_CALL    GetSize() = 0;
        virtual void*               AMF_STD_CALL    GetNative() = 0;

        //AMFPropertyStorage observers
        virtual void                AMF_STD_CALL    AddObserver(AMFPropertyStorageObserver* pObserver) = 0;
        virtual void                AMF_STD_CALL    RemoveObserver(AMFPropertyStorageObserver* pObserver) = 0;
        //AMFBuffer observers
        virtual void                AMF_STD_CALL    AddObserver(AMFBufferObserver* pObserver) = 0;
        virtual void                AMF_STD_CALL    RemoveObserver(AMFBufferObserver* pObserver) = 0;
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFBuffer> AMFBufferPtr;
} // namespace

#endif //#ifndef __AMFBuffer_h__
