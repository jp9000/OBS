///-------------------------------------------------------------------------
///  Trade secret of Advanced Micro Devices, Inc.
///  Copyright 2013, Advanced Micro Devices, Inc., (unpublished)
///
///  All rights reserved.  This notice is intended as a precaution against
///  inadvertent publication and does not imply publication or any waiver
///  of confidentiality.  The year included in the foregoing notice is the
///  year of creation of the work.
///-------------------------------------------------------------------------
///  @file   PropertyStorage.h
///  @brief  AMFPropertyStorage interface
///-------------------------------------------------------------------------
#ifndef __AMFPropertyStorage_h__
#define __AMFPropertyStorage_h__
#pragma once

#include "Variant.h"

namespace amf
{
    //----------------------------------------------------------------------------------------------
    // AMFPropertyStorage interface
    //----------------------------------------------------------------------------------------------
    class AMFPropertyStorageListener
    {
    public:
        virtual void AMF_STD_CALL OnPropertyChanged(const wchar_t* name) = 0;
        virtual ~AMFPropertyStorageListener() {}
    };

    class AMFPropertyStorage : virtual public AMFInterface
    {
    public:
        AMF_DECLARE_IID("AMFPropertyStorage")
        virtual AMF_ERROR           AMF_STD_CALL SetProperty(const wchar_t* name, AMFVARIANT value) = 0;
        virtual AMF_ERROR           AMF_STD_CALL GetProperty(const wchar_t* name, AMFVARIANT* pValue) = 0;

        template<typename _T>
        AMF_ERROR                   AMF_STD_CALL SetProperty(const wchar_t* name, const _T& value);
        template<typename _T>
        AMF_ERROR                   AMF_STD_CALL GetProperty(const wchar_t* name, _T* pValue);

        virtual bool                AMF_STD_CALL HasProperty(const wchar_t* name) = 0;
        virtual amf_int32           AMF_STD_CALL GetPropertyCount() = 0;

        virtual AMF_ERROR           AMF_STD_CALL Clear() = 0;
        virtual AMF_ERROR           AMF_STD_CALL AddTo(AMFPropertyStorage* pDest, bool overwrite, bool deep) = 0;
        virtual AMF_ERROR           AMF_STD_CALL CopyTo(AMFPropertyStorage* pDest, bool deep) = 0;

        virtual void                AMF_STD_CALL RegisterListener(AMFPropertyStorageListener* pListener) = 0;
        virtual void                AMF_STD_CALL UnregisterListener(AMFPropertyStorageListener* pListener) = 0;

    };
    //----------------------------------------------------------------------------------------------
    // template methods implementations
    //----------------------------------------------------------------------------------------------
    template<typename _T>
    AMF_ERROR                       AMF_STD_CALL AMFPropertyStorage::SetProperty(const wchar_t* name, const _T& value)
    {
        AMF_ERROR err = SetProperty(name, static_cast<const AMFVARIANT&>(AMFVariant(value)));
        return err;
    }
    template<typename _T>       
    AMF_ERROR                       AMF_STD_CALL AMFPropertyStorage::GetProperty(const wchar_t* name, _T* pValue)
    {
        AMFVariant var;
        AMF_ERROR err = GetProperty(name, static_cast<AMFVARIANT*>(&var));
        if(err == AMF_OK)
        {
            *pValue = static_cast<_T>(var);
        }
        return err;
    }
    template<>       
    inline AMF_ERROR                       AMF_STD_CALL AMFPropertyStorage::GetProperty(const wchar_t* name, AMFInterface** ppValue)
    {
        AMFVariant var;
        AMF_ERROR err = GetProperty(name, static_cast<AMFVARIANT*>(&var));
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
    typedef AMFRefPtr<AMFPropertyStorage> AMFPropertyStoragePtr; 
}//namespace amf

#endif // #ifndef __AMFPropertyStorage_h__
