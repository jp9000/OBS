/*******************************************************************************
 Copyright ©2014 Advanced Micro Devices, Inc. All rights reserved.

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
 ******************************************************************************/

/**
 *******************************************************************************
 * @file <AMFPlatform.h>
 *
 * @brief Header file for Platform
 *
 *******************************************************************************
 */

#pragma once

#include <memory>
#include <string>
#include <iterator>
#include <cctype>
#include <algorithm>
#include "..\..\amf\core\Platform.h"

#if defined(_WIN32)
    #define PATH_SEPARATOR_WSTR         L"\\"
    #define PATH_SEPARATOR_WCHAR        L'\\'
#endif

#define AMF_MIN(a, b) ((a) < (b) ? (a) : (b))
#define AMF_MAX(a, b) ((a) > (b) ? (a) : (b))

#define amf_countof(x) (sizeof(x) / sizeof(x[0]))

// threads
#define AMF_INFINITE        (0xFFFFFFFF) // Infinite ulTimeout
#define AMF_SECOND          10000000L    // 1 second in 100 nanoseconds

// threads: atomic
amf_long AMF_STD_CALL amf_atomic_inc(amf_long* X);
amf_long AMF_STD_CALL amf_atomic_dec(amf_long* X);

// file system
bool AMF_STD_CALL amf_path_is_relative(const wchar_t* const path);

// threads: critical section
amf_handle AMF_STD_CALL amf_create_critical_section();
bool AMF_STD_CALL amf_delete_critical_section(amf_handle cs);
bool AMF_STD_CALL amf_enter_critical_section(amf_handle cs);
bool AMF_STD_CALL amf_leave_critical_section(amf_handle cs);
// threads: event
amf_handle AMF_STD_CALL amf_create_event(bool bInitiallyOwned, bool bManualReset, const wchar_t* pName);
bool AMF_STD_CALL amf_delete_event(amf_handle hevent);
bool AMF_STD_CALL amf_set_event(amf_handle hevent);
bool AMF_STD_CALL amf_reset_event(amf_handle hevent);
bool AMF_STD_CALL amf_wait_for_event(amf_handle hevent, amf_ulong ulTimeout);
bool AMF_STD_CALL amf_wait_for_event_timeout(amf_handle hevent, amf_ulong ulTimeout);

// threads: mutex
amf_handle AMF_STD_CALL amf_create_mutex(bool bInitiallyOwned, const wchar_t* pName);

#if defined(_WIN32)
amf_handle AMF_STD_CALL amf_open_mutex(const wchar_t* pName);
#endif

bool AMF_STD_CALL amf_delete_mutex(amf_handle hmutex);
bool AMF_STD_CALL amf_wait_for_mutex(amf_handle hmutex, amf_ulong ulTimeout);
bool AMF_STD_CALL amf_release_mutex(amf_handle hmutex);

// threads: semaphore
amf_handle AMF_STD_CALL amf_create_semaphore(amf_long iInitCount, amf_long iMaxCount, const wchar_t* pName);
bool AMF_STD_CALL amf_delete_semaphore(amf_handle hsemaphore);
bool AMF_STD_CALL amf_wait_for_semaphore(amf_handle hsemaphore, amf_ulong ulTimeout);
bool AMF_STD_CALL amf_release_semaphore(amf_handle hsemaphore, amf_long iCount, amf_long* iOldCount);

// threads: timers
void AMF_STD_CALL amf_sleep(amf_ulong delay);
amf_pts AMF_STD_CALL amf_high_precision_clock();
void AMF_STD_CALL amf_increase_timer_precision();

enum AMF_FileAccess{
    AMF_FileWrite   = 0x0001,
    AMF_FileRead    = 0x0002,
    AMF_FileAppend  = 0x0004,
    AMF_FileRandom  = 0x0008
};

enum AMF_FileSeekOrigin
{
    AMF_FileSeekBegin           =0,
    AMF_FileSeekCurrent         =1,
    AMF_FileSeekEnd             =2,
};

bool AMF_STD_CALL amf_get_application_data_path(wchar_t* path, amf_size buflen);

class AMFDataStream;
typedef std::shared_ptr<AMFDataStream> AMFDataStreamPtr;

class AMFDataStream
{
public:
    virtual ~AMFDataStream(){}

    static AMFDataStreamPtr Create(const wchar_t* path, AMF_FileAccess fileaccess);
    static AMFDataStreamPtr Create(void* data, amf_size size);

    virtual amf_int64 Size() = 0;
    virtual amf_int64 Position() = 0;

    virtual amf_int64 Seek(AMF_FileSeekOrigin eOrigin, amf_int64 iPosition) = 0;
    virtual amf_size Read(void* pData, amf_size dataSize) = 0;
    virtual amf_size Write(const void* pData, amf_size dataSize) = 0;
};

inline std::wstring toUpper(const std::wstring& str)
{
    std::wstring result;
    std::transform(str.begin(), str.end(), std::back_inserter(result), &std::toupper);
    return result;
}
