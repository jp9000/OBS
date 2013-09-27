/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 - 2012 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "d3d11_device.h"

#if MFX_D3D11_SUPPORT

#include "sample_defs.h"

CD3D11Device::CD3D11Device()
{
}

CD3D11Device::~CD3D11Device()
{
    Close();
}

mfxStatus CD3D11Device::FillSCD(mfxHDL hWindow, DXGI_SWAP_CHAIN_DESC& scd)
{
    scd.Windowed = TRUE;
    scd.OutputWindow = (HWND)hWindow;
    scd.SampleDesc.Count = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 1;

    return MFX_ERR_NONE;
}

mfxStatus CD3D11Device::Init(
    mfxHDL hWindow,
    mfxU16 nViews,
    mfxU32 nAdapterNum)
{
    mfxStatus sts = MFX_ERR_NONE;
    HRESULT hres = S_OK;
    m_nViews = nViews;
    if (2 < nViews)
        return MFX_ERR_UNSUPPORTED;
    m_bDefaultStereoEnabled = FALSE;

    static D3D_FEATURE_LEVEL FeatureLevels[] = { 
        D3D_FEATURE_LEVEL_11_1, 
        D3D_FEATURE_LEVEL_11_0, 
        D3D_FEATURE_LEVEL_10_1, 
        D3D_FEATURE_LEVEL_10_0 
    };
    D3D_FEATURE_LEVEL pFeatureLevelsOut;

    hres = CreateDXGIFactory(__uuidof(IDXGIFactory2), (void**)(m_pDXGIFactory.Assign()) );
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    if (m_nViews == 2 && hWindow)
    {
        hres = m_pDXGIFactory->QueryInterface(__uuidof(IDXGIDisplayControl), (void **)m_pDisplayControl.Assign());
        if (FAILED(hres))
            return MFX_ERR_DEVICE_FAILED;
        m_bDefaultStereoEnabled = m_pDisplayControl->IsStereoEnabled();
        if (!m_bDefaultStereoEnabled) 
            m_pDisplayControl->SetStereoEnabled(TRUE);
    }
    
    hres = m_pDXGIFactory->EnumAdapters(nAdapterNum, m_pAdapter.Assign());
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    hres =  D3D11CreateDevice(m_pAdapter ,
                            D3D_DRIVER_TYPE_UNKNOWN,
                            NULL,
                            0,
                            FeatureLevels,
                            MSDK_ARRAY_LEN(FeatureLevels),
                            D3D11_SDK_VERSION,
                            m_pD3D11Device.Assign(),
                            &pFeatureLevelsOut,
                            m_pD3D11Ctx.Assign());

    if (FAILED(hres))    
        return MFX_ERR_DEVICE_FAILED;

    m_pDXGIDev = m_pD3D11Device;
    m_pDX11VideoDevice = m_pD3D11Device;
    m_pVideoContext = m_pD3D11Ctx;
    
    MSDK_CHECK_POINTER(!!m_pDXGIDev, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(!!m_pDX11VideoDevice, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(!!m_pVideoContext, MFX_ERR_NULL_PTR);

    // turn on multithreading for the Context 
    ComPtr<ID3D10Multithread> p_mt;
    p_mt = m_pVideoContext;

    if (p_mt)
        p_mt->SetMultithreadProtected(true);
    else
        return MFX_ERR_DEVICE_FAILED; 

    // create swap chain only for rendering use case (hWindow != 0)    
    DXGI_SWAP_CHAIN_DESC scd;
    if (hWindow)
    {
        ZeroMemory(&scd, sizeof(scd));
        sts = FillSCD(hWindow, scd);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        MSDK_CHECK_POINTER (!!m_pDXGIFactory, MFX_ERR_NULL_PTR);
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {0};
        swapChainDesc.Width = 0;                                     // Use automatic sizing.
        swapChainDesc.Height = 0;
        swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;           // This is the most common swap chain format.
        swapChainDesc.Stereo = m_nViews == 2 ? TRUE : FALSE;
        swapChainDesc.SampleDesc.Count = 1;                          // Don't use multi-sampling.
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = 2;                               // Use double buffering to minimize latency.
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        hres = m_pDXGIFactory->CreateSwapChainForHwnd(m_pD3D11Device,
            (HWND)hWindow, 
            &swapChainDesc,
            NULL,
            NULL,
            reinterpret_cast<IDXGISwapChain1**>(&m_pSwapChain) );
        if (FAILED(hres))
            return MFX_ERR_DEVICE_FAILED;
    }

    return sts;
}

mfxStatus CD3D11Device::CreateVideoProcessor(mfxFrameSurface1 * pSrf)
{
    HRESULT hres = S_OK;

    if (!!m_VideoProcessorEnum || NULL == pSrf)
        return MFX_ERR_NONE;

    //create video processor
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC ContentDesc;
    MSDK_ZERO_MEMORY( ContentDesc );

    ContentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    ContentDesc.InputFrameRate.Numerator = 30000;
    ContentDesc.InputFrameRate.Denominator = 1000;
    ContentDesc.InputWidth  = pSrf->Info.CropW;
    ContentDesc.InputHeight = pSrf->Info.CropH;
    ContentDesc.OutputWidth = pSrf->Info.CropW;
    ContentDesc.OutputHeight = pSrf->Info.CropH;
    ContentDesc.OutputFrameRate.Numerator = 30000;
    ContentDesc.OutputFrameRate.Denominator = 1000;

    ContentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    hres = m_pDX11VideoDevice->CreateVideoProcessorEnumerator( &ContentDesc, m_VideoProcessorEnum.Assign() );
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    hres = m_pDX11VideoDevice->CreateVideoProcessor( m_VideoProcessorEnum, 0, m_pVideoProcessor.Assign() );
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;
    
    return MFX_ERR_NONE;
}

mfxStatus CD3D11Device::Reset()
{
    // Changing video mode back to the original state
    if (2 == m_nViews && !m_bDefaultStereoEnabled)
        m_pDisplayControl->SetStereoEnabled(FALSE);
    return MFX_ERR_NONE;
}

mfxStatus CD3D11Device::GetHandle(mfxHandleType type, mfxHDL *pHdl)
{
    if (MFX_HANDLE_D3D11_DEVICE == type)
    {
        *pHdl = m_pD3D11Device;
        return MFX_ERR_NONE;
    }
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus CD3D11Device::SetHandle(mfxHandleType /*type*/, mfxHDL /*hdl*/)
{
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus CD3D11Device::RenderFrame(mfxFrameSurface1 * pSrf, mfxFrameAllocator * pAlloc)
{
    HRESULT hres = S_OK;
    mfxStatus sts;

    sts = CreateVideoProcessor(pSrf);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    
    hres = m_pSwapChain->GetBuffer(0, __uuidof( ID3D11Texture2D ), (void**)m_pDXGIBackBuffer.Assign());
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC OutputViewDesc;
    if (2 == m_nViews)
    {
        m_pVideoContext->VideoProcessorSetStreamStereoFormat(m_pVideoProcessor, 0, TRUE,D3D11_VIDEO_PROCESSOR_STEREO_FORMAT_SEPARATE,
            TRUE, TRUE, D3D11_VIDEO_PROCESSOR_STEREO_FLIP_NONE, NULL);
        m_pVideoContext->VideoProcessorSetOutputStereoMode(m_pVideoProcessor,TRUE);

        OutputViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2DARRAY;
        OutputViewDesc.Texture2DArray.ArraySize = 2;
        OutputViewDesc.Texture2DArray.MipSlice = 0;
        OutputViewDesc.Texture2DArray.FirstArraySlice = 0;
    }
    else
    {
        OutputViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
        OutputViewDesc.Texture2D.MipSlice = 0;        
    }

    if (1 == m_nViews || 0 == pSrf->Info.FrameId.ViewId)
    {
        hres = m_pDX11VideoDevice->CreateVideoProcessorOutputView( 
            m_pDXGIBackBuffer,
            m_VideoProcessorEnum,
            &OutputViewDesc,
            m_pOutputView.Assign() );
        if (FAILED(hres))
            return MFX_ERR_DEVICE_FAILED;
    }

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC InputViewDesc;
    InputViewDesc.FourCC = 0;
    InputViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    InputViewDesc.Texture2D.MipSlice = 0;
    InputViewDesc.Texture2D.ArraySlice = 0;
    
    mfxHDLPair pair = {NULL};
    sts = pAlloc->GetHDL(pAlloc->pthis, pSrf->Data.MemId, (mfxHDL*)&pair);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    ID3D11Texture2D  *pRTTexture2D = reinterpret_cast<ID3D11Texture2D*>(pair.first);
    D3D11_TEXTURE2D_DESC RTTexture2DDesc;

    if(!m_pTempTexture && m_nViews == 2)
    {
        pRTTexture2D->GetDesc(&RTTexture2DDesc);
        hres = m_pD3D11Device->CreateTexture2D(&RTTexture2DDesc,NULL, m_pTempTexture.Assign());
        if (FAILED(hres)) 
            return MFX_ERR_DEVICE_FAILED;
    }
    
    // Creating input views for left and righ eyes
    if (1 == m_nViews)
    {
        hres = m_pDX11VideoDevice->CreateVideoProcessorInputView( 
            pRTTexture2D,
            m_VideoProcessorEnum,
            &InputViewDesc,
            m_pInputViewLeft.Assign() );
    
    }
    else if (2 == m_nViews && 0 == pSrf->Info.FrameId.ViewId)
    {
        m_pD3D11Ctx->CopyResource(m_pTempTexture,pRTTexture2D);
        hres = m_pDX11VideoDevice->CreateVideoProcessorInputView( 
            m_pTempTexture,
            m_VideoProcessorEnum,
            &InputViewDesc,
            m_pInputViewLeft.Assign() );
    }
    else
    {
        hres = m_pDX11VideoDevice->CreateVideoProcessorInputView(
            pRTTexture2D,
            m_VideoProcessorEnum,
            &InputViewDesc,
            m_pInputViewRight.Assign() );
        
    }
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;   

    //  NV12 surface to RGB backbuffer       
    RECT rect = {0};
    rect.right  = pSrf->Info.CropW;
    rect.bottom = pSrf->Info.CropH;

    D3D11_VIDEO_PROCESSOR_STREAM StreamData;

    if (1 == m_nViews || pSrf->Info.FrameId.ViewId == 1)
    {    
        StreamData.Enable = TRUE;
        StreamData.OutputIndex = 0;
        StreamData.InputFrameOrField = 0;
        StreamData.PastFrames = 0;
        StreamData.FutureFrames = 0;
        StreamData.ppPastSurfaces = NULL;
        StreamData.ppFutureSurfaces = NULL;
        StreamData.pInputSurface = m_pInputViewLeft;
        StreamData.ppPastSurfacesRight = NULL;
        StreamData.ppFutureSurfacesRight = NULL;
        StreamData.pInputSurfaceRight = m_nViews == 2 ? m_pInputViewRight : NULL;

        m_pVideoContext->VideoProcessorSetStreamSourceRect(m_pVideoProcessor, 0, true, &rect);
        m_pVideoContext->VideoProcessorSetStreamFrameFormat( m_pVideoProcessor, 0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);        
        hres = m_pVideoContext->VideoProcessorBlt( m_pVideoProcessor, m_pOutputView, 0, 1, &StreamData );  
        if (FAILED(hres))
            return MFX_ERR_DEVICE_FAILED;
    }       
    
    if (1 == m_nViews || 1 == pSrf->Info.FrameId.ViewId)
    {    
        DXGI_PRESENT_PARAMETERS parameters = {0};
        hres = m_pSwapChain->Present1(0, 0, &parameters);
        if (FAILED(hres))
            return MFX_ERR_DEVICE_FAILED;
    }    

    return MFX_ERR_NONE;
}

void CD3D11Device::Close()
{
    Reset();
}

#endif // #if MFX_D3D11_SUPPORT