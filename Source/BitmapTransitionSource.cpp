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


const float fadeTime = 1.5f;


class BitmapTransitionSource : public ImageSource
{
    List<Texture*> textures;

    Vect2    fullSize;
    XElement *data;

    float transitionTime;

    UINT  curTexture;
    float curTransitionTime;
    float curFadeValue;
    bool  bTransitioning;

    void CreateErrorTexture()
    {
        LPBYTE textureData = (LPBYTE)Allocate(32*32*4);
        msetd(textureData, 0xFF0000FF, 32*32*4);

        textures << CreateTexture(32, 32, GS_RGB, textureData, FALSE);
        fullSize.Set(32.0f, 32.0f);

        Free(textureData);
    }

public:
    BitmapTransitionSource(XElement *data)
    {
        //traceIn(BitmapTransitionSource::BitmapImageSource);

        this->data = data;
        UpdateSettings();

        //traceOut;
    }

    ~BitmapTransitionSource()
    {
        for(UINT i=0; i<textures.Num(); i++)
            delete textures[i];
    }

    void Tick(float fSeconds)
    {
        curTransitionTime += fSeconds;
        if(curTransitionTime >= transitionTime)
        {
            curTransitionTime -= transitionTime;

            curFadeValue = 0.0f;
            bTransitioning = true;
        }

        if(bTransitioning)
        {
            curFadeValue -= fSeconds;

            if(curFadeValue >= fadeTime)
            {
                curFadeValue = 0.0f;
                bTransitioning = false;

                if(++curTexture == textures.Num())
                    curTexture = 0;
            }
        }
    }

    void Render(const Vect2 &pos, const Vect2 &size)
    {
        //traceIn(BitmapTransitionSource::Render);

        if(textures.Num())
        {
            DrawSprite(textures[curTexture], pos.x, pos.y, pos.x+size.x, pos.y+size.y);

            if(bTransitioning && textures.Num() > 1)
            {
                UINT nextTexture = (curTexture == textures.Num()-1) ? 0 : curTexture+1;
                BlendFunction(GS_BLEND_FACTOR, GS_BLEND_INVFACTOR, 1.0f-(curFadeValue/fadeTime));
            }
        }

        //traceOut;
    }

    void UpdateSettings()
    {
        //traceIn(BitmapTransitionSource::UpdateSettings);

        for(UINT i=0; i<textures.Num(); i++)
            delete textures[i];
        textures.Clear();

        //------------------------------------

        bool bFirst = true;

        UINT numBitmaps = data->NumDataItems(TEXT("bitmap"));
        for(UINT i=0; i<numBitmaps; i++)
        {
            XDataItem *bitmapItem = data->GetDataItemByID(i);

            CTSTR lpBitmap = bitmapItem->GetData();
            if(!lpBitmap || !*lpBitmap)
            {
                AppWarning(TEXT("BitmapTransitionSource::UpdateSettings: Empty path"));
                continue;
            }

            Texture *texture = GS->CreateTextureFromFile(lpBitmap, TRUE);
            if(!texture)
            {
                AppWarning(TEXT("BitmapTransitionSource::UpdateSettings: could not create texture '%s'"), lpBitmap);
                continue;
            }

            if(bFirst)
            {
                fullSize.x = float(texture->Width());
                fullSize.y = float(texture->Height());
                bFirst = false;
            }

            textures << texture;
        }

        if(textures.Num() == 0)
            CreateErrorTexture();

        curTexture = 0;
        curTransitionTime = 0.0f;
        bTransitioning = false;
        curFadeValue = 0.0f;

        //traceOut;
    }

    Vect2 GetSize() const {return fullSize;}
};


ImageSource* STDCALL CreateBitmapTransitionSource(XElement *data)
{
    if(!data)
        return NULL;

    return new BitmapTransitionSource(data);
}

struct ConfigBitmapInfo
{
    XElement *data;
    UINT cx, cy;
};

INT_PTR CALLBACK ConfigureBitmapTransitionProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                ConfigBitmapInfo *configInfo = (ConfigBitmapInfo*)lParam;
                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)configInfo);
                LocalizeWindow(hwnd);

                //--------------------------

                HWND hwndTemp = GetDlgItem(hwnd, IDC_BITMAPS);

                StringList bitmapList;
                configInfo->data->GetStringList(TEXT("bitmap"), bitmapList);
                for(UINT i=0; i<bitmapList.Num(); i++)
                {
                    CTSTR lpBitmap = bitmapList[i];

                    if(OSFileExists(lpBitmap))
                        SendMessage(hwndTemp, LB_ADDSTRING, 0, (LPARAM)lpBitmap);
                }

                //--------------------------

                hwndTemp = GetDlgItem(hwnd, IDC_TRANSITIONTIME);

                UINT transitionTime = configInfo->data->GetInt(TEXT("transitionTime"));
                SendMessage(hwndTemp, UDM_SETRANGE32, 5, 30);

                if(!transitionTime)
                    transitionTime = 10;

                SendMessage(hwndTemp, UDM_SETPOS32, 0, transitionTime);

                EnableWindow(GetDlgItem(hwnd, IDC_REPLACE), FALSE);
                EnableWindow(GetDlgItem(hwnd, IDC_REMOVE), FALSE);

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

                case IDC_BITMAPS:
                    if(HIWORD(wParam) == LBN_SELCHANGE)
                    {
                        EnableWindow(GetDlgItem(hwnd, IDC_REPLACE), TRUE);
                        EnableWindow(GetDlgItem(hwnd, IDC_REMOVE), TRUE);
                    }
                    break;

                case IDC_ADD:
                    {
                        String strBitmap = GetEditText(GetDlgItem(hwnd, IDC_BITMAP));
                        if(strBitmap.IsValid())
                        {
                            UINT idExisting = (UINT)SendMessage(GetDlgItem(hwnd, IDC_BITMAPS), LB_FINDSTRINGEXACT, -1, (LPARAM)strBitmap.Array());
                            if(idExisting == LB_ERR)
                                SendMessage(GetDlgItem(hwnd, IDC_BITMAPS), LB_ADDSTRING, 0, (LPARAM)strBitmap.Array());
                        }
                    }
                    break;

                case IDC_REPLACE:
                    {
                        String strBitmap = GetEditText(GetDlgItem(hwnd, IDC_BITMAP));
                        if(strBitmap.IsValid())
                        {
                            UINT idExisting = (UINT)SendMessage(GetDlgItem(hwnd, IDC_BITMAPS), LB_FINDSTRINGEXACT, -1, (LPARAM)strBitmap.Array());
                            if(idExisting == LB_ERR)
                            {
                                UINT curSel = (UINT)SendMessage(GetDlgItem(hwnd, IDC_BITMAPS), LB_GETCURSEL, 0, 0);
                                SendMessage(GetDlgItem(hwnd, IDC_BITMAPS), LB_DELETESTRING, curSel, 0);
                                SendMessage(GetDlgItem(hwnd, IDC_BITMAPS), LB_INSERTSTRING, curSel, (LPARAM)strBitmap.Array());
                                PostMessage(GetDlgItem(hwnd, IDC_BITMAPS), LB_SETCURSEL, curSel, 0);
                            }
                        }
                    }
                    break;

                case IDC_REMOVE:
                    {
                        UINT curSel = (UINT)SendMessage(GetDlgItem(hwnd, IDC_BITMAPS), LB_GETCURSEL, 0, 0);
                        if(curSel != LB_ERR)
                        {
                            SendMessage(GetDlgItem(hwnd, IDC_BITMAPS), LB_DELETESTRING, curSel, 0);
                            EnableWindow(GetDlgItem(hwnd, IDC_REPLACE), FALSE);
                            EnableWindow(GetDlgItem(hwnd, IDC_REMOVE), FALSE);
                        }
                    }
                    break;

                case IDOK:
                    {
                        HWND hwndBitmaps = GetDlgItem(hwnd, IDC_BITMAPS);

                        UINT numBitmaps = (UINT)SendMessage(hwndBitmaps, LB_GETCOUNT, 0, 0);
                        if(!numBitmaps)
                        {
                            MessageBox(hwnd, Str("Sources.TransitionSource.Empty"), NULL, 0);
                            break;
                        }

                        //---------------------------

                        StringList bitmapList;
                        for(UINT i=0; i<numBitmaps; i++)
                            bitmapList << GetLBText(hwndBitmaps, i);

                        ConfigBitmapInfo *configInfo = (ConfigBitmapInfo*)GetWindowLongPtr(hwnd, DWLP_USER);

                        D3DX10_IMAGE_INFO ii;
                        if(SUCCEEDED(D3DX10GetImageInfoFromFile(bitmapList[0], NULL, &ii, NULL)))
                        {
                            configInfo->cx = ii.Width;
                            configInfo->cy = ii.Height;
                        }
                        else
                        {
                            configInfo->cx = configInfo->cy = 32;
                            AppWarning(TEXT("ConfigureBitmapTransitionSource: could not get image info for bitmap '%s'"), bitmapList[0].Array());
                        }

                        configInfo->data->SetStringList(TEXT("bitmap"), bitmapList);

                        UINT transitionTime = (UINT)SendMessage(GetDlgItem(hwnd, IDC_TRANSITIONTIME), UDM_GETPOS32, 0, 0);
                        configInfo->data->SetInt(TEXT("transitionTime"), transitionTime);
                    }

                case IDCANCEL:
                    EndDialog(hwnd, LOWORD(wParam));
                    break;
            }
            break;
    }

    return 0;
}

bool STDCALL ConfigureBitmapTransitionSource(XElement *element, bool bCreating)
{
    if(!element)
    {
        AppWarning(TEXT("ConfigureBitmapTransitionSource: NULL element"));
        return false;
    }

    XElement *data = element->GetElement(TEXT("data"));
    if(!data)
        data = element->CreateElement(TEXT("data"));

    ConfigBitmapInfo configInfo;
    configInfo.data = data;

    if(DialogBoxParam(hinstMain, MAKEINTRESOURCE(IDD_CONFIGURETRANSITIONSOURCE), hwndMain, ConfigureBitmapTransitionProc, (LPARAM)&configInfo) == IDOK)
    {
        if(bCreating)
        {
            CTSTR lpBitmap = data->GetString(TEXT("path"));

            D3DX10_IMAGE_INFO ii;
            if(SUCCEEDED(D3DX10GetImageInfoFromFile(lpBitmap, NULL, &ii, NULL)))
            {
                element->SetInt(TEXT("cx"), ii.Width);
                element->SetInt(TEXT("cy"), ii.Height);
            }
            else
                AppWarning(TEXT("ConfigureBitmapTransitionSource: could not get image info for bitmap '%s'"), lpBitmap);
        }

        return true;
    }

    return false;
}
