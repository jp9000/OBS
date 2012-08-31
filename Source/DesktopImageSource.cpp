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


#define NUM_CAPTURE_TEXTURES 2

class DesktopImageSource : public ImageSource
{
    Texture *renderTextures[NUM_CAPTURE_TEXTURES];
    Texture *lastRendered;

    int      width, height;
    RECT     captureRect;
    HDC      hCaptureDC;
    UINT     frameTime;
    int      curCaptureTexture;
    XElement *data;

public:
    DesktopImageSource(UINT frameTime, XElement *data)
    {
        traceIn(DesktopImageSource::DesktopImageSource);

        this->data = data;
        UpdateSettings();

        curCaptureTexture = 0;
        this->frameTime = frameTime;

        //-------------------------------------------------------

        for(UINT i=0; i<NUM_CAPTURE_TEXTURES; i++)
            renderTextures[i] = CreateGDITexture(width, height);

        //-------------------------------------------------------

        hCaptureDC = GetDC(NULL);

        traceOut;
    }

    ~DesktopImageSource()
    {
        traceIn(DesktopImageSource::~DesktopImageSource);

        ReleaseDC(NULL, hCaptureDC);

        for(int i=0; i<NUM_CAPTURE_TEXTURES; i++)
            delete renderTextures[i];

        traceOut;
    }

    void Preprocess()
    {
        traceIn(DesktopImageSource::Preprocess);

        Texture *captureTexture = renderTextures[curCaptureTexture];

        HDC hDC;
        if(captureTexture->GetDC(hDC))
        {
            //----------------------------------------------------------
            // capture screen

            CURSORINFO ci;
            zero(&ci, sizeof(ci));
            ci.cbSize = sizeof(ci);
            BOOL bMouseCaptured;

            bMouseCaptured = GetCursorInfo(&ci);

            //CAPTUREBLT))  //necessary?  causes mouse flicker.  I haven't seen anything that doesn't display without it yet.
            if(!BitBlt(hDC, 0, 0, width, height, hCaptureDC, captureRect.left, captureRect.top, SRCCOPY))
            {
                int chi = GetLastError();
                AppWarning(TEXT("Capture BitBlt failed..  just so you know"));
            }

            //----------------------------------------------------------
            // capture mouse

            if(bMouseCaptured)
            {
                if(ci.flags & CURSOR_SHOWING)
                {
                    HICON hIcon = CopyIcon(ci.hCursor);

                    if(hIcon)
                    {
                        ICONINFO ii;
                        if(GetIconInfo(hIcon, &ii))
                        {
                            int x = ci.ptScreenPos.x - int(ii.xHotspot) - captureRect.left;
                            int y = ci.ptScreenPos.y - int(ii.yHotspot) - captureRect.top;

                            DrawIcon(hDC, x, y, hIcon);

                            DeleteObject(ii.hbmColor);
                            DeleteObject(ii.hbmMask);
                        }

                        DestroyIcon(hIcon);
                    }
                }
            }

            captureTexture->ReleaseDC();
        }
        else
            AppWarning(TEXT("Failed to get DC from capture surface"));

        lastRendered = captureTexture;

        if(++curCaptureTexture == NUM_CAPTURE_TEXTURES)
            curCaptureTexture = 0;

        traceOut;
    }

    void Render(const Vect2 &pos, const Vect2 &size)
    {
        traceIn(DesktopImageSource::Render);

        EnableBlending(FALSE);

        //-------------------------------

        DrawSprite(lastRendered, pos.x, pos.y, pos.x+size.x, pos.y+size.y);

        //-------------------------------

        EnableBlending(TRUE);

        traceOut;
    }

    Vect2 GetSize() const
    {
        return Vect2(float(width), float(height));
    }

    void UpdateSettings()
    {
        int x  = data->GetInt(TEXT("captureX"));
        int y  = data->GetInt(TEXT("captureY"));
        int cx = data->GetInt(TEXT("captureCX"), 100);
        int cy = data->GetInt(TEXT("captureCY"), 100);

        captureRect.left   = x;
        captureRect.top    = y;
        captureRect.right  = x+cx;
        captureRect.bottom = y+cy;

        width  = cx;
        height = cy;
    }
};

ImageSource* STDCALL CreateDesktopSource(XElement *data)
{
    if(!data)
        return NULL;

    return new DesktopImageSource(App->GetFrameTime(), data);
}

/*void RefreshWindowList(HWND hwndCombobox)
{
    SendMessage(hwndCombobox, CB_RESETCONTENT, 0, 0);

    HWND hwndCurrent = GetWindow(GetDesktopWindow(), GW_CHILD);
    do
    {
        if(IsWindowVisible(hwndCurrent) && !IsIconic(hwndCurrent))
        {
            RECT clientRect;
            GetClientRect(hwndCurrent, &clientRect);

            DWORD exStyles = GetWindowLongPtr(hwndCurrent, GWL_EXSTYLE);
            DWORD styles = GetWindowLongPtr(hwndCurrent, GWL_STYLE);

            if( (exStyles & WS_EX_TOOLWINDOW) == 0 && (styles & WS_CHILD) == 0 &&
                clientRect.bottom != 0 && clientRect.right != 0)
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
                    DWORD dwSize = MAX_PATH;
                    QueryFullProcessImageName(hProcess, 0, fileName, &dwSize);
                    CloseHandle(hProcess);
                }

                //-------

                String strFileName = fileName;
                strFileName.FindReplace(TEXT("\\"), TEXT("/"));

                String strText;
                strText << TEXT("[") << GetPathFileName(strFileName) << TEXT("]: ") << strWindowName;

                int id = SendMessage(hwndCombobox, CB_ADDSTRING, 0, (LPARAM)strText.Array());
                SendMessage(hwndCombobox, CB_SETITEMDATA, id, (LPARAM)hwndCurrent);
            }
        }
    } while (hwndCurrent = GetNextWindow(hwndCurrent, GW_HWNDNEXT));
}*/

INT_PTR CALLBACK ConfigDesktopSourceProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)lParam);
                LocalizeWindow(hwnd);

                HWND hwndTemp = GetDlgItem(hwnd, IDC_MONITOR);
                for(UINT i=0; i<App->NumMonitors(); i++)
                    SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)UIntString(i+1).Array());
                SendMessage(hwndTemp, CB_SETCURSEL, 0, 0);

                return TRUE;
            }

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDOK:
                    {
                        UINT id = (UINT)SendMessage(GetDlgItem(hwnd, IDC_MONITOR), CB_GETCURSEL, 0, 0);
                        const MonitorInfo &monitor = App->GetMonitor(id);

                        XElement *data = (XElement*)GetWindowLongPtr(hwnd, DWLP_USER);
                        data->SetInt(TEXT("captureX"),  monitor.rect.left);
                        data->SetInt(TEXT("captureY"),  monitor.rect.top);
                        data->SetInt(TEXT("captureCX"), monitor.rect.right-monitor.rect.left);
                        data->SetInt(TEXT("captureCY"), monitor.rect.bottom-monitor.rect.top);
                    }

                case IDCANCEL:
                    EndDialog(hwnd, LOWORD(wParam));
                    break;
            }
    }

    return FALSE;
}

bool STDCALL ConfigureDesktopSource(XElement *element, bool bInitialize)
{
    if(!element)
    {
        AppWarning(TEXT("ConfigureDesktopSource: NULL element"));
        return false;
    }

    //do not allow more than one desktop source per scene
    if(bInitialize)
    {
        XElement *sources = App->GetSceneElement()->GetElement(TEXT("sources"));
        if(sources)
        {
            UINT numSources = sources->NumElements();
            for(UINT i=0; i<numSources; i++)
            {
                XElement *source = sources->GetElementByID(i);
                if(source == element)
                    continue;

                if(scmpi(source->GetString(TEXT("class")), TEXT("DesktopImageSource")) == 0)
                {
                    MessageBox(hwndMain, Str("DesktopImageSource.CannotDuplicate"), NULL, 0);
                    return false;
                }
            }
        }
    }

    XElement *data = element->GetElement(TEXT("data"));
    if(!data)
        data = element->CreateElement(TEXT("data"));

    if(DialogBoxParam(hinstMain, MAKEINTRESOURCE(IDD_CONFIGUREDESKTOPSOURCE), hwndMain, ConfigDesktopSourceProc, (LPARAM)data) == IDOK)
    {
        if(bInitialize)
        {
            element->SetInt(TEXT("cx"), data->GetInt(TEXT("captureCX")));
            element->SetInt(TEXT("cy"), data->GetInt(TEXT("captureCY")));
        }
        return true;
    }

    return false;
}
