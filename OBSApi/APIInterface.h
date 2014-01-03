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


typedef LPVOID (STDCALL* OBSCREATEPROC)(XElement*); //data
typedef bool (STDCALL* OBSCONFIGPROC)(XElement*, bool); //element, bInitializing

typedef void (STDCALL* OBSHOTKEYPROC)(DWORD, UPARAM, bool);

#define HOTKEY_SHIFT    0x1
#define HOTKEY_CONTROL  0x2
#define HOTKEY_ALT      0x4

enum StreamInfoPriority
{
    StreamInfoPriority_Low,
    StreamInfoPriority_Medium,
    StreamInfoPriority_High,
    StreamInfoPriority_Critical,
};

//-------------------------------------------------------------------
// API interface, plugins should not ever use, use C funcs below

class APIInterface
{
    friend class OBS;
    friend class AudioSource;
    friend class SettingsPane;

public:
    virtual void Invalid03() const {}

    virtual ~APIInterface() {}

    virtual void EnterSceneMutex()=0;
    virtual void LeaveSceneMutex()=0;

    virtual void RegisterSceneClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc)=0;
    virtual void RegisterImageSourceClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc)=0;

    virtual ImageSource* CreateImageSource(CTSTR lpClassName, XElement *data)=0;

    virtual XElement* GetSceneListElement()=0;
    virtual XElement* GetGlobalSourceListElement()=0;
    
    virtual bool SetScene(CTSTR lpScene, bool bPost)=0;
    virtual Scene* GetScene() const=0;

    virtual CTSTR GetSceneName() const=0;
    virtual XElement* GetSceneElement()=0;

    //low-order word is VK, high-order word is modifier.  equivalent to the value given by hotkey controls
    virtual UINT CreateHotkey(DWORD hotkey, OBSHOTKEYPROC hotkeyProc, UPARAM param)=0;
    virtual void DeleteHotkey(UINT hotkeyID)=0;

    virtual Vect2 GetBaseSize() const=0;                //get the base scene size
    virtual Vect2 GetRenderFrameSize() const=0;         //get the render frame size
    virtual Vect2 GetOutputSize() const=0;              //get the stream output size

    virtual void GetBaseSize(UINT &width, UINT &height) const=0;
    virtual void GetRenderFrameSize(UINT &width, UINT &height) const=0;
    virtual void GetOutputSize(UINT &width, UINT &height) const=0;
    virtual UINT GetMaxFPS() const=0;

    virtual CTSTR GetLanguage() const=0;

    virtual HWND GetMainWindow() const=0;

    virtual CTSTR GetAppDataPath() const=0;
    virtual String GetPluginDataPath() const=0;

    virtual UINT AddStreamInfo(CTSTR lpInfo, StreamInfoPriority priority)=0;
    virtual void SetStreamInfo(UINT infoID, CTSTR lpInfo)=0;
    virtual void SetStreamInfoPriority(UINT infoID, StreamInfoPriority priority)=0;
    virtual void RemoveStreamInfo(UINT infoID)=0;

    virtual bool UseMultithreadedOptimizations() const=0;

    inline ImageSource* GetSceneImageSource(CTSTR lpImageSource)
    {
        Scene *scene = GetScene();
        if(scene)
        {
            SceneItem *item = scene->GetSceneItem(lpImageSource);
            if(item)
            {
                if(item->GetSource())
                    return item->GetSource();
            }
        }

        return NULL;
    }

    virtual void AddAudioSource(AudioSource *source)=0;
    virtual void RemoveAudioSource(AudioSource *source)=0;

    virtual QWORD GetAudioTime() const=0;

    virtual CTSTR GetAppPath() const=0;

    virtual void StartStopStream() = 0;
    virtual void StartStopPreview() = 0;
    virtual bool GetStreaming() = 0;
    virtual bool GetPreviewOnly() = 0;

    virtual void SetSourceOrder(StringList &sourceNames)=0;
    virtual void SetSourceRender(CTSTR lpSource, bool render) = 0;

    virtual void Invalid01() {}
    virtual void Invalid02() {}

    virtual void SetDesktopVolume(float val, bool finalValue) = 0;
    virtual float GetDesktopVolume() = 0;
    virtual void ToggleDesktopMute() = 0;
    virtual bool GetDesktopMuted() = 0;

    virtual void SetMicVolume(float val, bool finalValue) = 0;
    virtual float GetMicVolume() = 0;
    virtual void ToggleMicMute() = 0;
    virtual bool GetMicMuted() = 0;

    virtual DWORD GetOBSVersion() const=0;
    virtual bool IsTestVersion() const=0;

    //something about audio sources being modifiable by plugins, all good stuff
    virtual UINT NumAuxAudioSources() const=0;
    virtual AudioSource* GetAuxAudioSource(UINT id)=0;

    virtual AudioSource* GetDesktopAudioSource()=0;
    virtual AudioSource* GetMicAudioSource()=0;

    virtual void GetCurDesktopVolumeStats(float *rms, float *max, float *peak) const=0;
    virtual void GetCurMicVolumeStats(float *rms, float *max, float *peak) const=0;

    virtual void AddSettingsPane(SettingsPane *pane)=0;
    virtual void RemoveSettingsPane(SettingsPane *pane)=0;

    virtual void SetChangedSettings(bool isModified)=0;

    virtual Vect2 GetRenderFrameOffset() const=0;       //get the render frame offset inside the control
    virtual Vect2 GetRenderFrameControlSize() const=0;  //get the render frame control size

    virtual void GetRenderFrameOffset(UINT &x, UINT &y) const=0;
    virtual void GetRenderFrameControlSize(UINT &width, UINT &height) const=0;

    virtual bool GetRenderFrameIn1To1Mode() const=0;

    // Helpers for GetRenderFrame*() methods
    virtual Vect2 MapWindowToFramePos(Vect2 mousePos) const=0;
    virtual Vect2 MapFrameToWindowPos(Vect2 framePos) const=0;
    virtual Vect2 MapWindowToFrameSize(Vect2 windowSize) const=0;
    virtual Vect2 MapFrameToWindowSize(Vect2 frameSize) const=0;
    virtual Vect2 GetWindowToFrameScale() const=0;
    virtual Vect2 GetFrameToWindowScale() const=0;

    virtual UINT GetSampleRateHz() const=0;

    virtual void SetAbortApplySettings(bool abort) = 0;
};

BASE_EXPORT extern APIInterface *API;


//-------------------------------------------------------------------
//C-style API exports, use these

BASE_EXPORT void OBSEnterSceneMutex();
BASE_EXPORT void OBSLeaveSceneMutex();

BASE_EXPORT void OBSRegisterSceneClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc);
BASE_EXPORT void OBSRegisterImageSourceClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc);

BASE_EXPORT ImageSource* OBSCreateImageSource(CTSTR lpClassName, XElement *data);

BASE_EXPORT XElement* OBSGetSceneListElement();
BASE_EXPORT XElement* OBSGetGlobalSourceListElement();

BASE_EXPORT bool OBSSetScene(CTSTR lpScene, bool bPost);
BASE_EXPORT Scene* OBSGetScene();

BASE_EXPORT CTSTR OBSGetSceneName();
BASE_EXPORT XElement* OBSGetSceneElement();

//low-order word is VK, high-order word is modifier.  equivalent to the value given by hotkey controls
BASE_EXPORT UINT OBSCreateHotkey(DWORD hotkey, OBSHOTKEYPROC hotkeyProc, UPARAM param);
BASE_EXPORT void OBSDeleteHotkey(UINT hotkeyID);

BASE_EXPORT Vect2 OBSGetBaseSize();          //get the base scene size
BASE_EXPORT Vect2 OBSGetRenderFrameSize();   //get the render frame size
BASE_EXPORT Vect2 OBSGetOutputSize();        //get the stream output size

BASE_EXPORT void OBSGetBaseSize(UINT &width, UINT &height);
BASE_EXPORT void OBSGetRenderFrameSize(UINT &width, UINT &height);
BASE_EXPORT void OBSGetRenderFrameOffset(UINT &x, UINT &y);
BASE_EXPORT void OBSGetRenderFrameControlSize(UINT &width, UINT &height);
BASE_EXPORT void OBSGetOutputSize(UINT &width, UINT &height);
BASE_EXPORT UINT OBSGetMaxFPS();
BASE_EXPORT bool OBSGetIn1To1Mode();

BASE_EXPORT CTSTR OBSGetLanguage();

BASE_EXPORT HWND OBSGetMainWindow();

BASE_EXPORT CTSTR OBSGetAppDataPath();
BASE_EXPORT String OBSGetPluginDataPath();

BASE_EXPORT UINT OBSAddStreamInfo(CTSTR lpInfo, StreamInfoPriority priority);
BASE_EXPORT void OBSSetStreamInfo(UINT infoID, CTSTR lpInfo);
BASE_EXPORT void OBSSetStreamInfoPriority(UINT infoID, StreamInfoPriority priority);
BASE_EXPORT void OBSRemoveStreamInfo(UINT infoID);

BASE_EXPORT bool OBSUseMultithreadedOptimizations();

BASE_EXPORT void OBSAddAudioSource(AudioSource *source);
BASE_EXPORT void OBSRemoveAudioSource(AudioSource *source);

BASE_EXPORT QWORD OBSGetAudioTime();

BASE_EXPORT CTSTR OBSGetAppPath();

BASE_EXPORT void OBSStartStopStream();
BASE_EXPORT void OBSStartStopPreview();
BASE_EXPORT bool OBSGetStreaming();
BASE_EXPORT bool OBSGetPreviewOnly();

BASE_EXPORT void OBSSetSourceOrder(StringList &sourceNames);
BASE_EXPORT void OBSSetSourceRender(CTSTR lpSource, bool render);

inline ImageSource* OBSGetSceneImageSource(CTSTR lpImageSource)
{
    Scene *scene = OBSGetScene();
    if(scene)
    {
        SceneItem *item = scene->GetSceneItem(lpImageSource);
        if(item)
        {
            if(item->GetSource())
                return item->GetSource();
        }
    }

    return NULL;
}

/* Volume Calls */
BASE_EXPORT void OBSSetMicVolume(float val, bool finalValue);
BASE_EXPORT float OBSGetMicVolume();
BASE_EXPORT void OBSToggleMicMute();
BASE_EXPORT bool OBSGetMicMuted();

BASE_EXPORT void OBSSetDesktopVolume(float val, bool finalValue);
BASE_EXPORT float OBSGetDesktopVolume();
BASE_EXPORT void OBSToggleDesktopMute();
BASE_EXPORT bool OBSGetDesktopMuted();

BASE_EXPORT DWORD OBSGetVersion();
BASE_EXPORT bool OBSIsTestVersion();

BASE_EXPORT UINT OBSNumAuxAudioSources();
BASE_EXPORT AudioSource* OBSGetAuxAudioSource(UINT id);

BASE_EXPORT AudioSource* OBSGetDesktopAudioSource();
BASE_EXPORT AudioSource* OBSGetMicAudioSource();

BASE_EXPORT void OBSGetCurDesktopVolumeStats(float *rms, float *max, float *peak);
BASE_EXPORT void OBSGetCurMicVolumeStats(float *rms, float *max, float *peak);

BASE_EXPORT void OBSAddSettingsPane(SettingsPane *pane);
BASE_EXPORT void OBSRemoveSettingsPane(SettingsPane *pane);

/** gets API version.  version is formatted: 0xMMmm */
BASE_EXPORT UINT OBSGetAPIVersion();

BASE_EXPORT UINT OBSGetSampleRateHz();
