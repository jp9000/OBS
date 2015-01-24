#pragma once
#include <atlbase.h>
#include "d3d9.h"
#include <d3d10.h>
#include <d3d11.h>
//#include <gl/gl.h>
//#include <gl/glext.h>
#include <CL/cl.h>
#include <CL\cl_d3d10.h>
#include <amf\core\Result.h>
#include <vector>

class DeviceOpenCL
{
public:
    DeviceOpenCL();
    virtual ~DeviceOpenCL();
    
	AMF_RESULT Init(IDirect3DDevice9* pD3DDevice9, ID3D10Device *pD3DDevice10, ID3D11Device* pD3DDevice11/*, HGLRC hContextOGL, HDC hDC*/);
    AMF_RESULT Terminate();

    cl_command_queue            GetCommandQueue() { return m_hCommandQueue; }
	cl_mem CreateTexture2D(ID3D10Texture2D *tex);
	cl_int AcquireD3DObj(cl_mem tex);
	cl_int ReleaseD3DObj(cl_mem tex);
	bool RunKernels(cl_mem inTex, cl_mem yTex, cl_mem uvTex, size_t width, size_t height);
private:
    AMF_RESULT FindPlatformID(cl_platform_id &platform);
	bool BuildKernels();

    cl_command_queue            m_hCommandQueue;
    cl_context                  m_hContext;
    cl_kernel                   m_kernel_y;
    cl_kernel                   m_kernel_uv;
    cl_program                  m_program;
    std::vector<cl_device_id>   m_hDeviceIDs;

	clCreateFromD3D10Texture2DKHR_fn   clCreateFromD3D10Texture2DKHR;
	clEnqueueAcquireD3D10ObjectsKHR_fn clEnqueueAcquireD3D10ObjectsKHR;
	clEnqueueReleaseD3D10ObjectsKHR_fn clEnqueueReleaseD3D10ObjectsKHR;
};
