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
#include "BitmapImage.h"


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

struct ConfigDesktopSourceInfo
{
    CTSTR lpName;
    XElement *data;
    StringList strClasses;
};


class BitmapImageSource : public ImageSource
{
    BitmapImage bitmapImage;

    XElement *data;

    bool     bUseColorKey;
    DWORD    keyColor;
    UINT     keySimilarity, keyBlend;

    DWORD opacity;
    DWORD color;

    Shader   *colorKeyShader, *alphaIgnoreShader;


public:
    BitmapImageSource(XElement *data)
    {
        this->data = data;
       
        UpdateSettings();

        colorKeyShader      = CreatePixelShaderFromFile(TEXT("shaders\\ColorKey_RGB.pShader"));
        alphaIgnoreShader   = CreatePixelShaderFromFile(TEXT("shaders\\AlphaIgnore.pShader"));
        
        Log(TEXT("Using bitmap image"));
    }

    ~BitmapImageSource()
    {
        delete colorKeyShader;
        delete alphaIgnoreShader;
    }

    void Tick(float fSeconds)
    {
        bitmapImage.Tick(fSeconds);
    }

    void Render(const Vect2 &pos, const Vect2 &size)
    {
        Texture *texture = bitmapImage.GetTexture();

        if(texture)
        {
            if(bUseColorKey)
            {
                Shader *lastPixelShader = GetCurrentPixelShader();
                DWORD alpha = ((opacity*255/100)&0xFF);
                DWORD outputColor = (alpha << 24) | color&0xFFFFFF;
                LoadPixelShader(colorKeyShader);

                float fSimilarity = float(keySimilarity)*0.01f;
                float fBlend      = float(keyBlend)*0.01f;

                colorKeyShader->SetColor(colorKeyShader->GetParameter(2), keyColor);
                colorKeyShader->SetFloat(colorKeyShader->GetParameter(3), fSimilarity);
                colorKeyShader->SetFloat(colorKeyShader->GetParameter(4), fBlend);

                DrawSprite(texture, outputColor, pos.x, pos.y, pos.x+size.x, pos.y+size.y);
                LoadPixelShader(lastPixelShader);
            }
            else
            {
                DWORD alpha = ((opacity*255/100)&0xFF);
                DWORD outputColor = (alpha << 24) | color&0xFFFFFF;
                DrawSprite(texture, outputColor, pos.x, pos.y, pos.x+size.x, pos.y+size.y);
            }
        }
    }

    void UpdateSettings()
    {
        bitmapImage.SetPath(data->GetString(TEXT("path")));
        bitmapImage.EnableFileMonitor(data->GetInt(TEXT("monitor"), 0) == 1);
        bitmapImage.Init();

        //------------------------------------

        opacity = data->GetInt(TEXT("opacity"), 100);
        color = data->GetInt(TEXT("color"), 0xFFFFFFFF);
        if(opacity > 100)
            opacity = 100;

        bool bNewUseColorKey = data->GetInt(TEXT("useColorKey"), 0) != 0;
        keyColor        = data->GetInt(TEXT("keyColor"), 0xFFFFFFFF);
        keySimilarity   = data->GetInt(TEXT("keySimilarity"), 10);
        keyBlend        = data->GetInt(TEXT("keyBlend"), 0);

        bUseColorKey = bNewUseColorKey;
    }

    Vect2 GetSize() const {return bitmapImage.GetSize();}
};


ImageSource* STDCALL CreateBitmapSource(XElement *data)
{
    if(!data)
        return NULL;

    return new BitmapImageSource(data);
}

struct ConfigBitmapInfo
{
    XElement *data;
};

INT_PTR CALLBACK ConfigureBitmapProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static bool bSelectingColor = false;
    static bool bMouseDown = false;
    static ColorSelectionData colorData;

    switch(message)
    {
        case WM_INITDIALOG:
            {
                ConfigBitmapInfo *configInfo = (ConfigBitmapInfo*)lParam;
                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)configInfo);
                LocalizeWindow(hwnd);

                //--------------------------

                CTSTR lpBitmap = configInfo->data->GetString(TEXT("path"));
                SetWindowText(GetDlgItem(hwnd, IDC_BITMAP), lpBitmap);

                //--------------------------

                int opacity = configInfo->data->GetInt(TEXT("opacity"), 100);
                if(opacity > 100)
                    opacity = 100;
                else if(opacity < 0)
                    opacity = 0;

                SendMessage(GetDlgItem(hwnd, IDC_OPACITY), UDM_SETRANGE32, 0, 100);
                SendMessage(GetDlgItem(hwnd, IDC_OPACITY), UDM_SETPOS32, 0, opacity);

                //--------------------------

                DWORD color = configInfo->data->GetInt(TEXT("color"), 0xFFFFFFFF);
                DWORD colorkey = configInfo->data->GetInt(TEXT("keyColor"), 0xFFFFFFFF);
                UINT  similarity    = configInfo->data->GetInt(TEXT("keySimilarity"), 10);
                UINT  blend         = configInfo->data->GetInt(TEXT("keyBlend"), 0);

                CCSetColor(GetDlgItem(hwnd, IDC_COLOR), color);
                CCSetColor(GetDlgItem(hwnd, IDC_KEYCOLOR), colorkey);

                SendMessage(GetDlgItem(hwnd, IDC_BASETHRESHOLD), UDM_SETRANGE32, 0, 100);
                SendMessage(GetDlgItem(hwnd, IDC_BASETHRESHOLD), UDM_SETPOS32, 0, similarity);

                SendMessage(GetDlgItem(hwnd, IDC_BLEND), UDM_SETRANGE32, 0, 100);
                SendMessage(GetDlgItem(hwnd, IDC_BLEND), UDM_SETPOS32, 0, blend);

                //--------------------------

                int monitor = configInfo->data->GetInt(TEXT("monitor"), 0);
                SendMessage(GetDlgItem(hwnd, IDC_MONITOR), BM_SETCHECK, monitor ? BST_CHECKED : BST_UNCHECKED, 0);
                int colorkeyChk = configInfo->data->GetInt(TEXT("useColorKey"), 0);
                SendMessage(GetDlgItem(hwnd, IDC_USECOLORKEY), BM_SETCHECK, colorkeyChk ? BST_CHECKED : BST_UNCHECKED, 0);

                EnableWindow(GetDlgItem(hwnd, IDC_KEYCOLOR), colorkeyChk);
                EnableWindow(GetDlgItem(hwnd, IDC_SELECT), colorkeyChk);
                EnableWindow(GetDlgItem(hwnd, IDC_BASETHRESHOLD_EDIT), colorkeyChk);
                EnableWindow(GetDlgItem(hwnd, IDC_BASETHRESHOLD), colorkeyChk);
                EnableWindow(GetDlgItem(hwnd, IDC_BLEND_EDIT), colorkeyChk);
                EnableWindow(GetDlgItem(hwnd, IDC_BLEND), colorkeyChk);

                return TRUE;
            }

        case WM_LBUTTONDOWN:
            if(bSelectingColor)
            {
                bMouseDown = true;
                CCSetColor(GetDlgItem(hwnd, IDC_KEYCOLOR), colorData.GetColor());
                ConfigureBitmapProc(hwnd, WM_COMMAND, MAKEWPARAM(IDC_KEYCOLOR, CCN_CHANGED), (LPARAM)GetDlgItem(hwnd, IDC_KEYCOLOR));
            }
            break;

        case WM_MOUSEMOVE:
            if(bSelectingColor && bMouseDown)
            {
                CCSetColor(GetDlgItem(hwnd, IDC_KEYCOLOR), colorData.GetColor());
                ConfigureBitmapProc(hwnd, WM_COMMAND, MAKEWPARAM(IDC_KEYCOLOR, CCN_CHANGED), (LPARAM)GetDlgItem(hwnd, IDC_KEYCOLOR));
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

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_BROWSE:
                    {
                        TCHAR lpFile[MAX_PATH+1];
                        zero(lpFile, sizeof(lpFile));

                        OPENFILENAME ofn;
                        zero(&ofn, sizeof(ofn));
                        ofn.lStructSize = sizeof(ofn);
                        ofn.lpstrFile = lpFile;
                        ofn.hwndOwner = hwnd;
                        ofn.nMaxFile = MAX_PATH;
                        ofn.lpstrFilter = TEXT("All Formats (*.bmp;*.dds;*.jpg;*.png;*.gif)\0*.bmp;*.dds;*.jpg;*.png;*.gif\0");
                        ofn.nFilterIndex = 1;
                        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                        TCHAR curDirectory[MAX_PATH+1];
                        GetCurrentDirectory(MAX_PATH, curDirectory);

                        BOOL bOpenFile = GetOpenFileName(&ofn);
                        SetCurrentDirectory(curDirectory);

                        if(bOpenFile)
                            SetWindowText(GetDlgItem(hwnd, IDC_BITMAP), lpFile);

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

                        EnableWindow(GetDlgItem(hwnd, IDC_KEYCOLOR), bUseColorKey);
                        EnableWindow(GetDlgItem(hwnd, IDC_SELECT), bUseColorKey);
                        EnableWindow(GetDlgItem(hwnd, IDC_BASETHRESHOLD_EDIT), bUseColorKey);
                        EnableWindow(GetDlgItem(hwnd, IDC_BASETHRESHOLD), bUseColorKey);
                        EnableWindow(GetDlgItem(hwnd, IDC_BLEND_EDIT), bUseColorKey);
                        EnableWindow(GetDlgItem(hwnd, IDC_BLEND), bUseColorKey);
                        break;
                    }

                case IDC_KEYCOLOR:
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
                    break;

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
                        String strBitmap = GetEditText(GetDlgItem(hwnd, IDC_BITMAP));
                        if(strBitmap.IsEmpty())
                        {
                            OBSMessageBox(hwnd, Str("Sources.BitmapSource.Empty"), NULL, 0);
                            break;
                        }

                        ConfigBitmapInfo *configInfo = (ConfigBitmapInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                        configInfo->data->SetString(TEXT("path"), strBitmap);

                        BOOL bFailed;
                        int opacity = (int)SendMessage(GetDlgItem(hwnd, IDC_OPACITY), UDM_GETPOS32, 0, (LPARAM)&bFailed);
                        if(opacity > 100)
                            opacity = 100;
                        else if(opacity < 0)
                            opacity = 0;
                        configInfo->data->SetInt(TEXT("opacity"), bFailed ? 100 : opacity);

                        DWORD color = CCGetColor(GetDlgItem(hwnd, IDC_COLOR));
                        configInfo->data->SetInt(TEXT("color"), color);

                        BOOL  bUseColorKey  = SendMessage(GetDlgItem(hwnd, IDC_USECOLORKEY), BM_GETCHECK, 0, 0) == BST_CHECKED;
                        DWORD keyColor      = CCGetColor(GetDlgItem(hwnd, IDC_KEYCOLOR));
                        UINT  keySimilarity = (UINT)SendMessage(GetDlgItem(hwnd, IDC_BASETHRESHOLD), UDM_GETPOS32, 0, 0);
                        UINT  keyBlend      = (UINT)SendMessage(GetDlgItem(hwnd, IDC_BLEND), UDM_GETPOS32, 0, 0);

                        configInfo->data->SetInt(TEXT("useColorKey"), bUseColorKey);
                        configInfo->data->SetInt(TEXT("keyColor"), keyColor);
                        configInfo->data->SetInt(TEXT("keySimilarity"), keySimilarity);
                        configInfo->data->SetInt(TEXT("keyBlend"), keyBlend);
                        int monitor = (int)SendMessage(GetDlgItem(hwnd, IDC_MONITOR), BM_GETCHECK, 0, 0);

                        if (monitor == BST_CHECKED)
                            configInfo->data->SetInt(TEXT("monitor"), 1);
                        else
                            configInfo->data->SetInt(TEXT("monitor"), 0);
                    }

                case IDCANCEL:
                    EndDialog(hwnd, LOWORD(wParam));
                    break;
            }
            break;
    }

    return 0;
}

bool STDCALL ConfigureBitmapSource(XElement *element, bool bCreating)
{
    if(!element)
    {
        AppWarning(TEXT("ConfigureBitmapSource: NULL element"));
        return false;
    }

    XElement *data = element->GetElement(TEXT("data"));
    if(!data)
        data = element->CreateElement(TEXT("data"));

    ConfigBitmapInfo configInfo;
    configInfo.data = data;

    if(OBSDialogBox(hinstMain, MAKEINTRESOURCE(IDD_CONFIGUREBITMAPSOURCE), hwndMain, ConfigureBitmapProc, (LPARAM)&configInfo) == IDOK)
    {
        CTSTR lpBitmap = data->GetString(TEXT("path"));

        D3DX10_IMAGE_INFO ii;
        if(SUCCEEDED(D3DX10GetImageInfoFromFile(lpBitmap, NULL, &ii, NULL)))
        {
            element->SetInt(TEXT("cx"), ii.Width);
            element->SetInt(TEXT("cy"), ii.Height);
        }
        else
            AppWarning(TEXT("ConfigureBitmapSource: could not get image info for bitmap '%s'"), lpBitmap);

        return true;
    }

    return false;
}
