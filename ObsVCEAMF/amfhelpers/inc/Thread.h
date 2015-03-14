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
 * @file <Thread.h>
 *
 * @brief Header file for thread creation and handling
 *
 *******************************************************************************
 */

#pragma once


#include "AMFPlatform.h"
#include <list>

//----------------------------------------------------------------
class AMFSyncBase
{
public:
    virtual ~AMFSyncBase(){}

    virtual bool Lock(amf_ulong ulTimeout = AMF_INFINITE) = 0;
    virtual bool Unlock() = 0;
};
//----------------------------------------------------------------
class AMFEvent : public AMFSyncBase
{
private:
    amf_handle m_hSyncObject;

    AMFEvent(const AMFEvent&);
    AMFEvent& operator=(const AMFEvent&);

public:
    AMFEvent(bool bInitiallyOwned = false, bool bManualReset = false, const wchar_t* pName = NULL);
    virtual ~AMFEvent();

    virtual bool Lock(amf_ulong ulTimeout = AMF_INFINITE);
    virtual bool LockTimeout(amf_ulong ulTimeout = AMF_INFINITE);
    virtual bool Unlock();
    bool SetEvent();
    bool ResetEvent();
};
//----------------------------------------------------------------
class AMFMutex : public AMFSyncBase
{
private:
    amf_handle m_hSyncObject;

    AMFMutex(const AMFMutex&);
    AMFMutex& operator=(const AMFMutex&);

public:
    AMFMutex(bool bInitiallyOwned = false, const wchar_t* pName = NULL
    #if defined(_WIN32)
        , bool bOpenExistent = false
    #endif
        );
    virtual ~AMFMutex();

    virtual bool Lock(amf_ulong ulTimeout = AMF_INFINITE);
    virtual bool Unlock();
    bool IsValid();
};
//----------------------------------------------------------------
class AMFCriticalSection : public AMFSyncBase
{
private:
    amf_handle m_Sect;

    AMFCriticalSection(const AMFCriticalSection&);
    AMFCriticalSection& operator=(const AMFCriticalSection&);

public:
    AMFCriticalSection();
    virtual ~AMFCriticalSection();

    virtual bool Lock(amf_ulong ulTimeout = AMF_INFINITE);
    virtual bool Unlock();
};
//----------------------------------------------------------------
class AMFSemaphore : public AMFSyncBase
{
private:
    amf_handle m_hSemaphore;

    AMFSemaphore(const AMFSemaphore&);
    AMFSemaphore& operator=(const AMFSemaphore&);

public:
    AMFSemaphore(amf_long iInitCount, amf_long iMaxCount, const wchar_t* pName = NULL);
    virtual ~AMFSemaphore();

    virtual bool Create(amf_long iInitCount, amf_long iMaxCount, const wchar_t* pName = NULL);
    virtual bool Lock(amf_ulong ulTimeout = AMF_INFINITE);
    virtual bool Unlock();
};
//----------------------------------------------------------------
class AMFLock
{
private:
    AMFSyncBase* m_pBase;
    bool m_bLocked;

    AMFLock(const AMFLock&);
    AMFLock& operator=(const AMFLock&);

public:
    AMFLock(AMFSyncBase* pBase, amf_ulong ulTimeout = AMF_INFINITE);
    ~AMFLock();

    bool Lock(amf_ulong ulTimeout = AMF_INFINITE);
    bool Unlock();
    bool IsLocked();
};
//----------------------------------------------------------------
class AMFThreadObj;
class AMFThread
{
public:
    AMFThread();
    virtual ~AMFThread();

    virtual bool Start();
    virtual bool RequestStop();
    virtual bool WaitForStop();
    virtual bool StopRequested();

    // this is executed in the thread and overloaded by implementor
    virtual void Run() = 0;
    virtual bool Init();
    virtual bool Terminate();
private:
    AMFThreadObj* m_thread;

    AMFThread(const AMFThread&);
    AMFThread& operator=(const AMFThread&);
};

//----------------------------------------------------------------
template<typename T>
class AMFQueue
{
protected:
    class ItemData
    {
    public:
        T data;
        amf_ulong ulID;
        ItemData() : data(), ulID(0) {}
    };
    typedef std::list< ItemData > QueueList;

    QueueList           m_Queue;
    AMFCriticalSection  m_cSect;
    AMFEvent            m_SomethingInQueueEvent;
    AMFSemaphore        m_QueueSizeSem;
    amf_int32           m_iQueueSize;

    bool InternalGet(amf_ulong& ulID, T& item)
    {
        AMFLock lock(&m_cSect);
        if(!m_Queue.empty())  // something to get
        {
            ItemData& itemdata = m_Queue.front();
            ulID = itemdata.ulID;
            item = itemdata.data;
            m_Queue.pop_front();
            m_QueueSizeSem.Unlock();
            if(m_Queue.empty())
            {
                m_SomethingInQueueEvent.ResetEvent();
            }
            return true;
        }
        return false;
    }
public:
    AMFQueue(amf_int32 iQueueSize = 0) :
        m_Queue(),
        m_cSect(),
        m_SomethingInQueueEvent(false, false),
        m_QueueSizeSem(iQueueSize, iQueueSize > 0 ? iQueueSize + 1 : 0),
        m_iQueueSize(iQueueSize) {}
    virtual ~AMFQueue(){}

    virtual bool SetQueueSize(amf_int32 iQueueSize)
    {
        bool success = m_QueueSizeSem.Create(iQueueSize, iQueueSize > 0 ? iQueueSize + 1 : 0);
        if(success)
        {
            m_iQueueSize = iQueueSize;
        }
        return success;
    }
    virtual amf_int32 GetQueueSize()
    {
        return m_iQueueSize;
    }
    virtual bool Add(amf_ulong ulID, const T& item, amf_ulong ulTimeout = AMF_INFINITE)
    {
        if(m_QueueSizeSem.Lock(ulTimeout) == false)
        {
            return false;
        }
        {
            AMFLock lock(&m_cSect);

            ItemData itemdata;
            itemdata.ulID = ulID;
            itemdata.data = item;

            m_Queue.push_back(itemdata);
            m_SomethingInQueueEvent.SetEvent(); // this will set all waiting threads - some of them get data, some of them not
        }
        return true;
    }

    virtual bool Get(amf_ulong& ulID, T& item, amf_ulong ulTimeout)
    {
        if(InternalGet(ulID, item))  // try right away
        {
            return true;
        }
        // wait for queue
        if(m_SomethingInQueueEvent.Lock(ulTimeout))
        {
            return InternalGet(ulID, item);
        }
        return false;
    }
    virtual void Clear()
    {
        while(true)
        {
            amf_ulong ulID;
            T item;
            if(!InternalGet(ulID, item))
            {
                break;
            }
        }
    }
    virtual amf_size GetSize()
    {
        AMFLock lock(&m_cSect);
        return m_Queue.size();
    }
};

class AMFPreciseWaiter
{
public:
    AMFPreciseWaiter() : m_WaitEvent(), m_bCancel(false)
    {}
    virtual ~AMFPreciseWaiter()
    {}
    amf_pts Wait(amf_pts waittime)
    {
        m_bCancel = false;
        amf_pts start = amf_high_precision_clock();
        amf_pts waited = 0;
        while(!m_bCancel)
        {
            if(!m_WaitEvent.LockTimeout(1))
            {
                break;
            }
            waited = amf_high_precision_clock() - start;
            if(waited >= waittime)
            {
                break;
            }
        }
        return waited;
    }
    void Cancel()
    {
        m_bCancel = true;
    }
protected:
    AMFEvent m_WaitEvent;
    bool m_bCancel;
};
