///-------------------------------------------------------------------------
///  Trade secret of Advanced Micro Devices, Inc.
///  Copyright 2013, Advanced Micro Devices, Inc., (unpublished)
///
///  All rights reserved.  This notice is intended as a precaution against
///  inadvertent publication and does not imply publication or any waiver
///  of confidentiality.  The year included in the foregoing notice is the
///  year of creation of the work.
///-------------------------------------------------------------------------
///  @file   PropertyStorageEx.h
///  @brief  AMFPropertyStorageEx interface
///-------------------------------------------------------------------------
#ifndef __AMFPropertyStorageEx_h__
#define __AMFPropertyStorageEx_h__
#pragma once

#include "PropertyStorage.h"

namespace amf
{
    enum AMF_PROPERTY_CONTENT_TYPE
    {
        AMF_PROPERTY_CONTENT_DEFAULT=0,
        AMF_PROPERTY_CONTENT_XML,           // m_eType is AMF_VT_STRING

        AMF_PROPERTY_CONTENT_FILE_OPEN_PATH,// m_eType AMF_VT_WSTRING
        AMF_PROPERTY_CONTENT_FILE_SAVE_PATH,// m_eType AMF_VT_WSTRING
        AMF_PROPERTY_CONTENT_ENUM,          // m_eType AMF_VT_INT64

        AMF_PROPERTY_CONTENT_PRIVATE,       // for internal usage only
                                            // private property must be skpped in common functionality
                                            // like UI, serialization etc
    };

    struct AMFPropertyInfo
    {
        const wchar_t*                  name;
        const wchar_t*                  desc;
        AMF_VARIANT_TYPE                type;
        AMF_PROPERTY_CONTENT_TYPE       contentType;

        AMFVARIANT                      defaultValue;
        AMFVARIANT                      minValue;
        AMFVARIANT                      maxValue;

        bool                            allowChangeInRuntime;
        const AMFEnumDescriptionEntry*  pEnumDescription;
    };
    
    struct AMFReplaceProperty
    {
        // TODO: consider dynamic allocation of output string
        virtual const wchar_t* findPropertyReplacement(const wchar_t* primitiveName, const wchar_t* propertyName) = 0;
    };
    //----------------------------------------------------------------------------------------------
    // AMFPropertyStorageEx interface
    //----------------------------------------------------------------------------------------------
    class AMF_CORE_LINK AMFPropertyStorageEx : virtual public AMFPropertyStorage
    {
    public:
        virtual ~AMFPropertyStorageEx() {}
        AMF_DECLARE_IID("AMFPropertyStorageEx")

        virtual amf_size            AMF_STD_CALL GetPropertiesInfoCount() = 0;
        virtual AMF_ERROR           AMF_STD_CALL GetPropertyInfo(amf_size ind, const AMFPropertyInfo** ppInfo) = 0;
        virtual AMF_ERROR           AMF_STD_CALL GetPropertyInfo(const wchar_t* name, const AMFPropertyInfo** ppInfo) = 0;
        virtual AMF_ERROR           AMF_STD_CALL ValidateProperty(const wchar_t* name, AMFVARIANT value, AMFVARIANT* pOutValidated) = 0;

    };

    typedef AMFRefPtr<AMFPropertyStorageEx> AMFPropertyStorageExPtr; 
}//namespace amf


#endif //#ifndef __AMFPropertyStorageEx_h__
