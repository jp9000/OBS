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
 * @file <PlatformWindows.cpp>
 *
 * @brief Source file for Platform Windows
 *
 *******************************************************************************
 */
#include "stdafx.h"
#include "..\inc\PlatformWindows.h"

#include <process.h>

#pragma warning(disable: 4996)

//----------------------------------------------------------------------------------------
// threading
//----------------------------------------------------------------------------------------
amf_long AMF_STD_CALL amf_atomic_inc(amf_long* X)
{
    return InterlockedIncrement((long*)X);
}
//----------------------------------------------------------------------------------------
amf_long AMF_STD_CALL amf_atomic_dec(amf_long* X)
{
    return InterlockedDecrement((long*)X);
}
//----------------------------------------------------------------------------------------
amf_handle AMF_STD_CALL amf_create_critical_section()
{
    CRITICAL_SECTION* cs = new CRITICAL_SECTION;
    ::InitializeCriticalSection(cs);
    return (amf_handle)cs; // in Win32 - no errors
}
//----------------------------------------------------------------------------------------
bool AMF_STD_CALL amf_delete_critical_section(amf_handle cs)
{
    ::DeleteCriticalSection((CRITICAL_SECTION*)cs);
    delete (CRITICAL_SECTION*)cs;
    return true; // in Win32 - no errors
}
//----------------------------------------------------------------------------------------
bool AMF_STD_CALL amf_enter_critical_section(amf_handle cs)
{
    ::EnterCriticalSection((CRITICAL_SECTION*)cs);
    return true; // in Win32 - no errors
}
//----------------------------------------------------------------------------------------
bool AMF_STD_CALL amf_leave_critical_section(amf_handle cs)
{
    ::LeaveCriticalSection((CRITICAL_SECTION*)cs);
    return true; // in Win32 - no errors
}
//----------------------------------------------------------------------------------------
amf_handle AMF_STD_CALL amf_create_event(bool bInitiallyOwned, bool bManualReset, const wchar_t* pName)
{
    return ::CreateEvent(NULL, bManualReset == true, bInitiallyOwned == true, pName);
}
//----------------------------------------------------------------------------------------
bool AMF_STD_CALL amf_delete_event(amf_handle hevent)
{
    return ::CloseHandle(hevent) != FALSE;
}
//----------------------------------------------------------------------------------------
bool AMF_STD_CALL amf_set_event(amf_handle hevent)
{
    return ::SetEvent(hevent) != FALSE;
}
//----------------------------------------------------------------------------------------
bool AMF_STD_CALL amf_reset_event(amf_handle hevent)
{
    return ::ResetEvent(hevent) != FALSE;
}
//----------------------------------------------------------------------------------------
bool AMF_STD_CALL amf_wait_for_event(amf_handle hevent, amf_ulong ulTimeout)
{
    return ::WaitForSingleObject(hevent, ulTimeout) == WAIT_OBJECT_0;
}
//----------------------------------------------------------------------------------------
bool AMF_STD_CALL amf_wait_for_event_timeout(amf_handle hevent, amf_ulong ulTimeout)
{
    DWORD ret;
    ret = ::WaitForSingleObject(hevent, ulTimeout);
    return ret == WAIT_OBJECT_0 || ret == WAIT_TIMEOUT;
}
//----------------------------------------------------------------------------------------
amf_handle AMF_STD_CALL amf_create_mutex(bool bInitiallyOwned, const wchar_t* pName)
{
    return ::CreateMutex(NULL, bInitiallyOwned == true, pName);
}
//----------------------------------------------------------------------------------------
amf_handle AMF_STD_CALL amf_open_mutex(const wchar_t* pName)
{
    return ::OpenMutex(MUTEX_ALL_ACCESS, FALSE, pName);
}
//----------------------------------------------------------------------------------------
bool AMF_STD_CALL amf_delete_mutex(amf_handle hmutex)
{
    return ::CloseHandle(hmutex) != FALSE;
}
//----------------------------------------------------------------------------------------
bool AMF_STD_CALL amf_wait_for_mutex(amf_handle hmutex, amf_ulong ulTimeout)
{
    return ::WaitForSingleObject(hmutex, ulTimeout) == WAIT_OBJECT_0;
}
//----------------------------------------------------------------------------------------
bool AMF_STD_CALL amf_release_mutex(amf_handle hmutex)
{
    return ::ReleaseMutex(hmutex) != FALSE;
}
//----------------------------------------------------------------------------------------
amf_handle AMF_STD_CALL amf_create_semaphore(amf_long iInitCount, amf_long iMaxCount, const wchar_t* pName)
{
    if(iMaxCount == NULL)
    {
        return NULL;
    }
    return ::CreateSemaphoreW(NULL, iInitCount, iMaxCount, pName);
}
//----------------------------------------------------------------------------------------
bool AMF_STD_CALL amf_delete_semaphore(amf_handle hsemaphore)
{
    if(hsemaphore == NULL)
    {
        return true;
    }
    return ::CloseHandle(hsemaphore) != FALSE;
}
//----------------------------------------------------------------------------------------
bool AMF_STD_CALL amf_wait_for_semaphore(amf_handle hsemaphore, amf_ulong timeout)
{
    if(hsemaphore == NULL)
    {
        return true;
    }
    return ::WaitForSingleObject(hsemaphore, timeout) == WAIT_OBJECT_0;
}
//----------------------------------------------------------------------------------------
bool AMF_STD_CALL amf_release_semaphore(amf_handle hsemaphore, amf_long iCount, amf_long* iOldCount)
{
    if(hsemaphore == NULL)
    {
        return true;
    }
    return ::ReleaseSemaphore(hsemaphore, iCount, iOldCount) != FALSE;
}
//------------------------------------------------------------------------------
void AMF_STD_CALL amf_sleep(amf_ulong delay)
{
    Sleep(delay);
}
//----------------------------------------------------------------------------------------
amf_pts AMF_STD_CALL amf_high_precision_clock()
{
    static int state = 0;
    static LARGE_INTEGER Frequency;
    if(state == 0)
    {
        if(QueryPerformanceFrequency(&Frequency))
        {
            state = 1;
        }
        else
        {
            state = 2;
        }
    }
    if(state == 1)
    {
        LARGE_INTEGER PerformanceCount;
        if(QueryPerformanceCounter(&PerformanceCount))
        {
            return static_cast<amf_pts>(PerformanceCount.QuadPart * 10000000.0 / Frequency.QuadPart);
        }
    }
    return GetTickCount() * 10;
}

void AMF_STD_CALL amf_increase_timer_precision()
{
    typedef NTSTATUS (CALLBACK * NTSETTIMERRESOLUTION)(IN ULONG DesiredTime,IN BOOLEAN SetResolution,OUT PULONG ActualTime);
    typedef NTSTATUS (CALLBACK * NTQUERYTIMERRESOLUTION)(OUT PULONG MaximumTime,OUT PULONG MinimumTime,OUT PULONG CurrentTime);

    HINSTANCE hNtDll = LoadLibrary(L"NTDLL.dll");
    if(hNtDll != NULL)
    {
        ULONG MinimumResolution=0;
        ULONG MaximumResolution=0;
        ULONG ActualResolution=0;

        NTQUERYTIMERRESOLUTION NtQueryTimerResolution = (NTQUERYTIMERRESOLUTION)GetProcAddress(hNtDll, "NtQueryTimerResolution");
        NTSETTIMERRESOLUTION NtSetTimerResolution = (NTSETTIMERRESOLUTION)GetProcAddress(hNtDll, "NtSetTimerResolution");

        if(NtQueryTimerResolution != NULL && NtSetTimerResolution != NULL)
        {
            NtQueryTimerResolution (&MinimumResolution, &MaximumResolution, &ActualResolution);
            if(MaximumResolution != 0)
            {
                NtSetTimerResolution (MaximumResolution, TRUE, &ActualResolution);
                NtQueryTimerResolution (&MinimumResolution, &MaximumResolution, &ActualResolution);

                // if call NtQueryTimerResolution() again it will return the same values but precision is actually increased
            }
        }
        FreeLibrary(hNtDll);
    }
}

bool AMF_STD_CALL amf_path_is_relative(const wchar_t* const path)
{
    if(!path)
    {
        return true;
    }
    std::wstring pathStr(path);
    if(pathStr.empty())
    {
        return true;
    }
    amf_size pos = pathStr.find(L":");
    if(pos != std::wstring::npos)
    {
        return false;
    }
    pos = pathStr.find(L"\\\\");
    if(!pos)
    {
        return false;
    }
    return true;
}
#if defined(_WIN32)

//----------------------------------------------------------------------------
AMFThreadObj::AMFThreadObj(AMFThread* owner)
    : m_pThread(-1L),
    m_StopEvent(true, true), m_pOwner(owner)
{}
//----------------------------------------------------------------------------
AMFThreadObj::~AMFThreadObj()
{
    //    RequestStop();
    //    WaitForStop();
}
//----------------------------------------------------------------------------
void AMF_CDECL_CALL AMFThreadObj::AMFThreadProc(void* pThis)
{
    AMFThreadObj* pT = (AMFThreadObj*)pThis;
    if(!pT->Init())
    {
        return;
    }
    pT->Run();
    pT->Terminate();

    pT->m_pThread = -1;
    if(pT->StopRequested())
    {
        pT->m_StopEvent.SetEvent(); // signal to stop that we just finished
    }
}
//----------------------------------------------------------------------------
bool AMFThreadObj::Start()
{
    if(m_pThread != (uintptr_t)-1L)
    {
        return true;
    }

    m_pThread = _beginthread(AMFThreadProc, 0, (void* )this);

    return m_pThread != (uintptr_t)-1L;
}

//----------------------------------------------------------------------------
bool AMFThreadObj::RequestStop()
{
    if(m_pThread == (uintptr_t)-1L)
    {
        return true;
    }

    m_StopEvent.ResetEvent();
    return true;
}
//----------------------------------------------------------------------------
bool AMFThreadObj::WaitForStop()
{
    if(m_pThread == (uintptr_t)-1L)
    {
        return true;
    }
    return m_StopEvent.Lock();
}
//----------------------------------------------------------------------------
bool AMFThreadObj::StopRequested()
{
    return !m_StopEvent.Lock(0);
}

#endif
bool AMF_STD_CALL amf_get_application_data_path(wchar_t* path, amf_size buflen)
{
    wchar_t* envvar = _wgetenv(L"APPDATA");
    if(envvar == 0)
    {
        return false;
    }
    const wchar_t suffix[] = L"\\AMD\\";
    if(wcslen(envvar) + 1 + wcslen(suffix) > buflen)
    {
        return false;
    }
    wcscpy(path, envvar);
    wcscat(path, suffix);
    return true;
}

AMFDataStreamPtr AMFDataStream::Create(const wchar_t* path, AMF_FileAccess fileaccess)
{
    AMFDataStreamFile* ret = new AMFDataStreamFile;
    if(ret->Open(path, fileaccess) == false)
    {
        delete ret;
        ret = NULL;
    }
    return AMFDataStreamPtr(ret);
}

AMFDataStreamFile::AMFDataStreamFile()
    :m_fileHandle(INVALID_HANDLE_VALUE)
{

}

AMFDataStreamFile::~AMFDataStreamFile()
{
    if(m_fileHandle == INVALID_HANDLE_VALUE)
    {
        return;
    }
    CloseHandle(m_fileHandle);
}

bool AMFDataStreamFile::Open(const wchar_t* path, AMF_FileAccess fileaccess)
{
    if(m_fileHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_fileHandle);
        m_fileHandle = INVALID_HANDLE_VALUE;
        return 0;
    }
    DWORD     desiredAccess = 0;
    DWORD     createDisposition = OPEN_EXISTING;
    DWORD     flags = 0;

    // Get the desire access
    if(fileaccess & AMF_FileWrite)
    {
        desiredAccess |= GENERIC_WRITE;

        if(fileaccess & AMF_FileAppend)
        {
            createDisposition = OPEN_ALWAYS;
        }
        else
        {
            createDisposition = CREATE_ALWAYS;
        }
    }

    if(fileaccess & AMF_FileRead)
    {
        desiredAccess |= GENERIC_READ;
    }

    if(fileaccess & AMF_FileRandom)
    {
        // Optimizes for random access
        flags |= FILE_FLAG_RANDOM_ACCESS;
    }

    // Open the file
    m_fileHandle = CreateFile(path, desiredAccess, FILE_SHARE_READ, NULL, createDisposition, 0, NULL);
    DWORD err = GetLastError();
    return m_fileHandle != INVALID_HANDLE_VALUE;
}

amf_int64 AMFDataStreamFile::Seek(AMF_FileSeekOrigin eOrigin, amf_int64 iPosition)
{
    if(m_fileHandle == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    DWORD origin = 0;
    switch( eOrigin )
    {
    case AMF_FileSeekBegin           : origin=FILE_BEGIN;    break;
    case AMF_FileSeekCurrent         : origin=FILE_CURRENT;    break;
    case AMF_FileSeekEnd             : origin=FILE_END;    break;
    }
    LARGE_INTEGER position;
    position.QuadPart=iPosition;
    LARGE_INTEGER newPosition;
    BOOL result = SetFilePointerEx(m_fileHandle, position, &newPosition, origin);
    if(result)
    {
        return newPosition.QuadPart;
    }
    return -1;
}

amf_size AMFDataStreamFile::Read(void* pData, amf_size dataSize)
{
    if(m_fileHandle == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    DWORD bytesRead = 0;
    ::ReadFile(m_fileHandle, pData, (DWORD)dataSize, &bytesRead, NULL);

    return static_cast<amf_size>(bytesRead);
}

amf_size AMFDataStreamFile::Write(const void* pData, amf_size dataSize)
{
    if(m_fileHandle == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    DWORD bytesWritten = 0;
    ::WriteFile(m_fileHandle, pData, (DWORD)dataSize, &bytesWritten, NULL);
    
    return static_cast<amf_size>(bytesWritten);
}

amf_int64 AMFDataStreamFile::Size()
{
    amf_int64 currentPos = Seek(AMF_FileSeekCurrent, 0);
    amf_int64 fileSize = Seek(AMF_FileSeekEnd, 0);
    Seek(AMF_FileSeekBegin, currentPos);
    return fileSize;
}

amf_int64 AMFDataStreamFile::Position()
{
    return Seek(AMF_FileSeekCurrent, 0);
}

AMFDataStreamMemory::AMFDataStreamMemory()
    :m_data(0),
    m_size(0),
    m_position(0)
{
}
AMFDataStreamMemory::~AMFDataStreamMemory()
{
    if(m_data)
    {
        free(m_data);
    }
}

bool AMFDataStreamMemory::Open(void* data, amf_size size)
{
    if(m_data)
    {
        free(m_data);
        m_data = 0;
    }
    m_data = (amf_uint8*)malloc((size_t)size);
    memcpy(m_data, data, (size_t)size);
    m_size = size;
    m_position = 0;
    return true;
}
amf_int64 AMFDataStreamMemory::Seek(AMF_FileSeekOrigin eOrigin, amf_int64 iPosition)
{
    switch (eOrigin)
    {
    case AMF_FileSeekBegin:
        m_position = amf_size(iPosition);
        break;
    case AMF_FileSeekCurrent:
        m_position += amf_size(iPosition);
        break;
    case AMF_FileSeekEnd:
        m_position = amf_size(m_size - iPosition);
        break;
    default:
        break;
    }
    return iPosition;
}

amf_size AMFDataStreamMemory::Read(void* pData, amf_size dataSize)
{
    amf_size ret_size = dataSize;
    if(m_position + dataSize > m_size)
    {
        ret_size = m_size - m_position;
    }
    if(ret_size)
    {
        memcpy(pData, m_data + m_position, ret_size);
        m_position += ret_size;
    }
    return ret_size;
}

amf_size AMFDataStreamMemory::Write(const void* pData, amf_size dataSize)
{
    amf_size ret_size = dataSize;
    if(m_position + dataSize > m_size)
    {
        ret_size = m_size - m_position;
    }
    if(ret_size)
    {
        memcpy(m_data + m_position, pData, ret_size);
        m_position += ret_size;
    }
    return ret_size;
}

amf_int64 AMFDataStreamMemory::Size()
{
    return amf_int64(m_size);
}

amf_int64 AMFDataStreamMemory::Position()
{
    return amf_int64(m_position);
}

AMFDataStreamPtr AMFDataStream::Create(void* data, amf_size size)
{
    AMFDataStreamMemory* ret = new AMFDataStreamMemory;
    if(ret->Open(data, size) == false)
    {
        delete ret;
        ret = NULL;
    }
    return AMFDataStreamPtr(ret);
}

