/********************************************************************************
Copyright (C) 2014 Timo Rothenpieler <timo@rothenpieler.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/

#include "nvmain.h"
#include "cudaDynload.h"

PCUINIT cuInit = NULL;
PCUDEVICEGETCOUNT cuDeviceGetCount = NULL;
PCUDEVICEGET cuDeviceGet = NULL;
PCUDEVICEGETNAME cuDeviceGetName = NULL;
PCUDEVICECOMPUTECAPABILITY cuDeviceComputeCapability = NULL;
PCUCTXCREATE cuCtxCreate = NULL;
PCUCTXPOPCURRENT cuCtxPopCurrent = NULL;
PCUCTXDESTROY cuCtxDestroy = NULL;

static HMODULE cudaLib = NULL;
static bool loadFailed = false;

#define CHECK_LOAD_FUNC(f, s) \
{ \
    f = (decltype(f))GetProcAddress(cudaLib, s); \
    if (f == NULL) \
    { \
        Log(TEXT("Failed loading %S from CUDA library"), s); \
        goto error; \
    } \
}

bool dyLoadCuda()
{
    if (cudaLib != NULL)
        return true;

    if (loadFailed)
        return false;

    cudaLib = LoadLibrary(TEXT("nvcuda.dll"));
    if (cudaLib == NULL)
    {
        Log(TEXT("Failed loading CUDA dll"));
        goto error;
    }

    CHECK_LOAD_FUNC(cuInit, "cuInit");
    CHECK_LOAD_FUNC(cuDeviceGetCount, "cuDeviceGetCount");
    CHECK_LOAD_FUNC(cuDeviceGet, "cuDeviceGet");
    CHECK_LOAD_FUNC(cuDeviceGetName, "cuDeviceGetName");
    CHECK_LOAD_FUNC(cuDeviceComputeCapability, "cuDeviceComputeCapability");
    CHECK_LOAD_FUNC(cuCtxCreate, "cuCtxCreate_v2");
    CHECK_LOAD_FUNC(cuCtxPopCurrent, "cuCtxPopCurrent_v2");
    CHECK_LOAD_FUNC(cuCtxDestroy, "cuCtxDestroy_v2");

    Log(TEXT("CUDA loaded successfully"));

    return true;

error:

    if (cudaLib != NULL)
        FreeLibrary(cudaLib);
    cudaLib = NULL;

    loadFailed = true;

    return false;
}
