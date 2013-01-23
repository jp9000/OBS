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
#include <Avrt.h>
#include <intrin.h>


//primarily main window stuff an initialization/destruction code


typedef bool (*LOADPLUGINPROC)();
typedef void (*UNLOADPLUGINPROC)();

ImageSource* STDCALL CreateDesktopSource(XElement *data);
bool STDCALL ConfigureDesktopSource(XElement *data, bool bCreating);

ImageSource* STDCALL CreateBitmapSource(XElement *data);
bool STDCALL ConfigureBitmapSource(XElement *element, bool bCreating);

ImageSource* STDCALL CreateBitmapTransitionSource(XElement *data);
bool STDCALL ConfigureBitmapTransitionSource(XElement *element, bool bCreating);

ImageSource* STDCALL CreateTextSource(XElement *data);
bool STDCALL ConfigureTextSource(XElement *element, bool bCreating);

ImageSource* STDCALL CreateGlobalSource(XElement *data);

void STDCALL SceneHotkey(DWORD hotkey, UPARAM param, bool bDown);

APIInterface* CreateOBSApiInterface();


#define QuickClearHotkey(hotkeyID) \
    if(hotkeyID) \
{ \
    API->DeleteHotkey(hotkeyID); \
    hotkeyID = NULL; \
}

//----------------------------

WNDPROC listboxProc = NULL;
WNDPROC listviewProc = NULL;

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
const int miscAreaWidth = 290;
const int totalControlAreaHeight = 171;//170;//
const int listAreaWidth = totalControlAreaWidth-miscAreaWidth;
const int controlWidth = miscAreaWidth/2;
const int controlHeight = 22;
const int volControlHeight = 32;
const int volMeterHeight = 10;
const int textControlHeight = 16;
const int listControlWidth = listAreaWidth/2;

Scene* STDCALL CreateNormalScene(XElement *data)
{
    return new Scene;
}


OBS::OBS()
{
    App = this;

    __cpuid(cpuInfo, 1);

    bSSE2Available = (cpuInfo[3] & (1<<26)) != 0;

    hSceneMutex = OSCreateMutex();
    hAuxAudioMutex = OSCreateMutex();

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
    InitVolumeMeter();

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
    // elements listview

    hwndTemp = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
        WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|WS_CLIPSIBLINGS|LVS_REPORT|LVS_NOCOLUMNHEADER|
        LVS_SHOWSELALWAYS | LVS_ALIGNLEFT | LVS_NOLABELWRAP,
        0, 0, 0, 0, hwndMain, (HMENU)ID_SOURCES, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    ListView_SetExtendedListViewStyle(hwndTemp, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    
    //add single column needed for report style
    LVCOLUMN column;    
    column.mask = LVCF_TEXT;
    column.fmt = LVCFMT_FIXED_WIDTH;
    column.cx = 0;
    column.pszText = TEXT("");

    ListView_InsertColumn(hwndTemp, 0, &column);
    ListView_InsertColumn(hwndTemp, 1, &column);

    listviewProc = (WNDPROC)GetWindowLongPtr(hwndTemp, GWLP_WNDPROC);
    SetWindowLongPtr(hwndTemp, GWLP_WNDPROC, (LONG_PTR)OBS::ListboxHook);

    HWND hwndSources = hwndTemp;

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


    //-----------------------------------------------------
    // mic volume meter

    hwndTemp = CreateWindow(VOLUME_METER_CLASS, NULL,
                            WS_CHILDWINDOW|WS_VISIBLE|WS_CLIPSIBLINGS,
                            0, 0, 0, 0, hwndMain, (HMENU)ID_MICVOLUMEMETER, 0, 0);

    //-----------------------------------------------------
    // desktop volume meter

    hwndTemp = CreateWindow(VOLUME_METER_CLASS, NULL,
                            WS_CHILDWINDOW|WS_VISIBLE|WS_CLIPSIBLINGS,
                            0, 0, 0, 0, hwndMain, (HMENU)ID_DESKTOPVOLUMEMETER, 0, 0);

    //-----------------------------------------------------
    // desktop volume control

    hwndTemp = CreateWindow(VOLUME_CONTROL_CLASS, NULL,
        WS_CHILDWINDOW|WS_VISIBLE|WS_CLIPSIBLINGS,
        0, 0, 0, 0, hwndMain, (HMENU)ID_DESKTOPVOLUME, 0, 0);
    SetVolumeControlIcons(hwndTemp, GetIcon(hinstMain, IDI_SOUND_DESKTOP), GetIcon(hinstMain, IDI_SOUND_DESKTOP_MUTED));

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

    API = CreateOBSApiInterface();

    ResizeWindow(false);
    ShowWindow(hwndMain, SW_SHOW);

    // make sure sources listview column widths are as expected after obs window is shown

    ListView_SetColumnWidth(hwndSources,0,LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(hwndSources,1,LVSCW_AUTOSIZE_USEHEADER);

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

    ReloadIniSettings();

    //-----------------------------------------------------

    bAutoReconnect = AppConfig->GetInt(TEXT("Publish"), TEXT("AutoReconnect"), 1) != 0;
    reconnectTimeout = AppConfig->GetInt(TEXT("Publish"), TEXT("AutoReconnectTimeout"), 10);
    if(reconnectTimeout < 5)
        reconnectTimeout = 5;

    hHotkeyThread = OSCreateThread((XTHREAD)HotkeyThread, NULL);

#ifndef OBS_DISABLE_AUTOUPDATE
    OSCloseThread(OSCreateThread((XTHREAD)CheckUpdateThread, NULL));
#endif

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

    if(hAuxAudioMutex)
        OSCloseMutex(hAuxAudioMutex);

    delete API;
    API = NULL;

    if(hInfoMutex)
        OSCloseMutex(hInfoMutex);
    if(hHotkeyMutex)
        OSCloseMutex(hHotkeyMutex);
}

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
    parts[1] = parts[2]-170;
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

    SetWindowPos(GetDlgItem(hwndMain, ID_MICVOLUMEMETER), NULL, xPos, yPos, controlWidth-controlPadding, volMeterHeight, flags);
    xPos += controlWidth;

    SetWindowPos(GetDlgItem(hwndMain, ID_DESKTOPVOLUMEMETER), NULL, xPos, yPos, controlWidth-controlPadding, volMeterHeight, flags);
    xPos += controlWidth;

    yPos += volMeterHeight+controlPadding;

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

void OBS::ReloadIniSettings()
{
    HWND hwndTemp;

    //-------------------------------------------
    // mic volume data
    hwndTemp = GetDlgItem(hwndMain, ID_MICVOLUME);

    if(!AppConfig->HasKey(TEXT("Audio"), TEXT("MicVolume")))
        AppConfig->SetFloat(TEXT("Audio"), TEXT("MicVolume"), 0.0f);
    SetVolumeControlValue(hwndTemp, AppConfig->GetFloat(TEXT("Audio"), TEXT("MicVolume"), 0.0f));

    AudioDeviceList audioDevices;
    GetAudioDevices(audioDevices, ADT_RECORDING);

    String strDevice = AppConfig->GetString(TEXT("Audio"), TEXT("Device"), NULL);
    if(strDevice.IsEmpty() || !audioDevices.HasID(strDevice))
    {
        AppConfig->SetString(TEXT("Audio"), TEXT("Device"), TEXT("Disable"));
        strDevice = TEXT("Disable");
    }

    audioDevices.FreeData();

    EnableWindow(hwndTemp, !strDevice.CompareI(TEXT("Disable")));

    //-------------------------------------------
    // desktop volume
    hwndTemp = GetDlgItem(hwndMain, ID_DESKTOPVOLUME);

    if(!AppConfig->HasKey(TEXT("Audio"), TEXT("DesktopVolume")))
        AppConfig->SetFloat(TEXT("Audio"), TEXT("DesktopVolume"), 1.0f);
    SetVolumeControlValue(hwndTemp, AppConfig->GetFloat(TEXT("Audio"), TEXT("DesktopVolume"), 0.0f));

    //-------------------------------------------
    // mic boost
    DWORD micBoostPercentage = AppConfig->GetInt(TEXT("Audio"), TEXT("MicBoostMultiple"), 1);
    if(micBoostPercentage < 1)
        micBoostPercentage = 1;
    else if(micBoostPercentage > 20)
        micBoostPercentage = 20;
    micBoost = float(micBoostPercentage);

    //-------------------------------------------
    // dashboard
    strDashboard = AppConfig->GetString(TEXT("Publish"), TEXT("Dashboard"));
    strDashboard.KillSpaces();

    //-------------------------------------------
    // hotkeys
    QuickClearHotkey(pushToTalkHotkeyID);
    QuickClearHotkey(muteMicHotkeyID);
    QuickClearHotkey(muteDesktopHotkeyID);
    QuickClearHotkey(stopStreamHotkeyID);
    QuickClearHotkey(startStreamHotkeyID);

    bUsingPushToTalk = AppConfig->GetInt(TEXT("Audio"), TEXT("UsePushToTalk")) != 0;
    DWORD hotkey = AppConfig->GetInt(TEXT("Audio"), TEXT("PushToTalkHotkey"));
    pushToTalkDelay = AppConfig->GetInt(TEXT("Audio"), TEXT("PushToTalkDelay"), 200);

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
}


void OBS::UpdateAudioMeters()
{
    SetVolumeMeterValue(GetDlgItem(hwndMain, ID_DESKTOPVOLUMEMETER), desktopMag, desktopMax, desktopPeak);
    SetVolumeMeterValue(GetDlgItem(hwndMain, ID_MICVOLUMEMETER), micMag, micMax, micPeak);
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
        strKBPS << IntString((statusBarData.bytesPerSec*8) / 1000) << TEXT("kb/s");
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
            case 2:
                {
                    double percentageDropped = 0.0;
                    if(App->network)
                    {
                        UINT numTotalFrames = App->network->NumTotalVideoFrames();
                        if(numTotalFrames)
                            percentageDropped = (double(App->network->NumDroppedFrames())/double(numTotalFrames))*100.0;
                    }
                    strOutString << Str("MainWindow.DroppedFrames") << FormattedString(TEXT(" %u (%0.2f%%)"), App->curFramesDropped, percentageDropped);
                }
                break;
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

void OBS::SelectSources()
{
    if(scene)
        scene->DeselectAll();

    HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);
    UINT numSelected = ListView_GetSelectedCount(hwndSources);

    if(numSelected)
    {
        List<UINT> selectedItems;
        selectedItems.SetSize(numSelected);
        //SendMessage(hwndSources, LB_GETSELITEMS, numSelected, (LPARAM)selectedItems.Array());

        if(scene)
        {
            int iPos = ListView_GetNextItem(hwndSources, -1, LVNI_SELECTED);
            while (iPos != -1)
            {
                SceneItem *sceneItem = scene->GetSceneItem(iPos);
                sceneItem->bSelected = true;
                
                iPos = ListView_GetNextItem(hwndSources, iPos, LVNI_SELECTED);
            }
        }
    }
}

void OBS::CheckSources()
{
    XElement *curSceneElement = App->sceneElement;
    XElement *sources = curSceneElement->GetElement(TEXT("sources"));

    if(!sources)
        return;

    HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);

    UINT numSources = ListView_GetItemCount(hwndSources);
    for(UINT i = 0; i < numSources; i++)
    {
        bool checked = ListView_GetCheckState(hwndSources, i) > 0;
        XElement *source =sources->GetElementByID(i);
        source->SetInt(TEXT("render"), (checked)?1:0);
        if(scene && i < scene->NumSceneItems())
        {
            SceneItem *sceneItem = scene->GetSceneItem(i);
            sceneItem->bRender = checked;
            sceneItem->SetRender(checked);
        }
    }
}

