/*******************************************************************************
Copyright ©2013 Advanced Micro Devices, Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1   Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
2   Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

// Ripped from Media SDK 1.0 with slight changes.

/**
********************************************************************************
* @file <MftUtils.h>
*
* @brief This file contains common functionality required for creating the
*        topology
*
********************************************************************************
*/

#ifndef _MFTUTILS_H_
#define _MFTUTILS_H_

/*******************************************************************************
*                             INCLUDE FILES                                    *
*******************************************************************************/
#include <Windows.h>
#include <initguid.h>
#include <cstdint>

#include <mfidl.h>
#include <wmcontainer.h>
#include <codecapi.h>
#include <wmcodecdsp.h>
#include <atlbase.h>
#include <evr.h>

#include <sstream>
#include <Mferror.h>

#include <atlcomcli.h>
#include <d3d11.h>
//#include <amf\AmfMft.h>

#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>

#include <d3d9.h>
#include <d3d9types.h>
#include <dxva2api.h>
#include "PrintLog.h"
//#include "VqMft.h"

/* GetVersionEx deprecated on VS2013*/
//#pragma warning(disable: 4996)
//In Win8.1 SDK, but can be used with older windows'
#include "VersionHelpers.h"

#define TRACE_MSG(message, param) \
{ \
    std::wostringstream stream; \
    stream << message << L" Param: " << param << \
    L". Where: " << __FILEW__ << \
    L" at line " << std::dec << __LINE__ << std::endl; \
    OutputDebugString(stream.str().c_str()); \
}

typedef struct _AMF_MFT_VIDEOENCODERPARAMS
{
	DWORD compressionStandard; // Compression standard (VEP_COMPRESSIONSTANDARD_XXX)
	DWORD gopSize; // Max number of frames in a GOP (0=auto)
	DWORD numBFrames; // Max number of consecutive B-frames
	DWORD maxBitrate; // Peak (maximum) bitrate of encoded video (only used for VBR)
	DWORD meanBitrate; // Bitrate of encoded video (bits per second)
	DWORD bufSize; // VBR buffer size
	DWORD rateControlMethod; // Rate control Method
	DWORD lowLatencyMode; // low latency mode (uses only 1 reference frame )
	DWORD qualityVsSpeed; // 0 - low quality faster encoding 100 - Higher quality, slower encoding
	DWORD commonQuality; // This parameter is used only when is encRateControlMethod  set to eAVEncCommonRateControlMode_Quality. in this mode encoder selects the bit rate to match the quality settings
	DWORD enableCabac; // 0 disable CABAC 1 Enable
	DWORD idrPeriod; // IDR interval 
} AMF_MFT_VIDEOENCODERPARAMS;

typedef struct _AMF_MFT_COMMONPARAMS
{
	DWORD useInterop; //UseInterop
	DWORD useSWCodec; //UseSWCodec
} AMF_MFT_COMMONPARAMS;

/**
*   @brief MF_LOW_LATENCY exists only on Windows 8+. So it is borrowed to be able run samples on Windows 7.
*/
const GUID BORROWED_MF_LOW_LATENCY = { 0x9c27891a, 0xed7a, 0x40e1,
{ 0x88, 0xe8, 0xb2, 0x27, 0x27, 0xa0,
0x24, 0xee } };

/**
*   @brief MF_SA_D3D11_AWARE exists only on Windows 8+. So it is borrowed to be able run samples on Windows 7.
*/
const GUID BORROWED_MF_SA_D3D11_AWARE =
{ 0x206b4fc8, 0xfcf9, 0x4c51, { 0xaf, 0xe3, 0x97, 0x64, 0x36,
0x9e, 0x33, 0xa0 } };

/**
*   @brief IID_IMFDXGIDeviceManager exists only on Windows 8+. So it is borrowed to be able run samples on Windows 7.
*/
const IID BORROWED_IID_IMFDXGIDeviceManager = { 0xeb533d5d, 0x2db6, 0x40f8,
{ 0x97, 0xa9, 0x49, 0x46,
0x92, 0x01, 0x4f, 0x07 } };

/**
*   @brief CODECAPI_AVEncVideoTemporalLayerCount exists only on Windows 8+. So it is borrowed to be able run samples on Windows 7.
*/
const GUID BORROWED_CODECAPI_AVEncVideoTemporalLayerCount = { 0x19CAEBFF,
0xB74D, 0x4CFD,
{ 0x8C, 0x27,
0xC2, 0xF9,
0xD9, 0x7D,
0x5F, 0x52 } };

/**
*   @brief Attribute containing flag which tells whether to use Interop or not.
*   // {FB29638A-9B46-49B6-9E2C-E6AE4F738D92}
*/
const GUID ATTRIBUTE_USE_INTEROP =
{ 0xfb29638a, 0x9b46, 0x49b6, { 0x9e, 0x2c, 0xe6, 0xae, 0x4f, 0x73, 0x8d, 0x92 } };


/**
* msdk_CMftBuilder
* Common MFT Util APIs
**/
class msdk_CMftBuilder
{
public:
    FILE *mLogFile;
    void setLogFile(FILE *logFile)
    {
        mLogFile = logFile;
    }

    /**
    *****************************************************************************
    *  @fn     isDxgiSupported
    *  @brief  Checks if Dx11 is supported or not
    *
    *
    *  @return bool : TRUE if Dx11 is supported, FALSE otherwise.
    *****************************************************************************
    */
    bool isDxgiSupported()
    {
        /*************************************************************************
        * DXGI API supported only on Windows 8                                   *
        *************************************************************************/

        return IsWindows8OrGreater();
        /*OSVERSIONINFO osVersionInfo;
        osVersionInfo.dwOSVersionInfoSize = sizeof(osVersionInfo);

        if (::GetVersionEx(&osVersionInfo))
        {
            return osVersionInfo.dwMajorVersion >= 6
                && osVersionInfo.dwMinorVersion >= 2;
        }
        return false;*/
    }
    /**
    *****************************************************************************
    *  @fn     createDxgiDeviceManagerPtr
    *  @brief  Creates the transform with given class id
    *
    *  @param[out] deviceManagerPtr             : Device pointer
    *
    *  @return HRESULT : S_OK if successful; otherwise returns microsoft error codes.
    *****************************************************************************
    */
    HRESULT createDxgiDeviceManagerPtr(ULONG_PTR* deviceManagerPtr)
    {
        if (deviceManagerPtr == nullptr)
        {
            return E_POINTER;
        }

        const D3D_FEATURE_LEVEL features[] = { D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0 };

        D3D_FEATURE_LEVEL supportedD3DFeatureLevel;

        UINT deviceFlags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT
            | D3D11_CREATE_DEVICE_DEBUG;

        HRESULT hr;

        CComPtr<ID3D11Device> d3dDevice;
        CComPtr<ID3D11DeviceContext> d3dContext;
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            deviceFlags, features, ARRAYSIZE(features),
            D3D11_SDK_VERSION, &d3dDevice,
            &supportedD3DFeatureLevel, &d3dContext);
        if (FAILED(hr))
        {
            deviceFlags &= (~D3D11_CREATE_DEVICE_DEBUG);
            hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                deviceFlags, features, ARRAYSIZE(features),
                D3D11_SDK_VERSION, &d3dDevice,
                &supportedD3DFeatureLevel, &d3dContext);
            RETURNIFFAILED(hr);
        }
        /**********************************************************************
        * Function MFCreateDXGIDeviceManager exists only on Windows 8+. So    *
        * we have to obtain its address dynamically to be able work on        *
        * Windows 7.                                                          *
        **********************************************************************/

        HMODULE hMfplatDll = LoadLibrary(L"mfplat.dll");
        if (NULL == hMfplatDll)
        {
            TRACE_MSG("Failed to load mfplat.dll", hMfplatDll)
                return E_FAIL;
        }

        LPCSTR mfCreateDXGIDeviceManagerProcName = "MFCreateDXGIDeviceManager";
        FARPROC mfCreateDXGIDeviceManagerFarProc = GetProcAddress(hMfplatDll,
            mfCreateDXGIDeviceManagerProcName);
        if (nullptr == mfCreateDXGIDeviceManagerFarProc)
        {
            TRACE_MSG("Failed to get MFCreateDXGIDeviceManager() address", mfCreateDXGIDeviceManagerFarProc)
                return E_FAIL;
        }

        typedef HRESULT(STDAPICALLTYPE* LPMFCreateDXGIDeviceManager)(UINT* resetToken, IMFDXGIDeviceManager** ppDeviceManager);

        LPMFCreateDXGIDeviceManager
            mfCreateDXGIDeviceManager =
            reinterpret_cast<LPMFCreateDXGIDeviceManager> (mfCreateDXGIDeviceManagerFarProc);

        UINT resetToken;
        CComPtr<IMFDXGIDeviceManager> dxgiDeviceManager;
        hr = mfCreateDXGIDeviceManager(&resetToken, &dxgiDeviceManager);
        RETURNIFFAILED(hr);

        hr = dxgiDeviceManager->ResetDevice(d3dDevice, resetToken);
        RETURNIFFAILED(hr);

        CComPtr<ID3D10Multithread> d3dMultithread;
        hr = d3dContext->QueryInterface(&d3dMultithread);
        RETURNIFFAILED(hr);

        d3dMultithread->SetMultithreadProtected(TRUE);

        *deviceManagerPtr
            = reinterpret_cast<ULONG_PTR> (dxgiDeviceManager.Detach());

        return S_OK;
    }
    /**
    *****************************************************************************
    *  @fn     createDirect3DDeviceManagerPtr
    *  @brief  Creates device manager
    *
    *  @param[in] hWnd              : Handle to the window
    *  @param[out] deviceManagerPtr : Device manager pointer
    *
    *  @return HRESULT : S_OK if successful; otherwise returns microsoft error codes.
    *****************************************************************************
    */
    HRESULT createDirect3DDeviceManagerPtr(HWND hWnd,
        ULONG_PTR* deviceManagerPtr)
    {
        if (INVALID_HANDLE_VALUE == hWnd)
        {
            return E_INVALIDARG;
        }

        if (nullptr == deviceManagerPtr)
        {
            return E_POINTER;
        }

        HRESULT hr;

        CComPtr<IDirect3D9Ex> d3d9;
        hr = Direct3DCreate9Ex(D3D9b_SDK_VERSION, &d3d9);
        RETURNIFFAILED(hr);

        D3DPRESENT_PARAMETERS presentParameters;
        ZeroMemory(&presentParameters, sizeof(presentParameters));
        presentParameters.Windowed = TRUE;
        presentParameters.SwapEffect = D3DSWAPEFFECT_COPY;
        presentParameters.BackBufferWidth = 1;
        presentParameters.BackBufferHeight = 1;
        presentParameters.BackBufferFormat = D3DFMT_UNKNOWN;
        presentParameters.Flags = D3DPRESENTFLAG_VIDEO;
        presentParameters.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
        presentParameters.hDeviceWindow = hWnd;

        CComPtr<IDirect3DDevice9Ex> d3dDevice;
        hr = d3d9->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
            D3DCREATE_NOWINDOWCHANGES | D3DCREATE_MULTITHREADED
            | D3DCREATE_HARDWARE_VERTEXPROCESSING
            | D3DCREATE_FPU_PRESERVE,
            &presentParameters, NULL, &d3dDevice);
        RETURNIFFAILED(hr);

        UINT resetToken;
        CComPtr<IDirect3DDeviceManager9> d3dDeviceManager;
        hr = DXVA2CreateDirect3DDeviceManager9(&resetToken, &d3dDeviceManager);
        RETURNIFFAILED(hr);

        hr = d3dDeviceManager->ResetDevice(d3dDevice, resetToken);
        RETURNIFFAILED(hr);

        *deviceManagerPtr
            = reinterpret_cast<ULONG_PTR> (d3dDeviceManager.Detach());

        return S_OK;
    }
    /**
    *****************************************************************************
    *  @fn     createVideoType
    *  @brief  Creates IMFMediaType and sets the necessary parameters
    *
    *  @param[out] ppType          : IMFMedia type pointer
    *  @param[in] videoSybtype     : subtype of video to be set
    *  @param[in] fixedSize        : True if video data is of fixed size
    *  @param[in] samplesIndependent : True if samples are not dependent on each other
    *  @param[in] frameRate        : Video frame rate
    *  @param[in] pixelAspectRatio : Aspect Ration of the video
    *  @param[in] width            : Width of the video
    *  @param[in] height           : Height of the video
    *  @param[in] interlaceMode    : True if video is interlaced otherwise false
    *
    *  @return HRESULT : S_OK if successful; otherwise returns microsoft error codes.
    *****************************************************************************
    */
    HRESULT createVideoType(IMFMediaType** ppType, const GUID& videoSybtype,
        BOOL fixedSize, BOOL samplesIndependent,
        const MFRatio* frameRate, const MFRatio* pixelAspectRatio,
        uint32_t width, uint32_t height,
        MFVideoInterlaceMode interlaceMode)
    {
        if (ppType == NULL)
        {
            return E_POINTER;
        }

        HRESULT hr;

        CComPtr<IMFMediaType> type = NULL;
        hr = MFCreateMediaType(&type);
        RETURNIFFAILED(hr);

        hr = type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        RETURNIFFAILED(hr);

        hr = type->SetGUID(MF_MT_SUBTYPE, videoSybtype);
        RETURNIFFAILED(hr);

        hr = type->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, fixedSize);
        RETURNIFFAILED(hr);

        hr = type->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, samplesIndependent);
        RETURNIFFAILED(hr);

        hr = type->SetUINT32(MF_MT_INTERLACE_MODE, interlaceMode);
        RETURNIFFAILED(hr);

        if (frameRate != nullptr)
        {
            hr = MFSetAttributeRatio(type, MF_MT_FRAME_RATE,
                frameRate->Numerator, frameRate->Denominator);
            RETURNIFFAILED(hr);
        }

        if (pixelAspectRatio != nullptr)
        {
            hr = MFSetAttributeRatio(type, MF_MT_PIXEL_ASPECT_RATIO,
                pixelAspectRatio->Numerator,
                pixelAspectRatio->Denominator);
            RETURNIFFAILED(hr);
        }

        if (width != 0 && height != 0)
        {
            hr = MFSetAttributeSize(type, MF_MT_FRAME_SIZE, width, height);
            RETURNIFFAILED(hr);

            LONG stride = 0;
            hr = MFGetStrideForBitmapInfoHeader(videoSybtype.Data1, width,
                &stride);
            /******************************************************************
            * If video sybtype is not compressed it has stride                *
            ******************************************************************/
            if (SUCCEEDED(hr))
            {
                hr = type->SetUINT32(MF_MT_DEFAULT_STRIDE, uint32_t(stride));
                RETURNIFFAILED(hr);

                UINT sampleSize = 0;
                hr = MFCalculateImageSize(videoSybtype, width, height,
                    &sampleSize);
                RETURNIFFAILED(hr);

                hr = type->SetUINT32(MF_MT_SAMPLE_SIZE, sampleSize);
                RETURNIFFAILED(hr);
            }
        }

        *ppType = type.Detach();

        return S_OK;
    }
    /**
    *****************************************************************************
    *  @fn     createVideoTypeFromSource
    *  @brief  Creates IMFMediaType using the parameters from source media type
    *
    *  @param[in] sourceVideoType    : Source IMFMedia type pointer
    *  @param[in] videoSybtype       : subtype of video to be set
    *  @param[in] fixedSize          : True if video data is of fixed size
    *  @param[in] samplesIndependent : True if samples are not dependent on each other
    *  @param[out] videoType         : output IMFMedia type
    *
    *  @return HRESULT : S_OK if successful; otherwise returns microsoft error codes.
    *****************************************************************************
    */
    HRESULT createVideoTypeFromSource(IMFMediaType* sourceVideoType,
        const GUID& videoSybtype, BOOL fixedSize,
        BOOL samplesIndependent, IMFMediaType** videoType)
    {
        HRESULT hr;

        uint32_t frameWidth;
        uint32_t frameHeight;
        hr = MFGetAttributeRatio(sourceVideoType, MF_MT_FRAME_SIZE,
            &frameWidth, &frameHeight);
        RETURNIFFAILED(hr);

        uint32_t interlaceMode;
        hr = sourceVideoType->GetUINT32(MF_MT_INTERLACE_MODE, &interlaceMode);
        RETURNIFFAILED(hr);

        uint32_t aspectRatioNumerator;
        uint32_t aspectRatioDenominator;
        hr = MFGetAttributeRatio(sourceVideoType, MF_MT_PIXEL_ASPECT_RATIO,
            &aspectRatioNumerator, &aspectRatioDenominator);
        RETURNIFFAILED(hr);

        uint32_t frameRateNumerator;
        uint32_t frameRateDenominator;
        hr = MFGetAttributeRatio(sourceVideoType, MF_MT_FRAME_RATE,
            &frameRateNumerator, &frameRateDenominator);
        RETURNIFFAILED(hr);

        MFRatio frameRate = { frameRateNumerator, frameRateDenominator };
        MFRatio aspectRatio = { aspectRatioNumerator, aspectRatioDenominator };

        return createVideoType(videoType, videoSybtype, fixedSize,
            samplesIndependent, &frameRate, &aspectRatio,
            frameWidth, frameHeight,
            static_cast<MFVideoInterlaceMode> (interlaceMode));
    }

    /**
    *****************************************************************************
    *  @fn     findVideoEncoder
    *  @brief  Findes available Encoder for the input video subtype
    *
    *  @param[in]  inputVideoSubtype  : input IMFMedia type pointer
    *  @param[in]  useHwCodec         : use Hw or Sw codec
    *  @param[out] inputVideoSubtype  : input IMFMedia type pointer
    *
    *  @return HRESULT : S_OK if successful; otherwise returns microsoft error codes.
    *****************************************************************************
    */
    HRESULT findVideoEncoder(REFCLSID inputVideoSubtype,
        BOOL useHwCodec,
        IMFTransform **ppEncoder)
    {
        HRESULT hr;
        uint32_t i;

        MFT_REGISTER_TYPE_INFO inputRegisterTypeInfo = { MFMediaType_Video,
            inputVideoSubtype };
        MFT_REGISTER_TYPE_INFO outputRegisterTypeInfo = { MFMediaType_Video,
            MFVideoFormat_H264 };

        uint32_t flags = MFT_ENUM_FLAG_SORTANDFILTER;

        if (useHwCodec)
        {
            flags |= MFT_ENUM_FLAG_HARDWARE;
            flags |= MFT_ENUM_FLAG_ASYNCMFT;
        }
        else
        {
            flags |= MFT_ENUM_FLAG_SYNCMFT;
        }

        IMFActivate **ppActivate = NULL;
        uint32_t registeredEncoderNumber = 0;

        hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER,
            flags,
            &inputRegisterTypeInfo,
            &outputRegisterTypeInfo,
            &ppActivate,
            &registeredEncoderNumber);
        RETURNIFFAILED(hr);

        if (SUCCEEDED(hr) && (registeredEncoderNumber == 0))
        {
            hr = MF_E_TOPO_CODEC_NOT_FOUND;
        }

        bool foundEncoder = false;
        if (SUCCEEDED(hr))
        {
            CComPtr<IMFTransform> ppTempEncoder;
            for (i = 0; i < registeredEncoderNumber; i++)
            {
                ppTempEncoder = NULL;
                hr = ppActivate[i]->ActivateObject(IID_PPV_ARGS(&ppTempEncoder));
                if (hr == S_OK)
                {
                    //Check if AMD Encoder is available
                    CComPtr<IMFAttributes> tmp_transformAttributes;
                    WCHAR hwurl[100];

                    hr = ppTempEncoder->GetAttributes(&tmp_transformAttributes);
                    if (hr == S_OK) // If Attributes are implemented
                    {
                        hr = tmp_transformAttributes->GetString(MFT_ENUM_HARDWARE_URL_Attribute, hwurl, 100, NULL);
                        if (hr == S_OK)
                        {
                            if (wcscmp(hwurl, L"AMDh264Encoder") == 0) // If we are using AMD HW H264 Encoder
                            {
                                printf("Selected AMD HW Encoder to encode the stream\n");
                                LOG(mLogFile, "Selected AMD HW Encoder to encode the stream");
                                hr = ppActivate[i]->ActivateObject(IID_PPV_ARGS(ppEncoder));
                                foundEncoder = true;
                                break;
                            }
                        }
                    }
                }
            }

            //AMD Encoder not found.  Select the first available HW Encoder
            if (!foundEncoder)
            {
                hr = ppActivate[0]->ActivateObject(IID_PPV_ARGS(ppEncoder));
                if (hr != S_OK)
                {
                    printf("Encoder could not be found\n");
                    LOG(mLogFile, "Encoder could not be found");
                    for (i = 0; i < registeredEncoderNumber; i++)
                    {
                        ppActivate[i]->Release();
                    }
                    CoTaskMemFree(ppActivate);
                    return E_FAIL;
                }

                if (useHwCodec)
                {
                    printf("Selected Non-AMD HW Encoder to encode the stream\n");
                    LOG(mLogFile, "Selected Non-AMD HW Encoder to encode the stream");
                }
                else
                {
                    printf("Selected SW Encoder to encode the stream\n");
                    LOG(mLogFile, "Selected SW Encoder to encode the stream");
                }
                foundEncoder = true;
            }

            displayMFT(*ppActivate);
        }

        for (i = 0; i < registeredEncoderNumber; i++)
        {
            ppActivate[i]->Release();
        }
        CoTaskMemFree(ppActivate);

        if (!foundEncoder)
        {
            return E_FAIL;
        }

        return S_OK;
    }

    /**
    *****************************************************************************
    *  @fn     setEncoderValue
    *  @brief  Set 32bit encoder attribute
    *
    *  @param[in]  guid             : CLSID of the encoder parameter to be set
    *  @param[out] value            : Parameter value
    *  @param[in] encoderTransform  : Pointer to the encoder transform
    *
    *  @return HRESULT : S_OK if successful; otherwise returns microsoft error codes.
    *****************************************************************************
    */
    HRESULT setEncoderValue(const GUID* guid, uint32_t value, CComPtr<IMFTransform> encoderTransform)
    {
        HRESULT hr = E_FAIL;
        if (encoderTransform != nullptr)
        {
            CComPtr<ICodecAPI> codecAPI;
            codecAPI = encoderTransform;
            hr = codecAPI->IsSupported(guid);
            RETURNIFFAILED(hr);
            VARIANT var;
            var.vt = VT_UI4;
            var.uintVal = value;
            hr = codecAPI->SetValue(guid, &var);
        }
        return hr;
    }

    HRESULT getEncoderValue(const GUID* guid, uint32_t *value, CComPtr<IMFTransform> encoderTransform)
    {
        HRESULT hr = E_FAIL;
        if (encoderTransform != nullptr && value != nullptr)
        {
            CComPtr<ICodecAPI> codecAPI;
            codecAPI = encoderTransform;
            hr = codecAPI->IsSupported(guid);
            RETURNIFFAILED(hr);
            VARIANT var;
            var.vt = VT_UI4;
            var.uintVal = 0;
            hr = codecAPI->GetValue(guid, &var);
            *value = var.uintVal;
        }
        return hr;
    }

    HRESULT getEncoderValue(const GUID* guid, UINT64 *value, CComPtr<IMFTransform> encoderTransform)
    {
        HRESULT hr = E_FAIL;
        if (encoderTransform != nullptr && value != nullptr)
        {
            CComPtr<ICodecAPI> codecAPI;
            codecAPI = encoderTransform;
            hr = codecAPI->IsSupported(guid);
            RETURNIFFAILED(hr);
            VARIANT var;
            var.vt = VT_UI8;
            var.ullVal = 0;
            hr = codecAPI->GetValue(guid, &var);
            *value = var.ullVal;
        }
        return hr;
    }
    /**
    *****************************************************************************
    *  @fn     setEncoderValue
    *  @brief  Set BOOL encoder parameter
    *
    *  @param[in]  guid             : CLSID of the encoder parameter to be set
    *  @param[out] value            : Parameter value
    *  @param[in] encoderTransform  : Pointer to the encoder transform
    *
    *  @return HRESULT : S_OK if successful; otherwise returns microsoft error codes.
    *****************************************************************************
    */
    HRESULT setEncoderValue(const GUID* guid, BOOL value, CComPtr<IMFTransform> encoderTransform)
    {
        HRESULT hr = E_FAIL;
        if (encoderTransform != nullptr)
        {
            CComPtr<ICodecAPI> codecAPI;
            codecAPI = encoderTransform;

            VARIANT var;
            var.vt = VT_BOOL;
            var.boolVal = (value) ? TRUE : FALSE;;
            hr = codecAPI->SetValue(guid, &var);
        }
        return hr;
    }
    HRESULT checkRange(uint32_t min, uint32_t max, uint32_t param)
    {
        bool result;
        result = param >= min ? ((param <= max) ? true : false) : false;
        if (result == false)
            return E_FAIL;
        else
            return S_OK;
    }
    HRESULT checkRange(double min, double max, double param)
    {
        bool result;
        result = param >= min ? ((param <= max) ? true : false) : false;
        if (result == false)
            return E_FAIL;
        else
            return S_OK;
    }
    /**
    *****************************************************************************
    *  @fn     createVideoEncoderNode
    *  @brief  Create video encoder node
    *
    *  @param[in]  encodedVideoType  : Encoder video type
    *  @param[in] sourceVideoType    : source video type
    *  @param[out] encoderNode       : Pointer to the encoder transform created
    *  @param[in] deviceManagerPtr   : Pointer to the device manager
    *  @param[in] vidEncParams       : Video encoder parameters to be set
    *
    *  @return HRESULT : S_OK if successful; otherwise returns microsoft error codes.
    *****************************************************************************
    */
    HRESULT createVideoEncoderNode(IMFMediaType* encodedVideoType,
        IMFMediaType* sourceVideoType,
        IMFTransform **encoderNode, ULONG_PTR deviceManagerPtr,
        AMF_MFT_VIDEOENCODERPARAMS *vidEncParams,
        BOOL useSwCodec)
    {
        HRESULT hr;

        CComPtr<IMFTransform> encoderTransform;
        BOOL useHwCodec = useSwCodec ? FALSE : TRUE;
        hr = findVideoEncoder(MFVideoFormat_NV12, useHwCodec, &encoderTransform);
        /**********************************************************************
        * case 1. If app doesn't find HW encoder then app will fall back to  *
        *         SW encoder, still app doesn't find SW encoder then app     *
        *         will exit                                                  *
        * case 2. If app doesn't find SW encoder then app will fall back to  *
        *         HW encoder, still app doesn't find HW encoder then app     *
        *         will exit                                                  *
        **********************************************************************/
        if (hr != S_OK)
        {
            useHwCodec = !useHwCodec;
            if (useHwCodec)
            {
                printf("---------------------\nWARNING: SW encoder not found, Using HW encoder.!!!\n");
                LOG(mLogFile, "---------------------\nWARNING: SW encoder not found, Using HW encoder.!!!\n");
            }
            else
            {
                printf("---------------------\nWARNING: HW encoder not found, Using SW encoder.!!!\n");
                LOG(mLogFile, "---------------------\nWARNING: HW encoder not found, Using SW encoder.!!!\n");
            }

            hr = findVideoEncoder(MFVideoFormat_NV12, useHwCodec, &encoderTransform);
        }
        RETURNIFFAILED(hr);

        if (useHwCodec)
        {
            CComPtr<IMFAttributes> encoderAttributes;
            hr = encoderTransform->GetAttributes(&encoderAttributes);
            RETURNIFFAILED(hr);

            uint32_t transformAsync;
            hr = encoderAttributes->GetUINT32(MF_TRANSFORM_ASYNC, &transformAsync);
            if (SUCCEEDED(hr) && TRUE == transformAsync)
            {
                hr = encoderAttributes->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE);
                RETURNIFFAILED(hr);
            }

            if (deviceManagerPtr != NULL)
            {
                CComPtr<IUnknown> deviceManagerUnknown = reinterpret_cast<IUnknown*> (deviceManagerPtr);
                CComPtr<IUnknown> dxgiDeviceManager;
                const CLSID & d3dAwareAttribute = (S_OK == deviceManagerUnknown->QueryInterface(BORROWED_IID_IMFDXGIDeviceManager, (void**)(&dxgiDeviceManager)))
                    ? BORROWED_MF_SA_D3D11_AWARE : MF_SA_D3D_AWARE;

                uint32_t d3dAware;
                hr = encoderAttributes->GetUINT32(d3dAwareAttribute, &d3dAware);
                if (SUCCEEDED(hr) && d3dAware != 0)
                {
                    hr = encoderTransform->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, deviceManagerPtr);
                    RETURNIFFAILED(hr);
                }
            }

            //Set Encoder Properties
            hr = setEncoderValue(&CODECAPI_AVEncMPVGOPSize,
                (uint32_t)vidEncParams->gopSize, encoderTransform);
            if (hr != S_OK)
            {
                LOG(mLogFile, "Failed to set CODECAPI_AVEncMPVGOPSize @ %s %d \n", TEXT(__FILE__), __LINE__);
            }

            hr = checkRange(0, 1, (uint32_t)vidEncParams->lowLatencyMode);
            LOGIFFAILED(mLogFile, hr, "Out of range :encLowLatencyMode @ %s %d \n", TEXT(__FILE__), __LINE__);
            hr = setEncoderValue(&CODECAPI_AVEncCommonLowLatency,
                (BOOL)vidEncParams->lowLatencyMode, encoderTransform);
            if (hr != S_OK)
            {
                LOG(mLogFile, "Failed to set CODECAPI_AVEncCommonLowLatency @ %s %d \n", TEXT(__FILE__), __LINE__);
            }
            /**********************************************************************
            * eAVEncCommonRateControlMode_LowDelayVBR,                            *
            * eAVEncCommonRateControlMode_GlobalVBR and                           *
            * eAVEncCommonRateControlMode_GlobalLowDelayVBR are not supported     *
            **********************************************************************/
            hr = checkRange(0, 3, (uint32_t)vidEncParams->rateControlMethod);
            LOGIFFAILED(mLogFile, hr, "un-supported encRateControlMethod @ %s %d \n", TEXT(__FILE__), __LINE__);
            hr = setEncoderValue(&CODECAPI_AVEncCommonRateControlMode,
                (uint32_t)vidEncParams->rateControlMethod, encoderTransform);
            if (hr != S_OK)
            {
                LOG(mLogFile, "Failed to set CODECAPI_AVEncCommonRateControlMode @ %s %d \n", TEXT(__FILE__), __LINE__);
            }
            hr = checkRange(0, 100, (uint32_t)vidEncParams->commonQuality);
            LOGIFFAILED(mLogFile, hr, "un-supported encCommonQuality parameter @ %s %d \n", TEXT(__FILE__), __LINE__);
            hr = setEncoderValue(&CODECAPI_AVEncCommonQuality,
                (uint32_t)vidEncParams->commonQuality, encoderTransform);
            if (hr != S_OK)
            {
                LOG(mLogFile, "Failed to set CODECAPI_AVEncCommonQuality @ %s %d \n", TEXT(__FILE__), __LINE__);
            }
            /**********************************************************************
            * Not supported presently. Supports only CABAC                        *
            **********************************************************************/
            hr = checkRange(0, 1, (uint32_t)vidEncParams->enableCabac);
            LOGIFFAILED(mLogFile, hr, "un-supported enableCabac @ %s %d \n", TEXT(__FILE__), __LINE__);
            hr = setEncoderValue(&CODECAPI_AVEncH264CABACEnable,
                (BOOL)vidEncParams->enableCabac, encoderTransform);
            if (hr != S_OK)
            {
                LOG(mLogFile, "un-supported Cabac enable flag @ %s %d \n", TEXT(__FILE__), __LINE__);
            }

            hr = checkRange(0, 100, (uint32_t)vidEncParams->qualityVsSpeed);
            LOGIFFAILED(mLogFile, hr, "un-supported encQualityVsSpeed parameter @ %s %d \n", TEXT(__FILE__), __LINE__);
            hr = setEncoderValue(&CODECAPI_AVEncCommonQualityVsSpeed,
                (uint32_t)vidEncParams->qualityVsSpeed, encoderTransform);
            if (hr != S_OK)
            {
                LOG(mLogFile, "Failed to set CODECAPI_AVEncCommonQualityVsSpeed @ %s %d \n", TEXT(__FILE__), __LINE__);
            }

            hr = setEncoderValue(&CODECAPI_AVEncCommonMaxBitRate,
                (uint32_t)vidEncParams->maxBitrate, encoderTransform);
            if (hr != S_OK)
            {
                LOG(mLogFile, "Failed to set CODECAPI_AVEncCommonMaxBitRate @ %s %d \n", TEXT(__FILE__), __LINE__);
            }

            hr = setEncoderValue(&CODECAPI_AVEncCommonBufferSize,
                (uint32_t)vidEncParams->bufSize, encoderTransform);
            if (hr != S_OK)
            {
                LOG(mLogFile, "Failed to set CODECAPI_AVEncCommonBufferSize @ %s %d \n", TEXT(__FILE__), __LINE__);
            }
        }

        hr = encoderTransform->SetOutputType(0, encodedVideoType, 0);
        LOGIFFAILED(mLogFile, hr, "Failed to set output type to encoder @ %s %d \n", TEXT(__FILE__), __LINE__);

        /*CComPtr<IMFMediaType> inputMediaType;
        hr = createVideoTypeFromSource(sourceVideoType, MFVideoFormat_NV12,
            TRUE, TRUE, &inputMediaType);
        LOGIFFAILED(mLogFile, hr, "Failed to create media type @ %s %d \n", TEXT(__FILE__), __LINE__);*/

        hr = encoderTransform->SetInputType(0, sourceVideoType, 0);
        LOGIFFAILED(mLogFile, hr, "Failed to set input type for the encoder @ %s %d \n", TEXT(__FILE__), __LINE__);

        uint32_t h264Profile;
        hr = encodedVideoType->GetUINT32(MF_MT_MPEG2_PROFILE, &h264Profile);
        LOGIFFAILED(mLogFile, hr, "Failed to set encoder profile @ %s %d \n", TEXT(__FILE__), __LINE__);

        /**********************************************************************
        * Set SVC temporal layer encoding                                     *
        **********************************************************************/
        if (eAVEncH264VProfile_UCConstrainedHigh == h264Profile)
        {
            const UINT temporalLayersCount = 3;

            CComPtr<ICodecAPI> codecApi;
            hr = encoderTransform->QueryInterface(&codecApi);
            LOGIFFAILED(mLogFile, hr, "Failed to query ICodecAPI interface @ %s %d \n", TEXT(__FILE__), __LINE__);

            VARIANT temporalLayerCount = { 0 };
            temporalLayerCount.vt = VT_UI4;
            temporalLayerCount.uintVal = temporalLayersCount;

            hr = codecApi->SetValue(&CODECAPI_AVEncVideoTemporalLayerCount,
                &temporalLayerCount);
            LOG(mLogFile, "Failed to set CODECAPI_AVEncVideoTemporalLayerCount @ %s %d \n", TEXT(__FILE__), __LINE__);
        }

        //Not using topology
        /*CComPtr<IMFTopologyNode> node;
        hr = MFCreateTopologyNode(MF_TOPOLOGY_TRANSFORM_NODE, &node);
        LOGIFFAILED(mLogFile, hr, "Failed to create node @ %s %d \n", TEXT(__FILE__), __LINE__);

        hr = node->SetObject(encoderTransform);
        LOGIFFAILED(mLogFile, hr, "Failed to add encoder node  @ %s %d \n", TEXT(__FILE__), __LINE__);

        *encoderNode = node.Detach();*/
        *encoderNode = encoderTransform.Detach();

        return S_OK;
    }

    /**
    *****************************************************************************
    *  @fn     getNV12ImageSize
    *  @brief  Create D3D11 texture 2d
    *
    *  @param[in] width       : width
    *  @param[in] height      : Height
    *  @param[out] pcbImage   : NV12 size
    *
    *  @return HRESULT : S_OK if successful; otherwise returns microsoft error codes.
    *****************************************************************************
    */
    HRESULT getNV12ImageSize(uint32_t width, uint32_t height, DWORD* pcbImage)
    {
        if ((height / 2 > MAXDWORD - height) || ((height + height / 2)
                        > MAXDWORD / width))
        {
            return E_INVALIDARG;
        }
        /**********************************************************************
        *  12 bpp                                                             *
        **********************************************************************/
        *pcbImage = width * (height + (height / 2));

        return S_OK;
    }

    HRESULT getRGBImageSize(uint32_t width, uint32_t height, DWORD* pcbImage)
    {
        if ((height * 4) > (MAXDWORD / width))
        {
            return E_INVALIDARG;
        }
        /**********************************************************************
        *  32 bpp                                                             *
        **********************************************************************/
        *pcbImage = width * height * 4;

        return S_OK;
    }

    HRESULT displayMFT(IMFActivate *pMFActivate)
    {
        HRESULT hr;

        /**********************************************************************
        * get the CLSID GUID from the IMFAttributes of the activation object *
        **********************************************************************/
        GUID guidMFT = { 0 };
        hr = pMFActivate->GetGUID(MFT_TRANSFORM_CLSID_Attribute, &guidMFT);
        if (MF_E_ATTRIBUTENOTFOUND == hr)
        {
            LOG(mLogFile, "IMFTransform has no CLSID.");
            return hr;
        }
        LOGIFFAILED(mLogFile, hr, "IMFAttributes::GetGUID(MFT_TRANSFORM_CLSID_Attribute) failed. ");

        LPWSTR szGuid = NULL;
        hr = StringFromIID(guidMFT, &szGuid);
        LOGIFFAILED(mLogFile, hr, "StringFromIID failed.");

        /**********************************************************************
        * get the friendly name string from the IMFAttributes of the         *
        * activation object                                                  *
        **********************************************************************/
        LPWSTR szFriendlyName = NULL;
#pragma prefast(suppress: __WARNING_PASSING_FUNCTION_UNEXPECTED_NULL, "IMFAttributes::GetAllocatedString third argument is optional");
        hr = pMFActivate->GetAllocatedString(
            MFT_FRIENDLY_NAME_Attribute,
            &szFriendlyName,
            NULL // don't care about the length of the string, and MSDN agrees, but SAL annotation is missing "opt"
            );

        if (MF_E_ATTRIBUTENOTFOUND == hr)
        {
            LOG(mLogFile, "IMFTransform has no friendly name.");
            return hr;
        }
        LOGIFFAILED(mLogFile, hr, "IMFAttributes::GetAllocatedString(MFT_FRIENDLY_NAME_Attribute) failed.");

        wprintf(L"MFT Friendly Name: %s\nMFT CLSID: %s\n---------------------\n", szFriendlyName, szGuid);
        if (mLogFile != NULL)
        {
            fwprintf(mLogFile, L"MFT Friendly Name: %s\nMFT CLSID: %s\n---------------------\n", szFriendlyName, szGuid);
            fflush(mLogFile);
        }

        return S_OK;
    }

};

#endif //_MFTUTILS_H
