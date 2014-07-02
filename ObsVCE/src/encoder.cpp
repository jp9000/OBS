#include "stdafx.h"
#include "ObsVCE.h"

static const char *source =
// global threads: width, height
"__kernel void Y444toNV12_Y(__global uchar4 *input, __global uchar *output, int alignedWidth)"
"{"
"int2 id = (int2)(get_global_id(0), get_global_id(1));"
"int width = get_global_size(0);"
"int offset = id.x + alignedWidth * id.y;"
"output[id.x + id.y * alignedWidth] = input[offset].y;"
"}"

// global threads: width/2, height/2
"__kernel void Y444toNV12_UV(__global uchar4 *input, __global uchar *output, int alignedWidth)"
"{"
"int2 id = (int2)(get_global_id(0), get_global_id(1));"
"int height = get_global_size(1);"

"int src = id.x * 2 + alignedWidth * id.y * 2;"
"int dst = id.x * 2 + alignedWidth * id.y + alignedWidth * height * 2;"

//Sample on Y
"float2 IN0 = convert_float2(input[src + 0].xz);"
"float2 IN1 = convert_float2(input[src + 1].xz);"
//Sample on Y + 1
"float2 IN2 = convert_float2(input[src + 0 + alignedWidth].xz);"
"float2 IN3 = convert_float2(input[src + 1 + alignedWidth].xz);"

"uchar2 OUT = convert_uchar2_sat_rte((IN0 + IN1 + IN2 + IN3) / 4);"
"output[dst]     = OUT.x;"
"output[dst + 1] = OUT.y;"
"}"
;


void mapBuffer(OVEncodeHandle *encodeHandle, int i, uint32_t size)
{
    cl_event inMapEvt = 0;
    cl_int   status = CL_SUCCESS;

    encodeHandle->inputSurfaces[i].mapPtr = clEnqueueMapBuffer(encodeHandle->clCmdQueue[0],
        (cl_mem)encodeHandle->inputSurfaces[i].surface,
        CL_FALSE,
        CL_MAP_WRITE,
        0,
        size,
        0,
        NULL,
        //NULL, 
        &inMapEvt,
        &status);

    if (status != CL_SUCCESS)
    {
        VCELog(TEXT("Failed to map input buffer: %d"), status);
    }

    //clEnqueueBarrier(encodeHandle->clCmdQueue[0]);
    clFlush(encodeHandle->clCmdQueue[0]);
    waitForEvent(inMapEvt);
    status = clReleaseEvent(inMapEvt);
    encodeHandle->inputSurfaces[i].isMapped = true;
}

void unmapBuffer(OVEncodeHandle *encodeHandle, int surf)
{
    cl_event unmapEvent;
    cl_int status = clEnqueueUnmapMemObject(encodeHandle->clCmdQueue[0],
        (cl_mem)encodeHandle->inputSurfaces[surf].surface,
        encodeHandle->inputSurfaces[surf].mapPtr,
        0,
        NULL,
        //NULL);
        &unmapEvent);
    /// clEnqueueBarrier causes green frames at start
    //clEnqueueBarrier(encodeHandle->clCmdQueue[0]);
    //clFlush(encodeHandle->clCmdQueue[0]);
    waitForEvent(unmapEvent);
    status = clReleaseEvent(unmapEvent);
    encodeHandle->inputSurfaces[surf].isMapped = false;
    encodeHandle->inputSurfaces[surf].mapPtr = NULL;
}

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

#ifdef _WIN64
    // May ${DEITY} have mercy on us all.
    intptr_t ptr = intptr_t((intptr_t*)&clDeviceID);
    clDeviceID = (cl_device_id)((intptr_t)clDeviceID | (ptr & 0xFFFFFFFF00000000));
#endif

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

    if (mUse444)
    {
        size_t sourceSize[] = { strlen(source) };
        cl_int clstatus = 0;
        mProgram = clCreateProgramWithSource((cl_context)mOveContext,
            1,
            (const char**)&source,
            sourceSize,
            &clstatus);
        //free(source);

        if (clstatus != CL_SUCCESS)
        {
            VCELog(TEXT("clCreateProgramWithSource failed."));
            return false;
        }

        std::string flagsStr("");// ("-save-temps ");
        flagsStr.append("-cl-single-precision-constant -cl-mad-enable -cl-fast-relaxed-math -cl-unsafe-math-optimizations ");

        /* create a cl program executable for all the devices specified */
        clstatus = clBuildProgram(mProgram,
            1,
            &clDeviceID,
            flagsStr.c_str(),
            NULL,
            NULL);
        if (clstatus != CL_SUCCESS)
        {
            VCELog(TEXT("clBuildProgram() failed."));
        }

        mKernel[0] = clCreateKernel(mProgram, "Y444toNV12_Y", &clstatus);
        if (clstatus != CL_SUCCESS)
        {
            VCELog(TEXT("clCreateKernel failed!"));
            return false;
        }

        mKernel[1] = clCreateKernel(mProgram, "Y444toNV12_UV", &clstatus);
        if (clstatus != CL_SUCCESS)
        {
            VCELog(TEXT("clCreateKernel(UV) failed!"));
            return false;
        }
    }
    return true;
}

bool VCEEncoder::encodeOpen(uint32_t deviceId)
{
    assert(mInputBufSize);
    cl_device_id clDeviceID = reinterpret_cast<cl_device_id>(deviceId);
    OVresult res = 0;
    cl_int err;

#ifdef _WIN64
    // May ${DEITY} have mercy on us all.
    intptr_t ptr = intptr_t((intptr_t*)&clDeviceID);
    clDeviceID = (cl_device_id)((intptr_t)clDeviceID | (ptr & 0xFFFFFFFF00000000));
#endif

    cl_command_queue_properties prop = 0;
    //(Currently) not using any CL kernels, so nothing to profile.
    //if (mConfigTable["ProfileKernels"] == 1)
    //    prop |= CL_QUEUE_PROFILING_ENABLE;

    //TODO Remove second unused queue or do something fancy with it
    mEncodeHandle.clCmdQueue[0] = clCreateCommandQueue((cl_context)mOveContext,
        clDeviceID, prop, &err);
    if (err != CL_SUCCESS)
    {
        VCELog(TEXT("Create command queue #0 failed! Error : %d"), err);
        return false;
    }

    mEncodeHandle.clCmdQueue[1] = NULL;

    for (int32_t i = 0; i<MAX_INPUT_SURFACE; i++)
    {
        mEncodeHandle.inputSurfaces[i].surface = clCreateBuffer((cl_context)mOveContext,
            CL_MEM_READ_ONLY | CL_MEM_USE_PERSISTENT_MEM_AMD, //CL_MEM_ALLOC_HOST_PTR,
            mInputBufSize,
            NULL,
            &err);
        if (err != CL_SUCCESS)
        {
            VCELog(TEXT("clCreateBuffer returned error %d"), err);
            return false;
        }
    }

    if (mUse444)
    {
        mOutput = clCreateBuffer((cl_context)mOveContext,
            CL_MEM_WRITE_ONLY | CL_MEM_USE_PERSISTENT_MEM_AMD,
            mOutputBufSize,
            NULL,
            &err);
        if (err != CL_SUCCESS)
        {
            VCELog(TEXT("clCreateBuffer returned error %d"), err);
            return false;
        }

        err = clSetKernelArg(mKernel[0], 1, sizeof(cl_mem), &mOutput);
        err |= clSetKernelArg(mKernel[1], 1, sizeof(cl_mem), &mOutput);
        err |= clSetKernelArg(mKernel[0], 2, sizeof(int32_t), &mAlignedSurfaceWidth);
        err |= clSetKernelArg(mKernel[1], 2, sizeof(int32_t), &mAlignedSurfaceWidth);
        if (err != CL_SUCCESS)
        {
            VCELog(TEXT("clSetKernelArg returned error %d"), err);
            return false;
        }
    }

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
    if(!setEncodeConfig(mEncodeHandle.session, &mConfigCtrl))
        return false;
    return true;
}

bool VCEEncoder::encodeClose()
{
    bool oveErr = false;
    cl_int err = CL_SUCCESS;
    OPSurface *inputSurfaces = mEncodeHandle.inputSurfaces;

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

    if (mKernel[0]) clReleaseKernel(mKernel[0]);
    if (mKernel[1]) clReleaseKernel(mKernel[1]);
    if (mProgram) clReleaseProgram(mProgram);

    for (int i = 0; i<2; i++) {
        if (mEncodeHandle.clCmdQueue[i])
            err = clReleaseCommandQueue(mEncodeHandle.clCmdQueue[i]);
        mEncodeHandle.clCmdQueue[i] = NULL;
        if (err != CL_SUCCESS)
        {
            VCELog(TEXT("Error releasing Command queue #%d"), i);
        }
    }

    if (mEncodeHandle.session && 
            !OVEncodeDestroySession(mEncodeHandle.session))
        VCELog(TEXT("Error releasing OVE Session"));

    mEncodeHandle.session = NULL;
    return true;
}