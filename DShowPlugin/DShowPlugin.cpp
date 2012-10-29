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


#include "DShowPlugin.h"

extern "C" __declspec(dllexport) bool LoadPlugin();
extern "C" __declspec(dllexport) void UnloadPlugin();
extern "C" __declspec(dllexport) CTSTR GetPluginName();
extern "C" __declspec(dllexport) CTSTR GetPluginDescription();

LocaleStringLookup *pluginLocale = NULL;
HINSTANCE hinstMain = NULL;


#define DSHOW_CLASSNAME TEXT("DeviceCapture")


bool SourceListHasDevice(CTSTR lpDevice, XElement *sourceList)
{
    UINT numSources = sourceList->NumElements();
    for(UINT i=0; i<numSources; i++)
    {
        XElement *sourceElement = sourceList->GetElementByID(i);
        if(scmpi(sourceElement->GetString(TEXT("class")), DSHOW_CLASSNAME) == 0)
        {
            XElement *data = sourceElement->GetElement(TEXT("data"));
            if(scmpi(data->GetString(TEXT("device")), lpDevice) == 0)
                return true;
        }
    }

    return false;
}

bool CurrentDeviceExists(CTSTR lpDevice, bool bGlobal, bool &isGlobal)
{
    isGlobal = false;

    XElement *globalSources = API->GetGlobalSourceListElement();
    if(globalSources)
    {
        if(SourceListHasDevice(lpDevice, globalSources))
        {
            isGlobal = true;
            return true;
        }
    }

    if(bGlobal)
    {
        XElement *sceneListElement = API->GetSceneListElement();
        if(sceneListElement)
        {
            UINT numScenes = sceneListElement->NumElements();
            for(UINT i=0; i<numScenes; i++)
            {
                XElement *sceneElement = sceneListElement->GetElementByID(i);
                if(sceneElement)
                {
                    XElement *sourceListElement = sceneElement->GetElement(TEXT("sources"));
                    if(sourceListElement)
                    {
                        if(SourceListHasDevice(lpDevice, sourceListElement))
                            return true;
                    }
                }
            }
        }
    }
    else
    {
        XElement *sceneElement = API->GetSceneElement();
        if(sceneElement)
        {
            XElement *sourceListElement = sceneElement->GetElement(TEXT("sources"));
            if(sourceListElement)
            {
                if(SourceListHasDevice(lpDevice, sourceListElement))
                    return true;
            }
        }
    }

    return false;
}


IBaseFilter* GetDeviceByName(CTSTR lpName)
{
    ICreateDevEnum *deviceEnum;
    IEnumMoniker *videoDeviceEnum;

    HRESULT err;
    err = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC, IID_ICreateDevEnum, (void**)&deviceEnum);
    if(FAILED(err))
    {
        AppWarning(TEXT("GetDeviceByName: CoCreateInstance for the device enum failed, result = %08lX"), err);
        return NULL;
    }

    err = deviceEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &videoDeviceEnum, 0);
    if(FAILED(err))
    {
        AppWarning(TEXT("GetDeviceByName: deviceEnum->CreateClassEnumerator failed, result = %08lX"), err);
        deviceEnum->Release();
        return NULL;
    }

    SafeRelease(deviceEnum);

    if(err == S_FALSE) //no devices, so NO ENUM FO U
        return NULL;

    IMoniker *deviceInfo;
    DWORD count;
    while(videoDeviceEnum->Next(1, &deviceInfo, &count) == S_OK)
    {
        IPropertyBag *propertyData;
        err = deviceInfo->BindToStorage(0, 0, IID_IPropertyBag, (void**)&propertyData);
        if(SUCCEEDED(err))
        {
            VARIANT valueThingy;
            valueThingy.vt = VT_BSTR;

            err = propertyData->Read(L"FriendlyName", &valueThingy, NULL);
            SafeRelease(propertyData);

            if(SUCCEEDED(err))
            {
                String strDeviceName = (CWSTR)valueThingy.bstrVal;
                if(strDeviceName == lpName)
                {
                    IBaseFilter *filter;
                    err = deviceInfo->BindToObject(NULL, 0, IID_IBaseFilter, (void**)&filter);
                    if(FAILED(err))
                    {
                        AppWarning(TEXT("GetDeviceByName: deviceInfo->BindToObject failed, result = %08lX"), err);
                        return NULL;
                    }

                    SafeRelease(deviceInfo);
                    SafeRelease(videoDeviceEnum);

                    return filter;
                }
            }
        }

        SafeRelease(deviceInfo);
    }

    SafeRelease(videoDeviceEnum);

    return NULL;
}


void FillOutListOfVideoDevices(HWND hwndCombo)
{
    SendMessage(hwndCombo, CB_RESETCONTENT, 0, 0);

    ICreateDevEnum *deviceEnum;
    IEnumMoniker *videoDeviceEnum;

    HRESULT err;
    err = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC, IID_ICreateDevEnum, (void**)&deviceEnum);
    if(FAILED(err))
    {
        AppWarning(TEXT("FillOutListOfVideoDevices: CoCreateInstance for the device enum failed, result = %08lX"), err);
        return;
    }

    err = deviceEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &videoDeviceEnum, 0);
    if(FAILED(err))
    {
        AppWarning(TEXT("FillOutListOfVideoDevices: deviceEnum->CreateClassEnumerator failed, result = %08lX"), err);
        deviceEnum->Release();
        return;
    }

    SafeRelease(deviceEnum);

    if(err == S_FALSE) //no devices
        return;

    IMoniker *deviceInfo;
    DWORD count;
    while(videoDeviceEnum->Next(1, &deviceInfo, &count) == S_OK)
    {
        IPropertyBag *propertyData;
        err = deviceInfo->BindToStorage(0, 0, IID_IPropertyBag, (void**)&propertyData);
        if(SUCCEEDED(err))
        {
            VARIANT valueThingy;
            valueThingy.vt = VT_BSTR;

            err = propertyData->Read(L"FriendlyName", &valueThingy, NULL);
            SafeRelease(propertyData);

            if(SUCCEEDED(err))
            {
                IBaseFilter *filter;
                err = deviceInfo->BindToObject(NULL, 0, IID_IBaseFilter, (void**)&filter);
                if(SUCCEEDED(err))
                {
                    String strDeviceName = (CWSTR)valueThingy.bstrVal;
                    if(SendMessage(hwndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)strDeviceName.Array()) == CB_ERR)
                        SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)strDeviceName.Array());

                    SafeRelease(filter);
                }
            }
        }

        SafeRelease(deviceInfo);
    }

    SafeRelease(videoDeviceEnum);
}

IPin* GetOutputPin(IBaseFilter *filter)
{
    IPin *foundPin = NULL;

    if(filter)
    {
        IEnumPins *pins;
        if(SUCCEEDED(filter->EnumPins(&pins)))
        {
            IPin *curPin;
            ULONG num;
            while(pins->Next(1, &curPin, &num) == S_OK)
            {
                PIN_DIRECTION pinDir;
                if(SUCCEEDED(curPin->QueryDirection(&pinDir))) 
                {
                    if(pinDir == PINDIR_OUTPUT)
                    {
                        IKsPropertySet *propertySet;
                        if(SUCCEEDED(curPin->QueryInterface(IID_IKsPropertySet, (void**)&propertySet)))
                        {
                            GUID pinCategory;
                            DWORD retSize;

                            PIN_INFO chi;
                            curPin->QueryPinInfo(&chi);

                            if(chi.pFilter)
                                chi.pFilter->Release();

                            if(SUCCEEDED(propertySet->Get(AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, NULL, 0, &pinCategory, sizeof(GUID), &retSize)))
                            {
                                if(pinCategory == PIN_CATEGORY_CAPTURE)
                                {
                                    SafeRelease(propertySet);
                                    SafeRelease(pins);
                                    return curPin;
                                }
                            }

                            SafeRelease(propertySet);
                        }
                    }
                }

                SafeRelease(curPin);
            }

            SafeRelease(pins);
        }
    }

    return foundPin;
}

void GetOutputList(IPin *curPin, List<MediaOutputInfo> &outputInfoList)
{
    IAMStreamConfig *config;
    if(SUCCEEDED(curPin->QueryInterface(IID_IAMStreamConfig, (void**)&config)))
    {
        int count, size;
        if(SUCCEEDED(config->GetNumberOfCapabilities(&count, &size)))
        {
            BYTE *capsData = (BYTE*)Allocate(size);

            int priority = -1;
            for(int i=0; i<count; i++)
            {
                AM_MEDIA_TYPE *pMT;
                if(SUCCEEDED(config->GetStreamCaps(i, &pMT, capsData)))
                {
                     VideoOutputType type = GetVideoOutputType(*pMT);

                    if(pMT->formattype == FORMAT_VideoInfo)
                    {
                        VIDEO_STREAM_CONFIG_CAPS *pVSCC = reinterpret_cast<VIDEO_STREAM_CONFIG_CAPS*>(capsData);
                        VIDEOINFOHEADER *pVih = reinterpret_cast<VIDEOINFOHEADER*>(pMT->pbFormat);

                        bool bUsingFourCC = false;
                        if(type == VideoOutputType_None)
                        {
                            type = GetVideoOutputTypeFromFourCC(pVih->bmiHeader.biCompression);
                            bUsingFourCC = true;
                        }

                        if(type != VideoOutputType_None)
                        {
                            MediaOutputInfo *outputInfo = outputInfoList.CreateNew();
                            outputInfo->mediaType = pMT;
                            outputInfo->videoType = type;
                            outputInfo->minFPS = 1000.0/(double(pVSCC->MaxFrameInterval)/10000.0);
                            outputInfo->maxFPS = 1000.0/(double(pVSCC->MinFrameInterval)/10000.0);
                            outputInfo->minCX = pVSCC->MinOutputSize.cx;
                            outputInfo->maxCX = pVSCC->MaxOutputSize.cx;
                            outputInfo->minCY = pVSCC->MinOutputSize.cy;
                            outputInfo->maxCY = pVSCC->MaxOutputSize.cy;
                            outputInfo->bUsingFourCC = bUsingFourCC;

                            //actually due to the other code in GetResolutionFPSInfo, we can have this granularity
                            // back to the way it was.  now, even if it's corrupted, it will always work
                            outputInfo->xGranularity = max(pVSCC->OutputGranularityX, 1);
                            outputInfo->yGranularity = max(pVSCC->OutputGranularityY, 1);
                        }
                        else
                        {
                            FreeMediaType(*pMT);
                            CoTaskMemFree(pMT);
                        }
                    }
                    else
                    {
                        FreeMediaType(*pMT);
                        CoTaskMemFree(pMT);
                    }
                }
            }

            Free(capsData);
        }

        SafeRelease(config);
    }
}


inline bool ResolutionListHasValue(const List<SIZE> &resolutions, SIZE &size)
{
    bool bHasResolution = false;

    for(UINT i=0; i<resolutions.Num(); i++)
    {
        SIZE &testSize = resolutions[i];
        if(size.cx == testSize.cx && size.cy == testSize.cy)
        {
            bHasResolution = true;
            break;
        }
    }

    return bHasResolution;
}


struct FPSInfo
{
    int minFPS, maxFPS;
};


bool GetClosestResolution(List<MediaOutputInfo> &outputList, SIZE &resolution, UINT &fps)
{
    LONG width, height;
    UINT internalFPS = API->GetMaxFPS();
    API->GetBaseSize((UINT&)width, (UINT&)height);

    LONG bestDistance = 0x7FFFFFFF;
    SIZE bestSize;
    UINT minFPS = 0;
    UINT bestFPS = 0;

    for(UINT i=0; i<outputList.Num(); i++)
    {
        MediaOutputInfo &outputInfo = outputList[i];

        LONG outputWidth  = outputInfo.minCX;
        do
        {
            LONG distWidth = width-outputWidth;
            if(distWidth < 0)
                break;

            if(distWidth > bestDistance)
            {
                outputWidth  += outputInfo.xGranularity;
                continue;
            }

            LONG outputHeight = outputInfo.minCY;
            do
            {
                LONG distHeight = height-outputHeight;
                if(distHeight < 0)
                    break;

                LONG totalDist = distHeight+distWidth;
                if((totalDist <= bestDistance) || (totalDist == bestDistance && outputInfo.maxFPS > bestFPS))
                {
                    bestDistance = totalDist;
                    bestSize.cx = outputWidth;
                    bestSize.cy = outputHeight;
                    minFPS = (UINT)outputInfo.minFPS;
                    bestFPS = (UINT)outputInfo.maxFPS;
                }

                outputHeight += outputInfo.yGranularity;
            }while((UINT)outputHeight <= outputInfo.maxCY);

            outputWidth  += outputInfo.xGranularity;
        }while((UINT)outputWidth <= outputInfo.maxCX);
    }

    if(bestDistance != 0x7FFFFFFF)
    {
        resolution.cx = bestSize.cx;
        resolution.cy = bestSize.cy;

        if(internalFPS < minFPS)
            fps = minFPS;
        else if(internalFPS > bestFPS)
            fps = bestFPS;
        else
            fps = internalFPS;
        return true;
    }

    return false;
}

struct ConfigDialogData
{
    CTSTR lpName;
    XElement *data;
    List<MediaOutputInfo> outputList;
    List<SIZE> resolutions;
    bool bGlobalSource;
    bool bCreating;

    ~ConfigDialogData()
    {
        ClearOutputList();
    }

    void ClearOutputList()
    {
        for(UINT i=0; i<outputList.Num(); i++)
            outputList[i].FreeData();
        outputList.Clear();
    }

    void GetResolutions(List<SIZE> &resolutions)
    {
        resolutions.Clear();

        for(UINT i=0; i<outputList.Num(); i++)
        {
            MediaOutputInfo &outputInfo = outputList[i];
            SIZE size;

            size.cx = outputInfo.minCX;
            size.cy = outputInfo.minCY;
            if(!ResolutionListHasValue(resolutions, size))
                resolutions << size;

            size.cx = outputInfo.maxCX;
            size.cy = outputInfo.maxCY;
            if(!ResolutionListHasValue(resolutions, size))
                resolutions << size;
        }

        //sort
        for(UINT i=0; i<resolutions.Num(); i++)
        {
            SIZE &rez = resolutions[i];

            for(UINT j=i+1; j<resolutions.Num(); j++)
            {
                SIZE &testRez = resolutions[j];

                if(testRez.cy < rez.cy)
                {
                    resolutions.SwapValues(i, j);
                    j = i;
                }
            }
        }
    }

    bool GetResolutionFPSInfo(SIZE &resolution, FPSInfo &fpsInfo)
    {
        fpsInfo.minFPS = -1;
        fpsInfo.maxFPS = -1;

        for(UINT i=0; i<outputList.Num(); i++)
        {
            MediaOutputInfo &outputInfo = outputList[i];

            if( UINT(resolution.cx) >= outputInfo.minCX && UINT(resolution.cx) <= outputInfo.maxCX &&
                UINT(resolution.cy) >= outputInfo.minCY && UINT(resolution.cy) <= outputInfo.maxCY )
            {
                if((resolution.cx-outputInfo.minCX) % outputInfo.xGranularity || (resolution.cy-outputInfo.minCY) % outputInfo.yGranularity)
                    return false;

                int minFPS = int(outputInfo.minFPS+0.5);
                int maxFPS = int(outputInfo.maxFPS+0.5);

                if(fpsInfo.minFPS == -1)
                {
                    fpsInfo.minFPS = minFPS;
                    fpsInfo.maxFPS = maxFPS;
                }
                else
                {
                    fpsInfo.minFPS = MIN(fpsInfo.minFPS, minFPS);
                    fpsInfo.maxFPS = MAX(fpsInfo.maxFPS, maxFPS);
                }
            }
        }

        return fpsInfo.minFPS != -1;
    }
};


bool GetResolution(HWND hwndResolution, SIZE &resolution, BOOL bSelChange)
{
    String strResolution;
    if(bSelChange)
        strResolution = GetCBText(hwndResolution);
    else
        strResolution = GetEditText(hwndResolution);

    if(strResolution.NumTokens('x') != 2)
        return false;

    String strCX = strResolution.GetToken(0, 'x');
    String strCY = strResolution.GetToken(1, 'x');

    if(strCX.IsEmpty() || strCX.IsEmpty() || !ValidIntString(strCX) || !ValidIntString(strCY))
        return false;

    UINT cx = strCX.ToInt();
    UINT cy = strCY.ToInt();

    if(cx < 32 || cy < 32 || cx > 4096 || cy > 4096)
        return false;

    resolution.cx = cx;
    resolution.cy = cy;

    return true;
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


INT_PTR CALLBACK ConfigureDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static bool bSelectingColor = false;
    static bool bMouseDown = false;
    static ColorSelectionData colorData;

    switch(message)
    {
        case WM_INITDIALOG:
            {
                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)lParam);

                ConfigDialogData *configData = (ConfigDialogData*)lParam;

                HWND hwndDeviceList     = GetDlgItem(hwnd, IDC_DEVICELIST);
                HWND hwndResolutionList = GetDlgItem(hwnd, IDC_RESOLUTION);
                HWND hwndFPS            = GetDlgItem(hwnd, IDC_FPS);
                HWND hwndFlip           = GetDlgItem(hwnd, IDC_FLIPIMAGE);

                //------------------------------------------

                bool bFlipVertical = configData->data->GetInt(TEXT("flipImage")) != 0;
                SendMessage(hwndFlip, BM_SETCHECK, bFlipVertical ? BST_CHECKED : BST_UNCHECKED, 0);

                //------------------------------------------

                String strDevice = configData->data->GetString(TEXT("device"));
                UINT cx  = configData->data->GetInt(TEXT("resolutionWidth"));
                UINT cy  = configData->data->GetInt(TEXT("resolutionHeight"));
                UINT fps = configData->data->GetInt(TEXT("fps"));

                BOOL bCustomResolution = configData->data->GetInt(TEXT("customResolution"));
                SendMessage(GetDlgItem(hwnd, IDC_CUSTOMRESOLUTION), BM_SETCHECK, bCustomResolution ? BST_CHECKED : BST_UNCHECKED, 0);

                LocalizeWindow(hwnd, pluginLocale);
                FillOutListOfVideoDevices(GetDlgItem(hwnd, IDC_DEVICELIST));

                UINT deviceID = CB_ERR;
                if(strDevice.IsValid() && cx > 0 && cy > 0 && fps > 0)
                    deviceID = (UINT)SendMessage(hwndDeviceList, CB_FINDSTRINGEXACT, -1, (LPARAM)strDevice.Array());

                if(deviceID == CB_ERR)
                {
                    SendMessage(hwndDeviceList, CB_SETCURSEL, 0, 0);
                    ConfigureDialogProc(hwnd, WM_COMMAND, MAKEWPARAM(IDC_DEVICELIST, CBN_SELCHANGE), (LPARAM)hwndDeviceList);
                }
                else
                {
                    SendMessage(hwndDeviceList, CB_SETCURSEL, deviceID, 0);
                    ConfigureDialogProc(hwnd, WM_COMMAND, MAKEWPARAM(IDC_DEVICELIST, CBN_SELCHANGE), (LPARAM)hwndDeviceList);

                    if(bCustomResolution)
                    {
                        String strResolution;
                        strResolution << UIntString(cx) << TEXT("x") << UIntString(cy);

                        SendMessage(hwndResolutionList, WM_SETTEXT, 0, (LPARAM)strResolution.Array());
                        ConfigureDialogProc(hwnd, WM_COMMAND, MAKEWPARAM(IDC_RESOLUTION, CBN_EDITCHANGE), (LPARAM)hwndResolutionList);

                        SendMessage(hwndFPS, UDM_SETPOS32, 0, (LPARAM)fps);
                    }
                }

                ConfigureDialogProc(hwnd, WM_COMMAND, MAKEWPARAM(IDC_CUSTOMRESOLUTION, BN_CLICKED), (LPARAM)GetDlgItem(hwnd, IDC_CUSTOMRESOLUTION));

                //------------------------------------------

                BOOL  bUseColorKey  = configData->data->GetInt(TEXT("useColorKey"), 0);
                DWORD keyColor      = configData->data->GetInt(TEXT("keyColor"), 0xFFFFFFFF);
                UINT  similarity    = configData->data->GetInt(TEXT("keySimilarity"), 0);
                UINT  blend         = configData->data->GetInt(TEXT("keyBlend"), 10);
                UINT  gamma         = configData->data->GetInt(TEXT("keyGamma"), 0);

                SendMessage(GetDlgItem(hwnd, IDC_USECOLORKEY), BM_SETCHECK, bUseColorKey ? BST_CHECKED : BST_UNCHECKED, 0);
                CCSetColor(GetDlgItem(hwnd, IDC_COLOR), keyColor);

                SendMessage(GetDlgItem(hwnd, IDC_BASETHRESHOLD), UDM_SETRANGE32, 0, 100);
                SendMessage(GetDlgItem(hwnd, IDC_BASETHRESHOLD), UDM_SETPOS32, 0, similarity);

                SendMessage(GetDlgItem(hwnd, IDC_BLEND), UDM_SETRANGE32, 0, 100);
                SendMessage(GetDlgItem(hwnd, IDC_BLEND), UDM_SETPOS32, 0, blend);

                SendMessage(GetDlgItem(hwnd, IDC_GAMMA), UDM_SETRANGE32, -75, 75);
                SendMessage(GetDlgItem(hwnd, IDC_GAMMA), UDM_SETPOS32, 0, gamma);

                EnableWindow(GetDlgItem(hwnd, IDC_COLOR), bUseColorKey);
                EnableWindow(GetDlgItem(hwnd, IDC_SELECTCOLOR), bUseColorKey);
                EnableWindow(GetDlgItem(hwnd, IDC_BASETHRESHOLD_EDIT), bUseColorKey);
                EnableWindow(GetDlgItem(hwnd, IDC_BASETHRESHOLD), bUseColorKey);
                EnableWindow(GetDlgItem(hwnd, IDC_BLEND_EDIT), bUseColorKey);
                EnableWindow(GetDlgItem(hwnd, IDC_BLEND), bUseColorKey);
                EnableWindow(GetDlgItem(hwnd, IDC_GAMMA_EDIT), bUseColorKey);
                EnableWindow(GetDlgItem(hwnd, IDC_GAMMA), bUseColorKey);

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
            }
            break;

        case WM_MOUSEMOVE:
            if(bSelectingColor && bMouseDown)
                CCSetColor(GetDlgItem(hwnd, IDC_COLOR), colorData.GetColor());
            break;

        case WM_LBUTTONUP:
            if(bSelectingColor)
            {
                colorData.Clear();
                ReleaseCapture();
                bMouseDown = false;
                bSelectingColor = false;
            }
            break;

        case WM_CAPTURECHANGED:
            if(bSelectingColor)
            {
                if(colorData.bValid)
                {
                    CCSetColor(GetDlgItem(hwnd, IDC_COLOR), colorData.GetColor());
                    colorData.Clear();
                }

                ReleaseCapture();
                bMouseDown = false;
                bSelectingColor = false;
            }
            break;

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_CUSTOMRESOLUTION:
                    {
                        HWND hwndUseCustomResolution = (HWND)lParam;
                        BOOL bCustomResolution = SendMessage(hwndUseCustomResolution, BM_GETCHECK, 0, 0) == BST_CHECKED;

                        EnableWindow(GetDlgItem(hwnd, IDC_RESOLUTION), bCustomResolution);
                        EnableWindow(GetDlgItem(hwnd, IDC_FPS), bCustomResolution);
                        EnableWindow(GetDlgItem(hwnd, IDC_FPS_EDIT), bCustomResolution);
                        break;
                    }

                case IDC_USECOLORKEY:
                    {
                        HWND hwndUseColorKey = (HWND)lParam;
                        BOOL bUseColorKey = SendMessage(hwndUseColorKey, BM_GETCHECK, 0, 0) == BST_CHECKED;

                        ConfigDialogData *configData = (ConfigDialogData*)GetWindowLongPtr(hwnd, DWLP_USER);
                        ImageSource *source = API->GetSceneImageSource(configData->lpName);
                        if(source)
                            source->SetInt(TEXT("useColorKey"), bUseColorKey);

                        EnableWindow(GetDlgItem(hwnd, IDC_COLOR), bUseColorKey);
                        EnableWindow(GetDlgItem(hwnd, IDC_SELECTCOLOR), bUseColorKey);
                        EnableWindow(GetDlgItem(hwnd, IDC_BASETHRESHOLD_EDIT), bUseColorKey);
                        EnableWindow(GetDlgItem(hwnd, IDC_BASETHRESHOLD), bUseColorKey);
                        EnableWindow(GetDlgItem(hwnd, IDC_BLEND_EDIT), bUseColorKey);
                        EnableWindow(GetDlgItem(hwnd, IDC_BLEND), bUseColorKey);
                        EnableWindow(GetDlgItem(hwnd, IDC_GAMMA_EDIT), bUseColorKey);
                        EnableWindow(GetDlgItem(hwnd, IDC_GAMMA), bUseColorKey);
                        break;
                    }

                case IDC_SELECTCOLOR:
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
                            }
                            else
                                colorData.Clear();
                        }
                        break;
                    }

                case IDC_COLOR:
                    {
                        ConfigDialogData *configData = (ConfigDialogData*)GetWindowLongPtr(hwnd, DWLP_USER);
                        ImageSource *source = API->GetSceneImageSource(configData->lpName);

                        if(source)
                        {
                            DWORD color = CCGetColor((HWND)lParam);
                            source->SetInt(TEXT("keyColor"), color);
                        }
                        break;
                    }

                case IDC_FLIPIMAGE:
                    if(HIWORD(wParam) == BN_CLICKED)
                    {
                        ConfigDialogData *configData = (ConfigDialogData*)GetWindowLongPtr(hwnd, DWLP_USER);
                        ImageSource *source = API->GetSceneImageSource(configData->lpName);
                        if(source)
                        {
                            HWND hwndFlip = (HWND)lParam;
                            BOOL bFlipImage = SendMessage(hwndFlip, BM_GETCHECK, 0, 0) == BST_CHECKED;

                            source->SetInt(TEXT("flipImage"), bFlipImage);
                        }
                    }
                    break;

                case IDC_BASETHRESHOLD_EDIT:
                case IDC_BLEND_EDIT:
                case IDC_GAMMA_EDIT:
                    if(HIWORD(wParam) == EN_CHANGE)
                    {
                        ConfigDialogData *configData = (ConfigDialogData*)GetWindowLongPtr(hwnd, DWLP_USER);
                        if(configData)
                        {
                            ImageSource *source = API->GetSceneImageSource(configData->lpName);

                            if(source)
                            {
                                HWND hwndVal = NULL;
                                switch(LOWORD(wParam))
                                {
                                    case IDC_BASETHRESHOLD_EDIT: hwndVal = GetDlgItem(hwnd, IDC_BASETHRESHOLD); break;
                                    case IDC_BLEND_EDIT:         hwndVal = GetDlgItem(hwnd, IDC_BLEND); break;
                                    case IDC_GAMMA_EDIT:         hwndVal = GetDlgItem(hwnd, IDC_GAMMA); break;
                                }

                                int val = (int)SendMessage(hwndVal, UDM_GETPOS32, 0, 0);
                                switch(LOWORD(wParam))
                                {
                                    case IDC_BASETHRESHOLD_EDIT: source->SetInt(TEXT("keySimilarity"), val); break;
                                    case IDC_BLEND_EDIT:         source->SetInt(TEXT("keyBlend"), val); break;
                                    case IDC_GAMMA_EDIT:         source->SetInt(TEXT("keyGamma"), val); break;
                                }
                            }
                        }
                    }
                    break;

                case IDC_REFRESH:
                    {
                        HWND hwndDeviceList = GetDlgItem(hwnd, IDC_DEVICELIST);

                        FillOutListOfVideoDevices(hwndDeviceList);
                        SendMessage(hwndDeviceList, CB_SETCURSEL, 0, 0);
                        ConfigureDialogProc(hwnd, WM_COMMAND, MAKEWPARAM(IDC_DEVICELIST, CBN_SELCHANGE), (LPARAM)hwndDeviceList);
                        break;
                    }

                case IDC_DEVICELIST:
                    if(HIWORD(wParam) == CBN_SELCHANGE)
                    {
                        HWND hwndResolutions = GetDlgItem(hwnd, IDC_RESOLUTION);
                        SendMessage(hwndResolutions, CB_RESETCONTENT, 0, 0);

                        HWND hwndDevices = (HWND)lParam;
                        UINT id = (UINT)SendMessage(hwndDevices, CB_GETCURSEL, 0, 0);
                        if(id == CB_ERR)
                        {
                            EnableWindow(GetDlgItem(hwnd, IDC_RESOLUTION), FALSE);
                            EnableWindow(GetDlgItem(hwnd, IDC_FPS), FALSE);
                            EnableWindow(GetDlgItem(hwnd, IDC_FPS_EDIT), FALSE);
                            EnableWindow(GetDlgItem(hwnd, IDC_CONFIG), FALSE);
                            EnableWindow(GetDlgItem(hwnd, IDOK), FALSE);
                        }
                        else
                        {
                            EnableWindow(GetDlgItem(hwnd, IDC_RESOLUTION), TRUE);
                            EnableWindow(GetDlgItem(hwnd, IDC_FPS), TRUE);
                            EnableWindow(GetDlgItem(hwnd, IDC_FPS_EDIT), TRUE);
                            EnableWindow(GetDlgItem(hwnd, IDC_CONFIG), TRUE);
                            EnableWindow(GetDlgItem(hwnd, IDOK), TRUE);

                            ConfigDialogData *configData = (ConfigDialogData*)GetWindowLongPtr(hwnd, DWLP_USER);

                            String strSel = GetCBText(hwndDevices, id);
                            IBaseFilter *filter = GetDeviceByName(strSel);
                            if(filter)
                            {
                                IPin *outputPin = GetOutputPin(filter);
                                if(outputPin)
                                {
                                    configData->ClearOutputList();
                                    GetOutputList(outputPin, configData->outputList);

                                    configData->GetResolutions(configData->resolutions);
                                    for(UINT i=0; i<configData->resolutions.Num(); i++)
                                    {
                                        SIZE &resolution = configData->resolutions[i];

                                        String strResolution;
                                        strResolution << IntString(resolution.cx) << TEXT("x") << IntString(resolution.cy);
                                        SendMessage(hwndResolutions, CB_ADDSTRING, 0, (LPARAM)strResolution.Array());
                                    }

                                    outputPin->Release();
                                }

                                filter->Release();
                            }

                            //-------------------------------------------------

                            SIZE size;
                            UINT fps;
                            if(GetClosestResolution(configData->outputList, size, fps))
                            {
                                String strResolution;
                                strResolution << UIntString(size.cx) << TEXT("x") << UIntString(size.cy);

                                SendMessage(hwndResolutions, WM_SETTEXT, 0, (LPARAM)strResolution.Array());
                                ConfigureDialogProc(hwnd, WM_COMMAND, MAKEWPARAM(IDC_RESOLUTION, CBN_EDITCHANGE), (LPARAM)hwndResolutions);

                                HWND hwndFPS = GetDlgItem(hwnd, IDC_FPS);
                                SendMessage(hwndFPS, UDM_SETPOS32, 0, fps);
                            }
                            else
                            {
                                SendMessage(hwndResolutions, CB_SETCURSEL, 0, 0);
                                ConfigureDialogProc(hwnd, WM_COMMAND, MAKEWPARAM(IDC_RESOLUTION, CBN_SELCHANGE), (LPARAM)hwndResolutions);
                            }
                        }
                    }
                    break;

                case IDC_RESOLUTION:
                    if(HIWORD(wParam) == CBN_EDITCHANGE || HIWORD(wParam) == CBN_SELCHANGE)
                    {
                        ConfigDialogData *configData = (ConfigDialogData*)GetWindowLongPtr(hwnd, DWLP_USER);

                        HWND hwndResolution = (HWND)lParam;
                        HWND hwndFPS        = GetDlgItem(hwnd, IDC_FPS);
                        HWND hwndFPSEdit    = GetDlgItem(hwnd, IDC_FPS_EDIT);

                        SIZE resolution;
                        FPSInfo fpsInfo;

                        if(!GetResolution(hwndResolution, resolution, HIWORD(wParam) == CBN_SELCHANGE) || !configData->GetResolutionFPSInfo(resolution, fpsInfo))
                        {
                            SendMessage(hwndFPS, UDM_SETRANGE32, 0, 0);
                            SendMessage(hwndFPS, UDM_SETPOS32, 0, 0);
                            SetWindowText(hwndFPSEdit, TEXT("0"));
                            break;
                        }

                        SendMessage(hwndFPS, UDM_SETRANGE32, fpsInfo.minFPS, fpsInfo.maxFPS);
                        SendMessage(hwndFPS, UDM_SETPOS32, 0, fpsInfo.maxFPS);

                        String strFPS = IntString(fpsInfo.maxFPS);
                        SetWindowText(hwndFPSEdit, strFPS.Array());
                    }
                    break;

                case IDC_CONFIG:
                    {
                        UINT id = (UINT)SendMessage(GetDlgItem(hwnd, IDC_DEVICELIST), CB_GETCURSEL, 0, 0);
                        if(id != CB_ERR)
                        {
                            String strSel = GetCBText(GetDlgItem(hwnd, IDC_DEVICELIST), id);
                            IBaseFilter *filter = GetDeviceByName(strSel);
                            if(filter)
                            {
                                ISpecifyPropertyPages *propPages;
                                CAUUID cauuid;

                                if(SUCCEEDED(filter->QueryInterface(IID_ISpecifyPropertyPages, (void**)&propPages)))
                                {
                                    if(SUCCEEDED(propPages->GetPages(&cauuid)))
                                    {
                                        if(cauuid.cElems)
                                        {
                                            OleCreatePropertyFrame(hwnd, 0, 0, NULL, 1, (LPUNKNOWN*)&filter, cauuid.cElems, cauuid.pElems, 0, 0, NULL);
                                            CoTaskMemFree(cauuid.pElems);
                                        }
                                    }
                                    propPages->Release();
                                }

                                filter->Release();
                            }
                        }
                    }
                    break;

                case IDOK:
                    {
                        UINT deviceID = (UINT)SendMessage(GetDlgItem(hwnd, IDC_DEVICELIST), CB_GETCURSEL, 0, 0);
                        if(deviceID == CB_ERR)
                            break;

                        ConfigDialogData *configData = (ConfigDialogData*)GetWindowLongPtr(hwnd, DWLP_USER);

                        SIZE resolution;
                        if(!GetResolution(GetDlgItem(hwnd, IDC_RESOLUTION), resolution, FALSE))
                        {
                            MessageBox(hwnd, PluginStr("DeviceSelection.InvalidResolution"), NULL, 0);
                            break;
                        }

                        String strDevice = GetCBText(GetDlgItem(hwnd, IDC_DEVICELIST), deviceID);

                        BOOL bUDMError;
                        UINT fps = (UINT)SendMessage(GetDlgItem(hwnd, IDC_FPS), UDM_GETPOS32, 0, (LPARAM)&bUDMError);
                        if(bUDMError)
                            break;

                        if(fps == 0)
                        {
                            MessageBox(hwnd, PluginStr("DeviceSelection.UnsupportedResolution"), NULL, 0);
                            break;
                        }

                        if(configData->bCreating)
                        {
                            bool bFoundGlobal;
                            if(CurrentDeviceExists(strDevice, configData->bGlobalSource, bFoundGlobal))
                            {
                                if(bFoundGlobal)
                                    MessageBox(hwnd, PluginStr("DeviceSelection.GlobalExists"), NULL, 0);
                                else
                                {
                                    if(configData->bGlobalSource)
                                        MessageBox(hwnd, PluginStr("DeviceSelection.ExistsSomewhere"), NULL, 0);
                                    else
                                        MessageBox(hwnd, PluginStr("DeviceSelection.ExistsInScene"), NULL, 0);
                                }

                                break;
                            }
                        }

                        BOOL bFlip = SendMessage(GetDlgItem(hwnd, IDC_FLIPIMAGE), BM_GETCHECK, 0, 0) == BST_CHECKED;
                        BOOL bCustomResolution = SendMessage(GetDlgItem(hwnd, IDC_CUSTOMRESOLUTION), BM_GETCHECK, 0, 0) == BST_CHECKED;

                        configData->data->SetString(TEXT("device"), strDevice);
                        configData->data->SetInt(TEXT("customResolution"), bCustomResolution);
                        configData->data->SetInt(TEXT("resolutionWidth"), resolution.cx);
                        configData->data->SetInt(TEXT("resolutionHeight"), resolution.cy);
                        configData->data->SetInt(TEXT("fps"), fps);
                        configData->data->SetInt(TEXT("flipImage"), bFlip);

                        BOOL bUseColorKey = SendMessage(GetDlgItem(hwnd, IDC_USECOLORKEY), BM_GETCHECK, 0, 0) == BST_CHECKED;
                        DWORD color = CCGetColor(GetDlgItem(hwnd, IDC_COLOR));

                        UINT keySimilarity = (UINT)SendMessage(GetDlgItem(hwnd, IDC_BASETHRESHOLD), UDM_GETPOS32, 0, (LPARAM)&bUDMError);
                        if(bUDMError) keySimilarity = 0;

                        UINT keyBlend = (UINT)SendMessage(GetDlgItem(hwnd, IDC_BLEND), UDM_GETPOS32, 0, (LPARAM)&bUDMError);
                        if(bUDMError) keyBlend = 10;

                        int keyGamma = (int)SendMessage(GetDlgItem(hwnd, IDC_GAMMA), UDM_GETPOS32, 0, (LPARAM)&bUDMError);
                        if(bUDMError) keyGamma = 0;

                        configData->data->SetInt(TEXT("useColorKey"), bUseColorKey);
                        configData->data->SetInt(TEXT("keyColor"), color);
                        configData->data->SetInt(TEXT("keySimilarity"), keySimilarity);
                        configData->data->SetInt(TEXT("keyBlend"), keyBlend);
                        configData->data->SetInt(TEXT("keyGamma"), keyGamma);
                    }

                case IDCANCEL:
                    if(LOWORD(wParam) == IDCANCEL)
                    {
                        ConfigDialogData *configData = (ConfigDialogData*)GetWindowLongPtr(hwnd, DWLP_USER);
                        ImageSource *source = API->GetSceneImageSource(configData->lpName);

                        if(source)
                        {
                            source->SetInt(TEXT("flipImage"),     configData->data->GetInt(TEXT("flipImage"), 0));

                            source->SetInt(TEXT("useColorKey"),   configData->data->GetInt(TEXT("useColorKey"), 0));
                            source->SetInt(TEXT("keyColor"),      configData->data->GetInt(TEXT("keyColor"), 0xFFFFFFFF));
                            source->SetInt(TEXT("keySimilarity"), configData->data->GetInt(TEXT("keySimilarity"), 0));
                            source->SetInt(TEXT("keyBlend"),      configData->data->GetInt(TEXT("keyBlend"), 10));
                            source->SetInt(TEXT("keyGamma"),      configData->data->GetInt(TEXT("keyGamma"), 0));
                        }
                    }

                    EndDialog(hwnd, LOWORD(wParam));
            }
    }

    return FALSE;
}


bool STDCALL ConfigureDShowSource(XElement *element, bool bCreating)
{
    if(!element)
    {
        AppWarning(TEXT("ConfigureDShowSource: NULL element"));
        return false;
    }

    XElement *data = element->GetElement(TEXT("data"));
    if(!data)
        data = element->CreateElement(TEXT("data"));

    ConfigDialogData *configData = new ConfigDialogData;
    configData->lpName = element->GetName();
    configData->data = data;
    configData->bGlobalSource = (scmpi(element->GetParent()->GetName(), TEXT("global sources")) == 0);
    configData->bCreating = bCreating;

    if(DialogBoxParam(hinstMain, MAKEINTRESOURCE(IDD_CONFIG), API->GetMainWindow(), ConfigureDialogProc, (LPARAM)configData) == IDOK)
    {
        element->SetInt(TEXT("cx"), data->GetInt(TEXT("resolutionWidth")));
        element->SetInt(TEXT("cy"), data->GetInt(TEXT("resolutionHeight")));

        delete configData;
        return true;
    }

    delete configData;
    return false;
}

ImageSource* STDCALL CreateDShowSource(XElement *data)
{
    DeviceSource *source = new DeviceSource;
    if(!source->Init(data))
    {
        delete source;
        return NULL;
    }

    return source;
}


bool LoadPlugin()
{
    traceIn(DShowPluginLoadPlugin);

    InitColorControl(hinstMain);

    pluginLocale = new LocaleStringLookup;

    if(!pluginLocale->LoadStringFile(TEXT("plugins/DShowPlugin/locale/en.txt")))
        AppWarning(TEXT("Could not open locale string file '%s'"), TEXT("plugins/DShowPlugin/locale/en.txt"));

    if(scmpi(API->GetLanguage(), TEXT("en")) != 0)
    {
        String pluginStringFile;
        pluginStringFile << TEXT("plugins/DShowPlugin/locale/") << API->GetLanguage() << TEXT(".txt");
        if(!pluginLocale->LoadStringFile(pluginStringFile))
            AppWarning(TEXT("Could not open locale string file '%s'"), pluginStringFile.Array());
    }

    API->RegisterImageSourceClass(DSHOW_CLASSNAME, PluginStr("ClassName"), (OBSCREATEPROC)CreateDShowSource, (OBSCONFIGPROC)ConfigureDShowSource);

    return true;

    traceOut;
}

void UnloadPlugin()
{
    delete pluginLocale;
}

CTSTR GetPluginName()
{
    return PluginStr("Plugin.Name");
}

CTSTR GetPluginDescription()
{
    return PluginStr("Plugin.Description");
}


BOOL CALLBACK DllMain(HINSTANCE hInst, DWORD dwReason, LPVOID lpBla)
{
    if(dwReason == DLL_PROCESS_ATTACH)
        hinstMain = hInst;

    return TRUE;
}

