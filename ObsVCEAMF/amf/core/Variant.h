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
 * @file  Variant.h
 * @brief AMFVariant interface declaration
 ***************************************************************************************************
 */

#ifndef __AMFVariant_h__
#define __AMFVariant_h__
#pragma once
#pragma warning(disable: 4996)

#include "Interface.h"

////////////////////////////////////////////////////////////////////////////////////
namespace amf
{
    enum AMF_VARIANT_TYPE
    {
        AMF_VARIANT_EMPTY       = 0,

        AMF_VARIANT_BOOL        = 1,
        AMF_VARIANT_INT64       = 2,
        AMF_VARIANT_DOUBLE      = 3,

        AMF_VARIANT_RECT        = 4,
        AMF_VARIANT_SIZE        = 5,
        AMF_VARIANT_POINT       = 6,
        AMF_VARIANT_RATE        = 7,
        AMF_VARIANT_RATIO       = 8,
        AMF_VARIANT_COLOR       = 9,

        AMF_VARIANT_STRING      = 10,  // value is char*
        AMF_VARIANT_WSTRING     = 11,  // value is wchar*
        AMF_VARIANT_INTERFACE   = 12,  // value is AMFInterface*
    };

    struct AMFVariantStruct
    {
        AMF_VARIANT_TYPE    type;
        union
        {
            amf_bool        boolValue;
            amf_int64       int64Value;
            amf_double      doubleValue;
            char*           stringValue;
            wchar_t*        wstringValue;
            AMFInterface*   pInterface;
            AMFRect         rectValue;
            AMFSize         sizeValue;
            AMFPoint        pointValue;
            AMFRate         rateValue;
            AMFRatio        ratioValue;
            AMFColor        colorValue;
        };
    };

extern  "C"
{
    AMF_CORE_LINK AMF_RESULT AMF_CDECL_CALL AMFVariantInit(AMFVariantStruct* pVariant);
    AMF_CORE_LINK AMF_RESULT AMF_CDECL_CALL AMFVariantClear(AMFVariantStruct* pVariant);
    AMF_CORE_LINK AMF_RESULT AMF_CDECL_CALL AMFVariantCompare(const AMFVariantStruct* pFirst, const AMFVariantStruct* pSecond, bool& equal);
    AMF_CORE_LINK AMF_RESULT AMF_CDECL_CALL AMFVariantCopy(AMFVariantStruct* pDest, const AMFVariantStruct* pSrc);
    AMF_CORE_LINK AMF_RESULT AMF_CDECL_CALL AMFVariantChangeType(AMFVariantStruct* pDest, const AMFVariantStruct* pSrc, AMF_VARIANT_TYPE newType);

    AMF_CORE_LINK AMF_RESULT AMF_CDECL_CALL AMFVariantAssignBool(AMFVariantStruct* pDest, bool value);
    AMF_CORE_LINK AMF_RESULT AMF_CDECL_CALL AMFVariantAssignInt64(AMFVariantStruct* pDest, amf_int64 value);
    AMF_CORE_LINK AMF_RESULT AMF_CDECL_CALL AMFVariantAssignDouble(AMFVariantStruct* pDest, amf_double value);
    AMF_CORE_LINK AMF_RESULT AMF_CDECL_CALL AMFVariantAssignString(AMFVariantStruct* pDest, const char* value);
    AMF_CORE_LINK AMF_RESULT AMF_CDECL_CALL AMFVariantAssignWString(AMFVariantStruct* pDest, const wchar_t* value);
    AMF_CORE_LINK AMF_RESULT AMF_CDECL_CALL AMFVariantAssignInterface(AMFVariantStruct* pDest, AMFInterface* value);

    AMF_CORE_LINK AMF_RESULT AMF_CDECL_CALL AMFVariantAssignRect (AMFVariantStruct* pDest, const AMFRect& value);
    AMF_CORE_LINK AMF_RESULT AMF_CDECL_CALL AMFVariantAssignSize (AMFVariantStruct* pDest, const AMFSize& value);
    AMF_CORE_LINK AMF_RESULT AMF_CDECL_CALL AMFVariantAssignPoint(AMFVariantStruct* pDest, const AMFPoint& value);
    AMF_CORE_LINK AMF_RESULT AMF_CDECL_CALL AMFVariantAssignRate (AMFVariantStruct* pDest, const AMFRate& value);
    AMF_CORE_LINK AMF_RESULT AMF_CDECL_CALL AMFVariantAssignRatio(AMFVariantStruct* pDest, const AMFRatio& value);
    AMF_CORE_LINK AMF_RESULT AMF_CDECL_CALL AMFVariantAssignColor(AMFVariantStruct* pDest, const AMFColor& value);

    //helpers
    AMF_CORE_LINK char* AMF_CDECL_CALL AMFVariantDuplicateString(const char* from);
    AMF_CORE_LINK void AMF_CDECL_CALL AMFVariantFreeString(char* from);

    AMF_CORE_LINK wchar_t* AMF_CDECL_CALL AMFVariantDuplicateWString(const wchar_t* from);
    AMF_CORE_LINK void AMF_CDECL_CALL AMFVariantFreeWString(wchar_t* from);
} // extern  "C"

    inline AMF_VARIANT_TYPE     AMF_STD_CALL AMFVariantGetType(const AMFVariantStruct* _variant) { return (_variant)->type; }
    inline AMF_VARIANT_TYPE&    AMF_STD_CALL AMFVariantGetType(AMFVariantStruct* _variant) { return (_variant)->type; }
    inline amf_bool             AMF_STD_CALL AMFVariantGetBool(const AMFVariantStruct* _variant) { return (_variant)->boolValue; }
    inline amf_int64            AMF_STD_CALL AMFVariantGetInt64(const AMFVariantStruct* _variant) { return (_variant)->int64Value; }
    inline amf_double           AMF_STD_CALL AMFVariantGetDouble(const AMFVariantStruct* _variant) { return (_variant)->doubleValue; }
    inline const char*          AMF_STD_CALL AMFVariantGetString(const AMFVariantStruct* _variant) { return (_variant)->stringValue; }
    inline const wchar_t*       AMF_STD_CALL AMFVariantGetWString(const AMFVariantStruct* _variant) { return (_variant)->wstringValue; }
    inline const AMFInterface*  AMF_STD_CALL AMFVariantGetInterface(const AMFVariantStruct* _variant) { return (_variant)->pInterface; }
    inline AMFInterface*        AMF_STD_CALL AMFVariantGetInterface(AMFVariantStruct* _variant) { return (_variant)->pInterface; }

    inline const AMFRect &       AMF_STD_CALL AMFVariantGetRect (const AMFVariantStruct* _variant) { return (_variant)->rectValue; }
    inline const AMFSize &       AMF_STD_CALL AMFVariantGetSize (const AMFVariantStruct* _variant) { return (_variant)->sizeValue; }
    inline const AMFPoint&       AMF_STD_CALL AMFVariantGetPoint(const AMFVariantStruct* _variant) { return (_variant)->pointValue; }
    inline const AMFRate &       AMF_STD_CALL AMFVariantGetRate (const AMFVariantStruct* _variant) { return (_variant)->rateValue; }
    inline const AMFRatio&       AMF_STD_CALL AMFVariantGetRatio(const AMFVariantStruct* _variant) { return (_variant)->ratioValue; }
    inline const AMFColor&       AMF_STD_CALL AMFVariantGetColor(const AMFVariantStruct* _variant) { return (_variant)->colorValue; }
}

//inline Variant helper class
namespace amf
{

    class AMFVariant : public AMFVariantStruct
    {
    public:
        class String;
        class WString;

    public:
        AMFVariant() {  AMFVariantInit(this); }
        explicit AMFVariant(const AMFVariantStruct& other) { AMFVariantInit(this); AMFVariantCopy(this, const_cast<AMFVariantStruct*>(&other)); }

        explicit AMFVariant(const AMFVariantStruct* pOther);
        template<typename T>
        explicit AMFVariant(const AMFInterfacePtr_T<T>& pValue);

        AMFVariant(const AMFVariant& other) { AMFVariantInit(this); AMFVariantCopy(this, const_cast<AMFVariantStruct*>(static_cast<const AMFVariantStruct*>(&other))); }

        explicit inline AMFVariant(amf_bool value)          { AMFVariantInit(this); AMFVariantAssignBool(this, value); }
        explicit inline AMFVariant(amf_int64 value)         { AMFVariantInit(this); AMFVariantAssignInt64(this, value); }
        explicit inline AMFVariant(amf_uint64 value)        { AMFVariantInit(this); AMFVariantAssignInt64(this, (amf_int64)value); }
        explicit inline AMFVariant(amf_int32 value)         { AMFVariantInit(this); AMFVariantAssignInt64(this, value); }
        explicit inline AMFVariant(amf_uint32 value)        { AMFVariantInit(this); AMFVariantAssignInt64(this, value); }
        explicit inline AMFVariant(amf_double value)        { AMFVariantInit(this); AMFVariantAssignDouble(this, value); }
        explicit inline AMFVariant(const AMFRect & value)   { AMFVariantInit(this); AMFVariantAssignRect(this, value); }
        explicit inline AMFVariant(const AMFSize & value)   { AMFVariantInit(this); AMFVariantAssignSize(this, value); }
        explicit inline AMFVariant(const AMFPoint& value)   { AMFVariantInit(this); AMFVariantAssignPoint(this, value); }
        explicit inline AMFVariant(const AMFRate & value)   { AMFVariantInit(this); AMFVariantAssignRate(this, value); }
        explicit inline AMFVariant(const AMFRatio& value)   { AMFVariantInit(this); AMFVariantAssignRatio(this, value); }
        explicit inline AMFVariant(const AMFColor& value)   { AMFVariantInit(this); AMFVariantAssignColor(this, value); }
        explicit inline AMFVariant(const char* value)       { AMFVariantInit(this); AMFVariantAssignString(this, value); }
        explicit inline AMFVariant(const wchar_t* value)    { AMFVariantInit(this); AMFVariantAssignWString(this, value); }
        explicit inline AMFVariant(AMFInterface* pValue)    { AMFVariantInit(this); AMFVariantAssignInterface(this, pValue); }

        ~AMFVariant() { AMFVariantClear(this); }

        AMFVariant& operator=(const AMFVariantStruct& other);
        AMFVariant& operator=(const AMFVariantStruct* pOther);
        AMFVariant& operator=(const AMFVariant& other);

        AMFVariant& operator=(amf_bool          value)      { AMFVariantAssignBool(this, value); return *this;}
        AMFVariant& operator=(amf_int64         value)      { AMFVariantAssignInt64(this, value); return *this;}
        AMFVariant& operator=(amf_uint64        value)      { AMFVariantAssignInt64(this, (amf_int64)value);  return *this;}
        AMFVariant& operator=(amf_int32         value)      { AMFVariantAssignInt64(this, value);  return *this;}
        AMFVariant& operator=(amf_uint32        value)      { AMFVariantAssignInt64(this, value);  return *this;}
        AMFVariant& operator=(amf_double        value)      { AMFVariantAssignDouble(this, value);  return *this;}
        AMFVariant& operator=(const AMFRect &   value)      { AMFVariantAssignRect(this, value);  return *this;}
        AMFVariant& operator=(const AMFSize &   value)      { AMFVariantAssignSize(this, value);  return *this;}
        AMFVariant& operator=(const AMFPoint&   value)      { AMFVariantAssignPoint(this, value);  return *this;}
        AMFVariant& operator=(const AMFRate &   value)      { AMFVariantAssignRate(this, value);  return *this;}
        AMFVariant& operator=(const AMFRatio&   value)      { AMFVariantAssignRatio(this, value);  return *this;}
        AMFVariant& operator=(const AMFColor&   value)      { AMFVariantAssignColor(this, value);  return *this;}
        AMFVariant& operator=(const char*       value)      { AMFVariantAssignString(this, value);  return *this;}
        AMFVariant& operator=(const wchar_t*    value)      { AMFVariantAssignWString(this, value);  return *this;}
        AMFVariant& operator=(AMFInterface*     value)      { AMFVariantAssignInterface(this, value);  return *this;}

        template<typename T> AMFVariant& operator=(const AMFInterfacePtr_T<T>& value);

        operator amf_bool() const          { return ToBool();       }
        operator amf_int64() const         { return ToInt64();      }
        operator amf_uint64() const        { return ToUInt64();     }
        operator amf_int32() const         { return ToInt32();      }
        operator amf_uint32() const        { return ToUInt32();     }
        operator amf_double() const        { return ToDouble();     }
        operator amf_float() const         { return ToFloat();      }
        operator AMFRect () const          { return ToRect ();      }
        operator AMFSize () const          { return ToSize ();      }
        operator AMFPoint() const          { return ToPoint();      }
        operator AMFRate () const          { return ToRate ();      }
        operator AMFRatio() const          { return ToRatio();      }
        operator AMFColor() const          { return ToColor();      }
        operator AMFInterface*() const     { return ToInterface();  }

        inline amf_bool         ToBool() const      { return Empty() ? false        : GetValue<amf_bool,   AMF_VARIANT_BOOL>(AMFVariantGetBool); }
        inline amf_int64        ToInt64() const     { return Empty() ? 0            : GetValue<amf_int64,  AMF_VARIANT_INT64>(AMFVariantGetInt64); }
        inline amf_uint64       ToUInt64() const    { return Empty() ? 0            : GetValue<amf_uint64, AMF_VARIANT_INT64>(AMFVariantGetInt64); }
        inline amf_int32        ToInt32() const     { return Empty() ? 0            : GetValue<amf_int32,  AMF_VARIANT_INT64>(AMFVariantGetInt64); }
        inline amf_uint32       ToUInt32() const    { return Empty() ? 0            : GetValue<amf_uint32, AMF_VARIANT_INT64>(AMFVariantGetInt64); }
        inline amf_double       ToDouble() const    { return Empty() ? 0            : GetValue<amf_double, AMF_VARIANT_DOUBLE>(AMFVariantGetDouble); }
        inline amf_float        ToFloat() const     { return Empty() ? 0            : GetValue<amf_float,  AMF_VARIANT_DOUBLE>(AMFVariantGetDouble); }
        inline AMFRect          ToRect () const     { return Empty() ? AMFRect()    : GetValue<AMFRect,  AMF_VARIANT_RECT>(AMFVariantGetRect); }
        inline AMFSize          ToSize () const     { return Empty() ? AMFSize()    : GetValue<AMFSize,  AMF_VARIANT_SIZE>(AMFVariantGetSize); }
        inline AMFPoint         ToPoint() const     { return Empty() ? AMFPoint()   : GetValue<AMFPoint, AMF_VARIANT_POINT>(AMFVariantGetPoint); }
        inline AMFRate          ToRate () const     { return Empty() ? AMFRate()    : GetValue<AMFRate,  AMF_VARIANT_RATE>(AMFVariantGetRate); }
        inline AMFRatio         ToRatio() const     { return Empty() ? AMFRatio()   : GetValue<AMFRatio, AMF_VARIANT_RATIO>(AMFVariantGetRatio); }
        inline AMFColor         ToColor() const     { return Empty() ? AMFColor()   : GetValue<AMFColor, AMF_VARIANT_COLOR>(AMFVariantGetColor); }
        inline AMFInterface*    ToInterface() const { return AMFVariantGetType(this) == AMF_VARIANT_INTERFACE ? this->pInterface : NULL; }
        inline String           ToString() const;
        inline WString          ToWString() const;

        bool operator==(const AMFVariantStruct& other) const;
        bool operator==(const AMFVariantStruct* pOther) const;

        bool operator!=(const AMFVariantStruct& other) const;
        bool operator!=(const AMFVariantStruct* pOther) const;

        void Clear() { AMFVariantClear(this); }

        void Attach(AMFVariantStruct& variant);
        AMFVariantStruct Detach();

        AMFVariantStruct& GetVariant();

        void ChangeType(AMF_VARIANT_TYPE type, const AMFVariant* pSrc = NULL);

        bool Empty() const;
    private:
        template<class ReturnType, AMF_VARIANT_TYPE variantType, typename Getter>
        ReturnType GetValue(Getter getter) const;
    };
    //-------------------------------------------------------------------------------------------------

    inline AMFVariant::AMFVariant(const AMFVariantStruct* pOther)
    {
        AMFVariantInit(this);
        if(pOther != NULL)
        {
            AMFVariantCopy(this, const_cast<AMFVariantStruct*>(pOther));
        }
    }
    //-------------------------------------------------------------------------------------------------
    template<typename T>
    AMFVariant::AMFVariant(const AMFInterfacePtr_T<T>& pValue)
    {
        AMFVariantInit(this);
        AMFVariantAssignInterface(this, pValue);
    }
    //-------------------------------------------------------------------------------------------------
    template<class ReturnType, AMF_VARIANT_TYPE variantType, typename Getter>
    ReturnType AMFVariant::GetValue(Getter getter) const
    {
        ReturnType str = ReturnType();
        if(AMFVariantGetType(this) == variantType)
        {
            str = static_cast<ReturnType>(getter(this));
        }
        else
        {
            AMFVariant varDest;
            varDest.ChangeType(variantType, this);
            if(varDest.type != AMF_VARIANT_EMPTY)
            {
                str = static_cast<ReturnType>(getter(&varDest));
            }
        }
        return str;
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant& AMFVariant::operator=(const AMFVariantStruct& other)
    {
        AMFVariantCopy(this, const_cast<AMFVariantStruct*>(&other));
        return *this;
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant& AMFVariant::operator=(const AMFVariantStruct* pOther)
    {
        if(pOther != NULL)
        {
            AMFVariantCopy(this, const_cast<AMFVariantStruct*>(pOther));
        }
        return *this;
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariant& AMFVariant::operator=(const AMFVariant& other)
    {
        AMFVariantCopy(this,
                const_cast<AMFVariantStruct*>(static_cast<const AMFVariantStruct*>(&other)));
        return *this;
    }
    //-------------------------------------------------------------------------------------------------
    template<typename T>
    AMFVariant& AMFVariant::operator=(const AMFInterfacePtr_T<T>& value)
    {
        AMFVariantAssignInterface(this, value);
        return *this;
    }
    //-------------------------------------------------------------------------------------------------
    inline bool AMFVariant::operator==(const AMFVariantStruct& other) const
    {
        return *this == &other;
    }
    //-------------------------------------------------------------------------------------------------
    inline bool AMFVariant::operator==(const AMFVariantStruct* pOther) const
    {
        //TODO: double check
        bool ret = false;
        if(pOther == NULL)
        {
            ret = false;
        }
        else
        {
            AMFVariantCompare(this, pOther, ret);
        }
        return ret;
    }
    //-------------------------------------------------------------------------------------------------
    inline bool AMFVariant::operator!=(const AMFVariantStruct& other) const
    {
        return !(*this == &other);
    }
    //-------------------------------------------------------------------------------------------------
    inline bool AMFVariant::operator!=(const AMFVariantStruct* pOther) const
    {
        return !(*this == pOther);
    }
    //-------------------------------------------------------------------------------------------------
    inline void AMFVariant::Attach(AMFVariantStruct& variant)
    {
        Clear();
        memcpy(this, &variant, sizeof(variant));
        AMFVariantGetType(&variant) = AMF_VARIANT_EMPTY;
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariantStruct AMFVariant::Detach()
    {
        AMFVariantStruct varResult = *this;
        AMFVariantGetType(this) = AMF_VARIANT_EMPTY;
        return varResult;
    }
    //-------------------------------------------------------------------------------------------------
    inline AMFVariantStruct& AMFVariant::GetVariant()
    {
        return *static_cast<AMFVariantStruct*>(this);
    }
    //-------------------------------------------------------------------------------------------------
    inline void AMFVariant::ChangeType(AMF_VARIANT_TYPE type, const AMFVariant* pSrc)
    {
        AMFVariantChangeType(this, pSrc, type);
    }
    //-------------------------------------------------------------------------------------------------
    inline bool AMFVariant::Empty() const
    {
        return type == AMF_VARIANT_EMPTY;
    }


    //-------------------------------------------------------------------------------------------------
    class AMFVariant::String
    {
        friend class AMFVariant;
    private:
        String()
            :m_Str(NULL)
        {
        }
        String(const char* str) 
            :m_Str(NULL)
        {
            m_Str = AMFVariantDuplicateString(str);
        }
        void Free()
        {
            if (m_Str != NULL)
            {
                AMFVariantFreeString(m_Str);
            }
        }
    public:
        String(const String& p_other)
            : m_Str(NULL)
        {
            operator=(p_other);
        }

#if (__cplusplus == 201103L) || defined(__GXX_EXPERIMENTAL_CXX0X) || (_MSC_VER >= 1600)
        String(String&& p_other)
            : m_Str(NULL)
        {
            operator=(p_other);
        }
#endif
        ~String()
        {
            Free();
        }

        String& operator=(const String& p_other)
        {
            Free();
            m_Str = AMFVariantDuplicateString(p_other.m_Str);
            return *this;
        }
#if (__cplusplus == 201103L) || defined(__GXX_EXPERIMENTAL_CXX0X) || (_MSC_VER >= 1600)
        String& operator=(String&& p_other)
        {
            Free();
            m_Str = p_other.m_Str;
            p_other.m_Str = NULL;    //    Transfer the ownership
            return *this;
        }
#endif
        const char* c_str() const { return m_Str; }
    private:
        char*    m_Str;
    };

    class AMFVariant::WString
    {
        friend class AMFVariant;
    private:
        WString()
            :m_Str(NULL)
        {
        }
        WString(const wchar_t* str)
            : m_Str(NULL)
        {
            m_Str = AMFVariantDuplicateWString(str);
        }
        void Free()
        {
            if (m_Str != NULL)
            {
                AMFVariantFreeWString(m_Str);
            }
        }
    public:
        WString(const WString& p_other)
            : m_Str(NULL)
        {
            operator=(p_other);
        }
#if (__cplusplus == 201103L) || defined(__GXX_EXPERIMENTAL_CXX0X) || (_MSC_VER >= 1600)
        WString(WString&& p_other)
            : m_Str(NULL)
        {
            operator=(p_other);
        }
#endif
        ~WString()
        {
            Free();
        }

        WString& operator=(const WString& p_other)
        {
            Free();
            m_Str = AMFVariantDuplicateWString(p_other.m_Str);
            return *this;
        }
#if (__cplusplus == 201103L) || defined(__GXX_EXPERIMENTAL_CXX0X) || (_MSC_VER >= 1600)
        WString& operator=(WString&& p_other)
        {
            Free();
            m_Str = p_other.m_Str;
            p_other.m_Str = NULL;    //    Transfer the ownership
            return *this;
        }
#endif
        const wchar_t* c_str() const { return m_Str; }
    private:
        wchar_t*    m_Str;
    };
    //-------------------------------------------------------------------------------------------------
    AMFVariant::String       AMFVariant::ToString() const
    {
        String temp = GetValue<String, AMF_VARIANT_STRING>(AMFVariantGetString); 
        return String(temp.c_str());
    }
    //-------------------------------------------------------------------------------------------------

    AMFVariant::WString      AMFVariant::ToWString() const
    {
        WString temp = GetValue<WString, AMF_VARIANT_WSTRING>(AMFVariantGetWString);
        return WString(temp.c_str());
    }

}

#endif //#ifndef __AMFVariant_h__
