/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>
 Copyright (C) 2012 Bill Hamilton <bill@ecologylab.net>

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

void OBS::ReportStartStreamTrigger()
{
    for (UINT i=0; i<plugins.Num(); i++)
    {
        OBS_CALLBACK callback = plugins[i].startStreamCallback;

        if (callback)
            (*callback)();
    }
}
void OBS::ReportStopStreamTrigger()
{
    for (UINT i=0; i<plugins.Num(); i++)
    {
        OBS_CALLBACK callback = plugins[i].stopStreamCallback;

        if (callback)
            (*callback)();
    }
}

void OBS::ReportStreamStatus(bool streaming, bool previewOnly, 
                               UINT bytesPerSec, double strain, 
                               UINT totalStreamtime, UINT numTotalFrames,
                               UINT numDroppedFrames, UINT fps)
{
    for (UINT i=0; i<plugins.Num(); i++)
    {
        OBS_STREAM_STATUS_CALLBACK callback = plugins[i].streamStatusCallback;

        if (callback)
            (*callback)(streaming, previewOnly, bytesPerSec, 
                        strain, totalStreamTime, numTotalFrames, 
                        numDroppedFrames, fps);
    }
}

void OBS::ReportSwitchScenes(CTSTR scene)
{
    for (UINT i=0; i<plugins.Num(); i++)
    {
        OBS_SCENE_SWITCH_CALLBACK callback = plugins[i].sceneSwitchCallback;

        if (callback)
            (*callback)(scene);
    }
}

void OBS::ReportScenesChanged()
{
    for (UINT i=0; i<plugins.Num(); i++)
    {
        OBS_CALLBACK callback = plugins[i].scenesChangedCallback;

        if (callback)
            (*callback)();
    }
}

void OBS::ReportSourceOrderChanged()
{
    for (UINT i=0; i<plugins.Num(); i++)
    {
        OBS_CALLBACK callback = plugins[i].sourceOrderChangedCallback;

        if (callback)
            (*callback)();
    }
}

void OBS::ReportSourceChanged(CTSTR sourceName, XElement* source)
{
    for (UINT i=0; i<plugins.Num(); i++)
    {
        OBS_SOURCE_CHANGED_CALLBACK callback = plugins[i].sourceChangedCallback;

        if (callback)
            (*callback)(sourceName, source);
    }
}

void OBS::ReportSourcesAddedOrRemoved()
{
    for (UINT i=0; i<plugins.Num(); i++)
    {
        OBS_CALLBACK callback = plugins[i].sourcesAddedOrRemovedCallback;

        if (callback)
            (*callback)();
    }
}

void OBS::ReportMicVolumeChange(float level, bool muted, bool finalValue)
{
    for (UINT i=0; i<plugins.Num(); i++)
    {
        OBS_VOLUME_CHANGED_CALLBACK callback = plugins[i].micVolumeChangeCallback;

        if (callback)
            (*callback)(level, muted, finalValue);
    }
}

void OBS::ReportDesktopVolumeChange(float level, bool muted, bool finalValue)
{
    for (UINT i=0; i<plugins.Num(); i++)
    {
        OBS_VOLUME_CHANGED_CALLBACK callback = plugins[i].desktopVolumeChangeCallback;

        if (callback)
            (*callback)(level, muted, finalValue);
    }
}