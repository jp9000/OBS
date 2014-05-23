/*******************************************************************************
Copyright ©2013 Advanced Micro Devices, Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1   Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
2   Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/
/**
********************************************************************************
* @file <PrintLog.h>
*
* @brief This file contains logging functions from Media SDK 1.0
*
********************************************************************************
*/
#ifndef PRINTLOG_H_
#define PRINTLOG_H_

#include <iostream>
#include <cstdarg>
#include "Typedef.h"
#include <windows.h>
#include <lmerr.h>
#include <tchar.h>
//OBSApi
__declspec(dllimport) void __cdecl   Log(const TCHAR *format, ...);

#define ERRMSGBUFFERSIZE 256

#define DISPLAYERROR(error) fxnDisplayError(error)

#define LOGIFFAILED(fp,hr,format,...) if (FAILED(hr)) \
{\
    LOG(fp, " Error code : 0x%x ", hr); \
    LOG(fp, format, ##__VA_ARGS__); \
    return hr;  \
}

#define LOGIFFAILEDAMF(fp,hr,format,...) if (hr != AMF_OK) \
{\
    LOG(fp, " Error code : 0x%x ", hr); \
    LOG(fp, format, ##__VA_ARGS__); \
    return hr;  \
}

//#define LOG(fp,format,...) dumpLog(fp,format,##__VA_ARGS__)
#define LOG(fp,format,...) Log(TEXT(format),##__VA_ARGS__)

#define RETURNIFFAILED(hr) if  (FAILED(hr)) \
{ \
    return hr; \
}
#define LOGHRESULT(hr, message) \
{ \
    std::wostringstream errorStream; \
    errorStream << message << L", hr: " << std::hex << hr << \
    L" in " << __FILEW__ << \
    L" at line " << std::dec << __LINE__ << std::endl; \
    OutputDebugString(errorStream.str().c_str()); \
}

#define EXCEPTIONTOHR(expression) \
{ \
    try \
    { \
    hr = S_OK; \
    expression; \
    } \
    catch (const CAtlException& e) \
    { \
    hr = e.m_hr; \
    } \
    catch (...) \
    { \
    hr = E_OUTOFMEMORY; \
    } \
}

/**
*******************************************************************************
*  @fn     dumpLog
*  @brief  Dumps the log to a file
*          usage example : dumpLog(fp,"%s %d","failed ",__LINE__);
*
*  @param[in] fp      : File pointer
*  @param[in] format  : Format of the output
*  @return void
*******************************************************************************
*/
inline void dumpLog(FILE *fp, char* format, ...)
{
    va_list arguments;
    char *buffer;
    if (fp != NULL)
    {
        va_start(arguments, format);
        uint32 len = _vscprintf(format, arguments) + 1;
        buffer = (char*)malloc(len * sizeof(char));
        vsprintf_s(buffer, len, format, arguments);
        fputs(buffer, fp);
        fflush(fp);
        free(buffer);
    }
}

/**
*******************************************************************************
*  @fn     fxnDisplayError
*  @brief  Display corresponding error message/code
*
*  @param[in] dwError : Error
*
*  @return void
*******************************************************************************
*/
inline void fxnDisplayError(DWORD dwError)
{
    DWORD ret = 0;
    HINSTANCE hInst = NULL;
    HLOCAL pBuffer;

    pBuffer = LocalAlloc(LMEM_ZEROINIT, ERRMSGBUFFERSIZE);

    /**************************************************************************
    * MSMQ errors only.                                                      *
    **************************************************************************/
    if (HRESULT_FACILITY(dwError) == FACILITY_MSMQ)
    {
        /**********************************************************************
        * MSMQ errors only (see winerror.h for facility info).               *
        * Load the MSMQ library containing the error message strings.        *
        **********************************************************************/
        hInst = LoadLibrary(TEXT("MQUTIL.DLL"));
        if (hInst != 0)
        {
            /******************************************************************
            * hInst not NULL if the library was successfully loaded.         *
            * Get the text string for a message definition                   *
            ******************************************************************/
            ret = FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_HMODULE |
                FORMAT_MESSAGE_IGNORE_INSERTS,
                hInst,
                dwError,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR)&pBuffer,
                ERRMSGBUFFERSIZE,
                NULL
                );
        }
    }
    /**************************************************************************
    * Network error.                                                         *
    **************************************************************************/
    else if (dwError >= NERR_BASE && dwError <= MAX_NERR)
    {
        /**********************************************************************
        * Load the library containing network messages.                      *
        **********************************************************************/
        hInst = LoadLibrary(TEXT("NETMSG.DLL"));
        if (hInst != 0)
        {
            ret = FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_HMODULE |
                FORMAT_MESSAGE_IGNORE_INSERTS,
                hInst,
                dwError,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR)&pBuffer,
                ERRMSGBUFFERSIZE,
                NULL
                );
        }
    }
    /**************************************************************************
    * Unknown message source.                                                *
    **************************************************************************/
    else
    {
        /**********************************************************************
        * Get the message string from the system.                            *
        **********************************************************************/
        ret = FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            hInst,
            dwError,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)&pBuffer,
            ERRMSGBUFFERSIZE,
            NULL
            );
    }


    /**************************************************************************
    * Display the string.                                                    *
    **************************************************************************/
    if (ret)
    {
        _tprintf(_TEXT("--------------------------------------------------\n"));
        _tprintf(_TEXT("ERRORMESSAGE: %s"), (LPTSTR)pBuffer);
        _tprintf(_TEXT("--------------------------------------------------\n"));
    }
    else
    {
        _tprintf(_TEXT("--------------------------------------------------\n"));
        _tprintf(_TEXT("ERRORNUMBER: 0x%x\n"), dwError);
        _tprintf(_TEXT("--------------------------------------------------\n"));
    }

    LocalFree(pBuffer);
}

#endif
