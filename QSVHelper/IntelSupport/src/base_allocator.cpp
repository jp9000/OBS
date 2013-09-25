/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2012 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include <assert.h>
#include <algorithm>
#include "base_allocator.h"

MFXFrameAllocator::MFXFrameAllocator()
{    
    pthis = this;
    Alloc = Alloc_;
    Lock  = Lock_;
    Free  = Free_;
    Unlock = Unlock_;
    GetHDL = GetHDL_;
}

MFXFrameAllocator::~MFXFrameAllocator()
{
}

mfxStatus MFXFrameAllocator::Alloc_(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.AllocFrames(request, response);
}

mfxStatus MFXFrameAllocator::Lock_(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.LockFrame(mid, ptr);
}

mfxStatus MFXFrameAllocator::Unlock_(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.UnlockFrame(mid, ptr);
}

mfxStatus MFXFrameAllocator::Free_(mfxHDL pthis, mfxFrameAllocResponse *response)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.FreeFrames(response);
}

mfxStatus MFXFrameAllocator::GetHDL_(mfxHDL pthis, mfxMemId mid, mfxHDL *handle)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.GetFrameHDL(mid, handle);
}

BaseFrameAllocator::BaseFrameAllocator()
{
}

BaseFrameAllocator::~BaseFrameAllocator()
{
}

mfxStatus BaseFrameAllocator::CheckRequestType(mfxFrameAllocRequest *request)
{
    if (0 == request)
        return MFX_ERR_NULL_PTR;

    // check that Media SDK component is specified in request 
    if ((request->Type & MEMTYPE_FROM_MASK) != 0)
        return MFX_ERR_NONE;
    else
        return MFX_ERR_UNSUPPORTED;
}

mfxStatus BaseFrameAllocator::AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    if (0 == request || 0 == response || 0 == request->NumFrameSuggested)
        return MFX_ERR_MEMORY_ALLOC; 

    if (MFX_ERR_NONE != CheckRequestType(request))
        return MFX_ERR_UNSUPPORTED;

    mfxStatus sts = MFX_ERR_NONE;

    if ( (request->Type & MFX_MEMTYPE_EXTERNAL_FRAME) && (request->Type & MFX_MEMTYPE_FROM_DECODE) )
    {
        // external decoder allocations
        std::list<UniqueResponse>::iterator it = 
            std::find_if( m_ExtResponses.begin()
                        , m_ExtResponses.end()
                        , UniqueResponse (*response, request->Info.CropW, request->Info.CropH, 0));

        if (it != m_ExtResponses.end())
        {
            // check if enough frames were allocated
            if (request->NumFrameMin > it->NumFrameActual)
                return MFX_ERR_MEMORY_ALLOC;

            it->m_refCount++;
            // return existing response
            *response = (mfxFrameAllocResponse&)*it;
        }
        else
        {
            sts = AllocImpl(request, response);
            if (sts == MFX_ERR_NONE)
            {
                m_ExtResponses.push_back(UniqueResponse(*response, request->Info.CropW, request->Info.CropH, request->Type & MEMTYPE_FROM_MASK));
            }
        }
    }
    else
    {
        // internal allocations

        // reserve space before allocation to avoid memory leak
        m_responses.push_back(mfxFrameAllocResponse());

        sts = AllocImpl(request, response);
        if (sts == MFX_ERR_NONE)
        {
            m_responses.back() = *response;
        }
        else
        {
            m_responses.pop_back();
        }
    }

    return sts;
}

mfxStatus BaseFrameAllocator::FreeFrames(mfxFrameAllocResponse *response)
{
    if (response == 0)
        return MFX_ERR_INVALID_HANDLE;

    mfxStatus sts = MFX_ERR_NONE;
    
    // check whether response is an external decoder response
    std::list<UniqueResponse>::iterator i = 
        std::find_if( m_ExtResponses.begin(), m_ExtResponses.end(), std::bind1st(IsSame(), *response));

    if (i != m_ExtResponses.end())
    {
        if ((--i->m_refCount) == 0)
        {
            sts = ReleaseResponse(response);
            m_ExtResponses.erase(i);
        }
        return sts;
    }

    // if not found so far, then search in internal responses
    std::list<mfxFrameAllocResponse>::iterator i2 = 
        std::find_if(m_responses.begin(), m_responses.end(), std::bind1st(IsSame(), *response));

    if (i2 != m_responses.end())
    {
        sts = ReleaseResponse(response);
        m_responses.erase(i2);
        return sts;
    }

    // not found anywhere, report an error
    return MFX_ERR_INVALID_HANDLE;
}

mfxStatus BaseFrameAllocator::Close()
{
    std::list<UniqueResponse> ::iterator i;
    for (i = m_ExtResponses.begin(); i!= m_ExtResponses.end(); i++)
    {
        ReleaseResponse(&*i);
    }
    m_ExtResponses.clear();

    std::list<mfxFrameAllocResponse> ::iterator i2;
    for (i2 = m_responses.begin(); i2!= m_responses.end(); i2++)
    {
        ReleaseResponse(&*i2);
    }

    return MFX_ERR_NONE;
}

MFXBufferAllocator::MFXBufferAllocator()
{    
    pthis = this;
    Alloc = Alloc_;
    Lock  = Lock_;
    Free  = Free_;
    Unlock = Unlock_;
}

MFXBufferAllocator::~MFXBufferAllocator()
{
}

mfxStatus MFXBufferAllocator::Alloc_(mfxHDL pthis, mfxU32 nbytes, mfxU16 type, mfxMemId *mid)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXBufferAllocator& self = *(MFXBufferAllocator *)pthis;

    return self.AllocBuffer(nbytes, type, mid);
}

mfxStatus MFXBufferAllocator::Lock_(mfxHDL pthis, mfxMemId mid, mfxU8 **ptr)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXBufferAllocator& self = *(MFXBufferAllocator *)pthis;

    return self.LockBuffer(mid, ptr);
}

mfxStatus MFXBufferAllocator::Unlock_(mfxHDL pthis, mfxMemId mid)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXBufferAllocator& self = *(MFXBufferAllocator *)pthis;

    return self.UnlockBuffer(mid);
}

mfxStatus MFXBufferAllocator::Free_(mfxHDL pthis, mfxMemId mid)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXBufferAllocator& self = *(MFXBufferAllocator *)pthis;

    return self.FreeBuffer(mid);
}

