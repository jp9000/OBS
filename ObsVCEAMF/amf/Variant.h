///-------------------------------------------------------------------------
///  Trade secret of Advanced Micro Devices, Inc.
///  Copyright 2013, Advanced Micro Devices, Inc., (unpublished)
///
///  All rights reserved.  This notice is intended as a precaution against
///  inadvertent publication and does not imply publication or any waiver
///  of confidentiality.  The year included in the foregoing notice is the
///  year of creation of the work.
///-------------------------------------------------------------------------
///  @file   Variant.h
///  @brief  Variant API
///-------------------------------------------------------------------------
#ifndef __AMFVariant_h__
#define __AMFVariant_h__
#pragma once

#include "Typedefs.h"

////////////////////////////////////////////////////////////////////////////////////
namespace amf
{
    enum AMF_VARIANT_TYPE
    {
        AMF_VT_EMPTY,
        
        AMF_VT_BOOL,
        AMF_VT_INT64,
        AMF_VT_DOUBLE,

        AMF_VT_STRING,   // m_Value is char* 
        AMF_VT_WSTRING,  // m_Value is wchar* 
        AMF_VT_INTERFACE,
    };
    
    struct AMFVARIANT
    {
        AMF_VARIANT_TYPE type;
        union
        {
            amf_bool        boolValue;
            amf_int64       int64Value;
            amf_double      doubleValue;
            char*           stringValue;
            wchar_t*        wstringValue;
            AMFInterface*   pInterface;
        };
    };

    AMF_CORE_LINK AMF_ERROR    AMF_STD_CALL AMFVariantInit(AMFVARIANT* pVariant);
    AMF_CORE_LINK AMF_ERROR    AMF_STD_CALL AMFVariantClear(AMFVARIANT* pVariant);
    AMF_CORE_LINK AMF_ERROR    AMF_STD_CALL AMFVariantCompare(const AMFVARIANT* pFirst, const AMFVARIANT* pSecond, bool& equal);
    AMF_CORE_LINK AMF_ERROR    AMF_STD_CALL AMFVariantCopy(AMFVARIANT* pDest, const AMFVARIANT* pSrc);
    AMF_CORE_LINK AMF_ERROR    AMF_STD_CALL AMFVariantChangeType(AMFVARIANT* pDest, const AMFVARIANT* pSrc, AMF_VARIANT_TYPE newType);

    AMF_CORE_LINK AMF_ERROR    AMF_STD_CALL AMFVariantAssignBool(AMFVARIANT* pDest, bool value);
    AMF_CORE_LINK AMF_ERROR    AMF_STD_CALL AMFVariantAssignInt64(AMFVARIANT* pDest, amf_int64 value);
    AMF_CORE_LINK AMF_ERROR    AMF_STD_CALL AMFVariantAssignDouble(AMFVARIANT* pDest, amf_double value);
    AMF_CORE_LINK AMF_ERROR    AMF_STD_CALL AMFVariantAssignString(AMFVARIANT* pDest, const char* value);
    AMF_CORE_LINK AMF_ERROR    AMF_STD_CALL AMFVariantAssignWString(AMFVARIANT* pDest, const wchar_t* value);
    AMF_CORE_LINK AMF_ERROR    AMF_STD_CALL AMFVariantAssignInterface(AMFVARIANT* pDest, AMFInterface* value);

    #define AMFVariantType(_variant) (_variant)->type
    #define AMFVariantBool(_variant) (_variant)->boolValue
    #define AMFVariantInt64(_variant) (_variant)->int64Value
    #define AMFVariantDouble(_variant) (_variant)->doubleValue
    #define AMFVariantString(_variant) (_variant)->stringValue
    #define AMFVariantWString(_variant) (_variant)->wstringValue
    #define AMFVariantInterface(_variant) (_variant)->pInterface
}


namespace amf
{
    class AMFVariant : public AMFVARIANT
    {
    public:
        AMFVariant();
        AMFVariant(const AMFVARIANT& other);
        AMFVariant(const AMFVARIANT* pOther);
        AMFVariant(const AMFVariant& other);
        AMFVariant(AMFVARIANT& other, bool copy); // attach if (bCopy == false)

        AMFVariant(amf_bool value);
        AMFVariant(amf_int64 value);
        AMFVariant(amf_uint64 value);
        AMFVariant(amf_int32 value);
        AMFVariant(amf_uint32 value);
        AMFVariant(amf_double value);
        AMFVariant(const char* value);
        AMFVariant(const wchar_t* value);
        AMFVariant(AMFInterface *value);
        template <typename T> AMFVariant(const AMFRefPtr<T>& pValue);

        ~AMFVariant();
 
        AMFVariant& operator=(const AMFVARIANT& other);
        AMFVariant& operator=(const AMFVARIANT* pOther);
        AMFVariant& operator=(const AMFVariant& other);

        AMFVariant& operator=(amf_bool value);
        AMFVariant& operator=(amf_int64 value);
        AMFVariant& operator=(amf_uint64 value);
        AMFVariant& operator=(amf_int32 value);
        AMFVariant& operator=(amf_uint32 value);
        AMFVariant& operator=(amf_double value);
        AMFVariant& operator=(const char* value);
        AMFVariant& operator=(const wchar_t* value);
        AMFVariant& operator=(AMFInterface* pValue);
        template <typename T> AMFVariant& operator=(const AMFRefPtr<T>& value);

        operator amf_bool() const;
        operator amf_int64() const;
        operator amf_uint64() const;
        operator amf_int32() const;
        operator amf_uint32() const;
        operator amf_float() const;
        operator amf_double() const;
#if AMF_BUILD_STL
        operator amf_string() const;
        operator amf_wstring() const;
#endif//#if AMF_BUILD_STL
        operator AMFInterface*() const;

        bool operator==(const AMFVARIANT& other) const;
        bool operator==(const AMFVARIANT* pOther) const;

        bool operator!=(const AMFVARIANT& other) const;
        bool operator!=(const AMFVARIANT* pOther) const;

        void Clear();

        void Attach(AMFVARIANT& variant);
        AMFVARIANT Detach();

        AMFVARIANT& GetVariant();

        void ChangeType(AMF_VARIANT_TYPE type, const AMFVariant* pSrc = NULL);

        bool Empty() const;
    };

    inline void AMF_CHECK_ERROR_TMP(AMF_ERROR err)
    {
        //TODO: ???
        //assert(err == AMF_OK);
		(void)err;
    }


    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::AMFVariant()
    {
        AMFVariantInit(this);
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::AMFVariant(const AMFVARIANT& other)
    {
        AMFVariantInit(this);
        AMF_CHECK_ERROR_TMP(AMFVariantCopy(this, const_cast<AMFVARIANT*>(&other)));
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::AMFVariant(const AMFVARIANT* pOther)
    {
        if (pOther == NULL)
        {
            AMF_CHECK_ERROR_TMP(AMF_NULL_POINTER);
        }
        else
        {
            AMFVariantInit(this);
            AMF_CHECK_ERROR_TMP(AMFVariantCopy(this, const_cast<AMFVARIANT*>(pOther)));
        }
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::AMFVariant(const AMFVariant& other)
    {
        AMFVariantInit(this);
        AMF_CHECK_ERROR_TMP(AMFVariantCopy(this, const_cast<AMFVARIANT*>(static_cast<const AMFVARIANT*>(&other))));
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::AMFVariant(AMFVARIANT& other, bool copy)
    {
        if(copy)
        {
            AMFVariantInit(this);
            AMF_CHECK_ERROR_TMP(AMFVariantCopy(this, &other));
        } 
        else
        {
            memcpy(this, &other, sizeof(other));
            AMFVariantType(this) = AMF_VT_EMPTY;
        }
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::AMFVariant(amf_bool value)
    {
        AMFVariantInit(this);
        AMFVariantAssignBool(this, value);
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::AMFVariant(amf_int64 value)
    {
        AMFVariantInit(this);
        AMFVariantAssignInt64(this, value);
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::AMFVariant(amf_uint64 value)
    {
        AMFVariantInit(this);
        AMFVariantAssignInt64(this, (amf_int64)value);
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::AMFVariant(amf_int32 value)
    {
        AMFVariantInit(this);
        AMFVariantAssignInt64(this, value);
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::AMFVariant(amf_uint32 value)
    {
        AMFVariantInit(this);
        AMFVariantAssignInt64(this, value);
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::AMFVariant(amf_double value)
    {
        AMFVariantInit(this);
        AMFVariantAssignDouble(this, value);
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::AMFVariant(const char* value)
    {
        AMFVariantInit(this);
        AMFVariantAssignString(this, value);
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::AMFVariant(const wchar_t* value)
    {
        AMFVariantInit(this);
        AMFVariantAssignWString(this, value);
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::AMFVariant(AMFInterface* pValue)
    {
        AMFVariantInit(this);
        AMFVariantAssignInterface(this, pValue);
    }
    //-------------------------------------------------------------------------------------------------
    template <typename T>
    AMFVariant::AMFVariant(const AMFRefPtr<T>& pValue)
    {
        AMFVariantInit(this);
        AMFVariantAssignInterface(this, pValue);
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::~AMFVariant()
    {
        AMFVariantClear(this);
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::operator amf_bool() const
    {
        if(AMFVariantType(this) == AMF_VT_BOOL)
        {
            return AMFVariantBool(this) ? true : false;
        }

        if(Empty())
        {
            return false;
        }

        AMFVariant varDest;
        varDest.ChangeType(AMF_VT_BOOL, this);

        return (AMFVariantBool(&varDest) == true) ? true : false;
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::operator amf_int64() const
    {
        if(AMFVariantType(this) == AMF_VT_INT64)
        {
            return AMFVariantInt64(this);
        }

        AMFVariant varDest;
        varDest.ChangeType(AMF_VT_INT64, this);

        return AMFVariantInt64(&varDest);
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::operator amf_uint64() const
    {
        if(AMFVariantType(this) == AMF_VT_INT64)
        {
            return AMFVariantInt64(this);
        }

        AMFVariant varDest;
        varDest.ChangeType(AMF_VT_INT64, this);

        return static_cast<amf_uint64>(AMFVariantInt64(&varDest));
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::operator amf_int32() const
    {
        if(AMFVariantType(this) == AMF_VT_INT64)
        {
            return static_cast<amf_int32>(AMFVariantInt64(this));
        }

        AMFVariant varDest;
        varDest.ChangeType(AMF_VT_INT64, this);

        return static_cast<amf_int32>(AMFVariantInt64(&varDest));
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::operator amf_uint32() const
    {
        if(AMFVariantType(this) == AMF_VT_INT64)
        {
            return static_cast<amf_uint32>(AMFVariantInt64(this));
        }

        AMFVariant varDest;
        varDest.ChangeType(AMF_VT_INT64, this);

        return static_cast<amf_uint32>(AMFVariantInt64(&varDest));
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::operator amf_double() const
    {
        if(AMFVariantType(this) == AMF_VT_DOUBLE)
        {
            return AMFVariantDouble(this);
        }

        AMFVariant varDest;
        varDest.ChangeType(AMF_VT_DOUBLE, this);

        return AMFVariantDouble(&varDest);
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::operator amf_float() const
    {
        if(AMFVariantType(this) == AMF_VT_DOUBLE)
        {
            return static_cast<amf_float>(AMFVariantDouble(this));
        }

        AMFVariant varDest;
        varDest.ChangeType(AMF_VT_DOUBLE, this);

        return static_cast<amf_float>(AMFVariantDouble(&varDest));
    }
    //-------------------------------------------------------------------------------------------------
#if AMF_BUILD_STL
    inline AMFVariant::operator amf_string() const
    {
        if(AMFVariantType(this) == AMF_VT_STRING)
        {
            return AMFVariantString(this);
        }

        AMFVariant varDest;
        varDest.ChangeType(AMF_VT_STRING, this);

        return AMFVariantString(&varDest);
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::operator amf_wstring() const
    {
        if(AMFVariantType(this) == AMF_VT_WSTRING)
        {
            return AMFVariantWString(this);
        }

        AMFVariant varDest;
        varDest.ChangeType(AMF_VT_WSTRING, this);

        return AMFVariantWString(&varDest);
    }
#endif//#if AMF_BUILD_STL
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant::operator AMFInterface*() const
    {
        if(AMFVariantType(this) == AMF_VT_INTERFACE)
        {
            return AMFVariantInterface(this);
        }
        return NULL;
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant& AMFVariant::operator=(const AMFVARIANT& other) 
    {
        AMF_CHECK_ERROR_TMP(AMFVariantCopy(this, const_cast<AMFVARIANT*>(&other)));
        return *this;
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant& AMFVariant::operator=(const AMFVARIANT* pOther)
    {
        if(pOther == NULL)
        {
            AMF_CHECK_ERROR_TMP(AMF_NULL_POINTER);
        }
        else
        {
            AMF_CHECK_ERROR_TMP(AMFVariantCopy(this, const_cast<AMFVARIANT*>(pOther)));
        }
        return *this;
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant& AMFVariant::operator=(const AMFVariant& other)
    {
        AMF_CHECK_ERROR_TMP(AMFVariantCopy(this, const_cast<AMFVARIANT*>(static_cast<const AMFVARIANT*>(&other))));
        return *this;
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant& AMFVariant::operator=(amf_bool value)
    {
        AMFVariantAssignBool(this, value);
        return *this;
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant& AMFVariant::operator=(amf_int64 value)
    {
        AMFVariantAssignInt64(this, value);
        return *this;
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant& AMFVariant::operator=(amf_uint64 value)
    {
        AMFVariantAssignInt64(this, static_cast<amf_int64>(value));
        return *this;
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant& AMFVariant::operator=(amf_double value)
    {
        AMFVariantAssignDouble(this, value);
        return *this;
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant& AMFVariant::operator=(const char* value)
    {
        AMFVariantAssignString(this, value);
        return *this;
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant& AMFVariant::operator=(const wchar_t* value)
    {
        AMFVariantAssignWString(this, value);
        return *this;
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant& AMFVariant::operator=(AMFInterface* pValue)
    {
        AMFVariantAssignInterface(this, pValue);
        return *this;
    }
    //-------------------------------------------------------------------------------------------------
    template <typename T>
    AMFVariant& AMFVariant::operator=(const AMFRefPtr<T>& value)
    {
        AMFVariantAssignInterface(this, value);
        return *this;
    }
    //-------------------------------------------------------------------------------------------------
    inline bool AMFVariant::operator==(const AMFVARIANT& other) const
    {
        return *this == &other;
    }
    //-------------------------------------------------------------------------------------------------
    inline bool AMFVariant::operator==(const AMFVARIANT* pOther) const
    {
        //TODO: double check
        bool ret = false;
        if(pOther == NULL) 
        {
            ret = false;
        }
        else
        {
            AMF_ERROR err = AMFVariantCompare(this, pOther, ret);
            AMF_CHECK_ERROR_TMP(err);
        }
        return ret;
    }
    //-------------------------------------------------------------------------------------------------
    inline bool AMFVariant::operator!=(const AMFVARIANT& other) const
    {
        return !(*this == &other);
    }
    //-------------------------------------------------------------------------------------------------
    inline bool AMFVariant::operator!=(const AMFVARIANT* pOther) const
    {
        return !(*this == pOther);
    }
    //-------------------------------------------------------------------------------------------------
    inline void AMFVariant::Clear()
    {
        AMF_CHECK_ERROR_TMP(AMFVariantClear(this));
    }
    //-------------------------------------------------------------------------------------------------
    inline void AMFVariant::Attach(AMFVARIANT& variant)
    {
        Clear();
        memcpy(this, &variant, sizeof(variant));
        AMFVariantType(&variant) = AMF_VT_EMPTY;
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVARIANT AMFVariant::Detach()
    {
        AMFVARIANT varResult = *this;
        AMFVariantType(this) = AMF_VT_EMPTY;
        return varResult;
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVARIANT& AMFVariant::GetVariant()
    {
        return *static_cast<AMFVARIANT*>(this);
    }
    //-------------------------------------------------------------------------------------------------
    inline void AMFVariant::ChangeType(AMF_VARIANT_TYPE type, const AMFVariant* pSrc)
    {
        AMF_CHECK_ERROR_TMP(AMFVariantChangeType(this, pSrc, type));
    }
    //-------------------------------------------------------------------------------------------------
    inline bool AMFVariant::Empty() const
    {
        return type == AMF_VT_EMPTY;
    }
    //-------------------------------------------------------------------------------------------------
}


#endif //#ifndef __AMFVariant_h__
