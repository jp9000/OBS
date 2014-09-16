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
    if (bShuttingDown)
        return;

    for (UINT i=0; i<plugins.Num(); i++)
    {
        OBS_CALLBACK callback = plugins[i].startStreamCallback;

        if (callback)
            (*callback)();
    }
}
void OBS::ReportStopStreamTrigger()
{
    if (bShuttingDown)
        return;

    for (UINT i=0; i<plugins.Num(); i++)
    {
        OBS_CALLBACK callback = plugins[i].stopStreamCallback;

        if (callback)
            (*callback)();
    }
}

void OBS::ReportStartStreamingTrigger()
{
    if (bShuttingDown || bTestStream)
        return;

    for (UINT i = 0; i<plugins.Num(); i++)
    {
        OBS_CALLBACK callback = plugins[i].startStreamingCallback;

        if (callback)
            (*callback)();
    }
}
void OBS::ReportStopStreamingTrigger()
{
    if (bShuttingDown || bTestStream)
        return;

    for (UINT i = 0; i<plugins.Num(); i++)
    {
        OBS_CALLBACK callback = plugins[i].stopStreamingCallback;

        if (callback)
            (*callback)();
    }
}

void OBS::ReportStartRecordingTrigger()
{
    if (bShuttingDown)
        return;

    for (UINT i = 0; i<plugins.Num(); i++)
    {
        OBS_CALLBACK callback = plugins[i].startRecordingCallback;

        if (callback)
            (*callback)();
    }
}
void OBS::ReportStopRecordingTrigger()
{
    if (bShuttingDown)
        return;

    for (UINT i = 0; i<plugins.Num(); i++)
    {
        OBS_CALLBACK callback = plugins[i].stopRecordingCallback;

        if (callback)
            (*callback)();
    }
}

void OBS::ReportStartRecordingReplayBufferTrigger()
{
    if (bShuttingDown)
        return;

    for (UINT i = 0; i<plugins.Num(); i++)
    {
        OBS_CALLBACK callback = plugins[i].startRecordingReplayBufferCallback;

        if (callback)
            (*callback)();
    }
}
void OBS::ReportStopRecordingReplayBufferTrigger()
{
    if (bShuttingDown)
        return;

    for (UINT i = 0; i<plugins.Num(); i++)
    {
        OBS_CALLBACK callback = plugins[i].stopRecordingReplayBufferCallback;

        if (callback)
            (*callback)();
    }
}
void OBS::ReportReplayBufferSavedTrigger(String filename, UINT recordingLengthMS)
{
    if (bShuttingDown)
        return;

    for (UINT i = 0; i<plugins.Num(); i++)
    {
        OBS_REPLAY_BUFFER_SAVED_CALLBACK callback = plugins[i].replayBufferSavedCallback;

        if (callback)
            (*callback)(filename, recordingLengthMS);
    }
}

void OBS::ReportOBSStatus(bool running, bool streaming, bool recording, bool previewing, bool reconnecting)
{
    if (bShuttingDown)
        return;

    for (UINT i = 0; i < plugins.Num(); i++)
        if (plugins[i].statusCallback)
            plugins[i].statusCallback(running, streaming, recording, previewing, reconnecting);
}

void OBS::ReportStreamStatus(bool streaming, bool previewOnly, 
                               UINT bytesPerSec, double strain, 
                               UINT totalStreamtime, UINT numTotalFrames,
                               UINT numDroppedFrames, UINT fps)
{
    if (bShuttingDown)
            return;

    for (UINT i=0; i<plugins.Num(); i++)
    {
        OBS_STREAM_STATUS_CALLBACK callback = plugins[i].streamStatusCallback;

        if (callback)
            (*callback)(streaming, previewOnly, bytesPerSec, 
                        strain, totalStreamtime, numTotalFrames,
                        numDroppedFrames, fps);
    }
}

void OBS::ReportSwitchScenes(CTSTR scene)
{
    if (bShuttingDown)
            return;

    for (UINT i=0; i<plugins.Num(); i++)
    {
        OBS_SCENE_SWITCH_CALLBACK callback = plugins[i].sceneSwitchCallback;

        if (callback)
            (*callback)(scene);
    }
}

void OBS::ReportSwitchSceneCollections(CTSTR collection)
{
    if (bShuttingDown)
        return;

    for (UINT i = 0; i<plugins.Num(); i++)
    {
        OBS_SCENE_SWITCH_CALLBACK callback = plugins[i].sceneCollectionSwitchCallback;

        if (callback)
            (*callback)(collection);
    }
}

void OBS::ReportScenesChanged()
{
    if (bShuttingDown)
            return;

    for (UINT i=0; i<plugins.Num(); i++)
    {
        OBS_CALLBACK callback = plugins[i].scenesChangedCallback;

        if (callback)
            (*callback)();
    }
}

void OBS::ReportSceneCollectionsChanged()
{
    if (bShuttingDown)
        return;

    for (UINT i = 0; i<plugins.Num(); i++)
    {
        OBS_CALLBACK callback = plugins[i].sceneCollectionsChangedCallback;

        if (callback)
            (*callback)();
    }
}

void OBS::ReportSourceOrderChanged()
{
    if (bShuttingDown)
            return;

    for (UINT i=0; i<plugins.Num(); i++)
    {
        OBS_CALLBACK callback = plugins[i].sourceOrderChangedCallback;

        if (callback)
            (*callback)();
    }
}

void OBS::ReportSourceChanged(CTSTR sourceName, XElement* source)
{
    if (bShuttingDown)
            return;

    for (UINT i=0; i<plugins.Num(); i++)
    {
        OBS_SOURCE_CHANGED_CALLBACK callback = plugins[i].sourceChangedCallback;

        if (callback)
            (*callback)(sourceName, source);
    }
}

void OBS::ReportSourcesAddedOrRemoved()
{
    if (bShuttingDown)
            return;

    for (UINT i=0; i<plugins.Num(); i++)
    {
        OBS_CALLBACK callback = plugins[i].sourcesAddedOrRemovedCallback;

        if (callback)
            (*callback)();
    }
}

void OBS::ReportMicVolumeChange(float level, bool muted, bool finalValue)
{
    if (bShuttingDown)
            return;

    for (UINT i=0; i<plugins.Num(); i++)
    {
        OBS_VOLUME_CHANGED_CALLBACK callback = plugins[i].micVolumeChangeCallback;

        if (callback)
            (*callback)(level, muted, finalValue);
    }
}

void OBS::ReportDesktopVolumeChange(float level, bool muted, bool finalValue)
{
    if (bShuttingDown)
            return;

    for (UINT i=0; i<plugins.Num(); i++)
    {
        OBS_VOLUME_CHANGED_CALLBACK callback = plugins[i].desktopVolumeChangeCallback;

        if (callback)
            (*callback)(level, muted, finalValue);
    }
}

void OBS::ReportLogUpdate(CTSTR logDelta, UINT length)
{
    if (bShuttingDown)
        return;

    for (UINT i = 0; i < plugins.Num(); i++)
    {
        if (plugins[i].logUpdateCallback)
            plugins[i].logUpdateCallback(logDelta, length);
    }
}