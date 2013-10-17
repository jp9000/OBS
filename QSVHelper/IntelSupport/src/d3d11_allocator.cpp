/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 - 2013 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "d3d11_allocator.h"

#if MFX_D3D11_SUPPORT

#include <objbase.h>
#include <initguid.h>
#include <assert.h>
#include <algorithm>
#include <functional>
#include <iterator>
//#include "thread_defs.h"

#define D3DFMT_NV12 (DXGI_FORMAT)MAKEFOURCC('N','V','1','2')
#define D3DFMT_YV12 (DXGI_FORMAT)MAKEFOURCC('Y','V','1','2')

//for generating sequence of mfx handles
template <typename T>
struct sequence {
    T x;
    sequence(T seed) : x(seed) { }
};

template <>
struct sequence<mfxHDL> {
    mfxHDL x;
    sequence(mfxHDL seed) : x(seed) { }

    mfxHDL operator ()()
    {
        mfxHDL y = x;
        x = (mfxHDL)(1 + (size_t)(x));
        return y;
    }
};


D3D11FrameAllocator::D3D11FrameAllocator()
{
    m_pDeviceContext = NULL;
}

D3D11FrameAllocator::~D3D11FrameAllocator()
{
    Close();
}

D3D11FrameAllocator::TextureSubResource  D3D11FrameAllocator::GetResourceFromMid(mfxMemId mid)
{
    size_t index = (size_t)MFXReadWriteMid(mid).raw() - 1;

    if(m_memIdMap.size() <= index)
        return TextureSubResource();

    //reverse iterator dereferencing
    TextureResource * p = &(*m_memIdMap[index]);
    if (!p->bAlloc)
        return TextureSubResource();

    return TextureSubResource(p, mid);
}

mfxStatus D3D11FrameAllocator::Init(mfxAllocatorParams *pParams)
{
    D3D11AllocatorParams *pd3d11Params = 0;
    pd3d11Params = dynamic_cast<D3D11AllocatorParams *>(pParams);

    if (NULL == pd3d11Params ||
        NULL == pd3d11Params->pDevice)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    m_initParams = *pd3d11Params;
    MSDK_SAFE_RELEASE(m_pDeviceContext);
    pd3d11Params->pDevice->GetImmediateContext(&m_pDeviceContext);

    return MFX_ERR_NONE;
}

mfxStatus D3D11FrameAllocator::Close()
{
    mfxStatus sts = BaseFrameAllocator::Close();
    for(referenceType i = m_resourcesByRequest.begin(); i != m_resourcesByRequest.end(); i++)
    {
        i->Release();
    }
    m_resourcesByRequest.clear();
    m_memIdMap.clear();
    MSDK_SAFE_RELEASE(m_pDeviceContext);
    return sts;
}

mfxStatus D3D11FrameAllocator::LockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    HRESULT hRes = S_OK;

    D3D11_TEXTURE2D_DESC desc = {0};
    D3D11_MAPPED_SUBRESOURCE lockedRect = {0};


    //check that texture exists
    TextureSubResource sr = GetResourceFromMid(mid);
    if (!sr.GetTexture())
        return MFX_ERR_LOCK_MEMORY;

    D3D11_MAP mapType = D3D11_MAP_READ;
    UINT mapFlags = D3D11_MAP_FLAG_DO_NOT_WAIT;
    {
        if (NULL == sr.GetStaging())
        {
            hRes = m_pDeviceContext->Map(sr.GetTexture(), sr.GetSubResource(), D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &lockedRect);
            desc.Format = DXGI_FORMAT_P8;
        }
        else
        {
            sr.GetTexture()->GetDesc(&desc);

            if (DXGI_FORMAT_NV12 != desc.Format &&
                DXGI_FORMAT_420_OPAQUE != desc.Format &&
                DXGI_FORMAT_YUY2 != desc.Format &&
                DXGI_FORMAT_P8 != desc.Format &&
                DXGI_FORMAT_B8G8R8A8_UNORM != desc.Format)
            {
                return MFX_ERR_LOCK_MEMORY;
            }

            //coping data only in case user wants to read from stored surface
            {

                if (MFXReadWriteMid(mid, MFXReadWriteMid::reuse).isRead())
                {
                    m_pDeviceContext->CopySubresourceRegion(sr.GetStaging(), 0, 0, 0, 0, sr.GetTexture(), sr.GetSubResource(), NULL);
                }

                do
                {
                    hRes = m_pDeviceContext->Map(sr.GetStaging(), 0, mapType, mapFlags, &lockedRect);
                    if (S_OK != hRes && DXGI_ERROR_WAS_STILL_DRAWING != hRes)
                    {
                        msdk_printf(MSDK_STRING("ERROR: m_pDeviceContext->Map = 0x%08lx\n"), hRes);
                    }
                }
                while (DXGI_ERROR_WAS_STILL_DRAWING == hRes);
            }

        }
    }

    if (FAILED(hRes))
        return MFX_ERR_LOCK_MEMORY;

    switch (desc.Format)
    {
        case DXGI_FORMAT_NV12:
            ptr->Pitch = (mfxU16)lockedRect.RowPitch;
            ptr->Y = (mfxU8 *)lockedRect.pData;
            ptr->U = (mfxU8 *)lockedRect.pData + desc.Height * lockedRect.RowPitch;
            ptr->V = ptr->U + 1;

            break;

        case DXGI_FORMAT_420_OPAQUE: // can be unsupported by standard ms guid
            ptr->Pitch = (mfxU16)lockedRect.RowPitch;
            ptr->Y = (mfxU8 *)lockedRect.pData;
            ptr->V = ptr->Y + desc.Height * lockedRect.RowPitch;
            ptr->U = ptr->V + (desc.Height * lockedRect.RowPitch) / 4;

            break;

        case DXGI_FORMAT_YUY2:
            ptr->Pitch = (mfxU16)lockedRect.RowPitch;
            ptr->Y = (mfxU8 *)lockedRect.pData;
            ptr->U = ptr->Y + 1;
            ptr->V = ptr->Y + 3;

            break;

        case DXGI_FORMAT_P8 :
            ptr->Pitch = (mfxU16)lockedRect.RowPitch;
            ptr->Y = (mfxU8 *)lockedRect.pData;
            ptr->U = 0;
            ptr->V = 0;

            break;

        case DXGI_FORMAT_B8G8R8A8_UNORM :
            ptr->Pitch = (mfxU16)lockedRect.RowPitch;
            ptr->B = (mfxU8 *)lockedRect.pData;
            ptr->G = ptr->B + 1;
            ptr->R = ptr->B + 2;
            ptr->A = ptr->B + 3;

            break;

        default:

            return MFX_ERR_LOCK_MEMORY;
    }

    return MFX_ERR_NONE;
}

mfxStatus D3D11FrameAllocator::UnlockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    //check that texture exists
    TextureSubResource sr = GetResourceFromMid(mid);
    if (!sr.GetTexture())
        return MFX_ERR_LOCK_MEMORY;

    if (NULL == sr.GetStaging())
    {
        m_pDeviceContext->Unmap(sr.GetTexture(), sr.GetSubResource());
    }
    else
    {
        m_pDeviceContext->Unmap(sr.GetStaging(), 0);
        //only if user wrote something to texture
        if (MFXReadWriteMid(mid, MFXReadWriteMid::reuse).isWrite())
        {
            m_pDeviceContext->CopySubresourceRegion(sr.GetTexture(), sr.GetSubResource(), 0, 0, 0, sr.GetStaging(), 0, NULL);
        }
    }

    if (ptr)
    {
        ptr->Pitch=0;
        ptr->U=ptr->V=ptr->Y=0;
        ptr->A=ptr->R=ptr->G=ptr->B=0;
    }

    return MFX_ERR_NONE;
}


mfxStatus D3D11FrameAllocator::GetFrameHDL(mfxMemId mid, mfxHDL *handle)
{
    if (NULL == handle)
        return MFX_ERR_INVALID_HANDLE;

    TextureSubResource sr = GetResourceFromMid(mid);

    if (!sr.GetTexture())
        return MFX_ERR_INVALID_HANDLE;

    mfxHDLPair *pPair  =  (mfxHDLPair*)handle;

    pPair->first  = sr.GetTexture();
    pPair->second = (mfxHDL)(UINT_PTR)sr.GetSubResource();

    return MFX_ERR_NONE;
}

mfxStatus D3D11FrameAllocator::CheckRequestType(mfxFrameAllocRequest *request)
{
    mfxStatus sts = BaseFrameAllocator::CheckRequestType(request);
    if (MFX_ERR_NONE != sts)
        return sts;

    if ((request->Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)) != 0)
        return MFX_ERR_NONE;
    else
        return MFX_ERR_UNSUPPORTED;
}

mfxStatus D3D11FrameAllocator::ReleaseResponse(mfxFrameAllocResponse *response)
{
    if (NULL == response)
        return MFX_ERR_NULL_PTR;

    if (response->mids && 0 != response->NumFrameActual)
    {
        //check whether texture exsist
        TextureSubResource sr = GetResourceFromMid(response->mids[0]);

        if (!sr.GetTexture())
            return MFX_ERR_NULL_PTR;

        sr.Release();

        //if texture is last it is possible to remove also all handles from map to reduce fragmentation
        //search for allocated chunk
        if (m_resourcesByRequest.end() == std::find_if(m_resourcesByRequest.begin(), m_resourcesByRequest.end(), TextureResource::isAllocated))
        {
            m_resourcesByRequest.clear();
            m_memIdMap.clear();
        }
    }

    return MFX_ERR_NONE;
}
mfxStatus D3D11FrameAllocator::AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    HRESULT hRes;

    DXGI_FORMAT colorFormat = ConverColortFormat(request->Info.FourCC);

    if (DXGI_FORMAT_UNKNOWN == colorFormat)
    {
       return MFX_ERR_UNSUPPORTED;
    }

    TextureResource newTexture;

    if (request->Info.FourCC == MFX_FOURCC_P8)
    {
        D3D11_BUFFER_DESC desc = { 0 };

        desc.ByteWidth           = request->Info.Width * request->Info.Height;
        desc.Usage               = D3D11_USAGE_STAGING;
        desc.BindFlags           = 0;
        desc.CPUAccessFlags      = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags           = 0;
        desc.StructureByteStride = 0;

        ID3D11Buffer * buffer = 0;
        hRes = m_initParams.pDevice->CreateBuffer(&desc, 0, &buffer);
        if (FAILED(hRes))
            return MFX_ERR_MEMORY_ALLOC;

        newTexture.textures.push_back(reinterpret_cast<ID3D11Texture2D *>(buffer));
    }
    else
    {
        D3D11_TEXTURE2D_DESC desc = {0};

        desc.Width = request->Info.Width;
        desc.Height =  request->Info.Height;

        desc.MipLevels = 1;
        //number of subresources is 1 in case of not single texture
        desc.ArraySize = m_initParams.bUseSingleTexture ? request->NumFrameSuggested : 1;
        desc.Format = ConverColortFormat(request->Info.FourCC);
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.MiscFlags = m_initParams.uncompressedResourceMiscFlags;

        desc.BindFlags = D3D11_BIND_DECODER;

        if ( (MFX_MEMTYPE_FROM_VPPIN & request->Type) && (DXGI_FORMAT_YUY2 == desc.Format) ||
             (DXGI_FORMAT_B8G8R8A8_UNORM == desc.Format) )
        {
            desc.BindFlags = D3D11_BIND_RENDER_TARGET;
            if (desc.ArraySize > 2)
                return MFX_ERR_MEMORY_ALLOC;
        }

        if ( (MFX_MEMTYPE_FROM_VPPOUT & request->Type) ||
             (MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET & request->Type))
        {
            desc.BindFlags = D3D11_BIND_RENDER_TARGET;
            if (desc.ArraySize > 2)
                return MFX_ERR_MEMORY_ALLOC;
        }

        if( DXGI_FORMAT_P8 == desc.Format )
        {
            desc.BindFlags = 0;
        }

        ID3D11Texture2D* pTexture2D;

        for(size_t i = 0; i < request->NumFrameSuggested / desc.ArraySize; i++)
        {
            hRes = m_initParams.pDevice->CreateTexture2D(&desc, NULL, &pTexture2D);

            if (FAILED(hRes))
            {
                msdk_printf(MSDK_STRING("CreateTexture2D(%d) failed, hr = 0x%08lx\n"), i, hRes);
                return MFX_ERR_MEMORY_ALLOC;
            }
            newTexture.textures.push_back(pTexture2D);
        }

        desc.ArraySize = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.BindFlags = 0;
        desc.MiscFlags = 0;

        for(size_t i = 0; i < request->NumFrameSuggested; i++)
        {
            hRes = m_initParams.pDevice->CreateTexture2D(&desc, NULL, &pTexture2D);

            if (FAILED(hRes))
            {
                printf("Create staging texture(%d) failed hr = 0x%X\n", i, hRes);
                return MFX_ERR_MEMORY_ALLOC;
            }
            newTexture.stagingTexture.push_back(pTexture2D);
        }
    }

    // mapping to self created handles array, starting from zero or from last assigned handle + 1
    sequence<mfxHDL> seq_initializer(m_resourcesByRequest.empty() ? 0 :  m_resourcesByRequest.back().outerMids.back());

    //incrementing starting index
    //1. 0(NULL) is invalid memid
    //2. back is last index not new one
    seq_initializer();

    std::generate_n(std::back_inserter(newTexture.outerMids), request->NumFrameSuggested, seq_initializer);

    //saving texture resources
    m_resourcesByRequest.push_back(newTexture);

    //providing pointer to mids externally
    response->mids = &m_resourcesByRequest.back().outerMids.front();
    response->NumFrameActual = request->NumFrameSuggested;

    //iterator prior end()
    std::list <TextureResource>::iterator it_last = m_resourcesByRequest.end();
    //fill map
    std::fill_n(std::back_inserter(m_memIdMap), request->NumFrameSuggested, --it_last);

    return MFX_ERR_NONE;
}

DXGI_FORMAT D3D11FrameAllocator::ConverColortFormat(mfxU32 fourcc)
{
    switch (fourcc)
    {
        case MFX_FOURCC_NV12:
            return DXGI_FORMAT_NV12;

        case MFX_FOURCC_YUY2:
            return DXGI_FORMAT_YUY2;

        case MFX_FOURCC_RGB4:
            return DXGI_FORMAT_B8G8R8A8_UNORM;

        case MFX_FOURCC_P8:
        case MFX_FOURCC_P8_TEXTURE:
            return DXGI_FORMAT_P8;

        default:
            return DXGI_FORMAT_UNKNOWN;
    }
}

#endif // #if MFX_D3D11_SUPPORT
