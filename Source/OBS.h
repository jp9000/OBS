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

class Scene;

#define NUM_RENDER_BUFFERS 2

static const int minClientWidth  = 700;
static const int minClientHeight = 200;


struct AudioDeviceInfo
{
    String strID;
    String strName;

    inline void FreeData() {strID.Clear(); strName.Clear();}
};

struct AudioDeviceList
{
    List<AudioDeviceInfo> devices;

    inline ~AudioDeviceList()
    {
        FreeData();
    }

    inline void FreeData()
    {
        for(UINT i=0; i<devices.Num(); i++)
            devices[i].FreeData();
        devices.Clear();
    }

    inline bool HasID(CTSTR lpID) const
    {
        for(UINT i=0; i<devices.Num(); i++)
        {
            if(devices[i].strID == lpID)
                return true;
        }

        return false;
    }
};

void GetAudioDevices(AudioDeviceList &deviceList);
bool GetDefaultMicID(String &strVal);

//-------------------------------------------------------------------

struct DataPacket
{
    LPBYTE lpPacket;
    UINT size;
};

//-------------------------------------------------------------------

enum PacketType
{
    PacketType_VideoDisposable,
    PacketType_VideoLow,
    PacketType_VideoHigh,
    PacketType_VideoHighest,
    PacketType_Audio
};

class NetworkStream
{
public:
    virtual ~NetworkStream() {}
    virtual void SendPacket(BYTE *data, UINT size, DWORD timestamp, PacketType type)=0;
    virtual void BeginPublishing() {}

    virtual double GetPacketStrain() const=0;
    virtual QWORD GetCurrentSentBytes()=0;
    virtual DWORD NumDroppedFrames() const=0;
};

//-------------------------------------------------------------------

class VideoFileStream
{
public:
    virtual ~VideoFileStream() {}
    virtual void AddPacket(BYTE *data, UINT size, DWORD timestamp, PacketType type)=0;
};

//-------------------------------------------------------------------

enum
{
    NoAudioAvailable,
    AudioAvailable,
    ContinueAudioRequest
};

class AudioSource
{
public:
    virtual ~AudioSource() {}

    virtual void StartCapture()=0;
    virtual void StopCapture()=0;

    virtual UINT GetNextBuffer()=0;
    virtual bool GetBuffer(float **buffer, UINT *numFrames, DWORD &timestamp)=0;
};

//-------------------------------------------------------------------

class AudioEncoder
{
    friend class OBS;

protected:
    virtual bool    Encode(float *input, UINT numInputFrames, DataPacket &packet, DWORD &timestamp)=0;
    virtual void    GetHeaders(DataPacket &packet)=0;

public:
    virtual ~AudioEncoder() {}

    virtual UINT    GetFrameSize() const=0;

    virtual int     GetBitRate() const=0;
    virtual CTSTR   GetCodec() const=0;

    virtual String  GetInfoString() const=0;
};

//-------------------------------------------------------------------

class VideoEncoder
{
    friend class OBS;

protected:
    virtual bool Encode(LPVOID picIn, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD timestamp)=0;

public:
    virtual ~VideoEncoder() {}

    virtual int  GetBitRate() const=0;

    virtual void GetHeaders(DataPacket &packet)=0;
    virtual void GetSEI(DataPacket &packet)=0;

    virtual String GetInfoString() const=0;
};

//-------------------------------------------------------------------

struct MonitorInfo
{
    inline MonitorInfo(HMONITOR hMonitor, RECT *lpRect)
    {
        this->hMonitor = hMonitor;
        mcpy(&this->rect, lpRect, sizeof(rect));
    }

    HMONITOR hMonitor;
    RECT rect;
};

//-------------------------------------------------------------------

struct IconInfo
{
    HINSTANCE hInst;
    HICON hIcon;
    int resource;
};

struct FontInfo
{
    HFONT hFont;
    String strFontFace;
    int fontSize;
    int fontWeight;
};

//-------------------------------------------------------------------

struct FrameAudio
{
    List<BYTE> audioData;
    DWORD timestamp;
};


//===============================================================================================

struct ClassInfo
{
    String strClass;
    String strName;
    OBSCREATEPROC createProc;
    OBSCONFIGPROC configProc;

    inline void FreeData() {strClass.Clear(); strName.Clear();}
};

//----------------------------

struct PluginInfo
{
    String strFile;
    HMODULE hModule;
};

//----------------------------

struct GlobalSourceInfo
{
    String strName;
    XElement *element;
    ImageSource *source;

    inline void FreeData() {strName.Clear(); delete source; source = NULL;}
};

//----------------------------

enum
{
    ID_SETTINGS=5000,
    ID_STARTSTOP,
    ID_EXIT,
    ID_SCENEEDITOR,
    ID_DESKTOPVOLUME,
    ID_MICVOLUME,
    ID_STATUS,
    ID_SCENES,
    ID_SCENES_TEXT,
    ID_SOURCES,
    ID_SOURCES_TEXT,
    ID_TESTSTREAM,
    ID_GLOBALSOURCES,
    ID_PLUGINS,
    ID_DASHBOARD,
};

enum
{
    OBS_REQUESTSTOP=WM_USER+1,
    OBS_CALLHOTKEY,
    OBS_RECONNECT,
    OBS_SETSCENE,
    OBS_UPDATESTATUSBAR,
};

//----------------------------

enum ItemModifyType
{
    ItemModifyType_None,
    ItemModifyType_Move,
    ItemModifyType_ScaleBottomLeft,
    ItemModifyType_ScaleLeft,
    ItemModifyType_ScaleTopLeft,
    ItemModifyType_ScaleTop,
    ItemModifyType_ScaleTopRight,
    ItemModifyType_ScaleRight,
    ItemModifyType_ScaleBottomRight,
    ItemModifyType_ScaleBottom,
};

//----------------------------

struct SceneHotkeyInfo
{
    DWORD hotkeyID;
    DWORD hotkey;
    XElement *scene;
};

//----------------------------

struct StreamInfo
{
    UINT id;
    String strInfo;
    StreamInfoPriority priority;

    inline void FreeData() {strInfo.Clear();}
};

//----------------------------

struct StatusBarDrawData
{
    UINT bytesPerSec;
    double strain;
};

//----------------------------

class OBS
{
    friend class Scene;
    friend class SceneItem;
    friend class RTMPPublisher;
    friend class RTMPServer;
    friend class Connection;
    friend class D3D10System;
    friend class OBSAPIInterface;
    friend class GlobalSource;
    friend class TextOutputSource;

    //---------------------------------------------------
    // graphics stuff

    ID3D10Texture2D *copyTextures[2];

    Texture *mainRenderTextures[NUM_RENDER_BUFFERS];
    Texture *yuvRenderTextures[NUM_RENDER_BUFFERS];

    Texture *transitionTexture;
    bool    bTransitioning;
    float   transitionAlpha;

    Shader  *mainVertexShader, *mainPixelShader, *yuvScalePixelShader;
    Shader  *solidVertexShader, *solidPixelShader;

    //---------------------------------------------------
    // network
    NetworkStream *network;

    //---------------------------------------------------
    // audio
    AudioSource  *desktopAudio;
    AudioSource  *micAudio;
    AudioEncoder *audioEncoder;

    //---------------------------------------------------
    // video
    Scene                   *scene;
    VideoEncoder            *videoEncoder;
    HDC                     hCaptureDC;
    List<MonitorInfo>       monitors;

    XConfig                 scenesConfig;
    List<SceneHotkeyInfo>   sceneHotkeys;
    XElement                *sceneElement;

    inline void RemoveSceneHotkey(DWORD hotkey)
    {
        for(UINT i=0; i<sceneHotkeys.Num(); i++)
        {
            if(sceneHotkeys[i].hotkey == hotkey)
            {
                API->DeleteHotkey(sceneHotkeys[i].hotkeyID);
                sceneHotkeys.Remove(i);
                break;
            }
        }
    }

    void SelectSources();

    //---------------------------------------------------
    // settings window
    int     curSettingsSelection;
    HWND    hwndSettings;
    HWND    hwndCurrentSettings;
    bool    bSettingsChanged;

    inline void SetChangedSettings(bool bChanged)
    {
        EnableWindow(GetDlgItem(hwndSettings, IDC_APPLY), (bSettingsChanged = bChanged));
    }

    void   ApplySettings();

    void   RefreshDownscales(HWND hwnd, int cx, int cy);

    static INT_PTR CALLBACK GeneralSettingsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK EncoderSettingsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK PublishSettingsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK VideoSettingsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK AudioSettingsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK AdvancedSettingsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    //---------------------------------------------------

    String  strLanguage;
    bool    bTestStream;
    bool    bUseMultithreadedOptimizations;
    bool    bRunning;
    int     renderFrameWidth, renderFrameHeight;
    int     borderXSize, borderYSize;
    int     clientWidth, clientHeight;
    bool    bSizeChanging;
    bool    bResizeRenderView;

    bool    bAutoReconnect;
    bool    bRetrying;
    bool    bReconnecting;
    UINT    reconnectTimeout;

    bool    bEditMode;
    bool    bRenderViewEnabled;
    bool    bShowFPS;
    bool    bMouseMoved;
    bool    bMouseDown;
    bool    bItemWasSelected;
    Vect2   startMousePos, lastMousePos;
    ItemModifyType modifyType;
    SceneItem *scaleItem;

    int     cpuInfo[4];
    bool    bSSE2Available;

    int     lastRenderTarget;
    UINT    baseCX,   baseCY;
    UINT    scaleCX,  scaleCY;
    UINT    outputCX, outputCY;
    float   downscale;
    UINT    frameTime, fps;
    HANDLE  hMainThread;
    HANDLE  hSceneMutex;
    bool    bUsing444;

    DWORD bytesPerSec;
    DWORD captureFPS;
    DWORD curFramesDropped;
    double curStrain;

    DWORD totalStreamTime;

    bool        bUseSyncFix;
    List<UINT>  bufferedTimes;

    bool    bRecievedFirstAudioFrame;

    HANDLE  hHotkeyMutex;

    HANDLE  hSoundThread, hSoundDataMutex, hRequestAudioEvent;
    float   desktopVol, micVol;
    List<FrameAudio> pendingAudioFrames;
    bool    bForceMicMono;
    float   micBoost;

    HANDLE hHotkeyThread;
    bool bUsingPushToTalk, bPushToTalkOn;
    UINT pushToTalkHotkeyID;
    UINT muteMicHotkeyID;
    UINT muteDesktopHotkeyID;
    UINT startStreamHotkeyID;
    UINT stopStreamHotkeyID;

    bool bStartStreamHotkeyDown, bStopStreamHotkeyDown;

    bool bWriteToFile;
    VideoFileStream *fileStream;

    String  streamReport;

    String  strDashboard;

    List<IconInfo> Icons;
    List<FontInfo> Fonts;

    List<ClassInfo> sceneClasses;
    List<ClassInfo> imageSourceClasses;

    List<GlobalSourceInfo> globalSources;

    List<PluginInfo> plugins;

    HANDLE hInfoMutex;
    List<StreamInfo> streamInfoList;
    UINT streamInfoIDCounter;

    bool bShuttingDown;

    inline void ClearStreamInfo()
    {
        for(UINT i=0; i<streamInfoList.Num(); i++)
            streamInfoList[i].FreeData();
        streamInfoList.Clear();
    }

    ImageSource* AddGlobalSourceToScene(CTSTR lpName);

    inline ImageSource* GetGlobalSource(CTSTR lpName)
    {
        for(UINT i=0; i<globalSources.Num(); i++)
        {
            if(globalSources[i].strName.CompareI(lpName))
                return globalSources[i].source;
        }

        return AddGlobalSourceToScene(lpName);
    }

    inline ClassInfo* GetSceneClass(CTSTR lpClass) const
    {
        for(UINT i=0; i<sceneClasses.Num(); i++)
        {
            if(sceneClasses[i].strClass.CompareI(lpClass))
                return sceneClasses+i;
        }

        return NULL;
    }

    inline ClassInfo* GetImageSourceClass(CTSTR lpClass) const
    {
        for(UINT i=0; i<imageSourceClasses.Num(); i++)
        {
            if(imageSourceClasses[i].strClass.CompareI(lpClass))
                return imageSourceClasses+i;
        }

        return NULL;
    }

    //---------------------------------------------------

    void DeleteItems();
    void MoveSourcesUp();
    void MoveSourcesDown();
    void MoveSourcesToTop();
    void MoveSourcesToBottom();
    void CenterItems();
    void FitItemsToScreen();
    void ResetItemSizes();

    void Start();
    void Stop();

    void MainCaptureLoop();
    static DWORD STDCALL MainCaptureThread(LPVOID lpUnused);

    bool QueryNewAudio();
    void MainAudioLoop();
    static DWORD STDCALL MainAudioThread(LPVOID lpUnused);

    static void STDCALL StartStreamHotkey(DWORD hotkey, UPARAM param, bool bDown);
    static void STDCALL StopStreamHotkey(DWORD hotkey, UPARAM param, bool bDown);

    static void STDCALL PushToTalkHotkey(DWORD hotkey, UPARAM param, bool bDown);
    static void STDCALL MuteMicHotkey(DWORD hotkey, UPARAM param, bool bDown);
    static void STDCALL MuteDesktopHotkey(DWORD hotkey, UPARAM param, bool bDown);

    static void GetNewSceneName(String &strScene);
    static void GetNewSourceName(String &strSource);

    static DWORD STDCALL HotkeyThread(LPVOID lpUseless);


    static INT_PTR CALLBACK EnterGlobalSourceNameDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK EnterSourceNameDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK EnterSceneNameDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK SceneHotkeyDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK ReconnectDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK ListboxHook(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK RenderFrameProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK OBSProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    static INT_PTR CALLBACK SettingsDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    void ResizeRenderFrame(bool bRedrawRenderFrame);
    void ResizeWindow(bool bRedrawRenderFrame);

    void ToggleCapturing();

    Scene* CreateScene(CTSTR lpClassName, XElement *data);
    void ConfigureScene(XElement *element);
    void ConfigureImageSource(XElement *element);

    static INT_PTR CALLBACK PluginsDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    void GetGlobalSourceNames(List<CTSTR> &globalSourceNames);
    XElement* GetGlobalSourceElement(CTSTR lpName);

    static INT_PTR CALLBACK GlobalSourcesProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static bool STDCALL ConfigGlobalSource(XElement *element, bool bCreating);

    void CallHotkey(DWORD hotkeyID, bool bDown);

    void SetStatusBarData();

    static void ClearStatusBar();
    static void DrawStatusBar(DRAWITEMSTRUCT &dis);

public:
    OBS();
    ~OBS();

    char* EncMetaData(char *enc, char *pend, bool bFLVFile=false);

    inline void PostStopMessage() {if(hwndMain) PostMessage(hwndMain, OBS_REQUESTSTOP, 0, 0);}

    void GetBaseSize(UINT &width, UINT &height) const;

    inline void GetRenderFrameSize(UINT &width, UINT &height) const    {width = renderFrameWidth; height = renderFrameHeight;}
    inline void GetOutputSize(UINT &width, UINT &height) const         {width = outputCX;         height = outputCY;}

    inline Vect2 GetBaseSize() const
    {
        UINT width, height;
        GetBaseSize(width, height);
        return Vect2(float(width), float(height));
    }

    inline Vect2 GetOutputSize()      const {return Vect2(float(outputCX), float(outputCY));}
    inline Vect2 GetRenderFrameSize() const {return Vect2(float(renderFrameWidth), float(renderFrameHeight));}

    inline bool SSE2Available() const {return bSSE2Available;}

    inline AudioEncoder* GetAudioEncoder() const {return audioEncoder;}
    inline VideoEncoder* GetVideoEncoder() const {return videoEncoder;}

    inline void EnterSceneMutex() {OSEnterMutex(hSceneMutex);}
    inline void LeaveSceneMutex() {OSLeaveMutex(hSceneMutex);}

    inline bool IsRunning()    const {return bRunning;}
    inline UINT GetFPS()       const {return fps;}
    inline UINT GetFrameTime() const {return frameTime;}

    inline UINT NumMonitors()  const {return monitors.Num();}
    inline const MonitorInfo& GetMonitor(UINT id) const {if(id < monitors.Num()) return monitors[id]; else return monitors[0];}

    inline XElement* GetSceneElement() const {return sceneElement;}

    virtual HICON GetIcon(HINSTANCE hInst, int resource);
    virtual HFONT GetFont(CTSTR lpFontFace, int fontSize, int fontWeight);

    inline void GetVideoHeaders(DataPacket &packet) {videoEncoder->GetHeaders(packet);}
    inline void GetAudioHeaders(DataPacket &packet) {audioEncoder->GetHeaders(packet);}

    inline void SetStreamReport(CTSTR lpStreamReport) {streamReport = lpStreamReport;}

    UINT AddStreamInfo(CTSTR lpInfo, StreamInfoPriority priority);
    void SetStreamInfo(UINT infoID, CTSTR lpInfo);
    void SetStreamInfoPriority(UINT infoID, StreamInfoPriority priority);
    void RemoveStreamInfo(UINT infoID);
    String GetMostImportantInfo();

    //---------------------------------------------------------------------------

    virtual void RegisterSceneClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc);
    virtual void RegisterImageSourceClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc);

    virtual ImageSource* CreateImageSource(CTSTR lpClassName, XElement *data);

    virtual bool SetScene(CTSTR lpScene);
};

ID3D10Blob* CompileShader(CTSTR lpShader, LPCSTR lpTarget);

LONG CALLBACK OBSExceptionHandler (PEXCEPTION_POINTERS exceptionInfo);

