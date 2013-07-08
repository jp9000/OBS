/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

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


#pragma once

#include "OBSApi.h"

#include <dshow.h>
#include <Amaudio.h>
#include <Dvdmedia.h>

#include "MediaInfoStuff.h"
#include "CaptureFilter.h"
#include "DeviceSource.h"
#include "resource.h"

//-----------------------------------------------------------

extern HINSTANCE hinstMain;

IBaseFilter* GetDeviceByValue(const IID &enumType, WSTR lpType, CTSTR lpName, WSTR lpType2=NULL, CTSTR lpName2=NULL);
IPin* GetOutputPin(IBaseFilter *filter, const GUID *majorType);
void GetOutputList(IPin *curPin, List<MediaOutputInfo> &outputInfoList);
bool GetClosestResolutionFPS(List<MediaOutputInfo> &outputList, SIZE &resolution, UINT64 &frameInterval, bool bPrioritizeFPS);

extern LocaleStringLookup *pluginLocale;
#define PluginStr(text) pluginLocale->LookupString(TEXT2(text))

enum DeinterlacingType {
    DEINTERLACING_NONE,
    DEINTERLACING_DISCARD,
    DEINTERLACING_RETRO,
    DEINTERLACING_BLEND,
    DEINTERLACING_BLEND2x,
    DEINTERLACING_LINEAR,
    DEINTERLACING_LINEAR2x,
    DEINTERLACING_YADIF,
    DEINTERLACING_YADIF2x,
    DEINTERLACING__DEBUG,
    DEINTERLACING_TYPE_LAST
};

enum DeinterlacingFieldOrder {
    FIELD_ORDER_NONE,
    FIELD_ORDER_TFF = 1,
    FIELD_ORDER_BFF,
};

enum DeinterlacingProcessor {
    DEINTERLACING_PROCESSOR_CPU = 1,
    DEINTERLACING_PROCESSOR_GPU,
};

struct DeinterlacerConfig {
    int    type, fieldOrder, processor;
    bool   doublesFramerate;
};