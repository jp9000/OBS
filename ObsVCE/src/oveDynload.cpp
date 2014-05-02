#include "stdafx.h"
#include "ObsVCE.h"

OVEncodeGetDeviceInfo p_OVEncodeGetDeviceInfo = NULL;
OVEncodeGetDeviceCap p_OVEncodeGetDeviceCap = NULL;
OVCreateOVEHandleFromOPHandle p_OVCreateOVEHandleFromOPHandle = NULL;
OVReleaseOVEHandle p_OVReleaseOVEHandle = NULL;
OVEncodeAcquireObject p_OVEncodeAcquireObject = NULL;
OVEncodeReleaseObject p_OVEncodeReleaseObject = NULL;
OVCreateOVEEventFromOPEventHandle p_OVCreateOVEEventFromOPEventHandle = NULL;
OVEncodeReleaseOVEEventHandle p_OVEncodeReleaseOVEEventHandle = NULL;
OVEncodeCreateSession p_OVEncodeCreateSession = NULL;
OVEncodeDestroySession p_OVEncodeDestroySession = NULL;
OVEncodeGetPictureControlConfig p_OVEncodeGetPictureControlConfig = NULL;
OVEncodeGetRateControlConfig p_OVEncodeGetRateControlConfig = NULL;
OVEncodeGetMotionEstimationConfig p_OVEncodeGetMotionEstimationConfig = NULL;
OVEncodeGetRDOControlConfig p_OVEncodeGetRDOControlConfig = NULL;
OVEncodeSendConfig p_OVEncodeSendConfig = NULL;
OVEncodeTask p_OVEncodeTask = NULL;
OVEncodeQueryTaskDescription p_OVEncodeQueryTaskDescription = NULL;
OVEncodeReleaseTask p_OVEncodeReleaseTask = NULL;

#define LOADPROC(x) \
do { if ((p_##x = (x)GetProcAddress(hMod, #x)) == NULL){\
    VCELog(TEXT("GetProcAddress: cannot find ") TEXT(#x) TEXT("\r\n")); \
    goto error;}\
} while (0)

static HMODULE hMod = NULL;

void deinitOVE()
{
    if (hMod)
    {
        FreeLibrary(hMod);
        hMod = NULL;
    }
}

bool initOVE()
{
    if (hMod && p_OVEncodeCreateSession)
        return true;

#ifdef _WIN64
    hMod = LoadLibrary(TEXT("OpenVideo64.dll"));
#else
    hMod = LoadLibrary(TEXT("OpenVideo.dll"));
#endif

    if (!hMod)
        return false;

    LOADPROC(OVEncodeGetDeviceInfo);
    LOADPROC(OVEncodeGetDeviceCap);
    LOADPROC(OVCreateOVEHandleFromOPHandle);
    LOADPROC(OVReleaseOVEHandle);
    LOADPROC(OVEncodeAcquireObject);
    LOADPROC(OVEncodeReleaseObject);
    LOADPROC(OVCreateOVEEventFromOPEventHandle);
    LOADPROC(OVEncodeReleaseOVEEventHandle);
    LOADPROC(OVEncodeCreateSession);
    LOADPROC(OVEncodeDestroySession);
    LOADPROC(OVEncodeGetPictureControlConfig);
    LOADPROC(OVEncodeGetRateControlConfig);
    LOADPROC(OVEncodeGetMotionEstimationConfig);
    LOADPROC(OVEncodeGetRDOControlConfig);
    LOADPROC(OVEncodeSendConfig);
    LOADPROC(OVEncodeTask);
    LOADPROC(OVEncodeQueryTaskDescription);
    LOADPROC(OVEncodeReleaseTask);
    return true;

error:
    deinitOVE();
    return false;
}

#undef LOADPROC