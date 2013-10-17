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

File Name: mfx_dispatcher.cpp

\* ****************************************************************************** */

#include "mfx_dispatcher.h"
#include "mfx_dispatcher_log.h"
#include "mfx_load_dll.h"
#include <windows.h>


MFX_DISP_HANDLE::MFX_DISP_HANDLE(const mfxVersion requiredVersion) :
    apiVersion(requiredVersion)
{
    implType = MFX_LIB_SOFTWARE;
    impl = MFX_IMPL_SOFTWARE;
    dispVersion.Major = MFX_DISPATCHER_VERSION_MAJOR;
    dispVersion.Minor = MFX_DISPATCHER_VERSION_MINOR;
    session = (mfxSession) 0;

    hModule = (mfxModuleHandle) 0;

    memset(callTable, 0, sizeof(callTable));

} // MFX_DISP_HANDLE::MFX_DISP_HANDLE(const mfxVersion requiredVersion)

MFX_DISP_HANDLE::~MFX_DISP_HANDLE(void)
{
    Close();

} // MFX_DISP_HANDLE::~MFX_DISP_HANDLE(void)

mfxStatus MFX_DISP_HANDLE::Close(void)
{
    mfxStatus mfxRes;

    mfxRes = UnLoadSelectedDLL();

    // the library wasn't unloaded
    if (MFX_ERR_NONE == mfxRes)
    {
        implType = MFX_LIB_SOFTWARE;
        impl = MFX_IMPL_SOFTWARE;
        dispVersion.Major = MFX_DISPATCHER_VERSION_MAJOR;
        dispVersion.Minor = MFX_DISPATCHER_VERSION_MINOR;
        session = (mfxSession) 0;

        hModule = (mfxModuleHandle) 0;

        memset(callTable, 0, sizeof(callTable));
    }

    return mfxRes;

} // mfxStatus MFX_DISP_HANDLE::Close(void)

mfxStatus MFX_DISP_HANDLE::LoadSelectedDLL(const msdk_disp_char *pPath, eMfxImplType implType,
                                           mfxIMPL impl, mfxIMPL implInterface)
{
    mfxStatus mfxRes = MFX_ERR_NONE;

    // check error(s)
    if ((MFX_LIB_SOFTWARE != implType) &&
        (MFX_LIB_HARDWARE != implType))
    {
        DISPATCHER_LOG_ERROR((("implType == %s, should be either MFX_LIB_SOFTWARE ot MFX_LIB_HARDWARE\n"), DispatcherLog_GetMFXImplString(implType).c_str()));
        return MFX_ERR_ABORTED;
    }
    // only exact types of implementation is allowed
    if ((MFX_IMPL_SOFTWARE != impl) &&
        (MFX_IMPL_HARDWARE != impl) &&
        (MFX_IMPL_HARDWARE2 != impl) &&
        (MFX_IMPL_HARDWARE3 != impl) &&
        (MFX_IMPL_HARDWARE4 != impl))
    {
        DISPATCHER_LOG_ERROR((("invalid implementation impl == %s\n"), DispatcherLog_GetMFXImplString(impl).c_str()));
        return MFX_ERR_ABORTED;
    }        

    // close the handle before initialization
    Close();

    // save the library's type
    this->implType = implType;
    this->impl = impl;
    this->implInterface = implInterface;

    {
        mfxVersion maxAPIVersion = {0, 0};
        mfxVersion minAPIVersion = {0x7fff, 0x7fff};

        DISPATCHER_LOG_BLOCK(("invoking LoadLibrary(%S)\n", pPath));
        // load the DLL into the memory
        hModule = MFX::mfx_dll_load(pPath);
        
        if (hModule)
        {
            int i;

            DISPATCHER_LOG_OPERATION({
                msdk_disp_char modulePath[1024];
                GetModuleFileNameW((HMODULE)hModule, modulePath, sizeof(modulePath)/sizeof(modulePath[0]));
                DISPATCHER_LOG_INFO((("loaded module %S\n"), modulePath))
            });

            // load pointers to exposed functions
            for (i = 0; i < eFuncTotal; i += 1)
            {
                mfxFunctionPointer pProc = (mfxFunctionPointer) MFX::mfx_dll_get_addr(hModule, APIFunc[i].pName);
                if (pProc)
                {
                    // function exists in the library,
                    // save the pointer.
                    callTable[i] = pProc;
                    // save the maximum available API version
                    if (maxAPIVersion.Version < APIFunc[i].apiVersion.Version)
                    {
                        maxAPIVersion = APIFunc[i].apiVersion;
                        DISPATCHER_LOG_INFO((("found API function \"%s\", maxAPIVersion increased == %u.%u\n"), APIFunc[i].pName, maxAPIVersion.Major, maxAPIVersion.Minor ));
                    }
                }
                else if (minAPIVersion.Version > APIFunc[i].apiPrevVersion.Version)
                {
                    // The library doesn't contain the function.
                    // Reduce the minimal workable API version.
                    minAPIVersion = APIFunc[i].apiPrevVersion;
                    DISPATCHER_LOG_WRN((("Can't find API function \"%s\", minAPIVersion lowered=%u.%u\n"), APIFunc[i].pName,minAPIVersion.Major, minAPIVersion.Minor));
                }
            }

            // select the highest available API version
            if (minAPIVersion.Version > maxAPIVersion.Version)
            {
                DISPATCHER_LOG_INFO((("minAPIVersion(%u.%u) > maxAPIVersion(%u.%u), minAPIVersion lowered to (%u.%u)\n")
                    , minAPIVersion.Major, minAPIVersion.Minor
                    , maxAPIVersion.Major, maxAPIVersion.Minor
                    , maxAPIVersion.Major, maxAPIVersion.Minor));
                minAPIVersion = maxAPIVersion;
            }

            // if the library doesn't have required functions,
            // the library must be unloaded.
            if ((0 == minAPIVersion.Version) ||
                (minAPIVersion.Major != apiVersion.Major) ||
                (minAPIVersion.Minor < apiVersion.Minor))
            {
                DISPATCHER_LOG_WRN((("library version check failed: actual library version is %u.%u\n"),
                               minAPIVersion.Major, minAPIVersion.Minor))
                mfxRes = MFX_ERR_UNSUPPORTED;
            }
        }
        else
        {
            DISPATCHER_LOG_WRN((("can't find DLL: GetLastErr()=0x%x\n"), GetLastError()))
            mfxRes = MFX_ERR_UNSUPPORTED;
        }
    }

    // initialize the loaded DLL
    if (MFX_ERR_NONE == mfxRes)
    {
        mfxFunctionPointer pFunc;
        mfxVersion version(apiVersion);

        // call the DLL's function
        pFunc = callTable[eMFXInit];

        {
            DISPATCHER_LOG_BLOCK( ("MFXInit(%s,ver=%u.%u,session=0x%p)\n"
                                , DispatcherLog_GetMFXImplString(impl| implInterface).c_str()
                                , apiVersion.Major
                                , apiVersion.Minor
                                , &session));
        
            mfxRes = (*(mfxStatus (MFX_CDECL *) (mfxIMPL, mfxVersion *, mfxSession *)) pFunc) (impl | implInterface, &version, &session);
        }

        if (MFX_ERR_NONE != mfxRes)
        {
            DISPATCHER_LOG_WRN((("library can't be load. MFXInit returned %s \n"), DispatcherLog_GetMFXStatusString(mfxRes)))
        }
        else
        {
            //special hook for applications that uses sink api to get loaded library path
            DISPATCHER_LOG_LIBRARY(("%p" , hModule));
            DISPATCHER_LOG_INFO(("library loaded succesfully\n"))
        }
    }

    return mfxRes;

} // mfxStatus MFX_DISP_HANDLE::LoadSelectedDLL(const wchar_t *pPath, eMfxImplType implType, mfxIMPL impl)

mfxStatus MFX_DISP_HANDLE::UnLoadSelectedDLL(void)
{
    mfxStatus mfxRes = MFX_ERR_NOT_INITIALIZED;

    // close the loaded DLL
    if (session)
    {
        mfxFunctionPointer pFunc;

        // call the DLL's function
        pFunc = callTable[eMFXClose];
        mfxRes = (*(mfxStatus (MFX_CDECL *) (mfxSession)) pFunc) (session);
        if (MFX_ERR_NONE == mfxRes)
        {
            session = (mfxSession) 0;
        }

        DISPATCHER_LOG_INFO((("MFXClose(0x%x) returned %d\n"), session, mfxRes));
        // actually, the return value is required to pass outside only.
    }

    // it is possible, that there is an active child session.
    // can't unload library in that case.
    if ((MFX_ERR_UNDEFINED_BEHAVIOR != mfxRes) &&
        (hModule))
    {
        // unload the library.
        if (!MFX::mfx_dll_free(hModule))
        {
            mfxRes = MFX_ERR_UNDEFINED_BEHAVIOR;
        }
        hModule = (mfxModuleHandle) 0;
    }

    return mfxRes;

} // mfxStatus MFX_DISP_HANDLE::UnLoadSelectedDLL(void)
