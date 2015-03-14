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
 * @file  Result.h
 * @brief result codes
 ***************************************************************************************************
 */

#ifndef __AMFResult_h__
#define __AMFResult_h__
#pragma once

#include "Platform.h"


enum AMF_RESULT
{
    AMF_OK                                   = 0,
    AMF_FAIL                                    ,

// common errors
    AMF_UNEXPECTED                              ,

    AMF_ACCESS_DENIED                           ,
    AMF_INVALID_ARG                             ,
    AMF_OUT_OF_RANGE                            ,

    AMF_OUT_OF_MEMORY                           ,
    AMF_INVALID_POINTER                         ,

    AMF_NO_INTERFACE                            ,
    AMF_NOT_IMPLEMENTED                         ,
    AMF_NOT_SUPPORTED                           ,
    AMF_NOT_FOUND                               ,

    AMF_ALREADY_INITIALIZED                     ,
    AMF_NOT_INITIALIZED                         ,

    AMF_INVALID_FORMAT                          ,// invalid data format

    AMF_WRONG_STATE                             ,
    AMF_FILE_NOT_OPEN                           ,// cannot open file

// device common codes
    AMF_NO_DEVICE                               ,

// device directx
    AMF_DIRECTX_FAILED                          ,
// device opencl 
    AMF_OPENCL_FAILED                           ,
// device opengl 
    AMF_GLX_FAILED                              ,//failed to use GLX
// device XV 
    AMF_XV_FAILED                               , //failed to use Xv extension
// device alsa
    AMF_ALSA_FAILED                             ,//failed to use ALSA

// component common codes

    //result codes
    AMF_EOF                                     ,
    AMF_REPEAT                                  ,
    AMF_NEED_MORE_INPUT                         ,//returned by AMFComponent::QueryOutput if more frames to be submited
    AMF_INPUT_FULL                              ,//returned by AMFComponent::SubmitInput if input queue is full
    AMF_RESOLUTION_CHANGED                      ,//resolution changed client needs to Drain/Terminate/Init

    //error codes
    AMF_INVALID_DATA_TYPE                       ,//invalid data type
    AMF_INVALID_RESOLUTION                      ,//invalid resolution (width or height)
    AMF_CODEC_NOT_SUPPORTED                     ,//codec not supported
    AMF_SURFACE_FORMAT_NOT_SUPPORTED            ,//surface format not supported

// component video decoder
    AMF_DECODER_NOT_PRESENT                     ,//failed to create the decoder
    AMF_DECODER_SURFACE_ALLOCATION_FAILED       ,//failed to create the surface for decoding
    AMF_DECODER_NO_FREE_SURFACES                ,

// component video encoder
    AMF_ENCODER_NOT_PRESENT                     ,//failed to create the encoder

// component video processor

// component video conveter

// component dem
    AMF_DEM_ERROR                               ,
    AMF_DEM_PROPERTY_READONLY                   ,
    AMF_DEM_REMOTE_DISPLAY_CREATE_FAILED        ,
    AMF_DEM_START_ENCODING_FAILED               ,
    AMF_DEM_QUERY_OUTPUT_FAILED                 ,
};

namespace amf
{
    AMF_CORE_LINK const wchar_t* AMF_STD_CALL AMFGetResultText(AMF_RESULT res);
}


#endif //#ifndef __AMFResult_h__
