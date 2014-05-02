#include "stdafx.h"
#include "ObsVCE.h"

#ifdef OVE_DYN
t_OVEncodeGetDeviceInfo OVEncodeGetDeviceInfo = NULL;
t_OVEncodeGetDeviceCap OVEncodeGetDeviceCap = NULL;
t_OVCreateOVEHandleFromOPHandle OVCreateOVEHandleFromOPHandle = NULL;
t_OVReleaseOVEHandle OVReleaseOVEHandle = NULL;
t_OVEncodeAcquireObject OVEncodeAcquireObject = NULL;
t_OVEncodeReleaseObject OVEncodeReleaseObject = NULL;
t_OVCreateOVEEventFromOPEventHandle OVCreateOVEEventFromOPEventHandle = NULL;
t_OVEncodeReleaseOVEEventHandle OVEncodeReleaseOVEEventHandle = NULL;
t_OVEncodeCreateSession OVEncodeCreateSession = NULL;
t_OVEncodeDestroySession OVEncodeDestroySession = NULL;
t_OVEncodeGetPictureControlConfig OVEncodeGetPictureControlConfig = NULL;
t_OVEncodeGetRateControlConfig OVEncodeGetRateControlConfig = NULL;
t_OVEncodeGetMotionEstimationConfig OVEncodeGetMotionEstimationConfig = NULL;
t_OVEncodeGetRDOControlConfig OVEncodeGetRDOControlConfig = NULL;
t_OVEncodeSendConfig OVEncodeSendConfig = NULL;
t_OVEncodeTask OVEncodeTask = NULL;
t_OVEncodeQueryTaskDescription OVEncodeQueryTaskDescription = NULL;
t_OVEncodeReleaseTask OVEncodeReleaseTask = NULL;

#define LOADPROC(x) \
do { if ((##x = (t_##x)GetProcAddress(hMod, #x)) == NULL){\
    VCELog(TEXT("GetProcAddress: cannot find ") TEXT(#x)); \
    goto error;}\
} while (0)
#else
#define LOADPROC(x)
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
#ifdef OVE_DYN
    if (hMod && OVEncodeCreateSession)
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
#else
    return true;
#endif
}

#undef LOADPROC