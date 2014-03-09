/********************************************************************************
 Copyright (C) 2014 Timo Rothenpieler <timo@rothenpieler.org>
 
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


#include "Main.h"


typedef bool (__cdecl *PNVENCINITFUNC)(ConfigFile **appConfig);
typedef bool (__cdecl *PCHECKNVENCHARDWARESUPPORT)(bool log);
typedef VideoEncoder* (__cdecl *PCREATENVENCENCODER)(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR);

static HMODULE p_nvencModule = NULL;
static PCHECKNVENCHARDWARESUPPORT p_checkNVENCHardwareSupport = NULL;
static PCREATENVENCENCODER p_createNVENCEncoder = NULL;

void InitNVENCEncoder(bool log=true)
{
	if(p_nvencModule != NULL)
		return;

#ifdef _WIN64
	p_nvencModule = LoadLibrary(TEXT("ObsNvenc64.dll"));
#else
	p_nvencModule = LoadLibrary(TEXT("ObsNvenc32.dll"));
#endif

	if (p_nvencModule == NULL)
		p_nvencModule = LoadLibrary(TEXT("ObsNvenc.dll"));

	if(p_nvencModule == NULL)
	{
		if(log)
			Log(TEXT("Failed loading ObsNvenc.dll"));
		goto error;
	}

	if(log)
		Log(TEXT("Successfully loaded ObsNvenc.dll"));

	p_checkNVENCHardwareSupport = (PCHECKNVENCHARDWARESUPPORT)
		GetProcAddress(p_nvencModule, "CheckNVENCHardwareSupport");
	p_createNVENCEncoder = (PCREATENVENCENCODER)
		GetProcAddress(p_nvencModule, "CreateNVENCEncoder");

	PNVENCINITFUNC initFunction = (PNVENCINITFUNC)
		GetProcAddress(p_nvencModule, "InitNVENCEncoder");

	if(p_checkNVENCHardwareSupport == NULL || p_createNVENCEncoder == NULL || initFunction == NULL)
	{
		if(log)
			Log(TEXT("Failed loading all symbols from ObsNvenc.dll"));
		goto error;
	}

	if(!initFunction(&AppConfig))
		goto error;

	if(log)
		Log(TEXT("ObsNvenc initialized successfully"));

	return;

error:

	p_checkNVENCHardwareSupport = NULL;
	p_createNVENCEncoder = NULL;

	if(p_nvencModule != NULL)
	{
		FreeLibrary(p_nvencModule);
		p_nvencModule = NULL;
	}

	if(log)
		Log(TEXT("ObsNvenc initialization failed"));
}

bool CheckNVENCHardwareSupport(bool log=true)
{
	InitNVENCEncoder(log);

	if(p_checkNVENCHardwareSupport == NULL)
		return false;

	return p_checkNVENCHardwareSupport(log);
}

VideoEncoder* CreateNVENCEncoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR, String &errors)
{
	if (!CheckNVENCHardwareSupport(true))
	{
        if (p_nvencModule)
            errors << Str("Encoder.NVENC.NoHardwareSupport");
        else
            errors << Str("Encoder.NVENC.DllNotFound");
		return NULL;
	}

	if(p_createNVENCEncoder == NULL)
		return NULL;

	return p_createNVENCEncoder(fps, width, height, quality, preset, bUse444, colorDesc, maxBitRate, bufferSize, bUseCFR);
}
