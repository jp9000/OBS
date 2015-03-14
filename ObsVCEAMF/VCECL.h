#pragma once
//#include "Types.h"
#define FUNDEF(x) extern decltype(&x) f_##x
FUNDEF(clGetPlatformIDs);
FUNDEF(clGetPlatformInfo);
FUNDEF(clCreateContext);
FUNDEF(clReleaseContext);
FUNDEF(clCreateContextFromType);
FUNDEF(clCreateCommandQueue);
FUNDEF(clReleaseCommandQueue);
FUNDEF(clGetEventInfo);
FUNDEF(clCreateBuffer);
FUNDEF(clCreateKernel);
FUNDEF(clReleaseMemObject);
FUNDEF(clReleaseKernel);
FUNDEF(clReleaseProgram);
FUNDEF(clEnqueueMapBuffer);
FUNDEF(clEnqueueNDRangeKernel);
FUNDEF(clEnqueueUnmapMemObject);
FUNDEF(clReleaseEvent);
FUNDEF(clCreateProgramWithSource);
FUNDEF(clBuildProgram);
FUNDEF(clSetKernelArg);
FUNDEF(clGetDeviceInfo);
FUNDEF(clFlush);
FUNDEF(clFinish);
FUNDEF(clWaitForEvents);
#undef FUNDEF

bool getPlatform(cl_platform_id &platform);
bool gpuCheck(cl_platform_id platform, cl_device_type* dType);
void mapBuffer(cl_command_queue cmdqueue, InputBuffer &buffer, uint32_t size);
void unmapBuffer(cl_command_queue cmdqueue, InputBuffer &buffer);
void mapImage(cl_command_queue cmdqueue, InputBuffer &buffer, uint32_t idx, size_t width, size_t height);
void unmapImage(cl_command_queue cmdqueue, InputBuffer &buffer, uint32_t idx);