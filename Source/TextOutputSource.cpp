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

#include <memory>

#include <gdiplus.h>


#define ClampVal(val, minVal, maxVal) \
    if(val < minVal) val = minVal; \
    else if(val > maxVal) val = maxVal;

#define MAX_TEX_SIZE_W 8192
#define MAX_TEX_SIZE_H 8192
#define MIN_TEX_SIZE_W 32
#define MIN_TEX_SIZE_H 32

inline DWORD GetAlphaVal(UINT opacityLevel)
{
    return ((opacityLevel*255/100)&0xFF) << 24;
}


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
    UINT        globalOpacity;
    int         scrollSpeed;
    bool        bBold, bItalic, bUnderline, bVertical;

    UINT        backgroundOpacity;
    DWORD       backgroundColor;

    bool        bUseOutline;
    float       outlineSize;
    DWORD       outlineColor;
    UINT        outlineOpacity;

    bool        bUseExtents;
    UINT        extentWidth, extentHeight;

    bool        bWrap;
    bool        bScrollMode;
    int         align;

    Vect2       baseSize;
    SIZE        textureSize;
    bool        bUsePointFiltering;

    bool        bMonitoringFileChanges;
    OSFileChangeData *fileChangeMonitor;

    std::unique_ptr<SamplerState> sampler;

    bool        bDoUpdate;

    SamplerState *ss;

    XElement    *data;

    void DrawOutlineText(Gdiplus::Graphics *graphics,
                         Gdiplus::Font &font,
                         const Gdiplus::GraphicsPath &path,
                         const Gdiplus::StringFormat &format,
                         const Gdiplus::Brush *brush)
    {
                
        Gdiplus::GraphicsPath *outlinePath;

        outlinePath = path.Clone();

        // Outline color and size
        UINT tmpOpacity = (UINT)((((float)opacity * 0.01f) * ((float)outlineOpacity * 0.01f)) * 100.0f);
        Gdiplus::Pen pen(Gdiplus::Color(GetAlphaVal(tmpOpacity) | (outlineColor&0xFFFFFF)), outlineSize);
        pen.SetLineJoin(Gdiplus::LineJoinRound);

        // Widen the outline
        // It seems that Widen has a huge performance impact on DrawPath call, screw it! We're talking about freaking seconds in some extreme cases...
        //outlinePath->Widen(&pen);

        // Draw the outline
        graphics->DrawPath(&pen, outlinePath);

        // Draw the text        
        graphics->FillPath(brush, &path);

        delete outlinePath;
    }

    HFONT GetFont()
    {
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

        return hFont;
    }

    void UpdateCurrentText()
    {
        if(bMonitoringFileChanges)
        {
            OSMonitorFileDestroy(fileChangeMonitor);
            fileChangeMonitor = NULL;

            bMonitoringFileChanges = false;
        }

        if(mode == 0)
            strCurrentText = strText;

        else if(mode == 1 && strFile.IsValid())
        {
            XFile textFile;
            if(textFile.Open(strFile, XFILE_READ | XFILE_SHARED, XFILE_OPENEXISTING))
            {
                textFile.ReadFileToString(strCurrentText);
            }
            else
            {
                strCurrentText = TEXT("");
                AppWarning(TEXT("TextSource::UpdateTexture: could not open specified file (invalid file name or access violation)"));
            }

            if (fileChangeMonitor = OSMonitorFileStart (strFile))
                bMonitoringFileChanges = true;
        }
        else
            strCurrentText = TEXT("");
    }

    void SetStringFormat(Gdiplus::StringFormat &format)
    {
        UINT formatFlags;

        formatFlags = Gdiplus::StringFormatFlagsNoFitBlackBox
                    | Gdiplus::StringFormatFlagsMeasureTrailingSpaces;


        if(bVertical)
            formatFlags |= Gdiplus::StringFormatFlagsDirectionVertical
                         | Gdiplus::StringFormatFlagsDirectionRightToLeft;

        format.SetFormatFlags(formatFlags);
        format.SetTrimming(Gdiplus::StringTrimmingWord);

        if(bUseExtents && bWrap)
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
                    break;
                case 2:
                    if(bVertical)
                        format.SetLineAlignment(Gdiplus::StringAlignmentNear);
                    else
                        format.SetAlignment(Gdiplus::StringAlignmentFar);
                    break;
            }
        else if(bUseExtents && bVertical && !bWrap)
                format.SetLineAlignment(Gdiplus::StringAlignmentFar);
        else if(bVertical)
                format.SetLineAlignment(Gdiplus::StringAlignmentFar);

    }

    float ProcessScrollMode(Gdiplus::Graphics *graphics, Gdiplus::Font *font, Gdiplus::RectF &layoutBox, Gdiplus::StringFormat *format)
    {
        StringList strList;
        Gdiplus::RectF boundingBox;
        
        float offset = layoutBox.Height;

        Gdiplus::RectF l2(0.0f ,0.0f , layoutBox.Width, 32000.0f); // Really, it needs to be OVER9000

        strCurrentText.FindReplace(L"\n\r", L"\n");
        strCurrentText.GetTokenList(strList,'\n');

        if(strList.Num() != 0)
            strCurrentText.Clear();
        else 
            return 0.0f;

        for(int i = strList.Num() - 1; i >= 0; i--)
        {
            strCurrentText.InsertString(0, TEXT("\n"));
            strCurrentText.InsertString(0, strList.GetElement((unsigned int)i).Array());

            if(strCurrentText.IsValid())
            {
                graphics->MeasureString(strCurrentText, -1, font, l2, &boundingBox);
                offset = layoutBox.Height - boundingBox.Height;
            }
            
            if(offset < 0)
                break;
        }

        return offset;
    }

    void UpdateTexture()
    {
        HFONT hFont;
        Gdiplus::Status stat;
        Gdiplus::RectF layoutBox;
        SIZE textSize;
        float offset;

        Gdiplus::RectF boundingBox(0.0f, 0.0f, 32.0f, 32.0f);

        UpdateCurrentText();

        hFont = GetFont();
        if(!hFont)
            return;

        Gdiplus::StringFormat format(Gdiplus::StringFormat::GenericTypographic());

        SetStringFormat(format);

        HDC hdc = CreateCompatibleDC(NULL);

        Gdiplus::Font font(hdc, hFont);
        Gdiplus::Graphics *graphics = new Gdiplus::Graphics(hdc);

        graphics->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

        if(strCurrentText.IsValid())
        {
            if(bUseExtents && bWrap)
            {
                layoutBox.X = layoutBox.Y = 0;
                layoutBox.Width  = float(extentWidth);
                layoutBox.Height = float(extentHeight);

                if(bUseOutline)
                {
                    //Note: since there's no path widening in DrawOutlineText the padding is half than what it was supposed to be.
                    layoutBox.Width  -= outlineSize;
                    layoutBox.Height -= outlineSize;
                }

                if(!bVertical && bScrollMode)
                {
                    offset = ProcessScrollMode(graphics, &font, layoutBox, &format);

                    boundingBox = layoutBox;
                    boundingBox.Y = offset;
                    if(offset < 0)
                        boundingBox.Height -= offset;
                }
                else
                {
                    stat = graphics->MeasureString(strCurrentText, -1, &font, layoutBox, &format, &boundingBox);
                    if(stat != Gdiplus::Ok)
                        AppWarning(TEXT("TextSource::UpdateTexture: Gdiplus::Graphics::MeasureString failed: %u"), (int)stat);
                }
            }
            else
            {
                stat = graphics->MeasureString(strCurrentText, -1, &font, Gdiplus::PointF(0.0f, 0.0f), &format, &boundingBox);
                if(stat != Gdiplus::Ok)
                    AppWarning(TEXT("TextSource::UpdateTexture: Gdiplus::Graphics::MeasureString failed: %u"), (int)stat);
                if(bUseOutline)
                {
                    //Note: since there's no path widening in DrawOutlineText the padding is half than what it was supposed to be.
                    boundingBox.Width  += outlineSize;
                    boundingBox.Height += outlineSize;
				}
			}
        }

        delete graphics;

        DeleteDC(hdc);
        hdc = NULL;
        DeleteObject(hFont);

        if(bVertical)
        {
            if(boundingBox.Width<size)
            {
                textSize.cx = size;
                boundingBox.Width = float(size);
            }
            else
                textSize.cx = LONG(boundingBox.Width + EPSILON);

            textSize.cy = LONG(boundingBox.Height + EPSILON);
        }
        else
        {
            if(boundingBox.Height<size)
            {
                textSize.cy = size;
                boundingBox.Height = float(size);
            }
            else
                textSize.cy = LONG(boundingBox.Height + EPSILON);

            textSize.cx = LONG(boundingBox.Width + EPSILON);
        }

        if(bUseExtents)
        {
            if(bWrap)
            {
                textSize.cx = extentWidth;
                textSize.cy = extentHeight;
            }
            else
            {
                if(LONG(extentWidth) > textSize.cx)
                    textSize.cx = extentWidth;
                if(LONG(extentHeight) > textSize.cy)
                    textSize.cy = extentHeight;
            }
        }

        //textSize.cx &= 0xFFFFFFFE;
        //textSize.cy &= 0xFFFFFFFE;

        textSize.cx += textSize.cx%2;
        textSize.cy += textSize.cy%2;

        ClampVal(textSize.cx, MIN_TEX_SIZE_W, MAX_TEX_SIZE_W);
        ClampVal(textSize.cy, MIN_TEX_SIZE_H, MAX_TEX_SIZE_H);

        //----------------------------------------------------------------------
        // write image

        {
            HDC hTempDC = CreateCompatibleDC(NULL);

            BITMAPINFO bi;
            zero(&bi, sizeof(bi));

            void* lpBits;

            BITMAPINFOHEADER &bih = bi.bmiHeader;
            bih.biSize = sizeof(bih);
            bih.biBitCount = 32;
            bih.biWidth  = textSize.cx;
            bih.biHeight = textSize.cy;
            bih.biPlanes = 1;

            HBITMAP hBitmap = CreateDIBSection(hTempDC, &bi, DIB_RGB_COLORS, &lpBits, NULL, 0);

            Gdiplus::Bitmap      bmp(textSize.cx, textSize.cy, 4*textSize.cx, PixelFormat32bppARGB, (BYTE*)lpBits);

            graphics = new Gdiplus::Graphics(&bmp); 

            Gdiplus::SolidBrush  *brush = new Gdiplus::SolidBrush(Gdiplus::Color(GetAlphaVal(opacity)|(color&0x00FFFFFF)));

            DWORD bkColor;

		    if(backgroundOpacity == 0 && scrollSpeed !=0)
                bkColor = 1<<24 | (color&0x00FFFFFF);
            else
                bkColor = ((strCurrentText.IsValid() || bUseExtents) ? GetAlphaVal(backgroundOpacity) : GetAlphaVal(0)) | (backgroundColor&0x00FFFFFF);

            if((textSize.cx > boundingBox.Width  || textSize.cy > boundingBox.Height) && !bUseExtents)
            {
                stat = graphics->Clear(Gdiplus::Color( 0x00000000));
                if(stat != Gdiplus::Ok)
                    AppWarning(TEXT("TextSource::UpdateTexture: Graphics::Clear failed: %u"), (int)stat);

                Gdiplus::SolidBrush *bkBrush = new Gdiplus::SolidBrush(Gdiplus::Color( bkColor ));

                graphics->FillRectangle(bkBrush, boundingBox);

                delete bkBrush;
            }
            else
            {
                stat = graphics->Clear(Gdiplus::Color( bkColor ));
                if(stat != Gdiplus::Ok)
                    AppWarning(TEXT("TextSource::UpdateTexture: Graphics::Clear failed: %u"), (int)stat);
            }

            graphics->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
            graphics->SetCompositingMode(Gdiplus::CompositingModeSourceOver);
            graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            
            if(strCurrentText.IsValid())
            {
                if(bUseOutline)
                {
                    boundingBox.Offset(outlineSize/2, outlineSize/2);

                    Gdiplus::FontFamily fontFamily;
                    Gdiplus::GraphicsPath path;

                    font.GetFamily(&fontFamily);

                    path.AddString(strCurrentText, -1, &fontFamily, font.GetStyle(), font.GetSize(), boundingBox, &format);

                    DrawOutlineText(graphics, font, path, format, brush);
                }
                else
                {
                    stat = graphics->DrawString(strCurrentText, -1, &font, boundingBox, &format, brush);
                    if(stat != Gdiplus::Ok)
                        AppWarning(TEXT("TextSource::UpdateTexture: Graphics::DrawString failed: %u"), (int)stat);
                }
            }

            delete brush;
            delete graphics;

            //----------------------------------------------------------------------
            // upload texture

            if(textureSize.cx != textSize.cx || textureSize.cy != textSize.cy)
            {
                if(texture)
                {
                    delete texture;
                    texture = NULL;
                }

                mcpy(&textureSize, &textSize, sizeof(textureSize));
                texture = CreateTexture(textSize.cx, textSize.cy, GS_BGRA, lpBits, FALSE, FALSE);
            }
            else if(texture)
                texture->SetImage(lpBits, GS_IMAGEFORMAT_BGRA, 4*textSize.cx);

            if(!texture)
            {
                AppWarning(TEXT("TextSource::UpdateTexture: could not create texture"));
                DeleteObject(hFont);
            }

            DeleteDC(hTempDC);
            DeleteObject(hBitmap);
        }
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
        globalOpacity = 100;

        Log(TEXT("Using text output"));
    }

    ~TextOutputSource()
    {
        if(texture)
        {
            delete texture;
            texture = NULL;
        }

        delete ss;

        if(bMonitoringFileChanges)
        {
            OSMonitorFileDestroy(fileChangeMonitor);
        }
    }

    void Preprocess()
    {
        if(bMonitoringFileChanges)
        {
            if (OSFileHasChanged(fileChangeMonitor))
                bUpdateTexture = true;
        }

        if(bUpdateTexture)
        {
            bUpdateTexture = false;
            UpdateTexture();
        }
    }

    void Tick(float fSeconds)
    {
        if(scrollSpeed != 0 && texture)
        {
            scrollValue += fSeconds*float(scrollSpeed)/(bVertical?(-1.0f)*float(textureSize.cy):float(textureSize.cx));
            while(scrollValue > 1.0f)
                scrollValue -= 1.0f;
            while(scrollValue < -1.0f)
                scrollValue += 1.0f;
        }

        if(showExtentTime > 0.0f)
            showExtentTime -= fSeconds;

        if(bDoUpdate)
        {
            bDoUpdate = false;
            bUpdateTexture = true;
        }
    }

    void Render(const Vect2 &pos, const Vect2 &size)
    {
        if(texture)
        {
            //EnableBlending(FALSE);

            Vect2 sizeMultiplier = size/baseSize;
            Vect2 newSize = Vect2(float(textureSize.cx), float(textureSize.cy))*sizeMultiplier;

            if(bUseExtents)
            {
                Vect2 extentVal = Vect2(float(extentWidth), float(extentHeight))*sizeMultiplier;
                if(showExtentTime > 0.0f)
                {
                    Shader *pShader = GS->GetCurrentPixelShader();
                    Shader *vShader = GS->GetCurrentVertexShader();

                    Color4 rectangleColor = Color4(0.0f, 1.0f, 0.0f, 1.0f);
                    if(showExtentTime < 1.0f)
                        rectangleColor.w = showExtentTime;

                    App->solidPixelShader->SetColor(App->solidPixelShader->GetParameter(0), rectangleColor);

                    LoadVertexShader(App->solidVertexShader);
                    LoadPixelShader(App->solidPixelShader);
                    DrawBox(pos, extentVal);

                    LoadVertexShader(vShader);
                    LoadPixelShader(pShader);
                }

                if(!bWrap)
                {
                    XRect rect = {int(pos.x), int(pos.y), int(extentVal.x), int(extentVal.y)};
                    SetScissorRect(&rect);
                }
            }

            if(bUsePointFiltering) {
                if (!sampler) {
                    SamplerInfo samplerinfo;
                    samplerinfo.filter = GS_FILTER_POINT;
                    std::unique_ptr<SamplerState> new_sampler(CreateSamplerState(samplerinfo));
                    sampler = std::move(new_sampler);
                }

                LoadSamplerState(sampler.get(), 0);
            }

            DWORD alpha = DWORD(double(globalOpacity)*2.55);
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

            if (bUsePointFiltering)
                LoadSamplerState(NULL, 0);

            if(bUseExtents && !bWrap)
                SetScissorRect(NULL);
            //EnableBlending(TRUE);
        }
    }

    Vect2 GetSize() const
    {
        return baseSize;
    }

    void UpdateSettings()
    {
        strFont     = data->GetString(TEXT("font"), TEXT("Arial"));
        color       = data->GetInt(TEXT("color"), 0xFFFFFFFF);
        size        = data->GetInt(TEXT("fontSize"), 48);
        opacity     = data->GetInt(TEXT("textOpacity"), 100);
        scrollSpeed = data->GetInt(TEXT("scrollSpeed"), 0);
        bBold       = data->GetInt(TEXT("bold"), 0) != 0;
        bItalic     = data->GetInt(TEXT("italic"), 0) != 0;
        bWrap       = data->GetInt(TEXT("wrap"), 0) != 0;
        bScrollMode = data->GetInt(TEXT("scrollMode"), 0) != 0;
        bUnderline  = data->GetInt(TEXT("underline"), 0) != 0;
        bVertical   = data->GetInt(TEXT("vertical"), 0) != 0;
        bUseExtents = data->GetInt(TEXT("useTextExtents"), 0) != 0;
        extentWidth = data->GetInt(TEXT("extentWidth"), 0);
        extentHeight= data->GetInt(TEXT("extentHeight"), 0);
        align       = data->GetInt(TEXT("align"), 0);
        strFile     = data->GetString(TEXT("file"));
        strText     = data->GetString(TEXT("text"));
        mode        = data->GetInt(TEXT("mode"), 0);
        bUsePointFiltering = data->GetInt(TEXT("pointFiltering"), 0) != 0;

        baseSize.x  = data->GetFloat(TEXT("baseSizeCX"), MIN_TEX_SIZE_W);
        baseSize.y  = data->GetFloat(TEXT("baseSizeCY"), MIN_TEX_SIZE_H);

        bUseOutline    = data->GetInt(TEXT("useOutline")) != 0;
        outlineColor   = data->GetInt(TEXT("outlineColor"), 0xFF000000);
        outlineSize    = data->GetFloat(TEXT("outlineSize"), 2);
        outlineOpacity = data->GetInt(TEXT("outlineOpacity"), 100);

        backgroundColor   = data->GetInt(TEXT("backgroundColor"), 0xFF000000);
        backgroundOpacity = data->GetInt(TEXT("backgroundOpacity"), 0);

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
        else if(scmpi(lpName, TEXT("scrollMode")) == 0)
            bScrollMode = iValue != 0;
        else if(scmpi(lpName, TEXT("underline")) == 0)
            bUnderline = iValue != 0;
        else if(scmpi(lpName, TEXT("vertical")) == 0)
            bVertical = iValue != 0;
        else if(scmpi(lpName, TEXT("useTextExtents")) == 0)
            bUseExtents = iValue != 0;
        else if(scmpi(lpName, TEXT("extentWidth")) == 0)
        {
            showExtentTime = 2.0f;
            extentWidth = iValue;
        }
        else if(scmpi(lpName, TEXT("extentHeight")) == 0)
        {
            showExtentTime = 2.0f;
            extentHeight = iValue;
        }
        else if(scmpi(lpName, TEXT("align")) == 0)
            align = iValue;
        else if(scmpi(lpName, TEXT("mode")) == 0)
            mode = iValue;
        else if(scmpi(lpName, TEXT("useOutline")) == 0)
            bUseOutline = iValue != 0;
        else if(scmpi(lpName, TEXT("outlineColor")) == 0)
            outlineColor = iValue;
        else if(scmpi(lpName, TEXT("outlineOpacity")) == 0)
            outlineOpacity = iValue;
        else if(scmpi(lpName, TEXT("backgroundColor")) == 0)
            backgroundColor = iValue;
        else if(scmpi(lpName, TEXT("backgroundOpacity")) == 0)
            backgroundOpacity = iValue;

        bUpdateTexture = true;
    }

    void SetFloat(CTSTR lpName, float fValue)
    {
        if(scmpi(lpName, TEXT("outlineSize")) == 0)
            outlineSize = fValue;

        bUpdateTexture = true;
    }

    inline void ResetExtentRect() {showExtentTime = 0.0f;}
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
    if(fontType == TRUETYPE_FONTTYPE) //HomeWorld - GDI+ doesn't like anything other than truetype
    {
        configInfo->fontNames << logicalData->elfFullName;
        configInfo->fontFaces << logicalData->elfLogFont.lfFaceName;
    }

    return 1;
}

void DoCancelStuff(HWND hwnd)
{
    ConfigTextSourceInfo *configInfo = (ConfigTextSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
    ImageSource *source = API->GetSceneImageSource(configInfo->lpName);
    //XElement *data = configInfo->data;

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
    static bool bInitializedDialog = false;

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

                SendMessage(GetDlgItem(hwnd, IDC_TEXTSIZE), UDM_SETRANGE32, 5, MAX_TEX_SIZE_H);
                SendMessage(GetDlgItem(hwnd, IDC_TEXTSIZE), UDM_SETPOS32, 0, data->GetInt(TEXT("fontSize"), 48));

                //-----------------------------------------

                CCSetColor(GetDlgItem(hwnd, IDC_COLOR), data->GetInt(TEXT("color"), 0xFFFFFFFF));

                SendMessage(GetDlgItem(hwnd, IDC_TEXTOPACITY), UDM_SETRANGE32, 0, 100);
                SendMessage(GetDlgItem(hwnd, IDC_TEXTOPACITY), UDM_SETPOS32, 0, data->GetInt(TEXT("textOpacity"), 100));

                SendMessage(GetDlgItem(hwnd, IDC_SCROLLSPEED), UDM_SETRANGE32, -4095, 4095);
                SendMessage(GetDlgItem(hwnd, IDC_SCROLLSPEED), UDM_SETPOS32, 0, data->GetInt(TEXT("scrollSpeed"), 0));

                SendMessage(GetDlgItem(hwnd, IDC_BOLD), BM_SETCHECK, data->GetInt(TEXT("bold"), 0) ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hwnd, IDC_ITALIC), BM_SETCHECK, data->GetInt(TEXT("italic"), 0) ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hwnd, IDC_UNDERLINE), BM_SETCHECK, data->GetInt(TEXT("underline"), 0) ? BST_CHECKED : BST_UNCHECKED, 0);

                SendMessage(GetDlgItem(hwnd, IDC_VERTICALSCRIPT), BM_SETCHECK, data->GetInt(TEXT("vertical"), 0) ? BST_CHECKED : BST_UNCHECKED, 0);

                BOOL bUsePointFilter = data->GetInt(TEXT("pointFiltering"), 0) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_POINTFILTERING), BM_SETCHECK, bUsePointFilter ? BST_CHECKED : BST_UNCHECKED, 0);

                //-----------------------------------------

                CCSetColor(GetDlgItem(hwnd, IDC_BACKGROUNDCOLOR), data->GetInt(TEXT("backgroundColor"), 0xFF000000));

                SendMessage(GetDlgItem(hwnd, IDC_BACKGROUNDOPACITY), UDM_SETRANGE32, 0, 100);
                SendMessage(GetDlgItem(hwnd, IDC_BACKGROUNDOPACITY), UDM_SETPOS32, 0, data->GetInt(TEXT("backgroundOpacity"), 0));

                //-----------------------------------------

                bool bChecked = data->GetInt(TEXT("useOutline"), 0) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_USEOUTLINE), BM_SETCHECK, bChecked ? BST_CHECKED : BST_UNCHECKED, 0);

                EnableWindow(GetDlgItem(hwnd, IDC_OUTLINETHICKNESS_EDIT), bChecked);
                EnableWindow(GetDlgItem(hwnd, IDC_OUTLINETHICKNESS), bChecked);
                EnableWindow(GetDlgItem(hwnd, IDC_OUTLINECOLOR), bChecked);
                EnableWindow(GetDlgItem(hwnd, IDC_OUTLINEOPACITY_EDIT), bChecked);
                EnableWindow(GetDlgItem(hwnd, IDC_OUTLINEOPACITY), bChecked);

                SendMessage(GetDlgItem(hwnd, IDC_OUTLINETHICKNESS), UDM_SETRANGE32, 1, 20);
                SendMessage(GetDlgItem(hwnd, IDC_OUTLINETHICKNESS), UDM_SETPOS32, 0, data->GetInt(TEXT("outlineSize"), 2));

                CCSetColor(GetDlgItem(hwnd, IDC_OUTLINECOLOR), data->GetInt(TEXT("outlineColor"), 0xFF000000));

                SendMessage(GetDlgItem(hwnd, IDC_OUTLINEOPACITY), UDM_SETRANGE32, 0, 100);
                SendMessage(GetDlgItem(hwnd, IDC_OUTLINEOPACITY), UDM_SETPOS32, 0, data->GetInt(TEXT("outlineOpacity"), 100));

                //-----------------------------------------

                bChecked = data->GetInt(TEXT("useTextExtents"), 0) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_USETEXTEXTENTS), BM_SETCHECK, bChecked ? BST_CHECKED : BST_UNCHECKED, 0);
                ConfigureTextProc(hwnd, WM_COMMAND, MAKEWPARAM(IDC_USETEXTEXTENTS, BN_CLICKED), (LPARAM)GetDlgItem(hwnd, IDC_USETEXTEXTENTS));

                EnableWindow(GetDlgItem(hwnd, IDC_EXTENTWIDTH_EDIT), bChecked);
                EnableWindow(GetDlgItem(hwnd, IDC_EXTENTHEIGHT_EDIT), bChecked);
                EnableWindow(GetDlgItem(hwnd, IDC_EXTENTWIDTH), bChecked);
                EnableWindow(GetDlgItem(hwnd, IDC_EXTENTHEIGHT), bChecked);
                EnableWindow(GetDlgItem(hwnd, IDC_WRAP), bChecked);

                bool bVertical = data->GetInt(TEXT("vertical"), 0) != 0;

                SendMessage(GetDlgItem(hwnd, IDC_EXTENTWIDTH),  UDM_SETRANGE32, MIN_TEX_SIZE_W, MAX_TEX_SIZE_W);
                SendMessage(GetDlgItem(hwnd, IDC_EXTENTHEIGHT), UDM_SETRANGE32, MIN_TEX_SIZE_H, MAX_TEX_SIZE_H);
                SendMessage(GetDlgItem(hwnd, IDC_EXTENTWIDTH),  UDM_SETPOS32, 0, data->GetInt(TEXT("extentWidth"),  MIN_TEX_SIZE_W));
                SendMessage(GetDlgItem(hwnd, IDC_EXTENTHEIGHT), UDM_SETPOS32, 0, data->GetInt(TEXT("extentHeight"), MIN_TEX_SIZE_H));

                bool bWrap = data->GetInt(TEXT("wrap"), 0) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_WRAP), BM_SETCHECK, bWrap ? BST_CHECKED : BST_UNCHECKED, 0);

                bool bScrollMode = data->GetInt(TEXT("scrollMode"), 0) != 0;
                SendMessage(GetDlgItem(hwnd, IDC_SCROLLMODE), BM_SETCHECK, bScrollMode ? BST_CHECKED : BST_UNCHECKED, 0);

                EnableWindow(GetDlgItem(hwnd, IDC_ALIGN), bChecked && bWrap);
                EnableWindow(GetDlgItem(hwnd, IDC_SCROLLMODE), bChecked && bWrap && !bVertical);
                
                HWND hwndAlign = GetDlgItem(hwnd, IDC_ALIGN);
                SendMessage(hwndAlign, CB_ADDSTRING, 0, (LPARAM)Str("Sources.TextSource.Left"));
                SendMessage(hwndAlign, CB_ADDSTRING, 0, (LPARAM)Str("Sources.TextSource.Center"));
                SendMessage(hwndAlign, CB_ADDSTRING, 0, (LPARAM)Str("Sources.TextSource.Right"));

                int align = data->GetInt(TEXT("align"), 0);
                ClampVal(align, 0, 2);
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

                bInitializedDialog = true;

                return TRUE;
            }

        case WM_DESTROY:
            bInitializedDialog = false;
            break;

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_FONT:
                    if(bInitializedDialog)
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

                case IDC_OUTLINECOLOR:
                case IDC_BACKGROUNDCOLOR:
                case IDC_COLOR:
                    if(bInitializedDialog)
                    {
                        DWORD color = CCGetColor((HWND)lParam);

                        ConfigTextSourceInfo *configInfo = (ConfigTextSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                        if(!configInfo) break;
                        ImageSource *source = API->GetSceneImageSource(configInfo->lpName);
                        if(source)
                        {
                            switch(LOWORD(wParam))
                            {
                                case IDC_OUTLINECOLOR:      source->SetInt(TEXT("outlineColor"), color); break;
                                case IDC_BACKGROUNDCOLOR:   source->SetInt(TEXT("backgroundColor"), color); break;
                                case IDC_COLOR:             source->SetInt(TEXT("color"), color); break;
                            }
                        }
                    }
                    break;

                case IDC_TEXTSIZE_EDIT:
                case IDC_EXTENTWIDTH_EDIT:
                case IDC_EXTENTHEIGHT_EDIT:
                case IDC_BACKGROUNDOPACITY_EDIT:
                case IDC_TEXTOPACITY_EDIT:
                case IDC_OUTLINEOPACITY_EDIT:
                case IDC_OUTLINETHICKNESS_EDIT:
                case IDC_SCROLLSPEED_EDIT:
                    if(HIWORD(wParam) == EN_CHANGE && bInitializedDialog)
                    {
                        int val = (int)SendMessage(GetWindow((HWND)lParam, GW_HWNDNEXT), UDM_GETPOS32, 0, 0);

                        ConfigTextSourceInfo *configInfo = (ConfigTextSourceInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                        if(!configInfo) break;

                        ImageSource *source = API->GetSceneImageSource(configInfo->lpName);
                        if(source)
                        {
                            switch(LOWORD(wParam))
                            {
                                case IDC_TEXTSIZE_EDIT:             source->SetInt(TEXT("fontSize"), val); break;
                                case IDC_EXTENTWIDTH_EDIT:          source->SetInt(TEXT("extentWidth"), val); break;
                                case IDC_EXTENTHEIGHT_EDIT:         source->SetInt(TEXT("extentHeight"), val); break;
                                case IDC_TEXTOPACITY_EDIT:          source->SetInt(TEXT("textOpacity"), val); break;
                                case IDC_OUTLINEOPACITY_EDIT:       source->SetInt(TEXT("outlineOpacity"), val); break;
                                case IDC_BACKGROUNDOPACITY_EDIT:    source->SetInt(TEXT("backgroundOpacity"), val); break;
                                case IDC_OUTLINETHICKNESS_EDIT:     source->SetFloat(TEXT("outlineSize"), (float)val); break;
                                case IDC_SCROLLSPEED_EDIT:          source->SetInt(TEXT("scrollSpeed"), val); break;
                            }
                        }
                    }
                    break;

                case IDC_BOLD:
                case IDC_ITALIC:
                case IDC_UNDERLINE:
                case IDC_VERTICALSCRIPT:
                case IDC_WRAP:
                case IDC_SCROLLMODE:
                case IDC_USEOUTLINE:
                case IDC_USETEXTEXTENTS:
                    if(HIWORD(wParam) == BN_CLICKED && bInitializedDialog)
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
                                case IDC_SCROLLMODE:        source->SetInt(TEXT("scrollMode"), bChecked); break;
                                case IDC_USEOUTLINE:        source->SetInt(TEXT("useOutline"), bChecked); break;
                                case IDC_USETEXTEXTENTS:    source->SetInt(TEXT("useTextExtents"), bChecked); break;
                            }
                        }

                        if(LOWORD(wParam) == IDC_VERTICALSCRIPT)
                        {
                            bool bWrap = SendMessage(GetDlgItem(hwnd, IDC_WRAP), BM_GETCHECK, 0, 0) == BST_CHECKED;
                            bool bUseExtents = SendMessage(GetDlgItem(hwnd, IDC_USETEXTEXTENTS), BM_GETCHECK, 0, 0) == BST_CHECKED;
                            
                            EnableWindow(GetDlgItem(hwnd, IDC_SCROLLMODE), bWrap && bUseExtents && !bChecked);
                        }
                        else if(LOWORD(wParam) == IDC_WRAP)
                        {
                            bool bVertical = SendMessage(GetDlgItem(hwnd, IDC_VERTICALSCRIPT), BM_GETCHECK, 0, 0) == BST_CHECKED;

                            EnableWindow(GetDlgItem(hwnd, IDC_ALIGN), bChecked);
                            EnableWindow(GetDlgItem(hwnd, IDC_SCROLLMODE), bChecked && !bVertical);
                        }
                        else if(LOWORD(wParam) == IDC_USETEXTEXTENTS)
                        {
                            EnableWindow(GetDlgItem(hwnd, IDC_EXTENTWIDTH_EDIT), bChecked);
                            EnableWindow(GetDlgItem(hwnd, IDC_EXTENTHEIGHT_EDIT), bChecked);
                            EnableWindow(GetDlgItem(hwnd, IDC_EXTENTWIDTH), bChecked);
                            EnableWindow(GetDlgItem(hwnd, IDC_EXTENTHEIGHT), bChecked);
                            EnableWindow(GetDlgItem(hwnd, IDC_WRAP), bChecked);

                            bool bWrap = SendMessage(GetDlgItem(hwnd, IDC_WRAP), BM_GETCHECK, 0, 0) == BST_CHECKED;
                            bool bVertical = SendMessage(GetDlgItem(hwnd, IDC_VERTICALSCRIPT), BM_GETCHECK, 0, 0) == BST_CHECKED;
                            
                            EnableWindow(GetDlgItem(hwnd, IDC_ALIGN), bChecked && bWrap);
                            EnableWindow(GetDlgItem(hwnd, IDC_SCROLLMODE), bChecked && bWrap && !bVertical);
                        }
                        else if(LOWORD(wParam) == IDC_USEOUTLINE)
                        {
                            EnableWindow(GetDlgItem(hwnd, IDC_OUTLINETHICKNESS_EDIT), bChecked);
                            EnableWindow(GetDlgItem(hwnd, IDC_OUTLINETHICKNESS), bChecked);
                            EnableWindow(GetDlgItem(hwnd, IDC_OUTLINECOLOR), bChecked);
                            EnableWindow(GetDlgItem(hwnd, IDC_OUTLINEOPACITY_EDIT), bChecked);
                            EnableWindow(GetDlgItem(hwnd, IDC_OUTLINEOPACITY), bChecked);
                        }
                    }
                    break;

                case IDC_ALIGN:
                    if(HIWORD(wParam) == CBN_SELCHANGE && bInitializedDialog)
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
                    if(HIWORD(wParam) == EN_CHANGE && bInitializedDialog)
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
                    if(HIWORD(wParam) == BN_CLICKED && bInitializedDialog)
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
                    if(HIWORD(wParam) == BN_CLICKED && bInitializedDialog)
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
                        BOOL bUseOutline = SendMessage(GetDlgItem(hwnd, IDC_USEOUTLINE), BM_GETCHECK, 0, 0) == BST_CHECKED;
                        float outlineSize = (float)SendMessage(GetDlgItem(hwnd, IDC_OUTLINETHICKNESS), UDM_GETPOS32, 0, 0);

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

                        BOOL pointFiltering = SendMessage(GetDlgItem(hwnd, IDC_POINTFILTERING), BM_GETCHECK, 0, 0) == BST_CHECKED;

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
                            OBSMessageBox(hwnd, strError, NULL, 0);
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
                                if(strFile.IsEmpty() || !textFile.Open(strFile, XFILE_READ | XFILE_SHARED, XFILE_OPENEXISTING))
                                {
                                    String strError = Str("Sources.TextSource.FileNotFound");
                                    strError.FindReplace(TEXT("$1"), strFile);

                                    OBSMessageBox(hwnd, strError, NULL, 0);
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
                                OBSMessageBox(hwnd, strError, NULL, 0);
                                DeleteDC(hDC);
                                break;
                            }

                            {
                                Gdiplus::Graphics graphics(hDC);
                                Gdiplus::StringFormat format(Gdiplus::StringFormat::GenericTypographic());

                                UINT formatFlags;

                                formatFlags = Gdiplus::StringFormatFlagsNoFitBlackBox
                                            | Gdiplus::StringFormatFlagsMeasureTrailingSpaces;

                                if(bVertical)
                                    formatFlags |= Gdiplus::StringFormatFlagsDirectionVertical
                                                 | Gdiplus::StringFormatFlagsDirectionRightToLeft;

                                format.SetFormatFlags(formatFlags);
                                format.SetTrimming(Gdiplus::StringTrimmingWord);

                                Gdiplus::RectF rcf;
                                graphics.MeasureString(strOutputText, -1, &font, Gdiplus::PointF(0.0f, 0.0f), &format, &rcf);

                                if(bUseOutline)
                                {
                                    rcf.Height += outlineSize;
                                    rcf.Width  += outlineSize;
                                }

                                if(bVertical)
                                {
                                    if(rcf.Width<fontSize)
                                        rcf.Width = (float)fontSize;
                                }
                                else
                                {
                                    if(rcf.Height<fontSize)
                                        rcf.Height = (float)fontSize;
                                }
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
                        data->SetInt(TEXT("scrollMode"), SendMessage(GetDlgItem(hwnd, IDC_SCROLLMODE), BM_GETCHECK, 0, 0) == BST_CHECKED);
                        data->SetInt(TEXT("underline"), SendMessage(GetDlgItem(hwnd, IDC_UNDERLINE), BM_GETCHECK, 0, 0) == BST_CHECKED);
                        data->SetInt(TEXT("pointFiltering"), pointFiltering);

                        data->SetInt(TEXT("backgroundColor"), CCGetColor(GetDlgItem(hwnd, IDC_BACKGROUNDCOLOR)));
                        data->SetInt(TEXT("backgroundOpacity"), (UINT)SendMessage(GetDlgItem(hwnd, IDC_BACKGROUNDOPACITY), UDM_GETPOS32, 0, 0));

                        data->SetInt(TEXT("useOutline"), bUseOutline);
                        data->SetInt(TEXT("outlineColor"), CCGetColor(GetDlgItem(hwnd, IDC_OUTLINECOLOR)));
                        data->SetFloat(TEXT("outlineSize"), outlineSize);
                        data->SetInt(TEXT("outlineOpacity"), (UINT)SendMessage(GetDlgItem(hwnd, IDC_OUTLINEOPACITY), UDM_GETPOS32, 0, 0));

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

    if(OBSDialogBox(hinstMain, MAKEINTRESOURCE(IDD_CONFIGURETEXTSOURCE), hwndMain, ConfigureTextProc, (LPARAM)&configInfo) == IDOK)
    {
        element->SetFloat(TEXT("cx"), configInfo.cx);
        element->SetFloat(TEXT("cy"), configInfo.cy);

        return true;
    }

    return false;
}
