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


VertexBuffer* D3D10VertexBuffer::CreateVertexBuffer(VBData *vbData, BOOL bStatic)
{
    if(!vbData)
    {
        AppWarning(TEXT("D3D10VertexBuffer::CreateVertexBuffer: vbData NULL"));
        return NULL;
    }

    HRESULT err;

    D3D10VertexBuffer *buf = new D3D10VertexBuffer;
    buf->numVerts = vbData->VertList.Num();

    D3D11_BUFFER_DESC bd;
    D3D11_SUBRESOURCE_DATA srd;
    zero(&bd, sizeof(bd));
    zero(&srd, sizeof(srd));

    bd.Usage = bStatic ? D3D11_USAGE_DEFAULT : D3D11_USAGE_DYNAMIC;
    bd.CPUAccessFlags = bStatic ? 0 : D3D11_CPU_ACCESS_WRITE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    //----------------------------------------

    buf->vertexSize = sizeof(Vect);
    bd.ByteWidth = sizeof(Vect)*buf->numVerts;
    srd.pSysMem = vbData->VertList.Array();

    err = GetD3D()->CreateBuffer(&bd, &srd, &buf->vertexBuffer);
    if(FAILED(err))
    {
        AppWarning(TEXT("D3D10VertexBuffer::CreateVertexBuffer: Failed to create the vertex portion of the vertex buffer, result = %08lX"), err);
        delete buf;
        return NULL;
    }

    //----------------------------------------

    if(vbData->NormalList.Num())
    {
        buf->normalSize = sizeof(Vect);
        bd.ByteWidth = sizeof(Vect)*buf->numVerts;
        srd.pSysMem = vbData->NormalList.Array();
        err = GetD3D()->CreateBuffer(&bd, &srd, &buf->normalBuffer);
        if(FAILED(err))
        {
            AppWarning(TEXT("D3D10VertexBuffer::CreateVertexBuffer: Failed to create the normal portion of the vertex buffer, result = %08lX"), err);
            delete buf;
            return NULL;
        }
    }

    //----------------------------------------

    if(vbData->ColorList.Num())
    {
        buf->colorSize = sizeof(DWORD);
        bd.ByteWidth = sizeof(DWORD)*buf->numVerts;
        srd.pSysMem = vbData->ColorList.Array();
        err = GetD3D()->CreateBuffer(&bd, &srd, &buf->colorBuffer);
        if(FAILED(err))
        {
            AppWarning(TEXT("D3D10VertexBuffer::CreateVertexBuffer: Failed to create the color portion of the vertex buffer, result = %08lX"), err);
            delete buf;
            return NULL;
        }
    }

    //----------------------------------------

    if(vbData->TangentList.Num())
    {
        buf->tangentSize = sizeof(Vect);
        bd.ByteWidth = sizeof(Vect)*buf->numVerts;
        srd.pSysMem = vbData->TangentList.Array();
        err = GetD3D()->CreateBuffer(&bd, &srd, &buf->tangentBuffer);
        if(FAILED(err))
        {
            AppWarning(TEXT("D3D10VertexBuffer::CreateVertexBuffer: Failed to create the tangent portion of the vertex buffer, result = %08lX"), err);
            delete buf;
            return NULL;
        }
    }

    //----------------------------------------

    if(vbData->UVList.Num())
    {
        buf->UVBuffers.SetSize(vbData->UVList.Num());
        buf->UVSizes.SetSize(vbData->UVList.Num());

        for(UINT i=0; i<vbData->UVList.Num(); i++)
        {
            List<UVCoord> &textureVerts = vbData->UVList[i];

            buf->UVSizes[i] = sizeof(UVCoord);
            bd.ByteWidth = buf->UVSizes[i]*buf->numVerts;
            srd.pSysMem = textureVerts.Array();

            ID3D11Buffer *tvBuffer;
            err = GetD3D()->CreateBuffer(&bd, &srd, &tvBuffer);
            if(FAILED(err))
            {
                AppWarning(TEXT("D3D10VertexBuffer::CreateVertexBuffer: Failed to create the texture vertex %d portion of the vertex buffer, result = %08lX"), i, err);
                delete buf;
                return NULL;
            }

            buf->UVBuffers[i] = tvBuffer;
        }
    }

    //----------------------------------------

    buf->bDynamic = !bStatic;

    if(bStatic)
    {
        delete vbData;
        buf->data = NULL;
    }
    else
        buf->data = vbData;

    return buf;
}


D3D10VertexBuffer::~D3D10VertexBuffer()
{
    for(UINT i=0; i<UVBuffers.Num(); i++)
        SafeRelease(UVBuffers[i]);

    SafeRelease(tangentBuffer);
    SafeRelease(colorBuffer);
    SafeRelease(normalBuffer);
    SafeRelease(vertexBuffer);

    delete data;
}

void D3D10VertexBuffer::FlushBuffers()
{
    if(!bDynamic)
    {
        AppWarning(TEXT("D3D10VertexBuffer::FlushBuffers: Cannot flush buffers on a non-dynamic vertex buffer"));
        return;
    }

    HRESULT err;

    //---------------------------------------------------

    D3D11_MAPPED_SUBRESOURCE map;

    if(FAILED(err = GetD3DCtx()->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map)))
    {
        AppWarning(TEXT("D3D10VertexBuffer::FlushBuffers: failed to map vertex buffer, result = %08lX"), err);
        return;
    }

    mcpy(map.pData, data->VertList.Array(), sizeof(Vect)*numVerts);

    GetD3DCtx()->Unmap(vertexBuffer, 0);

    //---------------------------------------------------

    if(normalBuffer)
    {
        if(FAILED(err = GetD3DCtx()->Map(normalBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map)))
        {
            AppWarning(TEXT("D3D10VertexBuffer::FlushBuffers: failed to map normal buffer, result = %08lX"), err);
            return;
        }

        mcpy(map.pData, data->NormalList.Array(), sizeof(Vect)*numVerts);
        GetD3DCtx()->Unmap(normalBuffer, 0);
    }

    //---------------------------------------------------

    if(colorBuffer)
    {
        if(FAILED(err = GetD3DCtx()->Map(colorBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map)))
        {
            AppWarning(TEXT("D3D10VertexBuffer::FlushBuffers: failed to map color buffer, result = %08lX"), err);
            return;
        }

        mcpy(map.pData, data->ColorList.Array(), sizeof(Vect)*numVerts);
        GetD3DCtx()->Unmap(colorBuffer, 0);
    }

    //---------------------------------------------------

    if(tangentBuffer)
    {
        if(FAILED(err = GetD3DCtx()->Map(tangentBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map)))
        {
            AppWarning(TEXT("D3D10VertexBuffer::FlushBuffers: failed to map tangent buffer, result = %08lX"), err);
            return;
        }

        mcpy(map.pData, data->TangentList.Array(), sizeof(Vect)*numVerts);
        GetD3DCtx()->Unmap(tangentBuffer, 0);
    }

    //---------------------------------------------------

    if(UVBuffers.Num())
    {
        for(UINT i=0; i<UVBuffers.Num(); i++)
        {
            List<UVCoord> &textureVerts = data->UVList[i];

            ID3D11Buffer *buffer = UVBuffers[i];

            if(FAILED(err = GetD3DCtx()->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map)))
            {
                AppWarning(TEXT("D3D10VertexBuffer::FlushBuffers: failed to map texture vertex buffer %d, result = %08lX"), i, err);
                return;
            }

            mcpy(map.pData, textureVerts.Array(), sizeof(UVCoord)*numVerts);
            GetD3DCtx()->Unmap(buffer, 0);
        }
    }
}

VBData* D3D10VertexBuffer::GetData()
{
    if(!bDynamic)
    {
        AppWarning(TEXT("D3D10VertexBuffer::GetData: Cannot get vertex data of a non-dynamic vertex buffer"));
        return NULL;
    }

    return data;
}

void D3D10VertexBuffer::MakeBufferList(D3D10VertexShader *vShader, List<ID3D11Buffer*> &bufferList, List<UINT> &strides) const
{
    assert(vShader);

    bufferList << vertexBuffer;
    strides << vertexSize;

    if(vShader->bHasNormals)
    {
        if(normalBuffer)
        {
            bufferList << normalBuffer;
            strides << normalSize;
        }
        else
            AppWarning(TEXT("Trying to use a vertex buffer without normals with a vertex shader that requires normals"));
    }

    if(vShader->bHasColors)
    {
        if(colorBuffer)
        {
            bufferList << colorBuffer;
            strides << colorSize;
        }
        else
            AppWarning(TEXT("Trying to use a vertex buffer without colors with a vertex shader that requires colors"));
    }

    if(vShader->bHasTangents)
    {
        if(tangentBuffer)
        {
            bufferList << tangentBuffer;
            strides << tangentSize;
        }
        else
            AppWarning(TEXT("Trying to use a vertex buffer without tangents with a vertex shader that requires tangents"));
    }

    if(vShader->nTextureCoords <= UVBuffers.Num())
    {
        for(UINT i=0; i<vShader->nTextureCoords; i++)
        {
            bufferList << UVBuffers[i];
            strides << UVSizes[i];
        }
    }
    else
        AppWarning(TEXT("Trying to use a vertex buffer with insufficient texture coordinates compared to the vertex shader requirements"));
}
