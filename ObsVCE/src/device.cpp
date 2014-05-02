#include "stdafx.h"
#include "ObsVCE.h"

void waitForEvent(cl_event inMapEvt)
{
    cl_int eventStatus = CL_QUEUED;
    cl_int status;

    //TODO Might be RDP but sometimes gives -31 error (CL_INVALID_DEVICE_TYPE?)
    while (eventStatus > CL_COMPLETE)
    {
        status = clGetEventInfo(
            inMapEvt,
            CL_EVENT_COMMAND_EXECUTION_STATUS,
            sizeof(cl_int),
            &eventStatus,
            NULL);
    }
#ifdef _DEBUG
    if (eventStatus < 0)
        VCELog(TEXT("clGetEventInfo error %d"), eventStatus);
#endif
}

bool getDeviceCap(OPContextHandle oveContext, uint32_t oveDeviceID,
    OVE_ENCODE_CAPS *encodeCaps)
{
    uint32_t numCaps = 1;
    bool status;

    encodeCaps->EncodeModes = OVE_AVC_FULL;
    encodeCaps->encode_cap_size = sizeof(OVE_ENCODE_CAPS);
    encodeCaps->caps.encode_cap_full->max_picture_size_in_MB = 0;
    encodeCaps->caps.encode_cap_full->min_picture_size_in_MB = 0;
    encodeCaps->caps.encode_cap_full->num_picture_formats = 0;
    encodeCaps->caps.encode_cap_full->num_Profile_level = 0;
    encodeCaps->caps.encode_cap_full->max_bit_rate = 0;
    encodeCaps->caps.encode_cap_full->min_bit_rate = 0;
    encodeCaps->caps.encode_cap_full->supported_task_priority = OVE_ENCODE_TASK_PRIORITY_LEVEL1;

    for (int32_t j = 0; j<OVE_MAX_NUM_PICTURE_FORMATS_H264; j++)
        encodeCaps->caps.encode_cap_full->supported_picture_formats[j] = OVE_PICTURE_FORMAT_NONE;

    for (int32_t j = 0; j<OVE_MAX_NUM_PROFILE_LEVELS_H264; j++)
    {
        encodeCaps->caps.encode_cap_full->supported_profile_level[j].profile = 0;
        encodeCaps->caps.encode_cap_full->supported_profile_level[j].level = 0;
    }

    status = OVEncodeGetDeviceCap(oveContext,
        oveDeviceID,
        encodeCaps->encode_cap_size,
        &numCaps,
        encodeCaps);
    return(status);
}

bool getDeviceInfo(ovencode_device_info **deviceInfo,
    uint32_t *numDevices)
{
    bool status;
    status = OVEncodeGetDeviceInfo(numDevices, 0);
    if (!status)
    {
        VCELog(TEXT("OVEncodeGetDeviceInfo failed!\n"));
        return false;
    }
    else
    {
        if (*numDevices == 0)
        {
            VCELog(TEXT("No suitable devices found!\n"));
            return false;
        }
    }

    // Be sure to free after use!
    *deviceInfo = new ovencode_device_info[*numDevices];
    memset(*deviceInfo, 0, sizeof(ovencode_device_info)* (*numDevices));
    status = OVEncodeGetDeviceInfo(numDevices, *deviceInfo);
    if (!status)
    {
        VCELog(TEXT("OVEncodeGetDeviceInfo failed!\n"));
        return false;
    }
    return true;
}

bool getDevice(OVDeviceHandle *deviceHandle)
{
    bool status;

    deviceHandle->platform = NULL;
    status = getPlatform(deviceHandle->platform);
    if (status == FALSE)
    {
        return false;
    }

    cl_device_type dType = CL_DEVICE_TYPE_GPU;
    status = gpuCheck(deviceHandle->platform, &dType);
    if (status == FALSE ||
        dType != CL_DEVICE_TYPE_GPU) //No CL gpu, then probably can't use VCE too.
    {
        return false;
    }

    deviceHandle->numDevices = 0;
    deviceHandle->deviceInfo = NULL;

    // Be sure to free after use!
    status = getDeviceInfo(&deviceHandle->deviceInfo, &deviceHandle->numDevices);
    if (status == false)
    {
        return false;
    }
    return true;
}

bool getPlatform(cl_platform_id &platform)
{
    cl_uint numPlatforms;
    cl_int err = clGetPlatformIDs(0, NULL, &numPlatforms);
    if (CL_SUCCESS != err)
    {
        VCELog(TEXT("clGetPlatformIDs() failed %d\n"), err);
        return false;
    }
    /**************************************************************************/
    /* If there are platforms, make sure they are AMD.                        */
    /**************************************************************************/
    if (0 < numPlatforms)
    {
        cl_platform_id* platforms = new cl_platform_id[numPlatforms];
        err = clGetPlatformIDs(numPlatforms, platforms, NULL);
        if (CL_SUCCESS != err)
        {
            VCELog(TEXT("clGetPlatformIDs() failed %d\n"), err);
            delete[] platforms;
            return false;
        }

        for (uint32_t i = 0; i < numPlatforms; ++i)
        {
            char pbuf[100];
            err = clGetPlatformInfo(platforms[i],
                CL_PLATFORM_VENDOR,
                sizeof(pbuf),
                pbuf,
                NULL);

            if (!strcmp(pbuf, "Advanced Micro Devices, Inc."))
            {
                platform = platforms[i];
                break;
            }
        }
        delete[] platforms;
    }

    if (NULL == platform)
    {
        VCELog(TEXT("Couldn't find AMD platform, cannot proceed.\n"));
        return false;
    }

    return true;
}

bool gpuCheck(cl_platform_id platform, cl_device_type* dType)
{
    cl_int err;
    cl_context_properties cps[3] =
    {
        CL_CONTEXT_PLATFORM,
        (cl_context_properties)platform,
        0
    };

    cl_context context = clCreateContextFromType(cps,
        (*dType),
        NULL,
        NULL,
        &err);

    if (err == CL_DEVICE_NOT_FOUND)
    {
        VCELog(TEXT("GPU not found. Fallback to CPU\n"));
        *dType = CL_DEVICE_TYPE_CPU;
        return false;
    }
    clReleaseContext(context);
    return true;
}