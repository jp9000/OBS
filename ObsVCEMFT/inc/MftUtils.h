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
#include <amf\AmfMft.h>

#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>

#include <d3d9.h>
#include <d3d9types.h>
#include <dxva2api.h>
#include "PrintLog.h"
#include "VqMft.h"

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
    *  @fn     createSource
    *  @brief  Creates the source filter
    *
    *  @param[in] sourceFileName    : Input video file for transcoding
    *  @param[out] mediaSource       : pointer to the source filter
    *
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
    *****************************************************************************
    */
    HRESULT createSource(LPCWSTR sourceFileName, IMFMediaSource** mediaSource)
    {
        HRESULT hr;

        CComPtr<IMFSourceResolver> sourceResolver;

        hr = MFCreateSourceResolver(&sourceResolver);
        RETURNIFFAILED(hr);

        MF_OBJECT_TYPE objectType;
        CComPtr<IUnknown> sourceObject;

        hr
            = sourceResolver->CreateObjectFromURL(
            sourceFileName,
            MF_RESOLUTION_MEDIASOURCE
            | MF_RESOLUTION_CONTENT_DOES_NOT_HAVE_TO_MATCH_EXTENSION_OR_MIME_TYPE,
            nullptr, &objectType, &sourceObject);
        RETURNIFFAILED(hr);

        hr = sourceObject->QueryInterface(IID_PPV_ARGS(mediaSource));
        RETURNIFFAILED(hr);

        return S_OK;
    }

    /**
    *****************************************************************************
    *  @fn     createSinkActivate
    *  @brief  Creates sink filter
    *
    *  @param[in] sourcePresentationDescriptor : source descriptor
    *  @param[in] sinkFileName                 : output file name
    *  @param[in] videoType                    : output video type
    *  @param[in/out] mediaSinkActivate        : pointer to the sink filter
    *
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
    *****************************************************************************
    */
    HRESULT createSinkActivate(IMFPresentationDescriptor* sourcePresentationDescriptor,
        LPCWSTR sinkFileName, IMFMediaType* videoType,
        IMFActivate** mediaSinkActivate)
    {
        if (nullptr == sourcePresentationDescriptor ||
            nullptr == videoType ||
            nullptr == mediaSinkActivate)
        {
            return E_POINTER;
        }

        HRESULT hr;

        CComPtr<IMFASFProfile> sinkAsfProfile;
        hr = MFCreateASFProfile(&sinkAsfProfile);
        RETURNIFFAILED(hr);

        DWORD descriptorCount;
        hr = sourcePresentationDescriptor->GetStreamDescriptorCount(&descriptorCount);
        RETURNIFFAILED(hr);

        for (WORD i = 0; i < descriptorCount; i++)
        {
            CComPtr<IMFStreamDescriptor> sourceStreamDescriptor;
            BOOL selected;

            hr = sourcePresentationDescriptor->GetStreamDescriptorByIndex(i, &selected, &sourceStreamDescriptor);
            RETURNIFFAILED(hr);

            CComPtr<IMFMediaTypeHandler> sourceMediaTypeHandler;
            hr = sourceStreamDescriptor->GetMediaTypeHandler(&sourceMediaTypeHandler);
            RETURNIFFAILED(hr);

            CComPtr<IMFMediaType> sourceMediaType;
            hr = sourceMediaTypeHandler->GetCurrentMediaType(&sourceMediaType);
            RETURNIFFAILED(hr);

            GUID majorType;
            hr = sourceMediaType->GetMajorType(&majorType);
            RETURNIFFAILED(hr);

            CComPtr<IMFMediaType> sinkMediaType;
            hr = MFCreateMediaType(&sinkMediaType);
            RETURNIFFAILED(hr);

            /******************************************************************/
            /* If it is video then use give videoType.                        */
            /******************************************************************/
            if (IsEqualGUID(majorType, MFMediaType_Video))
            {
                hr = videoType->CopyAllItems(sinkMediaType);
                RETURNIFFAILED(hr);

                /******************************************************************/
                /*Check if there is interlace mode attribute and it is progressive*/
                /******************************************************************/
                UINT32 interlaceMode;
                hr = sinkMediaType->GetUINT32(MF_MT_INTERLACE_MODE, &interlaceMode);
                RETURNIFFAILED(hr);

                /*********************************************************/
                /* Add frame size and bit rate from source if they are   */
                /* not set                                               */
                /*********************************************************/
                UINT32 imageWidthInPixels;
                UINT32 imageHeightInPixels;
                hr = MFGetAttributeSize(sinkMediaType, MF_MT_FRAME_SIZE, &imageWidthInPixels, &imageHeightInPixels);
                if ((FAILED(hr)) ||
                    (imageWidthInPixels == 0) ||
                    (imageHeightInPixels == 0))
                {
                    hr = MFGetAttributeSize(sourceMediaType, MF_MT_FRAME_SIZE, &imageWidthInPixels, &imageHeightInPixels);
                    if (SUCCEEDED(hr))
                    {
                        hr = MFSetAttributeSize(sinkMediaType, MF_MT_FRAME_SIZE, imageWidthInPixels, imageHeightInPixels);
                        RETURNIFFAILED(hr);
                    }
                }

                UINT32 avBitrate;
                hr = sinkMediaType->GetUINT32(MF_MT_AVG_BITRATE, &avBitrate);
                if (FAILED(hr) ||
                    avBitrate == 0)
                {
                    hr = sourceMediaType->GetUINT32(MF_MT_AVG_BITRATE, &avBitrate);
                    if (SUCCEEDED(hr))
                    {
                        hr = sinkMediaType->SetUINT32(MF_MT_AVG_BITRATE, avBitrate);
                        RETURNIFFAILED(hr);
                    }
                }



                CComPtr<IMFASFStreamConfig> sinkAsfStreamConfig;
                hr = sinkAsfProfile->CreateStream(sinkMediaType, &sinkAsfStreamConfig);
                RETURNIFFAILED(hr);

                /***************************************************************************/
                /* ASF stream number must be in 1-127                                      */
                /***************************************************************************/
                hr = sinkAsfStreamConfig->SetStreamNumber(i + 1);
                RETURNIFFAILED(hr);

                hr = sinkAsfProfile->SetStream(sinkAsfStreamConfig);
                RETURNIFFAILED(hr);
            }
        }

        CComPtr<IMFASFContentInfo> asfContentInfo;
        hr = MFCreateASFContentInfo(&asfContentInfo);
        RETURNIFFAILED(hr);

        hr = asfContentInfo->SetProfile(sinkAsfProfile);
        RETURNIFFAILED(hr);

        CComPtr<IPropertyStore> propertyStore;
        hr = asfContentInfo->GetEncodingConfigurationPropertyStore(0, &propertyStore);
        RETURNIFFAILED(hr);

        PROPVARIANT var;
        var.vt = VT_BOOL;
        var.boolVal = VARIANT_TRUE;

        hr = propertyStore->SetValue(MFPKEY_ASFMEDIASINK_AUTOADJUST_BITRATE, var);
        RETURNIFFAILED(hr);

        hr = MFCreateASFMediaSinkActivate(sinkFileName, asfContentInfo,
            mediaSinkActivate);
        return S_OK;
    }

    /**
    *****************************************************************************
    *  @fn     createStreamSourceNode
    *  @brief  Creates source node
    *
    *  @param[in] mediaSource              : Media source
    *  @param[in] presentationDescriptor   : presentation descriptor for the source
    *  @param[in] streamDescriptor         : Stream descriptor
    *  @param[in/out] sourceStreamNode     : pointer to the source node
    *
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
    *****************************************************************************
    */
    HRESULT createStreamSourceNode(IMFMediaSource *mediaSource,
        IMFPresentationDescriptor *presentationDescriptor,
        IMFStreamDescriptor *streamDescriptor,
        IMFTopologyNode **sourceStreamNode)
    {
        HRESULT hr;

        CComPtr<IMFTopologyNode> node;
        hr = MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &node);
        RETURNIFFAILED(hr);

        hr = node->SetUnknown(MF_TOPONODE_SOURCE, mediaSource);
        RETURNIFFAILED(hr);

        hr = node->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR,
            presentationDescriptor);
        RETURNIFFAILED(hr);

        hr = node->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, streamDescriptor);
        RETURNIFFAILED(hr);

        hr = node->SetUINT32(MF_TOPONODE_CONNECT_METHOD,
            MF_CONNECT_ALLOW_DECODER);
        RETURNIFFAILED(hr);

        *sourceStreamNode = node.Detach();

        return S_OK;
    }
    /**
    *****************************************************************************
    *  @fn     createStreamSinkNode
    *  @brief  Creates sink node
    *
    *  @param[in] mediaSinkObject          : Media sink object
    *  @param[in] streamNumber             : presentation descriptor for the source
    *  @param[in/out] streamSinkNode       : pointer to the sink node
    *
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
    *****************************************************************************
    */
    HRESULT createStreamSinkNode(IUnknown* mediaSinkObject, DWORD streamNumber,
        IMFTopologyNode** streamSinkNode)
    {
        HRESULT hr;

        CComPtr<IMFTopologyNode> node;
        hr = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &node);
        RETURNIFFAILED(hr);

        hr = node->SetObject(mediaSinkObject);
        RETURNIFFAILED(hr);

        hr = node->SetUINT32(MF_TOPONODE_STREAMID, streamNumber);
        RETURNIFFAILED(hr);

        hr = node->SetUINT32(MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, FALSE);
        RETURNIFFAILED(hr);

        *streamSinkNode = node.Detach();

        return S_OK;
    }
    /**
    *****************************************************************************
    *  @fn     bindNode
    *  @brief  binds the given node
    *
    *  @param[in/out] node          : pointer to the node
    *
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
    *****************************************************************************
    */
    HRESULT bindNode(IMFTopologyNode *node)
    {
        HRESULT hr;

        CComPtr<IUnknown> nodeObject;
        hr = node->GetObject(&nodeObject);
        RETURNIFFAILED(hr);

        CComPtr<IMFActivate> activate;
        hr = nodeObject->QueryInterface(&activate);
        if (FAILED(hr))
        {
            CComPtr<IMFStreamSink> stream;
            return nodeObject->QueryInterface(IID_PPV_ARGS(&stream));
        }

        CComPtr<IMFMediaSink> sink;
        hr = activate->ActivateObject(IID_PPV_ARGS(&sink));
        RETURNIFFAILED(hr);

        DWORD dwStreamID = MFGetAttributeUINT32(node, MF_TOPONODE_STREAMID, 0);

        DWORD streamSinkCount;
        hr = sink->GetStreamSinkCount(&streamSinkCount);
        RETURNIFFAILED(hr);

        CComPtr<IMFStreamSink> stream;
        hr = sink->GetStreamSinkById(dwStreamID, &stream);
        if (FAILED(hr))
        {
            hr = sink->AddStreamSink(dwStreamID, nullptr, &stream);
        }

        if (SUCCEEDED(hr))
        {
            hr = node->SetObject(stream);
        }

        return hr;
    }

    /**
    *****************************************************************************
    *  @fn     bindOutputNodes
    *  @brief  binds output nodes in the topology
    *
    *  @param[in/out] topology          : pointer to the topology
    *
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
    *****************************************************************************
    */
    HRESULT bindOutputNodes(IMFTopology* topology)
    {
        HRESULT hr;

        CComPtr<IMFCollection> nodeCollection;
        hr = topology->GetOutputNodeCollection(&nodeCollection);
        RETURNIFFAILED(hr);

        DWORD nodeCount = 0;
        hr = nodeCollection->GetElementCount(&nodeCount);
        RETURNIFFAILED(hr);

        for (DWORD i = 0; i < nodeCount; i++)
        {
            CComPtr<IUnknown> nodeUnknown;
            hr = nodeCollection->GetElement(i, &nodeUnknown);
            RETURNIFFAILED(hr);

            CComPtr<IMFTopologyNode> node;
            hr = nodeUnknown->QueryInterface(&node);
            RETURNIFFAILED(hr);

            hr = bindNode(node);
            RETURNIFFAILED(hr);
        }

        return S_OK;
    }
    /**
    *****************************************************************************
    *  @fn     setHardwareAcceleration
    *  @brief  set acceleration mode to the topology
    *
    *  @param[in] enable          : true - enable hardware acceleratoin
    *  @param[in/out] topology    : pointer to the topology
    *
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
    *****************************************************************************
    */
    HRESULT setHardwareAcceleration(IMFTopology* topology, bool enable)
    {
        UINT32 topologyMode = enable ? MFTOPOLOGY_HWMODE_USE_HARDWARE
            : MFTOPOLOGY_HWMODE_SOFTWARE_ONLY;
        UINT32 dxvaMode = enable ? MFTOPOLOGY_DXVA_FULL : MFTOPOLOGY_DXVA_NONE;

        HRESULT hr;

        hr = topology->SetUINT32(MF_TOPOLOGY_HARDWARE_MODE, topologyMode);
        RETURNIFFAILED(hr);

        hr = topology->SetUINT32(MF_TOPOLOGY_DXVA_MODE, dxvaMode);
        RETURNIFFAILED(hr);

        return S_OK;
    }
    /**
    *****************************************************************************
    *  @fn     createTransform
    *  @brief  Creates the transform with given class id
    *
    *  @param[in] clsid             : Class id of the transform
    *  @param[in/out] transform     : pointer to the transform created
    *  @param[in] deviceManagerPtr  : dx9 or dx11 device manager
    *
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
    *****************************************************************************
    */
    HRESULT createTransform(const CLSID &clsid, IMFTransform** transform,
        ULONG_PTR deviceManagerPtr)
    {
        HRESULT hr;

        CComPtr<IUnknown> transformUnknown;
        hr = CoCreateInstance(clsid, nullptr, CLSCTX_INPROC, IID_IMFTransform,
            (void**)&transformUnknown);
        RETURNIFFAILED(hr);

        CComPtr<IMFTransform> mft;
        hr = transformUnknown->QueryInterface(&mft);
        RETURNIFFAILED(hr);

        CComPtr<IMFAttributes> transformAttributes;
        hr = mft->GetAttributes(&transformAttributes);
        if (hr != E_NOTIMPL)
        {
            RETURNIFFAILED(hr);

            UINT32 transformAsync;
            hr = transformAttributes->GetUINT32(MF_TRANSFORM_ASYNC,
                &transformAsync);
            if (SUCCEEDED(hr) && TRUE == transformAsync)
            {
                hr = transformAttributes->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK,
                    TRUE);
                RETURNIFFAILED(hr);
            }

            if (deviceManagerPtr != NULL)
            {
                CComPtr<IUnknown> deviceManagerUnknown
                    = reinterpret_cast<IUnknown*> (deviceManagerPtr);

                CComPtr<IUnknown> dxgiDeviceManager;

                const CLSID
                    & d3dAwareAttribute =
                    S_OK
                    == deviceManagerUnknown->QueryInterface(
                    BORROWED_IID_IMFDXGIDeviceManager,
                    (void**)(&dxgiDeviceManager)) ? BORROWED_MF_SA_D3D11_AWARE
                    : MF_SA_D3D_AWARE;

                UINT32 d3dAware;
                hr = transformAttributes->GetUINT32(d3dAwareAttribute,
                    &d3dAware);
                if (SUCCEEDED(hr) && d3dAware != 0)
                {
                    hr = mft->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER,
                        deviceManagerPtr);
                    RETURNIFFAILED(hr);
                }
            }
        }

        *transform = mft.Detach();

        return S_OK;
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
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
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

        CComPtr<ID3D10Multithread> d3dMulthiread;
        hr = d3dContext->QueryInterface(&d3dMulthiread);
        RETURNIFFAILED(hr);

        d3dMulthiread->SetMultithreadProtected(TRUE);

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
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
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
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
    *****************************************************************************
    */
    HRESULT createVideoType(IMFMediaType** ppType, const GUID& videoSybtype,
        BOOL fixedSize, BOOL samplesIndependent,
        const MFRatio* frameRate, const MFRatio* pixelAspectRatio,
        UINT32 width, UINT32 height,
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
                hr = type->SetUINT32(MF_MT_DEFAULT_STRIDE, UINT32(stride));
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
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
    *****************************************************************************
    */
    HRESULT createVideoTypeFromSource(IMFMediaType* sourceVideoType,
        const GUID& videoSybtype, BOOL fixedSize,
        BOOL samplesIndependent, IMFMediaType** videoType)
    {
        HRESULT hr;

        UINT32 frameWidth;
        UINT32 frameHeight;
        hr = MFGetAttributeRatio(sourceVideoType, MF_MT_FRAME_SIZE,
            &frameWidth, &frameHeight);
        RETURNIFFAILED(hr);

        UINT32 interlaceMode;
        hr = sourceVideoType->GetUINT32(MF_MT_INTERLACE_MODE, &interlaceMode);
        RETURNIFFAILED(hr);

        UINT32 aspectRatioNumerator;
        UINT32 aspectRatioDenominator;
        hr = MFGetAttributeRatio(sourceVideoType, MF_MT_PIXEL_ASPECT_RATIO,
            &aspectRatioNumerator, &aspectRatioDenominator);
        RETURNIFFAILED(hr);

        UINT32 frameRateNumerator;
        UINT32 frameRateDenominator;
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
    *  @fn     createVideoDecoderNode
    *  @brief  Creates Video decoder node
    *
    *  @param[in] sourceMediaType  : Source IMFMedia type pointer
    *  @param[out] decoderNode     : Decoder node created
    *  @param[in] deviceManagerPtr : Device manager pointer
    *  @param[out] transform       : Pointer to the decoder transform
    *
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
    *****************************************************************************
    */
    HRESULT createVideoDecoderNode(IMFMediaType* sourceMediaType,
        IMFTopologyNode** decoderNode, ULONG_PTR deviceManagerPtr,
        IMFTransform** transform, BOOL useSwCodec)
    {
        HRESULT hr;

        GUID subtype;
        hr = sourceMediaType->GetGUID(MF_MT_SUBTYPE, &subtype);
        RETURNIFFAILED(hr);

        CComPtr<IMFTransform> decoderTransform;
        BOOL useHwCodec = useSwCodec ? FALSE : TRUE;

        UINT32 inputImageWidth = 0;
        UINT32 inputImageHeight = 0;
        hr = MFGetAttributeSize(sourceMediaType, MF_MT_FRAME_SIZE,
            &inputImageWidth, &inputImageHeight);
        RETURNIFFAILED(hr);

        hr = findVideoDecoder(subtype, &useHwCodec, inputImageWidth, inputImageHeight, &decoderTransform);
        /**********************************************************************
        * case 1. If app doesn't find HW decoder then app will fall back to  *
        *         SW decoder, still app doesn't find SW decoder then app     *
        *         will exit                                                  *
        * case 2. If app doesn't find SW decoder then app will fall back to  *
        *         HW decoder, still app doesn't find HW decoder then app     *
        *         will exit                                                  *
        **********************************************************************/
        if (hr != S_OK)
        {
            useHwCodec = !useHwCodec;
            if (useHwCodec)
            {
                printf("---------------------\nWARNING: SW decoder not found, Using HW decoder.!!!\n");
                LOG(mLogFile, "---------------------\nWARNING: SW decoder not found, Using HW decoder.!!!\n");
            }
            else
            {
                printf("---------------------\nWARNING: HW decoder not found, Using SW decoder.!!!\n");
                LOG(mLogFile, "---------------------\nWARNING: HW decoder not found, Using SW decoder.!!!\n");
            }
            hr = findVideoDecoder(subtype, &useHwCodec, inputImageWidth, inputImageHeight, &decoderTransform);
        }
        RETURNIFFAILED(hr);

        if (useHwCodec)
        {
            CComPtr<IMFAttributes> decoderAttributes;
            hr = decoderTransform->GetAttributes(&decoderAttributes);
            RETURNIFFAILED(hr);

            UINT32 transformAsync;
            hr = decoderAttributes->GetUINT32(MF_TRANSFORM_ASYNC, &transformAsync);
            if (SUCCEEDED(hr) && TRUE == transformAsync)
            {
                hr = decoderAttributes->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE);
                RETURNIFFAILED(hr);
            }

            if (deviceManagerPtr != NULL)
            {
                CComPtr<IUnknown> deviceManagerUnknown = reinterpret_cast<IUnknown*> (deviceManagerPtr);
                CComPtr<IUnknown> dxgiDeviceManager;
                const CLSID & d3dAwareAttribute = (S_OK == deviceManagerUnknown->QueryInterface(BORROWED_IID_IMFDXGIDeviceManager, (void**)(&dxgiDeviceManager)))
                    ? BORROWED_MF_SA_D3D11_AWARE : MF_SA_D3D_AWARE;

                UINT32 d3dAware;
                hr = decoderAttributes->GetUINT32(d3dAwareAttribute, &d3dAware);
                if (SUCCEEDED(hr) && d3dAware != 0)
                {
                    hr = decoderTransform->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, deviceManagerPtr);
                    RETURNIFFAILED(hr);
                }
            }
        }

        hr = decoderTransform->SetInputType(0, sourceMediaType, 0);
        RETURNIFFAILED(hr);

        CComPtr<IMFTopologyNode> node;
        hr = MFCreateTopologyNode(MF_TOPOLOGY_TRANSFORM_NODE, &node);
        RETURNIFFAILED(hr);

        hr = node->SetObject(decoderTransform);
        RETURNIFFAILED(hr);

        hr = node->SetUINT32(MF_TOPONODE_CONNECT_METHOD,
            MF_CONNECT_ALLOW_CONVERTER);
        RETURNIFFAILED(hr);

        *decoderNode = node.Detach();
        if (transform != NULL)
        {
            *transform = decoderTransform.Detach();
        }
        return S_OK;
    }

    /**
    *****************************************************************************
    *  @fn     findVideoDecoderClsid
    *  @brief  Findes available dcoder's CLSId for the input video subtype
    *
    *  @param[in]  inputVideoSubtype  : input IMFMedia type pointer
    *  @param[out] decoderClsid       : Decoder clsid found
    *
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
    *****************************************************************************
    */
    HRESULT findVideoDecoderClsid(REFCLSID inputVideoSubtype,
        CLSID* decoderClsid)
    {
        HRESULT hr;

        MFT_REGISTER_TYPE_INFO inputRegisterTypeInfo = { MFMediaType_Video,
            inputVideoSubtype };
        const CLSID supportedOutputSubtypes[] = { MFVideoFormat_NV12,
            MFVideoFormat_YUY2 };
        bool foundDecoder = false;
        for (int i = 0; !foundDecoder && i < ARRAYSIZE(supportedOutputSubtypes); i++)
        {
            MFT_REGISTER_TYPE_INFO outputRegisterTypeInfo =
            { MFMediaType_Video, supportedOutputSubtypes[i] };

            CLSID* registeredDecodersClsids;
            UINT32 registeredDecodersNumber;
            hr = MFTEnum(MFT_CATEGORY_VIDEO_DECODER, 0, &inputRegisterTypeInfo,
                &outputRegisterTypeInfo, nullptr,
                &registeredDecodersClsids,
                &registeredDecodersNumber);
            RETURNIFFAILED(hr);

            if (registeredDecodersNumber != 0)
            {
                *decoderClsid = registeredDecodersClsids[0];
                foundDecoder = true;
            }

            CoTaskMemFree(registeredDecodersClsids);
        }
        if (!foundDecoder)
        {
            TRACE_MSG("There is no registered decoder for given video subtype.", 0);
            return E_FAIL;
        }
        return S_OK;
    }

    /**
    *****************************************************************************
    *  @fn     findVideoDecoder
    *  @brief  Findes available decoder for the input video subtype
    *
    *  @param[in]  inputVideoSubtype  : input IMFMedia type pointer
    *  @param[in]  useHwCodec         : use Hw or Sw codec
    *  @param[out] inputVideoSubtype  : input IMFMedia type pointer
    *
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
    *****************************************************************************
    */
    HRESULT findVideoDecoder(REFCLSID inputVideoSubtype,
        BOOL *useHwCodec,
        UINT32 inWidth,
        UINT32 inHeight,
        IMFTransform **ppDecoder)
    {
        HRESULT hr;
        UINT32 i, j;
        UINT32 flags = MFT_ENUM_FLAG_SORTANDFILTER;
        bool isHwCodecBeingUsed = false;

        MFT_REGISTER_TYPE_INFO inputRegisterTypeInfo = { MFMediaType_Video,
            inputVideoSubtype };

        if (IsEqualGUID(MFVideoFormat_MP4V, inputVideoSubtype) ||
            IsEqualGUID(MFVideoFormat_M4S2, inputVideoSubtype) ||
            IsEqualGUID(MFVideoFormat_XVID, inputVideoSubtype))
        {
            /******************************************************************
            * Currently AMD D3D11 HW Playback decoder suports only MPEG4      *
            * part 2 ASP with frame size more than 1280x720                   *
            ******************************************************************/
            if (inWidth >= 1280 && inHeight >= 720 && *useHwCodec)
            {
                flags |= MFT_ENUM_FLAG_HARDWARE;
                flags |= MFT_ENUM_FLAG_ASYNCMFT;
                isHwCodecBeingUsed = true;
            }
            else
            {
                flags |= MFT_ENUM_FLAG_SYNCMFT;
                //Setting HwCodec flag to false since we use Sw MPEG4 Decoder
                isHwCodecBeingUsed = false;
            }
        }
        else if (IsEqualGUID(MFVideoFormat_WMV3, inputVideoSubtype) ||
            IsEqualGUID(MFVideoFormat_WVC1, inputVideoSubtype))
        {
            // For this input we will use MS Decoder
            flags |= MFT_ENUM_FLAG_SYNCMFT;
        }
        else
        {
            flags |= MFT_ENUM_FLAG_SYNCMFT;

            if (*useHwCodec)
            {
                flags |= MFT_ENUM_FLAG_HARDWARE;
                isHwCodecBeingUsed = true;
            }
        }

        IMFActivate **ppActivate = NULL;
        UINT32 registeredDecodersNumber = 0;

        const CLSID supportedOutputSubtypes[] = { MFVideoFormat_NV12,
            MFVideoFormat_YUY2 };
        bool foundDecoder = false;
        for (i = 0; !foundDecoder && i < ARRAYSIZE(supportedOutputSubtypes); i++)
        {
            MFT_REGISTER_TYPE_INFO outputRegisterTypeInfo = { MFMediaType_Video, supportedOutputSubtypes[i] };

            hr = MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER,
                flags,
                &inputRegisterTypeInfo,
                &outputRegisterTypeInfo,
                &ppActivate,
                &registeredDecodersNumber);
            RETURNIFFAILED(hr);

            if (SUCCEEDED(hr) && (registeredDecodersNumber == 0))
            {
                hr = MF_E_TOPO_CODEC_NOT_FOUND;
            }

            if (SUCCEEDED(hr))
            {
                hr = ppActivate[0]->ActivateObject(IID_PPV_ARGS(ppDecoder));
                foundDecoder = true;

                if (IsEqualGUID(MFVideoFormat_WMV3, inputVideoSubtype) ||
                    IsEqualGUID(MFVideoFormat_WVC1, inputVideoSubtype))
                {
                    CComPtr<IMFTransform> decoderTransform;
                    // Check if Decoder is using HW acceleration
                    if (*useHwCodec)
                    {
                        decoderTransform = *ppDecoder;

                        CComPtr<IPropertyStore> propertyStore;
                        hr = decoderTransform->QueryInterface(IID_PPV_ARGS(&propertyStore));
                        RETURNIFFAILED(hr);

                        PROPVARIANT var;
                        hr = propertyStore->GetValue(MFPKEY_DXVA_ENABLED, &var);
                        RETURNIFFAILED(hr);

                        bool isHWAccelerated;
                        isHWAccelerated = var.boolVal == VARIANT_TRUE && var.vt == VT_BOOL;

                        if (!isHWAccelerated)
                        {
                            isHwCodecBeingUsed = false;
                            printf("Stream cannot be decoded using HW decoder.  Hence using SW decoder to decode the stream\n");
                            LOG(mLogFile, "Stream cannot be decoded using HW decoder.  Hence using SW decoder to decode the stream\n");
                        }
                        else
                        {
                            isHwCodecBeingUsed = true;
                            printf("Selected HW decoder to decode the stream\n");
                            LOG(mLogFile, "Selected HW decoder to decode the stream\n");
                        }
                    }
                    else
                    {
                        printf("Selected SW Decoder to decode the stream\n");
                        LOG(mLogFile, "Selected SW Decoder to decode the stream\n");
                    }
                }
                else if (IsEqualGUID(MFVideoFormat_MP4V, inputVideoSubtype) ||
                    IsEqualGUID(MFVideoFormat_M4S2, inputVideoSubtype) ||
                    IsEqualGUID(MFVideoFormat_XVID, inputVideoSubtype))
                {
                    if (*useHwCodec)
                    {
                        if (isHwCodecBeingUsed == true)
                        {
                            printf("Selected HW Decoder to decode the stream\n");
                            LOG(mLogFile, "Selected HW Decoder to decode the stream\n");
                        }
                        else
                        {
                            printf("AMD HW Decoder does not supports decoding of resolutions < 720p.  For lower resolutions SW decoder is used to decode the stream\n");
                            LOG(mLogFile, "AMD HW Decoder does not supports decoding of resolutions < 720p.  For lower resolutions SW decoder is used to decode the stream\n");
                        }
                    }
                    else
                    {
                        printf("Selected SW Decoder to decode the stream\n");
                        LOG(mLogFile, "Selected SW Decoder to decode the stream\n");
                    }
                }
                else
                {
                    if (*useHwCodec)
                    {
                        printf("Selected HW Decoder to decode the stream\n");
                        LOG(mLogFile, "Selected HW Decoder to decode the stream\n");
                    }
                    else
                    {
                        printf("Selected SW Decoder to decode the stream\n");
                        LOG(mLogFile, "Selected SW Decoder to decode the stream\n");
                    }
                }

                displayMFT(*ppActivate);
            }

            for (j = 0; j < registeredDecodersNumber; j++)
            {
                ppActivate[j]->Release();
            }
            CoTaskMemFree(ppActivate);
        }

        if (!foundDecoder)
        {
            return E_FAIL;
        }

        *useHwCodec = isHwCodecBeingUsed;

        return S_OK;
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
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
    *****************************************************************************
    */
    HRESULT findVideoEncoder(REFCLSID inputVideoSubtype,
        BOOL useHwCodec,
        IMFTransform **ppEncoder)
    {
        HRESULT hr;
        UINT32 i;

        MFT_REGISTER_TYPE_INFO inputRegisterTypeInfo = { MFMediaType_Video,
            inputVideoSubtype };
        MFT_REGISTER_TYPE_INFO outputRegisterTypeInfo = { MFMediaType_Video,
            MFVideoFormat_H264 };

        UINT32 flags = MFT_ENUM_FLAG_SORTANDFILTER;

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
        UINT32 registeredEncoderNumber = 0;

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
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
    *****************************************************************************
    */
    HRESULT setEncoderValue(const GUID* guid, UINT32 value, CComPtr<IMFTransform> encoderTransform)
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
    /**
    *****************************************************************************
    *  @fn     setEncoderValue
    *  @brief  Set BOOL encoder parameter
    *
    *  @param[in]  guid             : CLSID of the encoder parameter to be set
    *  @param[out] value            : Parameter value
    *  @param[in] encoderTransform  : Pointer to the encoder transform
    *
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
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
    HRESULT checkRange(uint32 min, uint32 max, uint32 param)
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
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
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

            UINT32 transformAsync;
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

                UINT32 d3dAware;
                hr = encoderAttributes->GetUINT32(d3dAwareAttribute, &d3dAware);
                if (SUCCEEDED(hr) && d3dAware != 0)
                {
                    hr = encoderTransform->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, deviceManagerPtr);
                    RETURNIFFAILED(hr);
                }
            }

            //Set Encoder Properties
            hr = setEncoderValue(&CODECAPI_AVEncMPVGOPSize,
                (uint32)vidEncParams->gopSize, encoderTransform);
            if (hr != S_OK)
            {
                LOG(mLogFile, "Failed to set CODECAPI_AVEncMPVGOPSize @ %s %d \n", TEXT(__FILE__), __LINE__);
            }

            hr = checkRange(0, 1, (uint32)vidEncParams->lowLatencyMode);
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
            hr = checkRange(0, 3, (uint32)vidEncParams->rateControlMethod);
            LOGIFFAILED(mLogFile, hr, "un-supported encRateControlMethod @ %s %d \n", TEXT(__FILE__), __LINE__);
            hr = setEncoderValue(&CODECAPI_AVEncCommonRateControlMode,
                (uint32)vidEncParams->rateControlMethod, encoderTransform);
            if (hr != S_OK)
            {
                LOG(mLogFile, "Failed to set CODECAPI_AVEncCommonRateControlMode @ %s %d \n", TEXT(__FILE__), __LINE__);
            }
            hr = checkRange(0, 100, (uint32)vidEncParams->commonQuality);
            LOGIFFAILED(mLogFile, hr, "un-supported encCommonQuality parameter @ %s %d \n", TEXT(__FILE__), __LINE__);
            hr = setEncoderValue(&CODECAPI_AVEncCommonQuality,
                (uint32)vidEncParams->commonQuality, encoderTransform);
            if (hr != S_OK)
            {
                LOG(mLogFile, "Failed to set CODECAPI_AVEncCommonQuality @ %s %d \n", TEXT(__FILE__), __LINE__);
            }
            /**********************************************************************
            * Not supported presently. Supports only CABAC                        *
            **********************************************************************/
            hr = checkRange(0, 1, (uint32)vidEncParams->enableCabac);
            LOGIFFAILED(mLogFile, hr, "un-supported enableCabac @ %s %d \n", TEXT(__FILE__), __LINE__);
            hr = setEncoderValue(&CODECAPI_AVEncH264CABACEnable,
                (BOOL)vidEncParams->enableCabac, encoderTransform);
            if (hr != S_OK)
            {
                LOG(mLogFile, "un-supported Cabac enable flag @ %s %d \n", TEXT(__FILE__), __LINE__);
            }

            hr = checkRange(0, 100, (uint32)vidEncParams->qualityVsSpeed);
            LOGIFFAILED(mLogFile, hr, "un-supported encQualityVsSpeed parameter @ %s %d \n", TEXT(__FILE__), __LINE__);
            hr = setEncoderValue(&CODECAPI_AVEncCommonQualityVsSpeed,
                (uint32)vidEncParams->qualityVsSpeed, encoderTransform);
            if (hr != S_OK)
            {
                LOG(mLogFile, "Failed to set CODECAPI_AVEncCommonQualityVsSpeed @ %s %d \n", TEXT(__FILE__), __LINE__);
            }

            hr = setEncoderValue(&CODECAPI_AVEncCommonMaxBitRate,
                (uint32)vidEncParams->maxBitrate, encoderTransform);
            if (hr != S_OK)
            {
                LOG(mLogFile, "Failed to set CODECAPI_AVEncCommonMaxBitRate @ %s %d \n", TEXT(__FILE__), __LINE__);
            }

            hr = setEncoderValue(&CODECAPI_AVEncCommonBufferSize,
                (uint32)vidEncParams->bufSize, encoderTransform);
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

        UINT32 h264Profile;
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
    *  @fn     createCustomTransformNode
    *  @brief  Create custom transform node
    *
    *  @param[in] targetVideoType     : Encoder video type
    *  @param[out] transformNode      : source video type
    *  @param[in] deviceManagerPtr    : Pointer to the Device manager
    *  @param[in] customTransformGuid : Custom transform GUID
    *  @param[out] transform          : Pointer to the transform
    *
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
    *****************************************************************************
    */
    HRESULT createCustomTransformNode(IMFMediaType* targetVideoType,
        IMFTopologyNode **transformNode,
        ULONG_PTR deviceManagerPtr, REFGUID customTransformGuid,
        IMFTransform** transform)
    {
        if (nullptr == transformNode)
        {
            return E_POINTER;
        }

        HRESULT hr;

        CComPtr<IMFTransform> customTransform;
        hr = createTransform(customTransformGuid, &customTransform,
            deviceManagerPtr);
        RETURNIFFAILED(hr);

        if (targetVideoType != nullptr)
        {
            CComPtr<IMFMediaType> outputVideoType;
            hr = createVideoTypeFromSource(targetVideoType, MFVideoFormat_NV12,
                TRUE, TRUE, &outputVideoType);
            RETURNIFFAILED(hr);

            hr = customTransform->SetOutputType(0, outputVideoType, 0);
            RETURNIFFAILED(hr);
        }

        CComPtr<IMFTopologyNode> node;
        hr = MFCreateTopologyNode(MF_TOPOLOGY_TRANSFORM_NODE, &node);
        RETURNIFFAILED(hr);

        hr = node->SetObject(customTransform);
        RETURNIFFAILED(hr);

        hr
            = node->SetUINT32(
            MF_TOPONODE_CONNECT_METHOD,
            MF_CONNECT_ALLOW_CONVERTER
            | MF_CONNECT_RESOLVE_INDEPENDENT_OUTPUTTYPES);
        RETURNIFFAILED(hr);

        *transformNode = node.Detach();

        if (transform != nullptr)
        {
            *transform = customTransform.Detach();
        }

        return S_OK;
    }
    /**
    *****************************************************************************
    *  @fn     createTexture
    *  @brief  Create D3D11 texture 2d
    *
    *  @param[in] width       : width
    *  @param[in] height      : Height
    *  @param[in] bindFlag    : bind flag
    *  @param[in] format      : video format
    *  @param[in] p3dDevice  : D3D device pointer
    *  @param[out] texture    : Pointer to the Texture created
    *
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
    *****************************************************************************
    */
    HRESULT createTexture(UINT width, UINT height, UINT bindFlag,
        DXGI_FORMAT format, ID3D11Device* p3dDevice,
        ID3D11Texture2D** texture)
    {
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(D3D11_TEXTURE2D_DESC));
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = bindFlag;
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

        return p3dDevice->CreateTexture2D(&desc, NULL, texture);
    }
    /**
    *****************************************************************************
    *  @fn     mediaBuffer2Texture
    *  @brief  Convert media buffer to texture
    *
    *  @param[in] pDevice          : Device
    *  @param[in] mediaType        : Media type
    *  @param[in] mediaBuffer      : Media buffer
    *  @param[out] ppTexture       : Texture output
    *  @param[in] textureViewIndex : Texture index
    *
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
    *****************************************************************************
    */
    HRESULT mediaBuffer2Texture(ID3D11Device* pDevice, IMFMediaType* mediaType,
        IMFMediaBuffer* mediaBuffer, ID3D11Texture2D** ppTexture,
        UINT* textureViewIndex)
    {
        /**********************************************************************
        * In case of system memory buffer on input, we create a new texture   *
        * and perform copy of pixels into it                                  *
        **********************************************************************/
        HRESULT hr = E_FAIL;
        /**********************************************************************
        * expect DX11 surface on input                                        *
        **********************************************************************/
        CComPtr<IMFDXGIBuffer> spD11Buffer;
        hr = mediaBuffer->QueryInterface(IID_IMFDXGIBuffer,
            (void**)&spD11Buffer);

        if (SUCCEEDED(hr))
        {
            hr = spD11Buffer->GetSubresourceIndex(textureViewIndex);
            RETURNIFFAILED(hr);

            hr = spD11Buffer->GetResource(IID_ID3D11Texture2D,
                (void**)ppTexture);
            RETURNIFFAILED(hr);
        }
        /**********************************************************************
        * no DX11: create new texture and copy pixels into it                 *
        **********************************************************************/
        else
        {
            UINT32 width = 0;
            UINT32 height = 0;
            INT32 pitch = 0;
            GUID subtype;
            MFGetAttributeSize(mediaType, MF_MT_FRAME_SIZE, &width, &height);
            pitch = MFGetAttributeUINT32(mediaType, MF_MT_DEFAULT_STRIDE, width);
            mediaType->GetGUID(MF_MT_SUBTYPE, &subtype);

            if (IsEqualGUID(subtype, MFVideoFormat_NV12))
            {
                /**************************************************************
                * create a new texture                                        *
                **************************************************************/
                BYTE* pSrc = nullptr;
                DWORD maxlenSrc = 0;
                DWORD lenSrc = 0;
                hr = mediaBuffer->Lock(&pSrc, &maxlenSrc, &lenSrc);

                if (SUCCEEDED(hr))
                {
                    do
                    {
                        /******************************************************
                        * create temporary surface in CPU space               *
                        ******************************************************/
                        CComPtr<ID3D11Texture2D> textureTmp;
                        D3D11_TEXTURE2D_DESC descTmp = { 0 };
                        descTmp.ArraySize = 1;
                        descTmp.MipLevels = 1;
                        descTmp.SampleDesc.Count = 1;
                        descTmp.Format = DXGI_FORMAT_NV12;
                        descTmp.BindFlags = 0;
                        descTmp.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                        descTmp.Usage = D3D11_USAGE_STAGING;
                        descTmp.Width = width;
                        descTmp.Height = height;
                        hr = pDevice->CreateTexture2D(&descTmp, nullptr,
                            &textureTmp);
                        if (FAILED(hr))
                            break;
                        /******************************************************
                        * map it to receive data pointer                      *
                        ******************************************************/
                        CComPtr<ID3D11DeviceContext> deviceContext;
                        pDevice->GetImmediateContext(&deviceContext);
                        D3D11_MAPPED_SUBRESOURCE mapped;
                        hr = deviceContext->Map(textureTmp, 0, D3D11_MAP_WRITE,
                            0, &mapped);
                        if (FAILED(hr))
                            break;
                        /******************************************************
                        * copy NV12 pixels into temporary surface             *
                        ******************************************************/
                        BYTE* dst = (BYTE*)mapped.pData;
                        BYTE* src = pSrc;
                        /******************************************************
                        * Y bytes                                             *
                        ******************************************************/
                        for (UINT32 i = 0; i < height; i++)
                        {
                            memcpy(dst, src, width);
                            dst += mapped.RowPitch;
                            src += pitch;
                        }
                        /******************************************************
                        * UV bytes                                            *
                        ******************************************************/
                        for (UINT32 i = 0; i < height / 2; i++)
                        {
                            memcpy(dst, src, width);
                            dst += mapped.RowPitch;
                            src += pitch;
                        }
                        deviceContext->Unmap(textureTmp, 0);
                        /******************************************************
                        * create output surface in GPU space                  *
                        ******************************************************/
                        D3D11_TEXTURE2D_DESC descNV12 = { 0 };
                        descNV12.ArraySize = 1;
                        descNV12.MipLevels = 1;
                        descNV12.SampleDesc.Count = 1;
                        descNV12.Format = DXGI_FORMAT_NV12;
                        descNV12.BindFlags = D3D11_BIND_SHADER_RESOURCE
                            | D3D11_BIND_RENDER_TARGET
                            | D3D11_BIND_UNORDERED_ACCESS;
                        descNV12.CPUAccessFlags = 0;
                        descNV12.Usage = D3D11_USAGE_DEFAULT;
                        descNV12.Width = width;
                        descNV12.Height = height;
                        hr = pDevice->CreateTexture2D(&descNV12, nullptr,
                            ppTexture);
                        if (FAILED(hr))
                            break;
                        /******************************************************
                        * copy surface pixels from CPU to GPU                 *
                        ******************************************************/
                        deviceContext->CopyResource(*ppTexture, textureTmp);
                        *textureViewIndex = 0;

                    } while (false);

                    mediaBuffer->Unlock();
                }
            }

            if (IsEqualGUID(subtype, MFVideoFormat_RGB32))
            {
                /**************************************************************
                * create a new texture                                        *
                **************************************************************/
                BYTE* pSrc = nullptr;
                DWORD maxlenSrc = 0;
                DWORD lenSrc = 0;
                hr = mediaBuffer->Lock(&pSrc, &maxlenSrc, &lenSrc);

                if (SUCCEEDED(hr))
                {
                    do
                    {
                        /******************************************************
                        * create temporary surface in CPU space               *
                        ******************************************************/
                        CComPtr<ID3D11Texture2D> textureTmp;
                        D3D11_TEXTURE2D_DESC descTmp = { 0 };
                        descTmp.ArraySize = 1;
                        descTmp.MipLevels = 1;
                        descTmp.SampleDesc.Count = 1;
                        descTmp.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                        descTmp.BindFlags = 0;
                        descTmp.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                        descTmp.Usage = D3D11_USAGE_STAGING;
                        descTmp.Width = width;
                        descTmp.Height = height;
                        hr = pDevice->CreateTexture2D(&descTmp, nullptr,
                            &textureTmp);
                        if (FAILED(hr))
                            break;
                        /******************************************************
                        * map it to receive data pointer                      *
                        ******************************************************/
                        CComPtr<ID3D11DeviceContext> deviceContext;
                        pDevice->GetImmediateContext(&deviceContext);
                        D3D11_MAPPED_SUBRESOURCE mapped;
                        hr = deviceContext->Map(textureTmp, 0, D3D11_MAP_WRITE,
                            0, &mapped);
                        if (FAILED(hr))
                            break;
                        /******************************************************
                        * copy RGB pixels into temporary surface              *
                        ******************************************************/
                        BYTE* dst = (BYTE*)mapped.pData;
                        BYTE* src = pSrc;
                        /******************************************************
                        * RGB bytes                                           *
                        ******************************************************/
                        for (UINT32 i = 0; i < height; i++)
                        {
                            memcpy(dst, src, width * 4);
                            dst += mapped.RowPitch;
                            src -= pitch;
                        }

                        deviceContext->Unmap(textureTmp, 0);
                        /******************************************************
                        * create output surface in GPU space                  *
                        ******************************************************/
                        D3D11_TEXTURE2D_DESC descRGB32 = { 0 };
                        descRGB32.ArraySize = 1;
                        descRGB32.MipLevels = 1;
                        descRGB32.SampleDesc.Count = 1;
                        descRGB32.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                        descRGB32.BindFlags = D3D11_BIND_SHADER_RESOURCE
                            | D3D11_BIND_RENDER_TARGET;
                        descRGB32.CPUAccessFlags = 0;
                        descRGB32.Usage = D3D11_USAGE_DEFAULT;
                        descRGB32.Width = width;
                        descRGB32.Height = height;
                        hr = pDevice->CreateTexture2D(&descRGB32, nullptr,
                            ppTexture);
                        if (FAILED(hr))
                            break;
                        /******************************************************
                        * copy surface pixels from CPU to GPU                 *
                        ******************************************************/
                        deviceContext->CopyResource(*ppTexture, textureTmp);
                        *textureViewIndex = 0;

                    } while (false);

                    mediaBuffer->Unlock();
                }
            }
        }

        return hr;
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
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
    *****************************************************************************
    */
    HRESULT getNV12ImageSize(UINT32 width, UINT32 height, DWORD* pcbImage)
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

    HRESULT getRGBImageSize(UINT32 width, UINT32 height, DWORD* pcbImage)
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

    /**
    *****************************************************************************
    *  @fn     createDx11VideoRendererActivate
    *  @brief  Create Dx11 renderer
    *
    *  @param[in] hWnd                   : Hanlde to the window
    *  @param[out] videoRendererActivate : Video renderer created
    *
    *  @return HRESULT : S_OK if successful; otherwise returns micorsoft error codes.
    *****************************************************************************
    */
    HRESULT createDx11VideoRendererActivate(HWND hWnd,
        IMFActivate** videoRendererActivate)
    {
        if (videoRendererActivate == nullptr)
        {
            return E_POINTER;
        }
        /**********************************************************************
        * Function DX11VideoRenderer.dll exists only on Windows 8+. So we     *
        * have to obtain its address dynamically to be able work on Windows 7.*
        **********************************************************************/

        HMODULE hDx11VideoRendererDll = LoadLibrary(L"DX11VideoRenderer.dll");
        if (NULL == hDx11VideoRendererDll)
        {
            TRACE_MSG("Failed to load DX11VideoRenderer.dll", hDx11VideoRendererDll)
                return E_FAIL;
        }

        LPCSTR functionName = "CreateDX11VideoRendererActivate";
        FARPROC createDX11VideoRendererActivateFarProc = GetProcAddress(
            hDx11VideoRendererDll, functionName);
        if (nullptr == createDX11VideoRendererActivateFarProc)
        {
            TRACE_MSG("Failed to get CreateDX11VideoRendererActivate() address", createDX11VideoRendererActivateFarProc)
                return E_FAIL;
        }

        typedef HRESULT(STDAPICALLTYPE* LPCreateDX11VideoRendererActivate)(HWND, IMFActivate**);

        LPCreateDX11VideoRendererActivate
            createDX11VideoRendererActivate =
            reinterpret_cast<LPCreateDX11VideoRendererActivate> (createDX11VideoRendererActivateFarProc);

        HRESULT hr = createDX11VideoRendererActivate(hWnd,
            videoRendererActivate);
        RETURNIFFAILED(hr);

        return S_OK;
    }
    HRESULT createVqTransform(ULONG_PTR deviceManagerPtr, IMFTransform** transform)
    {
        if (nullptr == transform)
        {
            return E_POINTER;
        }

        HRESULT hr;

        /**********************************************************************
        * Prebuilt VQ kernels                                                 *
        **********************************************************************/
        CComPtr<IAMFCacheBuilder> cacheBuilder;
        hr = AMFCreateCacheBuilderMFT(IID_PPV_ARGS(&cacheBuilder));
        RETURNIFFAILED(hr);
        hr = cacheBuilder->IsBuildCacheRequired((IUnknown*)deviceManagerPtr);
        hr = cacheBuilder->BuildCache((IUnknown*)deviceManagerPtr, nullptr);
        RETURNIFFAILED(hr);

        /**********************************************************************
        * Create VQ transform                                                 *
        **********************************************************************/
        CComPtr<IMFTransform> vqTransform;
        hr = AMFCreateVideoTransformMFT(IID_PPV_ARGS(&vqTransform));
        RETURNIFFAILED(hr);

        hr = vqTransform->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, deviceManagerPtr);
        RETURNIFFAILED(hr);

        *transform = vqTransform.Detach();

        return S_OK;
    }
    HRESULT createTransformNode(IMFTransform* transform, UINT32 connectionMethod,
        IMFTopologyNode **transformNode)
    {
        if (nullptr == transform)
        {
            return E_INVALIDARG;
        }

        if (nullptr == transformNode)
        {
            return E_POINTER;
        }

        HRESULT hr;

        CComPtr<IMFTopologyNode> node;
        hr = MFCreateTopologyNode(MF_TOPOLOGY_TRANSFORM_NODE, &node);
        RETURNIFFAILED(hr);

        hr = node->SetObject(transform);
        RETURNIFFAILED(hr);

        hr = node->SetUINT32(MF_TOPONODE_CONNECT_METHOD, connectionMethod);
        RETURNIFFAILED(hr);

        *transformNode = node.Detach();

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
