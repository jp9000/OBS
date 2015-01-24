/********************************************************************************
 Copyright (C) 2013 Hugh Bailey <obs.jim@gmail.com>

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


#include "OBSApi.h"


//C-style exports because you don't ever want to use virtual functions for an API
//...which I did until plugins started breaking left and right due to virtual function table changes
//......I'm sorry!  I didn't think things through at the time!


void OBSEnterSceneMutex() {API->EnterSceneMutex();}
void OBSLeaveSceneMutex() {API->LeaveSceneMutex();}

void OBSRegisterSceneClass(CTSTR lpClassName, CTSTR lpDisplayName,
                           OBSCREATEPROC createProc, OBSCONFIGPROC configProc)
{
    API->RegisterSceneClass(lpClassName, lpDisplayName, createProc, configProc);
}

void OBSRegisterImageSourceClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc)
{
    API->RegisterImageSourceClass(lpClassName, lpDisplayName, createProc, configProc);
}

ImageSource* OBSCreateImageSource(CTSTR lpClassName, XElement *data)
{
    return API->CreateImageSource(lpClassName, data);
}

XElement* OBSGetSceneListElement()
{
    return API->GetSceneListElement();
}

XElement* OBSGetGlobalSourceListElement()
{
    return API->GetGlobalSourceListElement();
}

bool OBSSetScene(CTSTR lpScene, bool bPost)
{
    return API->SetScene(lpScene, bPost);
}

Scene* OBSGetScene()            {return API->GetScene();}

CTSTR OBSGetSceneName()         {return API->GetSceneName();}
XElement* OBSGetSceneElement()  {return API->GetSceneElement();}

void OBSDisableTransitions()    { API->DisableTransitions(); }
void OBSEnableTransitions()     { API->EnableTransitions(); }
bool OBSTransitionsEnabled()    { return API->TransitionsEnabled(); }

bool OBSSetSceneCollection(CTSTR lpCollection, CTSTR lpScene)
{
    return API->SetSceneCollection(lpCollection, lpScene);
}

CTSTR OBSGetSceneCollectionName()                   { return API->GetSceneCollectionName(); }
void OBSGetSceneCollectionNames(StringList &list)   { API->GetSceneCollectionNames(list); }

//low-order word is VK, high-order word is modifier.  equivalent to the value given by hotkey controls
UINT OBSCreateHotkey(DWORD hotkey, OBSHOTKEYPROC hotkeyProc, UPARAM param)
{
    return API->CreateHotkey(hotkey, hotkeyProc, param);
}

void OBSDeleteHotkey(UINT hotkeyID)
{
    API->DeleteHotkey(hotkeyID);
}

Vect2 OBSGetBaseSize()          {return API->GetBaseSize();}
Vect2 OBSGetRenderFrameSize()   {return API->GetRenderFrameSize();}
Vect2 OBSGetOutputSize()        {return API->GetOutputSize();}

void OBSGetBaseSize(UINT &width, UINT &height)                  {API->GetBaseSize(width, height);}
void OBSGetRenderFrameSize(UINT &width, UINT &height)           {API->GetRenderFrameSize(width, height);}
void OBSGetRenderFrameOffset(UINT &x, UINT &y)                  {API->GetRenderFrameOffset(x, y);}
void OBSGetRenderFrameControlSize(UINT &width, UINT &height)    {API->GetRenderFrameControlSize(width, height);}
void OBSGetOutputSize(UINT &width, UINT &height)                {API->GetOutputSize(width, height);}
UINT OBSGetMaxFPS()                                             {return API->GetMaxFPS();}
bool OBSGetRenderFrameIn1To1Mode()                              {return API->GetRenderFrameIn1To1Mode();}
UINT OBSGetCaptureFPS()                                         {return API->GetCaptureFPS();}
UINT OBSGetTotalFrames()                                        {return API->GetTotalFrames();}
UINT OBSGetFramesDropped()                                      {return API->GetFramesDropped();}

CTSTR OBSGetLanguage()          {return API->GetLanguage();}

HWND OBSGetMainWindow()         {return API->GetMainWindow();}

CTSTR OBSGetAppDataPath()       {return API->GetAppDataPath();}
String OBSGetPluginDataPath()   {return API->GetPluginDataPath();}

UINT OBSAddStreamInfo(CTSTR lpInfo, StreamInfoPriority priority)            {return API->AddStreamInfo(lpInfo, priority);}
void OBSSetStreamInfo(UINT infoID, CTSTR lpInfo)                            {API->SetStreamInfo(infoID, lpInfo);}
void OBSSetStreamInfoPriority(UINT infoID, StreamInfoPriority priority)     {API->SetStreamInfoPriority(infoID, priority);}
void OBSRemoveStreamInfo(UINT infoID)                                       {API->RemoveStreamInfo(infoID);}

UINT OBSGetTotalStreamTime()    {return API->GetTotalStreamTime();}
UINT OBSGetBytesPerSec()        {return API->GetBytesPerSec();}

bool OBSUseMultithreadedOptimizations()         {return API->UseMultithreadedOptimizations();}

void OBSAddAudioSource(AudioSource *source)     {API->AddAudioSource(source);}
void OBSRemoveAudioSource(AudioSource *source)  {API->RemoveAudioSource(source);}

QWORD OBSGetAudioTime()         {return API->GetAudioTime();}

CTSTR OBSGetAppPath()           {return API->GetAppPath();}

void OBSStartStopStream()       {API->StartStopStream();}
void OBSStartStopPreview()      {API->StartStopPreview();}
void OBSStartStopRecording()    {API->StartStopRecording();}
bool OBSGetStreaming()          {return API->GetStreaming();}
bool OBSGetPreviewOnly()        {return API->GetPreviewOnly();}
bool OBSGetRecording()          {return API->GetRecording();}

bool OBSGetKeepRecording()      {return API->GetKeepRecording();}

void OBSStartStopRecordingReplayBuffer() {API->StartStopRecordingReplayBuffer();}
bool OBSGetRecordingReplayBuffer()       {return API->GetRecordingReplayBuffer();}
void OBSSaveReplayBuffer()               {API->SaveReplayBuffer();}

void OBSSetSourceOrder(StringList &sourceNames)             {API->SetSourceOrder(sourceNames);}
void OBSSetSourceRender(CTSTR lpSource, bool render)        {API->SetSourceRender(lpSource, render);}

void OBSSetDesktopVolume(float val, bool finalValue)        {API->SetDesktopVolume(val, finalValue);}
float OBSGetDesktopVolume()                                 {return API->GetDesktopVolume();}
void OBSToggleDesktopMute()                                 {API->ToggleDesktopMute();}
bool OBSGetDesktopMuted()                                   {return API->GetDesktopMuted();}

void OBSSetMicVolume(float val, bool finalValue)            {API->SetMicVolume(val, finalValue);}
float OBSGetMicVolume()                                     {return API->GetMicVolume();}
void OBSToggleMicMute()                                     {API->ToggleMicMute();}
bool OBSGetMicMuted()                                       {return API->GetMicMuted();}

DWORD OBSGetVersion()                       {return API->GetOBSVersion();}
bool OBSIsTestVersion()                     {return API->IsTestVersion();}

UINT OBSNumAuxAudioSources()                {return API->NumAuxAudioSources();}
AudioSource* OBSGetAuxAudioSource(UINT id)  {return API->GetAuxAudioSource(id);}

AudioSource* OBSGetDesktopAudioSource()     {return API->GetDesktopAudioSource();}
AudioSource* OBSGetMicAudioSource()         {return API->GetMicAudioSource();}

void OBSGetCurDesktopVolumeStats(float *rms, float *max, float *peak)   {API->GetCurDesktopVolumeStats(rms, max, peak);}
void OBSGetCurMicVolumeStats(float *rms, float *max, float *peak)       {API->GetCurMicVolumeStats(rms, max, peak);}

void OBSAddSettingsPane(SettingsPane *pane)     {API->AddSettingsPane(pane);}
void OBSRemoveSettingsPane(SettingsPane *pane)  {API->RemoveSettingsPane(pane);}

UINT OBSGetAPIVersion()                         {return 0x0103;}

UINT OBSGetSampleRateHz()                       {return API->GetSampleRateHz();}
