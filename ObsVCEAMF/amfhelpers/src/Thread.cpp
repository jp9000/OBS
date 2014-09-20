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
 * @file <Thread.cpp>
 *
 * @brief Source file for Thread creation and handling
 *
 *******************************************************************************
 */
#include "stdafx.h"
#include "..\inc\Thread.h"

#if defined(_WIN32)
    #include "..\inc\PlatformWindows.h"
#endif

//----------------------------------------------------------------------------
AMFEvent::AMFEvent(bool bInitiallyOwned, bool bManualReset, const wchar_t* pName) : m_hSyncObject()
{
    m_hSyncObject = amf_create_event(bInitiallyOwned, bManualReset, pName);
}
//----------------------------------------------------------------------------
AMFEvent::~AMFEvent()
{
    amf_delete_event(m_hSyncObject);
}
//----------------------------------------------------------------------------
bool AMFEvent::Lock(amf_ulong ulTimeout)
{
    return amf_wait_for_event(m_hSyncObject, ulTimeout);
}
//----------------------------------------------------------------------------
bool AMFEvent::LockTimeout(amf_ulong ulTimeout)
{
    return amf_wait_for_event_timeout(m_hSyncObject, ulTimeout);
}
//----------------------------------------------------------------------------
bool AMFEvent::Unlock()
{
    return true;
}
//----------------------------------------------------------------------------
bool AMFEvent::SetEvent()
{
    return amf_set_event(m_hSyncObject);
}
//----------------------------------------------------------------------------
bool AMFEvent::ResetEvent()
{
    return amf_reset_event(m_hSyncObject);
}
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
AMFMutex::AMFMutex(bool bInitiallyOwned, const wchar_t* pName
                #if defined(_WIN32)
                    , bool bOpenExistent
                #endif
                    ):m_hSyncObject()
{
#if defined(_WIN32)
    if(bOpenExistent)
    {
        m_hSyncObject = amf_open_mutex(pName);
    }
    else
#endif
    {
        m_hSyncObject = amf_create_mutex(bInitiallyOwned, pName);
    }
}
//----------------------------------------------------------------------------
AMFMutex::~AMFMutex()
{
    if(m_hSyncObject)
    {
        amf_delete_mutex(m_hSyncObject);
    }
}
//----------------------------------------------------------------------------
bool AMFMutex::Lock(amf_ulong ulTimeout)
{
    if(m_hSyncObject)
    {
        return amf_wait_for_mutex(m_hSyncObject, ulTimeout);
    }
    else
    {
        return false;
    }
}
//----------------------------------------------------------------------------
bool AMFMutex::Unlock()
{
    if(m_hSyncObject)
    {
        return amf_release_mutex(m_hSyncObject);
    }
    else
    {
        return false;
    }
}
//----------------------------------------------------------------------------
bool AMFMutex::IsValid()
{
    return m_hSyncObject != NULL;
}
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
AMFCriticalSection::AMFCriticalSection() : m_Sect()
{
    m_Sect = amf_create_critical_section();
}
//----------------------------------------------------------------------------
AMFCriticalSection::~AMFCriticalSection()
{
    amf_delete_critical_section(m_Sect);
}
//----------------------------------------------------------------------------
bool AMFCriticalSection::Lock(amf_ulong ulTimeout)
{
    (void)ulTimeout;
    return amf_enter_critical_section(m_Sect);
}
//----------------------------------------------------------------------------
bool AMFCriticalSection::Unlock()
{
    return amf_leave_critical_section(m_Sect);
}
//----------------------------------------------------------------------------
AMFSemaphore::AMFSemaphore(amf_long iInitCount, amf_long iMaxCount, const wchar_t* pName)
    : m_hSemaphore(NULL)
{
    Create(iInitCount, iMaxCount, pName);
}
//----------------------------------------------------------------------------
AMFSemaphore::~AMFSemaphore()
{
    amf_delete_semaphore(m_hSemaphore);
}
//----------------------------------------------------------------------------
bool AMFSemaphore::Create(amf_long iInitCount, amf_long iMaxCount, const wchar_t* pName)
{
    if(m_hSemaphore != NULL)  // delete old one
    {
        amf_delete_semaphore(m_hSemaphore);
        m_hSemaphore = NULL;
    }
    if(iMaxCount > 0)
    {
        m_hSemaphore = amf_create_semaphore(iInitCount, iMaxCount, pName);
    }
    return true;
}
//----------------------------------------------------------------------------
bool AMFSemaphore::Lock(amf_ulong ulTimeout)
{
    return amf_wait_for_semaphore(m_hSemaphore, ulTimeout);
}
//----------------------------------------------------------------------------
bool AMFSemaphore::Unlock()
{
    amf_long iOldCount = 0;
    return amf_release_semaphore(m_hSemaphore, 1, &iOldCount);
}
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
AMFLock::AMFLock(AMFSyncBase* pBase, amf_ulong ulTimeout)
    : m_pBase(pBase),
    m_bLocked()
{
    m_bLocked = Lock(ulTimeout);
}
//----------------------------------------------------------------------------
AMFLock::~AMFLock()
{
    Unlock();
}
//----------------------------------------------------------------------------
bool AMFLock::Lock(amf_ulong ulTimeout)
{
    if(m_pBase == NULL)
    {
        return false;
    }
    m_bLocked = m_pBase->Lock(ulTimeout);
    return m_bLocked;
}
//----------------------------------------------------------------------------
bool AMFLock::Unlock()
{
    if(m_pBase == NULL)
    {
        return false;
    }
    m_bLocked = m_pBase->Unlock();
    return m_bLocked;
}
//----------------------------------------------------------------------------
bool AMFLock::IsLocked()
{
    return m_bLocked;
}
//----------------------------------------------------------------------------

AMFThread::AMFThread() : m_thread()
{
    m_thread = new AMFThreadObj(this);
}

AMFThread::~AMFThread()
{
    delete m_thread;
}

bool AMFThread::Start()
{
    return m_thread->Start();
}

bool AMFThread::RequestStop()
{
    return m_thread->RequestStop();
}

bool AMFThread::WaitForStop()
{
    return m_thread->WaitForStop();
}

bool AMFThread::StopRequested()
{
    return m_thread->StopRequested();
}

bool AMFThread::Init()
{
    return true;
}

bool AMFThread::Terminate()
{
    return true;
}
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
