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
 * @file  ComponentCaps.h
 * @brief AMF Component Capability class declaration
 ***************************************************************************************************
 */
 
#pragma once

#include "Interface.h"
#include "Surface.h"

namespace amf
{
    enum AMF_ACCELERATION_TYPE
    {
        AMF_ACCEL_NOT_SUPPORTED = -1,
        AMF_ACCEL_HARDWARE,
        AMF_ACCEL_GPU,
        AMF_ACCEL_SOFTWARE
    };

    class AMFIOCaps : public virtual AMFInterface
    {
    public:
        //  Get supported resolution ranges in pixels/lines:
        virtual void AMF_STD_CALL GetWidthRange(amf_int32* minWidth, amf_int32* maxWidth) const = 0;
        virtual void AMF_STD_CALL GetHeightRange(amf_int32* minHeight, amf_int32* maxHeight) const = 0;

        //  Get memory alignment in lines:
        //  Vertical aligmnent should be multiples of this number
        virtual amf_int32 AMF_STD_CALL GetVertAlign() const = 0;
        
        //  Enumerate supported surface pixel formats:
        virtual amf_int32 AMF_STD_CALL GetNumOfFormats() const = 0;
        virtual  AMF_RESULT AMF_STD_CALL GetFormatAt(amf_int32 index, AMF_SURFACE_FORMAT* format, amf_bool* native) const = 0;

        //  Enumerate supported surface formats:
        virtual amf_int32 AMF_STD_CALL GetNumOfMemoryTypes() const = 0;
        virtual AMF_RESULT AMF_STD_CALL GetMemoryTypeAt(amf_int32 index, AMF_MEMORY_TYPE* memType, amf_bool* native) const = 0;
        
        //  interlaced support:
        virtual amf_bool AMF_STD_CALL IsInterlacedSupported() const = 0;
    };
    typedef AMFInterfacePtr_T<AMFIOCaps>    AMFIOCapsPtr;
    
    
    /*  This is a base interface for every h/w module supported by Capability Manager
        Every module-specific interface must extend this interface 
    */
    class AMFCaps : public virtual AMFInterface
    {
    public:
        //  Get acceleration type of a component:
        virtual AMF_ACCELERATION_TYPE AMF_STD_CALL GetAccelerationType() const = 0;

        //  Get input capabilities of a component:
        virtual AMF_RESULT AMF_STD_CALL GetInputCaps(AMFIOCaps** input) = 0;

        //  Get output capabilities of a component:
        virtual AMF_RESULT AMF_STD_CALL GetOutputCaps(AMFIOCaps** output) = 0;
    };
    typedef AMFInterfacePtr_T<AMFCaps>  AMFCapsPtr;
}
