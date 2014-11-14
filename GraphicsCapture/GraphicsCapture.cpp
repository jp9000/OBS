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


#include "GraphicsCapture.h"
#include <dwmapi.h>


extern "C" __declspec(dllexport) bool LoadPlugin();
extern "C" __declspec(dllexport) void UnloadPlugin();
extern "C" __declspec(dllexport) CTSTR GetPluginName();
extern "C" __declspec(dllexport) CTSTR GetPluginDescription();

HINSTANCE hinstMain = NULL;
HANDLE textureMutexes[2] = {NULL, NULL};


#define GRAPHICSCAPTURE_CLASSNAME TEXT("GraphicsCapture")


struct WindowInfo
{
    String strClass;
    String strExecutable;
    BOOL bRequiresAdmin;
    BOOL bFoundHookableModule;
};

struct ConfigDialogData
{
    CTSTR lpName;
    XElement *data;
    List<WindowInfo> windowData;
    StringList adminWindows;

    UINT cx, cy;

    inline void ClearData()
    {
        for(UINT i=0; i<windowData.Num(); i++)
        {
            windowData[i].strClass.Clear();
            windowData[i].strExecutable.Clear();
        }
        windowData.Clear();
        adminWindows.Clear();
    }

    inline ~ConfigDialogData()
    {
        for(UINT i=0; i<windowData.Num(); i++)
        {
            windowData[i].strClass.Clear();
            windowData[i].strExecutable.Clear();
        }
    }
};

typedef HANDLE (WINAPI *OPPROC) (DWORD, BOOL, DWORD);

void RefreshWindowList(HWND hwndCombobox, ConfigDialogData &configData)
{
    SendMessage(hwndCombobox, CB_RESETCONTENT, 0, 0);
    configData.ClearData();

    HWND hwndCurrent = GetWindow(GetDesktopWindow(), GW_CHILD);
    do
    {
        if(IsWindowVisible(hwndCurrent))
        {
            RECT clientRect;
            GetClientRect(hwndCurrent, &clientRect);

            String strWindowName;
            strWindowName.SetLength(GetWindowTextLength(hwndCurrent));
            GetWindowText(hwndCurrent, strWindowName, strWindowName.Length()+1);

            HWND hwndParent = GetParent(hwndCurrent);

            DWORD exStyles = (DWORD)GetWindowLongPtr(hwndCurrent, GWL_EXSTYLE);
            DWORD styles = (DWORD)GetWindowLongPtr(hwndCurrent, GWL_STYLE);

            if (strWindowName.IsValid() && sstri(strWindowName, L"battlefield") != nullptr)
                exStyles &= ~WS_EX_TOOLWINDOW;

            if((exStyles & WS_EX_TOOLWINDOW) == 0 && (styles & WS_CHILD) == 0 /*&& hwndParent == NULL*/)
            {
                BOOL bFoundModule = true;
                DWORD processID;
                GetWindowThreadProcessId(hwndCurrent, &processID);
                if(processID == GetCurrentProcessId())
                    continue;

                TCHAR fileName[MAX_PATH+1];
                scpy(fileName, TEXT("unknown"));

                char pOPStr[12];
                mcpy(pOPStr, "NpflUvhel{x", 12);
                for (int i=0; i<11; i++) pOPStr[i] ^= i^1;

                OPPROC pOpenProcess = (OPPROC)GetProcAddress(GetModuleHandle(TEXT("KERNEL32")), pOPStr);

                HANDLE hProcess = (*pOpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, processID);
                if(hProcess)
                {
                    DWORD dwSize = MAX_PATH;
                    QueryFullProcessImageName(hProcess, 0, fileName, &dwSize);

                    StringList moduleList;
                    if (OSGetLoadedModuleList(hProcess, moduleList) && moduleList.Num())
                    {
                        //note: this doesn't actually work cross-bit, but we may as well make as much use of
                        //the data we can get.
                        bFoundModule = false;
                        for(UINT i=0; i<moduleList.Num(); i++)
                        {
                            CTSTR moduleName = moduleList[i];

                            if (!scmp(moduleName, TEXT("d3d9.dll")) ||
                                !scmp(moduleName, TEXT("d3d10.dll")) ||
                                !scmp(moduleName, TEXT("d3d10_1.dll")) ||
                                !scmp(moduleName, TEXT("d3d11.dll")) ||
                                !scmp(moduleName, TEXT("dxgi.dll")) ||
                                !scmp(moduleName, TEXT("d3d8.dll")) ||
                                !scmp(moduleName, TEXT("opengl32.dll")))
                            {
                                bFoundModule = true;
                                break;
                            }
                        }

                        if (!bFoundModule)
                        {
                            CloseHandle(hProcess);
                            continue;
                        }
                    }

                    CloseHandle(hProcess);
                }
                else
                {
                    hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processID);
                    if(hProcess)
                    {
                        configData.adminWindows << strWindowName;
                        CloseHandle(hProcess);
                    }

                    continue;
                }

                //-------

                String strFileName = fileName;
                strFileName.FindReplace(TEXT("\\"), TEXT("/"));

                String strText;
                strText << TEXT("[") << GetPathFileName(strFileName);
                strText << TEXT("]: ") << strWindowName;

                int id = (int)SendMessage(hwndCombobox, CB_ADDSTRING, 0, (LPARAM)strText.Array());
                SendMessage(hwndCombobox, CB_SETITEMDATA, id, (LPARAM)hwndCurrent);

                String strClassName;
                strClassName.SetLength(256);
                GetClassName(hwndCurrent, strClassName.Array(), 255);
                strClassName.SetLength(slen(strClassName));

                TCHAR *baseExeName;
                baseExeName = wcsrchr(fileName, '\\');
                if (!baseExeName)
                    baseExeName = fileName;
                else
                    baseExeName++;

                WindowInfo &info    = *configData.windowData.CreateNew();
                info.strClass       = strClassName;
                info.strExecutable  = baseExeName;
                info.bRequiresAdmin = false; //todo: add later
                info.bFoundHookableModule = bFoundModule;

                info.strExecutable.MakeLower();
            }
        }
    } while (hwndCurrent = GetNextWindow(hwndCurrent, GW_HWNDNEXT));

    if(OSGetVersion() < 8)
    {
        BOOL isCompositionEnabled = FALSE;
        
        DwmIsCompositionEnabled(&isCompositionEnabled);
        
        if(isCompositionEnabled)
        {
            String strText;
            strText << TEXT("[DWM]: ") << Str("Sources.SoftwareCaptureSource.MonitorCapture");

            int id = (int)SendMessage(hwndCombobox, CB_ADDSTRING, 0, (LPARAM)strText.Array());
            SendMessage(hwndCombobox, CB_SETITEMDATA, id, (LPARAM)NULL);

            WindowInfo &info = *configData.windowData.CreateNew();
            info.strClass = TEXT("Dwm");
            info.strExecutable = TEXT("dwm.exe");
            info.bRequiresAdmin = false; //todo: add later
            info.bFoundHookableModule = true;
        }
    }

    // preserve the last used settings in case the target isn't open any more, prevents
    // Properties -> OK selecting a new random target.

    String oldWindow = configData.data->GetString(TEXT("window"));
    String oldClass = configData.data->GetString(TEXT("windowClass"));
    String oldExe = configData.data->GetString(TEXT("executable"));

    UINT windowID = (UINT)SendMessage(hwndCombobox, CB_FINDSTRINGEXACT, -1, (LPARAM)oldWindow.Array());

    if (windowID == CB_ERR && oldWindow.IsValid() && oldClass.IsValid())
    {
        int id = (int)SendMessage(hwndCombobox, CB_ADDSTRING, 0, (LPARAM)oldWindow.Array());
        SendMessage(hwndCombobox, CB_SETITEMDATA, id, (LPARAM)NULL);

        WindowInfo &info = *configData.windowData.CreateNew();
        info.strClass = oldClass;
        info.strExecutable = oldExe;
        info.bRequiresAdmin = false; //todo: add later
        info.bFoundHookableModule = true;
    }
}

int SetSliderText(HWND hwndParent, int controlSlider, int controlText)
{
    HWND hwndSlider = GetDlgItem(hwndParent, controlSlider);
    HWND hwndText   = GetDlgItem(hwndParent, controlText);

    int sliderVal = (int)SendMessage(hwndSlider, TBM_GETPOS, 0, 0);
    float floatVal = float(sliderVal)*0.01f;

    SetWindowText(hwndText, FormattedString(TEXT("%.02f"), floatVal));

    return sliderVal;
}

INT_PTR CALLBACK ConfigureDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                ConfigDialogData *info = (ConfigDialogData*)lParam;
                XElement *data = info->data;

                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)lParam);
                LocalizeWindow(hwnd);

                //--------------------------------------------

                SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_REFRESH, BN_CLICKED), (LPARAM)GetDlgItem(hwnd, IDC_APPLIST));

                //--------------------------------------------

                BOOL bCaptureMouse = data->GetInt(TEXT("captureMouse"), 1);
                BOOL bStretchImage = data->GetInt(TEXT("stretchImage"));
                BOOL bAlphaBlend = data->GetInt(TEXT("alphaBlend"));
                SendMessage(GetDlgItem(hwnd, IDC_STRETCHTOSCREEN),    BM_SETCHECK, bStretchImage ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hwnd, IDC_ALPHABLEND),         BM_SETCHECK, bAlphaBlend ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hwnd, IDC_IGNOREASPECT),       BM_SETCHECK, data->GetInt(TEXT("ignoreAspect")) ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hwnd, IDC_CAPTUREMOUSE),       BM_SETCHECK, bCaptureMouse                      ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hwnd, IDC_INVERTMOUSEONCLICK), BM_SETCHECK, data->GetInt(TEXT("invertMouse"))  ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hwnd, IDC_USESAFEHOOK),        BM_SETCHECK, data->GetInt(TEXT("safeHook"))     ? BST_CHECKED : BST_UNCHECKED, 0);
                EnableWindow(GetDlgItem(hwnd, IDC_INVERTMOUSEONCLICK), bCaptureMouse);
                EnableWindow(GetDlgItem(hwnd, IDC_IGNOREASPECT), bStretchImage);

                //------------------------------------------

                bool bUseHotkey = data->GetInt(TEXT("useHotkey"), 0) != 0;

                EnableWindow(GetDlgItem(hwnd, IDC_APPLIST),     !bUseHotkey);
                EnableWindow(GetDlgItem(hwnd, IDC_REFRESH),     !bUseHotkey);
                EnableWindow(GetDlgItem(hwnd, IDC_HOTKEY),       bUseHotkey);

                DWORD hotkey = data->GetInt(TEXT("hotkey"), VK_F12);
                SendMessage(GetDlgItem(hwnd, IDC_HOTKEY), HKM_SETHOTKEY, hotkey, 0);

                SendMessage(GetDlgItem(hwnd, IDC_SELECTAPP), BM_SETCHECK, bUseHotkey ? BST_UNCHECKED : BST_CHECKED, 0);
                SendMessage(GetDlgItem(hwnd, IDC_USEHOTKEY), BM_SETCHECK, bUseHotkey ? BST_CHECKED : BST_UNCHECKED, 0);

                //------------------------------------------

                int gammaVal = data->GetInt(TEXT("gamma"), 100);

                HWND hwndTemp = GetDlgItem(hwnd, IDC_GAMMA);
                SendMessage(hwndTemp, TBM_CLEARTICS, FALSE, 0);
                SendMessage(hwndTemp, TBM_SETRANGE, FALSE, MAKELPARAM(50, 175));
                SendMessage(hwndTemp, TBM_SETTIC, 0, 100);
                SendMessage(hwndTemp, TBM_SETPOS, TRUE, gammaVal);

                SetSliderText(hwnd, IDC_GAMMA, IDC_GAMMAVAL);

                return TRUE;
            }

        case WM_HSCROLL:
            {
                if(GetDlgCtrlID((HWND)lParam) == IDC_GAMMA)
                {
                    int gamma = SetSliderText(hwnd, IDC_GAMMA, IDC_GAMMAVAL);

                    ConfigDialogData *info = (ConfigDialogData*)GetWindowLongPtr(hwnd, DWLP_USER);
                    ImageSource *source = API->GetSceneImageSource(info->lpName);
                    if(source)
                        source->SetInt(TEXT("gamma"), gamma);
                }
            }
            break;

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_CAPTUREMOUSE:
                    {
                        BOOL bCaptureMouse = SendMessage(GetDlgItem(hwnd, IDC_CAPTUREMOUSE), BM_GETCHECK, 0, 0) == BST_CHECKED;
                        EnableWindow(GetDlgItem(hwnd, IDC_INVERTMOUSEONCLICK), bCaptureMouse);
                    }
                    break;

                case IDC_SELECTAPP:
                case IDC_USEHOTKEY:
                    if (HIWORD(wParam) == BN_CLICKED)
                    {
                        bool bUseHotkey = LOWORD(wParam) == IDC_USEHOTKEY;

                        EnableWindow(GetDlgItem(hwnd, IDC_APPLIST),     !bUseHotkey);
                        EnableWindow(GetDlgItem(hwnd, IDC_REFRESH),     !bUseHotkey);
                        EnableWindow(GetDlgItem(hwnd, IDC_HOTKEY),       bUseHotkey);
                    }
                    break;

                case IDC_STRETCHTOSCREEN:
                    {
                        BOOL bStretchToScreen = SendMessage(GetDlgItem(hwnd, IDC_STRETCHTOSCREEN), BM_GETCHECK, 0, 0) == BST_CHECKED;
                        EnableWindow(GetDlgItem(hwnd, IDC_IGNOREASPECT), bStretchToScreen);
                    }
                    break;

                case IDC_REFRESH:
                    {
                        ConfigDialogData *info = (ConfigDialogData*)GetWindowLongPtr(hwnd, DWLP_USER);
                        XElement *data = info->data;

                        CTSTR lpWindowName = data->GetString(TEXT("window"));

                        HWND hwndWindowList = GetDlgItem(hwnd, IDC_APPLIST);
                        RefreshWindowList(hwndWindowList, *info);

                        UINT windowID = 0;
                        if(lpWindowName)
                            windowID = (UINT)SendMessage(hwndWindowList, CB_FINDSTRINGEXACT, -1, (LPARAM)lpWindowName);

                        if(windowID != CB_ERR)
                            SendMessage(hwndWindowList, CB_SETCURSEL, windowID, 0);
                        else
                            SendMessage(hwndWindowList, CB_SETCURSEL, 0, 0);

                        String strInfoText;

                        if(info->adminWindows.Num())
                        {
                            strInfoText << Str("Sources.GameCaptureSource.RequiresAdmin") << TEXT("\r\n");

                            for(UINT i=0; i<info->adminWindows.Num(); i++)
                                strInfoText << info->adminWindows[i] << TEXT("\r\n");
                        }

                        SetWindowText(GetDlgItem(hwnd, IDC_INFO), strInfoText);
                    }
                    break;

                case IDOK:
                    {
                        UINT windowID = (UINT)SendMessage(GetDlgItem(hwnd, IDC_APPLIST), CB_GETCURSEL, 0, 0);
                        if(windowID == CB_ERR) windowID = 0;

                        ConfigDialogData *info = (ConfigDialogData*)GetWindowLongPtr(hwnd, DWLP_USER);
                        XElement *data = info->data;

                        if(!info->windowData.Num())
                            return 0;

                        String strWindow = GetCBText(GetDlgItem(hwnd, IDC_APPLIST), windowID);
                        data->SetString(TEXT("window"),      strWindow);
                        data->SetString(TEXT("windowClass"), info->windowData[windowID].strClass);
                        data->SetString(TEXT("executable"), info->windowData[windowID].strExecutable);

                        data->SetInt(TEXT("stretchImage"), SendMessage(GetDlgItem(hwnd, IDC_STRETCHTOSCREEN),    BM_GETCHECK, 0, 0) == BST_CHECKED);
                        data->SetInt(TEXT("alphaBlend"),   SendMessage(GetDlgItem(hwnd, IDC_ALPHABLEND),         BM_GETCHECK, 0, 0) == BST_CHECKED);
                        data->SetInt(TEXT("ignoreAspect"), SendMessage(GetDlgItem(hwnd, IDC_IGNOREASPECT),       BM_GETCHECK, 0, 0) == BST_CHECKED);
                        data->SetInt(TEXT("captureMouse"), SendMessage(GetDlgItem(hwnd, IDC_CAPTUREMOUSE),       BM_GETCHECK, 0, 0) == BST_CHECKED);
                        data->SetInt(TEXT("invertMouse"),  SendMessage(GetDlgItem(hwnd, IDC_INVERTMOUSEONCLICK), BM_GETCHECK, 0, 0) == BST_CHECKED);
                        data->SetInt(TEXT("safeHook"),     SendMessage(GetDlgItem(hwnd, IDC_USESAFEHOOK),        BM_GETCHECK, 0, 0) == BST_CHECKED);

                        data->SetInt(TEXT("useHotkey"),    SendMessage(GetDlgItem(hwnd, IDC_USEHOTKEY),          BM_GETCHECK, 0, 0) == BST_CHECKED);
                        data->SetInt(TEXT("hotkey"),       (DWORD)SendMessage(GetDlgItem(hwnd, IDC_HOTKEY), HKM_GETHOTKEY, 0, 0));

                        data->SetInt(TEXT("gamma"),        (int)SendMessage(GetDlgItem(hwnd, IDC_GAMMA), TBM_GETPOS, 0, 0));

                        EndDialog(hwnd, LOWORD(wParam));
                    }
                    break;

                case IDCANCEL:
                    {
                        ConfigDialogData *info = (ConfigDialogData*)GetWindowLongPtr(hwnd, DWLP_USER);
                        ImageSource *source = API->GetSceneImageSource(info->lpName);
                        XElement *data = info->data;

                        if(source)
                        {
                            source->SetInt(TEXT("gamma"), data->GetInt(TEXT("gamma"), 100));
                        }

                        EndDialog(hwnd, LOWORD(wParam));
                    }
            }
            break;

        case WM_CLOSE:
            EndDialog(hwnd, IDCANCEL);
    }
    return 0;
}

bool STDCALL ConfigureGraphicsCaptureSource(XElement *element, bool bCreating)
{
    if(!element)
    {
        AppWarning(TEXT("ConfigureGraphicsCaptureSource: NULL element"));
        return false;
    }

    XElement *data = element->GetElement(TEXT("data"));
    if(!data)
        data = element->CreateElement(TEXT("data"));

    ConfigDialogData *configData = new ConfigDialogData;
    configData->data = data;
    configData->lpName = element->GetName();

    if(OBSDialogBox(hinstMain, MAKEINTRESOURCE(IDD_CONFIG), API->GetMainWindow(), ConfigureDialogProc, (LPARAM)configData) == IDOK)
    {
        UINT width, height;
        API->GetBaseSize(width, height);
        element->SetInt(TEXT("cx"), width);
        element->SetInt(TEXT("cy"), height);

        delete configData;
        return true;
    }

    delete configData;
    return false;
}

ImageSource* STDCALL CreateGraphicsCaptureSource(XElement *data)
{
    GraphicsCaptureSource *source = new GraphicsCaptureSource;
    if(!source->Init(data))
    {
        delete source;
        return NULL;
    }

    return source;
}

bool LoadPlugin()
{
    InitHotkeyExControl(hinstMain);

    textureMutexes[0] = CreateMutex(NULL, NULL, TEXTURE_MUTEX1);
    if(!textureMutexes[0])
    {
        AppWarning(TEXT("Could not create texture mutex 1, GetLastError = %u"), GetLastError());
        return false;
    }

    textureMutexes[1] = CreateMutex(NULL, NULL, TEXTURE_MUTEX2);
    if(!textureMutexes[1])
    {
        AppWarning(TEXT("Could not create texture mutex 2, GetLastError = %u"), GetLastError());
        return false;
    }

    API->RegisterImageSourceClass(GRAPHICSCAPTURE_CLASSNAME, Str("Sources.GameCaptureSource"), (OBSCREATEPROC)CreateGraphicsCaptureSource, (OBSCONFIGPROC)ConfigureGraphicsCaptureSource);

    return true;
}

void UnloadPlugin()
{
    CloseHandle(textureMutexes[0]);
    CloseHandle(textureMutexes[1]);
}

CTSTR GetPluginName()
{
    return Str("Sources.GameCaptureSource.PluginName");
}

CTSTR GetPluginDescription()
{
    return Str("Sources.GameCaptureSource.PluginDescription");
}

BOOL CALLBACK DllMain(HINSTANCE hInst, DWORD dwReason, LPVOID lpBla)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
#if defined _M_X64 && _MSC_VER == 1800
        //workaround AVX2 bug in VS2013, http://connect.microsoft.com/VisualStudio/feedback/details/811093
        _set_FMA3_enable(0);
#endif
        hinstMain = hInst;
    }

    return TRUE;
}
