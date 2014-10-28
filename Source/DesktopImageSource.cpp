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
#include <dwmapi.h>


#define NUM_CAPTURE_TEXTURES 2

class DesktopImageSource : public ImageSource
{
    Texture *renderTextures[NUM_CAPTURE_TEXTURES];
    Texture *lastRendered;

    UINT     captureType;
    String   strWindow, strWindowClass;
    BOOL     bClientCapture, bCaptureMouse, bCaptureLayered;
    HWND     hwndFoundWindow;

    Shader   *colorKeyShader, *alphaIgnoreShader;

    int      width, height;
    RECT     captureRect;
    UINT     frameTime;
    int      curCaptureTexture;
    XElement *data;

    UINT     warningID;

    bool     bUseColorKey, bUsePointFiltering;
    DWORD    keyColor;
    UINT     keySimilarity, keyBlend;

    UINT     opacity;
    int      gamma;

    float    rotateDegrees;

    //-------------------------
    // stuff for compatibility mode
    bool     bCompatibilityMode;
    HDC      hdcCompatible;
    HBITMAP  hbmpCompatible, hbmpOld;
    BYTE     *captureBits;

    //-------------------------
    // win 8 capture stuff
    Texture  *cursorTexture;
    int      xHotspot, yHotspot;
    UINT     monitor;
    UINT     deviceOutputID;
    float    retryAcquire;
    bool     bWindows8MonitorCapture;
    MonitorInfo monitorData;
    bool     bMouseCaptured;
    POINT    cursorPos;
    HCURSOR  hCurrentCursor;
    bool     bInInit;

    OutputDuplicator *duplicator;

public:
    DesktopImageSource(UINT frameTime, XElement *data)
    {
        this->data = data;
        duplicator = NULL;
        
        bInInit = true;

        UpdateSettings();

        bInInit = false;

        curCaptureTexture = 0;
        this->frameTime = frameTime;

        colorKeyShader      = CreatePixelShaderFromFile(TEXT("shaders\\ColorKey_RGB.pShader"));
        alphaIgnoreShader   = CreatePixelShaderFromFile(TEXT("shaders\\AlphaIgnore.pShader"));

        if(captureType == 0)
            Log(TEXT("Using Monitor Capture"));
        else if(captureType == 1)
            Log(TEXT("Using Window Capture"));
    }

    ~DesktopImageSource()
    {
        for(int i=0; i<NUM_CAPTURE_TEXTURES; i++)
            delete renderTextures[i];

        if(warningID)
            App->RemoveStreamInfo(warningID);

        delete duplicator;
        delete cursorTexture;
        delete alphaIgnoreShader;
        delete colorKeyShader;

        if(bCompatibilityMode)
        {
            SelectObject(hdcCompatible, hbmpOld);
            DeleteDC(hdcCompatible);
            DeleteObject(hbmpCompatible);
        }
    }

    void BeginScene()
    {
        if(bWindows8MonitorCapture && !duplicator)
            duplicator = GS->CreateOutputDuplicator(deviceOutputID);
    }

    void EndScene()
    {
        if(bWindows8MonitorCapture && duplicator)
        {
            delete duplicator;
            duplicator = NULL;
        }
    }

    //----------------------------------------------------------------------------------
    // TODO  - have win8 monitor capture behave even when using it as a global source

    void GlobalSourceEnterScene()
    {
    }
    
    void GlobalSourceLeaveScene()
    {
    }
    
    //----------------------------------------------------------------------------------

    void Tick(float fSeconds)
    {
        if(bWindows8MonitorCapture && !duplicator)
        {
            retryAcquire += fSeconds;
            if(retryAcquire > 1.0f)
            {
                retryAcquire = 0.0f;

                lastRendered = NULL;
                duplicator = GS->CreateOutputDuplicator(deviceOutputID);
            }
        }
    }

    void PreprocessWindows8MonitorCapture()
    {
        //----------------------------------------------------------
        // capture monitor

        if(duplicator)
        {
            switch(duplicator->AcquireNextFrame(0))
            {
                case DuplicatorInfo_Lost:
                    {
                        delete duplicator;
                        lastRendered = NULL;
                        duplicator = GS->CreateOutputDuplicator(deviceOutputID);
                        return;
                    }

                case DuplicatorInfo_Error:
                    delete duplicator;
                    duplicator = NULL;
                    lastRendered = NULL;
                    return;

                case DuplicatorInfo_Timeout:
                    return;
            }

            lastRendered = duplicator->GetCopyTexture();
        }

        //----------------------------------------------------------
        // capture mouse

        bMouseCaptured = false;
        if(bCaptureMouse)
        {
            CURSORINFO ci;
            zero(&ci, sizeof(ci));
            ci.cbSize = sizeof(ci);

            if(GetCursorInfo(&ci))
            {
                mcpy(&cursorPos, &ci.ptScreenPos, sizeof(cursorPos));

                if(ci.flags & CURSOR_SHOWING)
                {
                    if(ci.hCursor == hCurrentCursor)
                        bMouseCaptured = true;
                    else
                    {
                        HICON hIcon = CopyIcon(ci.hCursor);
                        hCurrentCursor = ci.hCursor;

                        delete cursorTexture;
                        cursorTexture = NULL;

                        if(hIcon)
                        {
                            ICONINFO ii;
                            if(GetIconInfo(hIcon, &ii))
                            {
                                xHotspot = int(ii.xHotspot);
                                yHotspot = int(ii.yHotspot);

                                UINT width, height;
                                LPBYTE lpData = GetCursorData(hIcon, ii, width, height);
                                if(lpData && width && height)
                                {
                                    cursorTexture = CreateTexture(width, height, GS_BGRA, lpData, FALSE);
                                    if(cursorTexture)
                                        bMouseCaptured = true;

                                    Free(lpData);
                                }

                                DeleteObject(ii.hbmColor);
                                DeleteObject(ii.hbmMask);
                            }

                            DestroyIcon(hIcon);
                        }
                    }
                }
            }
        }
    }

    HDC GetCurrentHDC()
    {
        HDC hDC = NULL;

        Texture *captureTexture = renderTextures[curCaptureTexture];

        if(bCompatibilityMode)
        {
            hDC = hdcCompatible;
            //zero(captureBits, width*height*4);
        }
        else if(captureTexture)
            captureTexture->GetDC(hDC);

        return hDC;
    }

    void ReleaseCurrentHDC(HDC hDC)
    {
        Texture *captureTexture = renderTextures[curCaptureTexture];

        if(!bCompatibilityMode)
            captureTexture->ReleaseDC();
    }

    void EndPreprocess()
    {
        if(bCompatibilityMode)
        {
            renderTextures[0]->SetImage(captureBits, GS_IMAGEFORMAT_BGRA, width*4);
            lastRendered = renderTextures[0];
        }
        else
        {
            lastRendered = renderTextures[curCaptureTexture];

            if(++curCaptureTexture == NUM_CAPTURE_TEXTURES)
                curCaptureTexture = 0;
        }
    }

    void Preprocess()
    {
        if(bWindows8MonitorCapture)
        {
            PreprocessWindows8MonitorCapture();
            return;
        }

        HDC hDC;
        if(hDC = GetCurrentHDC())
        {
            //----------------------------------------------------------
            // capture screen

            CURSORINFO ci;
            zero(&ci, sizeof(ci));
            ci.cbSize = sizeof(ci);
            BOOL bMouseCaptured;

            bMouseCaptured = bCaptureMouse && GetCursorInfo(&ci);

            bool bWindowNotFound = false;
            HWND hwndCapture = NULL;
            if(captureType == 1)
            {
                if(hwndFoundWindow && IsWindow(hwndFoundWindow))
                {
                    TCHAR lpClassName[256];
                    BOOL bSuccess = GetClassName(hwndFoundWindow, lpClassName, 255);

                    if(bSuccess && scmpi(lpClassName, strWindowClass) == 0)
                        hwndCapture = hwndFoundWindow;
                    else
                    {
                        hwndCapture = FindWindow(strWindowClass, strWindow);
                        if(!hwndCapture)
                            hwndCapture = FindWindow(strWindowClass, NULL);
                    }
                }
                else
                {
                    hwndCapture = FindWindow(strWindowClass, strWindow);
                    if(!hwndCapture)
                        hwndCapture = FindWindow(strWindowClass, NULL);
                }

                //note to self - hwndFoundWindow is used to make sure it doesn't pick up another window unintentionally if the title changes
                hwndFoundWindow = hwndCapture;

                if(!hwndCapture)
                    bWindowNotFound = true;
                if(hwndCapture && (IsIconic(hwndCapture) || !IsWindowVisible(hwndCapture)))
                {
                    ReleaseCurrentHDC(hDC);
                    //bWindowMinimized = true;

                    if(!warningID)
                    {
                        String strWarning;

                        strWarning << Str("Sources.SoftwareCaptureSource.WindowMinimized");
                        warningID = App->AddStreamInfo(strWarning, bWindowNotFound ? StreamInfoPriority_High : StreamInfoPriority_Medium);
                    }
                    return;
                }
                else if(!bWindowNotFound)
                {
                    if(warningID)
                    {
                        App->RemoveStreamInfo(warningID);
                        warningID = 0;
                    }
                }
            }

            HDC hCaptureDC;
            if(hwndCapture && captureType == 1 && !bClientCapture)
                hCaptureDC = GetWindowDC(hwndCapture);
            else
                hCaptureDC = GetDC(hwndCapture);

            if(bWindowNotFound)
            {
                RECT rc = {0, 0, width, height};
                FillRect(hDC, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

                if(!warningID)
                    warningID = App->AddStreamInfo(Str("Sources.SoftwareCaptureSource.WindowNotFound"), StreamInfoPriority_High);
            }
            else
            {
                //CAPTUREBLT causes mouse flicker, so make capturing layered optional
                if(!BitBlt(hDC, 0, 0, width, height, hCaptureDC, captureRect.left, captureRect.top, bCaptureLayered ? SRCCOPY|CAPTUREBLT : SRCCOPY))
                {
                    RUNONCE AppWarning(TEXT("Capture BitBlt failed (%d)..  just so you know"), GetLastError());
                }
            }

            ReleaseDC(hwndCapture, hCaptureDC);

            //----------------------------------------------------------
            // capture mouse

            if(bMouseCaptured && (captureType == 0 || (captureType == 1 && hwndFoundWindow == GetForegroundWindow())))
            {
                if(ci.flags & CURSOR_SHOWING)
                {
                    HICON hIcon = CopyIcon(ci.hCursor);

                    if(hIcon)
                    {
                        ICONINFO ii;
                        if(GetIconInfo(hIcon, &ii))
                        {
                            POINT capturePos = {captureRect.left, captureRect.top};

                            if(captureType == 1)
                            {
                                if(bClientCapture)
                                    ClientToScreen(hwndCapture, &capturePos);
                                else
                                {
                                    RECT windowRect;
                                    GetWindowRect(hwndCapture, &windowRect);
                                    capturePos.x += windowRect.left;
                                    capturePos.y += windowRect.top;
                                }
                            }

                            int x = ci.ptScreenPos.x - int(ii.xHotspot) - capturePos.x;
                            int y = ci.ptScreenPos.y - int(ii.yHotspot) - capturePos.y;

                            DrawIcon(hDC, x, y, hIcon);

                            DeleteObject(ii.hbmColor);
                            DeleteObject(ii.hbmMask);
                        }

                        DestroyIcon(hIcon);
                    }
                }
            }

            ReleaseCurrentHDC(hDC);
        }
        else
        {
            RUNONCE AppWarning(TEXT("Failed to get DC from capture surface"));
        }

        EndPreprocess();
    }

    void Render(const Vect2 &pos, const Vect2 &size)
    {
        SamplerState *sampler = NULL;
        /*if(bWindows8MonitorCapture)
        {
            RenderWindows8MonitorCapture(pos, size);
            return;
        }*/

        Vect2 ulCoord = Vect2(0.0f, 0.0f),
              lrCoord = Vect2(1.0f, 1.0f);

        if(captureType == 1 && !hwndFoundWindow) {
            // Don't render a giant black rectangle if the window isn't found.
            return;
        }

        if(bWindows8MonitorCapture)
        {
            LONG monitorWidth  = monitorData.rect.right-monitorData.rect.left;
            LONG monitorHeight = monitorData.rect.bottom-monitorData.rect.top;
            RECT captureArea = {captureRect.left-monitorData.rect.left,
                                captureRect.top-monitorData.rect.top,
                                captureRect.right-monitorData.rect.left,
                                captureRect.bottom-monitorData.rect.top};

            ulCoord.x = float(double(captureArea.left)/double(monitorWidth));
            ulCoord.y = float(double(captureArea.top)/double(monitorHeight));

            lrCoord.x = float(double(captureArea.right)/double(monitorWidth));
            lrCoord.y = float(double(captureArea.bottom)/double(monitorHeight));
        }

        if(lastRendered)
        {
            float fGamma = float(-(gamma-100) + 100) * 0.01f;

            Shader *lastPixelShader = GetCurrentPixelShader();

            float fOpacity = float(opacity)*0.01f;
            DWORD opacity255 = DWORD(fOpacity*255.0f);

            if(bUseColorKey)
            {
                LoadPixelShader(colorKeyShader);
                HANDLE hGamma = colorKeyShader->GetParameterByName(TEXT("gamma"));
                if(hGamma)
                    colorKeyShader->SetFloat(hGamma, fGamma);

                float fSimilarity = float(keySimilarity)*0.01f;
                float fBlend      = float(keyBlend)*0.01f;

                colorKeyShader->SetColor(colorKeyShader->GetParameter(2), keyColor);
                colorKeyShader->SetFloat(colorKeyShader->GetParameter(3), fSimilarity);
                colorKeyShader->SetFloat(colorKeyShader->GetParameter(4), fBlend);

            }
            else
            {
                LoadPixelShader(alphaIgnoreShader);
                HANDLE hGamma = alphaIgnoreShader->GetParameterByName(TEXT("gamma"));
                if(hGamma)
                    alphaIgnoreShader->SetFloat(hGamma, fGamma);
            }

            if(bUsePointFiltering) {
                SamplerInfo samplerinfo;
                samplerinfo.filter = GS_FILTER_POINT;
                sampler = CreateSamplerState(samplerinfo);
                LoadSamplerState(sampler, 0);
            }

            if(bCompatibilityMode)
                DrawSpriteEx(lastRendered, (opacity255<<24) | 0xFFFFFF,
                    pos.x, pos.y+size.y, pos.x+size.x, pos.y,
                    ulCoord.x, ulCoord.y,
                    lrCoord.x, lrCoord.y);
            else
                GS->DrawSpriteExRotate(lastRendered, (opacity255<<24) | 0xFFFFFF,
                    pos.x, pos.y, pos.x+size.x, pos.y+size.y, 0.0f,
                    ulCoord.x, ulCoord.y,
                    lrCoord.x, lrCoord.y, rotateDegrees);

            if(bUsePointFiltering) delete(sampler);

            LoadPixelShader(lastPixelShader);

            //draw mouse
            if(bWindows8MonitorCapture && cursorTexture && bMouseCaptured && bCaptureMouse)
            {
                POINT newPos = {cursorPos.x-xHotspot, cursorPos.y-yHotspot};

                if( newPos.x >= captureRect.left && newPos.y >= captureRect.top &&
                    newPos.x < captureRect.right && newPos.y < captureRect.bottom)
                {
                    Vect2 newCursorPos  = Vect2(float(newPos.x-captureRect.left), float(newPos.y-captureRect.top));
                    Vect2 newCursorSize = Vect2(float(cursorTexture->Width()), float(cursorTexture->Height()));

                    Vect2 sizeMultiplier = size/GetSize();
                    newCursorPos *= sizeMultiplier;
                    newCursorPos += pos;
                    newCursorSize *= sizeMultiplier;

                    GS->SetCropping(0.0f, 0.0f, 0.0f, 0.0f);
                    DrawSprite(cursorTexture, 0xFFFFFFFF, newCursorPos.x, newCursorPos.y, newCursorPos.x+newCursorSize.x, newCursorPos.y+newCursorSize.y);
                }
            }
        }
    }

    Vect2 GetSize() const
    {
        return Vect2(float(width), float(height));
    }

    void UpdateSettings()
    {
        App->EnterSceneMutex();

        UINT newCaptureType     = data->GetInt(TEXT("captureType"));
        String strNewWindow     = data->GetString(TEXT("window"));
        String strNewWindowClass= data->GetString(TEXT("windowClass"));
        BOOL bNewClientCapture  = data->GetInt(TEXT("innerWindow"), 1);

        bCaptureMouse   = data->GetInt(TEXT("captureMouse"), 1);
        bCaptureLayered = data->GetInt(TEXT("captureLayered"), 0);

        bool bNewUsePointFiltering = data->GetInt(TEXT("usePointFiltering"), 0) != 0;

        int x  = data->GetInt(TEXT("captureX"));
        int y  = data->GetInt(TEXT("captureY"));
        int cx = data->GetInt(TEXT("captureCX"), 32);
        int cy = data->GetInt(TEXT("captureCY"), 32);

        bool bNewCompatibleMode = data->GetInt(TEXT("compatibilityMode")) != 0;
        if(bNewCompatibleMode && (OSGetVersion() >= 8 && newCaptureType == 0))
            bNewCompatibleMode = false;

        gamma = data->GetInt(TEXT("gamma"), 100);
        if(gamma < 50)        gamma = 50;
        else if(gamma > 175)  gamma = 175;

        UINT newMonitor = data->GetInt(TEXT("monitor"));
        if(newMonitor > App->NumMonitors())
            newMonitor = 0;

        if( captureRect.left != x || captureRect.right != (x+cx) || captureRect.top != cy || captureRect.bottom != (y+cy) ||
            newCaptureType != captureType || !strNewWindowClass.CompareI(strWindowClass) || !strNewWindow.CompareI(strWindow) ||
            bNewClientCapture != bClientCapture || (OSGetVersion() >= 8 && newMonitor != monitor) ||
            bNewCompatibleMode != bCompatibilityMode)
        {
            for(int i=0; i<NUM_CAPTURE_TEXTURES; i++)
            {
                delete renderTextures[i];
                renderTextures[i] = NULL;
            }

            if(duplicator)
            {
                delete duplicator;
                duplicator = NULL;
            }

            if(cursorTexture)
            {
                delete cursorTexture;
                cursorTexture = NULL;
            }

            if(bCompatibilityMode)
            {
                SelectObject(hdcCompatible, hbmpOld);
                DeleteDC(hdcCompatible);
                DeleteObject(hbmpCompatible);

                hdcCompatible = NULL;
                hbmpCompatible = NULL;
                captureBits = NULL;
            }

            hCurrentCursor = NULL;

            captureType        = newCaptureType;
            strWindow          = strNewWindow;
            strWindowClass     = strNewWindowClass;
            bClientCapture     = bNewClientCapture;

            bCompatibilityMode = bNewCompatibleMode;
            bUsePointFiltering = bNewUsePointFiltering;

            captureRect.left   = x;
            captureRect.top    = y;
            captureRect.right  = x+cx;
            captureRect.bottom = y+cy;

            monitor = newMonitor;
            const MonitorInfo &monitorInfo = App->GetMonitor(monitor);
            mcpy(&monitorData, &monitorInfo, sizeof(monitorInfo));

            rotateDegrees = 0.0f;

            if(captureType == 0 && OSGetVersion() >= 8)
            {
                LONG monitorWidth  = monitorInfo.rect.right-monitorInfo.rect.left;
                LONG monitorHeight = monitorInfo.rect.bottom-monitorInfo.rect.top;

                DeviceOutputs outputs;
                GetDisplayDevices(outputs);
                
                bWindows8MonitorCapture = false;

                if(outputs.devices.Num())
                {
                    const MonitorInfo &info = App->GetMonitor(monitor);
                    for(UINT j=0; j<outputs.devices[0].monitors.Num(); j++)
                    {
                        if(outputs.devices[0].monitors[j].hMonitor == info.hMonitor)
                        {
                            deviceOutputID = j;
                            rotateDegrees = outputs.devices[0].monitors[j].rotationDegrees;
                            bWindows8MonitorCapture = true;
                        }
                    }
                }
            }

            width  = cx;
            height = cy;

            if(bCompatibilityMode)
            {
                hdcCompatible = CreateCompatibleDC(NULL);

                BITMAPINFO bi;
                zero(&bi, sizeof(bi));

                BITMAPINFOHEADER &bih = bi.bmiHeader;
                bih.biSize = sizeof(bih);
                bih.biBitCount = 32;
                bih.biWidth  = width;
                bih.biHeight = height;
                bih.biPlanes = 1;

                hbmpCompatible = CreateDIBSection(hdcCompatible, &bi, DIB_RGB_COLORS, (void**)&captureBits, NULL, 0);
                hbmpOld = (HBITMAP)SelectObject(hdcCompatible, hbmpCompatible);
            }

            if(bWindows8MonitorCapture && !bInInit)
                duplicator = GS->CreateOutputDuplicator(deviceOutputID);
            else if(bCompatibilityMode)
                renderTextures[0] = CreateTexture(width, height, GS_BGRA, NULL, FALSE, FALSE);
            else
            {
                for(UINT i=0; i<NUM_CAPTURE_TEXTURES; i++)
                    renderTextures[i] = CreateGDITexture(width, height);
            }

            lastRendered = NULL;
        }

        bool bNewUseColorKey = data->GetInt(TEXT("useColorKey"), 0) != 0;
        keyColor        = data->GetInt(TEXT("keyColor"), 0xFFFFFFFF);
        keySimilarity   = data->GetInt(TEXT("keySimilarity"), 10);
        keyBlend        = data->GetInt(TEXT("keyBlend"), 0);
        bUsePointFiltering = data->GetInt(TEXT("usePointFiltering"), 0) != 0;

        bUseColorKey = bNewUseColorKey;

        opacity = data->GetInt(TEXT("opacity"), 100);

        App->LeaveSceneMutex();
    }

    void SetInt(CTSTR lpName, int iVal)
    {
        if(scmpi(lpName, TEXT("useColorKey")) == 0)
        {
            bool bNewVal = iVal != 0;
            bUseColorKey = bNewVal;
        }
        else if(scmpi(lpName, TEXT("keyColor")) == 0)
        {
            keyColor = (DWORD)iVal;
        }
        else if(scmpi(lpName, TEXT("keySimilarity")) == 0)
        {
            keySimilarity = iVal;
        }
        else if(scmpi(lpName, TEXT("keyBlend")) == 0)
        {
            keyBlend = iVal;
        }
        else if(scmpi(lpName, TEXT("opacity")) == 0)
        {
            opacity = (UINT)iVal;
        }
        else if(scmpi(lpName, TEXT("gamma")) == 0)
        {
            gamma = iVal;
            if(gamma < 50)        gamma = 50;
            else if(gamma > 175)  gamma = 175;
        }
    }
};

ImageSource* STDCALL CreateDesktopSource(XElement *data)
{
    if(!data)
        return NULL;

    return new DesktopImageSource(App->GetFrameTime(), data);
}

void RefreshWindowList(HWND hwndCombobox, StringList &classList)
{
    SendMessage(hwndCombobox, CB_RESETCONTENT, 0, 0);
    classList.Clear();

    HWND hwndCurrent = GetWindow(GetDesktopWindow(), GW_CHILD);
    do
    {
        if(IsWindowVisible(hwndCurrent) && !IsIconic(hwndCurrent))
        {
            RECT clientRect;
            GetClientRect(hwndCurrent, &clientRect);

            String strWindowName;
            strWindowName.SetLength(GetWindowTextLength(hwndCurrent));
            GetWindowText(hwndCurrent, strWindowName, strWindowName.Length()+1);

            DWORD exStyles = (DWORD)GetWindowLongPtr(hwndCurrent, GWL_EXSTYLE);
            DWORD styles = (DWORD)GetWindowLongPtr(hwndCurrent, GWL_STYLE);

            if (strWindowName.IsValid() && sstri(strWindowName, L"battlefield") != nullptr)
                exStyles &= ~WS_EX_TOOLWINDOW;

            if( (exStyles & WS_EX_TOOLWINDOW) == 0 && (styles & WS_CHILD) == 0 &&
                clientRect.bottom != 0 && clientRect.right != 0 /*&& hwndParent == NULL*/)
            {
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

                if(strWindowName.IsEmpty()) strWindowName = GetPathFileName(strFileName);

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

struct ConfigDesktopSourceInfo
{
    CTSTR lpName;
    XElement *data;
    int dialogID;
    StringList strClasses;
    int prevCX, prevCY;
    bool sizeSet;

    inline ConfigDesktopSourceInfo()
    {
        zero(this, sizeof(*this));
    }
};

void SetDesktopCaptureType(HWND hwnd, UINT type)
{
    SendMessage(GetDlgItem(hwnd, IDC_MONITORCAPTURE), BM_SETCHECK, type == 0 ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(hwnd, IDC_WINDOWCAPTURE),  BM_SETCHECK, type == 1 ? BST_CHECKED : BST_UNCHECKED, 0);

    EnableWindow(GetDlgItem(hwnd, IDC_MONITOR),     type == 0);

    EnableWindow(GetDlgItem(hwnd, IDC_WINDOW),      type == 1);
    EnableWindow(GetDlgItem(hwnd, IDC_REFRESH),     type == 1);
    EnableWindow(GetDlgItem(hwnd, IDC_OUTERWINDOW), type == 1);
    EnableWindow(GetDlgItem(hwnd, IDC_INNERWINDOW), type == 1);
}

void SelectTargetWindow(HWND hwnd, bool bRefresh)
{
    HWND hwndWindowList = GetDlgItem(hwnd, IDC_WINDOW);
    UINT windowID = (UINT)SendMessage(hwndWindowList, CB_GETCURSEL, 0, 0);
    if(windowID == CB_ERR)
        return;

    String strWindow = GetCBText(hwndWindowList, windowID);

    ConfigDesktopSourceInfo *info = (ConfigDesktopSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);

    if(windowID >= info->strClasses.Num())
        return;

    HWND hwndTarget = FindWindow(info->strClasses[windowID], strWindow);
    if(!hwndTarget)
    {
        HWND hwndLastKnownHWND = (HWND)SendMessage(hwndWindowList, CB_GETITEMDATA, windowID, 0);
        if(IsWindow(hwndLastKnownHWND))
            hwndTarget = hwndLastKnownHWND;
        else
            hwndTarget = FindWindow(info->strClasses[windowID], NULL);
    }

    if(!hwndTarget)
        return;

    //------------------------------------------

    BOOL bInnerWindow = SendMessage(GetDlgItem(hwnd, IDC_INNERWINDOW), BM_GETCHECK, 0, 0) == BST_CHECKED;

    RECT rc;
    if(bInnerWindow)
        GetClientRect(hwndTarget, &rc);
    else
    {
        GetWindowRect(hwndTarget, &rc);

        rc.bottom -= rc.top;
        rc.right -= rc.left;
        rc.left = rc.top = 0;
    }

    //------------------------------------------

    rc.bottom -= rc.top;
    rc.right -= rc.left;

    if(rc.right < 16)
        rc.right = 16;
    if(rc.bottom < 16)
        rc.bottom = 16;

    BOOL bRegion = SendMessage(GetDlgItem(hwnd, IDC_REGIONCAPTURE), BM_GETCHECK, 0, 0) == BST_CHECKED;
    if(!bRegion || bRefresh)
    {
        SetWindowText(GetDlgItem(hwnd, IDC_POSX),  IntString(rc.left).Array());
        SetWindowText(GetDlgItem(hwnd, IDC_POSY),  IntString(rc.top).Array());
        SetWindowText(GetDlgItem(hwnd, IDC_SIZEX), IntString(rc.right).Array());
        SetWindowText(GetDlgItem(hwnd, IDC_SIZEY), IntString(rc.bottom).Array());
    }
}

struct RegionWindowData
{
    HWND hwndConfigDialog, hwndCaptureWindow;
    POINT startPos;
    RECT captureRect;
    bool bMovingWindow;
    bool bInnerWindowRegion;

    inline RegionWindowData()
    {
        bMovingWindow = false;
        bInnerWindowRegion = false;
    }
};

RegionWindowData regionWindowData;

#define CAPTUREREGIONCLASS TEXT("CaptureRegionThingy")

LRESULT WINAPI RegionWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if(message == WM_MOUSEMOVE)
    {
        RECT client;
        GetClientRect(hwnd, &client);

        POINT pos;
        pos.x = (long)(short)LOWORD(lParam);
        pos.y = (long)(short)HIWORD(lParam);

        if(regionWindowData.bMovingWindow)
        {
            RECT rc;
            GetWindowRect(hwnd, &rc);

            POINT posOffset = {pos.x-regionWindowData.startPos.x, pos.y-regionWindowData.startPos.y};
            POINT newPos = {rc.left+posOffset.x, rc.top+posOffset.y};
            SIZE windowSize = {rc.right-rc.left, rc.bottom-rc.top};

            if(newPos.x < regionWindowData.captureRect.left)
                newPos.x = regionWindowData.captureRect.left;
            if(newPos.y < regionWindowData.captureRect.top)
                newPos.y = regionWindowData.captureRect.top;

            POINT newBottomRight = {rc.right+posOffset.x, rc.bottom+posOffset.y};

            if(newBottomRight.x > regionWindowData.captureRect.right)
                newPos.x -= (newBottomRight.x-regionWindowData.captureRect.right);
            if(newBottomRight.y > regionWindowData.captureRect.bottom)
                newPos.y -= (newBottomRight.y-regionWindowData.captureRect.bottom);

            SetWindowPos(hwnd, NULL, newPos.x, newPos.y, 0, 0, SWP_NOSIZE|SWP_NOZORDER);
        }
        else
        {
            BOOL bLeft   = (pos.x < 6);
            BOOL bTop    = (pos.y < 6);
            BOOL bRight  = (!bLeft && (pos.x > client.right-6));
            BOOL bBottom = (!bTop  && (pos.y > client.bottom-6));

            if(bLeft)
            {
                if(bTop)
                    SetCursor(LoadCursor(NULL, IDC_SIZENWSE));
                else if(bBottom)
                    SetCursor(LoadCursor(NULL, IDC_SIZENESW));
                else
                    SetCursor(LoadCursor(NULL, IDC_SIZEWE));
            }
            else if(bRight)
            {
                if(bTop)
                    SetCursor(LoadCursor(NULL, IDC_SIZENESW));
                else if(bBottom)
                    SetCursor(LoadCursor(NULL, IDC_SIZENWSE));
                else
                    SetCursor(LoadCursor(NULL, IDC_SIZEWE));
            }
            else if(bTop || bBottom)
                SetCursor(LoadCursor(NULL, IDC_SIZENS));
        }

        return 0;
    }
    else if(message == WM_LBUTTONDOWN)
    {
        RECT client;
        GetClientRect(hwnd, &client);

        POINT pos;
        pos.x = (long)LOWORD(lParam);
        pos.y = (long)HIWORD(lParam);

        BOOL bLeft   = (pos.x < 6);
        BOOL bTop    = (pos.y < 6);
        BOOL bRight  = (!bLeft && (pos.x > client.right-6));
        BOOL bBottom = (!bTop  && (pos.y > client.bottom-6));

        ClientToScreen(hwnd, &pos);

        POINTS newPos;
        newPos.x = (SHORT)pos.x;
        newPos.y = (SHORT)pos.y;

        SendMessage(hwnd, WM_MOUSEMOVE, 0, lParam);

        if(bLeft)
        {
            if(bTop)
                SendMessage(hwnd, WM_NCLBUTTONDOWN, HTTOPLEFT, (LPARAM)&newPos);
            else if(bBottom)
                SendMessage(hwnd, WM_NCLBUTTONDOWN, HTBOTTOMLEFT, (LPARAM)&newPos);
            else
                SendMessage(hwnd, WM_NCLBUTTONDOWN, HTLEFT, (LPARAM)&newPos);
        }
        else if(bRight)
        {
            if(bTop)
                SendMessage(hwnd, WM_NCLBUTTONDOWN, HTTOPRIGHT, (LPARAM)&newPos);
            else if(bBottom)
                SendMessage(hwnd, WM_NCLBUTTONDOWN, HTBOTTOMRIGHT, (LPARAM)&newPos);
            else
                SendMessage(hwnd, WM_NCLBUTTONDOWN, HTRIGHT, (LPARAM)&newPos);
        }
        else if(bTop)
            SendMessage(hwnd, WM_NCLBUTTONDOWN, HTTOP, (LPARAM)&newPos);
        else if(bBottom)
            SendMessage(hwnd, WM_NCLBUTTONDOWN, HTBOTTOM, (LPARAM)&newPos);
        else
        {
            regionWindowData.bMovingWindow = true; //can't use HTCAPTION -- if holding down long enough, other windows minimize

            pos.x = (long)(short)LOWORD(lParam);
            pos.y = (long)(short)HIWORD(lParam);
            mcpy(&regionWindowData.startPos, &pos, sizeof(pos));
            SetCapture(hwnd);
        }

        return 0;
    }
    else if(message == WM_LBUTTONUP)
    {
        if(regionWindowData.bMovingWindow)
        {
            regionWindowData.bMovingWindow = false;
            ReleaseCapture();
        }
    }
    else if(message == WM_PAINT)
    {
        PAINTSTRUCT ps;
        HDC hDC = BeginPaint(hwnd, &ps);
        if(hDC)
        {
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);

            //-----------------------------------------

            HPEN oldPen = (HPEN)SelectObject(hDC, GetStockObject(BLACK_PEN));

            MoveToEx(hDC, 0, 0, NULL);
            LineTo(hDC, clientRect.right-1, 0);
            LineTo(hDC, clientRect.right-1, clientRect.bottom-1);
            LineTo(hDC, 0, clientRect.bottom-1);
            LineTo(hDC, 0, 0);

            MoveToEx(hDC, 5, 5, NULL);
            LineTo(hDC, clientRect.right-6, 5);
            LineTo(hDC, clientRect.right-6, clientRect.bottom-6);
            LineTo(hDC, 5, clientRect.bottom-6);
            LineTo(hDC, 5, 5);

            SelectObject(hDC, oldPen);

            //-----------------------------------------

            CTSTR lpStr = Str("Sources.SoftwareCaptureSource.RegionWindowText");

            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HFONT hfontOld = (HFONT)SelectObject(hDC, hFont);
            
            SIZE textExtent;
            GetTextExtentPoint32(hDC, lpStr, slen(lpStr), &textExtent);
            
            SetBkMode(hDC, TRANSPARENT);
            SetTextAlign(hDC, TA_CENTER);
            TextOut(hDC, clientRect.right/2, (clientRect.bottom - textExtent.cy)/2, lpStr, slen(lpStr));

            //-----------------------------------------

            SelectObject(hDC, hfontOld);

            EndPaint(hwnd, &ps);
        }
    }
    else if(message == WM_KEYDOWN)
    {
        if(wParam == VK_ESCAPE || wParam == VK_RETURN || wParam == 'Q')
            DestroyWindow(hwnd);
    }
    else if(message == WM_SIZING)
    {
        RECT &rc = *(RECT*)lParam;

        bool bTop  = wParam == WMSZ_TOPLEFT || wParam == WMSZ_TOPRIGHT || wParam == WMSZ_TOP;
        bool bLeft = wParam == WMSZ_TOPLEFT || wParam == WMSZ_LEFT     || wParam == WMSZ_BOTTOMLEFT;

        if(bLeft)
        {
            if(rc.right-rc.left < 16)
                rc.left = rc.right-16;
        }
        else
        {
            if(rc.right-rc.left < 16)
                rc.right = rc.left+16;
        }

        if(bTop)
        {
            if(rc.bottom-rc.top < 16)
                rc.top = rc.bottom-16;
        }
        else
        {
            if(rc.bottom-rc.top < 16)
                rc.bottom = rc.top+16;
        }

        if(regionWindowData.hwndCaptureWindow)
        {
            if(rc.left < regionWindowData.captureRect.left)
                rc.left = regionWindowData.captureRect.left;
            if(rc.top < regionWindowData.captureRect.top)
                rc.top = regionWindowData.captureRect.top;

            if(rc.right > regionWindowData.captureRect.right)
                rc.right = regionWindowData.captureRect.right;
            if(rc.bottom > regionWindowData.captureRect.bottom)
                rc.bottom = regionWindowData.captureRect.bottom;
        }

        return TRUE;
    }
    else if(message == WM_SIZE || message == WM_MOVE)
    {
        RECT rc;
        GetWindowRect(hwnd, &rc);

        if(rc.right-rc.left < 16)
            rc.right = rc.left+16;
        if(rc.bottom-rc.top < 16)
            rc.bottom = rc.top+16;

        SetWindowText(GetDlgItem(regionWindowData.hwndConfigDialog, IDC_SIZEX), IntString(rc.right-rc.left).Array());
        SetWindowText(GetDlgItem(regionWindowData.hwndConfigDialog, IDC_SIZEY), IntString(rc.bottom-rc.top).Array());

        if(regionWindowData.hwndCaptureWindow)
        {
            rc.left -= regionWindowData.captureRect.left;
            rc.top  -= regionWindowData.captureRect.top;
        }

        SetWindowText(GetDlgItem(regionWindowData.hwndConfigDialog, IDC_POSX),  IntString(rc.left).Array());
        SetWindowText(GetDlgItem(regionWindowData.hwndConfigDialog, IDC_POSY),  IntString(rc.top).Array());

        if(message == WM_SIZE)
            RedrawWindow(hwnd, NULL, NULL, RDW_INTERNALPAINT|RDW_ERASE|RDW_INVALIDATE);
    }
    else if(message == WM_ACTIVATE)
    {
        if(wParam == WA_INACTIVE)
            DestroyWindow(hwnd);
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

struct ColorSelectionData
{
    HDC hdcDesktop;
    HDC hdcDestination;
    HBITMAP hBitmap;
    bool bValid;

    inline ColorSelectionData() : hdcDesktop(NULL), hdcDestination(NULL), hBitmap(NULL), bValid(false) {}
    inline ~ColorSelectionData() {Clear();}

    inline bool Init()
    {
        hdcDesktop = GetDC(NULL);
        if(!hdcDesktop)
            return false;

        hdcDestination = CreateCompatibleDC(hdcDesktop);
        if(!hdcDestination)
            return false;

        hBitmap = CreateCompatibleBitmap(hdcDesktop, 1, 1);
        if(!hBitmap)
            return false;

        SelectObject(hdcDestination, hBitmap);
        bValid = true;

        return true;
    }

    inline void Clear()
    {
        if(hdcDesktop)
        {
            ReleaseDC(NULL, hdcDesktop);
            hdcDesktop = NULL;
        }

        if(hdcDestination)
        {
            DeleteDC(hdcDestination);
            hdcDestination = NULL;
        }

        if(hBitmap)
        {
            DeleteObject(hBitmap);
            hBitmap = NULL;
        }

        bValid = false;
    }

    inline DWORD GetColor()
    {
        POINT p;
        if(GetCursorPos(&p))
        {
            BITMAPINFO data;
            zero(&data, sizeof(data));

            data.bmiHeader.biSize = sizeof(data.bmiHeader);
            data.bmiHeader.biWidth = 1;
            data.bmiHeader.biHeight = 1;
            data.bmiHeader.biPlanes = 1;
            data.bmiHeader.biBitCount = 24;
            data.bmiHeader.biCompression = BI_RGB;
            data.bmiHeader.biSizeImage = 4;

            if(BitBlt(hdcDestination, 0, 0, 1, 1, hdcDesktop, p.x, p.y, SRCCOPY|CAPTUREBLT))
            {
                DWORD buffer;
                if(GetDIBits(hdcDestination, hBitmap, 0, 1, &buffer, &data, DIB_RGB_COLORS))
                    return 0xFF000000|buffer;
            }
            else
            {
                int err = GetLastError();
                nop();
            }
        }

        return 0xFF000000;
    }
};

INT_PTR CALLBACK ConfigDesktopSourceProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static bool bSelectingColor = false;
    static bool bMouseDown = false;
    static ColorSelectionData colorData;
    HWND hwndTemp;

    switch(message)
    {
        case WM_INITDIALOG:
            {
                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)lParam);
                LocalizeWindow(hwnd);

                //--------------------------------------------

                HWND hwndToolTip = CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL, WS_POPUP|TTS_NOPREFIX|TTS_ALWAYSTIP,
                    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                    hwnd, NULL, hinstMain, NULL);

                TOOLINFO ti;
                zero(&ti, sizeof(ti));
                ti.cbSize = sizeof(ti);
                ti.uFlags = TTF_SUBCLASS|TTF_IDISHWND;
                ti.hwnd = hwnd;

                if (LocaleIsRTL())
                    ti.uFlags |= TTF_RTLREADING;

                SendMessage(hwndToolTip, TTM_SETMAXTIPWIDTH, 0, 500);
                SendMessage(hwndToolTip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 8000);

                //-----------------------------------------------------

                ConfigDesktopSourceInfo *info = (ConfigDesktopSourceInfo*)lParam;
                XElement *data = info->data;

                info->prevCX = AppConfig->GetInt(TEXT("Video"), TEXT("BaseWidth"));
                info->prevCY = AppConfig->GetInt(TEXT("Video"), TEXT("BaseHeight"));

                if (info->dialogID == IDD_CONFIGUREMONITORCAPTURE || info->dialogID == IDD_CONFIGUREDESKTOPSOURCE) {
                    hwndTemp = GetDlgItem(hwnd, IDC_MONITOR);
                    for(UINT i=0; i<App->NumMonitors(); i++)
                        SendMessage(hwndTemp, CB_ADDSTRING, 0, (LPARAM)UIntString(i+1).Array());
                    SendMessage(hwndTemp, CB_SETCURSEL, 0, 0);

                    if (OSGetVersion() > 7) {
                        EnableWindow(GetDlgItem(hwnd, IDC_CAPTURELAYERED), FALSE);
                        EnableWindow(GetDlgItem(hwnd, IDC_COMPATIBILITYMODE), (info->dialogID != IDD_CONFIGUREMONITORCAPTURE));
                    }
                }

                UINT captureType = data->GetInt(TEXT("captureType"), 0);

                if (info->dialogID == IDD_CONFIGUREDESKTOPSOURCE)
                    SetDesktopCaptureType(hwnd, captureType);

                //-----------------------------------------------------

                if (info->dialogID == IDD_CONFIGUREMONITORCAPTURE || info->dialogID == IDD_CONFIGUREDESKTOPSOURCE) {
                    hwndTemp = GetDlgItem(hwnd, IDC_MONITOR);
                    UINT monitor = data->GetInt(TEXT("monitor"));
                    SendMessage(hwndTemp, CB_SETCURSEL, monitor, 0);
                }

                //-----------------------------------------------------

                if (info->dialogID == IDD_CONFIGUREMONITORCAPTURE) {
                    String strWarnings;
                    if (OSGetVersion() < 8) {
                        strWarnings << Str("Sources.SoftwareCaptureSource.Warning");

                        BOOL bComposition;
                        DwmIsCompositionEnabled(&bComposition);
                        if (bComposition)
                            strWarnings << TEXT("\r\n") << Str("Sources.SoftwareCaptureSource.WarningAero");
                    }

                    SetWindowText(GetDlgItem(hwnd, IDC_WARNING), strWarnings);
                }

                //-----------------------------------------------------

                bool bFoundWindow = false;
                if (info->dialogID == IDD_CONFIGUREWINDOWCAPTURE || info->dialogID == IDD_CONFIGUREDESKTOPSOURCE) {
                    HWND hwndWindowList = GetDlgItem(hwnd, IDC_WINDOW);

                    CTSTR lpWindowName = data->GetString(TEXT("window"));
                    CTSTR lpWindowClass = data->GetString(TEXT("windowClass"));
                    BOOL bInnerWindow = (BOOL)data->GetInt(TEXT("innerWindow"), 1);

                    RefreshWindowList(hwndWindowList, info->strClasses);

                    UINT windowID = 0;
                    if(lpWindowName)
                        windowID = (UINT)SendMessage(hwndWindowList, CB_FINDSTRINGEXACT, -1, (LPARAM)lpWindowName);

                    bFoundWindow = (windowID != CB_ERR);
                    if(!bFoundWindow)
                        windowID = 0;

                    SendMessage(hwndWindowList, CB_SETCURSEL, windowID, 0);

                    if(bInnerWindow)
                        SendMessage(GetDlgItem(hwnd, IDC_INNERWINDOW), BM_SETCHECK, BST_CHECKED, 0);
                    else
                        SendMessage(GetDlgItem(hwnd, IDC_OUTERWINDOW), BM_SETCHECK, BST_CHECKED, 0);
                }

                //-----------------------------------------------------

                bool bMouseCapture = data->GetInt(TEXT("captureMouse"), 1) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_CAPTUREMOUSE), BM_SETCHECK, (bMouseCapture) ? BST_CHECKED : BST_UNCHECKED, 0);

                bool bCaptureLayered = data->GetInt(TEXT("captureLayered"), 0) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_CAPTURELAYERED), BM_SETCHECK, (bCaptureLayered) ? BST_CHECKED : BST_UNCHECKED, 0);

                ti.lpszText = (LPWSTR)Str("Sources.SoftwareCaptureSource.CaptureLayeredTip");
                ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_CAPTURELAYERED);
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                bool bCompatibilityMode = data->GetInt(TEXT("compatibilityMode"), 0) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_COMPATIBILITYMODE), BM_SETCHECK, (bCompatibilityMode) ? BST_CHECKED : BST_UNCHECKED, 0);

                //-----------------------------------------------------

                bool bRegion = data->GetInt(TEXT("regionCapture")) != FALSE;
                if(captureType == 1 && !bFoundWindow)
                    bRegion = false;

                SendMessage(GetDlgItem(hwnd, IDC_REGIONCAPTURE), BM_SETCHECK, (bRegion) ? BST_CHECKED : BST_UNCHECKED, 0);
                EnableWindow(GetDlgItem(hwnd, IDC_POSX),            bRegion);
                EnableWindow(GetDlgItem(hwnd, IDC_POSY),            bRegion);
                EnableWindow(GetDlgItem(hwnd, IDC_SIZEX),           bRegion);
                EnableWindow(GetDlgItem(hwnd, IDC_SIZEY),           bRegion);
                EnableWindow(GetDlgItem(hwnd, IDC_SELECTREGION),    bRegion);

                int posX = 0, posY = 0, sizeX = 32, sizeY = 32;
                if(!data->GetBaseItem(TEXT("captureX")) || !bRegion)
                {
                    if(captureType == 0)
                        PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_MONITOR, 0), 0);
                    else if(captureType == 1)
                        PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_WINDOW, 0), 0);
                }
                else
                {
                    posX  = data->GetInt(TEXT("captureX"));
                    posY  = data->GetInt(TEXT("captureY"));
                    sizeX = data->GetInt(TEXT("captureCX"), 32);
                    sizeY = data->GetInt(TEXT("captureCY"), 32);

                    if(sizeX < 16)
                        sizeX = 16;
                    if(sizeY < 16)
                        sizeY = 16;

                    SetWindowText(GetDlgItem(hwnd, IDC_POSX),  IntString(posX).Array());
                    SetWindowText(GetDlgItem(hwnd, IDC_POSY),  IntString(posY).Array());
                    SetWindowText(GetDlgItem(hwnd, IDC_SIZEX), IntString(sizeX).Array());
                    SetWindowText(GetDlgItem(hwnd, IDC_SIZEY), IntString(sizeY).Array());
                }

                ti.lpszText = (LPWSTR)Str("Sources.SoftwareCaptureSource.SelectRegionTooltip");
                ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_SELECTREGION);
                SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

                //------------------------------------------

                BOOL  bUseColorKey  = data->GetInt(TEXT("useColorKey"), 0);
                DWORD keyColor      = data->GetInt(TEXT("keyColor"), 0xFFFFFFFF);
                UINT  similarity    = data->GetInt(TEXT("keySimilarity"), 10);
                UINT  blend         = data->GetInt(TEXT("keyBlend"), 0);
                
                BOOL  bUsePointFiltering = data->GetInt(TEXT("usePointFiltering"), 0);
                SendMessage(GetDlgItem(hwnd, IDC_POINTFILTERING), BM_SETCHECK, bUsePointFiltering ? BST_CHECKED : BST_UNCHECKED, 0);

                SendMessage(GetDlgItem(hwnd, IDC_USECOLORKEY), BM_SETCHECK, bUseColorKey ? BST_CHECKED : BST_UNCHECKED, 0);
                CCSetColor(GetDlgItem(hwnd, IDC_COLOR), keyColor);

                SendMessage(GetDlgItem(hwnd, IDC_BASETHRESHOLD), UDM_SETRANGE32, 0, 100);
                SendMessage(GetDlgItem(hwnd, IDC_BASETHRESHOLD), UDM_SETPOS32, 0, similarity);

                SendMessage(GetDlgItem(hwnd, IDC_BLEND), UDM_SETRANGE32, 0, 100);
                SendMessage(GetDlgItem(hwnd, IDC_BLEND), UDM_SETPOS32, 0, blend);

                EnableWindow(GetDlgItem(hwnd, IDC_COLOR), bUseColorKey);
                EnableWindow(GetDlgItem(hwnd, IDC_SELECT), bUseColorKey);
                EnableWindow(GetDlgItem(hwnd, IDC_BASETHRESHOLD_EDIT), bUseColorKey);
                EnableWindow(GetDlgItem(hwnd, IDC_BASETHRESHOLD), bUseColorKey);
                EnableWindow(GetDlgItem(hwnd, IDC_BLEND_EDIT), bUseColorKey);
                EnableWindow(GetDlgItem(hwnd, IDC_BLEND), bUseColorKey);

                //------------------------------------------

                UINT opacity = data->GetInt(TEXT("opacity"), 100);

                SendMessage(GetDlgItem(hwnd, IDC_OPACITY2), UDM_SETRANGE32, 0, 100);
                SendMessage(GetDlgItem(hwnd, IDC_OPACITY2), UDM_SETPOS32, 0, opacity);

                //------------------------------------------

                int gammaVal = data->GetInt(TEXT("gamma"), 100);

                hwndTemp = GetDlgItem(hwnd, IDC_GAMMA);
                SendMessage(hwndTemp, TBM_CLEARTICS, FALSE, 0);
                SendMessage(hwndTemp, TBM_SETRANGE, FALSE, MAKELPARAM(50, 175));
                SendMessage(hwndTemp, TBM_SETTIC, 0, 100);
                SendMessage(hwndTemp, TBM_SETPOS, TRUE, gammaVal);

                SetSliderText(hwnd, IDC_GAMMA, IDC_GAMMAVAL);

                //--------------------------------------------

                if (info->dialogID == IDD_CONFIGUREWINDOWCAPTURE)
                    PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_WINDOW, 0), 0);

                return TRUE;
            }

        case WM_DESTROY:
            if(colorData.bValid)
            {
                CCSetColor(GetDlgItem(hwnd, IDC_COLOR), colorData.GetColor());
                colorData.Clear();
            }
            break;

        case WM_LBUTTONDOWN:
            if(bSelectingColor)
            {
                bMouseDown = true;
                CCSetColor(GetDlgItem(hwnd, IDC_COLOR), colorData.GetColor());
                ConfigDesktopSourceProc(hwnd, WM_COMMAND, MAKEWPARAM(IDC_COLOR, CCN_CHANGED), (LPARAM)GetDlgItem(hwnd, IDC_COLOR));
            }
            break;

        case WM_MOUSEMOVE:
            if(bSelectingColor && bMouseDown)
            {
                CCSetColor(GetDlgItem(hwnd, IDC_COLOR), colorData.GetColor());
                ConfigDesktopSourceProc(hwnd, WM_COMMAND, MAKEWPARAM(IDC_COLOR, CCN_CHANGED), (LPARAM)GetDlgItem(hwnd, IDC_COLOR));
            }
            break;

        case WM_LBUTTONUP:
            if(bSelectingColor)
            {
                colorData.Clear();
                ReleaseCapture();
                bMouseDown = false;
                bSelectingColor = false;

                ConfigDesktopSourceInfo *configData = (ConfigDesktopSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                ImageSource *source = API->GetSceneImageSource(configData->lpName);
                if(source)
                    source->SetInt(TEXT("useColorKey"), true);
            }
            break;

        case WM_CAPTURECHANGED:
            if(bSelectingColor)
            {
                if(colorData.bValid)
                {
                    CCSetColor(GetDlgItem(hwnd, IDC_COLOR), colorData.GetColor());
                    ConfigDesktopSourceProc(hwnd, WM_COMMAND, MAKEWPARAM(IDC_COLOR, CCN_CHANGED), (LPARAM)GetDlgItem(hwnd, IDC_COLOR));
                    colorData.Clear();
                }

                ReleaseCapture();
                bMouseDown = false;
                bSelectingColor = false;

                ConfigDesktopSourceInfo *configData = (ConfigDesktopSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                ImageSource *source = API->GetSceneImageSource(configData->lpName);
                if(source)
                    source->SetInt(TEXT("useColorKey"), true);
            }
            break;

        case WM_HSCROLL:
            {
                if(GetDlgCtrlID((HWND)lParam) == IDC_GAMMA)
                {
                    int gamma = SetSliderText(hwnd, IDC_GAMMA, IDC_GAMMAVAL);

                    ConfigDesktopSourceInfo *configData = (ConfigDesktopSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                    ImageSource *source = API->GetSceneImageSource(configData->lpName);
                    if(source)
                        source->SetInt(TEXT("gamma"), gamma);
                }
            }
            break;

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_MONITORCAPTURE:
                    SetDesktopCaptureType(hwnd, 0);
                    PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_MONITOR, 0), 0);

                    if(OSGetVersion() >= 8)
                    {
                        EnableWindow(GetDlgItem(hwnd, IDC_COMPATIBILITYMODE), FALSE);
                        SendMessage(GetDlgItem(hwnd, IDC_COMPATIBILITYMODE), BM_SETCHECK, BST_UNCHECKED, 0);
                    }
                    break;

                case IDC_WINDOWCAPTURE:
                    SetDesktopCaptureType(hwnd, 1);
                    PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_WINDOW, 0), 0);

                    if(OSGetVersion() >= 8)
                        EnableWindow(GetDlgItem(hwnd, IDC_COMPATIBILITYMODE), TRUE);
                    break;

                case IDC_REGIONCAPTURE:
                    {
                        ConfigDesktopSourceInfo *info = (ConfigDesktopSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);

                        UINT captureType = 0;
                        
                        if (info->dialogID == IDD_CONFIGUREDESKTOPSOURCE)
                            captureType = (SendMessage(GetDlgItem(hwnd, IDC_WINDOWCAPTURE), BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
                        else if (info->dialogID == IDD_CONFIGUREMONITORCAPTURE)
                            captureType = 0;
                        else
                            captureType = 1;

                        BOOL bRegion = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;

                        EnableWindow(GetDlgItem(hwnd, IDC_POSX),            bRegion);
                        EnableWindow(GetDlgItem(hwnd, IDC_POSY),            bRegion);
                        EnableWindow(GetDlgItem(hwnd, IDC_SIZEX),           bRegion);
                        EnableWindow(GetDlgItem(hwnd, IDC_SIZEY),           bRegion);
                        EnableWindow(GetDlgItem(hwnd, IDC_SELECTREGION),    bRegion);

                        if(!bRegion)
                        {
                            if(captureType == 0)
                                PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_MONITOR, 0), 0);
                            else
                                PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_WINDOW, 0), 0);
                        }
                        break;
                    }

                case IDC_SELECTREGION:
                    {
                        UINT posX, posY, sizeX, sizeY;
                        posX  = GetEditText(GetDlgItem(hwnd, IDC_POSX)).ToInt();
                        posY  = GetEditText(GetDlgItem(hwnd, IDC_POSY)).ToInt();
                        sizeX = GetEditText(GetDlgItem(hwnd, IDC_SIZEX)).ToInt();
                        sizeY = GetEditText(GetDlgItem(hwnd, IDC_SIZEY)).ToInt();

                        //--------------------------------------------

                        ConfigDesktopSourceInfo *info = (ConfigDesktopSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);

                        UINT captureType = 0;
                        
                        if (info->dialogID == IDD_CONFIGUREDESKTOPSOURCE)
                            captureType = (SendMessage(GetDlgItem(hwnd, IDC_WINDOWCAPTURE), BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
                        else if (info->dialogID == IDD_CONFIGUREMONITORCAPTURE)
                            captureType = 0;
                        else
                            captureType = 1;

                        BOOL bRegion = (SendMessage(GetDlgItem(hwnd, IDC_REGIONCAPTURE), BM_GETCHECK, 0, 0) == BST_CHECKED);

                        if(bRegion && captureType == 1)
                        {
                            UINT windowID = (UINT)SendMessage(GetDlgItem(hwnd, IDC_WINDOW), CB_GETCURSEL, 0, 0);
                            if(windowID == CB_ERR) windowID = 0;

                            if(windowID >= info->strClasses.Num())
                            {
                                OBSMessageBox(hwnd, Str("Sources.SoftwareCaptureSource.WindowNotFound"), NULL, 0);
                                break;
                            }

                            String strWindow = GetCBText(GetDlgItem(hwnd, IDC_WINDOW), windowID);

                            zero(&regionWindowData, sizeof(regionWindowData));

                            regionWindowData.hwndCaptureWindow = FindWindow(info->strClasses[windowID], strWindow);
                            if(!regionWindowData.hwndCaptureWindow)
                            {
                                HWND hwndLastKnownHWND = (HWND)SendMessage(GetDlgItem(hwnd, IDC_WINDOW), CB_GETITEMDATA, windowID, 0);
                                if(IsWindow(hwndLastKnownHWND))
                                    regionWindowData.hwndCaptureWindow = hwndLastKnownHWND;
                                else
                                    regionWindowData.hwndCaptureWindow = FindWindow(info->strClasses[windowID], NULL);
                            }

                            if(!regionWindowData.hwndCaptureWindow)
                            {
                                OBSMessageBox(hwnd, Str("Sources.SoftwareCaptureSource.WindowNotFound"), NULL, 0);
                                break;
                            }
                            else if(IsIconic(regionWindowData.hwndCaptureWindow))
                            {
                                OBSMessageBox(hwnd, Str("Sources.SoftwareCaptureSource.WindowMinimized"), NULL, 0);
                                break;
                            }

                            regionWindowData.bInnerWindowRegion = SendMessage(GetDlgItem(hwnd, IDC_INNERWINDOW), BM_GETCHECK, 0, 0) == BST_CHECKED;

                            if(regionWindowData.bInnerWindowRegion)
                            {
                                GetClientRect(regionWindowData.hwndCaptureWindow, &regionWindowData.captureRect);
                                ClientToScreen(regionWindowData.hwndCaptureWindow, (POINT*)&regionWindowData.captureRect);
                                ClientToScreen(regionWindowData.hwndCaptureWindow, (POINT*)&regionWindowData.captureRect.right);
                            }
                            else
                                GetWindowRect(regionWindowData.hwndCaptureWindow, &regionWindowData.captureRect);

                            posX += regionWindowData.captureRect.left;
                            posY += regionWindowData.captureRect.top;
                        }
                        else
                        {
                            UINT monitorID = (UINT)SendMessage(GetDlgItem(hwnd, IDC_MONITOR), CB_GETCURSEL, 0, 0);
                            if(monitorID == CB_ERR) monitorID = 0;

                            const MonitorInfo &monitor = App->GetMonitor(monitorID);
                            regionWindowData.hwndCaptureWindow = NULL;
                            mcpy(&regionWindowData.captureRect, &monitor.rect, sizeof(RECT));
                        }

                        //--------------------------------------------

                        regionWindowData.hwndConfigDialog = hwnd;
                        HWND hwndRegion = CreateWindowEx(WS_EX_TOPMOST, CAPTUREREGIONCLASS, NULL, WS_POPUP|WS_VISIBLE, posX, posY, sizeX, sizeY, hwnd, NULL, hinstMain, NULL);

                        //everyone better thank homeworld for this
                        SetWindowLongW(hwndRegion, GWL_EXSTYLE, GetWindowLong(hwndRegion, GWL_EXSTYLE) | WS_EX_LAYERED);
                        SetLayeredWindowAttributes(hwndRegion, 0x000000, 0xC0, LWA_ALPHA);
                        break;
                    }

                case IDC_SETSTREAMSIZE:
                    if (MessageBoxW(hwnd, Str("Sources.SoftwareCaptureSource.ResizeWarning"), Str("Sources.SoftwareCaptureSource.ResizeWarningTitle"), MB_YESNO | MB_ICONWARNING) == IDYES)
                    {
                        ConfigDesktopSourceInfo *info = (ConfigDesktopSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);

                        UINT sizeX = (UINT)GetEditText(GetDlgItem(hwnd, IDC_SIZEX)).ToInt();
                        UINT sizeY = (UINT)GetEditText(GetDlgItem(hwnd, IDC_SIZEY)).ToInt();

                        if(sizeX < 128)
                            sizeX = 128;
                        else if(sizeX > 4096)
                            sizeX = 4096;

                        if(sizeY < 128)
                            sizeY = 128;
                        else if(sizeY > 4096)
                            sizeY = 4096;

                        AppConfig->SetInt(TEXT("Video"), TEXT("BaseWidth"), sizeX);
                        AppConfig->SetInt(TEXT("Video"), TEXT("BaseHeight"), sizeY);

                        if(!App->IsRunning())
                            App->ResizeWindow(false);

                        info->sizeSet = true;
                    }
                    break;

                case IDC_MONITOR:
                    {
                        if (!lParam || HIWORD(wParam) == CBN_SELCHANGE) {
                            UINT id = (UINT) SendMessage(GetDlgItem(hwnd, IDC_MONITOR), CB_GETCURSEL, 0, 0);
                            const MonitorInfo &monitor = App->GetMonitor(id);

                            UINT x, y, cx, cy;
                            x = monitor.rect.left;
                            y = monitor.rect.top;
                            cx = monitor.rect.right - monitor.rect.left;
                            cy = monitor.rect.bottom - monitor.rect.top;

                            if (cx < 16)
                                cx = 16;
                            if (cy < 16)
                                cy = 16;

                            SetWindowText(GetDlgItem(hwnd, IDC_POSX), IntString(x).Array());
                            SetWindowText(GetDlgItem(hwnd, IDC_POSY), IntString(y).Array());
                            SetWindowText(GetDlgItem(hwnd, IDC_SIZEX), IntString(cx).Array());
                            SetWindowText(GetDlgItem(hwnd, IDC_SIZEY), IntString(cy).Array());
                        }
                        break;
                    }

                case IDC_WINDOW:
                case IDC_OUTERWINDOW:
                case IDC_INNERWINDOW:
                    SelectTargetWindow(hwnd, HIWORD(wParam)==CBN_SELCHANGE);
                    break;

                case IDC_REFRESH:
                    {
                        ConfigDesktopSourceInfo *info = (ConfigDesktopSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                        XElement *data = info->data;

                        CTSTR lpWindowName = data->GetString(TEXT("window"));

                        HWND hwndWindowList = GetDlgItem(hwnd, IDC_WINDOW);
                        RefreshWindowList(hwndWindowList, info->strClasses);

                        UINT windowID = 0;
                        if(lpWindowName)
                            windowID = (UINT)SendMessage(hwndWindowList, CB_FINDSTRINGEXACT, -1, (LPARAM)lpWindowName);

                        if(windowID != CB_ERR)
                            SendMessage(hwndWindowList, CB_SETCURSEL, windowID, 0);
                        else
                        {
                            SendMessage(hwndWindowList, CB_INSERTSTRING, 0, (LPARAM)lpWindowName);
                            SendMessage(hwndWindowList, CB_SETCURSEL, 0, 0);
                        }
                        break;
                    }

                case IDC_USECOLORKEY:
                    {
                        HWND hwndUseColorKey = (HWND)lParam;
                        BOOL bUseColorKey = SendMessage(hwndUseColorKey, BM_GETCHECK, 0, 0) == BST_CHECKED;

                        ConfigDesktopSourceInfo *configData = (ConfigDesktopSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                        ImageSource *source = API->GetSceneImageSource(configData->lpName);
                        if(source)
                            source->SetInt(TEXT("useColorKey"), bUseColorKey);

                        EnableWindow(GetDlgItem(hwnd, IDC_COLOR), bUseColorKey);
                        EnableWindow(GetDlgItem(hwnd, IDC_SELECT), bUseColorKey);
                        EnableWindow(GetDlgItem(hwnd, IDC_BASETHRESHOLD_EDIT), bUseColorKey);
                        EnableWindow(GetDlgItem(hwnd, IDC_BASETHRESHOLD), bUseColorKey);
                        EnableWindow(GetDlgItem(hwnd, IDC_BLEND_EDIT), bUseColorKey);
                        EnableWindow(GetDlgItem(hwnd, IDC_BLEND), bUseColorKey);
                        break;
                    }

                case IDC_COLOR:
                    {
                        ConfigDesktopSourceInfo *configData = (ConfigDesktopSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                        ImageSource *source = API->GetSceneImageSource(configData->lpName);

                        if(source)
                        {
                            DWORD color = CCGetColor((HWND)lParam);
                            source->SetInt(TEXT("keyColor"), color);
                        }
                        break;
                    }

                case IDC_SELECT:
                    {
                        if(!bSelectingColor)
                        {
                            if(colorData.Init())
                            {
                                bMouseDown = false;
                                bSelectingColor = true;
                                SetCapture(hwnd);
                                HCURSOR hCursor = (HCURSOR)LoadImage(hinstMain, MAKEINTRESOURCE(IDC_COLORPICKER), IMAGE_CURSOR, 32, 32, 0);
                                SetCursor(hCursor);

                                ConfigDesktopSourceInfo *configData = (ConfigDesktopSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                                ImageSource *source = API->GetSceneImageSource(configData->lpName);
                                if(source)
                                    source->SetInt(TEXT("useColorKey"), false);
                            }
                            else
                                colorData.Clear();
                        }
                        break;
                    }

                case IDC_OPACITY_EDIT:
                case IDC_BASETHRESHOLD_EDIT:
                case IDC_BLEND_EDIT:
                    if(HIWORD(wParam) == EN_CHANGE)
                    {
                        ConfigDesktopSourceInfo *configData = (ConfigDesktopSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                        if(configData)
                        {
                            ImageSource *source = API->GetSceneImageSource(configData->lpName);

                            if(source)
                            {
                                HWND hwndVal = NULL;
                                switch(LOWORD(wParam))
                                {
                                    case IDC_BASETHRESHOLD_EDIT:    hwndVal = GetDlgItem(hwnd, IDC_BASETHRESHOLD); break;
                                    case IDC_BLEND_EDIT:            hwndVal = GetDlgItem(hwnd, IDC_BLEND); break;
                                    case IDC_OPACITY_EDIT:          hwndVal = GetDlgItem(hwnd, IDC_OPACITY2); break;
                                }

                                int val = (int)SendMessage(hwndVal, UDM_GETPOS32, 0, 0);
                                switch(LOWORD(wParam))
                                {
                                    case IDC_BASETHRESHOLD_EDIT:    source->SetInt(TEXT("keySimilarity"), val); break;
                                    case IDC_BLEND_EDIT:            source->SetInt(TEXT("keyBlend"), val); break;
                                    case IDC_OPACITY_EDIT:          source->SetInt(TEXT("opacity"), val); break;
                                }
                            }
                        }
                    }
                    break;

                case IDOK:
                    {
                        ConfigDesktopSourceInfo *info = (ConfigDesktopSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);

                        UINT captureType = 0;

                        if (info->dialogID == IDD_CONFIGUREDESKTOPSOURCE) {
                            if(SendMessage(GetDlgItem(hwnd, IDC_MONITORCAPTURE), BM_GETCHECK, 0, 0) == BST_CHECKED)
                                captureType = 0;
                            else if(SendMessage(GetDlgItem(hwnd, IDC_WINDOWCAPTURE), BM_GETCHECK, 0, 0) == BST_CHECKED)
                                captureType = 1;
                        } else if (info->dialogID == IDD_CONFIGUREMONITORCAPTURE) {
                            captureType = 0;
                        } else { //IDD_CONFIGUREWINDOWCAPTURE
                            captureType = 1;
                        }

                        BOOL bRegion = (SendMessage(GetDlgItem(hwnd, IDC_REGIONCAPTURE), BM_GETCHECK, 0, 0) == BST_CHECKED);

                        UINT monitorID = 0;

                        if (info->dialogID == IDD_CONFIGUREDESKTOPSOURCE || info->dialogID == IDD_CONFIGUREMONITORCAPTURE)
                            monitorID = (UINT)SendMessage(GetDlgItem(hwnd, IDC_MONITOR), CB_GETCURSEL, 0, 0);
                        if(monitorID == CB_ERR) monitorID = 0;

                        UINT windowID = 0;
                        if (info->dialogID == IDD_CONFIGUREDESKTOPSOURCE || info->dialogID == IDD_CONFIGUREWINDOWCAPTURE)
                            windowID = (UINT)SendMessage(GetDlgItem(hwnd, IDC_WINDOW), CB_GETCURSEL, 0, 0);
                        if(windowID == CB_ERR) windowID = 0;

                        if(captureType == 1 && windowID >= info->strClasses.Num())
                            break;

                        BOOL bInnerWindow = FALSE;
                        String strWindow;

                        if (info->dialogID == IDD_CONFIGUREDESKTOPSOURCE || info->dialogID == IDD_CONFIGUREWINDOWCAPTURE) {
                            strWindow = GetCBText(GetDlgItem(hwnd, IDC_WINDOW), windowID);
                            bInnerWindow = SendMessage(GetDlgItem(hwnd, IDC_INNERWINDOW), BM_GETCHECK, 0, 0) == BST_CHECKED;
                        }

                        UINT posX, posY, sizeX, sizeY;
                        posX  = GetEditText(GetDlgItem(hwnd, IDC_POSX)).ToInt();
                        posY  = GetEditText(GetDlgItem(hwnd, IDC_POSY)).ToInt();
                        sizeX = GetEditText(GetDlgItem(hwnd, IDC_SIZEX)).ToInt();
                        sizeY = GetEditText(GetDlgItem(hwnd, IDC_SIZEY)).ToInt();

                        if(sizeX < 16)
                            sizeX = 16;
                        if(sizeY < 16)
                            sizeY = 16;

                        BOOL bCaptureLayered = FALSE;
                        BOOL bCaptureMouse = SendMessage(GetDlgItem(hwnd, IDC_CAPTUREMOUSE), BM_GETCHECK, 0, 0) == BST_CHECKED;
                        //if (info->dialogID == IDD_CONFIGUREDESKTOPSOURCE || info->dialogID == IDD_CONFIGUREMONITORCAPTURE)
                            bCaptureLayered = SendMessage(GetDlgItem(hwnd, IDC_CAPTURELAYERED), BM_GETCHECK, 0, 0) == BST_CHECKED;
                        BOOL bCompatibilityMode = SendMessage(GetDlgItem(hwnd, IDC_COMPATIBILITYMODE), BM_GETCHECK, 0, 0) == BST_CHECKED;
                        BOOL bUsePointFiltering = SendMessage(GetDlgItem(hwnd, IDC_POINTFILTERING), BM_GETCHECK, 0, 0) == BST_CHECKED;

                        //---------------------------------

                        XElement *data = info->data;

                        data->SetInt(TEXT("captureType"),   captureType);

                        data->SetInt(TEXT("monitor"),       monitorID);

                        if(captureType == 1)
                        {
                            data->SetString(TEXT("window"),     strWindow);
                            data->SetString(TEXT("windowClass"),info->strClasses[windowID]);
                        }

                        data->SetInt(TEXT("innerWindow"),   bInnerWindow);

                        data->SetInt(TEXT("regionCapture"), bRegion);

                        data->SetInt(TEXT("captureMouse"),  bCaptureMouse);

                        data->SetInt(TEXT("captureLayered"), bCaptureLayered);

                        data->SetInt(TEXT("compatibilityMode"), bCompatibilityMode);

                        data->SetInt(TEXT("usePointFiltering"), bUsePointFiltering);

                        data->SetInt(TEXT("captureX"),      posX);
                        data->SetInt(TEXT("captureY"),      posY);
                        data->SetInt(TEXT("captureCX"),     sizeX);
                        data->SetInt(TEXT("captureCY"),     sizeY);

                        //---------------------------------

                        BOOL  bUseColorKey  = SendMessage(GetDlgItem(hwnd, IDC_USECOLORKEY), BM_GETCHECK, 0, 0) == BST_CHECKED;
                        DWORD keyColor      = CCGetColor(GetDlgItem(hwnd, IDC_COLOR));
                        UINT  keySimilarity = (UINT)SendMessage(GetDlgItem(hwnd, IDC_BASETHRESHOLD), UDM_GETPOS32, 0, 0);
                        UINT  keyBlend      = (UINT)SendMessage(GetDlgItem(hwnd, IDC_BLEND), UDM_GETPOS32, 0, 0);

                        data->SetInt(TEXT("useColorKey"), bUseColorKey);
                        data->SetInt(TEXT("keyColor"), keyColor);
                        data->SetInt(TEXT("keySimilarity"), keySimilarity);
                        data->SetInt(TEXT("keyBlend"), keyBlend);

                        //---------------------------------

                        UINT opacity = (UINT)SendMessage(GetDlgItem(hwnd, IDC_OPACITY2), UDM_GETPOS32, 0, 0);
                        data->SetInt(TEXT("opacity"), opacity);

                        //---------------------------------

                        int gamma = (int)SendMessage(GetDlgItem(hwnd, IDC_GAMMA), TBM_GETPOS, 0, 0);
                        data->SetInt(TEXT("gamma"), gamma);

                        if (captureType == 0 && OSGetVersion() < 8)
                        {
                            BOOL bComposition;
                            DwmIsCompositionEnabled(&bComposition);
                            
                            if (bComposition)
                                OBSMessageBox (hwnd, Str("Sources.SoftwareCaptureSource.WarningAero"), TEXT("Warning"), MB_ICONEXCLAMATION);
                        }
                    }

                case IDCANCEL:
                    if(LOWORD(wParam) == IDCANCEL)
                    {
                        ConfigDesktopSourceInfo *info = (ConfigDesktopSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                        ImageSource *source = API->GetSceneImageSource(info->lpName);
                        if(source)
                        {
                            XElement *data = info->data;
                            source->SetInt(TEXT("useColorKey"),   data->GetInt(TEXT("useColorKey"), 0));
                            source->SetInt(TEXT("keyColor"),      data->GetInt(TEXT("keyColor"), 0xFFFFFFFF));
                            source->SetInt(TEXT("keySimilarity"), data->GetInt(TEXT("keySimilarity"), 10));
                            source->SetInt(TEXT("keyBlend"),      data->GetInt(TEXT("keyBlend"), 0));
                            source->SetInt(TEXT("opacity"),       data->GetInt(TEXT("opacity"), 100));
                            source->SetInt(TEXT("gamma"),         data->GetInt(TEXT("gamma"), 100));
                        }
                    }

                    EndDialog(hwnd, LOWORD(wParam));
                    break;
            }
    }

    return FALSE;
}

bool bMadeCaptureRegionClass = false;

static bool STDCALL ConfigureDesktopSource2(XElement *element, bool bInitialize, int dialogID)
{
    if(!bMadeCaptureRegionClass)
    {
        WNDCLASS wc;
        zero(&wc, sizeof(wc));
        wc.hInstance = hinstMain;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = CAPTUREREGIONCLASS;
        wc.lpfnWndProc = (WNDPROC)RegionWindowProc;
        wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
        RegisterClass(&wc);

        bMadeCaptureRegionClass = true;
    }

    if(!element)
    {
        AppWarning(TEXT("ConfigureDesktopSource: NULL element"));
        return false;
    }

    XElement *data = element->GetElement(TEXT("data"));
    if(!data)
        data = element->CreateElement(TEXT("data"));

    ConfigDesktopSourceInfo info;
    info.data = data;
    info.lpName = element->GetName();
    info.dialogID = dialogID;

    if(OBSDialogBox(hinstMain, MAKEINTRESOURCE(dialogID), hwndMain, ConfigDesktopSourceProc, (LPARAM)&info) == IDOK)
    {
        element->SetInt(TEXT("cx"), data->GetInt(TEXT("captureCX")));
        element->SetInt(TEXT("cy"), data->GetInt(TEXT("captureCY")));
        return true;
    } else {
        if (info.sizeSet) {
            AppConfig->SetInt(TEXT("Video"), TEXT("BaseWidth"), info.prevCX);
            AppConfig->SetInt(TEXT("Video"), TEXT("BaseHeight"), info.prevCY);

            if(!App->IsRunning())
                App->ResizeWindow(false);
        }
    }

    return false;
}

bool STDCALL ConfigureDesktopSource(XElement *element, bool bInitialize)
{
    return ConfigureDesktopSource2(element, bInitialize, IDD_CONFIGUREDESKTOPSOURCE);
}

bool STDCALL ConfigureWindowCaptureSource(XElement *element, bool bInitialize)
{
    return ConfigureDesktopSource2(element, bInitialize, IDD_CONFIGUREWINDOWCAPTURE);
}

bool STDCALL ConfigureMonitorCaptureSource(XElement *element, bool bInitialize)
{
    return ConfigureDesktopSource2(element, bInitialize, IDD_CONFIGUREMONITORCAPTURE);
}
