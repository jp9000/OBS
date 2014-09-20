#include "stdafx.h"
#include "Encoder.h"

#define EXPORT_C_(x) extern "C" __declspec(dllexport) x __cdecl

ConfigFile **VCEAppConfig = 0;

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
    )
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
#if defined _M_X64 && _MSC_VER == 1800
        //workaround AVX2 bug in VS2013, http://connect.microsoft.com/VisualStudio/feedback/details/811093
        _set_FMA3_enable(0);
#endif
    }
    return TRUE;
}


EXPORT_C_(bool) InitVCEEncoder(ConfigFile **appConfig, VideoEncoder *enc)
{
    VCEAppConfig = appConfig;
    if (enc && ((VCEEncoder*)enc)->Init() != S_OK)
    {
        //! Delete encoder to release COM/MF stuff (done in Encoder_VCE.cpp CreateVCEEncoder)
        return false;
    }
    return true;
}

//FIXME check something...
EXPORT_C_(bool) CheckVCEHardwareSupport(bool log)
{
    return true;
}

EXPORT_C_(VideoEncoder*) CreateVCEEncoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR, void *d3d10)
{

    VCEEncoder *enc = new VCEEncoder(fps, width, height, quality, preset, bUse444, colorDesc, maxBitRate, bufferSize, bUseCFR);
    return enc;
}
