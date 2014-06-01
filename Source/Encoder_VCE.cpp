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

//XXX MFT probably becomes the one and only
#include "Main.h"

typedef bool(__cdecl *PVCEINITFUNC)(ConfigFile **appConfig, VideoEncoder*);
typedef bool(__cdecl *PCHECKVCEHARDWARESUPPORT)(bool log);
typedef VideoEncoder* (__cdecl *PCREATEVCEENCODER)(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR);

static HMODULE p_vceModule = NULL;
static PCHECKVCEHARDWARESUPPORT p_checkVCEHardwareSupport = NULL;
static PCREATEVCEENCODER p_createVCEEncoder = NULL;
static PVCEINITFUNC initFunction = NULL;
static bool bUsingMFT = false;

void InitVCEEncoder(bool log = true, bool useMFT = false)
{
    if (p_vceModule != NULL)
    {
        if (useMFT != bUsingMFT)
            OBSMessageBox(NULL, TEXT("To change between OpenVideo and Media Foundation codec,\r\nplease restart OBS."), TEXT("Warning"), 0);
        return;
    }

#ifdef _WIN64
    p_vceModule = LoadLibrary(useMFT ? TEXT("ObsVCEMFT64.dll") : TEXT("ObsVCE64.dll"));
#else
    p_vceModule = LoadLibrary(useMFT ? TEXT("ObsVCEMFT32.dll") : TEXT("ObsVCE32.dll"));
#endif

    TCHAR *modName = useMFT ? TEXT("ObsVCEMFT.dll") : TEXT("ObsVCE.dll");
    if (p_vceModule == NULL)
        p_vceModule = LoadLibrary(modName);

    if (p_vceModule == NULL)
    {
        if (log)
            Log(TEXT("Failed loading %s"), modName);
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
            Log(TEXT("Failed loading all symbols from ObsVCE.dll"));
        goto error;
    }

    if (!initFunction(&AppConfig, NULL))
        goto error;

    if (log)
        Log(TEXT("ObsVCE initialized successfully"));
    bUsingMFT = useMFT;
    return;

error:

    p_checkVCEHardwareSupport = NULL;
    p_createVCEEncoder = NULL;

    if (p_vceModule != NULL)
    {
        FreeLibrary(p_vceModule);
        p_vceModule = NULL;
    }

    if (log)
        Log(TEXT("ObsVCE initialization failed"));
}

bool CheckVCEHardwareSupport(bool log = true, bool useMFT = false)
{
    if(useMFT)
        Log(TEXT("VCE encoding with MFT."));
    InitVCEEncoder(log, useMFT);

    if (p_checkVCEHardwareSupport == NULL)
        return false;

    return p_checkVCEHardwareSupport(log);
}

VideoEncoder* CreateVCEEncoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR, String &errors, bool useMFT)
{
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

    if (useMFT != bUsingMFT)
    {
        OBSMessageBox(GetForegroundWindow(), TEXT("To change between OpenVideo and Media Foundation codec,\r\nplease restart OBS."), TEXT("Warning"), 0);
        return NULL;
    }

    VideoEncoder *encoder = p_createVCEEncoder(fps, width, height, quality, preset, bUse444, colorDesc, maxBitRate, bufferSize, bUseCFR);
    if (!initFunction(&AppConfig, encoder))
    {
        delete encoder;
        return NULL;
    }

    return encoder;
}
