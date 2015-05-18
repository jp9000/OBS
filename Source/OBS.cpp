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

void SetupSceneCollection(CTSTR scenecollection);

//primarily main window stuff an initialization/destruction code

typedef bool (*LOADPLUGINPROC)();
typedef bool (*LOADPLUGINEXPROC)(UINT);
typedef void (*UNLOADPLUGINPROC)();
typedef CTSTR (*GETPLUGINNAMEPROC)();

ImageSource* STDCALL CreateDesktopSource(XElement *data);
bool STDCALL ConfigureDesktopSource(XElement *data, bool bCreating);
bool STDCALL ConfigureWindowCaptureSource(XElement *data, bool bCreating);
bool STDCALL ConfigureMonitorCaptureSource(XElement *data, bool bCreating);

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

const int totalControlAreaWidth  = minClientWidth - 10;
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

BOOL IsWebrootLoaded()
{
    BOOL ret = FALSE;
    StringList moduleList;

    OSGetLoadedModuleList (GetCurrentProcess(), moduleList);

    HMODULE msIMG = GetModuleHandle(TEXT("MSIMG32"));
    if (msIMG)
    {
        FARPROC alphaBlend = GetProcAddress(msIMG, "AlphaBlend");
        if (alphaBlend)
        {
            if (!IsBadReadPtr(alphaBlend, 5))
            {
                BYTE opCode = *(BYTE *)alphaBlend;

                if (opCode == 0xE9)
                {
                    if (moduleList.HasValue(TEXT("wrusr.dll")))
                        ret = TRUE;
                }
            }
        }
    }

    return ret;
}



//---------------------------------------------------------------------------


OBS::OBS()
{
    App = this;

    performTransition = true;        //Default to true and don't set the conf. 
                                     //We don't want to let plugins disable standard behavior permanently.
    hSceneMutex = OSCreateMutex();
    hAuxAudioMutex = OSCreateMutex();
    hVideoEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    monitors.Clear();
    EnumDisplayMonitors(NULL, NULL, (MONITORENUMPROC)MonitorInfoEnumProc, (LPARAM)&monitors);

    INITCOMMONCONTROLSEX ecce;
    ecce.dwSize = sizeof(ecce);
    ecce.dwICC = ICC_STANDARD_CLASSES;
    if(!InitCommonControlsEx(&ecce))
        CrashError(TEXT("Could not initalize common shell controls"));

    InitHotkeyExControl(hinstMain);
    InitColorControl(hinstMain);
    InitVolumeControl(hinstMain);
    InitVolumeMeter(hinstMain);

    // still need this here for API
    strLanguage = GlobalConfig->GetString(TEXT("General"), TEXT("Language"), TEXT("en"));

    //-----------------------------------------------------
    // load classes

    RegisterSceneClass(TEXT("Scene"), Str("Scene"), (OBSCREATEPROC)CreateNormalScene, NULL, false);
    RegisterImageSourceClass(TEXT("DesktopImageSource"), Str("Sources.SoftwareCaptureSource"), (OBSCREATEPROC)CreateDesktopSource, (OBSCONFIGPROC)ConfigureDesktopSource, true);
    RegisterImageSourceClass(TEXT("WindowCaptureSource"), Str("Sources.SoftwareCaptureSource.WindowCapture"), (OBSCREATEPROC)CreateDesktopSource, (OBSCONFIGPROC)ConfigureWindowCaptureSource, false);
    RegisterImageSourceClass(TEXT("MonitorCaptureSource"), Str("Sources.SoftwareCaptureSource.MonitorCapture"), (OBSCREATEPROC)CreateDesktopSource, (OBSCONFIGPROC)ConfigureMonitorCaptureSource, false);
    RegisterImageSourceClass(TEXT("BitmapImageSource"), Str("Sources.BitmapSource"), (OBSCREATEPROC)CreateBitmapSource, (OBSCONFIGPROC)ConfigureBitmapSource, false);
    RegisterImageSourceClass(TEXT("BitmapTransitionSource"), Str("Sources.TransitionSource"), (OBSCREATEPROC)CreateBitmapTransitionSource, (OBSCONFIGPROC)ConfigureBitmapTransitionSource, false);
    RegisterImageSourceClass(TEXT("GlobalSource"), Str("Sources.GlobalSource"), (OBSCREATEPROC)CreateGlobalSource, (OBSCONFIGPROC)OBS::ConfigGlobalSource, false);

    RegisterImageSourceClass(TEXT("TextSource"), Str("Sources.TextSource"), (OBSCREATEPROC)CreateTextSource, (OBSCONFIGPROC)ConfigureTextSource, false);

    //-----------------------------------------------------
    // render frame class
    WNDCLASS wc;
    zero(&wc, sizeof(wc));
    wc.hInstance = hinstMain;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    wc.lpszClassName = OBS_RENDERFRAME_CLASS;
    wc.lpfnWndProc = (WNDPROC)OBS::RenderFrameProc;
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);

    if(!RegisterClass(&wc))
        CrashError(TEXT("Could not register render frame class"));

    //-----------------------------------------------------
    // projector frame class
    wc.lpszClassName = OBS_PROJECTORFRAME_CLASS;
    wc.lpfnWndProc = (WNDPROC)OBS::ProjectorFrameProc;
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);

    if(!RegisterClass(&wc))
        CrashError(TEXT("Could not register projector frame class"));
    
    //-----------------------------------------------------
    // log window class
    wc.lpszClassName = OBS_LOGWINDOW_CLASS;
    wc.lpfnWndProc = (WNDPROC)OBS::LogWindowProc;
    wc.hIcon = LoadIcon(hinstMain, MAKEINTRESOURCE(IDI_ICON1));
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;

    if(!RegisterClass(&wc))
        CrashError(TEXT("Could not register main window class"));

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

    clientWidth  = GlobalConfig->GetInt(TEXT("General"), TEXT("Width"),  defaultClientWidth);
    clientHeight = GlobalConfig->GetInt(TEXT("General"), TEXT("Height"), defaultClientHeight);

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

    int posX = GlobalConfig->GetInt(TEXT("General"), TEXT("PosX"), -9999);
    int posY = GlobalConfig->GetInt(TEXT("General"), TEXT("PosY"), -9999);

    bool bInsideMonitors = false;
    for(UINT i=0; i<monitors.Num(); i++)
    {
        if( posX >= monitors[i].rect.left && posX < monitors[i].rect.right  &&
            posY >= monitors[i].rect.top  && posY < monitors[i].rect.bottom )
        {
            bInsideMonitors = true;
            break;
        }
    }

    if(bInsideMonitors)
    {
        x = posX;
        y = posY;
    }

    bPanelVisibleWindowed = GlobalConfig->GetInt(TEXT("General"), TEXT("PanelVisibleWindowed"), 1) != 0;
    bPanelVisibleFullscreen = GlobalConfig->GetInt(TEXT("General"), TEXT("PanelVisibleFullscreen"), 0) != 0;
    bPanelVisible = bPanelVisibleWindowed; // Assuming OBS always starts windowed
    bPanelVisibleProcessed = false; // Force immediate process

    bFullscreenMode = false;

    hwndMain = CreateWindowEx(WS_EX_CONTROLPARENT|WS_EX_WINDOWEDGE|(LocaleIsRTL() ? WS_EX_LAYOUTRTL : 0), OBS_WINDOW_CLASS, GetApplicationName(),
        WS_OVERLAPPED | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        x, y, cx, cy, NULL, NULL, hinstMain, NULL);
    if(!hwndMain)
        CrashError(TEXT("Could not create main window"));

    hmenuMain = GetMenu(hwndMain);
    LocalizeMenu(hmenuMain);

    //-----------------------------------------------------
    // render frame

    hwndRenderFrame = CreateWindow(OBS_RENDERFRAME_CLASS, NULL,
        WS_CHILDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 0, 0,
        hwndMain, NULL, hinstMain, NULL);
    if(!hwndRenderFrame)
        CrashError(TEXT("Could not create render frame"));

    //-----------------------------------------------------
    // projector window

    hwndProjector = CreateWindow(OBS_PROJECTORFRAME_CLASS,
        L"OBS Projector Window",
        WS_POPUP, 0, 0, 0, 0,
        NULL, NULL, hinstMain, NULL);

    //-----------------------------------------------------
    // log window

    x = (fullscreenX/2)-(600/2);
    y = (fullscreenY/2)-(500/2);

    int logPosX  = GlobalConfig->GetInt(TEXT("General"), TEXT("LogPosX"), -9999);
    int logPosY  = GlobalConfig->GetInt(TEXT("General"), TEXT("LogPosY"), -9999);
    int logSizeX = GlobalConfig->GetInt(TEXT("General"), TEXT("LogSizeX"), 600);
    int logSizeY = GlobalConfig->GetInt(TEXT("General"), TEXT("LogSizeY"), 500);

    bInsideMonitors = false;
    for(UINT i=0; i<monitors.Num(); i++)
    {
        if( logPosX >= monitors[i].rect.left && logPosX < monitors[i].rect.right  &&
            logPosY >= monitors[i].rect.top  && logPosY < monitors[i].rect.bottom )
        {
            bInsideMonitors = true;
            break;
        }
    }

    if(bInsideMonitors)
    {
        x = logPosX;
        y = logPosY;
    }
    else
    {
        logSizeX = 600;
        logSizeY = 500;
    }

    hwndLogWindow = CreateWindowEx(LocaleIsRTL() ? WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT : 0, OBS_LOGWINDOW_CLASS, L"LogWindow", WS_OVERLAPPEDWINDOW, x, y, logSizeX, logSizeY, NULL, NULL, hinstMain, NULL);
    LocalizeWindow(hwndLogWindow);

    RECT client;
    GetClientRect(hwndLogWindow, &client);

    hwndLog = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
        ES_MULTILINE | WS_VISIBLE | WS_CHILD | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL | ES_READONLY | ES_NOHIDESEL,
        client.left, client.top, client.right, client.bottom, hwndLogWindow, (HMENU)ID_LOG_WINDOW, 0, 0);
    SendMessage(hwndLog, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    SendMessage(hwndLog, EM_SETLIMITTEXT, 0, 0);
    ShowWindow(hwndLog, SW_SHOW);

    ResetLogUpdateCallback([] { PostMessage(hwndLogWindow, WM_COMMAND, MAKEWPARAM(ID_LOG_WINDOW, 0), 0); });

    //-----------------------------------------------------
    // render frame text

    hwndRenderMessage = CreateWindow(TEXT("STATIC"), Str("MainWindow.BeginMessage"),
        WS_CHILDWINDOW|WS_VISIBLE|WS_CLIPSIBLINGS|SS_CENTER,
        0, 0, 0, 0, hwndRenderFrame, NULL, hinstMain, NULL);
    SendMessage(hwndRenderMessage, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

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
    // start/stop recording button

    hwndTemp = CreateWindow(TEXT("BUTTON"), Str("MainWindow.StartRecording"),
        WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|BS_TEXT|BS_SPLITBUTTON|WS_CLIPSIBLINGS,
        0, 0, 0, 0, hwndMain, (HMENU)ID_TOGGLERECORDING, 0, 0);
    SendMessage(hwndTemp, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

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
    // notification area

    bNotificationAreaIcon = false;
    wmExplorerRestarted = RegisterWindowMessage(TEXT("TaskbarCreated"));
    if (AppConfig->GetInt(TEXT("General"), TEXT("ShowNotificationAreaIcon"), 0) != 0)
    {
        ShowNotificationAreaIcon();
    }

    //-----------------------------------------------------
    // populate scenes

    hwndTemp = GetDlgItem(hwndMain, ID_SCENES);

    String collection = GetCurrentSceneCollection();

    if (!OSFileExists(String() << lpAppDataPath << L"\\sceneCollection\\" << collection << L".xconfig"))
        collection.Clear();

    if (collection.IsEmpty())
    {
        OSFindData ofd;
        HANDLE hFind = OSFindFirstFile(String() << lpAppDataPath << L"\\sceneCollection\\*.xconfig", ofd);
        if (hFind)
        {
            do
            {
                if (!ofd.bDirectory)
                {
                    collection = GetPathWithoutExtension(ofd.fileName);
                    break;
                }
            } while (OSFindNextFile(hFind, ofd));
            OSFindClose(hFind);
        }

        if (collection.IsEmpty())
        {
            CopyFile(String() << lpAppDataPath << L"\\scenes.xconfig", String() << lpAppDataPath << L"\\sceneCollection\\scenes.xconfig", true);
            collection = L"scenes";
            GlobalConfig->SetString(L"General", L"SceneCollection", collection);
        }
    }

    String strScenesConfig;
    strScenesConfig = FormattedString(L"%s\\sceneCollection\\%s.xconfig", lpAppDataPath, collection.Array());

    if(!scenesConfig.Open(strScenesConfig))
        CrashError(TEXT("Could not open '%s'"), strScenesConfig.Array());

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
        //scene->SetString(TEXT("class"), TEXT("Scene"));
        SendMessage(hwndTemp, LB_ADDSTRING, 0, (LPARAM)scene->GetName());
    }

    //-----------------------------------------------------
    // populate sources

    if(numScenes)
    {
        String strScene = AppConfig->GetString(TEXT("General"), TEXT("CurrentScene"));
        int id = (int)SendMessage(hwndTemp, LB_FINDSTRINGEXACT, -1, (LPARAM)strScene.Array());
        if(id == LB_ERR)
            id = 0;

        SendMessage(hwndTemp, LB_SETCURSEL, (WPARAM)id, 0);
        SendMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_SCENES, LBN_SELCHANGE), (LPARAM)GetDlgItem(hwndMain, ID_SCENES));
    }

    //-----------------------------------------------------

    hHotkeyMutex = OSCreateMutex();
    hInfoMutex = OSCreateMutex();
    hStartupShutdownMutex = OSCreateMutex();

    //-----------------------------------------------------

    API = CreateOBSApiInterface();

    bDragResize = false;

    if(GlobalConfig->GetInt(TEXT("General"), TEXT("Maximized")))
    { // Window was maximized last session
        SendMessage(hwndMain, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    }

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
    // Add built-in settings panes

    currentSettingsPane = NULL;
    AddBuiltInSettingsPanes();
    AddEncoderSettingsPanes();

    //-----------------------------------------------------

    ReloadIniSettings();
    ResetProfileMenu();
    ResetSceneCollectionMenu();
    ResetLogUploadMenu();

    //-----------------------------------------------------

    bAutoReconnect = AppConfig->GetInt(TEXT("Publish"), TEXT("AutoReconnect"), 1) != 0;
    reconnectTimeout = AppConfig->GetInt(TEXT("Publish"), TEXT("AutoReconnectTimeout"), 10);

    hHotkeyThread = OSCreateThread((XTHREAD)HotkeyThread, NULL);

#ifndef OBS_DISABLE_AUTOUPDATE
    ULARGE_INTEGER lastUpdateTime;
    ULARGE_INTEGER currentTime;
    FILETIME systemTime;

    lastUpdateTime.QuadPart = GlobalConfig->GetInt(TEXT("General"), OBS_CONFIG_UPDATE_KEY, 0);

    GetSystemTimeAsFileTime(&systemTime);
    currentTime.LowPart = systemTime.dwLowDateTime;
    currentTime.HighPart = systemTime.dwHighDateTime;

    //OBS doesn't support 64 bit ints in the config file, so we have to normalize it to a 32 bit int
    currentTime.QuadPart /= 10000000;
    currentTime.QuadPart -= 13000000000;

    if (currentTime.QuadPart - lastUpdateTime.QuadPart >= 3600)
    {
        GlobalConfig->SetInt(TEXT("General"), OBS_CONFIG_UPDATE_KEY, (int)currentTime.QuadPart);
        OSCloseThread(OSCreateThread((XTHREAD)CheckUpdateThread, (LPVOID)0));
    }
#endif

    // TODO: Should these be stored in the config file?
    bRenderViewEnabled = GlobalConfig->GetInt(TEXT("General"), TEXT("PreviewEnabled"), 1) != 0;
    bForceRenderViewErase = false;
    renderFrameIn1To1Mode = false;

    if(GlobalConfig->GetInt(TEXT("General"), TEXT("ShowWebrootWarning"), TRUE) && IsWebrootLoaded())
        OBSMessageBox(hwndMain, TEXT("Webroot Secureanywhere appears to be active.  This product will cause problems with OBS as the security features block OBS from accessing Windows GDI functions.  It is highly recommended that you disable Secureanywhere and restart OBS.\r\n\r\nOf course you can always just ignore this message if you want, but it may prevent you from being able to stream certain things. Please do not report any bugs you may encounter if you leave Secureanywhere enabled."), TEXT("Just a slight issue you might want to be aware of"), MB_OK);

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
                    bool bLoaded = false;

                    //slightly redundant I suppose seeing as both these things are being added at the same time
                    LOADPLUGINEXPROC loadPluginEx = (LOADPLUGINEXPROC)GetProcAddress(hPlugin, "LoadPluginEx");
                    if (loadPluginEx) {
                        bLoaded = loadPluginEx(OBSGetAPIVersion());
                    } else {
                        LOADPLUGINPROC loadPlugin = (LOADPLUGINPROC)GetProcAddress(hPlugin, "LoadPlugin");
                        bLoaded = loadPlugin && loadPlugin();
                    }

                    if (bLoaded) {
                        PluginInfo *pluginInfo = plugins.CreateNew();
                        pluginInfo->hModule = hPlugin;
                        pluginInfo->strFile = ofd.fileName;

                        /* get event callbacks for the plugin */
                        pluginInfo->startStreamCallback                = (OBS_CALLBACK)GetProcAddress(hPlugin, "OnStartStream");
                        pluginInfo->stopStreamCallback                 = (OBS_CALLBACK)GetProcAddress(hPlugin, "OnStopStream");
                        pluginInfo->startStreamingCallback             = (OBS_CALLBACK)GetProcAddress(hPlugin, "OnStartStreaming");
                        pluginInfo->stopStreamingCallback              = (OBS_CALLBACK)GetProcAddress(hPlugin, "OnStopStreaming");
                        pluginInfo->startRecordingCallback             = (OBS_CALLBACK)GetProcAddress(hPlugin, "OnStartRecording");
                        pluginInfo->stopRecordingCallback              = (OBS_CALLBACK)GetProcAddress(hPlugin, "OnStopRecording");
                        pluginInfo->statusCallback                     = (OBS_STATUS_CALLBACK)GetProcAddress(hPlugin, "OnOBSStatus");
                        pluginInfo->streamStatusCallback               = (OBS_STREAM_STATUS_CALLBACK)GetProcAddress(hPlugin, "OnStreamStatus");
                        pluginInfo->sceneSwitchCallback                = (OBS_SCENE_SWITCH_CALLBACK)GetProcAddress(hPlugin, "OnSceneSwitch");
                        pluginInfo->sceneCollectionSwitchCallback      = (OBS_SCENE_SWITCH_CALLBACK)GetProcAddress(hPlugin, "OnSceneCollectionSwitch");
                        pluginInfo->scenesChangedCallback              = (OBS_CALLBACK)GetProcAddress(hPlugin, "OnScenesChanged");
                        pluginInfo->sceneCollectionsChangedCallback    = (OBS_CALLBACK)GetProcAddress(hPlugin, "OnSceneCollectionsChanged");
                        pluginInfo->sourceOrderChangedCallback         = (OBS_CALLBACK)GetProcAddress(hPlugin, "OnSourceOrderChanged");
                        pluginInfo->sourceChangedCallback              = (OBS_SOURCE_CHANGED_CALLBACK)GetProcAddress(hPlugin, "OnSourceChanged");
                        pluginInfo->sourcesAddedOrRemovedCallback      = (OBS_CALLBACK)GetProcAddress(hPlugin, "OnSourcesAddedOrRemoved");
                        pluginInfo->micVolumeChangeCallback            = (OBS_VOLUME_CHANGED_CALLBACK)GetProcAddress(hPlugin, "OnMicVolumeChanged");
                        pluginInfo->desktopVolumeChangeCallback        = (OBS_VOLUME_CHANGED_CALLBACK)GetProcAddress(hPlugin, "OnDesktopVolumeChanged");
                        pluginInfo->logUpdateCallback                  = (OBS_LOG_UPDATE_CALLBACK)GetProcAddress(hPlugin, "OnLogUpdate");
                        pluginInfo->startRecordingReplayBufferCallback = (OBS_CALLBACK)GetProcAddress(hPlugin, "OnStartRecordingReplayBuffer");
                        pluginInfo->stopRecordingReplayBufferCallback  = (OBS_CALLBACK)GetProcAddress(hPlugin, "OnStopRecordingReplayBuffer");
                        pluginInfo->replayBufferSavedCallback          = (OBS_REPLAY_BUFFER_SAVED_CALLBACK)GetProcAddress(hPlugin, "OnReplayBufferSaved");

                        //GETPLUGINNAMEPROC getName = (GETPLUGINNAMEPROC)GetProcAddress(hPlugin, "GetPluginName");

                        //CTSTR lpName = (getName) ? getName() : TEXT("<unknown>");

                        //FIXME: TODO: log this somewhere else, it comes before the OBS version info and looks weird.
                        //Log(TEXT("Loaded plugin '%s', %s"), lpName, strLocation);
                    }
                    else
                    {
                        Log(TEXT("Failed to initialize plugin %s"), strLocation.Array());
                        FreeLibrary(hPlugin);
                    }
                }
                else
                {
                    DWORD err = GetLastError();
                    Log(TEXT("Failed to load plugin %s, %d"), strLocation.Array(), err);
#ifndef _DEBUG
                    if (err == 193)
                    {
#ifdef _M_X64
                        String message = FormattedString(Str("Plugins.InvalidVersion"), ofd.fileName, 32);
#else
                        String message = FormattedString(Str("Plugins.InvalidVersion"), ofd.fileName, 64);
#endif
                        OBSMessageBox(hwndMain, message.Array(), NULL, MB_ICONEXCLAMATION);
                    }
#endif
                }
            }
        } while (OSFindNextFile(hFind, ofd));

        OSFindClose(hFind);
    }

    ConfigureStreamButtons();

    ResizeWindow(false);
    ShowWindow(hwndMain, SW_SHOW);

    renderFrameIn1To1Mode = !!GlobalConfig->GetInt(L"General", L"1to1Preview", false);

    App->bAlwaysOnTop = !!GlobalConfig->GetInt(L"General", L"AlwaysOnTop");
    CheckMenuItem(GetMenu(hwndMain), ID_ALWAYSONTOP, (App->bAlwaysOnTop) ? MF_CHECKED : MF_UNCHECKED);
    SetWindowPos(hwndMain, (App->bAlwaysOnTop) ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);

    // make sure sources listview column widths are as expected after obs window is shown

    ListView_SetColumnWidth(hwndSources,0,LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(hwndSources,1,LVSCW_AUTOSIZE_USEHEADER);

    if (GlobalConfig->GetInt(L"General", L"ShowLogWindowOnLaunch") != 0)
        PostMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_SHOWLOG, 0), 0);

    if (bStreamOnStart)
        PostMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_STARTSTOP, 0), NULL);
}


OBS::~OBS()
{
    Stop(true, true);

    bShuttingDown = true;

    OSTerminateThread(hHotkeyThread, 2500);

    ClosePendingStreams();

    for(UINT i=0; i<plugins.Num(); i++)
    {
        PluginInfo &pluginInfo = plugins[i];

        UNLOADPLUGINPROC unloadPlugin = (UNLOADPLUGINPROC)GetProcAddress(pluginInfo.hModule, "UnloadPlugin");
        if(unloadPlugin)
            unloadPlugin();

        FreeLibrary(pluginInfo.hModule);
        pluginInfo.strFile.Clear();

        ZeroMemory(&pluginInfo, sizeof(pluginInfo));
    }

    if (AppConfig->GetInt(TEXT("General"), TEXT("ShowNotificationAreaIcon"), 0) != 0)
    {
        App->HideNotificationAreaIcon();
    }

    if (logDirectoryMonitor)
        OSMonitorDirectoryCallbackStop(logDirectoryMonitor);

    //DestroyWindow(hwndMain);

    // Remember window state for next launch
    WINDOWPLACEMENT placement;
    placement.length = sizeof(placement);
    GetWindowPlacement(hwndMain, &placement);
    RECT rect = { 0 };
    GetWindowRect(hwndMain, &rect);
    GlobalConfig->SetInt(TEXT("General"), TEXT("PosX"), rect.left);
    GlobalConfig->SetInt(TEXT("General"), TEXT("PosY"), rect.top);
    GlobalConfig->SetInt(TEXT("General"), TEXT("Width"),
            rect.right - rect.left -
            GetSystemMetrics(SM_CXSIZEFRAME) * 2);
    GlobalConfig->SetInt(TEXT("General"), TEXT("Height"),
            rect.bottom - rect.top -
            GetSystemMetrics(SM_CYSIZEFRAME) * 2 - GetSystemMetrics(SM_CYCAPTION) - GetSystemMetrics(SM_CYMENU));
    GlobalConfig->SetInt(TEXT("General"), TEXT("Maximized"), placement.showCmd == SW_SHOWMAXIMIZED ? 1 : 0);

	GetWindowRect(hwndLogWindow, &rect);
    GlobalConfig->SetInt(TEXT("General"), TEXT("LogPosX"), rect.left);
    GlobalConfig->SetInt(TEXT("General"), TEXT("LogPosY"), rect.top);
    GlobalConfig->SetInt(TEXT("General"), TEXT("LogSizeX"), rect.right - rect.left);
    GlobalConfig->SetInt(TEXT("General"), TEXT("LogSizeY"), rect.bottom - rect.top);

    GlobalConfig->SetInt(L"General", L"1to1Preview", renderFrameIn1To1Mode);
    GlobalConfig->SetInt(L"General", L"AlwaysOnTop", App->bAlwaysOnTop);
    
    // Save control panel visibility
    GlobalConfig->SetInt(TEXT("General"), TEXT("PanelVisibleWindowed"), bPanelVisibleWindowed ? 1 : 0);
    GlobalConfig->SetInt(TEXT("General"), TEXT("PanelVisibleFullscreen"), bPanelVisibleFullscreen ? 1 : 0);

    // Save preview enabled/disabled state
    GlobalConfig->SetInt(TEXT("General"), TEXT("PreviewEnabled"), bRenderViewEnabled ? 1 : 0);

    scenesConfig.SaveTo(String() << lpAppDataPath << "\\scenes.xconfig");
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

    if (hVideoEvent)
        CloseHandle(hVideoEvent);

    if(hSceneMutex)
        OSCloseMutex(hSceneMutex);

    if(hAuxAudioMutex)
        OSCloseMutex(hAuxAudioMutex);

    delete API;
    API = NULL;

    for (UINT i=0; i<settingsPanes.Num(); i++)
        delete settingsPanes[i];

    if(hInfoMutex)
        OSCloseMutex(hInfoMutex);
    if(hHotkeyMutex)
        OSCloseMutex(hHotkeyMutex);

    App = NULL;
}

/**
 * Controls which message should be displayed in the middle of the main window.
 */
void OBS::UpdateRenderViewMessage()
{
    if(bRunning)
    {
        if(bRenderViewEnabled)
        {
            // Message should be invisible
            ShowWindow(hwndRenderMessage, SW_HIDE);
        }
        else
        {
            ShowWindow(hwndRenderMessage, SW_SHOW);
            SetWindowText(hwndRenderMessage, Str("MainWindow.PreviewDisabled"));
        }
    }
    else
    {
        ShowWindow(hwndRenderMessage, SW_SHOW);
        SetWindowText(hwndRenderMessage, Str("MainWindow.BeginMessage"));
    }
}

void OBS::ResizeRenderFrame(bool bRedrawRenderFrame)
{
    // Get output steam size and aspect ratio
    int curCX, curCY;
    float mainAspect;
    if(bRunning)
    {
        curCX = outputCX;
        curCY = outputCY;
        mainAspect = float(curCX)/float(curCY);
    }
    else
    {
        // Default to the monitor's resolution if the base size is undefined
        int monitorID = AppConfig->GetInt(TEXT("Video"), TEXT("Monitor"));
        if(monitorID >= (int)monitors.Num())
            monitorID = 0;
        RECT &screenRect = monitors[monitorID].rect;
        int defCX = screenRect.right  - screenRect.left;
        int defCY = screenRect.bottom - screenRect.top;

        // Calculate output size using the same algorithm that's in OBS::Start()
        float scale = AppConfig->GetFloat(TEXT("Video"), TEXT("Downscale"), 1.0f);
        curCX = AppConfig->GetInt(TEXT("Video"), TEXT("BaseWidth"),  defCX);
        curCY = AppConfig->GetInt(TEXT("Video"), TEXT("BaseHeight"), defCY);
        curCX = MIN(MAX(curCX, 128), 4096);
        curCY = MIN(MAX(curCY, 128), 4096);
        curCX = UINT(double(curCX) / double(scale));
        curCY = UINT(double(curCY) / double(scale));
        curCX = curCX & 0xFFFFFFFC; // Align width to 128bit for fast SSE YUV4:2:0 conversion
        curCY = curCY & 0xFFFFFFFE;

        mainAspect = float(curCX)/float(curCY);
    }

    // Get area to render in
    int x, y;
    UINT controlWidth  = clientWidth;
    UINT controlHeight = clientHeight;
    if(bPanelVisible)
        controlHeight -= totalControlAreaHeight + controlPadding;
    UINT newRenderFrameWidth, newRenderFrameHeight;
    if(renderFrameIn1To1Mode)
    {
        newRenderFrameWidth  = (UINT)curCX;
        newRenderFrameHeight = (UINT)curCY;
        x = (int)controlWidth / 2 - curCX / 2;
        y = (int)controlHeight / 2 - curCY / 2;
    }
    else
    { // Scale to fit
        Vect2 renderSize = Vect2(float(controlWidth), float(controlHeight));
        float renderAspect = renderSize.x/renderSize.y;

        if(renderAspect > mainAspect)
        {
            renderSize.x = renderSize.y*mainAspect;
            x = int((float(controlWidth)-renderSize.x)*0.5f);
            y = 0;
        }
        else
        {
            renderSize.y = renderSize.x/mainAspect;
            x = 0;
            y = int((float(controlHeight)-renderSize.y)*0.5f);
        }

        // Round and ensure even size
        newRenderFrameWidth  = int(renderSize.x+0.5f)&0xFFFFFFFE;
        newRenderFrameHeight = int(renderSize.y+0.5f)&0xFFFFFFFE;
    }

    // Fill the majority of the window with the 3D scene. We'll render everything in DirectX
    SetWindowPos(hwndRenderFrame, NULL, 0, 0, controlWidth, controlHeight, SWP_NOOWNERZORDER);

    //----------------------------------------------

    renderFrameX = x;
    renderFrameY = y;
    renderFrameWidth  = newRenderFrameWidth;
    renderFrameHeight = newRenderFrameHeight;
    renderFrameCtrlWidth  = controlWidth;
    renderFrameCtrlHeight = controlHeight;
    if(!bRunning)
    {
        oldRenderFrameCtrlWidth = renderFrameCtrlWidth;
        oldRenderFrameCtrlHeight = renderFrameCtrlHeight;
        InvalidateRect(hwndRenderMessage, NULL, true); // Repaint text
    }
    else if(bRunning && bRedrawRenderFrame)
    {
        oldRenderFrameCtrlWidth = renderFrameCtrlWidth;
        oldRenderFrameCtrlHeight = renderFrameCtrlHeight;
        bResizeRenderView = true;
    }
}

void OBS::SetFullscreenMode(bool fullscreen)
{
    if(App->bFullscreenMode == fullscreen)
        return; // Nothing to do

    App->bFullscreenMode = fullscreen;
    if(fullscreen)
    {
        // Remember current window placement
        fullscreenPrevPlacement.length = sizeof(fullscreenPrevPlacement);
        GetWindowPlacement(hwndMain, &fullscreenPrevPlacement);

        // Update panel visibility if required
        if(bPanelVisible != bPanelVisibleFullscreen) {
            bPanelVisible = bPanelVisibleFullscreen;
            bPanelVisibleProcessed = false;
        }

        // Hide borders
        LONG style = GetWindowLong(hwndMain, GWL_STYLE);
        SetWindowLong(hwndMain, GWL_STYLE, style & ~(WS_CAPTION | WS_THICKFRAME));

        // Hide menu and status bar
        SetMenu(hwndMain, NULL);

        // Fill entire screen
        HMONITOR monitorForWidow = MonitorFromWindow(hwndMain, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo;
        monitorInfo.cbSize = sizeof(monitorInfo);
        GetMonitorInfo(monitorForWidow, &monitorInfo);
        int x = monitorInfo.rcMonitor.left;
        int y = monitorInfo.rcMonitor.top;
        int cx = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
        int cy = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
        SetWindowPos(hwndMain, HWND_TOPMOST, x, y, cx, cy, SWP_FRAMECHANGED);

        // Update menu checkboxes
        CheckMenuItem(hmenuMain, ID_FULLSCREENMODE, MF_CHECKED);
    }
    else
    {
        // Show borders
        LONG style = GetWindowLong(hwndMain, GWL_STYLE);
        SetWindowLong(hwndMain, GWL_STYLE, style | WS_CAPTION | WS_THICKFRAME);

        // Show menu and status bar
        SetMenu(hwndMain, hmenuMain);

        // Restore control panel visible state if required
        if(bPanelVisible != bPanelVisibleWindowed) {
            bPanelVisible = bPanelVisibleWindowed;
            bPanelVisibleProcessed = false;
        }

        // Restore original window size
        SetWindowPlacement(hwndMain, &fullscreenPrevPlacement);

        // Update menu checkboxes
        CheckMenuItem(hmenuMain, ID_FULLSCREENMODE, MF_UNCHECKED);

        // Disable always-on-top if needed
        SetWindowPos(hwndMain, (App->bAlwaysOnTop)?HWND_TOPMOST:HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
    }

    // Workaround: If the window is maximized, resize isn't called, so do it manually
    // Also, when going into fullscreen, this can prevent pixelation
    ResizeWindow(true);
}

/**
 * Show or hide the control panel.
 */
void OBS::ProcessPanelVisible(bool fromResizeWindow)
{
    if(bPanelVisibleProcessed)
        return; // Already done

    const int visible = bPanelVisible ? SW_SHOW : SW_HIDE;

    ShowWindow(GetDlgItem(hwndMain, ID_MICVOLUME), visible);
    ShowWindow(GetDlgItem(hwndMain, ID_DESKTOPVOLUME), visible);
    ShowWindow(GetDlgItem(hwndMain, ID_MICVOLUMEMETER), visible);
    ShowWindow(GetDlgItem(hwndMain, ID_DESKTOPVOLUMEMETER), visible);
    ShowWindow(GetDlgItem(hwndMain, ID_SETTINGS), visible);
    ShowWindow(GetDlgItem(hwndMain, ID_TOGGLERECORDING), visible);
    ShowWindow(GetDlgItem(hwndMain, ID_STARTSTOP), visible);
    ShowWindow(GetDlgItem(hwndMain, ID_SCENEEDITOR), visible);
    ShowWindow(GetDlgItem(hwndMain, ID_TESTSTREAM), visible);
    ShowWindow(GetDlgItem(hwndMain, ID_GLOBALSOURCES), visible);
    ShowWindow(GetDlgItem(hwndMain, ID_PLUGINS), visible);
    if(!bPanelVisible) ShowWindow(GetDlgItem(hwndMain, ID_DASHBOARD), SW_HIDE);
    ShowWindow(GetDlgItem(hwndMain, ID_EXIT), visible);
    ShowWindow(GetDlgItem(hwndMain, ID_SCENES_TEXT), visible);
    ShowWindow(GetDlgItem(hwndMain, ID_SOURCES_TEXT), visible);
    ShowWindow(GetDlgItem(hwndMain, ID_SCENES), visible);
    ShowWindow(GetDlgItem(hwndMain, ID_SOURCES), visible);

    bPanelVisibleProcessed = true;

    // HACK: Force resize to fix dashboard button. The setting should not be calculated every resize
    if(bPanelVisible && !fromResizeWindow)
        ResizeWindow(false);
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
    parts[0] = parts[1]-170;
    SendMessage(hwndTemp, SB_SETPARTS, 5, (LPARAM)parts);

    int resetXPos = xStart+listControlWidth*2;

    //-----------------------------------------------------

    UpdateRenderViewMessage();
    SetWindowPos(hwndRenderMessage, NULL, 0, renderFrameCtrlHeight / 2 - 10, renderFrameCtrlWidth, 50, flags & ~SWP_SHOWWINDOW);

    //-----------------------------------------------------

    // Don't waste time resizing invisible controls
    if(!bPanelVisibleProcessed)
        ProcessPanelVisible(true);
    if(!bPanelVisible)
        return;

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

    SetWindowPos(GetDlgItem(hwndMain, ID_TOGGLERECORDING), NULL, xPos, yPos, controlWidth-controlPadding, controlHeight, flags);
    xPos += controlWidth;

    yPos += controlHeight+controlPadding;

    //-----------------------------------------------------

    xPos = resetXPos;

    SetWindowPos(GetDlgItem(hwndMain, ID_GLOBALSOURCES), NULL, xPos, yPos, controlWidth-controlPadding, controlHeight, flags);
    xPos += controlWidth;

    SetWindowPos(GetDlgItem(hwndMain, ID_TESTSTREAM), NULL, xPos, yPos, controlWidth-controlPadding, controlHeight, flags);
    xPos += controlWidth;

    yPos += controlHeight+controlPadding;

    //-----------------------------------------------------

    xPos = resetXPos;

    SetWindowPos(GetDlgItem(hwndMain, ID_PLUGINS), NULL, xPos, yPos, controlWidth-controlPadding, controlHeight, flags);
    xPos += controlWidth;

    UpdateDashboardButton();

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

void OBS::GetProfiles(StringList &profileList)
{
    String strProfilesWildcard;
    OSFindData ofd;
    HANDLE hFind;

    profileList.Clear();

    String profileDir(FormattedString(L"%s/profiles/", OBSGetAppDataPath()));

    strProfilesWildcard << profileDir << "*.ini";

    if(hFind = OSFindFirstFile(strProfilesWildcard, ofd))
    {
        do
        {
            String profile(GetPathWithoutExtension(ofd.fileName));
            String profilePath(FormattedString(L"%s%s.ini", profileDir.Array(), profile.Array()));
            if(ofd.bDirectory || !OSFileExists(profilePath) || profileList.HasValue(profile)) continue;
            profileList << profile;
        } while(OSFindNextFile(hFind, ofd));

        OSFindClose(hFind);
    }
}

void OBS::GetSceneCollection(StringList &sceneCollectionList)
{
    String strSceneCollectionWildcard;
    OSFindData ofd;
    HANDLE hFind;

    sceneCollectionList.Clear();

    String sceneCollectionDir(FormattedString(L"%s/sceneCollection/", OBSGetAppDataPath()));
    strSceneCollectionWildcard << sceneCollectionDir << "*.xconfig";
    if (hFind = OSFindFirstFile(strSceneCollectionWildcard, ofd))
    {
        do
        {
            String sceneCollection(GetPathWithoutExtension(ofd.fileName));
            String sceneCollectionPath(FormattedString(L"%s%s.xconfig", sceneCollectionDir.Array(), sceneCollection.Array()));
            if (ofd.bDirectory || !OSFileExists(sceneCollectionPath) || sceneCollectionList.HasValue(sceneCollection)) continue;
            sceneCollectionList << sceneCollection;
        } while (OSFindNextFile(hFind, ofd));
        OSFindClose(hFind);
    }
}

void OBS::RefreshStreamButtons(bool disable)
{
    if (bShuttingDown) return;
    int networkMode = AppConfig->GetInt(TEXT("Publish"), TEXT("Mode"), 2);
    bRecordingOnly = (networkMode == 1);
    bool canStream = networkMode == 0 && !bTestStream && bStreamFlushed;
    canRecord = !bTestStream;
    bool canTest = !bRecordingReplayBuffer && !bRecording && (!bStreaming || bTestStream);

    EnableWindow(GetDlgItem(hwndMain, ID_STARTSTOP), !disable && canStream);
    EnableWindow(GetDlgItem(hwndMain, ID_TOGGLERECORDING), !disable && canRecord);
    EnableWindow(GetDlgItem(hwndMain, ID_TESTSTREAM), !disable && canTest);

    HMENU fileMenu = GetSubMenu(GetMenu(hwndMain), 0);
    EnableMenuItem(fileMenu, 2, (canRecord ? MF_ENABLED : MF_DISABLED) | MF_BYPOSITION);
}

void OBS::ConfigureStreamButtons()
{
    if (bShuttingDown) return;

    if (GetWindowThreadProcessId(hwndMain, nullptr) != GetCurrentThreadId())
        return PostConfigureStreamButtons();

    RefreshStreamButtons();
    SetWindowText(GetDlgItem(hwndMain, ID_STARTSTOP), (bStreaming && !bTestStream) ? Str("MainWindow.StopStream") : Str("MainWindow.StartStream"));
    SetWindowText(GetDlgItem(hwndMain, ID_TOGGLERECORDING), bRecording ? Str("MainWindow.StopRecording") : Str("MainWindow.StartRecording"));
    SetWindowText(GetDlgItem(hwndMain, ID_TESTSTREAM), bTestStream ? Str("MainWindow.StopTest") : Str("MainWindow.TestStream"));
}

void OBS::PostConfigureStreamButtons()
{
    if (hwndMain) PostMessage(hwndMain, OBS_CONFIGURE_STREAM_BUTTONS, 0, 0);
}

void OBS::ReloadSceneCollection()
{
    HWND hwndTemp;
    hwndTemp = GetDlgItem(hwndMain, ID_SCENES);

    CTSTR collection = GetCurrentSceneCollection();
    String strScenesConfig = FormattedString(L"%s\\sceneCollection\\%s.xconfig", lpAppDataPath, collection);

    if (!scenesConfig.Open(strScenesConfig))
        CrashError(TEXT("Could not open '%s'"), strScenesConfig.Array());

    XElement *scenes = scenesConfig.GetElement(TEXT("scenes"));

    if (!scenes)
        scenes = scenesConfig.CreateElement(TEXT("scenes"));

    SendMessage(hwndTemp, LB_RESETCONTENT, 0, 0);

    App->sceneElement = NULL;
    
    UINT numScenes = scenes->NumElements();
    if (!numScenes)
    {
        XElement *scene = scenes->CreateElement(Str("Scene"));
        scene->SetString(TEXT("class"), TEXT("Scene"));
        numScenes++;
    }

    for (UINT i = 0; i<numScenes; i++)
    {
        XElement *scene = scenes->GetElementByID(i);
        SendMessage(hwndTemp, LB_ADDSTRING, 0, (LPARAM)scene->GetName());
    }
    //-----------------------------------------------------
    // populate sources

    if (numScenes)
    {
        String strScene = AppConfig->GetString(TEXT("General"), TEXT("CurrentScene"));
        int id = (int)SendMessage(hwndTemp, LB_FINDSTRINGEXACT, -1, (LPARAM)strScene.Array());
        if (id == LB_ERR)
            id = 0;

        SendMessage(hwndTemp, LB_SETCURSEL, (WPARAM)id, 0);
        SendMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_SCENES, LBN_SELCHANGE), (LPARAM)GetDlgItem(hwndMain, ID_SCENES));
    }

    //-----------------------------------------------------

    for (UINT i = 0; i<numScenes; i++)
    {
        XElement *scene = scenes->GetElementByID(i);
        DWORD hotkey = scene->GetInt(TEXT("hotkey"));
        if (hotkey)
        {
            SceneHotkeyInfo hotkeyInfo;
            hotkeyInfo.hotkey = hotkey;
            hotkeyInfo.scene = scene;
            hotkeyInfo.hotkeyID = API->CreateHotkey(hotkey, SceneHotkey, 0);

            if (hotkeyInfo.hotkeyID)
                sceneHotkeys << hotkeyInfo;
        }
    }
}

void OBS::ReloadIniSettings()
{
    HWND hwndTemp;

    //-------------------------------------------
    // mic volume data
    hwndTemp = GetDlgItem(hwndMain, ID_MICVOLUME);

    if(!AppConfig->HasKey(TEXT("Audio"), TEXT("MicVolume")))
        AppConfig->SetFloat(TEXT("Audio"), TEXT("MicVolume"), 1.0f);
    SetVolumeControlValue(hwndTemp, AppConfig->GetFloat(TEXT("Audio"), TEXT("MicVolume"), 1.0f));
    SetVolumeControlMutedVal(hwndTemp, AppConfig->GetFloat(TEXT("Audio"), TEXT("MicMutedVolume"), 1.0f));

    AudioDeviceList audioDevices;
    GetAudioDevices(audioDevices, ADT_RECORDING, false, true);

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
    SetVolumeControlMutedVal(hwndTemp, AppConfig->GetFloat(TEXT("Audio"), TEXT("DesktopMutedVolume"), 1.0f));

    //-------------------------------------------
    // desktop boost
    DWORD desktopBoostMultiple = GlobalConfig->GetInt(TEXT("Audio"), TEXT("DesktopBoostMultiple"), 1);
    if(desktopBoostMultiple < 1)
        desktopBoostMultiple = 1;
    else if(desktopBoostMultiple > 20)
        desktopBoostMultiple = 20;
    desktopBoost = float(desktopBoostMultiple);

    //-------------------------------------------
    // mic boost
    DWORD micBoostMultiple = AppConfig->GetInt(TEXT("Audio"), TEXT("MicBoostMultiple"), 1);
    if(micBoostMultiple < 1)
        micBoostMultiple = 1;
    else if(micBoostMultiple > 20)
        micBoostMultiple = 20;
    micBoost = float(micBoostMultiple);

    //-------------------------------------------
    // dashboard
    /*
    strDashboard = AppConfig->GetString(TEXT("Publish"), TEXT("Dashboard"));
    strDashboard.KillSpaces();
    UpdateDashboardButton();
    */

    //-------------------------------------------
    // hotkeys
    QuickClearHotkey(pushToTalkHotkeyID);
    QuickClearHotkey(pushToTalkHotkey2ID);
    QuickClearHotkey(muteMicHotkeyID);
    QuickClearHotkey(muteDesktopHotkeyID);
    QuickClearHotkey(stopStreamHotkeyID);
    QuickClearHotkey(startStreamHotkeyID);
    QuickClearHotkey(stopRecordingHotkeyID);
    QuickClearHotkey(startRecordingHotkeyID);
    QuickClearHotkey(stopReplayBufferHotkeyID);
    QuickClearHotkey(startReplayBufferHotkeyID);
    QuickClearHotkey(saveReplayBufferHotkeyID);
    QuickClearHotkey(recordFromReplayBufferHotkeyID);

    bUsingPushToTalk = !!AppConfig->GetInt(L"Audio", L"UsePushToTalk") != 0;
    DWORD hotkey = AppConfig->GetInt(TEXT("Audio"), TEXT("PushToTalkHotkey"));
    DWORD hotkey2 = AppConfig->GetInt(TEXT("Audio"), TEXT("PushToTalkHotkey2"));
    pushToTalkDelay = AppConfig->GetInt(TEXT("Audio"), TEXT("PushToTalkDelay"), 200);

    if(bUsingPushToTalk && hotkey)
        pushToTalkHotkeyID = API->CreateHotkey(hotkey, OBS::PushToTalkHotkey, NULL);
    if(bUsingPushToTalk && hotkey2)
        pushToTalkHotkey2ID = API->CreateHotkey(hotkey2, OBS::PushToTalkHotkey, NULL);

    hotkey = AppConfig->GetInt(TEXT("Audio"), TEXT("MuteMicHotkey"));
    if(hotkey)
        muteMicHotkeyID = API->CreateHotkey(hotkey, OBS::MuteMicHotkey, NULL);

    hotkey = AppConfig->GetInt(TEXT("Audio"), TEXT("MuteDesktopHotkey"));
    if(hotkey)
        muteDesktopHotkeyID = API->CreateHotkey(hotkey, OBS::MuteDesktopHotkey, NULL);

    hotkey = AppConfig->GetInt(TEXT("Publish"), TEXT("StopStreamHotkey"));
    if (hotkey)
        stopStreamHotkeyID = API->CreateHotkey(hotkey, OBS::StopStreamHotkey, NULL);

    hotkey = AppConfig->GetInt(TEXT("Publish"), TEXT("StartStreamHotkey"));
    if (hotkey)
        startStreamHotkeyID = API->CreateHotkey(hotkey, OBS::StartStreamHotkey, NULL);

    hotkey = AppConfig->GetInt(TEXT("Publish"), TEXT("StopRecordingHotkey"));
    if (hotkey)
        stopRecordingHotkeyID = API->CreateHotkey(hotkey, OBS::StopRecordingHotkey, NULL);

    hotkey = AppConfig->GetInt(TEXT("Publish"), TEXT("StartRecordingHotkey"));
    if (hotkey)
        startRecordingHotkeyID = API->CreateHotkey(hotkey, OBS::StartRecordingHotkey, NULL);

    hotkey = AppConfig->GetInt(L"Publish", L"StopReplayBufferHotkey");
    if (hotkey)
        stopReplayBufferHotkeyID = API->CreateHotkey(hotkey, OBS::StopReplayBufferHotkey, NULL);

    hotkey = AppConfig->GetInt(L"Publish", L"StartReplayBufferHotkey");
    if (hotkey)
        startReplayBufferHotkeyID = API->CreateHotkey(hotkey, OBS::StartReplayBufferHotkey, NULL);

    hotkey = AppConfig->GetInt(L"Publish", L"SaveReplayBufferHotkey");
    if (hotkey)
        saveReplayBufferHotkeyID = API->CreateHotkey(hotkey, OBS::SaveReplayBufferHotkey, NULL);

    hotkey = AppConfig->GetInt(L"Publish", L"RecordFromReplayBufferHotkey");
    if (hotkey)
        recordFromReplayBufferHotkeyID = API->CreateHotkey(hotkey, OBS::RecordFromReplayBufferHotkey, NULL);

    //-------------------------------------------
    // Notification Area icon
    bool showIcon = AppConfig->GetInt(TEXT("General"), TEXT("ShowNotificationAreaIcon"), 0) != 0;
    bool minimizeToIcon = AppConfig->GetInt(TEXT("General"), TEXT("MinimizeToNotificationArea"), 0) != 0;
    if (showIcon)
    {
        ShowNotificationAreaIcon();
        if (minimizeToIcon && IsIconic(hwndMain))
            ShowWindow(hwndMain, SW_HIDE);
    }
    else
        HideNotificationAreaIcon();

    bKeepRecording = AppConfig->GetInt(TEXT("Publish"), TEXT("KeepRecording")) != 0;

    if (!minimizeToIcon && !IsWindowVisible(hwndMain))
        ShowWindow(hwndMain, SW_SHOW);

    ConfigureStreamButtons();

    //--------------------------------------------
    // Update old config, transition old encoder selection
    int qsv = AppConfig->GetInt(L"Video Encoding", L"UseQSV", -1);
    int nvenc = AppConfig->GetInt(L"Video Encoding", L"UseNVENC", -1);
    if (qsv != -1 || nvenc != -1)
    {
        AppConfig->SetString(L"Video Encoding", L"Encoder", (qsv > 0) ? L"QSV" : (nvenc > 0) ? L"NVENC" : L"x264");
        AppConfig->Remove(L"Video Encoding", L"UseQSV");
        AppConfig->Remove(L"Video Encoding", L"UseNVENC");

        int custom = AppConfig->GetInt(L"Video Encoding", L"UseCustomSettings", -1);
        int customQSV = AppConfig->GetInt(L"Video Encoding", L"QSVUseVideoEncoderSettings", -1);
        if (custom > 0 && customQSV > 0)
            AppConfig->SetString(L"Video Encoding", L"CustomQSVSettings", AppConfig->GetString(L"Video Encoding", L"CustomSettings"));
    }
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
    if (bRunning && OSTryEnterMutex(hStartupShutdownMutex))
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
    
        if (bRunning && network)
        {
            ReportStreamStatus(bRunning, bTestStream, 
                (UINT) App->bytesPerSec, App->curStrain, 
                (UINT)this->totalStreamTime, (UINT)network->NumTotalVideoFrames(),
                (UINT)App->curFramesDropped, (UINT) App->captureFPS);
        }

        ReportOBSStatus(bRunning, bStreaming, bRecording, bTestStream, bReconnecting);

        OSLeaveMutex(hStartupShutdownMutex);
    }
}

void OBS::DrawStatusBar(DRAWITEMSTRUCT &dis)
{
    if(!App->bRunning && !App->bStreaming && !App->bRecording)
        return;

    HDC hdcTemp = CreateCompatibleDC(dis.hDC);
    HBITMAP hbmpTemp = CreateCompatibleBitmap(dis.hDC, dis.rcItem.right-dis.rcItem.left, dis.rcItem.bottom-dis.rcItem.top);
    SelectObject(hdcTemp, hbmpTemp);

    SelectObject(hdcTemp, GetCurrentObject(dis.hDC, OBJ_FONT));
    SetTextColor(hdcTemp, GetTextColor(dis.hDC));

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
        HBRUSH hColorBrush;
        DWORD green = 0xFF, red;

        statusBarData.bytesPerSec = App->bytesPerSec;
        statusBarData.strain = App->curStrain;
        //statusBarData.strain = rand()%101;

        //show grey rather than green when not connected
        if (App->network && App->network->NumTotalVideoFrames() == 0)
        {
            hColorBrush = CreateSolidBrush(RGB(100,100,100));
        }
        else
        {
            if(statusBarData.strain > 50.0)
                green = DWORD(((50.0-(statusBarData.strain-50.0))/50.0)*255.0);

            double redStrain = statusBarData.strain/50.0;
            if(redStrain > 1.0)
                redStrain = 1.0;

            red = DWORD(redStrain*255.0);

            //--------------------------------

            hColorBrush = CreateSolidBrush((green<<8)|red);
        }

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
        DWORD color = 0x000000;

        switch(dis.itemID)
        {
            case 0:
                {
                    StreamInfoPriority priority;
                    strOutString << App->GetMostImportantInfo(priority);
                    if (priority == StreamInfoPriority_Critical)
                        color = 0x0000FF;
                    break;
                }

            case 1:
                {
                    DWORD streamTimeSecondsTotal = App->totalStreamTime/1000;
                    DWORD streamTimeMinutesTotal = streamTimeSecondsTotal/60;
                    DWORD streamTimeSeconds = streamTimeSecondsTotal%60;

                    DWORD streamTimeHours = streamTimeMinutesTotal/60;
                    DWORD streamTimeMinutes = streamTimeMinutesTotal%60;

                    int networkMode = AppConfig->GetInt(TEXT("Publish"), TEXT("Mode"), 2);

                    strOutString = FormattedString(TEXT("%u:%02u:%02u"), streamTimeHours, streamTimeMinutes, streamTimeSeconds);

                    StringList mods;
                    if (App->bStreaming && !App->bTestStream)
                        mods << L"LIVE";
                    if (App->bRecording)
                        mods << L"REC";
                    if (App->bRecordingReplayBuffer)
                        mods << L"BUF";
                    if (App->bTestStream)
                        mods << L"Preview";

                    strOutString << FormattedString(L" (%s)", mods.Join(L" + ").Array());
                }
                break;
            case 2:
                {
                    double percentageDropped = 0.0;
                    if (OSTryEnterMutex(App->hStartupShutdownMutex))
                    {
                        if(App->network)
                        {
                            UINT numTotalFrames = App->network->NumTotalVideoFrames();
                            if(numTotalFrames)
                                percentageDropped = (double(App->network->NumDroppedFrames())/double(numTotalFrames))*100.0;
                        }
                        OSLeaveMutex(App->hStartupShutdownMutex);
                    }
                    strOutString << Str("MainWindow.DroppedFrames") << FormattedString(TEXT(" %u (%0.2f%%)"), App->curFramesDropped, percentageDropped);
                }
                break;
            case 3: strOutString << TEXT("FPS: ") << IntString(App->captureFPS); break;
        }

        if(strOutString.IsValid())
        {
            SetTextColor(hdcTemp, color);
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
        bool curRender = source->GetInt(TEXT("render"), 0) > 0;
        if(curRender != checked)
        {
            source->SetInt(TEXT("render"), (checked)?1:0);
            if(scene && i < scene->NumSceneItems())
            {
                SceneItem *sceneItem = scene->GetSceneItem(i);
                sceneItem->bRender = checked;
                sceneItem->SetRender(checked);
            }
            ReportSourceChanged(source->GetName(), source);
        }
    }
}

void OBS::SetSourceRender(CTSTR sourceName, bool render)
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
        if(scmp(source->GetName(), sourceName) == 0 && checked != render)
        {
            if(scene && i < scene->NumSceneItems())
            {
                SceneItem *sceneItem = scene->GetSceneItem(i);
                sceneItem->SetRender(render);
            }
            else
            {
                source->SetInt(TEXT("render"), (render)?1:0);
            }
            App->bChangingSources = true;
            ListView_SetCheckState(hwndSources, i, render);
            App->bChangingSources = false;

            ReportSourceChanged(sourceName, source);

            break;
        }
    }
}

BOOL OBS::SetNotificationAreaIcon(DWORD dwMessage, int idIcon, const String &tooltip)
{
    NOTIFYICONDATA niData;
    BOOL result;
    
    ZeroMemory(&niData, sizeof(NOTIFYICONDATA));
    niData.cbSize = sizeof(niData);
    niData.hWnd = hwndMain;
    niData.uID = 0;
    
    if (NIM_DELETE != dwMessage)
    {
        niData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        niData.uCallbackMessage = OBS_NOTIFICATIONAREA;
        niData.hIcon = LoadIcon(hinstMain, MAKEINTRESOURCE(idIcon));
        scpy_n (niData.szTip, tooltip, _countof(niData.szTip)-1);
    }

    result = Shell_NotifyIcon(dwMessage, &niData);

    if(niData.hIcon)
       DestroyIcon(niData.hIcon);

    return result;
}

BOOL OBS::ShowNotificationAreaIcon()
{
    BOOL result = FALSE;
    int idIcon = (bRunning && !bTestStream) ? IDI_ICON2 : IDI_ICON1;

    if (!bNotificationAreaIcon)
    {
        bNotificationAreaIcon = true;
        result = SetNotificationAreaIcon(NIM_ADD, idIcon, GetApplicationName());
    }
    else
    {
        result = SetNotificationAreaIcon(NIM_MODIFY, idIcon, GetApplicationName());
    }
    return result;
}

BOOL OBS::UpdateNotificationAreaIcon()
{
    if (bNotificationAreaIcon)
        return ShowNotificationAreaIcon();
    return TRUE;
}

BOOL OBS::HideNotificationAreaIcon()
{
    bNotificationAreaIcon = false;
    return SetNotificationAreaIcon(NIM_DELETE, 0, TEXT(""));
}

BOOL OBS::UpdateDashboardButton()
{
    //Are we in live stream mode, and do we have a non-empty dashboard string?
    /*
    BOOL bStreamOutput = AppConfig->GetInt(TEXT("Publish"), TEXT("Mode")) == 0;
    BOOL bDashboardValid = strDashboard.IsValid();

    BOOL bShowDashboardButton = bPanelVisible && bStreamOutput && bDashboardValid;

    return ShowWindow(GetDlgItem(hwndMain, ID_DASHBOARD), bShowDashboardButton ? SW_SHOW : SW_HIDE);
    */
    return false;
}

void OBS::ActuallyEnableProjector()
{
    DisableProjector();

    D3D10System *sys = static_cast<D3D10System*>(GS);

    DXGI_SWAP_CHAIN_DESC swapDesc;
    zero(&swapDesc, sizeof(swapDesc));
    swapDesc.BufferCount = 2;
    swapDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapDesc.BufferDesc.Width  = projectorWidth;
    swapDesc.BufferDesc.Height = projectorHeight;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.OutputWindow = hwndProjector;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.Windowed = TRUE;

    if (!bShutdownEncodeThread)
        SetWindowPos(hwndProjector, NULL, projectorX, projectorY, projectorWidth, projectorHeight, SWP_SHOWWINDOW);

    ID3D10Texture2D *backBuffer = NULL;
    ID3D10RenderTargetView *target = NULL;

    if (FAILED(sys->factory->CreateSwapChain(sys->d3d, &swapDesc, &projectorSwap))) {
        AppWarning(L"Could not create projector swap chain");
        goto exit;
    }

    if (FAILED(projectorSwap->GetBuffer(0, IID_ID3D10Texture2D, (void**)&backBuffer))) {
        AppWarning(TEXT("Unable to get projector back buffer"));
        goto exit;
    }

    if(FAILED(sys->d3d->CreateRenderTargetView(backBuffer, NULL, &target))) {
        AppWarning(TEXT("Unable to get render view from projector back buffer"));
        goto exit;
    }

    D3D10Texture *tex = new D3D10Texture();
    tex->width        = projectorWidth;
    tex->height       = projectorHeight;
    tex->format       = GS_BGRA;
    tex->texture      = backBuffer;
    tex->renderTarget = target;

    projectorTexture = tex;
    bProjector = true;

exit:
    if (!bProjector) {
        SafeRelease(projectorSwap);
        SafeRelease(backBuffer);
    }

    bPleaseEnableProjector = false;
}

void OBS::EnableProjector(UINT monitorID)
{
    const MonitorInfo &mi = GetMonitor(monitorID);

    projectorMonitorID = monitorID;
    projectorWidth     = mi.rect.right-mi.rect.left;
    projectorHeight    = mi.rect.bottom-mi.rect.top;
    projectorX         = mi.rect.left;
    projectorY         = mi.rect.top;

    bPleaseEnableProjector = true;
}

void OBS::DisableProjector()
{
    SafeRelease(projectorSwap);

    delete projectorTexture;
    projectorTexture = NULL;

    bProjector = false;
    bPleaseDisableProjector = false;

    if (!bShutdownEncodeThread)
        ShowWindow(hwndProjector, SW_HIDE);
}

void OBS::GetThreadHandles (HANDLE *videoThread, HANDLE *encodeThread)
{
    if (hVideoThread)
        *videoThread = hVideoThread;

    if (hEncodeThread)
        *encodeThread = hEncodeThread;
}

NetworkStream* CreateRTMPPublisher();
NetworkStream* CreateDelayedPublisher(DWORD delayTime);
NetworkStream* CreateNullNetwork();

void OBS::RestartNetwork()
{
    OSEnterMutex(App->hStartupShutdownMutex);

    //delete the old one
    App->network.reset();

    //start up a new one
    App->bSentHeaders = false;
    App->network.reset(CreateRTMPPublisher());

    OSLeaveMutex(App->hStartupShutdownMutex);
}
