/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 - 2012 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include "sample_defs.h" // defines MFX_D3D11_SUPPORT

#if MFX_D3D11_SUPPORT
#include "hw_device.h"
#include <windows.h>
#include <d3d11.h>
#include <atlbase.h>

#include <dxgi1_2.h>

class CD3D11Device: public CHWDevice
{
public:
    CD3D11Device();
    virtual ~CD3D11Device();
    virtual mfxStatus Init(
        mfxHDL hWindow,
        mfxU16 nViews,
        mfxU32 nAdapterNum);
    virtual mfxStatus Reset();
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *pHdl);
    virtual mfxStatus SetHandle(mfxHandleType type, mfxHDL hdl);
    virtual mfxStatus RenderFrame(mfxFrameSurface1 * pSurface, mfxFrameAllocator * pmfxAlloc);
    virtual void      Close();
protected:
    virtual mfxStatus FillSCD(mfxHDL hWindow, DXGI_SWAP_CHAIN_DESC& scd);
    mfxStatus CreateVideoProcessor(mfxFrameSurface1 * pSrf);

    CComPtr<ID3D11Device>                   m_pD3D11Device;
    CComPtr<ID3D11DeviceContext>            m_pD3D11Ctx;
    CComQIPtr<ID3D11VideoDevice>            m_pDX11VideoDevice;
    CComQIPtr<ID3D11VideoContext>           m_pVideoContext;
    CComPtr<ID3D11VideoProcessorEnumerator> m_VideoProcessorEnum;

    CComQIPtr<IDXGIDevice1>                 m_pDXGIDev;
    CComQIPtr<IDXGIAdapter>                 m_pAdapter;

    CComPtr<IDXGIFactory2>                  m_pDXGIFactory;

    CComPtr<IDXGISwapChain1>                m_pSwapChain;
    CComPtr<ID3D11VideoProcessor>           m_pVideoProcessor;

private:
    CComPtr<ID3D11VideoProcessorInputView>  m_pInputViewLeft;
    CComPtr<ID3D11VideoProcessorInputView>  m_pInputViewRight;
    CComPtr<ID3D11VideoProcessorOutputView> m_pOutputView;

    CComPtr<ID3D11Texture2D>                m_pDXGIBackBuffer;
    CComPtr<ID3D11Texture2D>                m_pTempTexture;
    CComPtr<IDXGIDisplayControl>            m_pDisplayControl;
    CComPtr<IDXGIOutput>                    m_pDXGIOutput;
    mfxU16                                  m_nViews;
    BOOL                                    m_bDefaultStereoEnabled;
};
#endif //#if MFX_D3D11_SUPPORT
