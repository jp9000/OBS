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


class BitmapImageSource : public ImageSource
{
    Texture  *texture;

    Vect2    fullSize;
    XElement *data;

    void CreateErrorTexture()
    {
        LPBYTE textureData = (LPBYTE)Allocate(32*32*4);
        msetd(textureData, 0xFF0000FF, 32*32*4);

        texture = CreateTexture(32, 32, GS_RGB, textureData, FALSE);
        fullSize.Set(32.0f, 32.0f);

        Free(textureData);
    }

public:
    BitmapImageSource(XElement *data)
    {
        traceIn(BitmapImageSource::BitmapImageSource);

        this->data = data;
        UpdateSettings();

        traceOut;
    }

    ~BitmapImageSource()
    {
        delete texture;
    }

    void Render(const Vect2 &pos, const Vect2 &size)
    {
        traceIn(BitmapImageSource::Render);

        if(texture)
            DrawSprite(texture, pos.x, pos.y, pos.x+size.x, pos.y+size.y);

        traceOut;
    }

    void UpdateSettings()
    {
        traceIn(BitmapImageSource::UpdateSettings);

        delete texture;
        texture = NULL;

        CTSTR lpBitmap = data->GetString(TEXT("path"));
        if(!lpBitmap || !*lpBitmap)
        {
            AppWarning(TEXT("BitmapImageSource::UpdateSettings: Empty path"));
            CreateErrorTexture();
            return;
        }

        //------------------------------------

        texture = GS->CreateTextureFromFile(lpBitmap, TRUE);
        if(!texture)
        {
            AppWarning(TEXT("BitmapImageSource::UpdateSettings: could not create texture '%s'"), lpBitmap);
            CreateErrorTexture();
            return;
        }

        fullSize.x = float(texture->Width());
        fullSize.y = float(texture->Height());

        //------------------------------------

        traceOut;
    }

    Vect2 GetSize() const {return fullSize;}
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
    bool bResetSize;
};

INT_PTR CALLBACK ConfigureBitmapProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                ConfigBitmapInfo *configInfo = (ConfigBitmapInfo*)lParam;
                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)configInfo);
                LocalizeWindow(hwnd);

                CTSTR lpBitmap = configInfo->data->GetString(TEXT("path"));
                SetWindowText(GetDlgItem(hwnd, IDC_BITMAP), lpBitmap);

                SendMessage(GetDlgItem(hwnd, IDC_RESETSIZE), BM_SETCHECK, BST_CHECKED, 0);
                return TRUE;
            }

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

                case IDOK:
                    {
                        String strBitmap = GetEditText(GetDlgItem(hwnd, IDC_BITMAP));
                        if(strBitmap.IsEmpty())
                        {
                            MessageBox(hwnd, Str("Sources.BitmapSource.Empty"), NULL, 0);
                            break;
                        }

                        ConfigBitmapInfo *configInfo = (ConfigBitmapInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                        configInfo->data->SetString(TEXT("path"), strBitmap);
                        configInfo->bResetSize = SendMessage(GetDlgItem(hwnd, IDC_RESETSIZE), BM_GETCHECK, 0, 0) == BST_CHECKED;
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

    if(DialogBoxParam(hinstMain, MAKEINTRESOURCE(IDD_CONFIGUREBITMAPSOURCE), hwndMain, ConfigureBitmapProc, (LPARAM)&configInfo) == IDOK)
    {
        if(bCreating || configInfo.bResetSize)
        {
            CTSTR lpBitmap = data->GetString(TEXT("path"));

            D3DX11_IMAGE_INFO ii;
            if(SUCCEEDED(D3DX11GetImageInfoFromFile(lpBitmap, NULL, &ii, NULL)))
            {
                element->SetInt(TEXT("cx"), ii.Width);
                element->SetInt(TEXT("cy"), ii.Height);
            }
            else
                AppWarning(TEXT("ConfigureBitmapSource: could not get image info for bitmap '%s'"), lpBitmap);
        }

        return true;
    }

    return false;
}
