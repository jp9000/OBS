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
class SettingsPane;
struct EncoderPicture;

#define NUM_RENDER_BUFFERS 2

static const int minClientWidth  = 640;
static const int minClientHeight = 275;

static const int defaultClientWidth  = 640;
static const int defaultClientHeight = 553;

enum edges {
    edgeLeft = 0x01,
    edgeRight = 0x02,
    edgeTop = 0x04,
    edgeBottom = 0x08,
};

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

enum AudioDeviceType {
    ADT_PLAYBACK,
    ADT_RECORDING
};

void GetAudioDevices(AudioDeviceList &deviceList, AudioDeviceType deviceType, bool ConntectedOnly=false, bool canDisable=false);
bool GetDefaultMicID(String &strVal);
bool GetDefaultSpeakerID(String &strVal);

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
    virtual DWORD NumTotalVideoFrames() const=0;
};

//-------------------------------------------------------------------

struct TimedPacket
{
    List<BYTE> data;
    DWORD timestamp;
    PacketType type;
};

//-------------------------------------------------------------------

class VideoFileStream
{
public:
    virtual ~VideoFileStream() {}
    virtual void AddPacket(BYTE *data, UINT size, DWORD timestamp, PacketType type)=0;
};

//-------------------------------------------------------------------

class AudioEncoder
{
    friend class OBS;

protected:
    virtual bool    Encode(float *input, UINT numInputFrames, DataPacket &packet, QWORD &timestamp)=0;
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

    virtual void RequestBuffers(LPVOID buffers) {}

public:
    virtual ~VideoEncoder() {}

    virtual int  GetBitRate() const=0;
    virtual bool DynamicBitrateSupported() const=0;
    virtual bool SetBitRate(DWORD maxBitrate, DWORD bufferSize)=0;

    virtual void GetHeaders(DataPacket &packet)=0;
    virtual void GetSEI(DataPacket &packet) {}

    virtual void RequestKeyframe() {}

    virtual String GetInfoString() const=0;

    virtual bool isQSV() { return false; }

    virtual int GetBufferedFrames() { if(HasBufferedFrames()) return -1; return 0; }
    virtual bool HasBufferedFrames() { return false; }
};

//-------------------------------------------------------------------

struct MonitorInfo
{
    inline MonitorInfo() {zero(this, sizeof(MonitorInfo));}

    inline MonitorInfo(HMONITOR hMonitor, RECT *lpRect)
    {
        this->hMonitor = hMonitor;
        mcpy(&this->rect, lpRect, sizeof(rect));
    }

    HMONITOR hMonitor;
    RECT rect;
    float rotationDegrees;
};

struct DeviceOutputData
{
    String strDevice;
    List<MonitorInfo> monitors;
    StringList monitorNameList;

    inline void ClearData()
    {
        strDevice.Clear();
        monitors.Clear();
        monitorNameList.Clear();
    }
};

struct DeviceOutputs
{
    List<DeviceOutputData> devices;

    inline ~DeviceOutputs()
    {
        ClearData();
    }

    inline void ClearData()
    {
        for(UINT i=0; i<devices.Num(); i++)
            devices[i].ClearData();
        devices.Clear();
    }
};

void GetDisplayDevices(DeviceOutputs &deviceList);


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
    QWORD timestamp;
};


//===============================================================================================

struct ClassInfo
{
    String strClass;
    String strName;
    OBSCREATEPROC createProc;
    OBSCONFIGPROC configProc;
    bool bDeprecated;

    inline void FreeData() {strClass.Clear(); strName.Clear();}
};

//----------------------------

/* Event callback signiture definitions */
typedef void (*OBS_CALLBACK)();
typedef void (*OBS_STATUS_CALLBACK)(bool /*running*/, bool /*streaming*/, bool /*recording*/,
                                    bool /*previewing*/, bool /*reconnecting*/);
typedef void (*OBS_STREAM_STATUS_CALLBACK)(bool /*streaming*/, bool /*previewOnly*/,
                                           UINT /*bytesPerSec*/, double /*strain*/, 
                                           UINT /*totalStreamtime*/, UINT /*numTotalFrames*/, 
                                           UINT /*numDroppedFrames*/, UINT /*fps*/);
typedef void (*OBS_SCENE_SWITCH_CALLBACK)(CTSTR);
typedef void (*OBS_SOURCE_CHANGED_CALLBACK)(CTSTR /*sourceName*/, XElement* /*source*/); 
typedef void (*OBS_VOLUME_CHANGED_CALLBACK)(float /*level*/, bool /*muted*/, bool /*finalFalue*/);

struct PluginInfo
{
    String strFile;
    HMODULE hModule;

    /* Event Callbacks */

    /* called on stream starting */
    OBS_CALLBACK startStreamCallback;
    
    /* called on stream stopping */
    OBS_CALLBACK stopStreamCallback;

    /* called when status bar is updated, even without network */
    OBS_STATUS_CALLBACK statusCallback;

    /* called when stream stats are updated in stats window */
    OBS_STREAM_STATUS_CALLBACK streamStatusCallback;

    /* called when scenes are switched */
    OBS_SCENE_SWITCH_CALLBACK sceneSwitchCallback;
    
    /* called when a scene is renamed, added, removed, or moved */
    OBS_CALLBACK scenesChangedCallback;

    /* called when the source order is changed */
    OBS_CALLBACK sourceOrderChangedCallback;
    
    /* called when a source is changed in some way */
    OBS_SOURCE_CHANGED_CALLBACK sourceChangedCallback;

    /* called when a sources have been added or removed */
    OBS_CALLBACK sourcesAddedOrRemovedCallback;

    /* called when audio source volumes have changed */
    OBS_VOLUME_CHANGED_CALLBACK micVolumeChangeCallback;
    OBS_VOLUME_CHANGED_CALLBACK desktopVolumeChangeCallback;
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
    ID_TOGGLERECORDING,
    ID_STARTSTOP,
    ID_EXIT,
    ID_SCENEEDITOR,
    ID_DESKTOPVOLUME,
    ID_MICVOLUME,
    ID_DESKTOPVOLUMEMETER,
    ID_MICVOLUMEMETER,
    ID_STATUS,
    ID_SCENES,
    ID_SCENES_TEXT,
    ID_SOURCES,
    ID_SOURCES_TEXT,
    ID_TESTSTREAM,
    ID_GLOBALSOURCES,
    ID_PLUGINS,
    ID_DASHBOARD,
    ID_MINIMIZERESTORE,

    ID_SWITCHPROFILE,
};

enum
{
    OBS_REQUESTSTOP=WM_USER+1,
    OBS_CALLHOTKEY,
    OBS_RECONNECT,
    OBS_SETSCENE,
    OBS_SETSOURCEORDER,
    OBS_SETSOURCERENDER,
    OBS_UPDATESTATUSBAR,
    OBS_NOTIFICATIONAREA,
};

//----------------------------

enum ColorPrimaries
{
    ColorPrimaries_BT709 = 1,
    ColorPrimaries_Unspecified,
    ColorPrimaries_BT470M = 4,
    ColorPrimaries_BT470BG,
    ColorPrimaries_SMPTE170M,
    ColorPrimaries_SMPTE240M,
    ColorPrimaries_Film,
    ColorPrimaries_BT2020
};

enum ColorTransfer
{
    ColorTransfer_BT709 = 1,
    ColorTransfer_Unspecified,
    ColorTransfer_BT470M = 4,
    ColorTransfer_BT470BG,
    ColorTransfer_SMPTE170M,
    ColorTransfer_SMPTE240M,
    ColorTransfer_Linear,
    ColorTransfer_Log100,
    ColorTransfer_Log316,
    ColorTransfer_IEC6196624,
    ColorTransfer_BT1361,
    ColorTransfer_IEC6196621,
    ColorTransfer_BT202010,
    ColorTransfer_BT202012
};

enum ColorMatrix
{
    ColorMatrix_GBR = 0,
    ColorMatrix_BT709,
    ColorMatrix_Unspecified,
    ColorMatrix_BT470M = 4,
    ColorMatrix_BT470BG,
    ColorMatrix_SMPTE170M,
    ColorMatrix_SMPTE240M,
    ColorMatrix_YCgCo,
    ColorMatrix_BT2020NCL,
    ColorMatrix_BT2020CL
};

struct ColorDescription
{
    int fullRange;
    int primaries;
    int transfer;
    int matrix;
};

//----------------------------

enum ItemModifyType
{
    ItemModifyType_None,
    ItemModifyType_Move,
    ItemModifyType_ScaleBottomLeft,
    ItemModifyType_CropBottomLeft,
    ItemModifyType_ScaleLeft,
    ItemModifyType_CropLeft,
    ItemModifyType_ScaleTopLeft,
    ItemModifyType_CropTopLeft,
    ItemModifyType_ScaleTop,
    ItemModifyType_CropTop,
    ItemModifyType_ScaleTopRight,
    ItemModifyType_CropTopRight,
    ItemModifyType_ScaleRight,
    ItemModifyType_CropRight,
    ItemModifyType_ScaleBottomRight,
    ItemModifyType_CropBottomRight,
    ItemModifyType_ScaleBottom,
    ItemModifyType_CropBottom
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

struct VideoPacketData
{
    List<BYTE> data;
    PacketType type;

    inline void Clear() {data.Clear();}
};

struct VideoSegment
{
    List<VideoPacketData> packets;
    DWORD timestamp;

    inline VideoSegment() : timestamp(0) {}
    inline ~VideoSegment() {Clear();}
    inline void Clear()
    {
        for(UINT i=0; i<packets.Num(); i++)
            packets[i].Clear();
        packets.Clear();
    }
};

//----------------------------

enum PreviewDrawType {
    Preview_Standard,
    Preview_Fullscreen,
    Preview_Projector
};

void ResetWASAPIAudioDevice(AudioSource *source);

struct FrameProcessInfo;

//todo: this class has become way too big, it's horrible, and I should be ashamed of myself
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
    friend class MMDeviceAudioSource;

    //---------------------------------------------------
    // graphics stuff

    IDXGISwapChain  *projectorSwap;
    ID3D10Texture2D *copyTextures[NUM_RENDER_BUFFERS];
    Texture         *mainRenderTextures[NUM_RENDER_BUFFERS];
    Texture         *yuvRenderTextures[NUM_RENDER_BUFFERS];

    Texture *transitionTexture;
    bool    bTransitioning;
    float   transitionAlpha;

    Shader  *mainVertexShader, *mainPixelShader, *yuvScalePixelShader;
    Shader  *solidVertexShader, *solidPixelShader;

    //---------------------------------------------------

    NetworkStream *network;

    //---------------------------------------------------
    // audio sources/encoder

    AudioSource  *desktopAudio;
    AudioSource  *micAudio;
    List<AudioSource*> auxAudioSources;

    UINT sampleRateHz;

    AudioEncoder *audioEncoder;

    //---------------------------------------------------
    // scene/encoder

    Scene                   *scene;
    VideoEncoder            *videoEncoder;
    HDC                     hCaptureDC;
    List<MonitorInfo>       monitors;

    UINT                    encoderSkipThreshold;

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
    void CheckSources();
    void SetSourceRender(CTSTR sourceName, bool render);

    //---------------------------------------------------
    // settings window

    int                 curSettingsSelection;
    HWND                hwndSettings;
    HWND                hwndCurrentSettings;
    bool                bSettingsChanged, bApplySettingsAborted;
    List<SettingsPane*> settingsPanes;
    SettingsPane *      currentSettingsPane;
    int                 numberOfBuiltInSettingsPanes;

    void   SetChangedSettings(bool bChanged);
    void   SetAbortApplySettings(bool abort);
    void   CancelSettings();
    void   ApplySettings();

    // Settings panes
public:
    void   AddSettingsPane(SettingsPane *pane);
    void   RemoveSettingsPane(SettingsPane *pane);

private:
    void   AddBuiltInSettingsPanes();

    friend class SettingsPane;
    friend class SettingsGeneral;
    friend class SettingsEncoding;
    friend class SettingsPublish;
    friend class SettingsVideo;
    friend class SettingsAudio;
    friend class SettingsAdvanced;

    //---------------------------------------------------
    // mainly manly main window stuff

    String  strLanguage;
    bool    bTestStream;
    bool    bUseMultithreadedOptimizations;
    bool    bRunning, bRecording, bRecordingOnly, bStartingUp, bStreaming, bKeepRecording;
    volatile bool bShutdownVideoThread, bShutdownEncodeThread;
    int     renderFrameWidth, renderFrameHeight; // The size of the preview only
    int     renderFrameX, renderFrameY; // The offset of the preview inside the preview control
    int     renderFrameCtrlWidth, renderFrameCtrlHeight; // The size of the entire preview control
    int     oldRenderFrameCtrlWidth, oldRenderFrameCtrlHeight; // The size of the entire preview control before the user began to resize the window
    HWND    hwndRenderMessage; // The text in the middle of the main window
    bool    renderFrameIn1To1Mode;
    int     borderXSize, borderYSize;
    int     clientWidth, clientHeight;
    bool    bPanelVisibleWindowed;
    bool    bPanelVisibleFullscreen;
    bool    bPanelVisible;
    bool    bPanelVisibleProcessed;
    bool    bDragResize;
    bool    bSizeChanging;
    bool    bResizeRenderView;

    bool    bAutoReconnect;
    bool    bRetrying;
    bool    bReconnecting;
    UINT    reconnectTimeout;

    bool    bDisableSceneSwitching;
    bool    bChangingSources;
    bool    bAlwaysOnTop;
    bool    bPleaseEnableProjector;   //I'm just too lazy
    bool    bPleaseDisableProjector;
    bool    bProjector;
    bool    bEnableProjectorCursor;
    UINT    projectorX, projectorY;
    UINT    projectorWidth, projectorHeight;
    UINT    projectorMonitorID;
    HWND    hwndProjector;
    Texture *projectorTexture;
    bool    bFullscreenMode;
    bool    bEditMode;
    bool    bRenderViewEnabled;
    bool    bForceRenderViewErase;
    bool    bShowFPS;
    bool    bMouseMoved;
    bool    bMouseDown;
    bool    bRMouseDown;
    bool    bItemWasSelected;
    Vect2   startMousePos, lastMousePos;
    ItemModifyType modifyType;
    SceneItem *scaleItem;

    HMENU           hmenuMain; // Main window menu so we can hide it in fullscreen mode
    WINDOWPLACEMENT fullscreenPrevPlacement;

    int     cpuInfo[4];

    //---------------------------------------------------
    // resolution/fps/downscale/etc settings

    int     lastRenderTarget;
    UINT    baseCX,   baseCY;
    UINT    scaleCX,  scaleCY;
    UINT    outputCX, outputCY;
    float   downscale;
    int     downscaleType;
    UINT    frameTime, fps;
    bool    bUsing444;
    ColorDescription colorDesc;

    //---------------------------------------------------
    // stats

    DWORD bytesPerSec;
    DWORD captureFPS;
    DWORD curFramesDropped;
    DWORD totalStreamTime;
    bool bFirstConnect;
    double curStrain;

    //---------------------------------------------------
    // main capture loop stuff

    int bufferingTime;

    HANDLE  hEncodeThread;
    HANDLE  hVideoThread;
    HANDLE  hSceneMutex;

    List<VideoSegment> bufferedVideo;

    CircularList<UINT> bufferedTimes;

    bool bRecievedFirstAudioFrame, bSentHeaders, bFirstAudioPacket;

    DWORD lastAudioTimestamp;

    QWORD firstSceneTimestamp;
    QWORD latestVideoTime;
    QWORD latestVideoTimeNS;

    bool bUseCFR;

    bool bWriteToFile;
    VideoFileStream *fileStream;

    bool bRequestKeyframe;
    int  keyframeWait;

    QWORD firstFrameTimestamp;
    EncoderPicture *curFramePic;
    HANDLE hVideoEvent;

    static DWORD STDCALL EncodeThread(LPVOID lpUnused);
    static DWORD STDCALL MainCaptureThread(LPVOID lpUnused);
    bool BufferVideoData(const List<DataPacket> &inputPackets, const List<PacketType> &inputTypes, DWORD timestamp, VideoSegment &segmentOut);
    void SendFrame(VideoSegment &curSegment, QWORD firstFrameTime);
    bool ProcessFrame(FrameProcessInfo &frameInfo);
    void EncodeLoop();  
    void MainCaptureLoop();

    void DrawPreview(const Vect2 &renderFrameSize, const Vect2 &renderFrameOffset, const Vect2 &renderFrameCtrlSize, int curRenderTarget, PreviewDrawType type);

    //---------------------------------------------------
    // main audio capture loop stuff

    CircularList<QWORD> bufferedAudioTimes;

    HANDLE  hSoundThread, hSoundDataMutex;//, hRequestAudioEvent;
    QWORD   latestAudioTime;

    float   desktopVol, micVol, curMicVol, curDesktopVol;
    float   desktopPeak, micPeak;
    float   desktopMax, micMax;
    float   desktopMag, micMag;
    List<FrameAudio> pendingAudioFrames;
    bool    bForceMicMono;
    float   desktopBoost, micBoost;

    HANDLE hAuxAudioMutex;

    //---------------------------------------------------
    // hotkey stuff

    HANDLE hHotkeyMutex;
    HANDLE hHotkeyThread;

    int pushToTalkDown;
    bool bUsingPushToTalk, bPushToTalkOn;
    long long pushToTalkDelay, pushToTalkTimeLeft;

    UINT pushToTalkHotkeyID;
    UINT muteMicHotkeyID;
    UINT muteDesktopHotkeyID;
    UINT startStreamHotkeyID;
    UINT stopStreamHotkeyID;

    bool bStartStreamHotkeyDown, bStopStreamHotkeyDown;

    static DWORD STDCALL MainAudioThread(LPVOID lpUnused);
    bool QueryAudioBuffers(bool bQueriedDesktopDebugParam);
    bool QueryNewAudio();
    void EncodeAudioSegment(float *buffer, UINT numFrames, QWORD timestamp);
    void MainAudioLoop();

    //---------------------------------------------------
    // notification area icon
    UINT wmExplorerRestarted;
    bool bNotificationAreaIcon;
    BOOL SetNotificationAreaIcon(DWORD dwMessage, int idIcon, const String &tooltip);

    //---------------------------------------------------
    // random bla-haa

    String  streamReport;

    //String  strDashboard;

    List<IconInfo> Icons;
    List<FontInfo> Fonts;

    List<ClassInfo> sceneClasses;
    List<ClassInfo> imageSourceClasses;

    List<GlobalSourceInfo> globalSources;

    HANDLE hInfoMutex;
    List<StreamInfo> streamInfoList;
    UINT streamInfoIDCounter;

    HANDLE hStartupShutdownMutex;

    //---------------------------------------------------

    List<PluginInfo> plugins;

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

    void AppendModifyListbox(HWND hwnd, HMENU hMenu, int id, int numItems, bool bSelected);
    void TrackModifyListbox(HWND hwnd, int ret);

    void DeleteItems();
    void SetSourceOrder(StringList &sourceNames);
    void MoveSourcesUp();
    void MoveSourcesDown();
    void MoveSourcesToTop();
    void MoveSourcesToBottom();
    void CenterItems(bool horizontal, bool vertical);
    void MoveItemsToEdge(int horizontal, int vertical);
    void MoveItemsByPixels(int dx, int dy);
    void FitItemsToScreen();
    void ResetItemSizes();
    void ResetItemCrops();

    void Start();
    void Stop(bool overrideKeepRecording=false);
    void StartRecording();
    void StopRecording();

    static void STDCALL StartStreamHotkey(DWORD hotkey, UPARAM param, bool bDown);
    static void STDCALL StopStreamHotkey(DWORD hotkey, UPARAM param, bool bDown);

    static void STDCALL PushToTalkHotkey(DWORD hotkey, UPARAM param, bool bDown);
    static void STDCALL MuteMicHotkey(DWORD hotkey, UPARAM param, bool bDown);
    static void STDCALL MuteDesktopHotkey(DWORD hotkey, UPARAM param, bool bDown);

    void UpdateAudioMeters();

    static void GetNewSceneName(String &strScene);
    static void GetNewSourceName(String &strSource);

    static DWORD STDCALL HotkeyThread(LPVOID lpUseless);

    // Helpers for converting between window and actual coordinates for the preview
    static Vect2 MapWindowToFramePos(Vect2 mousePos);
    static Vect2 MapFrameToWindowPos(Vect2 framePos);
    static Vect2 MapWindowToFrameSize(Vect2 windowSize);
    static Vect2 MapFrameToWindowSize(Vect2 frameSize);
    static Vect2 GetWindowToFrameScale();
    static Vect2 GetFrameToWindowScale();

    // helper to valid crops as you scale items
    static bool EnsureCropValid(SceneItem *&scaleItem, Vect2 &minSize, Vect2 &snapSize, bool bControlDown, int cropEdges, bool cropSymmetric);

    static INT_PTR CALLBACK EnterGlobalSourceNameDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK EnterSourceNameDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK EnterSceneNameDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK SceneHotkeyDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK ReconnectDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK ListboxHook(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK RenderFrameProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK ProjectorFrameProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK OBSProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    static INT_PTR CALLBACK SettingsDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    void ResizeRenderFrame(bool bRedrawRenderFrame);
    void UpdateRenderViewMessage();
    void ProcessPanelVisible(bool fromResizeWindow = false);

    void ActuallyEnableProjector();
    void EnableProjector(UINT monitorID);
    void DisableProjector();

    void ToggleCapturing();
    void ToggleRecording();

    Scene* CreateScene(CTSTR lpClassName, XElement *data);
    void ConfigureScene(XElement *element);
    void ConfigureImageSource(XElement *element);

    static INT_PTR CALLBACK PluginsDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    void GetGlobalSourceNames(List<CTSTR> &globalSourceNames);
    XElement* GetGlobalSourceElement(CTSTR lpName);

    static INT_PTR CALLBACK GlobalSourcesProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static bool STDCALL ConfigGlobalSource(XElement *element, bool bCreating);

    void CallHotkey(DWORD hotkeyID, bool bDown);

    static void AddProfilesToMenu(HMENU menu);
    static void ResetProfileMenu();

    static String GetApplicationName();
    static void ResetApplicationName();

    void SetStatusBarData();

    static void ClearStatusBar();
    static void DrawStatusBar(DRAWITEMSTRUCT &dis);

    void ConfigureStreamButtons();

    void ReloadIniSettings();

public:
    OBS();
    virtual ~OBS();

    void ResizeWindow(bool bRedrawRenderFrame);
    void SetFullscreenMode(bool fullscreen);

    void RequestKeyframe(int waitTime);

    inline void AddAudioSource(AudioSource *source)
    {
        OSEnterMutex(hAuxAudioMutex);
        auxAudioSources << source;
        OSLeaveMutex(hAuxAudioMutex);
    }
    inline void RemoveAudioSource(AudioSource *source)
    {
        OSEnterMutex(hAuxAudioMutex);
        auxAudioSources.RemoveItem(source);
        OSLeaveMutex(hAuxAudioMutex);
    }

    inline UINT GetSampleRateHz() const {return sampleRateHz;}

    inline QWORD GetAudioTime() const {return latestAudioTime;}
    inline QWORD GetVideoTime() const {return latestVideoTime;}

    char* EncMetaData(char *enc, char *pend, bool bFLVFile=false);

    inline void PostStopMessage() {if(hwndMain) PostMessage(hwndMain, OBS_REQUESTSTOP, 0, 0);}

    void GetBaseSize(UINT &width, UINT &height) const;

    inline void GetRenderFrameSize(UINT &width, UINT &height) const         {width = renderFrameWidth; height = renderFrameHeight;}
    inline void GetRenderFrameOffset(UINT &x, UINT &y) const                {x = renderFrameX; y = renderFrameY;}
    inline void GetRenderFrameControlSize(UINT &width, UINT &height) const  {width = renderFrameCtrlWidth; height = renderFrameCtrlHeight;}
    inline void GetOutputSize(UINT &width, UINT &height) const              {width = outputCX;         height = outputCY;}

    inline Vect2 GetBaseSize() const
    {
        UINT width, height;
        GetBaseSize(width, height);
        return Vect2(float(width), float(height));
    }

    inline Vect2 GetOutputSize()      const         {return Vect2(float(outputCX), float(outputCY));}
    inline Vect2 GetRenderFrameSize() const         {return Vect2(float(renderFrameWidth), float(renderFrameHeight));}
    inline Vect2 GetRenderFrameOffset() const       {return Vect2(float(renderFrameX), float(renderFrameY));}
    inline Vect2 GetRenderFrameControlSize() const  {return Vect2(float(renderFrameCtrlWidth), float(renderFrameCtrlHeight));}

    inline AudioEncoder* GetAudioEncoder() const {return audioEncoder;}
    inline VideoEncoder* GetVideoEncoder() const {return videoEncoder;}

    inline void EnterSceneMutex() {OSEnterMutex(hSceneMutex);}
    inline void LeaveSceneMutex() {OSLeaveMutex(hSceneMutex);}

    inline void EnableSceneSwitching(bool bEnable) {bDisableSceneSwitching = !bEnable;}

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
    String GetMostImportantInfo(StreamInfoPriority &priority);

    inline QWORD GetSceneTimestamp() {return firstSceneTimestamp;}

    //---------------------------------------------------------------------------

    inline static CTSTR GetCurrentProfile() {return GlobalConfig->GetStringPtr(TEXT("General"), TEXT("Profile"));}
    static void GetProfiles(StringList &profileList);

    //---------------------------------------------------------------------------

    UINT NumPlugins() const {return plugins.Num();}
    const PluginInfo *GetPluginInfo(UINT idx) const {if (idx < plugins.Num()) return plugins+idx; else return NULL;}

    //---------------------------------------------------------------------------

    virtual void RegisterSceneClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc, bool bDeprecated);
    virtual void RegisterImageSourceClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc, bool bDeprecated);

    virtual ImageSource* CreateImageSource(CTSTR lpClassName, XElement *data);

    virtual bool SetScene(CTSTR lpScene);
    virtual void InsertSourceItem(UINT index, LPWSTR name, bool checked);

    //---------------------------------------------------------------------------
    // volume stuff
    virtual void SetDesktopVolume(float val, bool finalValue);
    virtual float GetDesktopVolume();
    virtual void ToggleDesktopMute();
    virtual bool GetDesktopMuted();
    
    virtual void SetMicVolume(float val, bool finalValue);
    virtual float GetMicVolume();
    virtual void ToggleMicMute();
    virtual bool GetMicMuted();

    //---------------------------------------------------------------------------

    // event reporting functions
    virtual void ReportStartStreamTrigger();
    virtual void ReportStopStreamTrigger();
    virtual void OBS::ReportOBSStatus(bool running, bool streaming, bool recording,
                                   bool previewing, bool reconnecting);
    virtual void ReportStreamStatus(bool streaming, bool previewOnly = false, 
                                   UINT bytesPerSec = 0, double strain = 0, 
                                   UINT totalStreamtime = 0, UINT numTotalFrames = 0,
                                   UINT numDroppedFrames = 0, UINT fps = 0);
    virtual void ReportSwitchScenes(CTSTR scene);
    virtual void ReportScenesChanged();
    virtual void ReportSourceOrderChanged();
    virtual void ReportSourceChanged(CTSTR sourceName, XElement* source);
    virtual void ReportSourcesAddedOrRemoved();
    virtual void ReportMicVolumeChange(float level, bool muted, bool finalValue);
    virtual void ReportDesktopVolumeChange(float level, bool muted, bool finalValue);

    // notification area icon functions
    BOOL ShowNotificationAreaIcon();
    BOOL UpdateNotificationAreaIcon();
    BOOL HideNotificationAreaIcon();

    BOOL UpdateDashboardButton();

    inline void ResetMic() {if (bRunning && micAudio) ResetWASAPIAudioDevice(micAudio);}
    void GetThreadHandles (HANDLE *videoThread, HANDLE *encodeThread);
};

LONG CALLBACK OBSExceptionHandler (PEXCEPTION_POINTERS exceptionInfo);
