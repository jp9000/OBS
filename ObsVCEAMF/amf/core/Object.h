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
 * @file  Object.h
 * @brief Object class declaration
 ***************************************************************************************************
 */

#ifndef __AMFObject_h__
#define __AMFObject_h__
#pragma once

namespace amf
{
    //------------------------------------------------------------------------
    // base class for all AMF objects
    //------------------------------------------------------------------------
    class AMFObject
    {
    public:
        // Default constructor
        AMFObject() {}
        // Destructor
        virtual ~AMFObject() {}

    private: // Prohibit copying
        // Copying constructor
        AMFObject(const AMFObject&);
        // Assignment operator
        AMFObject& operator=(const AMFObject&);
    };
}

#endif //#ifndef __AMFObject_h__
