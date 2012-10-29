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
#include <gdiplus.h>


#define ClampInt(iVal, minVal, maxVal) \
    if(iVal < minVal) iVal = minVal; \
    else if(iVal > maxVal) iVal = maxVal;

class TextOutputSource : public ImageSource
{
    bool        bUpdateTexture;

    String      strCurrentText;
    Texture     *texture;
    float       scrollValue;
    float       showExtentTime;

    int         mode;
    String      strText;
    String      strFile;

    String      strFont;
    int         size;
    DWORD       color;
    UINT        opacity;
    int         scrollSpeed;
    bool        bBold, bItalic, bUnderline, bVertical;

    bool        bUseExtents;
    UINT        extentWidth, extentHeight;
    bool        bWrap;
    int         align;

    Vect2       baseSize;
    SIZE        textureSize;

    String      strDirectory;
    LPBYTE      changeBuffer;
    bool        bMonitoringFileChanges;
    HANDLE      hDirectory;
    OVERLAPPED  directoryChange;

    SamplerState *ss;

    XElement    *data;

    void UpdateTexture()
    {
        if(bMonitoringFileChanges)
        {
            CancelIoEx(hDirectory, &directoryChange);
            CloseHandle(hDirectory);

            bMonitoringFileChanges = false;
        }

        if(mode == 0)
            strCurrentText = strText;
        else if(mode == 1)
        {
            XFile textFile;
            if(strFile.IsValid() && textFile.Open(strFile, XFILE_READ, XFILE_OPENEXISTING))
            {
                textFile.ReadFileToString(strCurrentText);

                strDirectory = strFile;
                strDirectory.FindReplace(TEXT("\\"), TEXT("/"));
                strDirectory = GetPathDirectory(strDirectory);
                strDirectory.FindReplace(TEXT("/"), TEXT("\\"));
                strDirectory << TEXT("\\");

                hDirectory = CreateFile(strDirectory, FILE_LIST_DIRECTORY, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED, NULL);
                if(hDirectory != INVALID_HANDLE_VALUE)
                {
                    DWORD test;
                    zero(&directoryChange, sizeof(directoryChange));

                    if(ReadDirectoryChangesW(hDirectory, changeBuffer, 2048, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE, &test, &directoryChange, NULL))
                        bMonitoringFileChanges = true;
                    else
                    {
                        int err = GetLastError();
                        CloseHandle(hDirectory);
                    }
                }
                else
                {
                    int err = GetLastError();
                    nop();
                }
            }
            else
                strCurrentText = TEXT("[!]");
        }

        HFONT hFont = NULL;

        LOGFONT lf;
        zero(&lf, sizeof(lf));
        lf.lfHeight = size;
        lf.lfWeight = bBold ? FW_BOLD : FW_DONTCARE;
        lf.lfItalic = bItalic;
        lf.lfUnderline = bUnderline;
        lf.lfQuality = ANTIALIASED_QUALITY;
        if(strFont.IsValid())
        {
            scpy_n(lf.lfFaceName, strFont, 31);

            hFont = CreateFontIndirect(&lf);
        }

        if(!hFont)
        {
            scpy_n(lf.lfFaceName, TEXT("Arial"), 31);
            hFont = CreateFontIndirect(&lf);
        }

        if(!hFont)
            return;

        SIZE textSize;
        if(bUseExtents)
        {
            textSize.cx = extentWidth;
            textSize.cy = extentHeight;
        }
        else
        {
            HDC hDC = CreateCompatibleDC(NULL);
            {
                Gdiplus::Font font(hDC, hFont);
                Gdiplus::Graphics graphics(hDC);
                Gdiplus::StringFormat format;

                if(bVertical)
                    format.SetFormatFlags(Gdiplus::StringFormatFlagsDirectionVertical|Gdiplus::StringFormatFlagsDirectionRightToLeft);

                Gdiplus::RectF rcf;
                graphics.MeasureString(strCurrentText, -1, &font, Gdiplus::PointF(0.0f, 0.0f), &format, &rcf);

                textSize.cx = long(rcf.Width+EPSILON);
                textSize.cy = long(rcf.Height+EPSILON);

                if(!textSize.cx || !textSize.cy)
                {
                    DeleteObject(hFont);
                    return;
                }
            }
            DeleteDC(hDC);
        }

        textSize.cx++;
        textSize.cy++;
        textSize.cx &= 0xFFFFFFFE;
        textSize.cy &= 0xFFFFFFFE;

        ClampInt(textSize.cx, 32, 2048);
        ClampInt(textSize.cy, 32, 2048);

        if(textureSize.cx != textSize.cx || textureSize.cy != textSize.cy)
        {
            if(texture)
            {
                delete texture;
                texture = NULL;
            }

            mcpy(&textureSize, &textSize, sizeof(textureSize));
            texture = CreateGDITexture(textSize.cx, textSize.cy);
        }

        if(!texture)
        {
            DeleteObject(hFont);
            return;
        }

        HDC hDC;
        if(texture->GetDC(hDC))
        {
            {
                Gdiplus::Graphics    graphics(hDC);
                Gdiplus::SolidBrush  *brush = new Gdiplus::SolidBrush(Gdiplus::Color(0xFF000000|color));
                Gdiplus::Font        font(hDC, hFont);

                graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
                graphics.Clear(Gdiplus::Color(0));

                if(bUseExtents)
                {
                    Gdiplus::StringFormat format;
                    Gdiplus::PointF pos(0.0f, 0.0f);

                    switch(align)
                    {
                        case 0:
                            if(bVertical)
                                format.SetLineAlignment(Gdiplus::StringAlignmentFar);
                            else
                                format.SetAlignment(Gdiplus::StringAlignmentNear);
                            break;
                        case 1:
                            if(bVertical)
                                format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                            else
                                format.SetAlignment(Gdiplus::StringAlignmentCenter);
                            pos.X = float(textSize.cx/2);
                            break;
                        case 2:
                            if(bVertical)
                                format.SetLineAlignment(Gdiplus::StringAlignmentNear);
                            else
                                format.SetAlignment(Gdiplus::StringAlignmentFar);
                            pos.X = float(textSize.cx);
                            break;
                    }

                    if(bVertical)
                        format.SetFormatFlags(Gdiplus::StringFormatFlagsDirectionVertical|Gdiplus::StringFormatFlagsDirectionRightToLeft);

                    if(bWrap)
                    {
                        Gdiplus::RectF rcf(0.0f, 0.0f, float(textSize.cx), float(textSize.cy));
                        graphics.DrawString(strCurrentText, -1, &font, rcf, &format, brush);
                    }
                    else
                        graphics.DrawString(strCurrentText, -1, &font, pos, &format, brush);
                }
                else
                {
                    Gdiplus::StringFormat format;

                    if(bVertical)
                        format.SetFormatFlags(Gdiplus::StringFormatFlagsDirectionVertical|Gdiplus::StringFormatFlagsDirectionRightToLeft);

                    graphics.DrawString(strCurrentText, -1, &font, Gdiplus::PointF(bVertical ? float(textSize.cx) : 0.0f, 0.0f), &format, brush);
                }

                delete brush;
            }

            texture->ReleaseDC();
        }

        DeleteObject(hFont);
    }

public:
    inline TextOutputSource(XElement *data)
    {
        this->data = data;
        UpdateSettings();

        SamplerInfo si;
        zero(&si, sizeof(si));
        si.addressU = GS_ADDRESS_REPEAT;
        si.addressV = GS_ADDRESS_REPEAT;
        si.borderColor = 0;
        si.filter = GS_FILTER_LINEAR;
        ss = CreateSamplerState(si);

        changeBuffer = (LPBYTE)Allocate(2048);
        zero(changeBuffer, 2048);
    }

    ~TextOutputSource()
    {
        if(texture)
        {
            delete texture;
            texture = NULL;
        }

        delete ss;

        Free(changeBuffer);

        if(bMonitoringFileChanges)
        {
            CancelIoEx(hDirectory, &directoryChange);
            CloseHandle(hDirectory);
        }
    }

    void Preprocess()
    {
        if(bMonitoringFileChanges)
        {
            if(HasOverlappedIoCompleted(&directoryChange))
            {
                bMonitoringFileChanges = false;

                FILE_NOTIFY_INFORMATION *notify = (FILE_NOTIFY_INFORMATION*)changeBuffer;

                String strFileName;
                strFileName.SetLength(notify->FileNameLength);
                scpy_n(strFileName, notify->FileName, notify->FileNameLength/2);
                strFileName.KillSpaces();

                String strFileChanged;
                strFileChanged << strDirectory << strFileName;

                if(strFileChanged.CompareI(strFile))
                    bUpdateTexture = true;

                DWORD test;
                zero(&directoryChange, sizeof(directoryChange));
                zero(changeBuffer, 2048);

                if(ReadDirectoryChangesW(hDirectory, changeBuffer, 2048, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE, &test, &directoryChange, NULL))
                    bMonitoringFileChanges = true;
                else
                    CloseHandle(hDirectory);
            }
        }

        if(bUpdateTexture)
        {
            bUpdateTexture = false;
            UpdateTexture();
        }
    }

    void Tick(float fSeconds)
    {
        if(scrollSpeed != 0)
        {
            scrollValue += fSeconds*float(scrollSpeed)*0.01f;
            while(scrollValue > 1.0f)
                scrollValue -= 1.0f;
            while(scrollValue < -1.0f)
                scrollValue += 1.0f;
        }
    }

    void Render(const Vect2 &pos, const Vect2 &size)
    {
        if(texture)
        {
            Vect2 sizeMultiplier = size/baseSize;
            Vect2 newSize = Vect2(float(textureSize.cx), float(textureSize.cy))*sizeMultiplier;

            DWORD alpha = DWORD(double(opacity)*2.55);
            DWORD outputColor = (alpha << 24) | 0xFFFFFF;

            if(scrollSpeed != 0)
            {
                UVCoord ul(0.0f, 0.0f);
                UVCoord lr(1.0f, 1.0f);

                if(bVertical)
                {
                    /*float sizeVal = float(textureSize.cy);
                    float clampedVal = floorf(scrollValue*sizeVal)/sizeVal;*/
                    ul.y += scrollValue;
                    lr.y += scrollValue;
                }
                else
                {
                    /*float sizeVal = float(textureSize.cx);
                    float clampedVal = floorf(scrollValue*sizeVal)/sizeVal;*/
                    ul.x += scrollValue;
                    lr.x += scrollValue;
                }

                LoadSamplerState(ss);
                DrawSpriteEx(texture, outputColor, pos.x, pos.y, pos.x+newSize.x, pos.y+newSize.y, ul.x, ul.y, lr.x, lr.y);
            }
            else
                DrawSprite(texture, outputColor, pos.x, pos.y, pos.x+newSize.x, pos.y+newSize.y);
        }
    }

    Vect2 GetSize() const
    {
        return baseSize;
    }

    void UpdateSettings()
    {
        strFont     = data->GetString(TEXT("font"), TEXT("Arial"));
        color       = data->GetInt(TEXT("color"), 0xFF000000);
        size        = data->GetInt(TEXT("fontSize"), 12);
        opacity     = data->GetInt(TEXT("textOpacity"), 100);
        scrollSpeed = data->GetInt(TEXT("scrollSpeed"), 0);
        bBold       = data->GetInt(TEXT("bold"), 0) != 0;
        bItalic     = data->GetInt(TEXT("italic"), 0) != 0;
        bWrap       = data->GetInt(TEXT("wrap"), 0) != 0;
        bUnderline  = data->GetInt(TEXT("underline"), 0) != 0;
        bVertical   = data->GetInt(TEXT("vertical"), 0) != 0;
        bUseExtents = data->GetInt(TEXT("useTextExtents"), 0) != 0;
        extentWidth = data->GetInt(TEXT("extentWidth"), 0);
        extentHeight= data->GetInt(TEXT("extentHeight"), 0);
        align       = data->GetInt(TEXT("align"), 0);
        strFile     = data->GetString(TEXT("file"));
        strText     = data->GetString(TEXT("text"));
        mode        = data->GetInt(TEXT("mode"), 0);

        baseSize.x  = data->GetFloat(TEXT("baseSizeCX"), 100);
        baseSize.y  = data->GetFloat(TEXT("baseSizeCY"), 100);

        bUpdateTexture = true;
    }

    void SetString(CTSTR lpName, CTSTR lpVal)
    {
        if(scmpi(lpName, TEXT("font")) == 0)
            strFont = lpVal;
        else if(scmpi(lpName, TEXT("text")) == 0)
            strText = lpVal;
        else if(scmpi(lpName, TEXT("file")) == 0)
            strFile = lpVal;

        bUpdateTexture = true;
    }

    void SetInt(CTSTR lpName, int iValue)
    {
        if(scmpi(lpName, TEXT("color")) == 0)
            color = iValue;
        else if(scmpi(lpName, TEXT("fontSize")) == 0)
            size = iValue;
        else if(scmpi(lpName, TEXT("textOpacity")) == 0)
            opacity = iValue;
        else if(scmpi(lpName, TEXT("scrollSpeed")) == 0)
        {
            if(scrollSpeed == 0)
                scrollValue = 0.0f;
            scrollSpeed = iValue;
        }
        else if(scmpi(lpName, TEXT("bold")) == 0)
            bBold = iValue != 0;
        else if(scmpi(lpName, TEXT("italic")) == 0)
            bItalic = iValue != 0;
        else if(scmpi(lpName, TEXT("wrap")) == 0)
            bWrap = iValue != 0;
        else if(scmpi(lpName, TEXT("underline")) == 0)
            bUnderline = iValue != 0;
        else if(scmpi(lpName, TEXT("vertical")) == 0)
            bVertical = iValue != 0;
        else if(scmpi(lpName, TEXT("useTextExtents")) == 0)
            bUseExtents = iValue != 0;
        else if(scmpi(lpName, TEXT("extentWidth")) == 0)
            extentWidth = iValue;
        else if(scmpi(lpName, TEXT("extentHeight")) == 0)
            extentHeight = iValue;
        else if(scmpi(lpName, TEXT("align")) == 0)
            align = iValue;
        else if(scmpi(lpName, TEXT("mode")) == 0)
            mode = iValue;

        bUpdateTexture = true;
    }
};

struct ConfigTextSourceInfo
{
    CTSTR lpName;
    XElement *data;
    float cx, cy;

    StringList fontNames;
    StringList fontFaces;
};

ImageSource* STDCALL CreateTextSource(XElement *data)
{
    if(!data)
        return NULL;

    return new TextOutputSource(data);
}

int CALLBACK FontEnumProcThingy(ENUMLOGFONTEX *logicalData, NEWTEXTMETRICEX *physicalData, DWORD fontType, ConfigTextSourceInfo *configInfo)
{
    configInfo->fontNames << logicalData->elfFullName;
    configInfo->fontFaces << logicalData->elfLogFont.lfFaceName;

    return 1;
}

void DoCancelStuff(HWND hwnd)
{
    ConfigTextSourceInfo *configInfo = (ConfigTextSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
    ImageSource *source = API->GetSceneImageSource(configInfo->lpName);
    XElement *data = configInfo->data;

    if(source)
        source->UpdateSettings();
}

UINT FindFontFace(ConfigTextSourceInfo *configInfo, HWND hwndFontList, CTSTR lpFontFace)
{
    UINT id = configInfo->fontFaces.FindValueIndexI(lpFontFace);
    if(id == INVALID)
        return INVALID;
    else
    {
        for(UINT i=0; i<configInfo->fontFaces.Num(); i++)
        {
            UINT targetID = (UINT)SendMessage(hwndFontList, CB_GETITEMDATA, i, 0);
            if(targetID == id)
                return i;
        }
    }

    return INVALID;
}

UINT FindFontName(ConfigTextSourceInfo *configInfo, HWND hwndFontList, CTSTR lpFontFace)
{
    return configInfo->fontNames.FindValueIndexI(lpFontFace);
}

CTSTR GetFontFace(ConfigTextSourceInfo *configInfo, HWND hwndFontList)
{
    UINT id = (UINT)SendMessage(hwndFontList, CB_GETCURSEL, 0, 0);
    if(id == CB_ERR)
        return NULL;

    UINT actualID = (UINT)SendMessage(hwndFontList, CB_GETITEMDATA, id, 0);
    return configInfo->fontFaces[actualID];
}

INT_PTR CALLBACK ConfigureTextProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                ConfigTextSourceInfo *configInfo = (ConfigTextSourceInfo*)lParam;
                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)configInfo);
                LocalizeWindow(hwnd);

                XElement *data = configInfo->data;

                //-----------------------------------------

                HDC hDCtest = GetDC(hwnd);

                LOGFONT lf;
                zero(&lf, sizeof(lf));
                EnumFontFamiliesEx(hDCtest, &lf, (FONTENUMPROC)FontEnumProcThingy, (LPARAM)configInfo, 0);

                HWND hwndFonts = GetDlgItem(hwnd, IDC_FONT);
                for(UINT i=0; i<configInfo->fontNames.Num(); i++)
                {
                    int id = (int)SendMessage(hwndFonts, CB_ADDSTRING, 0, (LPARAM)configInfo->fontNames[i].Array());
                    SendMessage(hwndFonts, CB_SETITEMDATA, id, (LPARAM)i);
                }

                CTSTR lpFont = data->GetString(TEXT("font"));
                UINT id = FindFontFace(configInfo, hwndFonts, lpFont);
                if(id == INVALID)
                    id = (UINT)SendMessage(hwndFonts, CB_FINDSTRINGEXACT, -1, (LPARAM)TEXT("Arial"));

                SendMessage(hwndFonts, CB_SETCURSEL, id, 0);

                //-----------------------------------------

                SendMessage(GetDlgItem(hwnd, IDC_TEXTSIZE), UDM_SETRANGE32, 5, 2048);
                SendMessage(GetDlgItem(hwnd, IDC_TEXTSIZE), UDM_SETPOS32, 0, data->GetInt(TEXT("fontSize"), 12));

                //-----------------------------------------

                CCSetColor(GetDlgItem(hwnd, IDC_COLOR), data->GetInt(TEXT("color"), 0xFF000000));

                SendMessage(GetDlgItem(hwnd, IDC_TEXTOPACITY), UDM_SETRANGE32, 0, 100);
                SendMessage(GetDlgItem(hwnd, IDC_TEXTOPACITY), UDM_SETPOS32, 0, data->GetInt(TEXT("textOpacity"), 100));

                SendMessage(GetDlgItem(hwnd, IDC_SCROLLSPEED), UDM_SETRANGE32, -100, 100);
                SendMessage(GetDlgItem(hwnd, IDC_SCROLLSPEED), UDM_SETPOS32, 0, data->GetInt(TEXT("scrollSpeed"), 0));

                SendMessage(GetDlgItem(hwnd, IDC_BOLD), BM_SETCHECK, data->GetInt(TEXT("bold"), 0) ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hwnd, IDC_ITALIC), BM_SETCHECK, data->GetInt(TEXT("italic"), 0) ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hwnd, IDC_UNDERLINE), BM_SETCHECK, data->GetInt(TEXT("underline"), 0) ? BST_CHECKED : BST_UNCHECKED, 0);

                SendMessage(GetDlgItem(hwnd, IDC_VERTICALSCRIPT), BM_SETCHECK, data->GetInt(TEXT("vertical"), 0) ? BST_CHECKED : BST_UNCHECKED, 0);

                //-----------------------------------------

                SendMessage(GetDlgItem(hwnd, IDC_USETEXTEXTENTS), BM_SETCHECK, data->GetInt(TEXT("useTextExtents"), 0) ? BST_CHECKED : BST_UNCHECKED, 0);
                ConfigureTextProc(hwnd, WM_COMMAND, MAKEWPARAM(IDC_USETEXTEXTENTS, BN_CLICKED), (LPARAM)GetDlgItem(hwnd, IDC_USETEXTEXTENTS));

                SendMessage(GetDlgItem(hwnd, IDC_EXTENTWIDTH),  UDM_SETRANGE32, 32, 2048);
                SendMessage(GetDlgItem(hwnd, IDC_EXTENTHEIGHT), UDM_SETRANGE32, 32, 2048);
                SendMessage(GetDlgItem(hwnd, IDC_EXTENTWIDTH),  UDM_SETPOS32, 0, data->GetInt(TEXT("extentWidth"),  100));
                SendMessage(GetDlgItem(hwnd, IDC_EXTENTHEIGHT), UDM_SETPOS32, 0, data->GetInt(TEXT("extentHeight"), 100));

                SendMessage(GetDlgItem(hwnd, IDC_WRAP), BM_SETCHECK, data->GetInt(TEXT("wrap"), 0) ? BST_CHECKED : BST_UNCHECKED, 0);

                HWND hwndAlign = GetDlgItem(hwnd, IDC_ALIGN);
                SendMessage(hwndAlign, CB_ADDSTRING, 0, (LPARAM)Str("Sources.TextSource.Left"));
                SendMessage(hwndAlign, CB_ADDSTRING, 0, (LPARAM)Str("Sources.TextSource.Center"));
                SendMessage(hwndAlign, CB_ADDSTRING, 0, (LPARAM)Str("Sources.TextSource.Right"));

                int align = data->GetInt(TEXT("align"), 0);
                ClampInt(align, 0, 2);
                SendMessage(hwndAlign, CB_SETCURSEL, align, 0);

                //-----------------------------------------

                BOOL bUseFile = data->GetInt(TEXT("mode"), 0) == 1;
                SendMessage(GetDlgItem(hwnd, IDC_USEFILE), BM_SETCHECK, bUseFile ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hwnd, IDC_USETEXT), BM_SETCHECK, bUseFile ? BST_UNCHECKED : BST_CHECKED, 0);

                SetWindowText(GetDlgItem(hwnd, IDC_TEXT), data->GetString(TEXT("text")));
                SetWindowText(GetDlgItem(hwnd, IDC_FILE), data->GetString(TEXT("file")));

                EnableWindow(GetDlgItem(hwnd, IDC_TEXT), !bUseFile);
                EnableWindow(GetDlgItem(hwnd, IDC_FILE), bUseFile);
                EnableWindow(GetDlgItem(hwnd, IDC_BROWSE), bUseFile);

                return TRUE;
            }

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_FONT:
                    {
                        if(HIWORD(wParam) == CBN_SELCHANGE || HIWORD(wParam) == CBN_EDITCHANGE)
                        {
                            ConfigTextSourceInfo *configInfo = (ConfigTextSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                            if(!configInfo) break;

                            String strFont;
                            if(HIWORD(wParam) == CBN_SELCHANGE)
                                strFont = GetFontFace(configInfo, (HWND)lParam);
                            else
                            {
                                UINT id = FindFontName(configInfo, (HWND)lParam, GetEditText((HWND)lParam));
                                if(id != INVALID)
                                    strFont = configInfo->fontFaces[id];
                            }

                            ImageSource *source = API->GetSceneImageSource(configInfo->lpName);
                            if(source && strFont.IsValid())
                                source->SetString(TEXT("font"), strFont);
                        }
                    }
                    break;

                case IDC_COLOR:
                    {
                        DWORD color = CCGetColor((HWND)lParam);

                        ConfigTextSourceInfo *configInfo = (ConfigTextSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                        if(!configInfo) break;
                        ImageSource *source = API->GetSceneImageSource(configInfo->lpName);
                        if(source)
                            source->SetInt(TEXT("color"), color);
                    }
                    break;

                case IDC_TEXTSIZE_EDIT:
                case IDC_EXTENTWIDTH_EDIT:
                case IDC_EXTENTHEIGHT_EDIT:
                case IDC_TEXTOPACITY_EDIT:
                case IDC_SCROLLSPEED_EDIT:
                    if(HIWORD(wParam) == EN_CHANGE)
                    {
                        int val = (int)SendMessage(GetWindow((HWND)lParam, GW_HWNDNEXT), UDM_GETPOS32, 0, 0);

                        ConfigTextSourceInfo *configInfo = (ConfigTextSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                        if(!configInfo) break;

                        ImageSource *source = API->GetSceneImageSource(configInfo->lpName);
                        if(source)
                        {
                            switch(LOWORD(wParam))
                            {
                                case IDC_TEXTSIZE_EDIT:     source->SetInt(TEXT("fontSize"), val); break;
                                case IDC_EXTENTWIDTH_EDIT:  source->SetInt(TEXT("extentWidth"), val); break;
                                case IDC_EXTENTHEIGHT_EDIT: source->SetInt(TEXT("extentHeight"), val); break;
                                case IDC_TEXTOPACITY_EDIT:  source->SetInt(TEXT("textOpacity"), val); break;
                                case IDC_SCROLLSPEED_EDIT:  source->SetInt(TEXT("scrollSpeed"), val); break;
                            }
                        }
                    }
                    break;

                case IDC_BOLD:
                case IDC_ITALIC:
                case IDC_UNDERLINE:
                case IDC_VERTICALSCRIPT:
                case IDC_WRAP:
                case IDC_USETEXTEXTENTS:
                    if(HIWORD(wParam) == BN_CLICKED)
                    {
                        BOOL bChecked = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;

                        ConfigTextSourceInfo *configInfo = (ConfigTextSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                        if(!configInfo) break;
                        ImageSource *source = API->GetSceneImageSource(configInfo->lpName);
                        if(source)
                        {
                            switch(LOWORD(wParam))
                            {
                                case IDC_BOLD:              source->SetInt(TEXT("bold"), bChecked); break;
                                case IDC_ITALIC:            source->SetInt(TEXT("italic"), bChecked); break;
                                case IDC_UNDERLINE:         source->SetInt(TEXT("underline"), bChecked); break;
                                case IDC_VERTICALSCRIPT:    source->SetInt(TEXT("vertical"), bChecked); break;
                                case IDC_WRAP:              source->SetInt(TEXT("wrap"), bChecked); break;
                                case IDC_USETEXTEXTENTS:    source->SetInt(TEXT("useTextExtents"), bChecked); break;
                            }
                        }

                        if(LOWORD(wParam) == IDC_USETEXTEXTENTS)
                        {
                            EnableWindow(GetDlgItem(hwnd, IDC_EXTENTWIDTH_EDIT), bChecked);
                            EnableWindow(GetDlgItem(hwnd, IDC_EXTENTHEIGHT_EDIT), bChecked);
                            EnableWindow(GetDlgItem(hwnd, IDC_EXTENTWIDTH), bChecked);
                            EnableWindow(GetDlgItem(hwnd, IDC_EXTENTHEIGHT), bChecked);
                            EnableWindow(GetDlgItem(hwnd, IDC_WRAP), bChecked);
                            EnableWindow(GetDlgItem(hwnd, IDC_ALIGN), bChecked);
                        }
                    }
                    break;

                case IDC_ALIGN:
                    if(HIWORD(wParam) == CBN_SELCHANGE)
                    {
                        int align = (int)SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
                        if(align == CB_ERR)
                            break;

                        ConfigTextSourceInfo *configInfo = (ConfigTextSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                        if(!configInfo) break;
                        ImageSource *source = API->GetSceneImageSource(configInfo->lpName);
                        if(source)
                            source->SetInt(TEXT("align"), align);
                    }
                    break;

                case IDC_FILE:
                case IDC_TEXT:
                    if(HIWORD(wParam) == EN_CHANGE)
                    {
                        String strText = GetEditText((HWND)lParam);

                        ConfigTextSourceInfo *configInfo = (ConfigTextSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                        if(!configInfo) break;
                        ImageSource *source = API->GetSceneImageSource(configInfo->lpName);
                        if(source)
                        {
                            switch(LOWORD(wParam))
                            {
                                case IDC_FILE: source->SetString(TEXT("file"), strText); break;
                                case IDC_TEXT: source->SetString(TEXT("text"), strText); break;
                            }
                        }
                    }
                    break;

                case IDC_USEFILE:
                    if(HIWORD(wParam) == BN_CLICKED)
                    {
                        EnableWindow(GetDlgItem(hwnd, IDC_TEXT), FALSE);
                        EnableWindow(GetDlgItem(hwnd, IDC_FILE), TRUE);
                        EnableWindow(GetDlgItem(hwnd, IDC_BROWSE), TRUE);

                        ConfigTextSourceInfo *configInfo = (ConfigTextSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                        ImageSource *source = API->GetSceneImageSource(configInfo->lpName);
                        if(source)
                            source->SetInt(TEXT("mode"), 1);
                    }
                    break;

                case IDC_USETEXT:
                    if(HIWORD(wParam) == BN_CLICKED)
                    {
                        EnableWindow(GetDlgItem(hwnd, IDC_TEXT), TRUE);
                        EnableWindow(GetDlgItem(hwnd, IDC_FILE), FALSE);
                        EnableWindow(GetDlgItem(hwnd, IDC_BROWSE), FALSE);

                        ConfigTextSourceInfo *configInfo = (ConfigTextSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                        if(!configInfo) break;
                        ImageSource *source = API->GetSceneImageSource(configInfo->lpName);
                        if(source)
                            source->SetInt(TEXT("mode"), 0);
                    }
                    break;

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
                        ofn.lpstrFilter = TEXT("Text Files (*.txt)\0*.txt\0");
                        ofn.nFilterIndex = 1;
                        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                        TCHAR curDirectory[MAX_PATH+1];
                        GetCurrentDirectory(MAX_PATH, curDirectory);

                        BOOL bOpenFile = GetOpenFileName(&ofn);
                        SetCurrentDirectory(curDirectory);

                        if(bOpenFile)
                        {
                            SetWindowText(GetDlgItem(hwnd, IDC_FILE), lpFile);

                            ConfigTextSourceInfo *configInfo = (ConfigTextSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                            ImageSource *source = API->GetSceneImageSource(configInfo->lpName);
                            if(source)
                                source->SetString(TEXT("file"), lpFile);
                        }
                    }
                    break;

                case IDOK:
                    {
                        ConfigTextSourceInfo *configInfo = (ConfigTextSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                        if(!configInfo) break;
                        XElement *data = configInfo->data;

                        BOOL bUseTextExtents = SendMessage(GetDlgItem(hwnd, IDC_USETEXTEXTENTS), BM_GETCHECK, 0, 0) == BST_CHECKED;
                        int mode = SendMessage(GetDlgItem(hwnd, IDC_USEFILE), BM_GETCHECK, 0, 0) == BST_CHECKED;

                        String strText = GetEditText(GetDlgItem(hwnd, IDC_TEXT));
                        String strFile = GetEditText(GetDlgItem(hwnd, IDC_FILE));

                        UINT extentWidth  = (UINT)SendMessage(GetDlgItem(hwnd, IDC_EXTENTWIDTH),  UDM_GETPOS32, 0, 0);
                        UINT extentHeight = (UINT)SendMessage(GetDlgItem(hwnd, IDC_EXTENTHEIGHT), UDM_GETPOS32, 0, 0);

                        String strFont = GetFontFace(configInfo, GetDlgItem(hwnd, IDC_FONT));
                        UINT fontSize = (UINT)SendMessage(GetDlgItem(hwnd, IDC_TEXTSIZE), UDM_GETPOS32, 0, 0);

                        BOOL bBold = SendMessage(GetDlgItem(hwnd, IDC_BOLD), BM_GETCHECK, 0, 0) == BST_CHECKED;
                        BOOL bItalic = SendMessage(GetDlgItem(hwnd, IDC_ITALIC), BM_GETCHECK, 0, 0) == BST_CHECKED;
                        BOOL bVertical = SendMessage(GetDlgItem(hwnd, IDC_VERTICALSCRIPT), BM_GETCHECK, 0, 0) == BST_CHECKED;

                        String strFontDisplayName = GetEditText(GetDlgItem(hwnd, IDC_FONT));
                        if(strFont.IsEmpty())
                        {
                            UINT id = FindFontName(configInfo, GetDlgItem(hwnd, IDC_FONT), strFontDisplayName);
                            if(id != INVALID)
                                strFont = configInfo->fontFaces[id];
                        }

                        if(strFont.IsEmpty())
                        {
                            String strError = Str("Sources.TextSource.FontNotFound");
                            strError.FindReplace(TEXT("$1"), strFontDisplayName);
                            MessageBox(hwnd, strError, NULL, 0);
                            break;
                        }

                        if(bUseTextExtents)
                        {
                            configInfo->cx = float(extentWidth);
                            configInfo->cy = float(extentHeight);
                        }
                        else
                        {
                            String strOutputText;
                            if(mode == 0)
                                strOutputText = strText;
                            else if(mode == 1)
                            {
                                XFile textFile;
                                if(strFile.IsEmpty() || !textFile.Open(strFile, XFILE_READ, XFILE_OPENEXISTING))
                                {
                                    String strError = Str("Sources.TextSource.FileNotFound");
                                    strError.FindReplace(TEXT("$1"), strFile);

                                    MessageBox(hwnd, strError, NULL, 0);
                                    break;
                                }

                                textFile.ReadFileToString(strOutputText);
                            }

                            LOGFONT lf;
                            zero(&lf, sizeof(lf));
                            lf.lfHeight = fontSize;
                            lf.lfWeight = bBold ? FW_BOLD : FW_DONTCARE;
                            lf.lfItalic = bItalic;
                            lf.lfQuality = ANTIALIASED_QUALITY;
                            if(strFont.IsValid())
                                scpy_n(lf.lfFaceName, strFont, 31);
                            else
                                scpy_n(lf.lfFaceName, TEXT("Arial"), 31);

                            HDC hDC = CreateCompatibleDC(NULL);

                            Gdiplus::Font font(hDC, &lf);
                            
                            if(!font.IsAvailable())
                            {
                                String strError = Str("Sources.TextSource.FontNotFound");
                                strError.FindReplace(TEXT("$1"), strFontDisplayName);
                                MessageBox(hwnd, strError, NULL, 0);
                                DeleteDC(hDC);
                                break;
                            }

                            {
                                Gdiplus::Graphics graphics(hDC);
                                Gdiplus::StringFormat format;

                                if(bVertical)
                                    format.SetFormatFlags(Gdiplus::StringFormatFlagsDirectionVertical|Gdiplus::StringFormatFlagsDirectionRightToLeft);

                                Gdiplus::RectF rcf;
                                graphics.MeasureString(strOutputText, -1, &font, Gdiplus::PointF(0.0f, 0.0f), &format, &rcf);

                                configInfo->cx = MAX(rcf.Width,  32.0f);
                                configInfo->cy = MAX(rcf.Height, 32.0f);
                            }

                            DeleteDC(hDC);
                        }

                        data->SetFloat(TEXT("baseSizeCX"), configInfo->cx);
                        data->SetFloat(TEXT("baseSizeCY"), configInfo->cy);

                        data->SetString(TEXT("font"), strFont);
                        data->SetInt(TEXT("color"), CCGetColor(GetDlgItem(hwnd, IDC_COLOR)));
                        data->SetInt(TEXT("fontSize"), fontSize);
                        data->SetInt(TEXT("textOpacity"), (UINT)SendMessage(GetDlgItem(hwnd, IDC_TEXTOPACITY), UDM_GETPOS32, 0, 0));
                        data->SetInt(TEXT("scrollSpeed"), (int)SendMessage(GetDlgItem(hwnd, IDC_SCROLLSPEED), UDM_GETPOS32, 0, 0));
                        data->SetInt(TEXT("bold"), bBold);
                        data->SetInt(TEXT("italic"), bItalic);
                        data->SetInt(TEXT("vertical"), bVertical);
                        data->SetInt(TEXT("wrap"), SendMessage(GetDlgItem(hwnd, IDC_WRAP), BM_GETCHECK, 0, 0) == BST_CHECKED);
                        data->SetInt(TEXT("underline"), SendMessage(GetDlgItem(hwnd, IDC_UNDERLINE), BM_GETCHECK, 0, 0) == BST_CHECKED);
                        data->SetInt(TEXT("useTextExtents"), bUseTextExtents);
                        data->SetInt(TEXT("extentWidth"), extentWidth);
                        data->SetInt(TEXT("extentHeight"), extentHeight);
                        data->SetInt(TEXT("align"), (int)SendMessage(GetDlgItem(hwnd, IDC_ALIGN), CB_GETCURSEL, 0, 0));
                        data->SetString(TEXT("file"), strFile);
                        data->SetString(TEXT("text"), strText);
                        data->SetInt(TEXT("mode"), mode);
                    }

                case IDCANCEL:
                    if(LOWORD(wParam) == IDCANCEL)
                        DoCancelStuff(hwnd);

                    EndDialog(hwnd, LOWORD(wParam));
            }
            break;

        case WM_CLOSE:
            DoCancelStuff(hwnd);
            EndDialog(hwnd, IDCANCEL);
    }
    return 0;
}

bool STDCALL ConfigureTextSource(XElement *element, bool bCreating)
{
    if(!element)
    {
        AppWarning(TEXT("ConfigureTextSource: NULL element"));
        return false;
    }

    XElement *data = element->GetElement(TEXT("data"));
    if(!data)
        data = element->CreateElement(TEXT("data"));

    ConfigTextSourceInfo configInfo;
    configInfo.lpName = element->GetName();
    configInfo.data = data;

    if(DialogBoxParam(hinstMain, MAKEINTRESOURCE(IDD_CONFIGURETEXTSOURCE), hwndMain, ConfigureTextProc, (LPARAM)&configInfo) == IDOK)
    {
        element->SetFloat(TEXT("cx"), configInfo.cx);
        element->SetFloat(TEXT("cy"), configInfo.cy);

        return true;
    }

    return false;
}
