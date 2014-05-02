#include "stdafx.h"
#include "ObsVCE.h"

t_OVEncodeGetDeviceInfo p_OVEncodeGetDeviceInfo = NULL;
t_OVEncodeGetDeviceCap p_OVEncodeGetDeviceCap = NULL;
t_OVCreateOVEHandleFromOPHandle p_OVCreateOVEHandleFromOPHandle = NULL;
t_OVReleaseOVEHandle p_OVReleaseOVEHandle = NULL;
t_OVEncodeAcquireObject p_OVEncodeAcquireObject = NULL;
t_OVEncodeReleaseObject p_OVEncodeReleaseObject = NULL;
t_OVCreateOVEEventFromOPEventHandle p_OVCreateOVEEventFromOPEventHandle = NULL;
t_OVEncodeReleaseOVEEventHandle p_OVEncodeReleaseOVEEventHandle = NULL;
t_OVEncodeCreateSession p_OVEncodeCreateSession = NULL;
t_OVEncodeDestroySession p_OVEncodeDestroySession = NULL;
t_OVEncodeGetPictureControlConfig p_OVEncodeGetPictureControlConfig = NULL;
t_OVEncodeGetRateControlConfig p_OVEncodeGetRateControlConfig = NULL;
t_OVEncodeGetMotionEstimationConfig p_OVEncodeGetMotionEstimationConfig = NULL;
t_OVEncodeGetRDOControlConfig p_OVEncodeGetRDOControlConfig = NULL;
t_OVEncodeSendConfig p_OVEncodeSendConfig = NULL;
t_OVEncodeTask p_OVEncodeTask = NULL;
t_OVEncodeQueryTaskDescription p_OVEncodeQueryTaskDescription = NULL;
t_OVEncodeReleaseTask p_OVEncodeReleaseTask = NULL;

#ifdef OVE_DYN
#define LOADPROC(x) \
do { if ((p_##x = (t_##x)GetProcAddress(hMod, #x)) == NULL){\
    VCELog(TEXT("GetProcAddress: cannot find ") TEXT(#x)); \
    goto error;}\
} while (0)
#else
#define LOADPROC(x) \
do {if ((p_##x = &x) == NULL){\
    VCELog(TEXT("LOADPROC: cannot find ") TEXT(#x)); \
    goto error;}\
} while (0)
#endif

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