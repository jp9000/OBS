/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2012 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#ifndef __BASE_ALLOCATOR_H__
#define __BASE_ALLOCATOR_H__

#include <list>
#include <string.h>
#include <functional>
#include "mfxvideo.h"

struct mfxAllocatorParams
{
    virtual ~mfxAllocatorParams(){};
};

// this class implements methods declared in mfxFrameAllocator structure
// simply redirecting them to virtual methods which should be overridden in derived classes
class MFXFrameAllocator : public mfxFrameAllocator
{
public:
    MFXFrameAllocator();
    virtual ~MFXFrameAllocator();

     // optional method, override if need to pass some parameters to allocator from application
    virtual mfxStatus Init(mfxAllocatorParams *pParams) = 0;
    virtual mfxStatus Close() = 0;
    
    virtual mfxStatus AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response) = 0;
    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr) = 0;
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr) = 0;
    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle) = 0;
    virtual mfxStatus FreeFrames(mfxFrameAllocResponse *response) = 0;

private:
    static mfxStatus MFX_CDECL  Alloc_(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
    static mfxStatus MFX_CDECL  Lock_(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
    static mfxStatus MFX_CDECL  Unlock_(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
    static mfxStatus MFX_CDECL  GetHDL_(mfxHDL pthis, mfxMemId mid, mfxHDL *handle);
    static mfxStatus MFX_CDECL  Free_(mfxHDL pthis, mfxFrameAllocResponse *response);
};

// This class implements basic logic of memory allocator
// Manages responses for different components according to allocation request type
// External frames of a particular component-related type are allocated in one call
// Further calls return previously allocated response.
// Ex. Preallocated frame chain with type=FROM_ENCODE | FROM_VPPIN will be returned when 
// request type contains either FROM_ENCODE or FROM_VPPIN

// This class does not allocate any actual memory
class BaseFrameAllocator: public MFXFrameAllocator
{    
public:
    BaseFrameAllocator();
    virtual ~BaseFrameAllocator();       

    virtual mfxStatus Init(mfxAllocatorParams *pParams) = 0; 
    virtual mfxStatus Close();
    virtual mfxStatus AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);   
    virtual mfxStatus FreeFrames(mfxFrameAllocResponse *response);

protected:
    typedef std::list<mfxFrameAllocResponse>::iterator Iter;
    static const mfxU32 MEMTYPE_FROM_MASK = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_FROM_VPPIN | MFX_MEMTYPE_FROM_VPPOUT;

    struct UniqueResponse
        : mfxFrameAllocResponse 
    {
        mfxU16 m_cropw;
        mfxU16 m_croph;
        mfxU32 m_refCount;
        mfxU16 m_type;

        UniqueResponse()
        {
            memset(this, 0, sizeof(*this)); 
        }

        // compare responses by actual frame size, alignment (w and h) is up to application
        UniqueResponse(const mfxFrameAllocResponse & response, mfxU16 cropw, mfxU16 croph, mfxU16 type)
            : mfxFrameAllocResponse(response)
            , m_type(type)
            , m_refCount(1)
            , m_cropw(cropw)
            , m_croph(croph)
        { 
        } 
        //compare by resolution
        bool operator () (const UniqueResponse &response)const
        {
            return m_cropw == response.m_cropw && m_croph == response.m_croph;
        }
    };

    std::list<mfxFrameAllocResponse> m_responses;
    std::list<UniqueResponse> m_ExtResponses;

    struct IsSame
        : public std::binary_function<mfxFrameAllocResponse, mfxFrameAllocResponse, bool> 
    {
        bool operator () (const mfxFrameAllocResponse & l, const mfxFrameAllocResponse &r)const
        {
            return r.mids != 0 && l.mids != 0 && 
                r.mids[0] == l.mids[0] && 
                r.NumFrameActual == l.NumFrameActual;
        }
    };

    // checks if request is supported
    virtual mfxStatus CheckRequestType(mfxFrameAllocRequest *request);
    
    // frees memory attached to response
    virtual mfxStatus ReleaseResponse(mfxFrameAllocResponse *response) = 0;
    // allocates memory
    virtual mfxStatus AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response) = 0;

    template <class T>
    class safe_array
    {
    public:
        safe_array(T *ptr = 0):m_ptr(ptr)
        { // construct from object pointer            
        };        
        ~safe_array()
        {
            reset(0);
        }
        T* get()
        { // return wrapped pointer
            return m_ptr;
        }
        T* release()
        { // return wrapped pointer and give up ownership
            T* ptr = m_ptr;
            m_ptr = 0;
            return ptr;
        }
        void reset(T* ptr) 
        { // destroy designated object and store new pointer
            if (m_ptr)
            {
                delete[] m_ptr;
            }
            m_ptr = ptr;
        }        
    protected:
        T* m_ptr; // the wrapped object pointer
    };
};

class MFXBufferAllocator : public mfxBufferAllocator
{
public:
    MFXBufferAllocator();
    virtual ~MFXBufferAllocator();  

    virtual mfxStatus AllocBuffer(mfxU32 nbytes, mfxU16 type, mfxMemId *mid) = 0;
    virtual mfxStatus LockBuffer(mfxMemId mid, mfxU8 **ptr) = 0;
    virtual mfxStatus UnlockBuffer(mfxMemId mid) = 0;    
    virtual mfxStatus FreeBuffer(mfxMemId mid) = 0;  

private:
    static mfxStatus MFX_CDECL  Alloc_(mfxHDL pthis, mfxU32 nbytes, mfxU16 type, mfxMemId *mid);
    static mfxStatus MFX_CDECL  Lock_(mfxHDL pthis, mfxMemId mid, mfxU8 **ptr);
    static mfxStatus MFX_CDECL  Unlock_(mfxHDL pthis, mfxMemId mid);    
    static mfxStatus MFX_CDECL  Free_(mfxHDL pthis, mfxMemId mid);
};


#endif // __BASE_ALLOCATOR_H__