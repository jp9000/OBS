#pragma once
#include "..\..\amf\core\Result.h"
//__declspec(dllimport) void __cdecl Log(const TCHAR*, ...);

#define LOG_AMF_ERROR(err, text) \
	do{ \
		if( (err) != AMF_OK) \
		{ \
			Log(L"%s Error: %s", text, amf::AMFGetResultText((err)));\
		} \
	}while(0)

#define CHECK_RETURN(exp, err, text) \
	do{ \
		if((exp) == false) \
		{  \
			LOG_AMF_ERROR(err, text);\
			return (err); \
		} \
	}while(0)

#define CHECK_AMF_ERROR_RETURN(err, text) \
	do{ \
		if((err) != AMF_OK) \
		{  \
			LOG_AMF_ERROR(err, text);\
			return (err); \
		} \
	}while(0)

#define CHECK_HRESULT_ERROR_RETURN(err, text) \
	do{ \
		if(FAILED(err)) \
		{  \
			Log(L"%s HRESULT Error: %08X", text, err); \
			return AMF_FAIL; \
		} \
	}while(0)

#define CHECK_OPENCL_ERROR_RETURN(err, text) \
	do{ \
		if(err) \
		{  \
			Log(L"%s OpenCL Error: %d", text, err); \
			return AMF_FAIL; \
		} \
	}while(0)
