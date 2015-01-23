/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/


#include "Main.h"


inline bool IsPow2(UINT num)
{
    return num >= 2 && (num & (num-1)) == 0;
}


const DXGI_FORMAT convertFormat[] = {DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_A8_UNORM, DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_B8G8R8X8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_B5G5R5A1_UNORM, DXGI_FORMAT_B5G6R5_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_BC3_UNORM, DXGI_FORMAT_BC5_UNORM};
const UINT formatPitch[] = {0, 1, 1, 4, 4, 4, 4, 8, 16, 0, 0, 0};

const D3D10_TEXTURE_ADDRESS_MODE convertAddressMode[] = {D3D10_TEXTURE_ADDRESS_CLAMP, D3D10_TEXTURE_ADDRESS_WRAP, D3D10_TEXTURE_ADDRESS_MIRROR, D3D10_TEXTURE_ADDRESS_BORDER, D3D10_TEXTURE_ADDRESS_MIRROR_ONCE};
const D3D10_FILTER convertFilter[] = {D3D10_FILTER_MIN_MAG_MIP_LINEAR, D3D10_FILTER_MIN_MAG_MIP_POINT, D3D10_FILTER_ANISOTROPIC, D3D10_FILTER_MIN_MAG_POINT_MIP_LINEAR, D3D10_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT, D3D10_FILTER_MIN_POINT_MAG_MIP_LINEAR, D3D10_FILTER_MIN_LINEAR_MAG_MIP_POINT, D3D10_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR, D3D10_FILTER_MIN_MAG_LINEAR_MIP_POINT};

inline GSColorFormat GetGSFormatFromDXGIFormat(DXGI_FORMAT dxgiFormat)
{
    UINT numVals = sizeof(convertFormat)/sizeof(DXGI_FORMAT);

    for (UINT i=0; i<numVals; i++) {
        if (convertFormat[i] == dxgiFormat)
            return (GSColorFormat)i;
    }

    return GS_UNKNOWNFORMAT;
}

SamplerState* D3D10SamplerState::CreateSamplerState(SamplerInfo &info)
{
    D3D10_SAMPLER_DESC sampDesc;
    zero(&sampDesc, sizeof(sampDesc));
    sampDesc.AddressU       = convertAddressMode[(UINT)info.addressU];
    sampDesc.AddressV       = convertAddressMode[(UINT)info.addressV];
    sampDesc.AddressW       = convertAddressMode[(UINT)info.addressW];
    sampDesc.ComparisonFunc = D3D10_COMPARISON_ALWAYS;
    sampDesc.Filter         = convertFilter[(UINT)info.filter];
    sampDesc.MaxAnisotropy  = 1;//info.maxAnisotropy;
    sampDesc.MaxLOD         = FLT_MAX;
    mcpy(sampDesc.BorderColor, info.borderColor.ptr, sizeof(Color4));

    ID3D10SamplerState *state;
    HRESULT err = GetD3D()->CreateSamplerState(&sampDesc, &state);
    if(FAILED(err))
    {
        AppWarning(TEXT("D3D10SamplerState::CreateSamplerState: unable to create sampler state, result = %08lX"), err);
        return NULL;
    }

    //-------------------------------------------

    D3D10SamplerState *samplerState = new D3D10SamplerState;
    samplerState->state = state;
    mcpy(&samplerState->info, &info, sizeof(SamplerInfo));

    return samplerState;
}

D3D10SamplerState::~D3D10SamplerState()
{
    SafeRelease(state);
}


Texture* D3D10Texture::CreateFromSharedHandle(unsigned int width, unsigned int height, HANDLE handle)
{
    HRESULT err;

    if(!handle)
    {
        AppWarning(TEXT("D3D10Texture::CreateFromSharedHandle: NULL handle value."));
        return NULL;
    }

    ID3D10Resource *tempResource;
    if(FAILED(err = GetD3D()->OpenSharedResource(handle, __uuidof(ID3D10Resource), (void**)&tempResource)))
    {
        AppWarning(TEXT("D3D10Texture::CreateFromSharedHandle: Failed to open shared handle, result = 0x%08lX"), err);
        return NULL;
    }

    ID3D10Texture2D *texVal;
    if(FAILED(err = tempResource->QueryInterface(__uuidof(ID3D10Texture2D), (void**)&texVal)))
    {
        SafeRelease(tempResource);
        AppWarning(TEXT("D3D10Texture::CreateFromSharedHandle: could not query interface, result = 0x%08lX"), err);
        return NULL;
    }

    tempResource->Release();

    //------------------------------------------

    D3D10_TEXTURE2D_DESC td;
    texVal->GetDesc(&td);

    //------------------------------------------

    D3D10_SHADER_RESOURCE_VIEW_DESC resourceDesc;
    zero(&resourceDesc, sizeof(resourceDesc));
    resourceDesc.Format              = td.Format;
    resourceDesc.ViewDimension       = D3D10_SRV_DIMENSION_TEXTURE2D;
    resourceDesc.Texture2D.MipLevels = 1;

    ID3D10ShaderResourceView *resource = NULL;
    if(FAILED(err = GetD3D()->CreateShaderResourceView(texVal, &resourceDesc, &resource)))
    {
        SafeRelease(texVal);
        AppWarning(TEXT("D3D10Texture::CreateFromSharedHandle: CreateShaderResourceView failed, result = 0x%08lX"), err);
        return NULL;
    }

    //------------------------------------------

    D3D10Texture *newTex = new D3D10Texture;
    newTex->format = GetGSFormatFromDXGIFormat(td.Format);;
    newTex->resource = resource;
    newTex->texture = texVal;
    newTex->bDynamic = false;
    newTex->width = width;
    newTex->height = height;

    return newTex;
}

Texture* D3D10Texture::CreateTexture(unsigned int width, unsigned int height, GSColorFormat colorFormat, void *lpData, BOOL bGenMipMaps, BOOL bStatic)
{
    HRESULT err;

    if(colorFormat >= GS_DXT1)
    {
        AppWarning(TEXT("D3D10Texture::CreateTexture: tried to create a blank DXT texture.  Use CreateFromFile instead."));
        return NULL;
    }

    DXGI_FORMAT format = convertFormat[(UINT)colorFormat];

    if(bGenMipMaps && (!IsPow2(width) || !IsPow2(height)))
    {
        AppWarning(TEXT("D3D10Texture::CreateTexture: Cannot generate mipmaps for a non-power-of-two sized texture.  Disabling mipmap generation."));
        bGenMipMaps = FALSE;
    }

    D3D10_TEXTURE2D_DESC td;
    zero(&td, sizeof(td));
    td.Width            = width;
    td.Height           = height;
    td.MipLevels        = bGenMipMaps ? 0 : 1;
    td.ArraySize        = 1;
    td.Format           = format;
    td.BindFlags        = D3D10_BIND_SHADER_RESOURCE;
    td.SampleDesc.Count = 1;
    td.Usage            = bStatic ? D3D10_USAGE_DEFAULT : D3D10_USAGE_DYNAMIC;
    td.CPUAccessFlags   = bStatic ? 0 : D3D10_CPU_ACCESS_WRITE;

    D3D10_SUBRESOURCE_DATA srd;
    D3D10_SUBRESOURCE_DATA *lpSRD;
    if(lpData)
    {
        srd.pSysMem = lpData;
        srd.SysMemPitch = width*formatPitch[(UINT)colorFormat];
        srd.SysMemSlicePitch = 0;
        lpSRD = &srd;
    }
    else
        lpSRD = NULL;

    ID3D10Texture2D *texVal;
    if(FAILED(err = GetD3D()->CreateTexture2D(&td, lpSRD, &texVal)))
    {
        AppWarning(TEXT("D3D10Texture::CreateTexture: CreateTexture2D failed, result = 0x%08lX"), err);
        return NULL;
    }

    //------------------------------------------

    D3D10_SHADER_RESOURCE_VIEW_DESC resourceDesc;
    zero(&resourceDesc, sizeof(resourceDesc));
    resourceDesc.Format              = format;
    resourceDesc.ViewDimension       = D3D10_SRV_DIMENSION_TEXTURE2D;
    resourceDesc.Texture2D.MipLevels = bGenMipMaps ? -1 : 1;

    ID3D10ShaderResourceView *resource;
    if(FAILED(err = GetD3D()->CreateShaderResourceView(texVal, &resourceDesc, &resource)))
    {
        SafeRelease(texVal);
        AppWarning(TEXT("D3D10Texture::CreateTexture: CreateShaderResourceView failed, result = 0x%08lX"), err);
        return NULL;
    }

    //------------------------------------------

    D3D10Texture *newTex = new D3D10Texture;
    newTex->format = colorFormat;
    newTex->resource = resource;
    newTex->texture = texVal;
    newTex->bDynamic = !bStatic;
    newTex->width = width;
    newTex->height = height;

    return newTex;
}

Texture* D3D10Texture::CreateFromFile(CTSTR lpFile, BOOL bBuildMipMaps)
{
    HRESULT err;

    D3DX10_IMAGE_INFO ii;
    if(FAILED(D3DX10GetImageInfoFromFile(lpFile, NULL, &ii, NULL)))
    {
        AppWarning(TEXT("D3D10Texture::CreateFromFile: Could not get information about texture file '%s'"), lpFile);
        return NULL;
    }

    //------------------------------------------

    if(bBuildMipMaps && (!IsPow2(ii.Width) || !IsPow2(ii.Height)))
        bBuildMipMaps = FALSE;

    D3DX10_IMAGE_LOAD_INFO ili;
    ili.Width           = D3DX10_DEFAULT;
    ili.Height          = D3DX10_DEFAULT;
    ili.Depth           = D3DX10_DEFAULT;
    ili.FirstMipLevel   = D3DX10_DEFAULT;
    ili.MipLevels       = bBuildMipMaps ? 0 : 1;
    ili.Usage           = (D3D10_USAGE)D3DX10_DEFAULT;
    ili.BindFlags       = D3DX10_DEFAULT;
    ili.CpuAccessFlags  = D3DX10_DEFAULT;
    ili.MiscFlags       = D3DX10_DEFAULT;
    ili.Format          = (DXGI_FORMAT)D3DX10_DEFAULT;
    ili.Filter          = D3DX10_DEFAULT;
    ili.MipFilter       = D3DX10_DEFAULT;
    ili.pSrcInfo        = NULL;

    ID3D10Resource *texResource;
    if(FAILED(err = D3DX10CreateTextureFromFile(GetD3D(), lpFile, &ili, NULL, &texResource, NULL)))
    {
        AppWarning(TEXT("D3D10Texture::CreateFromFile: failed to load '%s'"), lpFile);
        return NULL;
    }

    //------------------------------------------

    D3D10_SHADER_RESOURCE_VIEW_DESC resourceDesc;
    zero(&resourceDesc, sizeof(resourceDesc));
    resourceDesc.Format              = ii.Format;
    resourceDesc.ViewDimension       = D3D10_SRV_DIMENSION_TEXTURE2D;
    resourceDesc.Texture2D.MipLevels = bBuildMipMaps ? -1 : 1;

    ID3D10ShaderResourceView *resource;
    if(FAILED(err = GetD3D()->CreateShaderResourceView(texResource, &resourceDesc, &resource)))
    {
        SafeRelease(texResource);
        AppWarning(TEXT("D3D10Texture::CreateFromFile: CreateShaderResourceView failed, result = 0x%08lX"), err);
        return NULL;
    }

    //------------------------------------------

    ID3D10Texture2D *tex2D;
    err = texResource->QueryInterface(__uuidof(ID3D10Texture2D), (void**)&tex2D);
    if(FAILED(err))
    {
        SafeRelease(texResource);
        SafeRelease(resource);
        AppWarning(TEXT("D3D10Texture::CreateFromFile: could not query texture interface"));
        return NULL;
    }

    texResource->Release();

    //------------------------------------------

    D3D10Texture *newTex = new D3D10Texture;
    newTex->resource = resource;
    newTex->texture = tex2D;
    newTex->width = ii.Width;
    newTex->height = ii.Height;

    switch(ii.Format)
    {
        case DXGI_FORMAT_R8_UNORM:              newTex->format = GS_ALPHA;       break;
        case DXGI_FORMAT_A8_UNORM:              newTex->format = GS_GRAYSCALE;   break;
        case DXGI_FORMAT_B8G8R8X8_UNORM:        newTex->format = GS_BGR;         break;
        case DXGI_FORMAT_B8G8R8A8_UNORM:        newTex->format = GS_BGRA;        break;
        case DXGI_FORMAT_R8G8B8A8_UNORM:        newTex->format = GS_RGBA;        break;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:    newTex->format = GS_RGBA16F;     break;
        case DXGI_FORMAT_R32G32B32A32_FLOAT:    newTex->format = GS_RGBA32F;     break;
        case DXGI_FORMAT_BC1_UNORM:             newTex->format = GS_DXT1;        break;
        case DXGI_FORMAT_BC2_UNORM:             newTex->format = GS_DXT3;        break;
        case DXGI_FORMAT_BC3_UNORM:             newTex->format = GS_DXT5;        break;
        default:
            newTex->format = GS_UNKNOWNFORMAT;
    }

    return newTex;
}

Texture* D3D10Texture::CreateRenderTarget(unsigned int width, unsigned int height, GSColorFormat colorFormat, BOOL bGenMipMaps)
{
    HRESULT err;

    if(colorFormat >= GS_DXT1)
    {
        AppWarning(TEXT("D3D10Texture::CreateRenderTarget: tried to a blank DXT render target"));
        return NULL;
    }

    DXGI_FORMAT format = convertFormat[(UINT)colorFormat];

    D3D10_TEXTURE2D_DESC td;
    zero(&td, sizeof(td));
    td.Width            = width;
    td.Height           = height;
    td.MipLevels        = bGenMipMaps ? 0 : 1;
    td.ArraySize        = 1;
    td.Format           = format;
    td.BindFlags        = D3D10_BIND_SHADER_RESOURCE|D3D10_BIND_RENDER_TARGET;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D10_USAGE_DEFAULT;

    ID3D10Texture2D *texVal;
    if(FAILED(err = GetD3D()->CreateTexture2D(&td, NULL, &texVal)))
    {
        AppWarning(TEXT("D3D10Texture::CreateRenderTarget: CreateTexture2D failed, result = 0x%08lX"), err);
        return NULL;
    }

    //------------------------------------------

    D3D10_SHADER_RESOURCE_VIEW_DESC resourceDesc;
    zero(&resourceDesc, sizeof(resourceDesc));
    resourceDesc.Format              = format;
    resourceDesc.ViewDimension       = D3D10_SRV_DIMENSION_TEXTURE2D;
    resourceDesc.Texture2D.MipLevels = bGenMipMaps ? -1 : 1;

    ID3D10ShaderResourceView *resource;
    if(FAILED(err = GetD3D()->CreateShaderResourceView(texVal, &resourceDesc, &resource)))
    {
        SafeRelease(texVal);
        AppWarning(TEXT("D3D10Texture::CreateRenderTarget: CreateShaderResourceView failed, result = 0x%08lX"), err);
        return NULL;
    }

    //------------------------------------------

    ID3D10RenderTargetView *view;
    err = GetD3D()->CreateRenderTargetView(texVal, NULL, &view);
    if(FAILED(err))
    {
        SafeRelease(texVal);
        SafeRelease(resource);
        AppWarning(TEXT("D3D10Texture::CreateRenderTarget: CreateRenderTargetView failed, result = 0x%08lX"), err);
        return NULL;
    }

    //------------------------------------------

    D3D10Texture *newTex = new D3D10Texture;
    newTex->format = colorFormat;
    newTex->resource = resource;
    newTex->texture = texVal;
    newTex->renderTarget = view;
    newTex->width = width;
    newTex->height = height;

    return newTex;
}

Texture* D3D10Texture::CreateGDITexture(unsigned int width, unsigned int height)
{
    HRESULT err;

    D3D10_TEXTURE2D_DESC td;
    zero(&td, sizeof(td));
    td.Width            = width;
    td.Height           = height;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.BindFlags        = D3D10_BIND_SHADER_RESOURCE|D3D10_BIND_RENDER_TARGET;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D10_USAGE_DEFAULT;
    td.MiscFlags        = D3D10_RESOURCE_MISC_GDI_COMPATIBLE;

    D3D10_SUBRESOURCE_DATA srd;
    zero(&srd, sizeof(srd));

    ID3D10Texture2D *texVal;
    if(FAILED(err = GetD3D()->CreateTexture2D(&td, NULL, &texVal)))
    {
        AppWarning(TEXT("D3D10Texture::CreateGDITexture: CreateTexture2D failed, result = 0x%08lX"), err);
        return NULL;
    }

    //------------------------------------------

    D3D10_SHADER_RESOURCE_VIEW_DESC resourceDesc;
    zero(&resourceDesc, sizeof(resourceDesc));
    resourceDesc.Format              = DXGI_FORMAT_B8G8R8A8_UNORM;
    resourceDesc.ViewDimension       = D3D10_SRV_DIMENSION_TEXTURE2D;
    resourceDesc.Texture2D.MipLevels = 1;

    ID3D10ShaderResourceView *resource;
    if(FAILED(err = GetD3D()->CreateShaderResourceView(texVal, &resourceDesc, &resource)))
    {
        SafeRelease(texVal);
        AppWarning(TEXT("D3D10Texture::CreateGDITexture: CreateShaderResourceView failed, result = 0x%08lX"), err);
        return NULL;
    }

    //------------------------------------------

    D3D10Texture *newTex = new D3D10Texture;
    newTex->format = GS_BGRA;
    newTex->resource = resource;
    newTex->texture = texVal;
    newTex->width = width;
    newTex->height = height;
    newTex->bGDICompatible = true;

    return newTex;
}

Texture* D3D10Texture::CreateShared(unsigned int width, unsigned int height)
{
    HRESULT err;

    D3D10_TEXTURE2D_DESC td;
    zero(&td, sizeof(td));
    td.Width            = width;
    td.Height           = height;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.BindFlags        = D3D10_BIND_SHADER_RESOURCE|D3D10_BIND_RENDER_TARGET;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D10_USAGE_DEFAULT;
    td.CPUAccessFlags   = 0;
    td.MiscFlags		= D3D10_RESOURCE_MISC_SHARED;

    ID3D10Texture2D *texVal;
    if(FAILED(err = GetD3D()->CreateTexture2D(&td, NULL, &texVal)))
    {
        AppWarning(TEXT("D3D10Texture::CreateShared: CreateTexture2D failed, result = 0x%08lX"), err);
        return NULL;
    }

    //------------------------------------------

    D3D10_SHADER_RESOURCE_VIEW_DESC resourceDesc;
    zero(&resourceDesc, sizeof(resourceDesc));
    resourceDesc.Format              = DXGI_FORMAT_B8G8R8A8_UNORM;
    resourceDesc.ViewDimension       = D3D10_SRV_DIMENSION_TEXTURE2D;
    resourceDesc.Texture2D.MipLevels = 1;

    ID3D10ShaderResourceView *resource;
    if(FAILED(err = GetD3D()->CreateShaderResourceView(texVal, &resourceDesc, &resource)))
    {
        SafeRelease(texVal);
        AppWarning(TEXT("D3D10Texture::CreateShared: CreateShaderResourceView failed, result = 0x%08lX"), err);
        return NULL;
    }

    //------------------------------------------

    ID3D10RenderTargetView *view;
    err = GetD3D()->CreateRenderTargetView(texVal, NULL, &view);
    if(FAILED(err))
    {
        SafeRelease(texVal);
        SafeRelease(resource);
        AppWarning(TEXT("D3D10Texture::CreateShared: CreateRenderTargetView failed, result = 0x%08lX"), err);
        return NULL;
    }

    //------------------------------------------

    D3D10Texture *newTex = new D3D10Texture;
    newTex->format = GS_BGRA;
    newTex->resource = resource;
    newTex->texture = texVal;
    newTex->renderTarget = view;
    newTex->width = width;
    newTex->height = height;

    return newTex;
}

D3D10Texture::~D3D10Texture()
{
    SafeRelease(renderTarget);
    SafeRelease(resource);
    SafeRelease(texture);
}

DWORD D3D10Texture::Width() const
{
    return width;
}

DWORD D3D10Texture::Height() const
{
    return height;
}

BOOL D3D10Texture::HasAlpha() const
{
    return format == 1 || (format >= GS_RGBA && format <= GS_RGBA32F) || format == GS_DXT3 || format == GS_DXT5;
}

GSColorFormat D3D10Texture::GetFormat() const
{
    return (GSColorFormat)format;
}

bool D3D10Texture::GetDC(HDC &hDC)
{
    if(!bGDICompatible)
    {
        AppWarning(TEXT("D3D10Texture::GetDC: function was called on a non-GDI-compatible texture"));
        return false;
    }

    HRESULT err;
    if(FAILED(err = texture->QueryInterface(__uuidof(IDXGISurface1), (void**)&surface)))
    {
        AppWarning(TEXT("D3D10Texture::GetDC: could not query surface interface, result = %08lX"), err);
        return false;
    }

    if(FAILED(err = surface->GetDC(TRUE, &hDC)))
    {
        AppWarning(TEXT("D3D10Texture::GetDC: could not get DC, result = %08lX"), err);
        SafeRelease(surface);
        return false;
    }

    return true;
}

void D3D10Texture::ReleaseDC()
{
    if(!surface)
    {
        AppWarning(TEXT("D3D10Texture::ReleaseDC: no DC to release"));
        return;
    }

    surface->ReleaseDC(NULL);
    SafeRelease(surface);
}

//====================================================================================

void CopyPackedRGB(BYTE *lpDest, BYTE *lpSource, UINT nPixels)
{
    DWORD curComponent = 0;

    UINT totalBytes = (nPixels*3);
    UINT alignedBytes = totalBytes&0xFFFFFFFC;
    UINT nDWords = alignedBytes>>2;

    DWORD *lpDWDest = (DWORD*)lpDest;
    DWORD *lpDWSrc  = (DWORD*)lpSource;

    while(nDWords)
    {
        switch(curComponent)
        {
            case 0: *(lpDWDest++)  = *lpDWSrc         & 0xFFFFFF; *lpDWDest     = *(lpDWSrc++)>>24; break; //RBGR
            case 1: *(lpDWDest++) |= ((*lpDWSrc)<<8)  & 0xFFFF00; *lpDWDest     = *(lpDWSrc++)>>16; break; //GRBG
            case 2: *(lpDWDest++) |= ((*lpDWSrc)<<16) & 0xFFFF00; *(lpDWDest++) = *(lpDWSrc++)>>8;  break; //BGRB
        }

        if(curComponent == 2)
            curComponent = 0;
        else
            curComponent++;

        nDWords--;
    }

    totalBytes -= alignedBytes;
    lpSource = (LPBYTE)lpDWSrc;
    lpDest   = (LPBYTE)lpDWDest;

    if(curComponent != 0)
        lpDest += curComponent;

    while(totalBytes--)
    {
        *(lpDest++) = *(lpSource++);

        if(curComponent == 2)
        {
            *(lpDest++) = 0;
            curComponent = 0;
        }
        else
            curComponent++;
    }
}

void D3D10Texture::SetImage(void *lpData, GSImageFormat imageFormat, UINT pitch)
{
    if(!bDynamic)
    {
        AppWarning(TEXT("3D11Texture::SetImage: cannot call on a non-dynamic texture"));
        return;
    }

    bool bMatchingFormat = false;
    UINT pixelBytes = 0;    //What is this needed for?

    switch(format)
    {
        case GS_ALPHA:      bMatchingFormat = (imageFormat == GS_IMAGEFORMAT_A8); pixelBytes = 1; break;
        case GS_GRAYSCALE:  bMatchingFormat = (imageFormat == GS_IMAGEFORMAT_L8); pixelBytes = 1; break;
        case GS_RGB:        bMatchingFormat = (imageFormat == GS_IMAGEFORMAT_RGB || imageFormat == GS_IMAGEFORMAT_RGBX); pixelBytes = 4; break;
        case GS_RGBA:       bMatchingFormat = (imageFormat == GS_IMAGEFORMAT_RGBA); pixelBytes = 4; break;
        case GS_BGR:        bMatchingFormat = (imageFormat == GS_IMAGEFORMAT_BGR || imageFormat == GS_IMAGEFORMAT_BGRX); pixelBytes = 4; break;
        case GS_BGRA:       bMatchingFormat = (imageFormat == GS_IMAGEFORMAT_BGRA); pixelBytes = 4; break;
        case GS_RGBA16F:    bMatchingFormat = (imageFormat == GS_IMAGEFORMAT_RGBA16F); pixelBytes = 8; break;
        case GS_RGBA32F:    bMatchingFormat = (imageFormat == GS_IMAGEFORMAT_RGBA32F); pixelBytes = 16; break;
    }

    if(!bMatchingFormat)
    {
        AppWarning(TEXT("D3D10Texture::SetImage: invalid or mismatching image format specified"));
        return;
    }

    HRESULT err;

    D3D10_MAPPED_TEXTURE2D map;
    if(FAILED(err = texture->Map(0, D3D10_MAP_WRITE_DISCARD, 0, &map)))
    {
        AppWarning(TEXT("D3D10Texture::SetImage: map failed, result = %08lX"), err);
        return;
    }

    //-------------------------------------------------------------------------
    if((format == GS_RGB || format == GS_BGR) && (imageFormat == GS_IMAGEFORMAT_BGR || imageFormat == GS_IMAGEFORMAT_RGB))
    {
        if(pitch == (width*3) && map.RowPitch == (width*4))
            CopyPackedRGB((BYTE*)map.pData, (BYTE*)lpData, width*height);
        else
        {
            for(UINT y=0; y<height; y++)
            {
                LPBYTE curInput  = ((LPBYTE)lpData)    + (pitch*y);
                LPBYTE curOutput = ((LPBYTE)map.pData) + (map.RowPitch*y);

                CopyPackedRGB(curOutput, curInput, width);
            }
        }
    }
    //-------------------------------------------------------------------------
    else
    {
        if(pitch == map.RowPitch)
            mcpy(map.pData, lpData, pitch*height);
        else
        {
            UINT bestPitch = MIN(pitch, map.RowPitch);

            for(UINT y=0; y<height; y++)
            {
                LPBYTE curInput  = ((LPBYTE)lpData)    + (pitch*y);
                LPBYTE curOutput = ((LPBYTE)map.pData) + (map.RowPitch*y);

                mcpy(curOutput, curInput, bestPitch);
            }
        }
    }

    texture->Unmap(0);
}

bool D3D10Texture::Map(BYTE *&lpData, UINT &pitch)
{
    HRESULT err;
    D3D10_MAPPED_TEXTURE2D map;

    if(FAILED(err = texture->Map(0, D3D10_MAP_WRITE_DISCARD, 0, &map)))
    {
        AppWarning(TEXT("D3D10Texture::Map: map failed, result = %08lX"), err);
        return false;
    }

    lpData = (BYTE*)map.pData;
    pitch = map.RowPitch;

    return true;
}

void D3D10Texture::Unmap()
{
    texture->Unmap(0);
}

HANDLE D3D10Texture::GetSharedHandle()
{
    HRESULT err;
    HANDLE handle = NULL;
    IDXGIResource *pDXGIResource;
    if FAILED(err = texture->QueryInterface(__uuidof(IDXGIResource), (void **)&pDXGIResource))
    {
        AppWarning(TEXT("D3D10Texture::GetSharedHandle: QueryInterface failed, result = %08lX"), err);
        return handle;
    }
    
    if FAILED(err = pDXGIResource->GetSharedHandle(&handle))
    {
        AppWarning(TEXT("D3D10Texture::GetSharedHandle: GetSharedHandle failed, result = %08lX"), err);
    }
    
    return handle;
}
