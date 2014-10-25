#include "stdafx.h"
#include "..\inc\CmdLogger.h"
#include "..\inc\DeviceOpenCL.h"
#include <CL/cl_d3d11.h>
#include <CL/cl_d3d10.h>
#include <CL/cl_dx9_media_sharing.h>
//#include <CL/cl_gl.h>
#pragma comment(lib, "opencl.lib")
#define USE_CL_BINARY

#define INITPFN(x) \
	x = static_cast<x ## _fn>(clGetExtensionFunctionAddressForPlatform(platformID, #x));\
	if(!x) { Log(TEXT("Cannot resolve ") TEXT(#x) TEXT(" function")); return AMF_FAIL;}

// Input format is probably CL_BGRA, CL_UNORM_INT8 which means only read_imagef works.
static const char *source =
//"#pragma OPENCL EXTENSION cl_khr_d3d10_sharing  : enable\n"
"__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;\n"
// global threads: width, height, output format is CL_R, UINT8
"__kernel void Y444toNV12_Y(__read_only image2d_t input, __write_only image2d_t output)"
"{"
"int2 id = (int2)(get_global_id(0), get_global_id(1));"
"uint4 in = convert_uint4_sat_rte(read_imagef(input, sampler, id) * 255);"
"write_imageui(output, id, (uint4)(in.y, 0, 0, 255));"
"}\n"

// global threads: width/2, height/2, output format is CL_RG, UINT8
"__kernel void Y444toNV12_UV(__read_only image2d_t input, __write_only image2d_t output)"
"{"
"int2 id = (int2)(get_global_id(0), get_global_id(1));"
"int2 idx2 = id * 2;"

"float2 IN00 = read_imagef(input, sampler, idx2               ).xz;"
"float2 IN10 = read_imagef(input, sampler, idx2 + (int2)(1, 0)).xz;"
"float2 IN01 = read_imagef(input, sampler, idx2 + (int2)(0, 1)).xz;"
"float2 IN11 = read_imagef(input, sampler, idx2 + (int2)(1, 1)).xz;"

//"uchar2 OUT = convert_uchar2_sat_rte((IN00 + IN10 + IN01 + IN11) / 4);"
"uint2 OUT = convert_uint2_sat_rte(((IN00 + IN10 + IN01 + IN11) / 4) * 255);"
"write_imageui(output, id, (uint4)(OUT.y, OUT.x, 0, 255));"
"}"
;

DeviceOpenCL::DeviceOpenCL() :
	m_hCommandQueue(0),
	m_hContext(0),
	m_kernel_y(0),
	m_kernel_uv(0),
	m_program(0)
{
}

DeviceOpenCL::~DeviceOpenCL()
{
	Terminate();
}

AMF_RESULT DeviceOpenCL::Init(IDirect3DDevice9* pD3DDevice9, ID3D10Device *pD3DDevice10,ID3D11Device* pD3DDevice11/*, HGLRC hContextOGL, HDC hDC*/)
{
	AMF_RESULT res = AMF_OK;
	cl_int status = 0;
	cl_platform_id platformID = 0;
	cl_device_id interoppedDeviceID = 0;
	char *exts;
	size_t extSize;

	// get AMD platform:
	res = FindPlatformID(platformID);
	CHECK_AMF_ERROR_RETURN(res, L"FindPlatformID() failed");

	status = clGetPlatformInfo(platformID, CL_PLATFORM_EXTENSIONS, 0, nullptr, &extSize);
	if (!status)
	{
		exts = new char[extSize];
		status = clGetPlatformInfo(platformID, CL_PLATFORM_EXTENSIONS, extSize, exts, nullptr);
		Log(L"CL Platform Extensions: %S", exts);
		delete[] exts;
	}

	std::vector<cl_context_properties> cps;
	
	// add devices if needed
	if(pD3DDevice11 != NULL)
	{
		clGetDeviceIDsFromD3D11KHR_fn pClGetDeviceIDsFromD3D11KHR = static_cast<clGetDeviceIDsFromD3D11KHR_fn>(clGetExtensionFunctionAddressForPlatform(platformID, "clGetDeviceIDsFromD3D11KHR"));
		if(pClGetDeviceIDsFromD3D11KHR == NULL)
		{
			Log(L"Cannot resolve ClGetDeviceIDsFromD3D11KHR function");
			return AMF_FAIL;
		}

		cl_device_id deviceDX11 = 0;
		status = (*pClGetDeviceIDsFromD3D11KHR)(platformID, CL_D3D11_DEVICE_KHR, (void*)pD3DDevice11, CL_PREFERRED_DEVICES_FOR_D3D11_KHR, 1,&deviceDX11,NULL);
		CHECK_OPENCL_ERROR_RETURN(status, L"pClGetDeviceIDsFromD3D11KHR() failed");
		interoppedDeviceID = deviceDX11;
		m_hDeviceIDs.push_back(deviceDX11);
		cps.push_back(CL_CONTEXT_D3D11_DEVICE_KHR);
		cps.push_back((cl_context_properties)pD3DDevice11);
	}
	if (pD3DDevice10 != NULL)
	{
		clGetDeviceIDsFromD3D10KHR_fn pClGetDeviceIDsFromD3D10KHR = static_cast<clGetDeviceIDsFromD3D10KHR_fn>(clGetExtensionFunctionAddressForPlatform(platformID, "clGetDeviceIDsFromD3D10KHR"));
		if (pClGetDeviceIDsFromD3D10KHR == NULL)
		{
			Log(L"Cannot resolve ClGetDeviceIDsFromD3D10KHR function");
			return AMF_FAIL;
		}

		INITPFN(clCreateFromD3D10Texture2DKHR);
		INITPFN(clEnqueueAcquireD3D10ObjectsKHR);
		INITPFN(clEnqueueReleaseD3D10ObjectsKHR);

		cl_device_id deviceDX10 = 0;
		status = (*pClGetDeviceIDsFromD3D10KHR)(platformID, CL_D3D10_DEVICE_KHR, (void*)pD3DDevice10, CL_PREFERRED_DEVICES_FOR_D3D10_KHR, 1, &deviceDX10, NULL);
		CHECK_OPENCL_ERROR_RETURN(status, L"pClGetDeviceIDsFromD3D10KHR() failed");
		interoppedDeviceID = deviceDX10;
		m_hDeviceIDs.push_back(deviceDX10);
		cps.push_back(CL_CONTEXT_D3D10_DEVICE_KHR);
		cps.push_back((cl_context_properties)pD3DDevice10);

		status = clGetDeviceInfo(deviceDX10, CL_DEVICE_EXTENSIONS, 0, nullptr, &extSize);
		if (!status)
		{
			exts = new char[extSize];
			status = clGetDeviceInfo(deviceDX10, CL_DEVICE_EXTENSIONS, extSize, exts, nullptr);
			Log(L"CL Device Extensions: %S", exts);
			delete[] exts;
		}

	}
	if(pD3DDevice9 != NULL)
	{
		clGetDeviceIDsFromDX9MediaAdapterKHR_fn pclGetDeviceIDsFromDX9MediaAdapterKHR = static_cast<clGetDeviceIDsFromDX9MediaAdapterKHR_fn>(clGetExtensionFunctionAddressForPlatform(platformID, "clGetDeviceIDsFromDX9MediaAdapterKHR"));
		if(pclGetDeviceIDsFromDX9MediaAdapterKHR == NULL)
		{
			Log(L"Cannot resolve clGetDeviceIDsFromDX9MediaAdapterKHR function");
			return AMF_FAIL;
		}
		cl_dx9_media_adapter_type_khr mediaAdapterType = CL_ADAPTER_D3D9EX_KHR;
		cl_device_id deviceDX9 = 0;
		status = (*pclGetDeviceIDsFromDX9MediaAdapterKHR)(platformID, 1, &mediaAdapterType, &pD3DDevice9,CL_PREFERRED_DEVICES_FOR_DX9_MEDIA_ADAPTER_KHR, 1, &deviceDX9, NULL);
		CHECK_OPENCL_ERROR_RETURN(status, L"pclGetDeviceIDsFromDX9MediaAdapterKHR() failed");

		interoppedDeviceID = deviceDX9;
		m_hDeviceIDs.push_back(deviceDX9);

		cps.push_back(CL_CONTEXT_ADAPTER_D3D9EX_KHR);
		cps.push_back((cl_context_properties)pD3DDevice9);
	}
	//if(hContextOGL != NULL)
	//{
	//    clGetGLContextInfoKHR_fn pclGetGLContextInfoKHR = static_cast<clGetGLContextInfoKHR_fn>(clGetExtensionFunctionAddressForPlatform(platformID, "clGetGLContextInfoKHR"));
	//    if(pclGetGLContextInfoKHR == NULL)
	//    {
	//        Log(L"Cannot resolve clGetGLContextInfoKHR function");
	//        return AMF_FAIL;
	//    }
	//    std::vector<cl_context_properties> gl_cps;
	//    gl_cps.push_back(CL_CONTEXT_PLATFORM);
	//    gl_cps.push_back((cl_context_properties)platformID);
	//    gl_cps.push_back(CL_GL_CONTEXT_KHR);
	//    gl_cps.push_back((cl_context_properties)hContextOGL);
	//    gl_cps.push_back(CL_WGL_HDC_KHR);
	//    gl_cps.push_back((cl_context_properties)hDC);
	//    gl_cps.push_back(0);

	//    cl_device_id deviceGL = NULL;
	//    status = pclGetGLContextInfoKHR(&gl_cps[0], CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof(cl_device_id), &deviceGL, NULL);
	//    CHECK_OPENCL_ERROR_RETURN(status, L"clGetGLContextInfoKHR() failed");
	//    interoppedDeviceID = deviceGL;
	//    m_hDeviceIDs.push_back(deviceGL);
	//    
	//    cps.push_back(CL_GL_CONTEXT_KHR);
	//    cps.push_back((cl_context_properties)hContextOGL);

	//    cps.push_back(CL_WGL_HDC_KHR);
	//    cps.push_back((cl_context_properties)hDC);
	//}
	cps.push_back(CL_CONTEXT_INTEROP_USER_SYNC);
	cps.push_back(CL_TRUE);

	cps.push_back(CL_CONTEXT_PLATFORM);
	cps.push_back((cl_context_properties)platformID);
	cps.push_back(0);

	if(interoppedDeviceID == NULL)
	{
		status = clGetDeviceIDs((cl_platform_id)platformID, CL_DEVICE_TYPE_GPU, 1, (cl_device_id*)&interoppedDeviceID, NULL);
		CHECK_OPENCL_ERROR_RETURN(status, L"clGetDeviceIDs() failed");
		m_hDeviceIDs.push_back(interoppedDeviceID);
	}
	//if(hContextOGL != 0)
	//{
	//    wglMakeCurrent(hDC, hContextOGL);
	//}
	m_hContext = clCreateContext(&cps[0], 1, &interoppedDeviceID, NULL, NULL, &status);
	/*if(hContextOGL != 0)
	{
		wglMakeCurrent(NULL, NULL);
	}*/
	CHECK_OPENCL_ERROR_RETURN(status, L"clCreateContext() failed");

	m_hCommandQueue = clCreateCommandQueue(m_hContext, interoppedDeviceID, (cl_command_queue_properties)NULL, &status);
	CHECK_OPENCL_ERROR_RETURN(status, L"clCreateCommandQueue() failed");

	/*for (auto id : m_hDeviceIDs)
		Log(L"device %p", id);*/

	if (pD3DDevice10 && !BuildKernels())
		return AMF_FAIL;

	return AMF_OK;
}

AMF_RESULT DeviceOpenCL::Terminate()
{
	if (m_kernel_uv)
	{
		clReleaseKernel(m_kernel_y);
		clReleaseKernel(m_kernel_uv);
		clReleaseProgram(m_program);
		m_kernel_uv = nullptr;
	}

	if(m_hCommandQueue != 0)
	{
		clReleaseCommandQueue(m_hCommandQueue);
		m_hCommandQueue = NULL;
	}
	if(m_hDeviceIDs.size() != 0)
	{
		for(std::vector<cl_device_id>::iterator it= m_hDeviceIDs.begin(); it != m_hDeviceIDs.end(); it++)
		{
			clReleaseDevice(*it);
		}
		m_hDeviceIDs.clear();
	}
	if(m_hContext != 0)
	{
		clReleaseContext(m_hContext);
		m_hContext = NULL;
	}
	return AMF_OK;
}

AMF_RESULT DeviceOpenCL::FindPlatformID(cl_platform_id &platform)
{
	cl_int status = 0;
	cl_uint numPlatforms = 0;
	status = clGetPlatformIDs(0, NULL, &numPlatforms);
	CHECK_OPENCL_ERROR_RETURN(status, L"clGetPlatformIDs() failed");
	if(numPlatforms == 0)
	{
		Log(TEXT("clGetPlatformIDs() returned 0 platforms: "));
		return AMF_FAIL;
	}
	std::vector<cl_platform_id> platforms;
	platforms.resize(numPlatforms);
	status = clGetPlatformIDs(numPlatforms, &platforms[0], NULL);
	CHECK_OPENCL_ERROR_RETURN(status, L"clGetPlatformIDs() failed");
	bool bFound = false;
	for(cl_uint i = 0; i < numPlatforms; ++i)
	{
		char pbuf[1000];
		status = clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, sizeof(pbuf), pbuf, NULL);
		CHECK_OPENCL_ERROR_RETURN(status, L"clGetPlatformInfo() failed");
		if(!strcmp(pbuf, "Advanced Micro Devices, Inc."))
		{
			platform = platforms[i];
			bFound = true;
			return AMF_OK;
		}
	}
	return AMF_FAIL;
}

cl_mem DeviceOpenCL::CreateTexture2D(ID3D10Texture2D *tex)
{
	cl_int status = 0;
	cl_mem clTex2D = clCreateFromD3D10Texture2DKHR(m_hContext, CL_MEM_READ_ONLY, tex, 0, &status);
	if (status != CL_SUCCESS)
		Log(L"Failed to create CL texture from D3D10 texture2D.");

	assert(clTex2D);
	return clTex2D;
}

cl_int DeviceOpenCL::AcquireD3D10(cl_mem tex)
{
	assert(clEnqueueAcquireD3D10ObjectsKHR);
	cl_int status = clEnqueueAcquireD3D10ObjectsKHR(m_hCommandQueue, 1, &tex, 0, nullptr, nullptr);
	if (status != CL_SUCCESS)
		Log(TEXT("Failed to acquire D3D10 object"));
	return status;
}

cl_int DeviceOpenCL::ReleaseD3D10(cl_mem tex)
{
	assert(clEnqueueReleaseD3D10ObjectsKHR);
	cl_int status = clEnqueueReleaseD3D10ObjectsKHR(m_hCommandQueue, 1, &tex, 0, nullptr, nullptr);
	if (status != CL_SUCCESS)
		Log(TEXT("Failed to release D3D10 object"));
	return status;
}

bool DeviceOpenCL::RunKernels(cl_mem inTex, cl_mem yTex, cl_mem uvTex, size_t width, size_t height)
{
	cl_int status = CL_SUCCESS;
	size_t globalThreads[] = { width, height };

	status = clSetKernelArg(m_kernel_y, 0, sizeof(cl_mem), &inTex);
	status |= clSetKernelArg(m_kernel_y, 1, sizeof(cl_mem), &yTex);
	if (status != CL_SUCCESS)
	{
		Log(L"Failed to set Y kernel arguments.");
		return false;
	}

	status = clSetKernelArg(m_kernel_uv, 0, sizeof(cl_mem), &inTex);
	status |= clSetKernelArg(m_kernel_uv, 1, sizeof(cl_mem), &uvTex);
	if (status != CL_SUCCESS)
	{
		Log(L"Failed to set UV kernel arguments.");
		return false;
	}

	status = clEnqueueNDRangeKernel(m_hCommandQueue, m_kernel_y, 2, nullptr, globalThreads, nullptr, 0, nullptr, nullptr);
	if (status != CL_SUCCESS)
	{
		Log(L"Failed to enqueue Y kernel.");
		return false;
	}

	//TODO off by one probably
	globalThreads[0] = width & 1 ? (width + 1) / 2 : width / 2;
	globalThreads[1] = height & 1 ? (height - 1) / 2 : height / 2;

	status = clEnqueueNDRangeKernel(m_hCommandQueue, m_kernel_uv, 2, nullptr, globalThreads, nullptr, 0, nullptr, nullptr);
	if (status != CL_SUCCESS)
	{
		Log(L"Failed to enqueue Y kernel.");
		return false;
	}

	//FIXME Sub 1ms can't be right...
	clFlush(m_hCommandQueue);
	//Finishing here so that it would count toward "GPU download and conversion" profile
	//clFinish(m_hCommandQueue);
	return true;
}

bool DeviceOpenCL::BuildKernels()
{
	if (m_hDeviceIDs.empty())
		return false;

	size_t size = strlen(source);
	cl_int status = 0, err = 0;
	bool usingBinary = false;

	//Even when kernels are pretty simple, there is some speed up
#pragma region Load program binary
#ifdef USE_CL_BINARY
	std::hash<std::string> hash_fn;
	size_t hash = hash_fn(std::string(source));
	String binPath = OBSGetAppDataPath();

	size_t devsize;
	status = clGetDeviceInfo(m_hDeviceIDs[0], CL_DEVICE_NAME, 0, nullptr, &devsize);
	if (!status)
	{
		char* devname = new char[devsize];
		status = clGetDeviceInfo(m_hDeviceIDs[0], CL_DEVICE_NAME, devsize, devname, nullptr);
		binPath += FormattedString(TEXT("\\%S_%08X.bin"), devname, hash);
		delete[] devname;
		if (OSFileExists(binPath))
			usingBinary = true;
	}
	else
		binPath.Clear();

	// Try loading CL program binary
	m_program = nullptr;
	if (usingBinary && binPath.IsValid())
	{
		size_t prgsize;
		std::vector<unsigned char> prg;
		FILE *hf = nullptr;
		errno_t err = _wfopen_s(&hf, binPath.Array(), L"rb");
		if (!err)
		{
			fseek(hf, 0, SEEK_END);
			prgsize = ftell(hf);
			fseek(hf, 0, SEEK_SET);
			prg.resize(prgsize);
			if (fread(&prg[0], 1, prg.size(), hf) == prgsize)
			{
				auto ptr = (const unsigned char*)&prg[0];
				m_program = clCreateProgramWithBinary(m_hContext, 1, &m_hDeviceIDs[0],
					&prgsize, &ptr, &status, &err);
			}
			fclose(hf);
		}
	}
#endif
#pragma endregion

	if (!m_program)
		m_program = clCreateProgramWithSource(m_hContext, 1, (const char**)&source, &size, &status);

	std::string flagsStr("");// ("-save-temps ");
	flagsStr.append("-cl-single-precision-constant -cl-mad-enable "
		"-cl-fast-relaxed-math -cl-unsafe-math-optimizations ");

	/* create a cl program executable for all the devices specified */
	status = clBuildProgram(m_program, 1, &m_hDeviceIDs[0], flagsStr.c_str(), NULL, NULL);
	if (status != CL_SUCCESS)
	{
		Log(TEXT("clBuildProgram failed."));

		size_t size = 0;
		if (status == CL_BUILD_PROGRAM_FAILURE && 
			(clGetProgramBuildInfo(m_program, m_hDeviceIDs[0], CL_PROGRAM_BUILD_LOG, 0, nullptr, &size) == CL_SUCCESS))
		{
			char* log = new char[size];
			status = clGetProgramBuildInfo(m_program, m_hDeviceIDs[0], CL_PROGRAM_BUILD_LOG, size, log, &size);
			if (!status)
				Log(TEXT("%S"), log);
			delete[] log;
		}
		return false;
	}

#pragma region Save program binary
	// Save binary
#ifdef USE_CL_BINARY
	if (!usingBinary && binPath.IsValid())
	{
		size_t prgsize = 0;
		clGetProgramInfo(m_program, CL_PROGRAM_BINARY_SIZES, sizeof(size_t), &prgsize, nullptr);

		uint8_t *prg = new uint8_t[prgsize];
		clGetProgramInfo(m_program, CL_PROGRAM_BINARIES, sizeof(uint8_t*), &prg, nullptr);

		FILE *hf = nullptr;
		errno_t err = _wfopen_s(&hf, binPath.Array(), L"wb");
		if (!err)
		{
			fwrite(prg, 1, prgsize, hf);
			fclose(hf);
		}
	}
#endif
#pragma endregion

	m_kernel_y = clCreateKernel(m_program, "Y444toNV12_Y", &status);
	if (status != CL_SUCCESS)
	{
		Log(TEXT("clCreateKernel(Y) failed!"));
		return false;
	}

	m_kernel_uv = clCreateKernel(m_program, "Y444toNV12_UV", &status);
	if (status != CL_SUCCESS)
	{
		Log(TEXT("clCreateKernel(UV) failed!"));
		return false;
	}

	return true;
}