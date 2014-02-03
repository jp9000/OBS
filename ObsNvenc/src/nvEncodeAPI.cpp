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

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#define NV_WINDOWS
#endif

NV_ENCODE_API_FUNCTION_LIST *pNvEnc = 0;
int iNvencDeviceCount = 0;
CUdevice pNvencDevices[16];
unsigned int iNvencUseDeviceID = 0;

#if defined(_WIN64)
static LPCTSTR encodeLibName = TEXT("nvEncodeAPI64.dll");
#elif defined(_WIN32)
static LPCTSTR encodeLibName = TEXT("nvEncodeAPI.dll");
#endif

static HMODULE nvEncLib = NULL;

bool _checkCudaErrors(CUresult err, const char *func)
{
    if (err != CUDA_SUCCESS)
    {
        Log(TEXT(">> %S - failed with error code 0x%x"), func, err);
        return false;
    }
    return true;
}

bool checkNvEnc()
{
    int deviceCount = 0;
    CUdevice cuDevice = 0;
    char gpu_name[128];
    int SMminor = 0, SMmajor = 0;

    if (iNvencDeviceCount > 0)
        return true;

    if (iNvencDeviceCount < 0)
        return false;

    if (!dyLoadCuda())
        goto error;

    checkCudaErrors(cuInit(0));

    checkCudaErrors(cuDeviceGetCount(&deviceCount));

    if (deviceCount == 0)
    {
        Log(TEXT("No CUDA capable devices found"));
        goto error;
    }

    Log(TEXT("%d CUDA capable devices found"), deviceCount);

    iNvencDeviceCount = 0;

    for (int i = 0; i < deviceCount && i < 16; ++i)
    {
        checkCudaErrors(cuDeviceGet(&cuDevice, i));
        checkCudaErrors(cuDeviceGetName(gpu_name, 128, cuDevice));
        checkCudaErrors(cuDeviceComputeCapability(&SMmajor, &SMminor, cuDevice));

        bool hasNvenc = ((SMmajor << 4) + SMminor) >= 0x30;

        Log(TEXT("[ GPU #%d - < %S > has Compute SM %d.%d, NVENC %S ]"), i, gpu_name, SMmajor, SMminor, hasNvenc ? "Available" : "Not Available");

        if (hasNvenc)
        {
            pNvencDevices[iNvencDeviceCount] = cuDevice;
            iNvencDeviceCount += 1;
        }
    }

    if (iNvencDeviceCount == 0)
    {
        Log(TEXT("No NVENC capable devices found"));
        goto error;
    }

    return true;

error:

    iNvencDeviceCount = -1;

    return false;
}

bool initNvEnc()
{
    if (pNvEnc != 0)
        return true;

    if (!checkNvEnc())
        return false;

    nvEncLib = LoadLibrary(encodeLibName);

    if(nvEncLib == 0)
    {
        NvLog(TEXT("Failed to load %s"), encodeLibName);
        goto error;
    }

    PNVENCODEAPICREATEINSTANCE nvEncodeAPICreateInstance =
        (PNVENCODEAPICREATEINSTANCE)GetProcAddress(nvEncLib, "NvEncodeAPICreateInstance");

    if(nvEncodeAPICreateInstance == 0)
    {
        NvLog(TEXT("Failed to load nvenc entrypoint"));
        goto error;
    }

    pNvEnc = new NV_ENCODE_API_FUNCTION_LIST;
    assert(pNvEnc);

    mset(pNvEnc, 0, sizeof(NV_ENCODE_API_FUNCTION_LIST));
    pNvEnc->version = NV_ENCODE_API_FUNCTION_LIST_VER;
    NVENCSTATUS status = nvEncodeAPICreateInstance(pNvEnc);

    if (status != NV_ENC_SUCCESS)
    {
        NvLog(TEXT("Failed to get nvenc instance"));
        goto error;
    }

    NvLog(TEXT("NVENC internal init finished successfully"));

    return true;

error:

    iNvencDeviceCount = 0;

    delete pNvEnc;
    pNvEnc = 0;

    FreeLibrary(nvEncLib);
    nvEncLib = 0;

    NvLog(TEXT("NVENC internal init failed"));

    return false;
}

void deinitNvEnc()
{
    iNvencDeviceCount = 0;

    delete pNvEnc;
    pNvEnc = 0;

    FreeLibrary(nvEncLib);
    nvEncLib = 0;

    NvLog(TEXT("NVENC deinitialized"));
}
