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
 * @file <PlatformWindows.h>
 *
 * @brief Header file for Platform Windows
 *
 *******************************************************************************
 */

#pragma once

#include "AMFPlatform.h"
#include "Thread.h"

class AMFThreadObj
{
    AMFThread*      m_pOwner;
    uintptr_t       m_pThread;
    AMFEvent        m_StopEvent;
public:
    // this icalled by owner
    AMFThreadObj(AMFThread* owner);
    virtual ~AMFThreadObj();

    virtual bool Start();
    virtual bool RequestStop();
    virtual bool WaitForStop();
    virtual bool StopRequested();


protected:
    static void AMF_CDECL_CALL AMFThreadProc(void* pThis);

    // this is executed in the thread and overloaded by implementor
    virtual void Run()
    {
        m_pOwner->Run();
    }
    virtual bool Init()
    {
        return m_pOwner->Init();
    }
    virtual bool Terminate()
    {
        return m_pOwner->Terminate();
    }
};

class AMFDataStreamFile : public AMFDataStream
{
public:
    AMFDataStreamFile();
    virtual ~AMFDataStreamFile();

    bool Open(const wchar_t* path, AMF_FileAccess fileaccess);
    amf_int64 Seek(AMF_FileSeekOrigin eOrigin, amf_int64 iPosition);
    amf_size Read(void* pData, amf_size dataSize);
    amf_size Write(const void* pData, amf_size dataSize);
    virtual amf_int64 Size();
    virtual amf_int64 Position();
private:
    amf_handle m_fileHandle;
};

class AMFDataStreamMemory : public AMFDataStream
{
public:
    AMFDataStreamMemory();
    virtual ~AMFDataStreamMemory();

    bool Open(void* data, amf_size size);
    amf_int64 Seek(AMF_FileSeekOrigin eOrigin, amf_int64 iPosition);
    amf_size Read(void* pData, amf_size dataSize);
    amf_size Write(const void* pData, amf_size dataSize);
    virtual amf_int64 Size();
    virtual amf_int64 Position();
private:
    amf_uint8*      m_data;
    amf_size        m_size;
    amf_size        m_position;
};
