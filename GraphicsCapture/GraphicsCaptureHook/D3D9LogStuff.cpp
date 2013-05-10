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


#include "GraphicsCaptureHook.h"

#include <d3d9.h>

static string GetD3D9FormatName(DWORD format)
{
    switch(format) {
    case D3DFMT_UNKNOWN             : return string("D3DFMT_UNKNOWN");
    case D3DFMT_R8G8B8              : return string("D3DFMT_R8G8B8");
    case D3DFMT_A8R8G8B8            : return string("D3DFMT_A8R8G8B8");
    case D3DFMT_X8R8G8B8            : return string("D3DFMT_X8R8G8B8");
    case D3DFMT_R5G6B5              : return string("D3DFMT_R5G6B5");
    case D3DFMT_X1R5G5B5            : return string("D3DFMT_X1R5G5B5");
    case D3DFMT_A1R5G5B5            : return string("D3DFMT_A1R5G5B5");
    case D3DFMT_A4R4G4B4            : return string("D3DFMT_A4R4G4B4");
    case D3DFMT_R3G3B2              : return string("D3DFMT_R3G3B2");
    case D3DFMT_A8                  : return string("D3DFMT_A8");
    case D3DFMT_A8R3G3B2            : return string("D3DFMT_A8R3G3B2");
    case D3DFMT_X4R4G4B4            : return string("D3DFMT_X4R4G4B4");
    case D3DFMT_A2B10G10R10         : return string("D3DFMT_A2B10G10R10");
    case D3DFMT_A8B8G8R8            : return string("D3DFMT_A8B8G8R8");
    case D3DFMT_X8B8G8R8            : return string("D3DFMT_X8B8G8R8");
    case D3DFMT_G16R16              : return string("D3DFMT_G16R16");
    case D3DFMT_A2R10G10B10         : return string("D3DFMT_A2R10G10B10");
    case D3DFMT_A16B16G16R16        : return string("D3DFMT_A16B16G16R16");
    case D3DFMT_A8P8                : return string("D3DFMT_A8P8");
    case D3DFMT_P8                  : return string("D3DFMT_P8");
    case D3DFMT_L8                  : return string("D3DFMT_L8");
    case D3DFMT_A8L8                : return string("D3DFMT_A8L8");
    case D3DFMT_A4L4                : return string("D3DFMT_A4L4");
    case D3DFMT_V8U8                : return string("D3DFMT_V8U8");
    case D3DFMT_L6V5U5              : return string("D3DFMT_L6V5U5");
    case D3DFMT_X8L8V8U8            : return string("D3DFMT_X8L8V8U8");
    case D3DFMT_Q8W8V8U8            : return string("D3DFMT_Q8W8V8U8");
    case D3DFMT_V16U16              : return string("D3DFMT_V16U16");
    case D3DFMT_A2W10V10U10         : return string("D3DFMT_A2W10V10U10");
    case D3DFMT_UYVY                : return string("D3DFMT_UYVY");
    case D3DFMT_R8G8_B8G8           : return string("D3DFMT_R8G8_B8G8");
    case D3DFMT_YUY2                : return string("D3DFMT_YUY2");
    case D3DFMT_G8R8_G8B8           : return string("D3DFMT_G8R8_G8B8");
    case D3DFMT_DXT1                : return string("D3DFMT_DXT1");
    case D3DFMT_DXT2                : return string("D3DFMT_DXT2");
    case D3DFMT_DXT3                : return string("D3DFMT_DXT3");
    case D3DFMT_DXT4                : return string("D3DFMT_DXT4");
    case D3DFMT_DXT5                : return string("D3DFMT_DXT5");
    case D3DFMT_D16_LOCKABLE        : return string("D3DFMT_D16_LOCKABLE");
    case D3DFMT_D32                 : return string("D3DFMT_D32");
    case D3DFMT_D15S1               : return string("D3DFMT_D15S1");
    case D3DFMT_D24S8               : return string("D3DFMT_D24S8");
    case D3DFMT_D24X8               : return string("D3DFMT_D24X8");
    case D3DFMT_D24X4S4             : return string("D3DFMT_D24X4S4");
    case D3DFMT_D16                 : return string("D3DFMT_D16");
    case D3DFMT_D32F_LOCKABLE       : return string("D3DFMT_D32F_LOCKABLE");
    case D3DFMT_D24FS8              : return string("D3DFMT_D24FS8");
    case D3DFMT_D32_LOCKABLE        : return string("D3DFMT_D32_LOCKABLE");
    case D3DFMT_S8_LOCKABLE         : return string("D3DFMT_S8_LOCKABLE");
    case D3DFMT_L16                 : return string("D3DFMT_L16");
    case D3DFMT_VERTEXDATA          : return string("D3DFMT_VERTEXDATA");
    case D3DFMT_INDEX16             : return string("D3DFMT_INDEX16");
    case D3DFMT_INDEX32             : return string("D3DFMT_INDEX32");
    case D3DFMT_Q16W16V16U16        : return string("D3DFMT_Q16W16V16U16");
    case D3DFMT_MULTI2_ARGB8        : return string("D3DFMT_MULTI2_ARGB8");
    case D3DFMT_R16F                : return string("D3DFMT_R16F");
    case D3DFMT_G16R16F             : return string("D3DFMT_G16R16F");
    case D3DFMT_A16B16G16R16F       : return string("D3DFMT_A16B16G16R16F");
    case D3DFMT_R32F                : return string("D3DFMT_R32F");
    case D3DFMT_G32R32F             : return string("D3DFMT_G32R32F");
    case D3DFMT_A32B32G32R32F       : return string("D3DFMT_A32B32G32R32F");
    case D3DFMT_CxV8U8              : return string("D3DFMT_CxV8U8");
    case D3DFMT_A1                  : return string("D3DFMT_A1");
    case D3DFMT_A2B10G10R10_XR_BIAS : return string("D3DFMT_A2B10G10R10_XR_BIAS");
    case D3DFMT_BINARYBUFFER        : return string("D3DFMT_BINARYBUFFER");
    }

    return IntString(format);
}

static string GetD3D9MultiSampleTypeName(DWORD mst)
{
    switch(mst) {
    case D3DMULTISAMPLE_NONE        : return string("D3DMULTISAMPLE_NONE");
    case D3DMULTISAMPLE_NONMASKABLE : return string("D3DMULTISAMPLE_NONMASKABLE");
    case D3DMULTISAMPLE_2_SAMPLES   : return string("D3DMULTISAMPLE_2_SAMPLES");
    case D3DMULTISAMPLE_3_SAMPLES   : return string("D3DMULTISAMPLE_3_SAMPLES");
    case D3DMULTISAMPLE_4_SAMPLES   : return string("D3DMULTISAMPLE_4_SAMPLES");
    case D3DMULTISAMPLE_5_SAMPLES   : return string("D3DMULTISAMPLE_5_SAMPLES");
    case D3DMULTISAMPLE_6_SAMPLES   : return string("D3DMULTISAMPLE_6_SAMPLES");
    case D3DMULTISAMPLE_7_SAMPLES   : return string("D3DMULTISAMPLE_7_SAMPLES");
    case D3DMULTISAMPLE_8_SAMPLES   : return string("D3DMULTISAMPLE_8_SAMPLES");
    case D3DMULTISAMPLE_9_SAMPLES   : return string("D3DMULTISAMPLE_9_SAMPLES");
    case D3DMULTISAMPLE_10_SAMPLES  : return string("D3DMULTISAMPLE_10_SAMPLES");
    case D3DMULTISAMPLE_11_SAMPLES  : return string("D3DMULTISAMPLE_11_SAMPLES");
    case D3DMULTISAMPLE_12_SAMPLES  : return string("D3DMULTISAMPLE_12_SAMPLES");
    case D3DMULTISAMPLE_13_SAMPLES  : return string("D3DMULTISAMPLE_13_SAMPLES");
    case D3DMULTISAMPLE_14_SAMPLES  : return string("D3DMULTISAMPLE_14_SAMPLES");
    case D3DMULTISAMPLE_15_SAMPLES  : return string("D3DMULTISAMPLE_15_SAMPLES");
    case D3DMULTISAMPLE_16_SAMPLES  : return string("D3DMULTISAMPLE_16_SAMPLES");
    }

    return IntString(mst);
}

static string GetD3D9SwapEffectName(DWORD se)
{
    switch(se) {
    case  D3DSWAPEFFECT_DISCARD: return string("D3DSWAPEFFECT_DISCARD");
    case  D3DSWAPEFFECT_FLIP   : return string("D3DSWAPEFFECT_FLIP");
    case  D3DSWAPEFFECT_COPY   : return string("D3DSWAPEFFECT_COPY");
    case  D3DSWAPEFFECT_OVERLAY: return string("D3DSWAPEFFECT_OVERLAY");
    case  D3DSWAPEFFECT_FLIPEX : return string("D3DSWAPEFFECT_FLIPEX");
    }

    return IntString(se);
}

static string GetD3D9D3DPPFlagsString(DWORD flags)
{
    if (!flags)
        return string("None");

    stringstream ss;
    if (flags & D3DPRESENTFLAG_LOCKABLE_BACKBUFFER) {
        ss << "D3DPRESENTFLAG_LOCKABLE_BACKBUFFER ";
        flags &= ~D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    }
    if (flags & D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL) {
        ss << "D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL ";
        flags &= ~D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL;
    }
    if (flags & D3DPRESENTFLAG_DEVICECLIP) {
        ss << "D3DPRESENTFLAG_DEVICECLIP ";
        flags &= ~D3DPRESENTFLAG_DEVICECLIP;
    }
    if (flags & D3DPRESENTFLAG_VIDEO) {
        ss << "D3DPRESENTFLAG_VIDEO ";
        flags &= ~D3DPRESENTFLAG_VIDEO;
    }
    if (flags & D3DPRESENTFLAG_NOAUTOROTATE) {
        ss << "D3DPRESENTFLAG_NOAUTOROTATE ";
        flags &= ~D3DPRESENTFLAG_NOAUTOROTATE;
    }
    if (flags & D3DPRESENTFLAG_UNPRUNEDMODE) {
        ss << "D3DPRESENTFLAG_UNPRUNEDMODE ";
        flags &= ~D3DPRESENTFLAG_UNPRUNEDMODE;
    }
    if (flags & D3DPRESENTFLAG_OVERLAY_LIMITEDRGB) {
        ss << "D3DPRESENTFLAG_OVERLAY_LIMITEDRGB ";
        flags &= ~D3DPRESENTFLAG_OVERLAY_LIMITEDRGB;
    }
    if (flags & D3DPRESENTFLAG_OVERLAY_YCbCr_BT709) {
        ss << "D3DPRESENTFLAG_OVERLAY_YCbCr_BT709 ";
        flags &= ~D3DPRESENTFLAG_OVERLAY_YCbCr_BT709;
    }
    if (flags & D3DPRESENTFLAG_OVERLAY_YCbCr_xvYCC) {
        ss << "D3DPRESENTFLAG_OVERLAY_YCbCr_xvYCC ";
        flags &= ~D3DPRESENTFLAG_OVERLAY_YCbCr_xvYCC;
    }
    if (flags & D3DPRESENTFLAG_RESTRICTED_CONTENT) {
        ss << "D3DPRESENTFLAG_RESTRICTED_CONTENT ";
        flags &= ~D3DPRESENTFLAG_RESTRICTED_CONTENT;
    }
    if (flags & D3DPRESENTFLAG_RESTRICT_SHARED_RESOURCE_DRIVER) {
        ss << "D3DPRESENTFLAG_RESTRICT_SHARED_RESOURCE_DRIVER ";
        flags &= ~D3DPRESENTFLAG_RESTRICT_SHARED_RESOURCE_DRIVER;
    }
    if (flags)
        ss << IntString(flags) << " ";

    return ss.str();
}

static string GetD3D9ResourceTypeName(DWORD rt)
{
    switch(rt) {
    case  D3DRTYPE_SURFACE      : return string("D3DRTYPE_SURFACE");
    case  D3DRTYPE_VOLUME       : return string("D3DRTYPE_VOLUME");
    case  D3DRTYPE_TEXTURE      : return string("D3DRTYPE_TEXTURE");
    case  D3DRTYPE_VOLUMETEXTURE: return string("D3DRTYPE_VOLUMETEXTURE");
    case  D3DRTYPE_CUBETEXTURE  : return string("D3DRTYPE_CUBETEXTURE");
    case  D3DRTYPE_VERTEXBUFFER : return string("D3DRTYPE_VERTEXBUFFER");
    case  D3DRTYPE_INDEXBUFFER  : return string("D3DRTYPE_INDEXBUFFER");
    }

    return IntString(rt);
}

static string GetD3D9PoolName(DWORD pool)
{
    switch(pool) {
    case  D3DPOOL_DEFAULT  : return string("D3DPOOL_DEFAULT");
    case  D3DPOOL_MANAGED  : return string("D3DPOOL_MANAGED");
    case  D3DPOOL_SYSTEMMEM: return string("D3DPOOL_SYSTEMMEM");
    case  D3DPOOL_SCRATCH  : return string("D3DPOOL_SCRATCH");
    }

    return IntString(pool);
}

static string GetD3D9UsageString(DWORD flags)
{
    if (!flags)
        return string("None");

    stringstream ss;
    if (flags & D3DUSAGE_RENDERTARGET) {
        ss << "D3DUSAGE_RENDERTARGET ";
        flags &= ~D3DUSAGE_RENDERTARGET;
    }
    if (flags & D3DUSAGE_DEPTHSTENCIL) {
        ss << "D3DUSAGE_DEPTHSTENCIL ";
        flags &= ~D3DUSAGE_DEPTHSTENCIL;
    }
    if (flags & D3DUSAGE_DYNAMIC) {
        ss << "D3DUSAGE_DYNAMIC ";
        flags &= ~D3DUSAGE_DYNAMIC;
    }
    if (flags & D3DUSAGE_NONSECURE) {
        ss << "D3DUSAGE_NONSECURE ";
        flags &= ~D3DUSAGE_NONSECURE;
    }
    if (flags & D3DUSAGE_AUTOGENMIPMAP) {
        ss << "D3DUSAGE_AUTOGENMIPMAP ";
        flags &= ~D3DUSAGE_AUTOGENMIPMAP;
    }
    if (flags & D3DUSAGE_DMAP) {
        ss << "D3DUSAGE_DMAP ";
        flags &= ~D3DUSAGE_DMAP;
    }
    if (flags & D3DUSAGE_QUERY_LEGACYBUMPMAP) {
        ss << "D3DUSAGE_QUERY_LEGACYBUMPMAP ";
        flags &= ~D3DUSAGE_QUERY_LEGACYBUMPMAP;
    }
    if (flags & D3DUSAGE_QUERY_SRGBREAD) {
        ss << "D3DUSAGE_QUERY_SRGBREAD ";
        flags &= ~D3DUSAGE_QUERY_SRGBREAD;
    }
    if (flags & D3DUSAGE_QUERY_FILTER) {
        ss << "D3DUSAGE_QUERY_FILTER ";
        flags &= ~D3DUSAGE_QUERY_FILTER;
    }
    if (flags & D3DUSAGE_QUERY_SRGBWRITE) {
        ss << "D3DUSAGE_QUERY_SRGBWRITE ";
        flags &= ~D3DUSAGE_QUERY_SRGBWRITE;
    }
    if (flags & D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING) {
        ss << "D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING ";
        flags &= ~D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING;
    }
    if (flags & D3DUSAGE_QUERY_VERTEXTEXTURE) {
        ss << "D3DUSAGE_QUERY_VERTEXTEXTURE ";
        flags &= ~D3DUSAGE_QUERY_VERTEXTEXTURE;
    }
    if (flags & D3DUSAGE_QUERY_WRAPANDMIP) {
        ss << "D3DUSAGE_QUERY_WRAPANDMIP ";
        flags &= ~D3DUSAGE_QUERY_WRAPANDMIP;
    }
    if (flags & D3DUSAGE_DYNAMIC) {
        ss << "D3DUSAGE_DYNAMIC ";
        flags &= ~D3DUSAGE_DYNAMIC;
    }
    if (flags & D3DUSAGE_WRITEONLY) {
        ss << "D3DUSAGE_WRITEONLY ";
        flags &= ~D3DUSAGE_WRITEONLY;
    }
    if (flags & D3DUSAGE_SOFTWAREPROCESSING) {
        ss << "D3DUSAGE_SOFTWAREPROCESSING ";
        flags &= ~D3DUSAGE_SOFTWAREPROCESSING;
    }
    if (flags & D3DUSAGE_DONOTCLIP) {
        ss << "D3DUSAGE_DONOTCLIP ";
        flags &= ~D3DUSAGE_DONOTCLIP;
    }
    if (flags & D3DUSAGE_POINTS) {
        ss << "D3DUSAGE_POINTS ";
        flags &= ~D3DUSAGE_POINTS;
    }
    if (flags & D3DUSAGE_RTPATCHES) {
        ss << "D3DUSAGE_RTPATCHES ";
        flags &= ~D3DUSAGE_RTPATCHES;
    }
    if (flags & D3DUSAGE_NPATCHES) {
        ss << "D3DUSAGE_NPATCHES ";
        flags &= ~D3DUSAGE_NPATCHES;
    }
    if (flags & D3DUSAGE_TEXTAPI) {
        ss << "D3DUSAGE_TEXTAPI ";
        flags &= ~D3DUSAGE_TEXTAPI;
    }
    if (flags & D3DUSAGE_RESTRICTED_CONTENT) {
        ss << "D3DUSAGE_RESTRICTED_CONTENT ";
        flags &= ~D3DUSAGE_RESTRICTED_CONTENT;
    }
    if (flags & D3DUSAGE_RESTRICT_SHARED_RESOURCE) {
        ss << "D3DUSAGE_RESTRICT_SHARED_RESOURCE ";
        flags &= ~D3DUSAGE_RESTRICT_SHARED_RESOURCE;
    }
    if (flags & D3DUSAGE_RESTRICT_SHARED_RESOURCE_DRIVER) {
        ss << "D3DUSAGE_RESTRICT_SHARED_RESOURCE_DRIVER ";
        flags &= ~D3DUSAGE_RESTRICT_SHARED_RESOURCE_DRIVER;
    }
    if (flags)
        ss << IntString(flags) << " ";

    return ss.str();
}

void LogPresentParams(D3DPRESENT_PARAMETERS &pp)
{
    string strTime = CurrentTimeString();

    /*if (pp.hDeviceWindow) {
        UINT testLen = GetWindowTextLength(pp.hDeviceWindow);
        testLen++;

        string strName;
        strName.resize(testLen);
        GetWindowTextA(pp.hDeviceWindow, (char*)strName.c_str(), testLen);

        logOutput << CurrentTimeString() << "found d3d9 present params for window: " << strName << endl;
    }*/

    logOutput << strTime << "D3DPRESENT_PARAMETERS {" << endl;
    logOutput << strTime << "\t" "BackBufferWidth: " << pp.BackBufferWidth << endl;
    logOutput << strTime << "\t" "BackBufferHeight: " << pp.BackBufferHeight << endl;
    logOutput << strTime << "\t" "BackBufferFormat: " << GetD3D9FormatName(pp.BackBufferFormat) << endl;
    logOutput << strTime << "\t" "BackBufferCount: " << pp.BackBufferCount << endl;
    logOutput << strTime << "\t" "MultiSampleType: " << GetD3D9MultiSampleTypeName(pp.MultiSampleType) << endl;
    logOutput << strTime << "\t" "MultiSampleQuality: " << pp.MultiSampleQuality << endl;
    logOutput << strTime << "\t" "SwapEffect: " << GetD3D9SwapEffectName(pp.SwapEffect) << endl;
    logOutput << strTime << "\t" "hDeviceWindow: " << DWORD(pp.hDeviceWindow) << endl;
    logOutput << strTime << "\t" "Windowed: " << (pp.Windowed ? "true" : "false") << endl;
    logOutput << strTime << "\t" "EnableAutoDepthStencil: " << (pp.EnableAutoDepthStencil ? "true" : "false") << endl;
    logOutput << strTime << "\t" "AutoDepthStencilFormat: " << GetD3D9FormatName(pp.AutoDepthStencilFormat) << endl;
    logOutput << strTime << "\t" "Flags: " << GetD3D9D3DPPFlagsString(pp.Flags) << endl;
    logOutput << strTime << "\t" "FullScreen_RefreshRateInHz: " << pp.FullScreen_RefreshRateInHz << endl;
    logOutput << strTime << "\t" "PresentationInterval: " << pp.PresentationInterval << endl;
    logOutput << strTime << "};" << endl;
}

void LogD3D9SurfaceInfo(IDirect3DSurface9 *surf)
{
    if (!surf) return;

    string strTime = CurrentTimeString();

    D3DSURFACE_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    if (SUCCEEDED(surf->GetDesc(&sd))) {
        logOutput << strTime << "D3DSURFACE_DESC {" << endl;
        logOutput << strTime << "\t" "Format: " << GetD3D9FormatName(sd.Format) << endl;
        logOutput << strTime << "\t" "Type: " << GetD3D9ResourceTypeName(sd.Type) << endl;
        logOutput << strTime << "\t" "Usage: " << GetD3D9UsageString(sd.Usage) << endl;
        logOutput << strTime << "\t" "Pool: " << GetD3D9PoolName(sd.Pool) << endl;
        logOutput << strTime << "\t" "MultiSampleType: " << GetD3D9MultiSampleTypeName(sd.MultiSampleType) << endl;
        logOutput << strTime << "\t" "MultiSampleQuality: " << sd.MultiSampleQuality << endl;
        logOutput << strTime << "\t" "Width: " << sd.Width << endl;
        logOutput << strTime << "\t" "Height: " << sd.Height << endl;
        logOutput << strTime << "};" << endl;
    } else {
        logOutput << strTime << "could not get D3DSURFACE_DESC from backbuffer for some reason" << endl;
    }
}
