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
 * @file  Debug.h
 * @brief AMFDebug interface declaration
 ***************************************************************************************************
 */

#ifndef __AMFDebug_h__
#define __AMFDebug_h__
#pragma once

#include "Platform.h"
#include "Result.h"


namespace amf
{

    // replace everywhere to AMF_TRACE_ERROR
    #define AMF_TRACE_ERROR     0
    #define AMF_TRACE_WARNING   1
    #define AMF_TRACE_INFO      2 // default in sdk
    #define AMF_TRACE_DEBUG     3
    #define AMF_TRACE_TRACE     4

    #define AMF_TRACE_TEST      5
    #define AMF_TRACE_NOLOG     100

    #define AMF_TRACE_WRITER_CONSOLE            L"Console"
    #define AMF_TRACE_WRITER_DEBUG_OUTPUT       L"DebugOutput"
    #define AMF_TRACE_WRITER_FILE               L"File"

    /**
    *******************************************************************************
    *   AMFTraceWriter
    *
    *   @brief
    *       AMFTraceWriter interface for custom trace writer
    *
    *******************************************************************************
    */
    class AMFTraceWriter
    {
    public:
        virtual ~AMFTraceWriter(){}

        virtual void Write(const wchar_t* scope, const wchar_t* message) = 0;
        virtual void Flush() = 0;
    };



extern "C"
{

    /**
    *******************************************************************************
    *   AMFTraceSetPath
    *
    *   @brief
    *       Set Trace path
    *
    *       Returns AMF_OK if succeeded
    *******************************************************************************
    */
    AMF_CORE_LINK AMF_RESULT AMF_CDECL_CALL  AMFTraceSetPath(const wchar_t* path);

    /**
    *******************************************************************************
    *   AMFTraceGetPath
    *
    *   @brief
    *       Get Trace path
    *
    *       Returns AMF_OK if succeeded
    *******************************************************************************
    */
    AMF_CORE_LINK AMF_RESULT AMF_CDECL_CALL  AMFTraceGetPath(
        wchar_t* path, ///< [out] buffer able to hold *pSize symbols; path is copied there, at least part fitting the buffer, always terminator is copied
        amf_size* pSize ///< [in, out] size of buffer, returned needed size of buffer including zero terminator
    );

    /**
    *******************************************************************************
    *   AMFTraceEnableWriter
    *
    *   @brief
    *       Disable trace to registered writer
    *
    *       Returns previous state
    *******************************************************************************
    */
    AMF_CORE_LINK bool AMF_CDECL_CALL  AMFTraceEnableWriter(const wchar_t* writerID, bool enable);

    /**
    *******************************************************************************
    *   AMFTraceWriterEnabled
    *
    *   @brief
    *       Return flag if writer enabled
    *******************************************************************************
    */
    AMF_CORE_LINK bool AMF_CDECL_CALL  AMFTraceWriterEnabled(const wchar_t* writerID);

    /**
    *******************************************************************************
    *   AMFTraceSetGlobalLevel
    *
    *   @brief
    *       Sets trace level for writer and scope
    *
    *       Returns previous setting
    *******************************************************************************
    */
    AMF_CORE_LINK amf_int32 AMF_CDECL_CALL  AMFTraceSetGlobalLevel(amf_int32 level); 

    /**
    *******************************************************************************
    *   AMFTraceGetGlobalLevel
    *
    *   @brief
    *       Returns global level
    *******************************************************************************
    */
    AMF_CORE_LINK amf_int32 AMF_CDECL_CALL  AMFTraceGetGlobalLevel();

    /**
    *******************************************************************************
    *   AMFTraceSetWriterLevel
    *
    *   @brief
    *       Sets trace level for writer 
    *
    *       Returns previous setting
    *******************************************************************************
    */
    AMF_CORE_LINK amf_int32 AMF_CDECL_CALL  AMFTraceSetWriterLevel(const wchar_t* writerID, amf_int32 level); 

    /**
    *******************************************************************************
    *   AMFTraceGetWriterLevel
    *
    *   @brief
    *       Gets trace level for writer 
    *******************************************************************************
    */
    AMF_CORE_LINK amf_int32 AMF_CDECL_CALL  AMFTraceGetWriterLevel(const wchar_t* writerID); 

    /**
    *******************************************************************************
    *   AMFTraceSetWriterLevelForScope
    *
    *   @brief
    *       Sets trace level for writer and scope
    *
    *       Returns previous setting
    *******************************************************************************
    */
    AMF_CORE_LINK amf_int32 AMF_CDECL_CALL  AMFTraceSetWriterLevelForScope(const wchar_t* writerID, const wchar_t* scope, amf_int32 level); 

    /**
    *******************************************************************************
    *   AMFTraceGetWriterLevelForScope
    *
    *   @brief
    *       Gets trace level for writer and scope
    *******************************************************************************
    */
    AMF_CORE_LINK amf_int32 AMF_CDECL_CALL  AMFTraceGetWriterLevelForScope(const wchar_t* writerID, const wchar_t* scope); 

    /**
    *******************************************************************************
    *   AMFTraceRegisterWriter
    *
    *   @brief
    *       Register custom trace writer
    *
    *******************************************************************************
    */
    AMF_CORE_LINK void AMF_CDECL_CALL  AMFTraceRegisterWriter(const wchar_t* writerID, AMFTraceWriter* pWriter);

    /**
    *******************************************************************************
    *   AMFTraceUnregisterWriter
    *
    *   @brief
    *       Register custom trace writer
    *
    *******************************************************************************
    */
    AMF_CORE_LINK void AMF_CDECL_CALL  AMFTraceUnregisterWriter(const wchar_t* writerID);

    /**
    *******************************************************************************
    *   AMFEnablePerformanceMonitor
    *
    *   @brief
    *       Enable Performance Monitor logging
    *
    *******************************************************************************
    */
    AMF_CORE_LINK void AMF_CDECL_CALL  AMFEnablePerformanceMonitor(bool enable);

    /**
    *******************************************************************************
    *   AMFPerformanceMonitorEnabled(
    *
    *   @brief
    *       Returns true if Performance Monitor logging is enabled
    *
    *******************************************************************************
    */
    AMF_CORE_LINK bool AMF_CDECL_CALL  AMFPerformanceMonitorEnabled();

    /**
    *******************************************************************************
    *   AMFAssertsEnable
    *
    *   @brief
    *       Enable asserts in checks
    *
    *******************************************************************************
    */
    AMF_CORE_LINK void AMF_CDECL_CALL  AMFAssertsEnable(bool enable);

    /**
    *******************************************************************************
    *   AMFAssertsEnabled
    *
    *   @brief
    *       Returns true if asserts in checks enabled
    *
    *******************************************************************************
    */
    AMF_CORE_LINK bool AMF_CDECL_CALL  AMFAssertsEnabled();

} // extern "C"

}

#endif // __AMFDebug_h__
