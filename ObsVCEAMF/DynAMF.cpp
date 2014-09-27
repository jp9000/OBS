#include "stdafx.h"

#ifndef AMF_CORE_STATIC
#include <amf\core\Context.h>
#include <amf\components\Component.h>
decltype(&AMFCreateContext) pAMFCreateContext = AMFCreateContext;
decltype(&AMFCreateComponent) pAMFCreateComponent = AMFCreateComponent;
#else
#include "DynAMF.h"

static HMODULE hMod = 0;
static LONG refCount = 0;

#define PROCDECL(x) decltype(x) x = nullptr

#define LOADPROC(x) \
do {\
if ((x = (x##Type)GetProcAddress(hMod, #x)) == NULL){\
	Log(TEXT("GetProcAddress: cannot find ") TEXT(#x)); \
	goto error; }\
} while (0)

namespace amf
{
	const wchar_t* AMF_STD_CALL AMFGetResultText(AMF_RESULT res)
	{
#define TXT(x) return TEXT(#x)
		switch (res)
		{
		case AMF_OK: TXT(AMF_OK);
		case AMF_FAIL: TXT(AMF_FAIL);
		case AMF_UNEXPECTED: TXT(AMF_UNEXPECTED);
		case AMF_ACCESS_DENIED: TXT(AMF_ACCESS_DENIED);
		case AMF_INVALID_ARG: TXT(AMF_INVALID_ARG);
		case AMF_OUT_OF_RANGE: TXT(AMF_OUT_OF_RANGE);
		case AMF_OUT_OF_MEMORY: TXT(AMF_OUT_OF_MEMORY);
		case AMF_INVALID_POINTER: TXT(AMF_INVALID_POINTER);
		case AMF_NO_INTERFACE: TXT(AMF_NO_INTERFACE);
		case AMF_NOT_IMPLEMENTED: TXT(AMF_NOT_IMPLEMENTED);
		case AMF_NOT_SUPPORTED: TXT(AMF_NOT_SUPPORTED);
		case AMF_NOT_FOUND: TXT(AMF_NOT_FOUND);
		case AMF_ALREADY_INITIALIZED: TXT(AMF_ALREADY_INITIALIZED);
		case AMF_NOT_INITIALIZED: TXT(AMF_NOT_INITIALIZED);
		case AMF_INVALID_FORMAT: TXT(AMF_INVALID_FORMAT);
		case AMF_WRONG_STATE: TXT(AMF_WRONG_STATE);
		case AMF_FILE_NOT_OPEN: TXT(AMF_FILE_NOT_OPEN);
		case AMF_NO_DEVICE: TXT(AMF_NO_DEVICE);
		case AMF_DIRECTX_FAILED: TXT(AMF_DIRECTX_FAILED);
		case AMF_OPENCL_FAILED: TXT(AMF_OPENCL_FAILED);
		case AMF_GLX_FAILED: TXT(AMF_GLX_FAILED);
		case AMF_XV_FAILED: TXT(AMF_XV_FAILED);
		case AMF_ALSA_FAILED: TXT(AMF_ALSA_FAILED);
		case AMF_EOF: TXT(AMF_EOF);
		case AMF_REPEAT: TXT(AMF_REPEAT);
		case AMF_NEED_MORE_INPUT: TXT(AMF_NEED_MORE_INPUT);
		case AMF_INPUT_FULL: TXT(AMF_INPUT_FULL);
		case AMF_RESOLUTION_CHANGED: TXT(AMF_RESOLUTION_CHANGED);
		case AMF_INVALID_DATA_TYPE: TXT(AMF_INVALID_DATA_TYPE);
		case AMF_INVALID_RESOLUTION: TXT(AMF_INVALID_RESOLUTION);
		case AMF_CODEC_NOT_SUPPORTED: TXT(AMF_CODEC_NOT_SUPPORTED);
		case AMF_SURFACE_FORMAT_NOT_SUPPORTED: TXT(AMF_SURFACE_FORMAT_NOT_SUPPORTED);
		case AMF_DECODER_NOT_PRESENT: TXT(AMF_DECODER_NOT_PRESENT);
		case AMF_DECODER_SURFACE_ALLOCATION_FAILED: TXT(AMF_DECODER_SURFACE_ALLOCATION_FAILED);
		case AMF_DECODER_NO_FREE_SURFACES: TXT(AMF_DECODER_NO_FREE_SURFACES);
		case AMF_ENCODER_NOT_PRESENT: TXT(AMF_ENCODER_NOT_PRESENT);
		case AMF_DEM_ERROR: TXT(AMF_DEM_ERROR);
		case AMF_DEM_PROPERTY_READONLY: TXT(AMF_DEM_PROPERTY_READONLY);
		case AMF_DEM_REMOTE_DISPLAY_CREATE_FAILED: TXT(AMF_DEM_REMOTE_DISPLAY_CREATE_FAILED);
		case AMF_DEM_START_ENCODING_FAILED: TXT(AMF_DEM_START_ENCODING_FAILED);
		case AMF_DEM_QUERY_OUTPUT_FAILED: TXT(AMF_DEM_QUERY_OUTPUT_FAILED);
		}
		return L"AMF_UNKNOWN_ERROR";
	}
}

//Static init stuff to null
decltype(&AMFCreateContext) pAMFCreateContext = nullptr;
decltype(&AMFCreateComponent) pAMFCreateComponent = nullptr;

PROCDECL(AMFVariantInit);
PROCDECL(AMFVariantClear);
PROCDECL(AMFVariantCompare);
PROCDECL(AMFVariantCopy);
PROCDECL(AMFVariantChangeType);

PROCDECL(AMFVariantAssignBool);
PROCDECL(AMFVariantAssignInt64);
PROCDECL(AMFVariantAssignDouble);
PROCDECL(AMFVariantAssignString);
PROCDECL(AMFVariantAssignWString);
PROCDECL(AMFVariantAssignInterface);

PROCDECL(AMFVariantAssignRect);
PROCDECL(AMFVariantAssignSize);
PROCDECL(AMFVariantAssignPoint);
PROCDECL(AMFVariantAssignRate);
PROCDECL(AMFVariantAssignRatio);
PROCDECL(AMFVariantAssignColor);

//helpers
PROCDECL(AMFVariantDuplicateString);
PROCDECL(AMFVariantFreeString);

PROCDECL(AMFVariantDuplicateWString);
PROCDECL(AMFVariantFreeWString);

void UnbindAMF()
{
	InterlockedDecrement(&refCount);
	if(refCount < 0)
		refCount = 0;
	if (refCount == 0 && hMod)
	{
#ifdef _DEBUG
		OSDebugOut(TEXT("Freeing AMF core library.\n"));
#endif
		FreeLibrary(hMod);
		hMod = NULL;
	}
}

bool BindAMF()
{
	InterlockedIncrement(&refCount);
	if (hMod && AMFVariantFreeWString) return true;

#ifdef _WIN64
	const TCHAR *mod = TEXT("amf-core-windesktop64.dll");
#else
	const TCHAR *mod = TEXT("amf-core-windesktop32.dll");
#endif

	hMod = LoadLibrary(mod);
	if (!hMod)
	{
		Log(TEXT("Cannot load library: %s"), mod);
		return false;
	}

	pAMFCreateContext = (decltype(&AMFCreateContext))GetProcAddress(hMod, "AMFCreateContext");
	pAMFCreateComponent = (decltype(&AMFCreateComponent))GetProcAddress(hMod, "AMFCreateComponent");

	if (!pAMFCreateContext)
	{
		Log(TEXT("GetProcAddress: cannot find AMFCreateContext"));
		goto error;
	}

	if (!pAMFCreateComponent)
	{
		Log(TEXT("GetProcAddress: cannot find AMFCreateComponent"));
		goto error;
	}

	LOADPROC(AMFVariantInit);
	LOADPROC(AMFVariantClear);
	LOADPROC(AMFVariantCompare);
	LOADPROC(AMFVariantCopy);
	LOADPROC(AMFVariantChangeType);

	LOADPROC(AMFVariantAssignBool);
	LOADPROC(AMFVariantAssignInt64);
	LOADPROC(AMFVariantAssignDouble);
	LOADPROC(AMFVariantAssignString);
	LOADPROC(AMFVariantAssignWString);
	LOADPROC(AMFVariantAssignInterface);

	LOADPROC(AMFVariantAssignRect);
	LOADPROC(AMFVariantAssignSize);
	LOADPROC(AMFVariantAssignPoint);
	LOADPROC(AMFVariantAssignRate);
	LOADPROC(AMFVariantAssignRatio);
	LOADPROC(AMFVariantAssignColor);

	//helpers
	LOADPROC(AMFVariantDuplicateString);
	LOADPROC(AMFVariantFreeString);

	LOADPROC(AMFVariantDuplicateWString);
	LOADPROC(AMFVariantFreeWString);

	return true;
error:
	UnbindAMF();
	return false;
}
#endif //AMF_CORE_STATIC