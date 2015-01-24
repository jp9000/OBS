/********************************************************************************
Copyright (C) 2014 Timo Rothenpieler <timo@rothenpieler.org>
Copyright (C) 2014 jackun <jackun@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/

//XXX AMF probably becomes the one and only
#include "Main.h"

typedef bool(__cdecl *PVCEINITFUNC)(ConfigFile **appConfig, VideoEncoder*);
typedef bool(__cdecl *PCHECKVCEHARDWARESUPPORT)(bool log);
typedef VideoEncoder* (__cdecl *PCREATEVCEENCODER)(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR, ID3D10Device *d3d);

static HMODULE p_vceModule = NULL;
static PCHECKVCEHARDWARESUPPORT p_checkVCEHardwareSupport = NULL;
static PCREATEVCEENCODER p_createVCEEncoder = NULL;
static PVCEINITFUNC initFunction = NULL;
static bool bUsingMFT = false;

static void UnloadModule()
{
    if (p_vceModule)
        FreeLibrary(p_vceModule);
    p_vceModule = NULL;
    p_checkVCEHardwareSupport = NULL;
    p_createVCEEncoder = NULL;
    initFunction = NULL;
}

void InitVCEEncoder(bool log = true, bool useMFT = false)
{
    TCHAR *modName = useMFT ? TEXT("ObsVCEAMF.dll") /*TEXT("ObsVCEMFT.dll")*/ : TEXT("ObsVCE.dll");

    if ((p_vceModule != NULL) && (useMFT != bUsingMFT))
    {
        UnloadModule();
    }

#ifdef _WIN64
    p_vceModule = LoadLibrary(useMFT ? TEXT("ObsVCEMFT64.dll") : TEXT("ObsVCE64.dll"));
#else
    p_vceModule = LoadLibrary(useMFT ? TEXT("ObsVCEMFT32.dll") : TEXT("ObsVCE32.dll"));
#endif

    if (p_vceModule == NULL)
        p_vceModule = LoadLibrary(modName);

    if (p_vceModule == NULL)
    {
        Log(TEXT("Failed to load %s"), modName);
        goto error;
    }

    if (log)
        Log(TEXT("Successfully loaded %s"), modName);

    p_checkVCEHardwareSupport = (PCHECKVCEHARDWARESUPPORT)
        GetProcAddress(p_vceModule, "CheckVCEHardwareSupport");
    p_createVCEEncoder = (PCREATEVCEENCODER)
        GetProcAddress(p_vceModule, "CreateVCEEncoder");

    initFunction = (PVCEINITFUNC)
        GetProcAddress(p_vceModule, "InitVCEEncoder");

    if (p_checkVCEHardwareSupport == NULL || p_createVCEEncoder == NULL || initFunction == NULL)
    {
        if (log)
            Log(TEXT("Failed loading all symbols from %s"), modName);
        goto error;
    }

    if (!initFunction(&AppConfig, NULL))
        goto error;

    if (log)
        Log(TEXT("%s initialized successfully"), modName);
    bUsingMFT = useMFT;
    return;

error:

    UnloadModule();
    if (log)
        Log(TEXT("%s initialization failed"), modName);
}

bool CheckVCEHardwareSupport(bool log = true, bool useMFT = false)
{
    if(useMFT)
        Log(TEXT("VCE encoding with AMF."));
    InitVCEEncoder(log, useMFT);

    if (p_checkVCEHardwareSupport == NULL)
        return false;

    return p_checkVCEHardwareSupport(log);
}

VideoEncoder* CreateVCEEncoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR, String &errors, bool useMFT)
{
    if (useMFT != bUsingMFT)
    {
        InitVCEEncoder(true, useMFT);
    }

    if (!CheckVCEHardwareSupport(true, useMFT))
    {
        if (p_vceModule)
            errors << Str("Encoder.VCE.NoHardwareSupport");
        else
            errors << Str("Encoder.VCE.DllNotFound");
        return NULL;
    }

    if (bUse444 && bUsingMFT)
    {
        errors << Str("Encoder.VCE.YUV444IsUnsupported");
        return NULL;
    }

    //TODO Maybe can do more, as long as width*height <= 1920*1088?
    if ((uint32_t)width > 1920 || (uint32_t)height > 1088)
    {
        errors << Str("Encoder.VCE.UnsupportedResolution");
        return NULL;
    }

    if (p_createVCEEncoder == NULL || initFunction == NULL)
        return NULL;

    VideoEncoder *encoder = p_createVCEEncoder(fps, width, height, quality, preset, bUse444, colorDesc, maxBitRate, bufferSize, bUseCFR, GetD3D());
    if (!initFunction(&AppConfig, encoder))
    {
        delete encoder;
        return NULL;
    }

    return encoder;
}