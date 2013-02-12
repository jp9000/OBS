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

void OBS::AddOBSEventListener(OBSTriggerHandler *handler)
{
    triggerHandlers.Add(handler);
}

void OBS::RemoveOBSEventListener(OBSTriggerHandler *handler)
{
    triggerHandlers.RemoveItem(handler);
}

void OBS::ReportStartStreamTrigger(bool previewOnly)
{
    for(UINT i = 0; i < triggerHandlers.Num(); i++)
    {
        OBSTriggerHandler* handler = triggerHandlers[i];
        handler->StreamStarting(previewOnly);
    }
}
void OBS::ReportStopStreamTrigger(bool previewOnly)
{
    for(UINT i = 0; i < triggerHandlers.Num(); i++)
    {
        OBSTriggerHandler* handler = triggerHandlers[i];
        handler->StreamStopping(previewOnly);
    }
}

void OBS::ReportStreamStatus(bool streaming, bool previewOnly, 
                               UINT bytesPerSec, double strain, 
                               UINT totalStreamtime, UINT numTotalFrames,
                               UINT numDroppedFrames, UINT fps)
{
    for(UINT i = 0; i < triggerHandlers.Num(); i++)
    {
        OBSTriggerHandler* handler = triggerHandlers[i];
        handler->StreamStatus(streaming, previewOnly, bytesPerSec, strain, 
                                    totalStreamtime, numTotalFrames, numDroppedFrames, fps);
    }
}

void OBS::ReportSwitchScenes(CTSTR scene)
{
    for(UINT i = 0; i < triggerHandlers.Num(); i++)
    {
        OBSTriggerHandler* handler = triggerHandlers[i];
        handler->ScenesSwitching(scene);
    }
}

void OBS::ReportScenesChanged()
{
    for(UINT i = 0; i < triggerHandlers.Num(); i++)
    {
        OBSTriggerHandler* handler = triggerHandlers[i];
        handler->ScenesChanged();
    }
}

void OBS::ReportSourceOrderChanged()
{
    for(UINT i = 0; i < triggerHandlers.Num(); i++)
    {
        OBSTriggerHandler* handler = triggerHandlers[i];
        handler->SourceOrderChanged();
    }
}

void OBS::ReportSourceChanged(CTSTR sourceName, XElement* source)
{
    for(UINT i = 0; i < triggerHandlers.Num(); i++)
    {
        OBSTriggerHandler* handler = triggerHandlers[i];
        handler->SourceChanged(sourceName, source);
    }
}

void OBS::ReportSourcesAddedOrRemoved()
{
    for(UINT i = 0; i < triggerHandlers.Num(); i++)
    {
        OBSTriggerHandler* handler = triggerHandlers[i];
        handler->SourcesAddedOrRemoved();
    }
}

void OBS::ReportMicVolumeChange(float level, bool muted, bool finalValue)
{
    for(UINT i = 0; i < triggerHandlers.Num(); i++)
    {
        OBSTriggerHandler* handler = triggerHandlers[i];
        handler->MicVolumeChanged(level, muted, finalValue);
    }
}

void OBS::ReportDesktopVolumeChange(float level, bool muted, bool finalValue)
{
    for(UINT i = 0; i < triggerHandlers.Num(); i++)
    {
        OBSTriggerHandler* handler = triggerHandlers[i];
        handler->DesktopVolumeChanged(level, muted, finalValue);
    }
}