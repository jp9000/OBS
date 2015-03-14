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
 * @file  PropertyStorage.h
 * @brief AMFPropertyStorage interface declaration
 ***************************************************************************************************
 */

#ifndef __AMFPropertyStorage_h__
#define __AMFPropertyStorage_h__
#pragma once

#include "Variant.h"

namespace amf
{
    //----------------------------------------------------------------------------------------------
    // AMFPropertyStorageObserver interface
    //----------------------------------------------------------------------------------------------
    class AMFPropertyStorageObserver
    {
    public:
        virtual void            AMF_STD_CALL    OnPropertyChanged(const wchar_t* name) = 0;
        virtual ~AMFPropertyStorageObserver() {}
    };
    //----------------------------------------------------------------------------------------------
    // AMFPropertyStorage interface
    //----------------------------------------------------------------------------------------------
    class AMFPropertyStorage : virtual public AMFInterface
    {
    public:
        AMF_DECLARE_IID(0xc7cec05b, 0xcfb9, 0x48af, 0xac, 0xe3, 0xf6, 0x8d, 0xf8, 0x39, 0x5f, 0xe3)

        virtual AMF_RESULT          AMF_STD_CALL    SetProperty(const wchar_t* name, AMFVariantStruct value) = 0;
        virtual AMF_RESULT          AMF_STD_CALL    GetProperty(const wchar_t* name, AMFVariantStruct* pValue) const = 0;

        virtual bool                AMF_STD_CALL    HasProperty(const wchar_t* name) const = 0;
        virtual amf_int32           AMF_STD_CALL    GetPropertyCount() const = 0;
        virtual AMF_RESULT           AMF_STD_CALL   GetPropertyAt(amf_int32 index, wchar_t* name, amf_size nameSize, AMFVariantStruct* pValue) const = 0;

        virtual AMF_RESULT          AMF_STD_CALL    Clear() = 0;
        virtual AMF_RESULT          AMF_STD_CALL    AddTo(AMFPropertyStorage* pDest, bool overwrite, bool deep) const= 0;
        virtual AMF_RESULT          AMF_STD_CALL    CopyTo(AMFPropertyStorage* pDest, bool deep) const = 0;

        virtual void                AMF_STD_CALL    AddObserver(AMFPropertyStorageObserver* pObserver) = 0;
        virtual void                AMF_STD_CALL    RemoveObserver(AMFPropertyStorageObserver* pObserver) = 0;

        template<typename _T>
        AMF_RESULT                  AMF_STD_CALL    SetProperty(const wchar_t* name, const _T& value);
        template<typename _T>
        AMF_RESULT                  AMF_STD_CALL    GetProperty(const wchar_t* name, _T* pValue) const;
        template<typename _T>
        AMF_RESULT                  AMF_STD_CALL    GetPropertyString(const wchar_t* name, _T* pValue) const;
        template<typename _T>
        AMF_RESULT                  AMF_STD_CALL    GetPropertyWString(const wchar_t* name, _T* pValue) const;

    };
    //----------------------------------------------------------------------------------------------
    // template methods implementations
    //----------------------------------------------------------------------------------------------
    template<typename _T> inline
    AMF_RESULT AMF_STD_CALL AMFPropertyStorage::SetProperty(const wchar_t* name, const _T& value)
    {
        AMF_RESULT err = SetProperty(name, static_cast<const AMFVariantStruct&>(AMFVariant(value)));
        return err;
    }
    //----------------------------------------------------------------------------------------------
    template<typename _T> inline
    AMF_RESULT AMF_STD_CALL AMFPropertyStorage::GetProperty(const wchar_t* name, _T* pValue) const
    {
        AMFVariant var;
        AMF_RESULT err = GetProperty(name, static_cast<AMFVariantStruct*>(&var));
        if(err == AMF_OK)
        {
            *pValue = static_cast<_T>(var);
        }
        return err;
    }
    //----------------------------------------------------------------------------------------------
    template<typename _T> inline
    AMF_RESULT AMF_STD_CALL AMFPropertyStorage::GetPropertyString(const wchar_t* name, _T* pValue) const
    {
        AMFVariant var;
        AMF_RESULT err = GetProperty(name, static_cast<AMFVariantStruct*>(&var));
        if(err == AMF_OK)
        {
            *pValue = var.ToString().c_str();
        }
        return err;
    }
    //----------------------------------------------------------------------------------------------
    template<typename _T> inline
    AMF_RESULT AMF_STD_CALL AMFPropertyStorage::GetPropertyWString(const wchar_t* name, _T* pValue) const
    {
        AMFVariant var;
        AMF_RESULT err = GetProperty(name, static_cast<AMFVariantStruct*>(&var));
        if(err == AMF_OK)
        {
            *pValue = var.ToWString().c_str();
        }
        return err;
    }
    //----------------------------------------------------------------------------------------------
    template<> inline
    AMF_RESULT AMF_STD_CALL AMFPropertyStorage::GetProperty(const wchar_t* name,
            AMFInterface** ppValue) const
    {
        AMFVariant var;
        AMF_RESULT err = GetProperty(name, static_cast<AMFVariantStruct*>(&var));
        if(err == AMF_OK)
        {
            *ppValue = static_cast<AMFInterface*>(var);
        }
        if(*ppValue)
        {
            (*ppValue)->Acquire();
        }
        return err;
    }
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFPropertyStorage> AMFPropertyStoragePtr;
} //namespace amf

#endif // #ifndef __AMFPropertyStorage_h__
