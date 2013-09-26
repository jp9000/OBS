/* ****************************************************************************** *\

Copyright (C) 2012-2013 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: main.cpp

\* ****************************************************************************** */

#include "mfx_dispatcher.h"
#include "mfx_load_dll.h"
#include "mfx_dispatcher_log.h"
#include "mfx_library_iterator.h"
#include "mfx_critical_section.h"

#include <memory>

// module-local definitions
namespace
{

const
struct
{
    // instance implementation type
    eMfxImplType implType;
    // real implementation
    mfxIMPL impl;
    // adapter numbers
    mfxU32 adapterID;

} implTypes[] =
{
    // MFX_IMPL_AUTO case
    {MFX_LIB_HARDWARE, MFX_IMPL_HARDWARE,  0},
    {MFX_LIB_SOFTWARE, MFX_IMPL_SOFTWARE,  0},

    // MFX_IMPL_ANY case
    {MFX_LIB_HARDWARE, MFX_IMPL_HARDWARE,  0},
    {MFX_LIB_HARDWARE, MFX_IMPL_HARDWARE2, 1},
    {MFX_LIB_HARDWARE, MFX_IMPL_HARDWARE3, 2},
    {MFX_LIB_HARDWARE, MFX_IMPL_HARDWARE4, 3},
    {MFX_LIB_SOFTWARE, MFX_IMPL_SOFTWARE,  0}
};

const
struct
{
    // start index in implTypes table for specified implementation
    mfxU32 minIndex;
    // last index in implTypes table for specified implementation
    mfxU32 maxIndex;

} implTypesRange[] =
{    
    {0, 1},  // MFX_IMPL_AUTO    
    {1, 1},  // MFX_IMPL_SOFTWARE    
    {0, 0},  // MFX_IMPL_HARDWARE    
    {2, 6},  // MFX_IMPL_AUTO_ANY    
    {2, 5},  // MFX_IMPL_HARDWARE_ANY    
    {3, 3},  // MFX_IMPL_HARDWARE2    
    {4, 4},  // MFX_IMPL_HARDWARE3    
    {5, 5}   // MFX_IMPL_HARDWARE4
};

MFX::mfxCriticalSection dispGuard = 0;

} // namespace

//
// Implement DLL exposed functions. MFXInit and MFXClose have to do
// slightly more than other. They require to be implemented explicitly.
// All other functions are implemented implicitly.
//

mfxStatus DISPATCHER_EXPOSED_PREFIX(MFXInit)(mfxIMPL impl, mfxVersion *pVer, mfxSession *session)
{
    MFX::MFXAutomaticCriticalSection guard(&dispGuard);

    DISPATCHER_LOG_BLOCK( ("MFXInit (impl=%s, pVer=%d.%d session=0x%p\n"
                        , DispatcherLog_GetMFXImplString(impl).c_str()
                        , (pVer)? pVer->Major : 0
                        , (pVer)? pVer->Minor : 0
                        , session));

    mfxStatus mfxRes;
    std::auto_ptr<MFX_DISP_HANDLE> allocatedHandle;
    MFX_DISP_HANDLE *pHandle;
    msdk_disp_char dllName[MFX_MAX_DLL_PATH];
    // there iterators are used only if the caller specified implicit type like AUTO
    mfxU32 curImplIdx, maxImplIdx;
    // particular implementation value
    mfxIMPL curImpl;
    // implementation method masked from the input parameter
    const mfxIMPL implMethod = impl & (MFX_IMPL_VIA_ANY - 1);
    // implementation interface masked from the input parameter
    mfxIMPL implInterface = impl & -MFX_IMPL_VIA_ANY;
    mfxIMPL implInterfaceOrig = implInterface;
    mfxVersion requiredVersion = {MFX_VERSION_MINOR, MFX_VERSION_MAJOR};

    // check error(s)
    if (NULL == session)
    {
        return MFX_ERR_NULL_PTR;
    }
    if ((MFX_IMPL_AUTO > implMethod) || (MFX_IMPL_HARDWARE4 < implMethod))
    {
        return MFX_ERR_UNSUPPORTED;
    }

    // set the minimal required version
    if (pVer)
    {
        requiredVersion = *pVer;
    }

    try
    {
        // reset the session value
        *session = 0;

        // allocate the dispatching handle and call-table
        allocatedHandle.reset(new MFX_DISP_HANDLE(requiredVersion));
    }
    catch(...)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    pHandle = allocatedHandle.get();

    DISPATCHER_LOG_OPERATION({
        if (pVer)
        {
            DISPATCHER_LOG_INFO((("Required API version is %u.%u\n"), pVer->Major, pVer->Minor));
        }
        else
        {
            DISPATCHER_LOG_INFO((("Default API version is %u.%u\n"), DEFAULT_API_VERSION_MAJOR, DEFAULT_API_VERSION_MINOR));
        }
    });

    // main query cycle
    curImplIdx = implTypesRange[implMethod].minIndex;
    maxImplIdx = implTypesRange[implMethod].maxIndex;

    do
    {
        MFX::MFXLibraryIterator libIterator;
        int currentStorage = MFX::MFX_CURRENT_USER_KEY;

        do
        {
            // initialize the library iterator
            mfxRes = libIterator.Init(implTypes[curImplIdx].implType, 
                                      implInterface,
                                      implTypes[curImplIdx].adapterID,
                                      currentStorage);

            // look through the list of installed SDK version,
            // looking for a suitable library with higher merit value.
            if (MFX_ERR_NONE == mfxRes)
            {
            
                if (
                    !implInterface || 
                    implInterface == MFX_IMPL_VIA_ANY)
                {
                    implInterface = libIterator.GetImplementationType();
                }

                do
                {
                    eMfxImplType implType;

                    // select a desired DLL
                    mfxRes = libIterator.SelectDLLVersion(dllName,
                                                          sizeof(dllName) / sizeof(dllName[0]),
                                                          &implType,
                                                          pHandle->apiVersion);
                    if (MFX_ERR_NONE != mfxRes)
                    {
                        break;
                    }
                    DISPATCHER_LOG_INFO((("loading library %S\n"), dllName));
                    
                    // try to load the selected DLL
                    curImpl = implTypes[curImplIdx].impl;
                    mfxRes = pHandle->LoadSelectedDLL(dllName, implType, curImpl, implInterface);
                    // unload the failed DLL
                    if (MFX_ERR_NONE != mfxRes)
                    {
                        pHandle->Close();
                    }

                } while (MFX_ERR_NONE != mfxRes);
            }

            // select another registry key
            currentStorage += 1;

        } while ((MFX_ERR_NONE != mfxRes) && (MFX::MFX_LOCAL_MACHINE_KEY >= currentStorage));

    } while ((MFX_ERR_NONE != mfxRes) && (++curImplIdx <= maxImplIdx));

    // use the default DLL search mechanism fail,
    // if hard-coded software library's path from the registry fails
    implInterface = implInterfaceOrig;
    if (MFX_ERR_NONE != mfxRes)
    {
        // set current library index again
        curImplIdx = implTypesRange[implMethod].minIndex;
        do
        {
            mfxRes = MFX::mfx_get_default_dll_name(dllName,
                                       sizeof(dllName) / sizeof(dllName[0]),
                                       implTypes[curImplIdx].implType);
            if (MFX_ERR_NONE == mfxRes)
            {
                DISPATCHER_LOG_INFO((("loading default library %S\n"), dllName))

                // try to load the selected DLL using default DLL search mechanism
                mfxRes = pHandle->LoadSelectedDLL(dllName,
                                                  implTypes[curImplIdx].implType,
                                                  implTypes[curImplIdx].impl,
                                                  implInterface);
                // unload the failed DLL
                if ((MFX_ERR_NONE != mfxRes) &&
                    (MFX_WRN_PARTIAL_ACCELERATION != mfxRes))
                {
                    pHandle->UnLoadSelectedDLL();
                }
            }
        }
        while ((MFX_ERR_NONE > mfxRes) && (++curImplIdx <= maxImplIdx));
    }

    // check the final result of loading
    if ((MFX_ERR_NONE == mfxRes) ||
        (MFX_WRN_PARTIAL_ACCELERATION == mfxRes))
    {
        // everything is OK. Save pointers to the output variable
        allocatedHandle.release();
        *((MFX_DISP_HANDLE **) session) = pHandle;
    }

    return mfxRes;

} // mfxStatus MFXInit(mfxIMPL impl, mfxVersion *ver, mfxSession *session)

mfxStatus DISPATCHER_EXPOSED_PREFIX(MFXClose)(mfxSession session)
{
    MFX::MFXAutomaticCriticalSection guard(&dispGuard);

    mfxStatus mfxRes = MFX_ERR_INVALID_HANDLE;
    MFX_DISP_HANDLE *pHandle = (MFX_DISP_HANDLE *) session;

    // check error(s)
    if (pHandle)
    {
        try
        {
            // unload the DLL library
            mfxRes = pHandle->Close();

            // it is possible, that there is an active child session.
            // can't unload library in that case.
            if (MFX_ERR_UNDEFINED_BEHAVIOR != mfxRes)
            {
                // release the handle
                delete pHandle;
            }
        }
        catch(...)
        {
            mfxRes = MFX_ERR_INVALID_HANDLE;
        }
    }

    return mfxRes;

} // mfxStatus MFXClose(mfxSession session)

mfxStatus DISPATCHER_EXPOSED_PREFIX(MFXJoinSession)(mfxSession session, mfxSession child_session)
{
    mfxStatus mfxRes = MFX_ERR_INVALID_HANDLE;
    MFX_DISP_HANDLE *pHandle = (MFX_DISP_HANDLE *) session;
    MFX_DISP_HANDLE *pChildHandle = (MFX_DISP_HANDLE *) child_session;

    // get the function's address and make a call
    if ((pHandle) && (pChildHandle) && (pHandle->apiVersion == pChildHandle->apiVersion))
    {
        mfxFunctionPointer pFunc = pHandle->callTable[eMFXJoinSession];

        if (pFunc)
        {
            // pass down the call
            mfxRes = (*(mfxStatus (MFX_CDECL *) (mfxSession, mfxSession)) pFunc) (pHandle->session, 
                                                                        pChildHandle->session);
        }
    }

    return mfxRes;

} // mfxStatus MFXJoinSession(mfxSession session, mfxSession child_session)

mfxStatus DISPATCHER_EXPOSED_PREFIX(MFXCloneSession)(mfxSession session, mfxSession *clone)
{
    mfxStatus mfxRes = MFX_ERR_INVALID_HANDLE;
    MFX_DISP_HANDLE *pHandle = (MFX_DISP_HANDLE *) session;
    mfxVersion apiVersion;
    mfxIMPL impl;

    // check error(s)
    if (pHandle)
    {
        // initialize the clone session
        apiVersion = pHandle->apiVersion;
        impl = pHandle->impl | pHandle->implInterface;
        mfxRes = MFXInit(impl, &apiVersion, clone);
        if (MFX_ERR_NONE != mfxRes)
        {
            return mfxRes;
        }

        // join the sessions
        mfxRes = MFXJoinSession(session, *clone);
        if (MFX_ERR_NONE != mfxRes)
        {
            MFXClose(*clone);
            *clone = NULL;
            return mfxRes;
        }
    }

    return mfxRes;

} // mfxStatus MFXCloneSession(mfxSession session, mfxSession *clone)

//
// implement all other calling functions.
// They just call a procedure of DLL library from the table.
//

#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list) \
return_value DISPATCHER_EXPOSED_PREFIX(func_name) formal_param_list \
{ \
    mfxStatus mfxRes = MFX_ERR_INVALID_HANDLE; \
    MFX_DISP_HANDLE *pHandle = (MFX_DISP_HANDLE *) session; \
    /* get the function's address and make a call */ \
    if (pHandle) \
    { \
        mfxFunctionPointer pFunc = pHandle->callTable[e##func_name]; \
        if (pFunc) \
        { \
            /* get the real session pointer */ \
            session = pHandle->session; \
            /* pass down the call */ \
            mfxRes = (*(mfxStatus (MFX_CDECL  *) formal_param_list) pFunc) actual_param_list; \
        } \
    } \
    return mfxRes; \
}

#include "mfx_exposed_functions_list.h"
