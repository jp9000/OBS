/*
 ***************************************************************************************************
 *
 * Copyright (c) 2013 Advanced Micro Devices, Inc. (unpublished)
 *
 *  All rights reserved.  This notice is intended as a precaution against inadvertent publication and
 *  does not imply publication or any waiver of confidentiality.  The year included in the foregoing
 *  notice is the year of creation of the work.
 *
 ***************************************************************************************************
 */
/**
 ***************************************************************************************************
 * @file  Platform.h
 * @brief framework types declarations
 ***************************************************************************************************
 */

#ifndef __AMFPlatform_h__
#define __AMFPlatform_h__
#pragma once

#include "Version.h"

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


#pragma warning(disable: 4251)
#pragma warning(disable: 4250)
#pragma warning(disable: 4275)


//-------------------------------------------------------------------------------------------------
// basic data types
//-------------------------------------------------------------------------------------------------

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
    #include <windows.h>

    typedef     __int64             amf_int64;
    typedef     __int32             amf_int32;
    typedef     __int16             amf_int16;
    typedef     __int8              amf_int8;

    typedef     unsigned __int64    amf_uint64;
    typedef     unsigned __int32    amf_uint32;
    typedef     unsigned __int16    amf_uint16;
    typedef     unsigned __int8     amf_uint8;
    typedef     size_t              amf_size;

    #define AMF_STD_CALL            __stdcall
    #define AMF_CDECL_CALL          __cdecl
    #define AMF_FAST_CALL           __fastcall
    #define AMF_INLINE              inline
    #define AMF_FORCEINLINE         __forceinline

#else // !WIN32 - Linux and Mac

    #include <stdint.h>
    #include <stdlib.h>
    #include <string.h>
    #include <stdio.h>

    typedef     int64_t             amf_int64;
    typedef     int32_t             amf_int32;
    typedef     int16_t             amf_int16;
    typedef     int8_t              amf_int8;

    typedef     uint64_t            amf_uint64;
    typedef     uint32_t            amf_uint32;
    typedef     uint16_t            amf_uint16;
    typedef     uint8_t             amf_uint8;
    typedef     size_t              amf_size;


    #define AMF_STD_CALL
    #define AMF_CDECL_CALL
    #define AMF_FAST_CALL
    #define AMF_INLINE              __inline__
    #define AMF_FORCEINLINE         __inline__

#endif // WIN32

typedef     void*               amf_handle;
typedef     double              amf_double;
typedef     float               amf_float;

typedef     void                amf_void;
typedef     bool                amf_bool;
typedef     long                amf_long; 
typedef     int                 amf_int; 
typedef     unsigned long       amf_ulong; 
typedef     unsigned int        amf_uint; 

typedef     amf_int64           amf_pts;     // in 100 nanosecs

struct AMFRect
{
    amf_int32 left;
    amf_int32 top;
    amf_int32 right;
    amf_int32 bottom;

    bool operator==(const AMFRect& other) const
    {
         return left == other.left && top == other.top && right == other.right && bottom == other.bottom; 
    }
    amf_int32 Width() const { return right - left; }
    amf_int32 Height() const { return bottom - top; }
};

inline AMFRect AMFConstructRect(amf_int32 left, amf_int32 top, amf_int32 right, amf_int32 bottom)
{
    AMFRect object = {left, top, right, bottom};
    return object;
}

struct AMFSize
{
    amf_int32 width;
    amf_int32 height;
    bool operator==(const AMFSize& other) const
    {
         return width == other.width && height == other.height; 
    }
};

inline AMFSize AMFConstructSize(amf_int32 width, amf_int32 height)
{
    AMFSize object = {width, height};
    return object;
}

struct AMFPoint
{
    amf_int32 x;
    amf_int32 y;
    bool operator==(const AMFPoint& other) const
    {
         return x == other.x && y == other.y; 
    }
};

inline AMFPoint AMFConstructPoint(amf_int32 x, amf_int32 y)
{
    AMFPoint object = {x, y};
    return object;
}

struct AMFRate
{
    amf_uint32 num;
    amf_uint32 den;
    bool operator==(const AMFRate& other) const
    {
         return num == other.num && den == other.den; 
    }
};

inline AMFRate AMFConstructRate(amf_int32 num, amf_int32 den)
{
    AMFRate object = {num, den};
    return object;
}

struct AMFRatio
{
    amf_uint32 num;
    amf_uint32 den;
    bool operator==(const AMFRatio& other) const
    {
         return num == other.num && den == other.den; 
    }
};

inline AMFRatio AMFConstructRatio(amf_int32 num, amf_int32 den)
{
    AMFRatio object = {num, den};
    return object;
}

#pragma warning(push)
#pragma warning(disable:4201)
#pragma pack(push, 1)
struct AMFColor
{
    union
    {
        struct
        {
            amf_uint8 r;
            amf_uint8 g;
            amf_uint8 b;
            amf_uint8 a;
        };
        amf_uint32 rgba;
    };
    bool operator==(const AMFColor& other) const
    {
         return r == other.r && g == other.g && b == other.b && a == other.a; 
    }
};
#pragma pack(pop)
#pragma warning(pop)


inline AMFColor AMFConstructColor(amf_uint8 r, amf_uint8 g, amf_uint8 b, amf_uint8 a)
{
    AMFColor object = {r, g, b, a};
    return object;
}

#endif //#ifndef __AMFPlatform_h__
