/* ****************************************************************************** *\

Copyright (C) 2012 Intel Corporation.  All rights reserved.

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

File Name: mfx_dispatcher.h

\* ****************************************************************************** */

#if !defined(__MFX_DISPATCHER_H)
#define __MFX_DISPATCHER_H

#include <mfxvideo.h>
#include <mfxplugin.h>
#include <stddef.h>


typedef wchar_t  msdk_disp_char;

#if !defined (MFX_DISPATCHER_EXPOSED_PREFIX)
    #define DISPATCHER_EXPOSED_PREFIX(fnc) fnc 
#else
    #define DISPATCHER_EXPOSED_PREFIX(fnc) _##fnc 
#endif


enum
{
    // to avoid code changing versions are just inherited
    // from the API header file.
    DEFAULT_API_VERSION_MAJOR   = MFX_VERSION_MAJOR,
    DEFAULT_API_VERSION_MINOR   = MFX_VERSION_MINOR
};

//
// declare functions' integer identifiers.
//

#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list) \
    e##func_name,

enum eFunc
{
    eMFXInit,
    eMFXClose,
    eMFXJoinSession,
    eMFXCloneSession,

#include "mfx_exposed_functions_list.h"

    eFuncTotal
};

// declare max buffer length for DLL path
enum
{
    MFX_MAX_VALUE_NAME          = 128
};

// declare the maximum DLL path
enum
{
    MFX_MAX_DLL_PATH = 1024
};

// declare library's implementation types
enum eMfxImplType
{
    MFX_LIB_HARDWARE            = 0,
    MFX_LIB_SOFTWARE            = 1,
    MFX_LIB_PSEUDO              = 2,

    MFX_LIB_IMPL_TYPES
};

// declare dispatcher's version
enum
{
    MFX_DISPATCHER_VERSION_MAJOR = 1,
    MFX_DISPATCHER_VERSION_MINOR = 2
};

// declare library module's handle
typedef void * mfxModuleHandle;

typedef void (MFX_CDECL * mfxFunctionPointer)(void);

// declare a dispatcher's handle
struct MFX_DISP_HANDLE
{
    // Default constructor
    MFX_DISP_HANDLE(const mfxVersion requiredVersion);
    // Destructor
    ~MFX_DISP_HANDLE(void);

    // Load the library's module
    mfxStatus LoadSelectedDLL(const msdk_disp_char *pPath, eMfxImplType implType, mfxIMPL impl, mfxIMPL implInterface);
    // Unload the library's module
    mfxStatus UnLoadSelectedDLL(void);

    // Close the handle
    mfxStatus Close(void);

    // NOTE: changing order of struct's members can make different version of
    // dispatchers incompatible. Think of different modules (e.g. MFT filters)
    // within a single application.

    // Library's implementation type (hardware or software)
    eMfxImplType implType;
    // Current library's implementation (exact implementation)
    mfxIMPL impl;
    // Current library's VIA interface
    mfxIMPL implInterface;
    // Dispatcher's version. If version is 1.1 or lower, then old dispatcher's
    // architecture is used. Otherwise it means current dispatcher's version.
    mfxVersion dispVersion;
    // A real handle passed to a called function
    mfxSession session;
    // required API version of session initialized
    const
    mfxVersion apiVersion;

    // Library's module handle
    mfxModuleHandle hModule;

    // function call table
    mfxFunctionPointer callTable[eFuncTotal];

private:
    // Declare assignment operator and copy constructor to prevent occasional assignment
    MFX_DISP_HANDLE(const MFX_DISP_HANDLE &);
    MFX_DISP_HANDLE & operator = (const MFX_DISP_HANDLE &);    

};

// declare comparison operator
inline
bool operator == (const mfxVersion &one, const mfxVersion &two)
{
    return (one.Version == two.Version);

} // bool operator == (const mfxVersion &one, const mfxVersion &two)

//
// declare a table with functions descriptions
//

typedef
struct FUNCTION_DESCRIPTION
{
    // Literal function's name
    const char *pName;
    // API version when function appeared first time
    mfxVersion apiVersion;
    // Minimal API version, which works without this function
    mfxVersion apiPrevVersion;

} FUNCTION_DESCRIPTION;

extern const
FUNCTION_DESCRIPTION APIFunc[eFuncTotal];

#endif // __MFX_DISPATCHER_H
