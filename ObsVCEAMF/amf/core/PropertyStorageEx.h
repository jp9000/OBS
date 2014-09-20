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
 * @file  PropertyStorageEx.h
 * @brief AMFPropertyStorageEx interface declaration
 ***************************************************************************************************
 */

#ifndef __AMFPropertyStorageEx_h__
#define __AMFPropertyStorageEx_h__
#pragma once

#include "PropertyStorage.h"

namespace amf
{
    //----------------------------------------------------------------------------------------------
    enum AMF_PROPERTY_ACCESS_TYPE
    {
        AMF_PROPERTY_ACCESS_PRIVATE = 0,
        AMF_PROPERTY_ACCESS_READ = 0x1,
        AMF_PROPERTY_ACCESS_WRITE = 0x2,
        AMF_PROPERTY_ACCESS_READ_WRITE = (AMF_PROPERTY_ACCESS_READ | AMF_PROPERTY_ACCESS_WRITE),
        AMF_PROPERTY_ACCESS_WRITE_RUNTIME = 0x4,
        AMF_PROPERTY_ACCESS_FULL = 0xFF,
    };
    //----------------------------------------------------------------------------------------------
    struct AMFEnumDescriptionEntry
    {
        amf_int             value;
        const wchar_t*      name;
    };
    //----------------------------------------------------------------------------------------------
    typedef amf_uint32 AMF_PROPERTY_CONTENT_TYPE;

    struct AMFPropertyInfo
    {
        const wchar_t*                  name;
        const wchar_t*                  desc;
        AMF_VARIANT_TYPE                type;
        AMF_PROPERTY_CONTENT_TYPE       contentType;

        AMFVariantStruct                defaultValue;
        AMFVariantStruct                minValue;
        AMFVariantStruct                maxValue;
        AMF_PROPERTY_ACCESS_TYPE        accessType;
        const AMFEnumDescriptionEntry*  pEnumDescription;

        AMFPropertyInfo() :
            name(NULL),
            desc(NULL),
            type(),
            contentType(),
            defaultValue(),
            minValue(),
            maxValue(),
            accessType(AMF_PROPERTY_ACCESS_FULL),
            pEnumDescription(NULL)
        {}


        bool AllowedRead() const
        {
            return (accessType & AMF_PROPERTY_ACCESS_READ) != 0;
        }
        bool AllowedWrite() const
        {
            return (accessType & AMF_PROPERTY_ACCESS_WRITE) != 0;
        }
        bool AllowedChangeInRuntime() const
        {
            return (accessType & AMF_PROPERTY_ACCESS_WRITE_RUNTIME) != 0;
        }
        virtual ~AMFPropertyInfo(){}

        AMFPropertyInfo(const AMFPropertyInfo& propery) : name(propery.name),
            desc(propery.desc),
            type(propery.type),
            contentType(propery.contentType),
            defaultValue(propery.defaultValue),
            minValue(propery.minValue),
            maxValue(propery.maxValue),
            accessType(propery.accessType),
            pEnumDescription(propery.pEnumDescription)
        {}

        AMFPropertyInfo& operator=(const AMFPropertyInfo& propery)
        {
            desc = propery.desc;
            type = propery.type;
            contentType = propery.contentType;
            defaultValue = propery.defaultValue;
            minValue = propery.minValue;
            maxValue = propery.maxValue;
            accessType = propery.accessType;
            pEnumDescription = propery.pEnumDescription;

            return *this;
        }
    };
    //----------------------------------------------------------------------------------------------
    // AMFPropertyStorageEx interface
    //----------------------------------------------------------------------------------------------
    class AMF_CORE_LINK AMFPropertyStorageEx : virtual public AMFPropertyStorage
    {
    public:
        virtual ~AMFPropertyStorageEx() {}
        AMF_DECLARE_IID(0x16b8958d, 0xe943, 0x4a33, 0xa3, 0x5a, 0x88, 0x5a, 0xd8, 0x28, 0xf2, 0x67)

        virtual amf_size            AMF_STD_CALL    GetPropertiesInfoCount() const = 0;
        virtual AMF_RESULT          AMF_STD_CALL    GetPropertyInfo(amf_size ind, const AMFPropertyInfo** ppInfo) const = 0;
        virtual AMF_RESULT          AMF_STD_CALL    GetPropertyInfo(const wchar_t* name, const AMFPropertyInfo** ppInfo) const = 0;
        virtual AMF_RESULT          AMF_STD_CALL    ValidateProperty(const wchar_t* name, AMFVariantStruct value, AMFVariantStruct* pOutValidated) const = 0;
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFPropertyStorageEx> AMFPropertyStorageExPtr;
} //namespace amf


#endif //#ifndef __AMFPropertyStorageEx_h__
