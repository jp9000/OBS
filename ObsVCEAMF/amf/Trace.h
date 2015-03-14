///-------------------------------------------------------------------------
///  Trade secret of Advanced Micro Devices, Inc.
///  Copyright 2013, Advanced Micro Devices, Inc., (unpublished)
///
///  All rights reserved.  This notice is intended as a precaution against
///  inadvertent publication and does not imply publication or any waiver
///  of confidentiality.  The year included in the foregoing notice is the
///  year of creation of the work.
///-------------------------------------------------------------------------
///  @file   Trace.h
///  @brief  Trace interface
///-------------------------------------------------------------------------
#ifndef __AMFTrace_h__
#define __AMFTrace_h__
#pragma once

#include "Typedefs.h"

namespace amf
{
    class AMFTraceListener
    {
    public:
        virtual ~AMFTraceListener(){}

        virtual void Write(const wchar_t* facility, const wchar_t* message) = 0;
    };

    AMF_CORE_LINK bool        AMF_STD_CALL    AMFRegisterTraceListener(AMFTraceListener* pListener);
    AMF_CORE_LINK bool        AMF_STD_CALL    AMFUnregisterTraceListener(AMFTraceListener* pListener);
};

#endif // __AMFTrace_h__
