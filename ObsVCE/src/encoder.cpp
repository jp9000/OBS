#include "stdafx.h"
#include "ObsVCE.h"

#define INITPFN(plat, x) \
    x = static_cast<x ## _fn>(f_clGetExtensionFunctionAddressForPlatform(plat, #x));\
    if(!x) { Log(TEXT("Cannot resolve ") TEXT(#x) TEXT(" function")); break;}

static const char *sourceHost =
// global threads: width, height
"__kernel void Y444toNV12_Y(__global uchar4 *input, __global uchar *output, int pitch)"
"{"
"int2 id = (int2)(get_global_id(0), get_global_id(1));"
"int width = get_global_size(0);"
"int offset = id.x + pitch * id.y;"
"output[offset] = input[offset].y;"
"}"

// global threads: width/2, height/2
"__kernel void Y444toNV12_UV(__global uchar4 *input, __global uchar *output, int pitch)"
"{"
"int2 id = (int2)(get_global_id(0), get_global_id(1));"
"int height = get_global_size(1);"

"int src = id.x * 2 + pitch * id.y * 2;"
"int dst = id.x * 2 + pitch * id.y + pitch * height * 2;"

//Sample on Y
"float2 IN0 = convert_float2(input[src + 0].xz);"
"float2 IN1 = convert_float2(input[src + 1].xz);"
//Sample on Y + 1
"float2 IN2 = convert_float2(input[src + 0 + pitch].xz);"
"float2 IN3 = convert_float2(input[src + 1 + pitch].xz);"

"uchar2 OUT = convert_uchar2_sat_rte((IN0 + IN1 + IN2 + IN3) / 4);"
"output[dst]     = OUT.x;"
"output[dst + 1] = OUT.y;"
"}"
;

// Input image format is probably CL_BGRA, CL_UNORM_INT8
static const char *sourceInterop =
"__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;\n"
// global threads: width, height
"__kernel void Y444toNV12_Y(__read_only image2d_t input, __global uchar *output, int pitch)"
"{"
"int2 id = (int2)(get_global_id(0), get_global_id(1));"
"uchar4 in = convert_uchar4_sat_rte(read_imagef(input, sampler, id) * 255);"
"output[id.x + id.y * pitch] = in.y;"
"}"

// global threads: width/2, height/2
"__kernel void Y444toNV12_UV(__read_only image2d_t input, __global uchar *output, int pitch)"
"{"
"int2 id = (int2)(get_global_id(0), get_global_id(1));"
"int2 idx2 = id * 2;"
"int height = get_global_size(1);"
"int dst = id.x * 2 + pitch * id.y + pitch * height * 2;"

"float2 IN00 = read_imagef(input, sampler, idx2 + (int2)(0, 0)).xz;"
"float2 IN10 = read_imagef(input, sampler, idx2 + (int2)(1, 0)).xz;"
"float2 IN01 = read_imagef(input, sampler, idx2 + (int2)(0, 1)).xz;"
"float2 IN11 = read_imagef(input, sampler, idx2 + (int2)(1, 1)).xz;"

"uchar2 OUT = convert_uchar2_sat_rte(((IN00 + IN10 + IN01 + IN11) / 4) * 255);"
"output[dst]     = OUT.y;"
"output[dst + 1] = OUT.x;"
"}"
;

void mapBuffer(OVEncodeHandle &encodeHandle, int i, uint32_t size)
{
    cl_event inMapEvt = 0;
    cl_int   status = CL_SUCCESS;

    encodeHandle.inputSurfaces[i].mapPtr = f_clEnqueueMapBuffer(encodeHandle.clCmdQueue[0],
        (cl_mem)encodeHandle.inputSurfaces[i].surface,
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
    f_clFlush(encodeHandle.clCmdQueue[0]);
    waitForEvent(inMapEvt);
    status = f_clReleaseEvent(inMapEvt);
    encodeHandle.inputSurfaces[i].isMapped = true;
}

void unmapBuffer(OVEncodeHandle &encodeHandle, int surf)
{
    cl_event unmapEvent;
    cl_int status = f_clEnqueueUnmapMemObject(encodeHandle.clCmdQueue[0],
        (cl_mem)encodeHandle.inputSurfaces[surf].surface,
        encodeHandle.inputSurfaces[surf].mapPtr,
        0,
        NULL,
        //NULL);
        &unmapEvent);

    waitForEvent(unmapEvent);
    status = f_clReleaseEvent(unmapEvent);
    encodeHandle.inputSurfaces[surf].isMapped = false;
    encodeHandle.inputSurfaces[surf].mapPtr = NULL;
}

bool VCEEncoder::encodeCreate(uint32_t deviceId)
{
    assert(mInputBufSize);
    bool status;
    cl_int err;
    mOveContext = NULL;
    OVresult res = 0;

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

    std::vector<cl_context_properties> props;
    props.push_back(CL_CONTEXT_PLATFORM);
    props.push_back((cl_context_properties)mDeviceHandle.platform);

    if (mD3D10Device)
    {
        do
        {
            clGetDeviceIDsFromD3D10KHR_fn pClGetDeviceIDsFromD3D10KHR = static_cast<clGetDeviceIDsFromD3D10KHR_fn>(f_clGetExtensionFunctionAddressForPlatform(mDeviceHandle.platform, "clGetDeviceIDsFromD3D10KHR"));
            if (pClGetDeviceIDsFromD3D10KHR == NULL)
            {
                VCELog(TEXT("Cannot resolve ClGetDeviceIDsFromD3D10KHR function"));
                break;
            }

            INITPFN(mDeviceHandle.platform, clCreateFromD3D10Texture2DKHR);
            INITPFN(mDeviceHandle.platform, clEnqueueAcquireD3D10ObjectsKHR);
            INITPFN(mDeviceHandle.platform, clEnqueueReleaseD3D10ObjectsKHR);

            cl_device_id deviceDX10 = 0;
            cl_int err = (*pClGetDeviceIDsFromD3D10KHR)(mDeviceHandle.platform, CL_D3D10_DEVICE_KHR, (void*)mD3D10Device, CL_PREFERRED_DEVICES_FOR_D3D10_KHR, 1, &deviceDX10, NULL);
            if (err)
            {
                VCELog(TEXT("clGetDeviceIDsFromD3D10KHR failed"));
                break;
            }

            if (deviceDX10 != clDeviceID)
            {
                //Well, can do but kind of pointless
                mCanInterop = false;
                VCELog(TEXT("D3D10 and OpenCL device do not match. Disabling interoperability."));
                break;
            }

            props.push_back(CL_CONTEXT_D3D10_DEVICE_KHR);
            props.push_back((cl_context_properties)mD3D10Device);
            mCanInterop = true;
        } while (0);

        if (!mCanInterop)
            VCELog(TEXT("NOT USING D3D10 INTEROP"));
        else
            VCELog(TEXT("Using D3D10 interop"));
    }

    props.push_back(0);

    mOveContext = f_clCreateContext(&props[0], 1, &clDeviceID, 0, 0, &err);
    if (mOveContext == (cl_context)0 || (err != CL_SUCCESS))
    {
        VCELog(TEXT("Cannot create CL context."));
        return false;
    }

    OVE_ENCODE_CAPS encodeCaps;
    OVE_ENCODE_CAPS_H264 encode_cap_full;
    encodeCaps.caps.encode_cap_full = (OVE_ENCODE_CAPS_H264 *)&encode_cap_full;
    status = getDeviceCap(mOveContext, deviceId, &encodeCaps);

    if (!status)
    {
        VCELog(TEXT("OVEncodeGetDeviceCap failed! Maybe too many open or unclosed (due to crashes?) encoder sessions."));
        return false;
    }

    VCELog(TEXT("**** CAPS ****\r\n* Bitrate Max: %d Min: %d"),
        encodeCaps.caps.encode_cap_full->max_bit_rate, 
        encodeCaps.caps.encode_cap_full->min_bit_rate);

    VCELog(TEXT("* Picture size Max: %d Min: %d (Macroblocks (width/16) * (height/16))\r\n* Profiles:"),
        encodeCaps.caps.encode_cap_full->max_picture_size_in_MB, 
        encodeCaps.caps.encode_cap_full->min_picture_size_in_MB);

    for (int i = 0; i< 20/*encodeCaps.caps.encode_cap_full->num_Profile_level*/; i++)
        VCELog(TEXT("*    Prof: %d Level: %d"),
            encodeCaps.caps.encode_cap_full->supported_profile_level[i].profile,
            encodeCaps.caps.encode_cap_full->supported_profile_level[i].level);

    const char *source = sourceHost;
    if (mCanInterop)
        source = sourceInterop;

    if (mUse444 || mCanInterop)
    {
        size_t sourceSize = strlen(source);
        cl_int clstatus = 0;
        mProgram = f_clCreateProgramWithSource((cl_context)mOveContext,
            1,
            (const char**)&source,
            &sourceSize,
            &clstatus);

        if (clstatus != CL_SUCCESS)
        {
            VCELog(TEXT("clCreateProgramWithSource failed."));
            return false;
        }

        std::string flagsStr("-cl-single-precision-constant -cl-mad-enable -cl-fast-relaxed-math -cl-unsafe-math-optimizations");
        //flagsStr.append(" -save-temps");

        clstatus = f_clBuildProgram(mProgram,
            1,
            &clDeviceID,
            flagsStr.c_str(),
            NULL,
            NULL);
        if (clstatus != CL_SUCCESS)
        {
            VCELog(TEXT("clBuildProgram failed."));
            return false;
        }

        mKernel[0] = f_clCreateKernel(mProgram, "Y444toNV12_Y", &clstatus);
        if (clstatus != CL_SUCCESS)
        {
            VCELog(TEXT("clCreateKernel(Y) failed!"));
            return false;
        }

        mKernel[1] = f_clCreateKernel(mProgram, "Y444toNV12_UV", &clstatus);
        if (clstatus != CL_SUCCESS)
        {
            VCELog(TEXT("clCreateKernel(UV) failed!"));
            return false;
        }
    }

    cl_command_queue_properties prop = 0;
    //if (mConfigTable["ProfileKernels"] == 1)
    //    prop |= CL_QUEUE_PROFILING_ENABLE;

    mEncodeHandle.clCmdQueue[0] = f_clCreateCommandQueue((cl_context)mOveContext,
        clDeviceID, prop, &err);
    if (err != CL_SUCCESS)
    {
        VCELog(TEXT("Failed to create command queue #0! Error : %d"), err);
        return false;
    }

    mEncodeHandle.clCmdQueue[1] = NULL;

    if (mCanInterop)
        mInputBufSize = mAlignedSurfaceHeight * mAlignedSurfaceWidth * 3 / 2;
	/*
	Somewhat confusing part how buffers are used currently:
	1.) Interopped: D3D10 -> image2d_t from tex -> convert to NV12 with CL (inputSurfaces) -> VCE
	2.) No interop, no CL conv: D3D10 -> ConvertY444ToNV12 to mapped 'inputSurfaces' -> VCE
	3.) CL conv: D3D10 -> memcpy texture to mapped 'inputSurfaces' -> convert to NV12 with CL (mOutput) -> VCE
	*/
    for (int32_t i = 0; i<MAX_INPUT_SURFACE; i++)
    {
        mEncodeHandle.inputSurfaces[i].surface = f_clCreateBuffer((cl_context)mOveContext,
            CL_MEM_READ_WRITE ,//| CL_MEM_USE_PERSISTENT_MEM_AMD,
            mInputBufSize,
            NULL,
            &err);
        if (err != CL_SUCCESS)
        {
            VCELog(TEXT("clCreateBuffer returned error %d"), err);
            return false;
        }
    }

    //if can't use interop then do CL conversion before encoding
    if (mUse444 && !mCanInterop)
    {
        mOutput = f_clCreateBuffer((cl_context)mOveContext,
            CL_MEM_WRITE_ONLY /*| CL_MEM_USE_PERSISTENT_MEM_AMD*/,
            mOutputBufSize,
            NULL,
            &err);
        if (err != CL_SUCCESS)
        {
            VCELog(TEXT("clCreateBuffer returned error %d"), err);
            return false;
        }

        err = f_clSetKernelArg(mKernel[0], 1, sizeof(cl_mem), &mOutput);
        err |= f_clSetKernelArg(mKernel[1], 1, sizeof(cl_mem), &mOutput);
        err |= f_clSetKernelArg(mKernel[0], 2, sizeof(int32_t), &mAlignedSurfaceWidth);
        err |= f_clSetKernelArg(mKernel[1], 2, sizeof(int32_t), &mAlignedSurfaceWidth);
        if (err != CL_SUCCESS)
        {
            VCELog(TEXT("clSetKernelArg returned error %d"), err);
            return false;
        }
    }

    mEncodeHandle.session = OVEncodeCreateSession(mOveContext,
        deviceId,
        mConfigCtrl.encodeMode,
        mConfigCtrl.profileLevel,
        mConfigCtrl.pictFormat,
        mConfigCtrl.width,
        mConfigCtrl.height,
        mConfigCtrl.priority);

    if (mEncodeHandle.session == NULL)
    {
        VCELog(TEXT("OVEncodeCreateSession failed. Maybe too many encode sessions (or crashes, restart your computer then)."));
        return false;
    }

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
        if (inputSurfaces[i].isMapped && !mCanInterop)
            unmapBuffer(mEncodeHandle, i);

        if (inputSurfaces[i].surface)
            err = f_clReleaseMemObject((cl_mem)inputSurfaces[i].surface);
        inputSurfaces[i].surface = NULL;
        if (err != CL_SUCCESS)
        {
            VCELog(TEXT("clReleaseMemObject returned error %d"), err);
        }
    }

    for (cl_mem mem : mCLMemObjs)
        f_clReleaseMemObject(mem);

    if (mKernel[0]) f_clReleaseKernel(mKernel[0]);
    if (mKernel[1]) f_clReleaseKernel(mKernel[1]);
    if (mProgram) f_clReleaseProgram(mProgram);
    memset(mKernel, 0, sizeof(mKernel));
    mProgram = nullptr;

    for (int i = 0; i<2; i++) {
        if (mEncodeHandle.clCmdQueue[i])
            err = f_clReleaseCommandQueue(mEncodeHandle.clCmdQueue[i]);
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

cl_mem VCEEncoder::CreateTexture2D(ID3D10Texture2D *tex)
{
    cl_int status = 0;
    cl_mem clTex2D = clCreateFromD3D10Texture2DKHR((cl_context)mOveContext, CL_MEM_READ_ONLY, tex, 0, &status);
    if (status != CL_SUCCESS)
        Log(L"Failed to create CL texture from D3D10 texture2D.");

    assert(clTex2D);
    return clTex2D;
}

cl_int VCEEncoder::AcquireD3D10(cl_mem tex)
{
    assert(clEnqueueAcquireD3D10ObjectsKHR);
    cl_int status = clEnqueueAcquireD3D10ObjectsKHR(mEncodeHandle.clCmdQueue[0], 1, &tex, 0, nullptr, nullptr);
    if (status != CL_SUCCESS)
        Log(TEXT("Failed to acquire D3D10 object."));
    return status;
}

cl_int VCEEncoder::ReleaseD3D10(cl_mem tex)
{
    assert(clEnqueueReleaseD3D10ObjectsKHR);
    cl_int status = clEnqueueReleaseD3D10ObjectsKHR(mEncodeHandle.clCmdQueue[0], 1, &tex, 0, nullptr, nullptr);
    if (status != CL_SUCCESS)
        Log(TEXT("Failed to release D3D10 object."));
    return status;
}

bool VCEEncoder::RunKernels(cl_mem inBuf, cl_mem yuvBuf, size_t width, size_t height)
{
    cl_int status = CL_SUCCESS;

    size_t globalThreads[2] = { width, height };

    status = f_clSetKernelArg(mKernel[0], 0, sizeof(cl_mem), &inBuf);
    status |= f_clSetKernelArg(mKernel[1], 0, sizeof(cl_mem), &inBuf);
    if (status != CL_SUCCESS)
    {
        VCELog(TEXT("clSetKernelArg input buffer failed with %d"), status);
        return false;
    }

    status = f_clSetKernelArg(mKernel[0], 1, sizeof(cl_mem), &yuvBuf);
    status |= f_clSetKernelArg(mKernel[1], 1, sizeof(cl_mem), &yuvBuf);
    if (status != CL_SUCCESS)
    {
        VCELog(TEXT("clSetKernelArg output buffer failed with %d"), status);
        return false;
    }

    status = f_clSetKernelArg(mKernel[0], 2, sizeof(int32_t), &mAlignedSurfaceWidth);
    status |= f_clSetKernelArg(mKernel[1], 2, sizeof(int32_t), &mAlignedSurfaceWidth);
    if (status != CL_SUCCESS)
    {
        VCELog(TEXT("clSetKernelArg pitch failed with %d"), status);
        return false;
    }

    status = f_clEnqueueNDRangeKernel(mEncodeHandle.clCmdQueue[0],
        mKernel[0], 2, 0, globalThreads, NULL, 0, 0, NULL);
    if (status != CL_SUCCESS)
    {
        VCELog(TEXT("clEnqueueNDRangeKernel failed with %d"), status);
        return false;
    }

    globalThreads[0] = mWidth / 2;
    globalThreads[1] = mHeight / 2;

    status = f_clEnqueueNDRangeKernel(mEncodeHandle.clCmdQueue[0],
        mKernel[1], 2, 0, globalThreads, NULL, 0, 0, NULL);
    if (status != CL_SUCCESS)
    {
        VCELog(TEXT("clEnqueueNDRangeKernel failed with %d"), status);
        return false;
    }

    //f_clFinish(mEncodeHandle.clCmdQueue[0]);
    return true;
}