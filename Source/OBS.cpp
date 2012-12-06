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


#include "Main.h"

#include <intrin.h>
#include <inttypes.h>
extern "C"
{
#include "../x264/x264.h"
}

typedef bool (*LOADPLUGINPROC)();
typedef void (*UNLOADPLUGINPROC)();


BOOL bLoggedSystemStats = FALSE;
void LogSystemStats();


VideoEncoder* CreateX264Encoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, int maxBitRate, int bufferSize);
AudioEncoder* CreateMP3Encoder(UINT bitRate);
AudioEncoder* CreateAACEncoder(UINT bitRate);

AudioSource* CreateAudioSource(bool bMic, CTSTR lpID);

ImageSource* STDCALL CreateDesktopSource(XElement *data);
bool STDCALL ConfigureDesktopSource(XElement *data, bool bCreating);

ImageSource* STDCALL CreateBitmapSource(XElement *data);
bool STDCALL ConfigureBitmapSource(XElement *element, bool bCreating);

ImageSource* STDCALL CreateBitmapTransitionSource(XElement *data);
bool STDCALL ConfigureBitmapTransitionSource(XElement *element, bool bCreating);

ImageSource* STDCALL CreateTextSource(XElement *data);
bool STDCALL ConfigureTextSource(XElement *element, bool bCreating);

ImageSource* STDCALL CreateGlobalSource(XElement *data);

//NetworkStream* CreateRTMPServer();
NetworkStream* CreateRTMPPublisher();
NetworkStream* CreateDelayedPublisher(DWORD delayTime);
NetworkStream* CreateBandwidthAnalyzer();

void StartBlankSoundPlayback();
void StopBlankSoundPlayback();

VideoEncoder* CreateNullVideoEncoder();
AudioEncoder* CreateNullAudioEncoder();
NetworkStream* CreateNullNetwork();

VideoFileStream* CreateMP4FileStream(CTSTR lpFile);
VideoFileStream* CreateFLVFileStream(CTSTR lpFile);
//VideoFileStream* CreateAVIFileStream(CTSTR lpFile);

void Convert444to420(LPBYTE input, int width, int pitch, int height, int startY, int endY, LPBYTE *output, bool bSSE2Available);

void STDCALL SceneHotkey(DWORD hotkey, UPARAM param, bool bDown);



//----------------------------

WNDPROC listboxProc = NULL;

//----------------------------

const float defaultBlendFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};

//----------------------------

BOOL CALLBACK MonitorInfoEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, List<MonitorInfo> &monitors)
{
    monitors << MonitorInfo(hMonitor, lprcMonitor);
    return TRUE;
}

const int controlPadding = 3;

const int totalControlAreaWidth  = minClientWidth;
const int totalControlAreaHeight = 158;//170;//

void OBS::ResizeRenderFrame(bool bRedrawRenderFrame)
{
    int x = controlPadding, y = controlPadding;

    UINT newRenderFrameWidth  = clientWidth  - (controlPadding*2);
    UINT newRenderFrameHeight = clientHeight - (controlPadding*2) - totalControlAreaHeight;

    Vect2 renderSize = Vect2(float(newRenderFrameWidth), float(newRenderFrameHeight));

    float renderAspect = renderSize.x/renderSize.y;
    float mainAspect;
    
    if(bRunning)
        mainAspect = float(baseCX)/float(baseCY);
    else
    {
        int monitorID = AppConfig->GetInt(TEXT("Video"), TEXT("Monitor"));
        if(monitorID >= (int)monitors.Num())
            monitorID = 0;

        RECT &screenRect = monitors[monitorID].rect;
        int defCX = screenRect.right  - screenRect.left;
        int defCY = screenRect.bottom - screenRect.top;

        int curCX = AppConfig->GetInt(TEXT("Video"), TEXT("BaseWidth"),  defCX);
        int curCY = AppConfig->GetInt(TEXT("Video"), TEXT("BaseHeight"), defCY);
        mainAspect = float(curCX)/float(curCY);
    }

    if(renderAspect > mainAspect)
    {
        renderSize.x = renderSize.y*mainAspect;
        x += int((float(newRenderFrameWidth)-renderSize.x)*0.5f);
    }
    else
    {
        renderSize.y = renderSize.x/mainAspect;
        y += int((float(newRenderFrameHeight)-renderSize.y)*0.5f);
    }

    newRenderFrameWidth  = int(renderSize.x+0.5f)&0xFFFFFFFE;
    newRenderFrameHeight = int(renderSize.y+0.5f)&0xFFFFFFFE;

    SetWindowPos(hwndRenderFrame, NULL, x, y, newRenderFrameWidth, newRenderFrameHeight, SWP_NOOWNERZORDER);

    //----------------------------------------------

    if(bRunning)
    {
        if(bRedrawRenderFrame)
        {
            renderFrameWidth  = newRenderFrameWidth;
            renderFrameHeight = newRenderFrameHeight;

            bResizeRenderView = true;
        }
    }
    else
    {
        renderFrameWidth  = newRenderFrameWidth;
        renderFrameHeight = newRenderFrameHeight;
    }
}


void OBS::GetBaseSize(UINT &width, UINT &height) const
{
    if(bRunning)
    {
        width = baseCX;
        height = baseCY;
    }
    else
    {
        int monitorID = AppConfig->GetInt(TEXT("Video"), TEXT("Monitor"));
        if(monitorID >= (int)monitors.Num())
            monitorID = 0;

        RECT &screenRect = monitors[monitorID].rect;
        int defCX = screenRect.right  - screenRect.left;
        int defCY = screenRect.bottom - screenRect.top;

        width = AppConfig->GetInt(TEXT("Video"), TEXT("BaseWidth"),  defCX);
        height = AppConfig->GetInt(TEXT("Video"), TEXT("BaseHeight"), defCY);
    }
}

void OBS::ResizeWindow(bool bRedrawRenderFrame)
{
    const int miscAreaWidth = 290;
    const int listAreaWidth = totalControlAreaWidth-miscAreaWidth;
    const int controlWidth = miscAreaWidth/2;
    const int controlHeight = 22;

    const int volControlHeight = 32;

    const int textControlHeight = 16;

    //const int statusHeight = 50;
    const int listControlWidth = listAreaWidth/2;
    //const int listControlHeight = totalControlAreaHeight - textControlHeight - controlHeight - controlPadding;

    //-----------------------------------------------------

    /*ShowWindow(GetDlgItem(hwndMain, ID_SCENES), SW_HIDE);
    ShowWindow(GetDlgItem(hwndMain, ID_SOURCES), SW_HIDE);
    ShowWindow(GetDlgItem(hwndMain, ID_SCENES_TEXT), SW_HIDE);
    ShowWindow(GetDlgItem(hwndMain, ID_SOURCES_TEXT), SW_HIDE);
    ShowWindow(GetDlgItem(hwndMain, ID_MICVOLUME), SW_HIDE);
    ShowWindow(GetDlgItem(hwndMain, ID_DESKTOPVOLUME), SW_HIDE);
    ShowWindow(GetDlgItem(hwndMain, ID_SETTINGS), SW_HIDE);
    ShowWindow(GetDlgItem(hwndMain, ID_STARTSTOP), SW_HIDE);
    ShowWindow(GetDlgItem(hwndMain, ID_SCENEEDITOR), SW_HIDE);
    ShowWindow(GetDlgItem(hwndMain, ID_EXIT), SW_HIDE);
    ShowWindow(GetDlgItem(hwndMain, ID_TESTSTREAM), SW_HIDE);
    ShowWindow(GetDlgItem(hwndMain, ID_DASHBOARD), SW_HIDE);
    ShowWindow(GetDlgItem(hwndMain, ID_GLOBALSOURCES), SW_HIDE);*/

    //-----------------------------------------------------

    ResizeRenderFrame(bRedrawRenderFrame);

    //-----------------------------------------------------

    DWORD flags = SWP_NOOWNERZORDER|SWP_SHOWWINDOW;

    int xStart = clientWidth/2 - totalControlAreaWidth/2 + (controlPadding/2 + 1);
    int yStart = clientHeight - totalControlAreaHeight;

    int xPos = xStart;
    int yPos = yStart;

    //-----------------------------------------------------

    HWND hwndTemp = GetDlgItem(hwndMain, ID_STATUS);
    //SetWindowPos(GetDlgItem(hwndMain, ID_STATUS), NULL, xPos, yPos+listControlHeight, totalWidth-controlPadding, statusHeight, 0);

    SendMessage(hwndTemp, WM_SIZE, SIZE_RESTORED, 0);

    int parts[5];
    parts[4] = -1;
    parts[3] = clientWidth-100;
    parts[2] = parts[3]-60;
    parts[1] = parts[2]-150;
    parts[0] = parts[1]-80;
    SendMessage(hwndTemp, SB_SETPARTS, 5, (LPARAM)parts);

    int resetXPos = xStart+listControlWidth*2;

    //-----------------------------------------------------

    xPos = resetXPos;
    yPos = yStart;

    SetWindowPos(GetDlgItem(hwndMain, ID_MICVOLUME), NULL, xPos, yPos, controlWidth-controlPadding, volControlHeight, flags);
    xPos += controlWidth;

    SetWindowPos(GetDlgItem(hwndMain, ID_DESKTOPVOLUME), NULL, xPos, yPos, controlWidth-controlPadding, volControlHeight, flags);
    xPos += controlWidth;

    yPos += volControlHeight+controlPadding;

    //-----------------------------------------------------

    xPos = resetXPos;

    SetWindowPos(GetDlgItem(hwndMain, ID_SETTINGS), NULL, xPos, yPos, controlWidth-controlPadding, controlHeight, flags);
    xPos += controlWidth;

    SetWindowPos(GetDlgItem(hwndMain, ID_STARTSTOP), NULL, xPos, yPos, controlWidth-controlPadding, controlHeight, flags);
    xPos += controlWidth;

    yPos += controlHeight+controlPadding;

    //-----------------------------------------------------

    xPos = resetXPos;

    SetWindowPos(GetDlgItem(hwndMain, ID_SCENEEDITOR), NULL, xPos, yPos, controlWidth-controlPadding, controlHeight, flags);
    xPos += controlWidth;

    SetWindowPos(GetDlgItem(hwndMain, ID_TESTSTREAM), NULL, xPos, yPos, controlWidth-controlPadding, controlHeight, flags);
    xPos += controlWidth;

    yPos += controlHeight+controlPadding;

    //-----------------------------------------------------

    xPos = resetXPos;

    SetWindowPos(GetDlgItem(hwndMain, ID_GLOBALSOURCES), NULL, xPos, yPos, controlWidth-controlPadding, controlHeight, flags);
    xPos += controlWidth;

    SetWindowPos(GetDlgItem(hwndMain, ID_PLUGINS), NULL, xPos, yPos, controlWidth-controlPadding, controlHeight, flags);
    xPos += controlWidth;

    yPos += controlHeight+controlPadding;

    //-----------------------------------------------------

    xPos = resetXPos;

    BOOL bStreamOutput = AppConfig->GetInt(TEXT("Publish"), TEXT("Mode")) == 0;
    BOOL bShowDashboardButton = strDashboard.IsValid() && bStreamOutput;

    SetWindowPos(GetDlgItem(hwndMain, ID_DASHBOARD), NULL, xPos, yPos, controlWidth-controlPadding, controlHeight, flags);
    xPos += controlWidth;

    ShowWindow(GetDlgItem(hwndMain, ID_DASHBOARD), bShowDashboardButton ? SW_SHOW : SW_HIDE);

    SetWindowPos(GetDlgItem(hwndMain, ID_EXIT), NULL, xPos, yPos, controlWidth-controlPadding, controlHeight, flags);
    xPos += controlWidth;

    yPos += controlHeight;

    //-----------------------------------------------------

    int listControlHeight = yPos-yStart-textControlHeight;

    xPos  = xStart;
    yPos  = yStart;

    SetWindowPos(GetDlgItem(hwndMain, ID_SCENES_TEXT), NULL, xPos+2, yPos, listControlWidth-controlPadding-2, textControlHeight, flags);
    xPos += listControlWidth;

    SetWindowPos(GetDlgItem(hwndMain, ID_SOURCES_TEXT), NULL, xPos+2, yPos, listControlWidth-controlPadding-2, textControlHeight, flags);
    xPos += listControlWidth;

    yPos += textControlHeight;
    xPos  = xStart;

    //-----------------------------------------------------

    SetWindowPos(GetDlgItem(hwndMain, ID_SCENES), NULL, xPos, yPos, listControlWidth-controlPadding, listControlHeight, flags);
    xPos += listControlWidth;

    SetWindowPos(GetDlgItem(hwndMain, ID_SOURCES), NULL, xPos, yPos, listControlWidth-controlPadding, listControlHeight, flags);
    xPos += listControlWidth;
}

Scene* STDCALL CreateNormalScene(XElement *data)
{
    return new Scene;
}

struct HotkeyInfo
{
    DWORD hotkeyID;
    DWORD hotkey;
    OBSHOTKEYPROC hotkeyProc;
    UPARAM param;
    bool bModifiersDown, bHotkeyDown, bDownSent;
};

class OBSAPIInterface : public APIInterface
{
    friend class OBS;

    List<HotkeyInfo> hotkeys;
    DWORD curHotkeyIDVal;

    void HandleHotkeys();

public:
    OBSAPIInterface() {bSSE2Availabe = App->bSSE2Available;}

    virtual void EnterSceneMutex() {App->EnterSceneMutex();}
    virtual void LeaveSceneMutex() {App->LeaveSceneMutex();}

    virtual void RegisterSceneClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc)
    {
        App->RegisterSceneClass(lpClassName, lpDisplayName, createProc, configProc);
    }

    virtual void RegisterImageSourceClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc)
    {
        App->RegisterImageSourceClass(lpClassName, lpDisplayName, createProc, configProc);
    }

    virtual ImageSource* CreateImageSource(CTSTR lpClassName, XElement *data)
    {
        return App->CreateImageSource(lpClassName, data);
    }

    virtual XElement* GetSceneListElement()         {return App->scenesConfig.GetElement(TEXT("scenes"));}
    virtual XElement* GetGlobalSourceListElement()  {return App->scenesConfig.GetElement(TEXT("global sources"));}

    virtual bool SetScene(CTSTR lpScene, bool bPost)
    {
        assert(lpScene && *lpScene);

        if(!lpScene || !*lpScene)
            return false;

        if(bPost)
        {
            SendMessage(hwndMain, OBS_SETSCENE, 0, (LPARAM)sdup(lpScene));
            return true;
        }

        return App->SetScene(lpScene);
    }
    virtual Scene* GetScene() const             {return App->scene;}

    virtual CTSTR GetSceneName() const          {return App->GetSceneElement()->GetName();}
    virtual XElement* GetSceneElement()         {return App->GetSceneElement();}

    virtual UINT CreateHotkey(DWORD hotkey, OBSHOTKEYPROC hotkeyProc, UPARAM param);
    virtual void DeleteHotkey(UINT hotkeyID);

    virtual Vect2 GetBaseSize() const           {return Vect2(float(App->baseCX), float(App->baseCY));}
    virtual Vect2 GetRenderFrameSize() const    {return Vect2(float(App->renderFrameWidth), float(App->renderFrameHeight));}
    virtual Vect2 GetOutputSize() const         {return Vect2(float(App->outputCX), float(App->outputCY));}

    virtual void GetBaseSize(UINT &width, UINT &height) const           {App->GetBaseSize(width, height);}
    virtual void GetRenderFrameSize(UINT &width, UINT &height) const    {App->GetRenderFrameSize(width, height);}
    virtual void GetOutputSize(UINT &width, UINT &height) const         {App->GetOutputSize(width, height);}

    virtual UINT GetMaxFPS() const              {return App->bRunning ? App->fps : AppConfig->GetInt(TEXT("Video"), TEXT("FPS"), 30);}

    virtual CTSTR GetLanguage() const           {return App->strLanguage;}

    virtual CTSTR GetAppDataPath() const        {return lpAppDataPath;}
    virtual String GetPluginDataPath() const    {return String() << lpAppDataPath << TEXT("\\pluginData");}

    virtual HWND GetMainWindow() const          {return hwndMain;}

    virtual UINT AddStreamInfo(CTSTR lpInfo, StreamInfoPriority priority)           {return App->AddStreamInfo(lpInfo, priority);}
    virtual void SetStreamInfo(UINT infoID, CTSTR lpInfo)                           {App->SetStreamInfo(infoID, lpInfo);}
    virtual void SetStreamInfoPriority(UINT infoID, StreamInfoPriority priority)    {App->SetStreamInfoPriority(infoID, priority);}
    virtual void RemoveStreamInfo(UINT infoID)                                      {App->RemoveStreamInfo(infoID);}

    virtual bool UseMultithreadedOptimizations() const {return App->bUseMultithreadedOptimizations;}
};


OBS::OBS()
{
    App = this;

    __cpuid(cpuInfo, 1);

    bSSE2Available = (cpuInfo[3] & (1<<26)) != 0;

    hSceneMutex = OSCreateMutex();

    monitors.Clear();
    EnumDisplayMonitors(NULL, NULL, (MONITORENUMPROC)MonitorInfoEnumProc, (LPARAM)&monitors);

    INITCOMMONCONTROLSEX ecce;
    ecce.dwSize = sizeof(ecce);
    ecce.dwICC = ICC_STANDARD_CLASSES;
    if(!InitCommonControlsEx(&ecce))
        CrashError(TEXT("Could not initalize common shell controls"));

    InitHotkeyExControl(hinstMain);
    InitColorControl(hinstMain);
    InitVolumeControl();

    //-----------------------------------------------------
    // load locale

    if(!locale->LoadStringFile(TEXT("locale/en.txt")))
        AppWarning(TEXT("Could not open locale string file '%s'"), TEXT("locale/en.txt"));

    strLanguage = GlobalConfig->GetString(TEXT("General"), TEXT("Language"), TEXT("en"));
    if(!strLanguage.CompareI(TEXT("en")))
    {
        String langFile;
        langFile << TEXT("locale/") << strLanguage << TEXT(".txt");

        if(!locale->LoadStringFile(langFile))
            AppWarning(TEXT("Could not open locale string file '%s'"), langFile.Array());
    }

    //-----------------------------------------------------
    // load classes

    RegisterSceneClass(TEXT("Scene"), Str("Scene"), (OBSCREATEPROC)CreateNormalScene, NULL);
    RegisterImageSourceClass(TEXT("DesktopImageSource"), Str("Sources.SoftwareCaptureSource"), (OBSCREATEPROC)CreateDesktopSource, (OBSCONFIGPROC)ConfigureDesktopSource);
    RegisterImageSourceClass(TEXT("BitmapImageSource"), Str("Sources.BitmapSource"), (OBSCREATEPROC)CreateBitmapSource, (OBSCONFIGPROC)ConfigureBitmapSource);
    RegisterImageSourceClass(TEXT("BitmapTransitionSource"), Str("Sources.TransitionSource"), (OBSCREATEPROC)CreateBitmapTransitionSource, (OBSCONFIGPROC)ConfigureBitmapTransitionSource);
    RegisterImageSourceClass(TEXT("GlobalSource"), Str("Sources.GlobalSource"), (OBSCREATEPROC)CreateGlobalSource, (OBSCONFIGPROC)OBS::ConfigGlobalSource);

    RegisterImageSourceClass(TEXT("TextSource"), Str("Sources.TextSource"), (OBSCREATEPROC)CreateTextSource, (OBSCONFIGPROC)ConfigureTextSource);

    //-----------------------------------------------------
    // render frame class
    WNDCLASS wc;
    zero(&wc, sizeof(wc));
    wc.hInstance = hinstMain;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    wc.lpszClassName = OBS_RENDERFRAME_CLASS;
    wc.lpfnWndProc = (WNDPROC)OBS::RenderFrameProc;
    wc.hbrBackground = (HBRUSH)GetStockObject(GRAY_BRUSH);

    if(!RegisterClass(&wc))
        CrashError(TEXT("Could not register render frame class"));

    //-----------------------------------------------------
    // main window class
    wc.lpszClassName = OBS_WINDOW_CLASS;
    wc.lpfnWndProc = (WNDPROC)OBSProc;
    wc.hIcon = LoadIcon(hinstMain, MAKEINTRESOURCE(IDI_ICON1));
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszMenuName = MAKEINTRESOURCE(IDR_MAINMENU);

    if(!RegisterClass(&wc))
        CrashError(TEXT("Could not register main window class"));

    //-----------------------------------------------------
    // create main window

    int fullscreenX = GetSystemMetrics(SM_CXFULLSCREEN);
    int fullscreenY = GetSystemMetrics(SM_CYFULLSCREEN);

    borderXSize = borderYSize = 0;

    borderXSize += GetSystemMetrics(SM_CXSIZEFRAME)*2;
    borderYSize += GetSystemMetrics(SM_CYSIZEFRAME)*2;
    borderYSize += GetSystemMetrics(SM_CYMENU);
    borderYSize += GetSystemMetrics(SM_CYCAPTION);

    clientWidth  = GlobalConfig->GetInt(TEXT("General"), TEXT("Width"),  700);
    clientHeight = GlobalConfig->GetInt(TEXT("General"), TEXT("Height"), 553);

    if(clientWidth < minClientWidth)
        clientWidth = minClientWidth;
    if(clientHeight < minClientHeight)
        clientHeight = minClientHeight;

    int maxCX = fullscreenX-borderXSize;
    int maxCY = fullscreenY-borderYSize;

    if(clientWidth > maxCX)
        clientWidth = maxCX;
    if(clientHeight > maxCY)
        clientHeight = maxCY;

    int cx = clientWidth  + borderXSize;
    int cy = clientHeight + borderYSize;

    int x = (fullscreenX/2)-(cx/2);
    int y = (fullscreenY/2)-(cy/2);

    int posX = GlobalConfig->GetInt(TEXT("General"), TEXT("PosX"));
    int posY = GlobalConfig->GetInt(TEXT("General"), TEXT("PosY"));

    bool bInsideMonitors = false;
    if(posX || posY)
    {
        for(UINT i=0; i<monitors.Num(); i++)
        {
            if( posX >= monitors[i].rect.left && posX < monitors[i].rect.right  &&
                posY >= monitors[i].rect.top  && posY < monitors[i].rect.bottom )
            {
                bInsideMonitors = true;
                break;
            }
        }
    }

    if(bInsideMonitors)
    {
        x = posX;
        y = posY;
    }

    hwndMain = CreateWindowEx(WS_EX_CONTROLPARENT|WS_EX_WINDOWEDGE, OBS_WINDOW_CLASS, OBS_VERSION_STRING,
        WS_OVERLAPPED | WS_THICKFRAME | WS_MINIMIZEBOX | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        x, y, cx, cy, NULL, NULL, hinstMain, NULL);
    if(!hwndMain)
        CrashError(TEXT("Could not create main window"));

    HMENU hMenu = GetMenu(hwndMain);
    LocalizeMenu(hMenu);

    //-----------------------------------------------------
    // render frame

    hwndRenderFrame = CreateWindow(OBS_RENDERFRAME_CLASS, NULL, WS_CHILDWINDOW|WS_VISIBLE,
        0, 0, 0, 0,
        hwndMain, NULL, hinstMain, NULL);
    if(!hwndRenderFrame)
        CrashError(TEXT("Could not create render frame"));

    //-----------------------------------------------------
    // scenes listbox

    HWND hwndTemp;
    hwndTemp = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("LISTBOX"), NULL,
        WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|LBS_HASSTRINGS|WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|WS_CLIPSIBLINGS,
        0, 0, 0, 0, hwndMain, (HMENU)ID_SCENES, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    listboxProc = (WNDPROC)GetWindowLongPtr(hwndTemp, GWLP_WNDPROC);
    SetWindowLongPtr(hwndTemp, GWLP_WNDPROC, (LONG_PTR)OBS::ListboxHook);

    //-----------------------------------------------------
    // elements listbox

    hwndTemp = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("LISTBOX"), NULL,
        WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|LBS_HASSTRINGS|WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|LBS_EXTENDEDSEL|WS_CLIPSIBLINGS,
        0, 0, 0, 0, hwndMain, (HMENU)ID_SOURCES, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    SetWindowLongPtr(hwndTemp, GWLP_WNDPROC, (LONG_PTR)OBS::ListboxHook);

    //-----------------------------------------------------
    // status control

    hwndTemp = CreateWindowEx(0, STATUSCLASSNAME, NULL,
        WS_CHILD|WS_VISIBLE|SBARS_SIZEGRIP|WS_CLIPSIBLINGS,
        0, 0, 0, 0, hwndMain, (HMENU)ID_STATUS, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    //-----------------------------------------------------
    // mic volume control

    hwndTemp = CreateWindow(VOLUME_CONTROL_CLASS, NULL,
        WS_CHILDWINDOW|WS_VISIBLE|WS_CLIPSIBLINGS,
        0, 0, 0, 0, hwndMain, (HMENU)ID_MICVOLUME, 0, 0);
    SetVolumeControlIcons(hwndTemp, GetIcon(hinstMain, IDI_SOUND_MIC), GetIcon(hinstMain, IDI_SOUND_MIC_MUTED));

    if(!AppConfig->HasKey(TEXT("Audio"), TEXT("MicVolume")))
        AppConfig->SetFloat(TEXT("Audio"), TEXT("MicVolume"), 0.0f);
    SetVolumeControlValue(hwndTemp, AppConfig->GetFloat(TEXT("Audio"), TEXT("MicVolume"), 0.0f));

    AudioDeviceList audioDevices;
    GetAudioDevices(audioDevices);

    String strDevice = AppConfig->GetString(TEXT("Audio"), TEXT("Device"), NULL);
    if(strDevice.IsEmpty() || !audioDevices.HasID(strDevice))
    {
        AppConfig->SetString(TEXT("Audio"), TEXT("Device"), TEXT("Disable"));
        strDevice = TEXT("Disable");
    }

    audioDevices.FreeData();

    EnableWindow(hwndTemp, !strDevice.CompareI(TEXT("Disable")));

    //-----------------------------------------------------
    // desktop volume control

    hwndTemp = CreateWindow(VOLUME_CONTROL_CLASS, NULL,
        WS_CHILDWINDOW|WS_VISIBLE|WS_CLIPSIBLINGS,
        0, 0, 0, 0, hwndMain, (HMENU)ID_DESKTOPVOLUME, 0, 0);
    SetVolumeControlIcons(hwndTemp, GetIcon(hinstMain, IDI_SOUND_DESKTOP), GetIcon(hinstMain, IDI_SOUND_DESKTOP_MUTED));

    if(!AppConfig->HasKey(TEXT("Audio"), TEXT("DesktopVolume")))
        AppConfig->SetFloat(TEXT("Audio"), TEXT("DesktopVolume"), 1.0f);
    SetVolumeControlValue(hwndTemp, AppConfig->GetFloat(TEXT("Audio"), TEXT("DesktopVolume"), 0.0f));

    //-----------------------------------------------------
    // settings button

    hwndTemp = CreateWindow(TEXT("BUTTON"), Str("Settings"),
        WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|BS_TEXT|BS_PUSHBUTTON|WS_CLIPSIBLINGS,
        0, 0, 0, 0, hwndMain, (HMENU)ID_SETTINGS, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    //-----------------------------------------------------
    // start/stop stream button

    hwndTemp = CreateWindow(TEXT("BUTTON"), Str("MainWindow.StartStream"),
        WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|BS_TEXT|BS_PUSHBUTTON|WS_CLIPSIBLINGS,
        0, 0, 0, 0, hwndMain, (HMENU)ID_STARTSTOP, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    //-----------------------------------------------------
    // edit scene button

    hwndTemp = CreateWindow(TEXT("BUTTON"), Str("MainWindow.SceneEditor"),
        WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|BS_TEXT|BS_AUTOCHECKBOX|BS_PUSHLIKE|WS_DISABLED|WS_CLIPSIBLINGS,
        0, 0, 0, 0, hwndMain, (HMENU)ID_SCENEEDITOR, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    //-----------------------------------------------------
    // global sources button

    hwndTemp = CreateWindow(TEXT("BUTTON"), Str("GlobalSources"),
        WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|BS_TEXT|BS_PUSHBUTTON|WS_CLIPSIBLINGS,
        0, 0, 0, 0, hwndMain, (HMENU)ID_GLOBALSOURCES, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    //-----------------------------------------------------
    // test stream button

    hwndTemp = CreateWindow(TEXT("BUTTON"), Str("MainWindow.TestStream"),
        WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|BS_TEXT|BS_PUSHBUTTON|WS_CLIPSIBLINGS,
        0, 0, 0, 0, hwndMain, (HMENU)ID_TESTSTREAM, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    //-----------------------------------------------------
    // plugins button

    hwndTemp = CreateWindow(TEXT("BUTTON"), Str("MainWindow.Plugins"),
        WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|BS_TEXT|BS_PUSHBUTTON|WS_CLIPSIBLINGS,
        0, 0, 0, 0, hwndMain, (HMENU)ID_PLUGINS, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    //-----------------------------------------------------
    // dashboard button

    hwndTemp = CreateWindow(TEXT("BUTTON"), Str("MainWindow.Dashboard"),
        WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|BS_TEXT|BS_PUSHBUTTON|WS_CLIPSIBLINGS,
        0, 0, 0, 0, hwndMain, (HMENU)ID_DASHBOARD, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    //-----------------------------------------------------
    // exit button

    hwndTemp = CreateWindow(TEXT("BUTTON"), Str("MainWindow.Exit"),
        WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|BS_TEXT|BS_PUSHBUTTON|WS_CLIPSIBLINGS,
        0, 0, 0, 0, hwndMain, (HMENU)ID_EXIT, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    //-----------------------------------------------------
    // scenes text

    hwndTemp = CreateWindow(TEXT("STATIC"), Str("MainWindow.Scenes"),
        WS_CHILDWINDOW|WS_VISIBLE|WS_CLIPSIBLINGS,
        0, 0, 0, 0, hwndMain, (HMENU)ID_SCENES_TEXT, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    //-----------------------------------------------------
    // scenes text

    hwndTemp = CreateWindow(TEXT("STATIC"), Str("MainWindow.Sources"),
        WS_CHILDWINDOW|WS_VISIBLE|WS_CLIPSIBLINGS,
        0, 0, 0, 0, hwndMain, (HMENU)ID_SOURCES_TEXT, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    //-----------------------------------------------------
    // populate scenes

    hwndTemp = GetDlgItem(hwndMain, ID_SCENES);

    String strScenesConfig;
    strScenesConfig << lpAppDataPath << TEXT("\\scenes.xconfig");

    if(!scenesConfig.Open(strScenesConfig))
        CrashError(TEXT("Could not open '%s'"), strScenesConfig);

    XElement *scenes = scenesConfig.GetElement(TEXT("scenes"));
    if(!scenes)
        scenes = scenesConfig.CreateElement(TEXT("scenes"));

    UINT numScenes = scenes->NumElements();
    if(!numScenes)
    {
        XElement *scene = scenes->CreateElement(Str("Scene"));
        scene->SetString(TEXT("class"), TEXT("Scene"));
        numScenes++;
    }

    for(UINT i=0; i<numScenes; i++)
    {
        XElement *scene = scenes->GetElementByID(i);
        scene->SetString(TEXT("class"), TEXT("Scene"));
        SendMessage(hwndTemp, LB_ADDSTRING, 0, (LPARAM)scene->GetName());
    }

    //-----------------------------------------------------
    // populate sources

    if(numScenes)
    {
        String strScene = AppConfig->GetString(TEXT("General"), TEXT("CurrentScene"));
        int id = (int)SendMessage(hwndTemp, LB_FINDSTRINGEXACT, -1, 0);
        if(id == LB_ERR)
            id = 0;

        SendMessage(hwndTemp, LB_SETCURSEL, (WPARAM)id, 0);
        SendMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_SCENES, LBN_SELCHANGE), (LPARAM)GetDlgItem(hwndMain, ID_SCENES));
    }

    //-----------------------------------------------------

    hHotkeyMutex = OSCreateMutex();
    hInfoMutex = OSCreateMutex();

    //-----------------------------------------------------

    API = new OBSAPIInterface;

    strDashboard = AppConfig->GetString(TEXT("Publish"), TEXT("Dashboard"));
    strDashboard.KillSpaces();

    ResizeWindow(false);
    ShowWindow(hwndMain, SW_SHOW);

    //-----------------------------------------------------

    for(UINT i=0; i<numScenes; i++)
    {
        XElement *scene = scenes->GetElementByID(i);
        DWORD hotkey = scene->GetInt(TEXT("hotkey"));
        if(hotkey)
        {
            SceneHotkeyInfo hotkeyInfo;
            hotkeyInfo.hotkey = hotkey;
            hotkeyInfo.scene = scene;
            hotkeyInfo.hotkeyID = API->CreateHotkey(hotkey, SceneHotkey, 0);

            if(hotkeyInfo.hotkeyID)
                sceneHotkeys << hotkeyInfo;
        }
    }

    bUsingPushToTalk = AppConfig->GetInt(TEXT("Audio"), TEXT("UsePushToTalk")) != 0;
    DWORD hotkey = AppConfig->GetInt(TEXT("Audio"), TEXT("PushToTalkHotkey"));

    if(bUsingPushToTalk && hotkey)
        pushToTalkHotkeyID = API->CreateHotkey(hotkey, OBS::PushToTalkHotkey, NULL);

    hotkey = AppConfig->GetInt(TEXT("Audio"), TEXT("MuteMicHotkey"));
    if(hotkey)
        muteMicHotkeyID = API->CreateHotkey(hotkey, OBS::MuteMicHotkey, NULL);

    hotkey = AppConfig->GetInt(TEXT("Audio"), TEXT("MuteDesktopHotkey"));
    if(hotkey)
        muteDesktopHotkeyID = API->CreateHotkey(hotkey, OBS::MuteDesktopHotkey, NULL);

    hotkey = AppConfig->GetInt(TEXT("Publish"), TEXT("StopStreamHotkey"));
    if(hotkey)
        stopStreamHotkeyID = API->CreateHotkey(hotkey, OBS::StopStreamHotkey, NULL);

    hotkey = AppConfig->GetInt(TEXT("Publish"), TEXT("StartStreamHotkey"));
    if(hotkey)
        startStreamHotkeyID = API->CreateHotkey(hotkey, OBS::StartStreamHotkey, NULL);

    DWORD micBoostPercentage = AppConfig->GetInt(TEXT("Audio"), TEXT("MicBoostMultiple"), 1);
    if(micBoostPercentage < 1)
        micBoostPercentage = 1;
    else if(micBoostPercentage > 20)
        micBoostPercentage = 20;
    micBoost = float(micBoostPercentage);

    //-----------------------------------------------------
    // load plugins

    OSFindData ofd;
    HANDLE hFind = OSFindFirstFile(TEXT("plugins/*.dll"), ofd);
    if(hFind)
    {
        do 
        {
            if(!ofd.bDirectory) //why would someone give a directory a .dll extension in the first place?  pranksters.
            {
                String strLocation;
                strLocation << TEXT("plugins/") << ofd.fileName;

                HMODULE hPlugin = LoadLibrary(strLocation);
                if(hPlugin)
                {
                    LOADPLUGINPROC loadPlugin = (LOADPLUGINPROC)GetProcAddress(hPlugin, "LoadPlugin");
                    if(loadPlugin && loadPlugin())
                    {
                        PluginInfo *pluginInfo = plugins.CreateNew();
                        pluginInfo->hModule = hPlugin;
                        pluginInfo->strFile = ofd.fileName;
                    }
                    else
                        FreeLibrary(hPlugin);
                }
            }
        } while (OSFindNextFile(hFind, ofd));

        OSFindClose(hFind);
    }

    //-----------------------------------------------------

    bAutoReconnect = AppConfig->GetInt(TEXT("Publish"), TEXT("AutoReconnect"), 1) != 0;
    reconnectTimeout = AppConfig->GetInt(TEXT("Publish"), TEXT("AutoReconnectTimeout"), 10);
    if(reconnectTimeout < 5)
        reconnectTimeout = 5;

    hHotkeyThread = OSCreateThread((XTHREAD)HotkeyThread, NULL);
    
    bRenderViewEnabled = true;
}


OBS::~OBS()
{
    Stop();

    bShuttingDown = true;
    OSTerminateThread(hHotkeyThread, 250);

    for(UINT i=0; i<plugins.Num(); i++)
    {
        PluginInfo &pluginInfo = plugins[i];

        UNLOADPLUGINPROC unloadPlugin = (UNLOADPLUGINPROC)GetProcAddress(pluginInfo.hModule, "UnloadPlugin");
        if(unloadPlugin)
            unloadPlugin();

        FreeLibrary(pluginInfo.hModule);
        pluginInfo.strFile.Clear();
    }

    //DestroyWindow(hwndMain);

    RECT rcWindow;
    GetWindowRect(hwndMain, &rcWindow);

    GlobalConfig->SetInt(TEXT("General"), TEXT("PosX"),   rcWindow.left);
    GlobalConfig->SetInt(TEXT("General"), TEXT("PosY"),   rcWindow.top);
    GlobalConfig->SetInt(TEXT("General"), TEXT("Width"),  clientWidth);
    GlobalConfig->SetInt(TEXT("General"), TEXT("Height"), clientHeight);

    scenesConfig.Close(true);

    for(UINT i=0; i<Icons.Num(); i++)
        DeleteObject(Icons[i].hIcon);
    Icons.Clear();

    for(UINT i=0; i<Fonts.Num(); i++)
    {
        DeleteObject(Fonts[i].hFont);
        Fonts[i].strFontFace.Clear();
    }
    Fonts.Clear();

    for(UINT i=0; i<sceneClasses.Num(); i++)
        sceneClasses[i].FreeData();
    for(UINT i=0; i<imageSourceClasses.Num(); i++)
        imageSourceClasses[i].FreeData();

    if(hSceneMutex)
        OSCloseMutex(hSceneMutex);

    delete API;
    API = NULL;

    if(hInfoMutex)
        OSCloseMutex(hInfoMutex);
    if(hHotkeyMutex)
        OSCloseMutex(hHotkeyMutex);
}

void OBS::ToggleCapturing()
{
    if(!bRunning)
        Start();
    else
        Stop();
}

void STDCALL SceneHotkey(DWORD hotkey, UPARAM param, bool bDown)
{
    if(!bDown) return;

    XElement *scenes = API->GetSceneListElement();
    if(scenes)
    {
        UINT numScenes = scenes->NumElements();
        for(UINT i=0; i<numScenes; i++)
        {
            XElement *scene = scenes->GetElementByID(i);
            DWORD sceneHotkey = (DWORD)scene->GetInt(TEXT("hotkey"));
            if(sceneHotkey == hotkey)
            {
                App->SetScene(scene->GetName());
                return;
            }
        }
    }
}

void STDCALL OBS::StartStreamHotkey(DWORD hotkey, UPARAM param, bool bDown)
{
    if(App->bStopStreamHotkeyDown)
        return;

    if(App->bStartStreamHotkeyDown && !bDown)
        App->bStartStreamHotkeyDown = false;
    else if(!App->bRunning)
    {
        if(App->bStartStreamHotkeyDown = bDown)
            App->Start();
    }
}

void STDCALL OBS::StopStreamHotkey(DWORD hotkey, UPARAM param, bool bDown)
{
    if(App->bStartStreamHotkeyDown)
        return;

    if(App->bStopStreamHotkeyDown && !bDown)
        App->bStopStreamHotkeyDown = false;
    else if(App->bRunning)
    {
        if(App->bStopStreamHotkeyDown = bDown)
            App->Stop();
    }
}

void STDCALL OBS::PushToTalkHotkey(DWORD hotkey, UPARAM param, bool bDown)
{
    App->bPushToTalkOn = bDown;
}


void STDCALL OBS::MuteMicHotkey(DWORD hotkey, UPARAM param, bool bDown)
{
    if(!bDown) return;

    if(App->micAudio)
        App->micVol = ToggleVolumeControlMute(GetDlgItem(hwndMain, ID_MICVOLUME));
}

void STDCALL OBS::MuteDesktopHotkey(DWORD hotkey, UPARAM param, bool bDown)
{
    if(!bDown) return;

    App->desktopVol = ToggleVolumeControlMute(GetDlgItem(hwndMain, ID_DESKTOPVOLUME));
}

HICON OBS::GetIcon(HINSTANCE hInst, int resource)
{
    for(UINT i=0; i<Icons.Num(); i++)
    {
        if(Icons[i].resource == resource && Icons[i].hInst == hInst)
            return Icons[i].hIcon;
    }

    //---------------------

    IconInfo ii;
    ii.hInst = hInst;
    ii.resource = resource;
    ii.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(resource));

    Icons << ii;

    return ii.hIcon;
}

HFONT OBS::GetFont(CTSTR lpFontFace, int fontSize, int fontWeight)
{
    for(UINT i=0; i<Fonts.Num(); i++)
    {
        if(Fonts[i].strFontFace.CompareI(lpFontFace) && Fonts[i].fontSize == fontSize && Fonts[i].fontWeight == fontWeight)
            return Fonts[i].hFont;
    }

    //---------------------

    HFONT hFont = NULL;

    LOGFONT lf;
    zero(&lf, sizeof(lf));
    scpy_n(lf.lfFaceName, lpFontFace, 31);
    lf.lfHeight = fontSize;
    lf.lfWeight = fontWeight;
    lf.lfQuality = ANTIALIASED_QUALITY;

    if(hFont = CreateFontIndirect(&lf))
    {
        FontInfo &fi = *Fonts.CreateNew();

        fi.hFont = hFont;
        fi.fontSize = fontSize;
        fi.fontWeight = fontWeight;
        fi.strFontFace = lpFontFace;
    }

    return hFont;
}

ID3D10Blob* CompileShader(CTSTR lpShader, LPCSTR lpTarget)
{
    ID3D10Blob *errorMessages = NULL, *shaderBlob = NULL;

    HRESULT err = D3DX10CompileFromFile(lpShader, NULL, NULL, "main", lpTarget, D3D10_SHADER_OPTIMIZATION_LEVEL3, 0, NULL, &shaderBlob, &errorMessages, NULL);
    if(FAILED(err))
    {
        if(errorMessages)
        {
            if(errorMessages->GetBufferSize())
            {
                LPSTR lpErrors = (LPSTR)errorMessages->GetBufferPointer();
                Log(TEXT("Error compiling shader '%s':\r\n\r\n%S\r\n"), lpShader, lpErrors);
            }

            errorMessages->Release();
        }

        CrashError(TEXT("Compilation of '%s' failed"), lpShader);
    }

    return shaderBlob;
}

void OBS::Start()
{
    if(bRunning) return;

    //-------------------------------------------------------------

    fps = AppConfig->GetInt(TEXT("Video"), TEXT("FPS"), 30);
    frameTime = 1000/fps;

    //-------------------------------------------------------------

    if(!bLoggedSystemStats)
    {
        LogSystemStats();
        bLoggedSystemStats = TRUE;
    }

    //-------------------------------------------------------------

    if (OSIncompatibleModulesLoaded())
    {
        MessageBox(hwndMain, Str("IncompatibleModules"), NULL, MB_ICONERROR);
        Log(TEXT("Incompatible modules detected."));
        return;
    }

    //-------------------------------------------------------------

    int networkMode = AppConfig->GetInt(TEXT("Publish"), TEXT("Mode"), 2);
    DWORD delayTime = (DWORD)AppConfig->GetInt(TEXT("Publish"), TEXT("Delay"));

    String strError;

    if(bTestStream)
        network = CreateNullNetwork();
    else
    {
        switch(networkMode)
        {
            case 0: network = (delayTime > 0) ? CreateDelayedPublisher(delayTime) : CreateRTMPPublisher(); break;
            case 1: network = CreateNullNetwork(); break;
        }
    }

    if(!network)
    {
        if(!bReconnecting)
            MessageBox(hwndMain, strError, NULL, MB_ICONERROR);
        else
            DialogBox(hinstMain, MAKEINTRESOURCE(IDD_RECONNECTING), hwndMain, OBS::ReconnectDialogProc);
        return;
    }

    bReconnecting = false;

    //-------------------------------------------------------------

    Log(TEXT("=====Stream Start====================================================================="));

    //-------------------------------------------------------------

    int monitorID = AppConfig->GetInt(TEXT("Video"), TEXT("Monitor"));
    if(monitorID >= (int)monitors.Num())
        monitorID = 0;

    RECT &screenRect = monitors[monitorID].rect;
    int defCX = screenRect.right  - screenRect.left;
    int defCY = screenRect.bottom - screenRect.top;

    downscale = AppConfig->GetFloat(TEXT("Video"), TEXT("Downscale"), 1.0f);
    baseCX = AppConfig->GetInt(TEXT("Video"), TEXT("BaseWidth"),  defCX);
    baseCY = AppConfig->GetInt(TEXT("Video"), TEXT("BaseHeight"), defCY);

    baseCX = MIN(MAX(baseCX, 128), 4096);
    baseCY = MIN(MAX(baseCY, 128), 4096);

    scaleCX = UINT(double(baseCX) / double(downscale));
    scaleCY = UINT(double(baseCY) / double(downscale));

    //align width to 128bit for fast SSE YUV4:2:0 conversion
    outputCX = scaleCX & 0xFFFFFFFC;
    outputCY = scaleCY & 0xFFFFFFFE;

    bUseMultithreadedOptimizations = AppConfig->GetInt(TEXT("General"), TEXT("UseMultithreadedOptimizations"), TRUE) != 0;
    Log(TEXT("  Multithreaded optimizations: %s"), (CTSTR)(bUseMultithreadedOptimizations ? TEXT("On") : TEXT("Off")));

    //------------------------------------------------------------------

    Log(TEXT("  Base resolution: %ux%u"), baseCX, baseCY);
    Log(TEXT("  Output resolution: %ux%u"), outputCX, outputCY);
    Log(TEXT("------------------------------------------"));

    //------------------------------------------------------------------

    GS = new D3D10System;
    GS->Init();

    //-------------------------------------------------------------

    mainVertexShader    = CreateVertexShaderFromFile(TEXT("shaders/DrawTexture.vShader"));
    mainPixelShader     = CreatePixelShaderFromFile(TEXT("shaders/DrawTexture.pShader"));

    solidVertexShader   = CreateVertexShaderFromFile(TEXT("shaders/DrawSolid.vShader"));
    solidPixelShader    = CreatePixelShaderFromFile(TEXT("shaders/DrawSolid.pShader"));

    //------------------------------------------------------------------

    CTSTR lpShader = NULL;
    if(CloseFloat(downscale, 1.0))
        lpShader = TEXT("shaders/DrawYUVTexture.pShader");
    else if(CloseFloat(downscale, 1.5))
        lpShader = TEXT("shaders/DownscaleYUV1.5.pShader");
    else if(CloseFloat(downscale, 2.0))
        lpShader = TEXT("shaders/DownscaleYUV2.pShader");
    else if(CloseFloat(downscale, 2.25))
        lpShader = TEXT("shaders/DownscaleYUV2.25.pShader");
    else if(CloseFloat(downscale, 3.0))
        lpShader = TEXT("shaders/DownscaleYUV3.pShader");
    else
        CrashError(TEXT("Invalid downscale value (must be either 1.0, 1.5, 2.0, 2.25, or 3.0)"));

    yuvScalePixelShader = CreatePixelShaderFromFile(lpShader);

    //-------------------------------------------------------------

    for(int i=0; i<NUM_RENDER_BUFFERS; i++)
    {
        mainRenderTextures[i] = CreateRenderTarget(baseCX, baseCY, GS_BGRA, FALSE);
        yuvRenderTextures[i]  = CreateRenderTarget(outputCX, outputCY, GS_BGRA, FALSE);
    }

    //-------------------------------------------------------------

    D3D10_TEXTURE2D_DESC td;
    zero(&td, sizeof(td));
    td.Width            = outputCX;
    td.Height           = outputCY;
    td.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.SampleDesc.Count = 1;
    td.ArraySize        = 1;
    td.Usage            = D3D10_USAGE_STAGING;
    td.CPUAccessFlags   = D3D10_CPU_ACCESS_READ;

    for(UINT i=0; i<2; i++)
    {
        HRESULT err = GetD3D()->CreateTexture2D(&td, NULL, &copyTextures[i]);
        if(FAILED(err))
        {
            CrashError(TEXT("Unable to create copy texture"));
            //todo - better error handling
        }
    }

    //-------------------------------------------------------------

    desktopAudio = CreateAudioSource(false, NULL);
    if(!desktopAudio)
        CrashError(TEXT("Cannot initialize desktop audio sound, more info in the log file."));

    AudioDeviceList audioDevices;
    GetAudioDevices(audioDevices);

    String strDevice = AppConfig->GetString(TEXT("Audio"), TEXT("Device"), NULL);
    if(strDevice.IsEmpty() || !audioDevices.HasID(strDevice))
    {
        AppConfig->SetString(TEXT("Audio"), TEXT("Device"), TEXT("Disable"));
        strDevice = TEXT("Disable");
    }

    audioDevices.FreeData();

    String strDefaultMic;
    bool bHasDefault = GetDefaultMicID(strDefaultMic);

    if(strDevice.CompareI(TEXT("Disable")))
        EnableWindow(GetDlgItem(hwndMain, ID_MICVOLUME), FALSE);
    else
    {
        bool bUseDefault = strDevice.CompareI(TEXT("Default")) != 0;
        if(!bUseDefault || bHasDefault)
        {
            if(bUseDefault)
                strDevice = strDefaultMic;
            micAudio = CreateAudioSource(true, strDevice);
            if(!micAudio)
                MessageBox(hwndMain, Str("MicrophoneFailure"), NULL, 0);

            EnableWindow(GetDlgItem(hwndMain, ID_MICVOLUME), micAudio != NULL);
        }
        else
            EnableWindow(GetDlgItem(hwndMain, ID_MICVOLUME), FALSE);
    }

    //-------------------------------------------------------------

    UINT bitRate = (UINT)AppConfig->GetInt(TEXT("Audio Encoding"), TEXT("Bitrate"), 96);
    String strEncoder = AppConfig->GetString(TEXT("Audio Encoding"), TEXT("Codec"), TEXT("MP3"));

    if(strEncoder.CompareI(TEXT("AAC")))
        audioEncoder = CreateAACEncoder(bitRate);
    else
        audioEncoder = CreateMP3Encoder(bitRate);

    //-------------------------------------------------------------

    desktopVol = AppConfig->GetFloat(TEXT("Audio"), TEXT("DesktopVolume"), 1.0f);
    micVol     = AppConfig->GetFloat(TEXT("Audio"), TEXT("MicVolume"),     1.0f);

    //-------------------------------------------------------------

    bRunning = true;

    if(sceneElement)
    {
        scene = CreateScene(sceneElement->GetString(TEXT("class")), sceneElement->GetElement(TEXT("data")));
        XElement *sources = sceneElement->GetElement(TEXT("sources"));
        if(sources)
        {
            UINT numSources = sources->NumElements();
            for(UINT i=0; i<numSources; i++)
            {
                SceneItem *item = scene->AddImageSource(sources->GetElementByID(i));
                if(item)
                {
                    if(SendMessage(GetDlgItem(hwndMain, ID_SOURCES), LB_GETSEL, i, 0) > 0)
                        item->Select(true);
                }
            }
        }

        scene->BeginScene();
    }

    if(scene && scene->HasMissingSources())
        MessageBox(hwndMain, Str("Scene.MissingSources"), NULL, 0);

    //-------------------------------------------------------------

    int maxBitRate = AppConfig->GetInt   (TEXT("Video Encoding"), TEXT("MaxBitrate"), 1000);
    int bufferSize = AppConfig->GetInt   (TEXT("Video Encoding"), TEXT("BufferSize"), 1000);
    int quality    = AppConfig->GetInt   (TEXT("Video Encoding"), TEXT("Quality"),    8);
    String preset  = AppConfig->GetString(TEXT("Video Encoding"), TEXT("Preset"),     TEXT("veryfast"));
    bUsing444      = AppConfig->GetInt   (TEXT("Video Encoding"), TEXT("Use444"),     0) != 0;

    bUseSyncFix    = AppConfig->GetInt   (TEXT("Video Encoding"), TEXT("UseSyncFix"), 0) != 0;

    if(bUseSyncFix)
    {
        Log(TEXT("------------------------------------------"));
        Log(TEXT("  Using audio/video sync fix"));
    }

    //-------------------------------------------------------------

    bWriteToFile = networkMode == 1 || AppConfig->GetInt(TEXT("Publish"), TEXT("SaveToFile")) != 0;
    String strOutputFile = AppConfig->GetString(TEXT("Publish"), TEXT("SavePath"));

    if(OSFileExists(strOutputFile))
    {
        strOutputFile.FindReplace(TEXT("\\"), TEXT("/"));
        String strFileWithoutExtension = GetPathWithoutExtension(strOutputFile);
        String strFileExtension = GetPathExtension(strOutputFile);
        UINT curFile = 0;

        String strNewFilePath;
        do 
        {
            strNewFilePath.Clear() << strFileWithoutExtension << TEXT(" (") << FormattedString(TEXT("%02u"), ++curFile) << TEXT(").") << strFileExtension;
        } while(OSFileExists(strNewFilePath));

        strOutputFile = strNewFilePath;
    }

    //-------------------------------------------------------------

    bRecievedFirstAudioFrame = false;

    bForceMicMono = AppConfig->GetInt(TEXT("Audio"), TEXT("ForceMicMono")) != 0;

    hRequestAudioEvent = CreateSemaphore(NULL, 0, 0x7FFFFFFFL, NULL);
    hSoundDataMutex = OSCreateMutex();
    hSoundThread = OSCreateThread((XTHREAD)OBS::MainAudioThread, NULL);

    //-------------------------------------------------------------

    StartBlankSoundPlayback();

    //-------------------------------------------------------------

    ctsOffset = 0;
    videoEncoder = CreateX264Encoder(fps, outputCX, outputCY, quality, preset, bUsing444, maxBitRate, bufferSize);

    //-------------------------------------------------------------

    if(!bTestStream && bWriteToFile && strOutputFile.IsValid())
    {
        String strFileExtension = GetPathExtension(strOutputFile);
        if(strFileExtension.CompareI(TEXT("flv")))
            fileStream = CreateFLVFileStream(strOutputFile);
        else if(strFileExtension.CompareI(TEXT("mp4")))
            fileStream = CreateMP4FileStream(strOutputFile);
        //else if(strFileExtension.CompareI(TEXT("avi")))
        //    fileStream = CreateAVIFileStream(strOutputFile));
    }

    hMainThread = OSCreateThread((XTHREAD)OBS::MainCaptureThread, NULL);

    if(bTestStream)
    {
        EnableWindow(GetDlgItem(hwndMain, ID_STARTSTOP), FALSE);
        SetWindowText(GetDlgItem(hwndMain, ID_TESTSTREAM), Str("MainWindow.StopTest"));
    }
    else
    {
        EnableWindow(GetDlgItem(hwndMain, ID_TESTSTREAM), FALSE);
        SetWindowText(GetDlgItem(hwndMain, ID_STARTSTOP), Str("MainWindow.StopStream"));
    }

    EnableWindow(GetDlgItem(hwndMain, ID_SCENEEDITOR), TRUE);

    //-------------------------------------------------------------

    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 0, 0, 0);
}

StatusBarDrawData statusBarData;

void OBS::ClearStatusBar()
{
    HWND hwndStatusBar = GetDlgItem(hwndMain, ID_STATUS);
    PostMessage(hwndStatusBar, SB_SETTEXT, 0, NULL);
    PostMessage(hwndStatusBar, SB_SETTEXT, 1, NULL);
    PostMessage(hwndStatusBar, SB_SETTEXT, 2, NULL);
    PostMessage(hwndStatusBar, SB_SETTEXT, 3, NULL);
    PostMessage(hwndStatusBar, SB_SETTEXT, 4, NULL);
}

void OBS::SetStatusBarData()
{
    HWND hwndStatusBar = GetDlgItem(hwndMain, ID_STATUS);

    SendMessage(hwndStatusBar, WM_SETREDRAW, 0, 0);
    SendMessage(hwndStatusBar, SB_SETTEXT, 0 | SBT_OWNERDRAW, NULL);
    SendMessage(hwndStatusBar, SB_SETTEXT, 1 | SBT_OWNERDRAW, NULL);
    SendMessage(hwndStatusBar, SB_SETTEXT, 2 | SBT_OWNERDRAW, NULL);
    SendMessage(hwndStatusBar, SB_SETTEXT, 3 | SBT_OWNERDRAW, NULL);
    SendMessage(hwndStatusBar, SB_SETTEXT, 4 | SBT_OWNERDRAW, NULL);

    SendMessage(hwndStatusBar, WM_SETREDRAW, 1, 0);
    InvalidateRect(hwndStatusBar, NULL, FALSE);
}

void OBS::DrawStatusBar(DRAWITEMSTRUCT &dis)
{
    if(!App->bRunning)
        return;

    HDC hdcTemp = CreateCompatibleDC(dis.hDC);
    HBITMAP hbmpTemp = CreateCompatibleBitmap(dis.hDC, dis.rcItem.right-dis.rcItem.left, dis.rcItem.bottom-dis.rcItem.top);
    SelectObject(hdcTemp, hbmpTemp);

    SelectObject(hdcTemp, GetCurrentObject(dis.hDC, OBJ_FONT));

    //HBRUSH  hColorBrush = CreateSolidBrush((green<<8)|red);

    RECT rc;
    mcpy(&rc, &dis.rcItem, sizeof(rc));

    rc.left   -= dis.rcItem.left;
    rc.right  -= dis.rcItem.left;
    rc.top    -= dis.rcItem.top;
    rc.bottom -= dis.rcItem.top;

    FillRect(hdcTemp, &rc, (HBRUSH)(COLOR_BTNFACE+1));

    //DeleteObject(hColorBrush);

    //--------------------------------

    if(dis.itemID == 4)
    {
        DWORD green = 0xFF, red = 0xFF;

        statusBarData.bytesPerSec = App->bytesPerSec;
        statusBarData.strain = App->curStrain;
        //statusBarData.strain = rand()%101;

        if(statusBarData.strain > 50.0)
            green = DWORD(((50.0-(statusBarData.strain-50.0))/50.0)*255.0);

        double redStrain = statusBarData.strain/50.0;
        if(redStrain > 1.0)
            redStrain = 1.0;

        red = DWORD(redStrain*255.0);

        //--------------------------------

        HBRUSH  hColorBrush = CreateSolidBrush((green<<8)|red);

        RECT rcBox = {0, 0, 20, 20};
        /*rc.left += dis.rcItem.left;
        rc.right += dis.rcItem.left;
        rc.top += dis.rcItem.top;
        rc.bottom += dis.rcItem.top;*/
        FillRect(hdcTemp, &rcBox, hColorBrush);

        DeleteObject(hColorBrush);

        //--------------------------------

        SetBkMode(hdcTemp, TRANSPARENT);

        rc.left += 22;

        String strKBPS;
        strKBPS << IntString((statusBarData.bytesPerSec*8) >> 10) << TEXT("kb/s");
        //strKBPS << IntString(rand()) << TEXT("kb/s");
        DrawText(hdcTemp, strKBPS, strKBPS.Length(), &rc, DT_VCENTER|DT_SINGLELINE|DT_LEFT);
    }
    else
    {
        String strOutString;

        switch(dis.itemID)
        {
            case 0: strOutString << App->GetMostImportantInfo(); break;
            case 1:
                {
                    DWORD streamTimeSecondsTotal = App->totalStreamTime/1000;
                    DWORD streamTimeMinutesTotal = streamTimeSecondsTotal/60;
                    DWORD streamTimeSeconds = streamTimeSecondsTotal%60;

                    DWORD streamTimeHours = streamTimeMinutesTotal/60;
                    DWORD streamTimeMinutes = streamTimeMinutesTotal%60;

                    strOutString = FormattedString(TEXT("%u:%02u:%02u"), streamTimeHours, streamTimeMinutes, streamTimeSeconds);
                }
                break;
            case 2: strOutString << Str("MainWindow.DroppedFrames") << TEXT(" ") << IntString(App->curFramesDropped); break;
            case 3: strOutString << TEXT("FPS: ") << IntString(App->captureFPS); break;
        }

        if(strOutString.IsValid())
        {
            SetBkMode(hdcTemp, TRANSPARENT);
            DrawText(hdcTemp, strOutString, strOutString.Length(), &rc, DT_VCENTER|DT_SINGLELINE|DT_LEFT);
        }
    }

    //--------------------------------

    BitBlt(dis.hDC, dis.rcItem.left, dis.rcItem.top, dis.rcItem.right-dis.rcItem.left, dis.rcItem.bottom-dis.rcItem.top, hdcTemp, 0, 0, SRCCOPY);

    DeleteObject(hdcTemp);
    DeleteObject(hbmpTemp);
}

void OBS::Stop()
{
    if(!bRunning) return;

    bRunning = false;
    if(hMainThread)
    {
        OSTerminateThread(hMainThread, 20000);
        hMainThread = NULL;
    }

    for(UINT i=0; i<globalSources.Num(); i++)
        globalSources[i].source->EndScene();

    if(scene)
        scene->EndScene();

    //-------------------------------------------------------------

    if(hSoundThread)
    {
        ReleaseSemaphore(hRequestAudioEvent, 1, NULL);
        OSTerminateThread(hSoundThread, 20000);
    }

    if(hRequestAudioEvent)
        CloseHandle(hRequestAudioEvent);
    if(hSoundDataMutex)
        OSCloseMutex(hSoundDataMutex);

    hSoundThread = NULL;
    hRequestAudioEvent = NULL;
    hSoundDataMutex = NULL;

    //-------------------------------------------------------------

    StopBlankSoundPlayback();

    //-------------------------------------------------------------

    delete network;

    delete micAudio;
    delete desktopAudio;

    delete fileStream;

    delete audioEncoder;
    delete videoEncoder;

    network = NULL;
    micAudio = NULL;
    desktopAudio = NULL;
    fileStream = NULL;
    audioEncoder = NULL;
    videoEncoder = NULL;

    //-------------------------------------------------------------

    for(UINT i=0; i<pendingAudioFrames.Num(); i++)
        pendingAudioFrames[i].audioData.Clear();
    pendingAudioFrames.Clear();

    //-------------------------------------------------------------

    if(GS)
        GS->UnloadAllData();

    //-------------------------------------------------------------

    delete scene;
    scene = NULL;

    for(UINT i=0; i<globalSources.Num(); i++)
        globalSources[i].FreeData();
    globalSources.Clear();

    //-------------------------------------------------------------

    for(int i=0; i<NUM_RENDER_BUFFERS; i++)
    {
        delete mainRenderTextures[i];
        delete yuvRenderTextures[i];

        mainRenderTextures[i] = NULL;
        yuvRenderTextures[i] = NULL;
    }

    for(UINT i=0; i<2; i++)
    {
        SafeRelease(copyTextures[i]);
    }

    delete transitionTexture;
    transitionTexture = NULL;

    //-------------------------------------------------------------

    delete mainVertexShader;
    delete mainPixelShader;
    delete yuvScalePixelShader;

    delete solidVertexShader;
    delete solidPixelShader;

    mainVertexShader = NULL;
    mainPixelShader = NULL;
    yuvScalePixelShader = NULL;

    solidVertexShader = NULL;
    solidPixelShader = NULL;

    //-------------------------------------------------------------

    delete GS;
    GS = NULL;

    //-------------------------------------------------------------

    ResizeRenderFrame(false);
    RedrawWindow(hwndRenderFrame, NULL, NULL, RDW_INVALIDATE|RDW_UPDATENOW);

    //-------------------------------------------------------------

    AudioDeviceList audioDevices;
    GetAudioDevices(audioDevices);

    String strDevice = AppConfig->GetString(TEXT("Audio"), TEXT("Device"), NULL);
    if(strDevice.IsEmpty() || !audioDevices.HasID(strDevice))
    {
        AppConfig->SetString(TEXT("Audio"), TEXT("Device"), TEXT("Disable"));
        strDevice = TEXT("Disable");
    }

    audioDevices.FreeData();
    EnableWindow(GetDlgItem(hwndMain, ID_MICVOLUME), !strDevice.CompareI(TEXT("Disable")));

    //-------------------------------------------------------------

    ClearStreamInfo();

    Log(TEXT("=====Stream End======================================================================="));

    if(streamReport.IsValid())
    {
        MessageBox(hwndMain, streamReport.Array(), Str("StreamReport"), MB_ICONINFORMATION|MB_OK);
        streamReport.Clear();
    }

    if(bTestStream)
    {
        SetWindowText(GetDlgItem(hwndMain, ID_TESTSTREAM), Str("MainWindow.TestStream"));
        EnableWindow(GetDlgItem(hwndMain, ID_STARTSTOP), TRUE);
    }
    else
    {
        SetWindowText(GetDlgItem(hwndMain, ID_STARTSTOP), Str("MainWindow.StartStream"));
        EnableWindow(GetDlgItem(hwndMain, ID_TESTSTREAM), TRUE);
    }

    bEditMode = false;
    SendMessage(GetDlgItem(hwndMain, ID_SCENEEDITOR), BM_SETCHECK, BST_UNCHECKED, 0);
    EnableWindow(GetDlgItem(hwndMain, ID_SCENEEDITOR), FALSE);
    ClearStatusBar();

    InvalidateRect(hwndRenderFrame, NULL, TRUE);

    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 1, 0, 0);

    bTestStream = false;
}

inline void MultiplyAudioBuffer(float *buffer, int totalFloats, float mulVal)
{
    if(!CloseFloat(mulVal, 1.0))
    {
        if(App->SSE2Available() && (UPARAM(buffer) & 0xF) == 0)
        {
            UINT alignedFloats = totalFloats & 0xFFFFFFFC;
            __m128 sseMulVal = _mm_set_ps1(mulVal);

            for(UINT i=0; i<alignedFloats; i += 4)
                _mm_store_ps(buffer+i, _mm_mul_ps(_mm_load_ps(buffer+i), sseMulVal));

            buffer      += alignedFloats;
            totalFloats -= alignedFloats;
        }

        for(int i=0; i<totalFloats; i++)
            buffer[i] *= mulVal;
    }
}


DWORD STDCALL OBS::MainCaptureThread(LPVOID lpUnused)
{
    App->MainCaptureLoop();
    return 0;
}

DWORD STDCALL OBS::MainAudioThread(LPVOID lpUnused)
{
    App->MainAudioLoop();
    return 0;
}

struct Convert444Data
{
    LPBYTE input;
    LPBYTE output[3];
    bool bKillThread;
    HANDLE hSignalConvert, hSignalComplete;
    int width, height, pitch, startY, endY;
};

DWORD STDCALL Convert444Thread(Convert444Data *data)
{
    do
    {
        WaitForSingleObject(data->hSignalConvert, INFINITE);
        if(data->bKillThread) break;

        Convert444to420(data->input, data->width, data->pitch, data->height, data->startY, data->endY, data->output, App->SSE2Available());

        SetEvent(data->hSignalComplete);
    }while(!data->bKillThread);

    return 0;
}

void OBS::MainCaptureLoop()
{
    int curRenderTarget = 0, curYUVTexture = 0, curCopyTexture = 0;
    int copyWait = NUM_RENDER_BUFFERS-1;
    UINT curStreamTime = 0, firstFrameTime = OSGetTime(), lastStreamTime = 0;
    UINT lastPTSVal = 0, lastUnmodifiedPTSVal = 0;

    bool bSentHeaders = false;

    bufferedTimes.Clear();

    Vect2 baseSize        = Vect2(float(baseCX), float(baseCY));
    Vect2 outputSize      = Vect2(float(outputCX), float(outputCY));
    Vect2 scaleSize       = Vect2(float(scaleCX), float(scaleCY));

    int numLongFrames = 0;
    int numTotalFrames = 0;

    LPVOID nullBuff = NULL;

    DWORD streamTimeStart = OSGetTime();
    totalStreamTime = 0;

    x264_picture_t outPics[2];
    x264_picture_init(&outPics[0]);
    x264_picture_init(&outPics[1]);

    if(bUsing444)
    {
        outPics[0].img.i_csp   = X264_CSP_BGRA; //although the x264 input says BGR, x264 actually will expect packed UYV
        outPics[0].img.i_plane = 1;

        outPics[1].img.i_csp   = X264_CSP_BGRA;
        outPics[1].img.i_plane = 1;
    }
    else
    {
        x264_picture_alloc(&outPics[0], X264_CSP_I420, outputCX, outputCY);
        x264_picture_alloc(&outPics[1], X264_CSP_I420, outputCX, outputCY);
    }

    int curPTS = 0;

    HANDLE hScaleVal = yuvScalePixelShader->GetParameterByName(TEXT("baseDimensionI"));

    desktopAudio->StartCapture();
    if(micAudio) micAudio->StartCapture();

    bytesPerSec = 0;
    captureFPS = 0;
    curFramesDropped = 0;
    curStrain = 0.0;
    PostMessage(hwndMain, OBS_UPDATESTATUSBAR, 0, 0);

    QWORD lastBytesSent = 0;
    DWORD lastFramesDropped = 0;
    float bpsTime = 0.0f;
    double lastStrain = 0.0f;

    int numThreads = MAX(OSGetTotalCores()-2, 1);
    HANDLE *h420Threads = (HANDLE*)Allocate(sizeof(HANDLE)*numThreads);
    Convert444Data *convertInfo = (Convert444Data*)Allocate(sizeof(Convert444Data)*numThreads);

    zero(h420Threads, sizeof(HANDLE)*numThreads);
    zero(convertInfo, sizeof(Convert444Data)*numThreads);

    for(int i=0; i<numThreads; i++)
    {
        convertInfo[i].width  = outputCX;
        convertInfo[i].height = outputCY;
        convertInfo[i].hSignalConvert  = CreateEvent(NULL, FALSE, FALSE, NULL);
        convertInfo[i].hSignalComplete = CreateEvent(NULL, FALSE, FALSE, NULL);

        if(i == 0)
            convertInfo[i].startY = 0;
        else
            convertInfo[i].startY = convertInfo[i-1].endY;

        if(i == (numThreads-1))
            convertInfo[i].endY = outputCY;
        else
            convertInfo[i].endY = ((outputCY/numThreads)*(i+1)) & 0xFFFFFFFE;
    }

    DWORD fpsTimeNumerator = 1000-(frameTime*fps);
    DWORD fpsTimeDenominator = fps;
    DWORD fpsTimeAdjust = 0;

    DWORD fpsCounter = 0;

    bool bFirstFrame = true;
    bool bFirstImage = true;
    bool bFirst420Encode = true;
    bool bUseThreaded420 = bUseMultithreadedOptimizations && (OSGetTotalCores() > 1) && !bUsing444;

    List<HANDLE> completeEvents;

    if(bUseThreaded420)
    {
        for(int i=0; i<numThreads; i++)
        {
            h420Threads[i] = OSCreateThread((XTHREAD)Convert444Thread, convertInfo+i);
            completeEvents << convertInfo[i].hSignalComplete;
        }
    }

    while(bRunning)
    {
        DWORD renderStartTime = OSGetTime();

        totalStreamTime = renderStartTime-streamTimeStart;

        DWORD frameTimeAdjust = frameTime;
        fpsTimeAdjust += fpsTimeNumerator;
        if(fpsTimeAdjust > fpsTimeDenominator)
        {
            fpsTimeAdjust -= fpsTimeDenominator;
            ++frameTimeAdjust;
        }

        bool bRenderView = !IsIconic(hwndMain) && bRenderViewEnabled;

        profileIn("frame");

        curStreamTime = renderStartTime-firstFrameTime;
        DWORD frameDelta = curStreamTime-lastStreamTime;

        if(bUseSyncFix)
        {
            OSEnterMutex(hSoundDataMutex);
            if(!pendingAudioFrames.Num())
                bufferedTimes << 0;
            else
                bufferedTimes << pendingAudioFrames.Last().timestamp;
            OSLeaveMutex(hSoundDataMutex);

            ReleaseSemaphore(hRequestAudioEvent, 1, NULL);
        }
        else
            bufferedTimes << curStreamTime;

        float fSeconds = float(frameDelta)*0.001f;
        lastStreamTime = curStreamTime;

        //------------------------------------

        OSEnterMutex(hSceneMutex);

        if(bResizeRenderView)
        {
            GS->ResizeView();
            bResizeRenderView = false;
        }

        //------------------------------------

        if(scene)
        {
            profileIn("scene->Preprocess");
            scene->Preprocess();

            for(UINT i=0; i<globalSources.Num(); i++)
                globalSources[i].source->Preprocess();

            profileOut;

            scene->Tick(fSeconds);

            for(UINT i=0; i<globalSources.Num(); i++)
                globalSources[i].source->Tick(fSeconds);
        }

        //------------------------------------

        QWORD curBytesSent = network->GetCurrentSentBytes();
        curFramesDropped = network->NumDroppedFrames();
        bool bUpdateBPS = false;

        bpsTime += fSeconds;
        if(bpsTime > 1.0f)
        {
            bytesPerSec = DWORD(curBytesSent - lastBytesSent);
            bpsTime = 0.0f;

            lastBytesSent = curBytesSent;

            captureFPS = fpsCounter;
            fpsCounter = 0;

            bUpdateBPS = true;
        }

        fpsCounter++;

        curStrain = network->GetPacketStrain();

        EnableBlending(TRUE);
        BlendFunction(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);

        //------------------------------------
        // render the mini render texture

        LoadVertexShader(mainVertexShader);
        LoadPixelShader(mainPixelShader);

        SetRenderTarget(mainRenderTextures[curRenderTarget]);

        Ortho(0.0f, baseSize.x, baseSize.y, 0.0f, -100.0f, 100.0f);
        SetViewport(0, 0, baseSize.x, baseSize.y);

        if(scene)
            scene->Render();

        //------------------------------------

        if(bTransitioning)
        {
            if(!transitionTexture)
            {
                transitionTexture = CreateTexture(baseCX, baseCY, GS_BGRA, NULL, FALSE, TRUE);
                if(transitionTexture)
                {
                    D3D10Texture *d3dTransitionTex = static_cast<D3D10Texture*>(transitionTexture);
                    D3D10Texture *d3dSceneTex = static_cast<D3D10Texture*>(mainRenderTextures[lastRenderTarget]);
                    GetD3D()->CopyResource(d3dTransitionTex->texture, d3dSceneTex->texture);
                }
                else
                    bTransitioning = false;
            }
            else if(transitionAlpha >= 1.0f)
            {
                delete transitionTexture;
                transitionTexture = NULL;

                bTransitioning = false;
            }
        }

        if(bTransitioning)
        {
            EnableBlending(TRUE);
            transitionAlpha += fSeconds*5.0f;
            if(transitionAlpha > 1.0f)
                transitionAlpha = 1.0f;
        }
        else
            EnableBlending(FALSE);

        //------------------------------------
        // render the mini view thingy

        if(bRenderView)
        {
            Vect2 renderFrameSize = Vect2(float(renderFrameWidth), float(renderFrameHeight));

            SetRenderTarget(NULL);

            LoadVertexShader(mainVertexShader);
            LoadPixelShader(mainPixelShader);

            Ortho(0.0f, renderFrameSize.x, renderFrameSize.y, 0.0f, -100.0f, 100.0f);
            SetViewport(0.0f, 0.0f, renderFrameSize.x, renderFrameSize.y);

            if(bTransitioning)
            {
                BlendFunction(GS_BLEND_ONE, GS_BLEND_ZERO);
                DrawSprite(transitionTexture, 0xFFFFFFFF, 0.0f, 0.0f, renderFrameSize.x, renderFrameSize.y);
                BlendFunction(GS_BLEND_FACTOR, GS_BLEND_INVFACTOR, transitionAlpha);
            }

            DrawSprite(mainRenderTextures[curRenderTarget], 0xFFFFFFFF, 0.0f, 0.0f, renderFrameSize.x, renderFrameSize.y);

            Ortho(0.0f, renderFrameSize.x, renderFrameSize.y, 0.0f, -100.0f, 100.0f);

            //draw selections if in edit mode
            if(bEditMode && !bSizeChanging)
            {
                LoadVertexShader(solidVertexShader);
                LoadPixelShader(solidPixelShader);
                solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0xFFFF0000);

                Ortho(0.0f, baseSize.x, baseSize.y, 0.0f, -100.0f, 100.0f);

                if(scene)
                    scene->RenderSelections();
            }
        }

        //------------------------------------
        // actual stream output

        LoadVertexShader(mainVertexShader);
        LoadPixelShader(yuvScalePixelShader);

        Texture *yuvRenderTexture = yuvRenderTextures[curRenderTarget];
        SetRenderTarget(yuvRenderTexture);

        yuvScalePixelShader->SetVector2(hScaleVal, 1.0f/baseSize);

        Ortho(0.0f, outputSize.x, outputSize.y, 0.0f, -100.0f, 100.0f);
        SetViewport(0.0f, 0.0f, outputSize.x, outputSize.y);

        //why am I using scaleSize instead of outputSize for the texture?
        //because outputSize can be trimmed by up to three pixels due to 128-bit alignment.
        //using the scale function with outputSize can cause slightly inaccurate scaled images
        if(bTransitioning)
        {
            BlendFunction(GS_BLEND_ONE, GS_BLEND_ZERO);
            DrawSpriteEx(transitionTexture, 0xFFFFFFFF, 0.0f, 0.0f, scaleSize.x, scaleSize.y, 0.0f, 0.0f, scaleSize.x, scaleSize.y);
            BlendFunction(GS_BLEND_FACTOR, GS_BLEND_INVFACTOR, transitionAlpha);
        }

        DrawSpriteEx(mainRenderTextures[curRenderTarget], 0xFFFFFFFF, 0.0f, 0.0f, outputSize.x, outputSize.y, 0.0f, 0.0f, outputSize.x, outputSize.y);

        //------------------------------------

        if(bRenderView && !copyWait)
            static_cast<D3D10System*>(GS)->swap->Present(0, 0);

        OSLeaveMutex(hSceneMutex);

        //------------------------------------
        // present/upload

        profileIn("video encoding and uploading");

        bool bEncode = true;

        if(copyWait)
        {
            copyWait--;
            bEncode = false;
        }
        else
        {
            if(bUseSyncFix)
            {
                if(bufferedTimes.Num() < 2 || bufferedTimes[1] == 0)
                {
                    if(bufferedTimes.Num() > 1)
                        bufferedTimes.Remove(0);

                    bEncode = false;
                }
            }
            else
            {
                //audio sometimes takes a bit to start -- do not start processing frames until audio has started capturing
                if(!bRecievedFirstAudioFrame)
                    bEncode = false;
                else if(bFirstFrame)
                {
                    if(bufferedTimes.Num() > 1)
                        bufferedTimes.RemoveRange(0, bufferedTimes.Num()-1);
                    lastStreamTime -= bufferedTimes[0];
                    firstFrameTime += bufferedTimes[0];
                    bufferedTimes[0] = 0;

                    bFirstFrame = false;
                }

                if(!bEncode)
                {
                    if(curYUVTexture == (NUM_RENDER_BUFFERS-1))
                        curYUVTexture = 0;
                    else
                        curYUVTexture++;
                }
            }
        }

        if(bEncode)
        {
            UINT prevCopyTexture = (curCopyTexture+1) & 1;

            ID3D10Texture2D *copyTexture = copyTextures[curCopyTexture];
            profileIn("CopyResource");

            if(!bFirst420Encode && bUseThreaded420)
            {
                WaitForMultipleObjects(completeEvents.Num(), completeEvents.Array(), TRUE, INFINITE);
                copyTexture->Unmap(0);
            }

            D3D10Texture *d3dYUV = static_cast<D3D10Texture*>(yuvRenderTextures[curYUVTexture]);
            GetD3D()->CopyResource(copyTexture, d3dYUV->texture);
            profileOut;

            ID3D10Texture2D *prevTexture = copyTextures[prevCopyTexture];

            D3D10_MAPPED_TEXTURE2D map;
            if(SUCCEEDED(prevTexture->Map(0, D3D10_MAP_READ, 0, &map)))
            {
                List<DataPacket> videoPackets;
                List<PacketType> videoPacketTypes;

                x264_picture_t &picOut = outPics[prevCopyTexture];

                if(!bUsing444)
                {
                    profileIn("conversion to 4:2:0");

                    if(bUseThreaded420)
                    {
                        x264_picture_t &newPicOut = outPics[curCopyTexture];

                        for(int i=0; i<numThreads; i++)
                        {
                            convertInfo[i].input     = (LPBYTE)map.pData;
                            convertInfo[i].pitch     = map.RowPitch;
                            convertInfo[i].output[0] = newPicOut.img.plane[0];
                            convertInfo[i].output[1] = newPicOut.img.plane[1];
                            convertInfo[i].output[2] = newPicOut.img.plane[2];
                            SetEvent(convertInfo[i].hSignalConvert);
                        }

                        if(bFirst420Encode)
                            bFirst420Encode = bEncode = false;
                    }
                    else
                    {
                        Convert444to420((LPBYTE)map.pData, outputCX, map.RowPitch, outputCY, 0, outputCY, picOut.img.plane, SSE2Available());
                        prevTexture->Unmap(0);
                    }

                    profileOut;
                }
                else
                {
                    picOut.img.i_stride[0] = map.RowPitch;
                    picOut.img.plane[0]    = (uint8_t*)map.pData;
                }

                if(bEncode && bFirstImage)
                    bFirstImage = bEncode = false;

                if(bEncode)
                {
                    //------------------------------------
                    // get timestamps

                    DWORD curTimeStamp = 0;
                    DWORD curPTSVal = 0;

                    curTimeStamp = bufferedTimes[0];
                    curPTSVal = bufferedTimes[curPTS++];

                    if(bUseSyncFix)
                    {
                        DWORD savedPTSVal = curPTSVal;

                        if(curPTSVal != 0)
                        {
                            curPTSVal = lastPTSVal+frameTimeAdjust;
                            if(curPTSVal < lastUnmodifiedPTSVal)
                                curPTSVal = lastUnmodifiedPTSVal;

                            bufferedTimes[curPTS-1] = curPTSVal;
                        }

                        lastUnmodifiedPTSVal = savedPTSVal;
                        lastPTSVal = curPTSVal;

                        //Log(TEXT("val: %u - adjusted: %u"), savedPTSVal, curPTSVal);
                    }

                    picOut.i_pts = curPTSVal;

                    //------------------------------------
                    // encode

                    profileIn("call to encoder");

                    videoEncoder->Encode(&picOut, videoPackets, videoPacketTypes, curTimeStamp, ctsOffset);
                    if(bUsing444) prevTexture->Unmap(0);

                    profileOut;

                    //------------------------------------
                    // upload

                    bool bSendingVideo = videoPackets.Num() > 0;

                    profileIn("sending stuff out");

                    //send headers before the first frame if not yet sent
                    if(bSendingVideo)
                    {
                        if(!bSentHeaders)
                        {
                            network->BeginPublishing();
                            bSentHeaders = true;

                            /*DataPacket seiPacket;
                            videoEncoder->GetSEI(seiPacket);

                            network->SendPacket(seiPacket.lpPacket, seiPacket.size, 0, PacketType_VideoHighest);*/
                        }

                        OSEnterMutex(hSoundDataMutex);

                        if(pendingAudioFrames.Num())
                        {
                            //Log(TEXT("pending frames %u, (in milliseconds): %u"), pendingAudioFrames.Num(), pendingAudioFrames.Last().timestamp-pendingAudioFrames[0].timestamp);
                            while(pendingAudioFrames.Num() && (pendingAudioFrames[0].timestamp+ctsOffset) < curTimeStamp)
                            {
                                List<BYTE> &audioData = pendingAudioFrames[0].audioData;

                                if(audioData.Num())
                                {
                                    network->SendPacket(audioData.Array(), audioData.Num(), pendingAudioFrames[0].timestamp+ctsOffset, PacketType_Audio);
                                    if(fileStream)
                                        fileStream->AddPacket(audioData.Array(), audioData.Num(), pendingAudioFrames[0].timestamp+ctsOffset, PacketType_Audio);

                                    audioData.Clear();
                                }

                                //Log(TEXT("audio packet timestamp: %u"), pendingAudioFrames[0].timestamp);

                                pendingAudioFrames[0].audioData.Clear();
                                pendingAudioFrames.Remove(0);
                            }
                        }

                        OSLeaveMutex(hSoundDataMutex);

                        for(UINT i=0; i<videoPackets.Num(); i++)
                        {
                            DataPacket &packet  = videoPackets[i];
                            PacketType type     = videoPacketTypes[i];

                            network->SendPacket(packet.lpPacket, packet.size, curTimeStamp, type);
                            if(fileStream)
                                fileStream->AddPacket(packet.lpPacket, packet.size, curTimeStamp, type);
                        }

                        curPTS--;

                        bufferedTimes.Remove(0);
                    }

                    profileOut;
                }
            }

            curCopyTexture = prevCopyTexture;

            if(curYUVTexture == (NUM_RENDER_BUFFERS-1))
                curYUVTexture = 0;
            else
                curYUVTexture++;
        }

        lastRenderTarget = curRenderTarget;

        if(curRenderTarget == (NUM_RENDER_BUFFERS-1))
            curRenderTarget = 0;
        else
            curRenderTarget++;

        if(bUpdateBPS || !CloseDouble(curStrain, lastStrain) || curFramesDropped != lastFramesDropped)
        {
            PostMessage(hwndMain, OBS_UPDATESTATUSBAR, 0, 0);
            lastStrain = curStrain;

            lastFramesDropped = curFramesDropped;
        }

        profileOut;
        profileOut;

        //------------------------------------
        // get audio while sleeping or capturing
        if(!bUseSyncFix)
            ReleaseSemaphore(hRequestAudioEvent, 1, NULL);

        //------------------------------------
        // frame sync

        DWORD renderStopTime = OSGetTime();
        DWORD totalTime = renderStopTime-renderStartTime;

        //OSDebugOut(TEXT("Frame adjust time: %d, "), frameTimeAdjust-totalTime);

        if(totalTime > frameTimeAdjust)
            numLongFrames++;

        numTotalFrames++;

        if(totalTime < frameTimeAdjust)
            OSSleep(frameTimeAdjust-totalTime);
    }

    if(!bUsing444)
    {
        if(bUseThreaded420)
        {
            for(int i=0; i<numThreads; i++)
            {
                if(h420Threads[i])
                {
                    convertInfo[i].bKillThread = true;
                    SetEvent(convertInfo[i].hSignalConvert);

                    OSTerminateThread(h420Threads[i], 10000);
                    h420Threads[i] = NULL;
                }

                if(convertInfo[i].hSignalConvert)
                {
                    CloseHandle(convertInfo[i].hSignalConvert);
                    convertInfo[i].hSignalConvert = NULL;
                }

                if(convertInfo[i].hSignalComplete)
                {
                    CloseHandle(convertInfo[i].hSignalComplete);
                    convertInfo[i].hSignalComplete = NULL;
                }
            }

            if(!bFirst420Encode)
            {
                ID3D10Texture2D *copyTexture = copyTextures[curCopyTexture];
                copyTexture->Unmap(0);
            }
        }

        x264_picture_clean(&outPics[0]);
        x264_picture_clean(&outPics[1]);
    }

    Free(h420Threads);
    Free(convertInfo);

    Log(TEXT("Total frames rendered: %d, number of frames that lagged: %d (%0.2f%%) (it's okay for some frames to lag)"), numTotalFrames, numLongFrames, (double(numLongFrames)/double(numTotalFrames))*100.0);
}

//only get audio if all audio sources have 441 frames pending
bool OBS::QueryNewAudio()
{
    UINT audioRet;

    while((audioRet = desktopAudio->GetNextBuffer()) == ContinueAudioRequest);
    if(audioRet == NoAudioAvailable)
        return false;

    if(micAudio != NULL)
    {
        while((audioRet = micAudio->GetNextBuffer()) == ContinueAudioRequest);
        if(audioRet == NoAudioAvailable)
            return false;
    }

    return true;
}

void OBS::MainAudioLoop()
{
    bPushToTalkOn = false;

    UINT curAudioFrame = 0;

    while(TRUE)
    {
        WaitForSingleObject(hRequestAudioEvent, INFINITE);
        if(!bRunning)
            break;

        //-----------------------------------------------

        float *desktopBuffer, *micBuffer;
        UINT desktopAudioFrames, micAudioFrames;

        float curMicVol;

        if(bUsingPushToTalk)
            curMicVol = bPushToTalkOn ? micVol : 0.0f;
        else
            curMicVol = micVol;

        curMicVol *= micBoost;

        bool bDesktopMuted = (desktopVol < EPSILON);
        bool bMicEnabled   = (micAudio != NULL);

        while(QueryNewAudio())
        {
            DWORD timestamp, nullTimestamp;

            desktopAudio->GetBuffer(&desktopBuffer, &desktopAudioFrames, timestamp);

            if(micAudio != NULL)
                micAudio->GetBuffer(&micBuffer, &micAudioFrames, nullTimestamp);

            UINT totalFloats = desktopAudioFrames*2;

            MultiplyAudioBuffer(desktopBuffer, totalFloats, desktopVol);
            if(bMicEnabled)
                MultiplyAudioBuffer(micBuffer, totalFloats, curMicVol);

            //-----------------
            // mix mic and desktop sound, using SSE2 if available
            // also, it's perfectly fine to just mix into the returned buffer
            if(bDesktopMuted)
            {
                if (bMicEnabled)
                {
                    desktopBuffer = micBuffer;
                    desktopAudioFrames = micAudioFrames;
                }
                else
                {
                    zero(desktopBuffer, sizeof(*desktopBuffer)*totalFloats);
                }
            }
            else if(bMicEnabled)
            {
                UINT floatsLeft    = totalFloats;
                float *desktopTemp = desktopBuffer;
                float *micTemp     = micBuffer;

                if(SSE2Available())
                {
                    UINT alignedFloats = floatsLeft & 0xFFFFFFFC;

                    if(bForceMicMono)
                    {
                        __m128 halfVal = _mm_set_ps1(0.5f);
                        for(UINT i=0; i<alignedFloats; i += 4)
                        {
                            float *micInput = micTemp+i;
                            __m128 val = _mm_load_ps(micInput);
                            __m128 shufVal = _mm_shuffle_ps(val, val, _MM_SHUFFLE(2, 3, 0, 1));

                            _mm_store_ps(micInput, _mm_mul_ps(_mm_add_ps(val, shufVal), halfVal));
                        }
                    }

                    __m128 maxVal = _mm_set_ps1(1.0f);
                    __m128 minVal = _mm_set_ps1(-1.0f);

                    for(UINT i=0; i<alignedFloats; i += 4)
                    {
                        float *pos = desktopBuffer+i;

                        __m128 mix;
                        mix = _mm_add_ps(_mm_load_ps(pos), _mm_load_ps(micBuffer+i));
                        mix = _mm_min_ps(mix, maxVal);
                        mix = _mm_max_ps(mix, minVal);

                        _mm_store_ps(pos, mix);
                    }

                    floatsLeft  &= 0x3;
                    desktopTemp += alignedFloats;
                    micTemp     += alignedFloats;
                }

                if(floatsLeft)
                {
                    if(bForceMicMono)
                    {
                        for(UINT i=0; i<floatsLeft; i += 2)
                        {
                            micTemp[i] += micTemp[i+1];
                            micTemp[i] *= 0.5f;
                            micTemp[i+1] = micTemp[i];
                        }
                    }

                    for(UINT i=0; i<floatsLeft; i++)
                    {
                        float val = desktopTemp[i]+micTemp[i];

                        if(val < -1.0f)     val = -1.0f;
                        else if(val > 1.0f) val = 1.0f;

                        desktopTemp[i] = val;
                    }
                }
            }

            DataPacket packet;
            if(audioEncoder->Encode(desktopBuffer, totalFloats>>1, packet, timestamp))
            {
                OSEnterMutex(hSoundDataMutex);

                FrameAudio *frameAudio = pendingAudioFrames.CreateNew();
                frameAudio->audioData.CopyArray(packet.lpPacket, packet.size);
                if(bUseSyncFix)
                    frameAudio->timestamp = DWORD(QWORD(curAudioFrame)*QWORD(GetAudioEncoder()->GetFrameSize())*10/441);
                else
                    frameAudio->timestamp = timestamp;

                /*DWORD calcTimestamp = DWORD(double(curAudioFrame)*double(GetAudioEncoder()->GetFrameSize())/44.1);
                Log(TEXT("returned timestamp: %u, calculated timestamp: %u"), timestamp, calcTimestamp);*/

                curAudioFrame++;

                OSLeaveMutex(hSoundDataMutex);
            }
        }

        //-----------------------------------------------

        if(!bRecievedFirstAudioFrame && pendingAudioFrames.Num())
            bRecievedFirstAudioFrame = true;
    }

    for(UINT i=0; i<pendingAudioFrames.Num(); i++)
        pendingAudioFrames[i].audioData.Clear();
}

void OBS::SelectSources()
{
    if(scene)
        scene->DeselectAll();

    HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);
    UINT numSelected = (UINT)SendMessage(hwndSources, LB_GETSELCOUNT, 0, 0);

    if(numSelected)
    {
        List<UINT> selectedItems;
        selectedItems.SetSize(numSelected);
        SendMessage(hwndSources, LB_GETSELITEMS, numSelected, (LPARAM)selectedItems.Array());

        if(scene)
        {
            for(UINT i=0; i<numSelected; i++)
            {
                SceneItem *sceneItem = scene->GetSceneItem(selectedItems[i]);
                sceneItem->bSelected = true;
            }
        }
    }
}

DWORD STDCALL OBS::HotkeyThread(LPVOID lpUseless)
{
    //-----------------------------------------------
    // check hotkeys.
    //   Why are we handling hotkeys like this?  Because RegisterHotkey and WM_HOTKEY
    // does not work with fullscreen apps.  Therefore, we use GetAsyncKeyState once
    // per frame instead.

    while(!App->bShuttingDown)
    {
        static_cast<OBSAPIInterface*>(API)->HandleHotkeys();
        OSSleep(50);
    }

    return 0;
}

void OBS::CallHotkey(DWORD hotkeyID, bool bDown)
{
    OBSAPIInterface *apiInterface = (OBSAPIInterface*)API;
    OBSHOTKEYPROC hotkeyProc = NULL;
    DWORD hotkey = 0;
    UPARAM param = NULL;

    OSEnterMutex(hHotkeyMutex);

    for(UINT i=0; i<apiInterface->hotkeys.Num(); i++)
    {
        HotkeyInfo &hi = apiInterface->hotkeys[i];
        if(hi.hotkeyID == hotkeyID)
        {
            if(!hi.hotkeyProc)
                return;

            hotkeyProc  = hi.hotkeyProc;
            param       = hi.param;
            hotkey      = hi.hotkey;
            break;
        }
    }

    OSLeaveMutex(hHotkeyMutex);

    hotkeyProc(hotkey, param, bDown);
}

UINT OBSAPIInterface::CreateHotkey(DWORD hotkey, OBSHOTKEYPROC hotkeyProc, UPARAM param)
{
    if(!hotkey)
        return 0;

    DWORD vk = LOBYTE(hotkey);
    DWORD modifier = HIBYTE(hotkey);
    DWORD fsModifiers = 0;

    if(modifier & HOTKEYF_ALT)
        fsModifiers |= MOD_ALT;
    if(modifier & HOTKEYF_CONTROL)
        fsModifiers |= MOD_CONTROL;
    if(modifier & HOTKEYF_SHIFT)
        fsModifiers |= MOD_SHIFT;

    OSEnterMutex(App->hHotkeyMutex);
    HotkeyInfo &hi      = *hotkeys.CreateNew();
    hi.hotkeyID         = ++curHotkeyIDVal;
    hi.hotkey           = hotkey;
    hi.hotkeyProc       = hotkeyProc;
    hi.param            = param;
    hi.bModifiersDown   = false;
    hi.bHotkeyDown      = false;
    OSLeaveMutex(App->hHotkeyMutex);

    return curHotkeyIDVal;
}

void OBSAPIInterface::DeleteHotkey(UINT hotkeyID)
{
    OSEnterMutex(App->hHotkeyMutex);
    for(UINT i=0; i<hotkeys.Num(); i++)
    {
        if(hotkeys[i].hotkeyID == hotkeyID)
        {
            hotkeys.Remove(i);
            break;
        }
    }
    OSLeaveMutex(App->hHotkeyMutex);
}

void OBSAPIInterface::HandleHotkeys()
{
    List<DWORD> hitKeys;

    DWORD modifiers = 0;
    if(GetAsyncKeyState(VK_MENU) & 0x8000)
        modifiers |= HOTKEYF_ALT;
    if(GetAsyncKeyState(VK_CONTROL) & 0x8000)
        modifiers |= HOTKEYF_CONTROL;
    if(GetAsyncKeyState(VK_SHIFT) & 0x8000)
        modifiers |= HOTKEYF_SHIFT;

    OSEnterMutex(App->hHotkeyMutex);

    for(UINT i=0; i<hotkeys.Num(); i++)
    {
        HotkeyInfo &info = hotkeys[i];

        DWORD hotkeyVK          = LOBYTE(info.hotkey);
        DWORD hotkeyModifiers   = HIBYTE(info.hotkey);

        bool bModifiersMatch = (hotkeyModifiers == modifiers);

        if(hotkeyModifiers && !hotkeyVK) //modifier-only hotkey
        {
            if((hotkeyModifiers & modifiers) == hotkeyModifiers)
            {
                if(!info.bHotkeyDown)
                {
                    PostMessage(hwndMain, OBS_CALLHOTKEY, TRUE, info.hotkeyID);
                    info.bDownSent = true;
                    info.bHotkeyDown = true;
                }

                continue;
            }
        }
        else
        {
            if(bModifiersMatch)
            {
                short keyState   = GetAsyncKeyState(hotkeyVK);
                bool bDown       = (keyState & 0x8000) != 0;
                bool bWasPressed = (keyState & 0x1) != 0;

                if(bDown || bWasPressed)
                {
                    if(!info.bHotkeyDown && info.bModifiersDown) //only triggers the hotkey if the actual main key was pressed second
                    {
                        PostMessage(hwndMain, OBS_CALLHOTKEY, TRUE, info.hotkeyID);
                        info.bDownSent = true;
                    }

                    info.bHotkeyDown = true;
                    if(bDown)
                        continue;
                }
            }
        }

        info.bModifiersDown = bModifiersMatch;

        if(info.bHotkeyDown) //key up
        {
            if(info.bDownSent)
            {
                PostMessage(hwndMain, OBS_CALLHOTKEY, FALSE, info.hotkeyID);
                info.bDownSent = false;
            }

            info.bHotkeyDown = false;
        }
    }

    OSLeaveMutex(App->hHotkeyMutex);
}

UINT OBS::AddStreamInfo(CTSTR lpInfo, StreamInfoPriority priority)
{
    OSEnterMutex(hInfoMutex);

    StreamInfo &streamInfo = *streamInfoList.CreateNew();
    UINT id = streamInfo.id = ++streamInfoIDCounter;
    streamInfo.priority = priority;
    streamInfo.strInfo = lpInfo;

    OSLeaveMutex(hInfoMutex);

    return id;
}

void OBS::SetStreamInfo(UINT infoID, CTSTR lpInfo)
{
    OSEnterMutex(hInfoMutex);

    for(UINT i=0; i<streamInfoList.Num(); i++)
    {
        if(streamInfoList[i].id == infoID)
        {
            streamInfoList[i].strInfo = lpInfo;
            break;
        }
    }

    OSLeaveMutex(hInfoMutex);
}

void OBS::SetStreamInfoPriority(UINT infoID, StreamInfoPriority priority)
{
    OSEnterMutex(hInfoMutex);

    for(UINT i=0; i<streamInfoList.Num(); i++)
    {
        if(streamInfoList[i].id == infoID)
        {
            streamInfoList[i].priority = priority;
            break;
        }
    }

    OSLeaveMutex(hInfoMutex);
}

void OBS::RemoveStreamInfo(UINT infoID)
{
    OSEnterMutex(hInfoMutex);

    for(UINT i=0; i<streamInfoList.Num(); i++)
    {
        if(streamInfoList[i].id == infoID)
        {
            streamInfoList[i].FreeData();
            streamInfoList.Remove(i);
            break;
        }
    }

    OSLeaveMutex(hInfoMutex);
}

String OBS::GetMostImportantInfo()
{
    OSEnterMutex(hInfoMutex);

    int bestInfoPriority = -1;
    CTSTR lpBestInfo = NULL;

    for(UINT i=0; i<streamInfoList.Num(); i++)
    {
        if((int)streamInfoList[i].priority > bestInfoPriority)
        {
            lpBestInfo = streamInfoList[i].strInfo;
            bestInfoPriority = streamInfoList[i].priority;
        }
    }

    String strInfo = lpBestInfo;
    OSLeaveMutex(hInfoMutex);

    return strInfo;
}
