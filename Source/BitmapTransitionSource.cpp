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

extern "C" double round(double val);

class BitmapTransitionSource : public ImageSource
{
    List<Texture*> textures;

    Vect2    fullSize;
    double   baseAspect;

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
        traceIn(BitmapTransitionSource::BitmapImageSource);

        this->data = data;
        UpdateSettings();

        traceOut;
    }

    ~BitmapTransitionSource()
    {
        for(UINT i=0; i<textures.Num(); i++)
            delete textures[i];
    }

    void Tick(float fSeconds)
    {
        if(bTransitioning)
        {
            curFadeValue += fSeconds;

            if(curFadeValue >= fadeTime)
            {
                curFadeValue = 0.0f;
                bTransitioning = false;

                if(++curTexture == textures.Num())
                    curTexture = 0;
            }
        }

        curTransitionTime += fSeconds;
        if(curTransitionTime >= transitionTime)
        {
            curTransitionTime -= transitionTime;

            curFadeValue = 0.0f;
            bTransitioning = true;
        }
    }

    void DrawBitmap(UINT texID, float alpha, const Vect2 &startPos, const Vect2 &startSize)
    {
        DWORD curAlpha = DWORD(alpha*255.0f);

        Vect2 pos = Vect2(0.0f, 0.0f);
        Vect2 size = fullSize;

        Vect2 itemSize = Vect2((float)textures[texID]->Width(), (float)textures[texID]->Height());

        double sourceAspect = double(itemSize.x)/double(itemSize.y);
        if(!CloseDouble(baseAspect, sourceAspect))
        {
            if(baseAspect < sourceAspect)
                size.y = float(double(size.x) / sourceAspect);
            else
                size.x = float(double(size.y) * sourceAspect);

            pos = (fullSize-size)*0.5f;

            pos.x = (float)round(pos.x);
            pos.y = (float)round(pos.y);

            size.x = (float)round(size.x);
            size.y = (float)round(size.y);
        }

        pos /= fullSize;
        pos *= startSize;
        pos += startPos;
        Vect2 lr;
        lr = pos + (size/fullSize*startSize);

        DrawSprite(textures[texID], (curAlpha<<24) | 0xFFFFFF, pos.x, pos.y, lr.x, lr.y);
    }

    void Render(const Vect2 &pos, const Vect2 &size)
    {
        traceIn(BitmapTransitionSource::Render);

        if(textures.Num())
        {
            if(bTransitioning && textures.Num() > 1)
            {
                float curAlpha = MIN(curFadeValue/fadeTime, 1.0f);
                DrawBitmap(curTexture, 1.0f-curAlpha, pos, size);

                UINT nextTexture = (curTexture == textures.Num()-1) ? 0 : curTexture+1;
                DrawBitmap(nextTexture, curAlpha, pos, size);
            }
            else
                DrawBitmap(curTexture, 1.0f, pos, size);
        }

        traceOut;
    }

    void UpdateSettings()
    {
        traceIn(BitmapTransitionSource::UpdateSettings);

        for(UINT i=0; i<textures.Num(); i++)
            delete textures[i];
        textures.Clear();

        //------------------------------------

        bool bFirst = true;

        StringList bitmapList;
        data->GetStringList(TEXT("bitmap"), bitmapList);
        for(UINT i=0; i<bitmapList.Num(); i++)
        {
            String &strBitmap = bitmapList[i];
            if(strBitmap.IsEmpty())
            {
                AppWarning(TEXT("BitmapTransitionSource::UpdateSettings: Empty path"));
                continue;
            }

            Texture *texture = GS->CreateTextureFromFile(strBitmap, TRUE);
            if(!texture)
            {
                AppWarning(TEXT("BitmapTransitionSource::UpdateSettings: could not create texture '%s'"), strBitmap.Array());
                continue;
            }

            if(bFirst)
            {
                fullSize.x = float(texture->Width());
                fullSize.y = float(texture->Height());
                baseAspect = double(fullSize.x)/double(fullSize.y);
                bFirst = false;
            }

            textures << texture;
        }

        if(textures.Num() == 0)
            CreateErrorTexture();

        //------------------------------------

        transitionTime = data->GetFloat(TEXT("transitionTime"));
        if(transitionTime < 5)
            transitionTime = 5;
        else if(transitionTime > 30)
            transitionTime = 30;

        //------------------------------------

        curTransitionTime = 0.0f;
        curTexture = 0;
        bTransitioning = false;
        curFadeValue = 0.0f;

        traceOut;
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
                EnableWindow(GetDlgItem(hwnd, IDC_MOVEUPWARD), FALSE);
                EnableWindow(GetDlgItem(hwnd, IDC_MOVEDOWNWARD), FALSE);

                return TRUE;
            }

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_ADD:
                    {
                        TSTR lpFile = (TSTR)Allocate(32*1024*sizeof(TCHAR));
                        zero(lpFile, 32*1024*sizeof(TCHAR));

                        OPENFILENAME ofn;
                        zero(&ofn, sizeof(ofn));
                        ofn.lStructSize = sizeof(ofn);
                        ofn.lpstrFile = lpFile;
                        ofn.hwndOwner = hwnd;
                        ofn.nMaxFile = 32*1024*sizeof(TCHAR);
                        ofn.lpstrFilter = TEXT("All Formats (*.bmp;*.dds;*.jpg;*.png;*.gif)\0*.bmp;*.dds;*.jpg;*.png;*.gif\0");
                        ofn.nFilterIndex = 1;
                        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

                        TCHAR curDirectory[MAX_PATH+1];
                        GetCurrentDirectory(MAX_PATH, curDirectory);

                        BOOL bOpenFile = GetOpenFileName(&ofn);

                        TCHAR newDirectory[MAX_PATH+1];
                        GetCurrentDirectory(MAX_PATH, newDirectory);

                        SetCurrentDirectory(curDirectory);

                        if(bOpenFile)
                        {
                            TSTR lpCurFile = lpFile+ofn.nFileOffset;

                            while(lpCurFile && *lpCurFile)
                            {
                                String strPath;
                                strPath << newDirectory << TEXT("\\") << lpCurFile;

                                UINT idExisting = (UINT)SendMessage(GetDlgItem(hwnd, IDC_BITMAPS), LB_FINDSTRINGEXACT, -1, (LPARAM)strPath.Array());
                                if(idExisting == LB_ERR)
                                    SendMessage(GetDlgItem(hwnd, IDC_BITMAPS), LB_ADDSTRING, 0, (LPARAM)strPath.Array());

                                lpCurFile += slen(lpCurFile)+1;
                            }
                        }

                        Free(lpFile);

                        break;
                    }

                case IDC_BITMAPS:
                    if(HIWORD(wParam) == LBN_SELCHANGE)
                    {
                        EnableWindow(GetDlgItem(hwnd, IDC_REPLACE), TRUE);
                        EnableWindow(GetDlgItem(hwnd, IDC_REMOVE), TRUE);
                        EnableWindow(GetDlgItem(hwnd, IDC_MOVEUPWARD), TRUE);
                        EnableWindow(GetDlgItem(hwnd, IDC_MOVEDOWNWARD), TRUE);
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
                            EnableWindow(GetDlgItem(hwnd, IDC_MOVEUPWARD), FALSE);
                            EnableWindow(GetDlgItem(hwnd, IDC_MOVEDOWNWARD), FALSE);
                        }
                    }
                    break;

                case IDC_MOVEUPWARD:
                    {
                        HWND hwndBitmaps = GetDlgItem(hwnd, IDC_BITMAPS);
                        UINT curSel = (UINT)SendMessage(hwndBitmaps, LB_GETCURSEL, 0, 0);
                        if(curSel != LB_ERR)
                        {
                            if(curSel > 0)
                            {
                                String strText = GetLBText(hwndBitmaps, curSel);

                                SendMessage(hwndBitmaps, LB_DELETESTRING, curSel, 0);
                                SendMessage(hwndBitmaps, LB_INSERTSTRING, --curSel, (LPARAM)strText.Array());
                                PostMessage(hwndBitmaps, LB_SETCURSEL, curSel, 0);
                            }
                        }
                    }
                    break;

                case IDC_MOVEDOWNWARD:
                    {
                        HWND hwndBitmaps = GetDlgItem(hwnd, IDC_BITMAPS);

                        UINT numBitmaps = (UINT)SendMessage(hwndBitmaps, LB_GETCOUNT, 0, 0);
                        UINT curSel = (UINT)SendMessage(hwndBitmaps, LB_GETCURSEL, 0, 0);
                        if(curSel != LB_ERR)
                        {
                            if(curSel < (numBitmaps-1))
                            {
                                String strText = GetLBText(hwndBitmaps, curSel);

                                SendMessage(hwndBitmaps, LB_DELETESTRING, curSel, 0);
                                SendMessage(hwndBitmaps, LB_INSERTSTRING, ++curSel, (LPARAM)strText.Array());
                                PostMessage(hwndBitmaps, LB_SETCURSEL, curSel, 0);
                            }
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
            element->SetInt(TEXT("cx"), configInfo.cx);
            element->SetInt(TEXT("cy"), configInfo.cy);
        }

        return true;
    }

    return false;
}
