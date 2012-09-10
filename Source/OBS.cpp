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


VideoEncoder* CreateX264Encoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, int maxBitRate, int bufferSize);
AudioEncoder* CreateMP3Encoder(UINT bitRate);
AudioEncoder* CreateAACEncoder(UINT bitRate);

AudioSource* CreateAudioSource(bool bMic, CTSTR lpID);

ImageSource* STDCALL CreateDesktopSource(XElement *data);
bool STDCALL ConfigureDesktopSource(XElement *data, bool bCreating);

ImageSource* STDCALL CreateBitmapSource(XElement *data);
bool STDCALL ConfigureBitmapSource(XElement *element, bool bCreating);

ImageSource* STDCALL CreateGlobalSource(XElement *data);

NetworkStream* CreateRTMPServer();
NetworkStream* CreateRTMPPublisher();
NetworkStream* CreateBandwidthAnalyzer();

void StartBlankSoundPlayback();
void StopBlankSoundPlayback();

VideoEncoder* CreateNullVideoEncoder();
AudioEncoder* CreateNullAudioEncoder();
NetworkStream* CreateNullNetwork();

void Convert444to420(LPBYTE input, int width, int height, LPBYTE *output, bool bSSE2Available);

//----------------------------

WNDPROC listboxProc = NULL;

//----------------------------

#define OBS_WINDOW_CLASS      TEXT("OBS")
#define OBS_RENDERFRAME_CLASS TEXT("RenderFrame")

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
const int totalControlAreaHeight = 150;

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
    const int listControlHeight = totalControlAreaHeight - textControlHeight /*- statusHeight*/;

    //-----------------------------------------------------

    ShowWindow(GetDlgItem(hwndMain, ID_SCENES), SW_HIDE);
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
    ShowWindow(GetDlgItem(hwndMain, ID_GLOBALSOURCES), SW_HIDE);
    ShowWindow(GetDlgItem(hwndMain, ID_BANDWIDTHMETER), SW_HIDE);

    //-----------------------------------------------------

    ResizeRenderFrame(bRedrawRenderFrame);

    //-----------------------------------------------------

    DWORD flags = SWP_NOOWNERZORDER|SWP_SHOWWINDOW;

    int xStart = clientWidth/2 - totalControlAreaWidth/2 + (controlPadding/2 + 1);
    int yStart = clientHeight - totalControlAreaHeight;

    int xPos = xStart;
    int yPos = yStart;

    //-----------------------------------------------------

    //SetWindowPos(GetDlgItem(hwndMain, ID_STATUS), NULL, xPos, yPos+listControlHeight, totalWidth-controlPadding, statusHeight, 0);

    //-----------------------------------------------------

    SetWindowPos(GetDlgItem(hwndMain, ID_SCENES_TEXT), NULL, xPos+2, yPos, listControlWidth-controlPadding-2, textControlHeight, flags);
    xPos += listControlWidth;

    SetWindowPos(GetDlgItem(hwndMain, ID_SOURCES_TEXT), NULL, xPos+2, yPos, listControlWidth-controlPadding-2, textControlHeight, flags);
    xPos += listControlWidth;

    yPos += textControlHeight;
    xPos  = xStart;

    //-----------------------------------------------------

    SetWindowPos(GetDlgItem(hwndMain, ID_SCENES), NULL, xPos, yPos, listControlWidth-controlPadding, listControlHeight-controlPadding, flags);
    xPos += listControlWidth;

    SetWindowPos(GetDlgItem(hwndMain, ID_SOURCES), NULL, xPos, yPos, listControlWidth-controlPadding, listControlHeight-controlPadding, flags);
    xPos += listControlWidth;

    //-----------------------------------------------------

    int resetXPos = xPos;
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

    DWORD meterFlags = flags;
    if(!bRunning)
        meterFlags &= ~SWP_SHOWWINDOW;

    SetWindowPos(GetDlgItem(hwndMain, ID_BANDWIDTHMETER), NULL, xPos, yPos, controlWidth-controlPadding, controlHeight, meterFlags);
    xPos += controlWidth;

    SetWindowPos(GetDlgItem(hwndMain, ID_EXIT), NULL, xPos, yPos, controlWidth-controlPadding, controlHeight, flags);
    xPos += controlWidth;

}

Scene* STDCALL CreateNormalScene(XElement *data)
{
    return new Scene;
}


class OBSAPIInterface : public APIInterface
{
    friend class OBS;

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

    virtual bool SetScene(CTSTR lpScene)        {return App->SetScene(lpScene);}
    virtual Scene* GetScene() const             {return App->scene;}

    virtual CTSTR GetSceneName() const          {return App->GetSceneElement()->GetName();}
    virtual XElement* GetSceneElement()         {return App->GetSceneElement();}

    virtual Vect2 GetBaseSize() const           {return Vect2(float(App->baseCX), float(App->baseCY));}
    virtual Vect2 GetRenderFrameSize() const    {return Vect2(float(App->renderFrameWidth), float(App->renderFrameHeight));}
    virtual Vect2 GetOutputSize() const         {return Vect2(float(App->outputCX), float(App->outputCY));}

    virtual CTSTR GetLanguage() const           {return App->strLanguage;}

    virtual HWND GetMainWindow() const          {return hwndMain;}
};


OBS::OBS()
{
    traceIn(OBS::OBS);

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

    InitVolumeControl();
    InitBandwidthMeter();

    //-----------------------------------------------------
    // load locale

    if(!locale->LoadStringFile(TEXT("locale/en.txt")))
        AppWarning(TEXT("Could not open locale string file '%s'"), TEXT("locale/en.txt"));

    strLanguage = AppConfig->GetString(TEXT("General"), TEXT("Language"), TEXT("en"));
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
    RegisterImageSourceClass(TEXT("GlobalSource"), Str("Sources.GlobalSource"), (OBSCREATEPROC)CreateGlobalSource, (OBSCONFIGPROC)OBS::ConfigGlobalSource);

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

    if(!RegisterClass(&wc))
        CrashError(TEXT("Could not register main window class"));

    //-----------------------------------------------------
    // create main window

    int fullscreenX = GetSystemMetrics(SM_CXFULLSCREEN);
    int fullscreenY = GetSystemMetrics(SM_CYFULLSCREEN);

    borderXSize = borderYSize = 0;

    borderXSize += GetSystemMetrics(SM_CXSIZEFRAME)*2;
    borderYSize += GetSystemMetrics(SM_CYSIZEFRAME)*2;
    borderYSize += GetSystemMetrics(SM_CYCAPTION);

    clientWidth  = AppConfig->GetInt(TEXT("General"), TEXT("Width"),  700);
    clientHeight = AppConfig->GetInt(TEXT("General"), TEXT("Height"), 548);

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

    hwndMain = CreateWindowEx(WS_EX_CONTROLPARENT|WS_EX_WINDOWEDGE, OBS_WINDOW_CLASS, OBS_VERSION_STRING,
        WS_OVERLAPPED | WS_THICKFRAME | WS_MINIMIZEBOX | WS_CAPTION | WS_SYSMENU,
        x, y, cx, cy, NULL, NULL, hinstMain, NULL);
    if(!hwndMain)
        CrashError(TEXT("Could not create main window"));

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
        WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|LBS_HASSTRINGS|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,
        0, 0, 0, 0, hwndMain, (HMENU)ID_SCENES, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    listboxProc = (WNDPROC)GetWindowLongPtr(hwndTemp, GWLP_WNDPROC);
    SetWindowLongPtr(hwndTemp, GWLP_WNDPROC, (LONG_PTR)OBS::ListboxHook);

    //-----------------------------------------------------
    // elements listbox

    hwndTemp = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("LISTBOX"), NULL,
        WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|LBS_HASSTRINGS|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|LBS_EXTENDEDSEL,
        0, 0, 0, 0, hwndMain, (HMENU)ID_SOURCES, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    SetWindowLongPtr(hwndTemp, GWLP_WNDPROC, (LONG_PTR)OBS::ListboxHook);

    //-----------------------------------------------------
    // status control

    /*hwndTemp = CreateWindowEx(WS_EX_STATICEDGE, TEXT("EDIT"), NULL,
        WS_CHILDWINDOW|WS_VISIBLE|WS_VSCROLL|ES_READONLY|ES_MULTILINE,
        0, 0, 0, 0, hwndMain, (HMENU)ID_STATUS, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);*/

    //-----------------------------------------------------
    // mic volume control

    hwndTemp = CreateWindow(VOLUME_CONTROL_CLASS, NULL,
        WS_CHILDWINDOW|WS_VISIBLE,
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
        WS_CHILDWINDOW|WS_VISIBLE,
        0, 0, 0, 0, hwndMain, (HMENU)ID_DESKTOPVOLUME, 0, 0);
    SetVolumeControlIcons(hwndTemp, GetIcon(hinstMain, IDI_SOUND_DESKTOP), GetIcon(hinstMain, IDI_SOUND_DESKTOP_MUTED));

    if(!AppConfig->HasKey(TEXT("Audio"), TEXT("DesktopVolume")))
        AppConfig->SetFloat(TEXT("Audio"), TEXT("DesktopVolume"), 1.0f);
    SetVolumeControlValue(hwndTemp, AppConfig->GetFloat(TEXT("Audio"), TEXT("DesktopVolume"), 0.0f));

    //-----------------------------------------------------
    // settings button

    hwndTemp = CreateWindow(TEXT("BUTTON"), Str("Settings"),
        WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|BS_TEXT|BS_PUSHBUTTON,
        0, 0, 0, 0, hwndMain, (HMENU)ID_SETTINGS, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    //-----------------------------------------------------
    // start/stop stream button

    hwndTemp = CreateWindow(TEXT("BUTTON"), Str("MainWindow.StartStream"),
        WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|BS_TEXT|BS_PUSHBUTTON,
        0, 0, 0, 0, hwndMain, (HMENU)ID_STARTSTOP, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    //-----------------------------------------------------
    // edit scene button

    hwndTemp = CreateWindow(TEXT("BUTTON"), Str("MainWindow.SceneEditor"),
        WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|BS_TEXT|BS_AUTOCHECKBOX|BS_PUSHLIKE|WS_DISABLED,
        0, 0, 0, 0, hwndMain, (HMENU)ID_SCENEEDITOR, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    //-----------------------------------------------------
    // global sources button

    hwndTemp = CreateWindow(TEXT("BUTTON"), Str("GlobalSources"),
        WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|BS_TEXT|BS_PUSHBUTTON,
        0, 0, 0, 0, hwndMain, (HMENU)ID_GLOBALSOURCES, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    //-----------------------------------------------------
    // test stream button

    hwndTemp = CreateWindow(TEXT("BUTTON"), Str("MainWindow.TestStream"),
        WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|BS_TEXT|BS_PUSHBUTTON,
        0, 0, 0, 0, hwndMain, (HMENU)ID_TESTSTREAM, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    //-----------------------------------------------------
    // plugins button

    hwndTemp = CreateWindow(TEXT("BUTTON"), Str("MainWindow.Plugins"),
        WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|BS_TEXT|BS_PUSHBUTTON,
        0, 0, 0, 0, hwndMain, (HMENU)ID_PLUGINS, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    //-----------------------------------------------------
    // exit button

    hwndTemp = CreateWindow(TEXT("BUTTON"), Str("MainWindow.Exit"),
        WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|BS_TEXT|BS_PUSHBUTTON,
        0, 0, 0, 0, hwndMain, (HMENU)ID_EXIT, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    //-----------------------------------------------------
    // bandwidth meter

    hwndTemp = CreateWindow(BANDWIDTH_METER_CLASS, NULL,
        WS_CHILDWINDOW|WS_TABSTOP|BS_TEXT|BS_PUSHBUTTON,
        0, 0, 0, 0, hwndMain, (HMENU)ID_BANDWIDTHMETER, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    //-----------------------------------------------------
    // scenes text

    hwndTemp = CreateWindow(TEXT("STATIC"), Str("MainWindow.Scenes"),
        WS_CHILDWINDOW|WS_VISIBLE,
        0, 0, 0, 0, hwndMain, (HMENU)ID_SCENES_TEXT, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    //-----------------------------------------------------
    // scenes text

    hwndTemp = CreateWindow(TEXT("STATIC"), Str("MainWindow.Sources"),
        WS_CHILDWINDOW|WS_VISIBLE,
        0, 0, 0, 0, hwndMain, (HMENU)ID_SOURCES_TEXT, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    //-----------------------------------------------------
    // populate scenes

    hwndTemp = GetDlgItem(hwndMain, ID_SCENES);

    if(!scenesConfig.Open(TEXT("scenes.xconfig")))
        CrashError(TEXT("Could not open scenes.xconfig"));

    XElement *scenes = scenesConfig.GetElement(TEXT("scenes"));
    if(!scenes)
        scenes = scenesConfig.CreateElement(TEXT("scenes"));

    UINT numScenes = scenes->NumElements();
    for(UINT i=0; i<numScenes; i++)
    {
        XElement *scene = scenes->GetElementByID(i);
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

    API = new OBSAPIInterface;

    ResizeWindow(false);
    ShowWindow(hwndMain, SW_SHOW);

    LPBYTE chi = LPBYTE(UPARAM(54));

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
                    if(!loadPlugin || loadPlugin())
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

    traceOut;
}


OBS::~OBS()
{
    traceIn(OBS::~OBS);

    Stop();

    for(UINT i=0; i<plugins.Num(); i++)
    {
        PluginInfo &pluginInfo = plugins[i];

        UNLOADPLUGINPROC unloadPlugin = (UNLOADPLUGINPROC)GetProcAddress(pluginInfo.hModule, "UnloadPlugin");
        if(unloadPlugin)
            unloadPlugin();

        FreeLibrary(pluginInfo.hModule);
        pluginInfo.strFile.Clear();
    }

    DestroyWindow(hwndMain);

    AppConfig->SetInt(TEXT("General"), TEXT("Width"),  clientWidth);
    AppConfig->SetInt(TEXT("General"), TEXT("Height"), clientHeight);

    scenesConfig.Close(true);

    for(UINT i=0; i<Icons.Num(); i++)
        DeleteObject(Icons[i].hIcon);
    Icons.Clear();

    for(UINT i=0; i<sceneClasses.Num(); i++)
        sceneClasses[i].FreeData();
    for(UINT i=0; i<imageSourceClasses.Num(); i++)
        imageSourceClasses[i].FreeData();

    OSCloseMutex(hSceneMutex);

    delete API;
    API = NULL;

    traceOut;
}

void OBS::ToggleCapturing()
{
    traceIn(OBS::ToggleCapturing);

    if(!bRunning)
        Start();
    else
        Stop();

    traceOut;
}

HICON OBS::GetIcon(HINSTANCE hInst, int resource)
{
    for(UINT i=0; i<Icons.Num(); i++)
    {
        if(Icons[i].resource == resource && Icons[i].hInst == hInst)
            return Icons[i].hIcon;
    }

    IconInfo ii;
    ii.hInst = hInst;
    ii.resource = resource;
    ii.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(resource));

    Icons << ii;

    return ii.hIcon;
}

ID3D10Blob* CompileShader(CTSTR lpShader, LPCSTR lpTarget)
{
    traceIn(CompileShader);

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

    traceOut;
}

void OBS::Start()
{
    traceIn(OBS::Start);

    if(bRunning) return;

    //-------------------------------------------------------------

    fps = AppConfig->GetInt(TEXT("Video"), TEXT("FPS"), 25);
    frameTime = 1000/fps;

    //-------------------------------------------------------------

    int networkMode = AppConfig->GetInt(TEXT("Publish"), TEXT("Mode"), 2);

    if(bTestStream)
        network = CreateBandwidthAnalyzer();
    else
    {
        switch(networkMode)
        {
            case 0: network = CreateRTMPPublisher(); break;
            case 1: network = CreateRTMPServer(); break;
        }
    }

    if(!network)
        return;

    //-------------------------------------------------------------

    Log(TEXT("=====Stream Start====================================================================="));

    //-------------------------------------------------------------

    int monitorID = AppConfig->GetInt(TEXT("Video"), TEXT("Monitor"));
    RECT &screenRect = monitors[monitorID].rect;
    int defCX = screenRect.right  - screenRect.left;
    int defCY = screenRect.bottom - screenRect.top;

    downscale = AppConfig->GetFloat(TEXT("Video"), TEXT("Downscale"), 1.0f);
    baseCX = AppConfig->GetInt(TEXT("Video"), TEXT("BaseWidth"),  defCX);
    baseCY = AppConfig->GetInt(TEXT("Video"), TEXT("BaseHeight"), defCY);

    baseCX = MIN(MAX(baseCX, 128), 4096);
    baseCY = MIN(MAX(baseCY, 128), 4096);

    //align width to 128bit for fast SSE YUV4:2:0 conversion
    outputCX = int(double(baseCX) / double(downscale)) & 0xFFFFFFFC;
    outputCY = int(double(baseCY) / double(downscale)) & 0xFFFFFFFC;

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

    HRESULT err = GetD3D()->CreateTexture2D(&td, NULL, &copyTexture);
    if(FAILED(err))
    {
        CrashError(TEXT("Unable to create copy texture"));
        //todo - better error handling
    }

    //-------------------------------------------------------------

    desktopAudio = CreateAudioSource(false, NULL);

    AudioDeviceList audioDevices;
    GetAudioDevices(audioDevices);

    String strDevice = AppConfig->GetString(TEXT("Audio"), TEXT("Device"), NULL);
    if(strDevice.IsEmpty() || !audioDevices.HasID(strDevice))
    {
        AppConfig->SetString(TEXT("Audio"), TEXT("Device"), TEXT("Disable"));
        strDevice = TEXT("Disable");
    }

    audioDevices.FreeData();

    if(strDevice.CompareI(TEXT("Disable")))
        EnableWindow(GetDlgItem(hwndMain, ID_MICVOLUME), FALSE);
    else
    {
        EnableWindow(GetDlgItem(hwndMain, ID_MICVOLUME), TRUE);

        if(!strDevice.CompareI(TEXT("Default")) || GetDefaultMicID(strDevice))
            micAudio = CreateAudioSource(true, strDevice);
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

    hRequestAudioEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    hSoundDataMutex = OSCreateMutex();
    hSoundThread = OSCreateThread((XTHREAD)OBS::MainAudioThread, NULL);

    //-------------------------------------------------------------

    int maxBitRate = AppConfig->GetInt   (TEXT("Video Encoding"), TEXT("MaxBitrate"), 1000);
    int bufferSize = AppConfig->GetInt   (TEXT("Video Encoding"), TEXT("BufferSize"), 1000);
    int quality    = AppConfig->GetInt   (TEXT("Video Encoding"), TEXT("Quality"),    8);
    String preset  = AppConfig->GetString(TEXT("Video Encoding"), TEXT("Preset"),     TEXT("veryfast"));
    bUsing444      = AppConfig->GetInt   (TEXT("Video Encoding"), TEXT("Use444"),     0) != 0;

    //-------------------------------------------------------------

    StartBlankSoundPlayback();

    //-------------------------------------------------------------

    videoEncoder = CreateX264Encoder(fps, outputCX, outputCY, quality, preset, bUsing444, maxBitRate, bufferSize);

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
    ShowWindow(GetDlgItem(hwndMain, ID_BANDWIDTHMETER), SW_SHOW);

    //-------------------------------------------------------------

    traceOut;
}

void OBS::Stop()
{
    traceIn(OBS::Stop);

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
        SetEvent(hRequestAudioEvent);
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

    delete network;

    delete micAudio;
    delete desktopAudio;

    delete audioEncoder;

    delete videoEncoder;

    network = NULL;
    micAudio = NULL;
    desktopAudio = NULL;
    audioEncoder = NULL;
    videoEncoder = NULL;

    //-------------------------------------------------------------

    StopBlankSoundPlayback();

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

    SafeRelease(copyTexture);

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
    ShowWindow(GetDlgItem(hwndMain, ID_BANDWIDTHMETER), SW_HIDE);

    bTestStream = false;

    traceOut;
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

void OBS::MainCaptureLoop()
{
    traceIn(OBS::MainCaptureLoop);

    int curRenderTarget = 0, curCopyTexture = 0;
    int copyWait = NUM_RENDER_BUFFERS-1;
    UINT curStreamTime = 0, firstFrameTime = OSGetTime(), lastStreamTime = 0;
    UINT lastTime = 0;

    List<UINT> bufferedTimes;

    bool bFirstFrame = true;

    //firstFrameTime -= UINT(frameTime);

    Vect2 baseSize        = Vect2(float(baseCX), float(baseCY));
    Vect2 outputSize      = Vect2(float(outputCX), float(outputCY));

    int numLongFrames = 0;
    int numTotalFrames = 0;

    LPVOID nullBuff = NULL;

    x264_picture_t picOut;
    x264_picture_init(&picOut);

    if(bUsing444)
    {
        picOut.img.i_csp   = X264_CSP_BGRA; //although the x264 input says BGR, x264 actually will expect packed UYV
        picOut.img.i_plane = 1;
    }
    else
        x264_picture_alloc(&picOut, X264_CSP_I420, outputCX, outputCY);

    int curPTS = 0;
    //int audioJump = 0;

    HANDLE hScaleVal = yuvScalePixelShader->GetParameterByName(TEXT("baseDimensionI"));

    desktopAudio->StartCapture();
    if(micAudio) micAudio->StartCapture();

    float timePassedThingy = 0.0f;

    while(bRunning)
    {
        DWORD renderStartTime = OSGetTime();

        bool bRenderView = !IsIconic(hwndMain);

        profileIn("frame");

        curStreamTime = renderStartTime-firstFrameTime;
        bufferedTimes << curStreamTime;

        float fSeconds = float(curStreamTime-lastStreamTime)*0.001f;

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

        SetBandwidthMeterValue(GetDlgItem(hwndMain, ID_BANDWIDTHMETER), network->GetBytesPerSec(), network->GetPacketStrain());

        //timePassedThingy += fSeconds;

        //if(timePassedThingy > 0.0f)
        //{
        //    Log(TEXT("bytes per sec %u, strain %g"), network->GetBytesPerSec(), network->GetPacketStrain());
        //    timePassedThingy -= 1.0f;
        //}

        EnableBlending(TRUE);
        BlendFunction(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);

        //------------------------------------
        // render the mini render texture

        LoadVertexShader(mainVertexShader);
        LoadPixelShader(mainPixelShader);

        SetRenderTarget(mainRenderTextures[curRenderTarget]);

        Ortho(0.0f, baseSize.x, baseSize.y, 0.0f, -1.0f, 1000.0f);
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

            Ortho(0.0f, renderFrameSize.x, renderFrameSize.y, 0.0f, -1.0f, 1000.0f);
            SetViewport(0.0f, 0.0f, renderFrameSize.x, renderFrameSize.y);

            if(bTransitioning)
            {
                BlendFunction(GS_BLEND_ONE, GS_BLEND_ZERO);
                DrawSprite(transitionTexture, 0.0f, 0.0f, renderFrameSize.x, renderFrameSize.y);
                BlendFunction(GS_BLEND_FACTOR, GS_BLEND_INVFACTOR, transitionAlpha);
            }

            DrawSprite(mainRenderTextures[curRenderTarget], 0.0f, 0.0f, renderFrameSize.x, renderFrameSize.y);

            Ortho(0.0f, renderFrameSize.x, renderFrameSize.y, 0.0f, -1.0f, 1000.0f);

            LoadVertexShader(solidVertexShader);
            LoadPixelShader(solidPixelShader);
            solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0xFFFF0000);

            //draw selections if in edit mode
            if(bEditMode && !bSizeChanging)
            {
                Ortho(0.0f, baseSize.x, baseSize.y, 0.0f, -1.0f, 1000.0f);

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

        Ortho(0.0f, outputSize.x, outputSize.y, 0.0f, -1.0f, 1000.0f);
        SetViewport(0.0f, 0.0f, outputSize.x, outputSize.y);

        if(bTransitioning)
        {
            BlendFunction(GS_BLEND_ONE, GS_BLEND_ZERO);
            DrawSpriteEx(transitionTexture, 0.0f, 0.0f, outputSize.x, outputSize.y, 0.0f, 0.0f, outputSize.x, outputSize.y);
            BlendFunction(GS_BLEND_FACTOR, GS_BLEND_INVFACTOR, transitionAlpha);
        }

        DrawSpriteEx(mainRenderTextures[curRenderTarget], 0.0f, 0.0f, outputSize.x, outputSize.y, 0.0f, 0.0f, outputSize.x, outputSize.y);

        //------------------------------------

        if(!static_cast<D3D10System*>(GS)->swap)
            Log(TEXT("swap is bad?"));

        if(bRenderView && !copyWait)
            static_cast<D3D10System*>(GS)->swap->Present(0, 0);

        OSLeaveMutex(hSceneMutex);

        //------------------------------------
        // present/upload

        profileIn("video encoding and uploading");

        if(copyWait)
            copyWait--;
        else
        {
            profileIn("CopyResource");
            D3D10Texture *d3dYUV = static_cast<D3D10Texture*>(yuvRenderTextures[curCopyTexture]);
            GetD3D()->CopyResource(copyTexture, d3dYUV->texture);
            profileOut;

            D3D10_MAPPED_TEXTURE2D map;
            if(SUCCEEDED(copyTexture->Map(0, D3D10_MAP_READ, 0, &map)))
            {
                List<DataPacket> videoPackets;
                List<PacketType> videoPacketTypes;

                if(!bUsing444)
                {
                    profileIn("conversion to 4:2:0");

                    Convert444to420((LPBYTE)map.pData, outputCX, outputCY, picOut.img.plane, SSE2Available());
                    copyTexture->Unmap(0);

                    profileOut;
                }
                else
                {
                    picOut.img.i_stride[0] = map.RowPitch;
                    picOut.img.plane[0]    = (uint8_t*)map.pData;
                }

                picOut.i_pts = bufferedTimes[curPTS++];//curPTS++;//bufferedTimes[curPTS++]*90;//

                videoEncoder->Encode(&picOut, videoPackets, videoPacketTypes, bufferedTimes[0]);
                if(bUsing444) copyTexture->Unmap(0);

                //DWORD relativeTimestamp = bufferedTimes[0]-lastTime;

                //------------------------------------
                // upload

                bool bSentVideo = false;

                //send headers before the first frame if not yet sent
                if(bFirstFrame && videoPackets.Num())
                {
                    network->BeginPublishing();
                    bFirstFrame = false;
                }

                for(UINT i=0; i<videoPackets.Num(); i++)
                {
                    DataPacket &packet  = videoPackets[i];
                    PacketType type     = videoPacketTypes[i];

                    network->SendPacket(packet.lpPacket, packet.size, bufferedTimes[0], type);

                    //Log(TEXT("video packet timestamp: %u, buffered timestamp: %u"), outputTimestamp, bufferedTimes[0]);
                    bSentVideo = true;
                }

                if(bSentVideo)
                {
                    //Log(TEXT("bytes per sec %u, strain %g"), network->GetBytesPerSec(), network->GetPacketStrain());

                    //Log(TEXT("What the first timestamp is: %d ...what it normally would be: %d"), outputTimestamp, bufferedTimes[0]);

                    OSEnterMutex(hSoundDataMutex);

                    if(pendingAudioFrames.Num())
                    {
                        while(pendingAudioFrames.Num() && pendingAudioFrames[0].timestamp < bufferedTimes[0])
                        {
                            List<BYTE> &audioData = pendingAudioFrames[0].audioData;

                            if(audioData.Num())
                            {
                                network->SendPacket(audioData.Array(), audioData.Num(), pendingAudioFrames[0].timestamp, PacketType_Audio);
                                audioData.Clear();
                            }

                            //Log(TEXT("audio packet timestamp: %u"), pendingAudioFrames[0].timestamp);

                            pendingAudioFrames[0].audioData.Clear();
                            pendingAudioFrames.Remove(0);
                        }
                    }

                    OSLeaveMutex(hSoundDataMutex);

                    lastTime = bufferedTimes[0];

                    curPTS--;
                    bufferedTimes.Remove(0);
                }
            }

            if(curCopyTexture == (NUM_RENDER_BUFFERS-1))
                curCopyTexture = 0;
            else
                curCopyTexture++;
        }

        lastRenderTarget = curRenderTarget;

        if(curRenderTarget == (NUM_RENDER_BUFFERS-1))
            curRenderTarget = 0;
        else
            curRenderTarget++;

        profileOut;
        profileOut;

        //------------------------------------
        // get audio while sleeping or capturing
        SetEvent(hRequestAudioEvent);

        //------------------------------------
        // frame sync

        DWORD renderStopTime = OSGetTime();
        DWORD totalTime = renderStopTime-renderStartTime;

        //OSDebugOut(TEXT("Total frame time: %d\r\n"), totalTime);
        if(totalTime > frameTime)
            numLongFrames++;

        numTotalFrames++;

        if(totalTime < frameTime)
            OSSleep(frameTime-totalTime);
    }

    if(!bUsing444)
        x264_picture_clean(&picOut);

    Log(TEXT("Total frames rendered: %d, number of frames that lagged: %d (%0.2f%%) (it's okay for some frames to lag)"), numTotalFrames, numLongFrames, (double(numLongFrames)/double(numTotalFrames))*100.0);

    traceOutStop;
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
    traceIn(OBS::MainAudioLoop);

    UINT curAudioFrame = 0;

    while(TRUE)
    {
        WaitForSingleObject(hRequestAudioEvent, INFINITE);
        if(!bRunning)
            break;

        //-----------------------------------------------

        OSEnterMutex(hSoundDataMutex);

        //-----------------------------------------------

        float *desktopBuffer, *micBuffer;
        UINT desktopAudioFrames, micAudioFrames;

        bool bDesktopMuted = (desktopVol < EPSILON);
        bool bMicMuted     = (micAudio == NULL);// || (micVol < EPSILON);

        while(QueryNewAudio())
        {
            desktopAudio->GetBuffer(&desktopBuffer, &desktopAudioFrames);

            if(micAudio != NULL)
                micAudio->GetBuffer(&micBuffer, &micAudioFrames);

            UINT totalFloats = desktopAudioFrames*2;

            MultiplyAudioBuffer(desktopBuffer, totalFloats, desktopVol);
            if(!bMicMuted)
                MultiplyAudioBuffer(micBuffer, totalFloats, micVol);

            //-----------------
            // mix mic and desktop sound, using SSE2 if available
            // also, it's perfectly fine to just mix into the returned buffer
            if(bDesktopMuted)
            {
                desktopBuffer = micBuffer;
                desktopAudioFrames = micAudioFrames;
            }
            else if(!bMicMuted)
            {
                UINT floatsLeft    = totalFloats;
                float *desktopTemp = desktopBuffer;
                float *micTemp     = micBuffer;

                if(SSE2Available())
                {
                    UINT alignedFloats = floatsLeft & 0xFFFFFFFC;

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
            if(audioEncoder->Encode(desktopBuffer, totalFloats>>1, packet))
            {
                FrameAudio *frameAudio = pendingAudioFrames.CreateNew();
                frameAudio->audioData.CopyArray(packet.lpPacket, packet.size);
                frameAudio->timestamp = DWORD(double(curAudioFrame)*double(GetAudioEncoder()->GetFrameSize())/44.1);
                curAudioFrame++;
            }
        }

        //-----------------------------------------------

        OSLeaveMutex(hSoundDataMutex);
    }

    for(UINT i=0; i<pendingAudioFrames.Num(); i++)
        pendingAudioFrames[i].audioData.Clear();

    traceOutStop;
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
