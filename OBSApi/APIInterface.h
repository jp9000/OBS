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

class APIInterface
{
    friend class OBS;

protected:
    bool bSSE2Availabe;

public:
    virtual ~APIInterface() {}

    virtual void EnterSceneMutex()=0;
    virtual void LeaveSceneMutex()=0;

    virtual void RegisterSceneClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc)=0;
    virtual void RegisterImageSourceClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc)=0;

    virtual ImageSource* CreateImageSource(CTSTR lpClassName, XElement *data)=0;

    virtual XElement* GetSceneListElement()=0;
    virtual XElement* GetGlobalSourceListElement()=0;

    virtual bool SetScene(CTSTR lpScene)=0;
    virtual Scene* GetScene() const=0;

    virtual CTSTR GetSceneName() const=0;
    virtual XElement* GetSceneElement()=0;

    //low-order word is VK, high-order word is modifier.  equivalent to the value given by hotkey controls
    virtual UINT CreateHotkey(DWORD hotkey, OBSHOTKEYPROC hotkeyProc, UPARAM param)=0;
    virtual void DeleteHotkey(UINT hotkeyID)=0;

    virtual Vect2 GetBaseSize() const=0;          //get the base scene size
    virtual Vect2 GetRenderFrameSize() const=0;   //get the render frame size
    virtual Vect2 GetOutputSize() const=0;        //get the stream output size

    virtual void GetBaseSize(UINT &width, UINT &height) const=0;
    virtual void GetRenderFrameSize(UINT &width, UINT &height) const=0;
    virtual void GetOutputSize(UINT &width, UINT &height) const=0;

    virtual CTSTR GetLanguage() const=0;

    virtual HWND GetMainWindow() const=0;

    virtual CTSTR GetAppDataPath() const=0;
    virtual String GetPluginDataPath() const=0;

    inline bool SSE2Available() {return bSSE2Availabe;}
};


BASE_EXPORT extern APIInterface *API;
