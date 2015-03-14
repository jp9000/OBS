///-------------------------------------------------------------------------
///  Trade secret of Advanced Micro Devices, Inc.
///  Copyright 2013, Advanced Micro Devices, Inc., (unpublished)
///
///  All rights reserved.  This notice is intended as a precaution against
///  inadvertent publication and does not imply publication or any waiver
///  of confidentiality.  The year included in the foregoing notice is the
///  year of creation of the work.
///-------------------------------------------------------------------------
///  @file   Core.h
///  @brief  Core configuration defines
///-------------------------------------------------------------------------
#ifndef __AMFCore_h__
#define __AMFCore_h__
#pragma once

//define export declaration
#ifdef _WIN32
    #if defined(AMF_CORE_STATIC)
        #define AMF_CORE_LINK
    #else
        #if defined(AMF_CORE_EXPORTS) 
            #define AMF_CORE_LINK __declspec(dllexport)
        #else
            #define AMF_CORE_LINK __declspec(dllimport)
        #endif
    #endif
#else // #ifdef _WIN32
    #define AMF_CORE_LINK
#endif // #ifdef _WIN32

#define AMF_VERSION_MAJOR 1
#define AMF_VERSION_MINOR 1
#define AMF_VERSION_RELEASE 1

//TODO:separate internal STL and external
#ifndef AMF_BUILD_STL
    #define AMF_BUILD_STL               1
#endif


#pragma warning(disable: 4251)
#pragma warning(disable: 4250)
#pragma warning(disable: 4275)



#endif //#ifndef __AMFCore_h__
