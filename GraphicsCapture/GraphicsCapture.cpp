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


extern "C" __declspec(dllexport) bool LoadPlugin();
extern "C" __declspec(dllexport) void UnloadPlugin();
extern "C" __declspec(dllexport) CTSTR GetPluginName();
extern "C" __declspec(dllexport) CTSTR GetPluginDescription();

HINSTANCE hinstMain = NULL;
HANDLE textureMutexes[2] = {NULL, NULL};


#define GRAPHICSCAPTURE_CLASSNAME TEXT("GraphicsCapture")

void RefreshWindowList(HWND hwndCombobox, StringList &classList)
{
    SendMessage(hwndCombobox, CB_RESETCONTENT, 0, 0);
    classList.Clear();

    BOOL bCurrentProcessIsWow64 = FALSE;
    IsWow64Process(GetCurrentProcess(), &bCurrentProcessIsWow64);

    HWND hwndCurrent = GetWindow(GetDesktopWindow(), GW_CHILD);
    do
    {
        if(IsWindowVisible(hwndCurrent))
        {
            RECT clientRect;
            GetClientRect(hwndCurrent, &clientRect);

            HWND hwndParent = GetParent(hwndCurrent);

            DWORD exStyles = (DWORD)GetWindowLongPtr(hwndCurrent, GWL_EXSTYLE);
            DWORD styles = (DWORD)GetWindowLongPtr(hwndCurrent, GWL_STYLE);


            if( (exStyles & WS_EX_TOOLWINDOW) == 0 && (styles & WS_CHILD) == 0 && hwndParent == NULL)
            {
                String strWindowName;
                strWindowName.SetLength(GetWindowTextLength(hwndCurrent));
                GetWindowText(hwndCurrent, strWindowName, strWindowName.Length()+1);

                //-------

                DWORD processID;
                GetWindowThreadProcessId(hwndCurrent, &processID);
                if(processID == GetCurrentProcessId())
                    continue;

                TCHAR fileName[MAX_PATH+1];
                scpy(fileName, TEXT("unknown"));

                HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processID);
                if(hProcess)
                {
                    BOOL bTargetProcessIsWow64 = FALSE;
                    IsWow64Process(hProcess, &bTargetProcessIsWow64);

                    DWORD dwSize = MAX_PATH;
                    QueryFullProcessImageName(hProcess, 0, fileName, &dwSize);
                    CloseHandle(hProcess);

                    if(bCurrentProcessIsWow64 != bTargetProcessIsWow64)
                        continue;
                }

                //-------

                String strFileName = fileName;
                strFileName.FindReplace(TEXT("\\"), TEXT("/"));

                String strText;
                strText << TEXT("[") << GetPathFileName(strFileName) << TEXT("]: ") << strWindowName;

                int id = (int)SendMessage(hwndCombobox, CB_ADDSTRING, 0, (LPARAM)strWindowName.Array());
                SendMessage(hwndCombobox, CB_SETITEMDATA, id, (LPARAM)hwndCurrent);

                String strClassName;
                strClassName.SetLength(256);
                GetClassName(hwndCurrent, strClassName.Array(), 255);
                strClassName.SetLength(slen(strClassName));

                classList << strClassName;
            }
        }
    } while (hwndCurrent = GetNextWindow(hwndCurrent, GW_HWNDNEXT));
}

struct ConfigDialogData
{
    XElement *data;
    StringList strWindowClasses;

    UINT cx, cy;
};

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

                SendMessage(GetDlgItem(hwnd, IDC_STRETCHTOSCREEN), BM_SETCHECK, data->GetInt(TEXT("stretchImage")) ? BST_CHECKED : BST_UNCHECKED, 0);

                return TRUE;
            }

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_REFRESH:
                    {
                        ConfigDialogData *info = (ConfigDialogData*)GetWindowLongPtr(hwnd, DWLP_USER);
                        XElement *data = info->data;

                        CTSTR lpWindowName = data->GetString(TEXT("window"));

                        HWND hwndWindowList = GetDlgItem(hwnd, IDC_APPLIST);
                        RefreshWindowList(hwndWindowList, info->strWindowClasses);

                        UINT windowID = 0;
                        if(lpWindowName)
                            windowID = (UINT)SendMessage(hwndWindowList, CB_FINDSTRINGEXACT, -1, (LPARAM)lpWindowName);

                        if(windowID != CB_ERR)
                            SendMessage(hwndWindowList, CB_SETCURSEL, windowID, 0);
                        else
                            SendMessage(hwndWindowList, CB_SETCURSEL, 0, 0);
                    }
                    break;

                case IDOK:
                    {
                        UINT windowID = (UINT)SendMessage(GetDlgItem(hwnd, IDC_APPLIST), CB_GETCURSEL, 0, 0);
                        if(windowID == CB_ERR) windowID = 0;

                        ConfigDialogData *info = (ConfigDialogData*)GetWindowLongPtr(hwnd, DWLP_USER);
                        XElement *data = info->data;

                        if(!info->strWindowClasses.Num())
                            return 0;

                        String strWindow = GetCBText(GetDlgItem(hwnd, IDC_APPLIST), windowID);
                        data->SetString(TEXT("window"),      strWindow);
                        data->SetString(TEXT("windowClass"), info->strWindowClasses[windowID]);

                        data->SetInt(TEXT("stretchImage"), SendMessage(GetDlgItem(hwnd, IDC_STRETCHTOSCREEN), BM_GETCHECK, 0, 0) == BST_CHECKED);
                    }

                case IDCANCEL:
                    EndDialog(hwnd, LOWORD(wParam));
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

    if(DialogBoxParam(hinstMain, MAKEINTRESOURCE(IDD_CONFIG), API->GetMainWindow(), ConfigureDialogProc, (LPARAM)configData) == IDOK)
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
    WNDCLASS wc;
    zero(&wc, sizeof(wc));
    wc.hInstance = hinstMain;
    wc.cbWndExtra = sizeof(LPVOID);
    wc.lpszClassName = RECEIVER_WINDOWCLASS;
    wc.lpfnWndProc = (WNDPROC)GraphicsCaptureSource::ReceiverWindowProc;

    if(!RegisterClass(&wc))
    {
        AppWarning(TEXT("Could not register window class for graphics plugin"));
        return false;
    }

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
    if(dwReason == DLL_PROCESS_ATTACH)
        hinstMain = hInst;

    return TRUE;
}
