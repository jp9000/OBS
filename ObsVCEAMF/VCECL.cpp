#include "stdafx.h"
#pragma comment(lib, "opencl.lib")

#include "OBS-min.h"
#define VCELog(...) Log(__VA_ARGS__)
#include "Types.h"
#include "VCECL.h"

//#ifndef LINK_TO_CL
//#define LOADPROCDECL(x) \
//do {\
//if ((f_##x = (decltype(&##x))GetProcAddress(hModCL, #x)) == NULL){\
//    VCELog(TEXT("GetProcAddress: cannot find ") TEXT(#x)); \
//    goto error; }\
//} while (0)
//#else
#define FUNDEFDECL(x) decltype(&x) f_##x = ##x
#define LOADPROCDECL(x) f_##x = ##x
//#endif

FUNDEFDECL(clGetPlatformIDs);
FUNDEFDECL(clGetPlatformInfo);
FUNDEFDECL(clCreateContext);
FUNDEFDECL(clReleaseContext);
FUNDEFDECL(clCreateContextFromType);
FUNDEFDECL(clCreateCommandQueue);
FUNDEFDECL(clReleaseCommandQueue);
FUNDEFDECL(clGetEventInfo);
FUNDEFDECL(clCreateBuffer);
FUNDEFDECL(clCreateKernel);
FUNDEFDECL(clReleaseMemObject);
FUNDEFDECL(clReleaseKernel);
FUNDEFDECL(clReleaseProgram);
FUNDEFDECL(clEnqueueMapBuffer);
FUNDEFDECL(clEnqueueNDRangeKernel);
FUNDEFDECL(clEnqueueUnmapMemObject);
FUNDEFDECL(clReleaseEvent);
FUNDEFDECL(clCreateProgramWithSource);
FUNDEFDECL(clBuildProgram);
FUNDEFDECL(clSetKernelArg);
FUNDEFDECL(clGetDeviceInfo);
FUNDEFDECL(clFlush);
FUNDEFDECL(clFinish);
FUNDEFDECL(clWaitForEvents);

bool getPlatform(cl_platform_id &platform)
{
	cl_uint numPlatforms;
	cl_int err = f_clGetPlatformIDs(0, NULL, &numPlatforms);
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
		err = f_clGetPlatformIDs(numPlatforms, platforms, NULL);
		if (CL_SUCCESS != err)
		{
			VCELog(TEXT("clGetPlatformIDs() failed %d\n"), err);
			delete[] platforms;
			return false;
		}

		for (uint32_t i = 0; i < numPlatforms; ++i)
		{
			char pbuf[100];
			err = f_clGetPlatformInfo(platforms[i],
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

	cl_context context = f_clCreateContextFromType(cps,
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
	f_clReleaseContext(context);
	return true;
}

void waitForEvent(cl_event inMapEvt)
{
	cl_int eventStatus = CL_QUEUED;
	cl_int status;

	while (eventStatus > CL_COMPLETE)
	{
		status = f_clGetEventInfo(
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

void mapBuffer(cl_command_queue cmdqueue, InputBuffer &buffer, uint32_t size)
{
	cl_event inMapEvt = 0;
	cl_int   status = CL_SUCCESS;

	buffer.pBuffer = (uint8_t*)f_clEnqueueMapBuffer(cmdqueue,
		buffer.surface,
		CL_FALSE,
		CL_MAP_WRITE_INVALIDATE_REGION,
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

	f_clFlush(cmdqueue);
	waitForEvent(inMapEvt);
	status = f_clReleaseEvent(inMapEvt);
	buffer.isMapped = true;
}

void unmapBuffer(cl_command_queue cmdqueue, InputBuffer &buffer)
{
	cl_event unmapEvent;
	cl_int status = f_clEnqueueUnmapMemObject(cmdqueue,
		buffer.surface,
		buffer.pBuffer,
		0,
		nullptr,
		//nullptr);
		&unmapEvent);

	waitForEvent(unmapEvent);
	status = f_clReleaseEvent(unmapEvent);
	buffer.isMapped = false;
	buffer.pBuffer = nullptr;
}

void mapImage(cl_command_queue cmdqueue, InputBuffer &buffer, uint32_t idx, size_t width, size_t height)
{
	cl_event inMapEvt = 0;
	cl_int   status = CL_SUCCESS;
	size_t origin[3] = { 0, 0, 0 };
	size_t size[3] = { width, height, 1 };
	buffer.yuv_host_ptr[idx] = (uint8_t*)clEnqueueMapImage(cmdqueue,
		buffer.yuv_surfaces[idx],
		true,
		CL_MAP_WRITE_INVALIDATE_REGION,
		origin,
		size, &buffer.yuv_row_pitches[idx], nullptr, 
		0, nullptr,
		nullptr /*&inMapEvt*/, &status);

	if (status != CL_SUCCESS)
	{
		VCELog(TEXT("Failed to map input CL image: %d"), status);
	}

	/*f_clFlush(cmdqueue);
	waitForEvent(inMapEvt);
	status = f_clReleaseEvent(inMapEvt);*/
	buffer.isMapped = true;
}

void unmapImage(cl_command_queue cmdqueue, InputBuffer &buffer, uint32_t idx)
{
	cl_event unmapEvent;
	cl_int status = f_clEnqueueUnmapMemObject(cmdqueue,
		buffer.yuv_surfaces[idx],
		buffer.yuv_host_ptr[idx],
		0,
		nullptr,
		//nullptr);
		&unmapEvent);

	waitForEvent(unmapEvent);
	status = f_clReleaseEvent(unmapEvent);
	buffer.isMapped = false;
	buffer.yuv_host_ptr[idx] = nullptr;
}