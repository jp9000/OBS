#include "stdafx.h"
#include "ObsVCE.h"

bool VCEEncoder::encodeCreate(uint32_t deviceId)
{
    bool status;
    cl_int err;
    mOveContext = NULL;

    intptr_t properties[] =
    {
        CL_CONTEXT_PLATFORM, (cl_context_properties)mDeviceHandle.platform,
        0
    };

    cl_device_id clDeviceID = reinterpret_cast<cl_device_id>(deviceId);
    if (clDeviceID == 0)
    {
        VCELog(TEXT("No suitable devices found!"));
        return false;
    }

    mOveContext = clCreateContext(properties, 1, &clDeviceID, 0, 0, &err);
    if (mOveContext == (cl_context)0)
    {
        VCELog(TEXT("Cannot create cl_context."));
        return false;
    }

    if (err != CL_SUCCESS) //Well, mOveContext is probably NULL anyway
    {
        VCELog(TEXT("Error in clCreateContext %d"), err);
        return false;
    }
    
    /**************************************************************************/
    /* Read the device capabilities...                                        */
    /* Device capabilities should be used to validate against the             */
    /* configuration set by the user for the codec                            */
    /**************************************************************************/

    OVE_ENCODE_CAPS encodeCaps;
    OVE_ENCODE_CAPS_H264 encode_cap_full;
    encodeCaps.caps.encode_cap_full = (OVE_ENCODE_CAPS_H264 *)&encode_cap_full;
    status = getDeviceCap(mOveContext, deviceId, &encodeCaps);

    if (!status)
    {
        VCELog(TEXT("OVEncodeGetDeviceCap failed! Maybe too many open or unclosed (due to crashes?) encoder sessions."));
        return false;
    }

    //TODO Log it? Check Log formatting
    //VCELog(TEXT("**** CAPS ****\r\n* Bitrate Max: %d Min: %d"), encodeCaps.caps.encode_cap_full->max_bit_rate, encodeCaps.caps.encode_cap_full->min_bit_rate);
    //VCELog(TEXT("* Picture size Max: %d Min: %d (Macroblocks (width/16) * (height/16))\r\n* Profiles:"), encodeCaps.caps.encode_cap_full->max_picture_size_in_MB, encodeCaps.caps.encode_cap_full->min_picture_size_in_MB);
    //for (int i = 0; i< 20/*encodeCaps.caps.encode_cap_full->num_Profile_level*/; i++)
    //    VCELog(TEXT("*\tProf: %d Level: %d"), encodeCaps.caps.encode_cap_full->supported_profile_level[i].profile, encodeCaps.caps.encode_cap_full->supported_profile_level[i].level);
    //VCELog(TEXT("**************\r\n"));

    return true;
}

bool VCEEncoder::encodeOpen(uint32_t deviceId)
{
    assert(mHostPtrSize);
    cl_device_id clDeviceID = reinterpret_cast<cl_device_id>(deviceId);
    OVresult res = 0;
    cl_int err;

    /**************************************************************************/
    /* Create an OVE Session                                                  */
    /**************************************************************************/
    mEncodeHandle.session = OVEncodeCreateSession(mOveContext,  /**<Platform context */
        deviceId,               /**< device id */
        mConfigCtrl.encodeMode,    /**< encode mode */
        mConfigCtrl.profileLevel,  /**< encode profile */
        mConfigCtrl.pictFormat,    /**< encode format */
        mConfigCtrl.width,         /**< width */
        mConfigCtrl.height,        /**< height */
        mConfigCtrl.priority);     /**< encode task priority, ie. FOR POSSIBLY LOW LATENCY OVE_ENCODE_TASK_PRIORITY_LEVEL2 */

    if (mEncodeHandle.session == NULL)
    {
        VCELog(TEXT("OVEncodeCreateSession failed. Maybe too many encode sessions (or crashes, restart your computer then)."));
        return false;
    }

    /**************************************************************************/
    /* Configure the encoding engine.                                         */
    /**************************************************************************/
    res = setEncodeConfig(mEncodeHandle.session, &mConfigCtrl);
    if (!res)
        return false;

    /**************************************************************************/
    /* Create a command queue                                                 */
    /**************************************************************************/

    cl_command_queue_properties prop = 0;
    //(Currently) not using any CL kernels, so nothing to profile.
    //if (mConfigTable["ProfileKernels"] == 1)
    //    prop |= CL_QUEUE_PROFILING_ENABLE;

    mEncodeHandle.clCmdQueue[0] = clCreateCommandQueue((cl_context)mOveContext,
        clDeviceID, prop, &err);
    if (err != CL_SUCCESS)
    {
        VCELog(TEXT("Create command queue #0 failed! Error : %d"), err);
        return false;
    }

    mEncodeHandle.clCmdQueue[1] = clCreateCommandQueue((cl_context)mOveContext,
        clDeviceID, prop, &err);
    if (err != CL_SUCCESS)
    {
        VCELog(TEXT("Create command queue #1 failed! Error : %d"), err);
        return false;
    }

    //cl_int mode[] = {CL_MEM_READ_ONLY, CL_MEM_WRITE_ONLY};
    //cl_int mode[] = { CL_MEM_WRITE_ONLY };
    for (int32_t i = 0; i<MAX_INPUT_SURFACE; i++)
    {
        mEncodeHandle.inputSurfaces[i].surface = clCreateBuffer((cl_context)mOveContext,
            CL_MEM_READ_WRITE,
            mHostPtrSize,
            NULL,
            &err);
        if (err != CL_SUCCESS)
        {
            VCELog(TEXT("clCreateBuffer returned error %d"), err);
            return false;
        }
    }
    return true;
}

void unmapBuffer(OVEncodeHandle *encodeHandle, int surf)
{
    cl_event unmapEvent;
    cl_int status = clEnqueueUnmapMemObject(encodeHandle->clCmdQueue[0],
        (cl_mem)encodeHandle->inputSurfaces[surf].surface,
        encodeHandle->inputSurfaces[surf].mapPtr,
        0,
        NULL,
        &unmapEvent);
    status = clFlush(encodeHandle->clCmdQueue[0]);
    waitForEvent(unmapEvent);
    status = clReleaseEvent(unmapEvent);
    encodeHandle->inputSurfaces[surf].isMapped = false;
    encodeHandle->inputSurfaces[surf].mapPtr = NULL;
}

bool VCEEncoder::encodeClose()
{
    bool oveErr = false;
    cl_int err = CL_SUCCESS;
    OPSurface *inputSurfaces = mEncodeHandle.inputSurfaces;

    //TODO unmap buffers if mapped
    for (int32_t i = 0; i<MAX_INPUT_SURFACE; i++)
    {
        err = CL_SUCCESS;
        if (inputSurfaces[i].isMapped)
            unmapBuffer(&mEncodeHandle, i);

        if (inputSurfaces[i].surface)
            err = clReleaseMemObject((cl_mem)inputSurfaces[i].surface);
        inputSurfaces[i].surface = NULL;
        if (err != CL_SUCCESS)
        {
            VCELog(TEXT("clReleaseMemObject returned error %d"), err);
        }
    }

    //TODO number of CL command queues
    for (int i = 0; i<2; i++) {
        if (mEncodeHandle.clCmdQueue[i])
            err = clReleaseCommandQueue(mEncodeHandle.clCmdQueue[i]);
        mEncodeHandle.clCmdQueue[i] = NULL;
        if (err != CL_SUCCESS)
        {
            VCELog(TEXT("Error releasing Command queue #%d"), i);
        }
    }

    if (mEncodeHandle.session)
        oveErr = OVEncodeDestroySession(mEncodeHandle.session);
    if (!oveErr)
    {
        VCELog(TEXT("Error releasing OVE Session"));
    }
    mEncodeHandle.session = NULL;
    return true;
}