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
 * @file  Clonable.h
 * @brief Clonable interface declaration
 ***************************************************************************************************
 */

#ifndef __AMFClonable_h__
#define __AMFClonable_h__
#pragma once

#include "Interface.h"

namespace amf
{
    //----------------------------------------------------------------------------------------------
    // AMFClonable interface
    //----------------------------------------------------------------------------------------------
    class AMFClonable : virtual public AMFInterface
    {
    public:
        AMF_DECLARE_IID(0xbae69a4d, 0xd445, 0x4851, 0x9b, 0xc1, 0x3f, 0xf7, 0xe1, 0xb5, 0x5f, 0xcb)
    public:
        virtual AMF_RESULT AMF_STD_CALL Clone(AMFClonable** ppNewObject) = 0;
    };
    typedef AMFInterfacePtr_T<AMFClonable> AMFClonablePtr;
}

#endif //#ifndef __AMFClonable_h__
