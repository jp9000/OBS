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
 * @file  Interface.h
 * @brief AMFInterface interface declaration
 ***************************************************************************************************
 */

#ifndef __AMFInterface_h__
#define __AMFInterface_h__
#pragma once

#include "Object.h"
#include "Guid.h"
#include "Result.h"

namespace amf
{
    #define AMF_DECLARE_IID(_data1, _data2, _data3, _data41, _data42, _data43, _data44, \
                            _data45, _data46, _data47, _data48) \
        static const amf::AMFGuid IID() \
        { \
            amf::AMFGuid uid(_data1, _data2, _data3, _data41, _data42, _data43, _data44, \
                    _data45, _data46, _data47, _data48); \
            return uid; \
        } \


    //------------------------------------------------------------------------
    // base class for all AMF interfaces
    //------------------------------------------------------------------------
    class AMFInterface : public AMFObject
    {
    public:
        AMF_DECLARE_IID(0x9d872f34, 0x90dc, 0x4b93, 0xb6, 0xb2, 0x6c, 0xa3, 0x7c, 0x85, 0x25, 0xdb)

        virtual amf_long AMF_STD_CALL Acquire() = 0;
        virtual amf_long AMF_STD_CALL Release() = 0;
        virtual AMF_RESULT AMF_STD_CALL QueryInterface(const AMFGuid& interfaceID,
            void** ppInterface) = 0;
    };

    template<typename _Interf>
    class AMFInterfacePtr_T
    {
    private:
        _Interf* m_pInterf;

        void InternalAcquire()
        {
            if(m_pInterf != NULL)
            {
                m_pInterf->Acquire();
            }
        }
        void InternalRelease()
        {
            if(m_pInterf != NULL)
            {
                m_pInterf->Release();
            }
        }
    public:
        AMFInterfacePtr_T() : m_pInterf(NULL)
        {}

        AMFInterfacePtr_T(const AMFInterfacePtr_T<_Interf>& p) : m_pInterf(p.m_pInterf)
        {
            InternalAcquire();
        }

        AMFInterfacePtr_T(_Interf* pInterface) : m_pInterf(pInterface)
        {
            InternalAcquire();
        }

        template<typename _OtherInterf>
        explicit AMFInterfacePtr_T(const AMFInterfacePtr_T<_OtherInterf>& cp) : m_pInterf(NULL)
        {
            void* pInterf = NULL;
            if((cp == NULL) || (cp->QueryInterface(_Interf::IID(), &pInterf) != AMF_OK))
            {
                pInterf = NULL;
            }
            m_pInterf = static_cast<_Interf*>(pInterf);
        }

        template<typename _OtherInterf>
        explicit AMFInterfacePtr_T(_OtherInterf* cp) : m_pInterf(NULL)
        {
            void* pInterf = NULL;
            if((cp == NULL) || (cp->QueryInterface(_Interf::IID(), &pInterf) != AMF_OK))
            {
                pInterf = NULL;
            }
            m_pInterf = static_cast<_Interf*>(pInterf);
        }

        ~AMFInterfacePtr_T()
        {
            InternalRelease();
        }

        AMFInterfacePtr_T& operator=(_Interf* pInterface)
        {
            if(m_pInterf != pInterface)
            {
                _Interf* pOldInterface = m_pInterf;
                m_pInterf = pInterface;
                InternalAcquire();
                if(pOldInterface != NULL)
                {
                    pOldInterface->Release();
                }
            }
            return *this;
        }

        AMFInterfacePtr_T& operator=(const AMFInterfacePtr_T<_Interf>& cp)
        {
            return operator=(cp.m_pInterf);
        }

        void Attach(_Interf* pInterface)
        {
            InternalRelease();
            m_pInterf = pInterface;
        }

        _Interf* Detach()
        {
            _Interf* const pOld = m_pInterf;
            m_pInterf = NULL;
            return pOld;
        }

        operator _Interf*() const
        {
            return m_pInterf;
        }

        _Interf& operator*() const
        {
            return *m_pInterf;
        }

        // Returns the address of the interface pointer contained in this
        // class. This is required for initializing from C-style factory function to
        // avoid getting an incorrect ref count at the beginning.

        _Interf** operator&()
        {
            InternalRelease();
            m_pInterf = 0;
            return &m_pInterf;
        }

        _Interf* operator->() const
        {
            return m_pInterf;
        }

        bool operator==(const AMFInterfacePtr_T<_Interf>& p)
        {
            return (m_pInterf == p.m_pInterf);
        }

        bool operator==(_Interf* p)
        {
            return (m_pInterf == p);
        }

        bool operator!=(const AMFInterfacePtr_T<_Interf>& p)
        {
            return !(operator==(p));
        }
        bool operator!=(_Interf* p)
        {
            return !(operator==(p));
        }

        _Interf* GetPtr()
        {
            return m_pInterf;
        }

        const _Interf* GetPtr() const
        {
            return m_pInterf;
        }
    };
    #define AMFNullPtr (static_cast<AMFInterface*>(NULL))

    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFInterface> AMFInterfacePtr;
}

#endif //#ifndef __AMFInterface_h__
