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


void D3D11Shader::LoadDefaults()
{
    for(UINT i=0; i<Params.Num(); i++)
    {
        ShaderParam &param = Params[i];

        if(param.defaultValue.Num())
        {
            param.bChanged = TRUE;
            param.curValue.CopyList(param.defaultValue);
        }
    }
}

bool D3D11Shader::ProcessData(ShaderProcessor &processor, CTSTR lpFileName)
{
    Params.TransferFrom(processor.Params);
    Samplers.TransferFrom(processor.Samplers);

    constantSize = 0;
    for(UINT i=0; i<Params.Num(); i++)
    {
        ShaderParam &param = Params[i];

        switch(param.type)
        {
            case Parameter_Bool:
            case Parameter_Float:
            case Parameter_Int:         constantSize += sizeof(float); break;
            case Parameter_Vector2:     constantSize += sizeof(float)*2; break;
            case Parameter_Vector:      constantSize += sizeof(float)*3; break;
            case Parameter_Vector4:     constantSize += sizeof(float)*4; break;
            case Parameter_Matrix3x3:   constantSize += sizeof(float)*3*3; break;
            case Parameter_Matrix:      constantSize += sizeof(float)*4*4; break;
        }
    }

    if(constantSize)
    {
        D3D11_BUFFER_DESC bd;
        zero(&bd, sizeof(bd));

        bd.ByteWidth        = (constantSize+15)&0xFFFFFFF0; //align to 128bit boundry
        bd.Usage            = D3D11_USAGE_DYNAMIC;
        bd.BindFlags        = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags   = D3D11_CPU_ACCESS_WRITE;

        HRESULT err = GetD3D()->CreateBuffer(&bd, NULL, &constantBuffer);
        if(FAILED(err))
        {
            AppWarning(TEXT("Unable to create constant buffer for shdaer '%s', result = %08lX"), lpFileName, err);
            return false;
        }
    }

    LoadDefaults();

    return true;
}

Shader* D3D11VertexShader::CreateVertexShader(CTSTR lpShader, CTSTR lpFileName)
{
    String errorString;

    ShaderProcessor shaderProcessor;
    if(!shaderProcessor.ProcessShader(lpShader))
        AppWarning(TEXT("Unable to process vertex shader '%s'"), lpFileName); //don't exit, leave it to the actual shader compiler to tell the errors

    //-----------------------------------------------

    LPSTR lpAnsiShader = tstr_createUTF8(lpShader);
    LPSTR lpAnsiFileName = tstr_createUTF8(lpFileName);

    ID3D10Blob *errorMessages = NULL, *shaderBlob = NULL;
    HRESULT err = D3DX11CompileFromMemory(lpAnsiShader, strlen(lpAnsiShader), lpAnsiFileName, NULL, NULL, "main", "vs_4_0_level_9_3", D3D10_SHADER_OPTIMIZATION_LEVEL3, 0, NULL, &shaderBlob, &errorMessages, NULL);

    Free(lpAnsiFileName);
    Free(lpAnsiShader);

    if(FAILED(err))
    {
        if(errorMessages)
        {
            if(errorMessages->GetBufferSize())
            {
                LPSTR lpErrors = (LPSTR)errorMessages->GetBufferPointer();
                Log(TEXT("Error compiling vertex shader '%s':\r\n\r\n%S\r\n"), lpFileName, lpErrors);
            }

            errorMessages->Release();
        }

        AppWarning(TEXT("Compilation of vertex shader '%s' failed, result = %08lX"), lpFileName, err);
        return NULL;
    }

    //-----------------------------------------------

    ID3D11VertexShader *vShader;
    ID3D11InputLayout *vShaderLayout;

    err = GetD3D()->CreateVertexShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), NULL, &vShader);
    if(FAILED(err))
    {
        AppWarning(TEXT("Unable to create vertex shader '%s', result = %08lX"), lpFileName, err);
        SafeRelease(shaderBlob);
        return NULL;
    }

    err = GetD3D()->CreateInputLayout(shaderProcessor.generatedLayout.Array(), shaderProcessor.generatedLayout.Num(), shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), &vShaderLayout);
    if(FAILED(err))
    {
        AppWarning(TEXT("Unable to create vertex layout for vertex shader '%s', result = %08lX"), lpFileName, err);
        SafeRelease(shaderBlob);
        SafeRelease(vShader);
        return NULL;
    }

    shaderBlob->Release();

    //-----------------------------------------------

    D3D11VertexShader *shader = new D3D11VertexShader;
    shader->vertexShader    = vShader;
    shader->inputLayout     = vShaderLayout;
    if(!shader->ProcessData(shaderProcessor, lpFileName))
    {
        delete shader;
        return NULL;
    }

    shader->bHasNormals     = shaderProcessor.bHasNormals;
    shader->bHasColors      = shaderProcessor.bHasColors;
    shader->bHasTangents    = shaderProcessor.bHasTangents;
    shader->nTextureCoords  = shaderProcessor.numTextureCoords;
    shader->hViewProj       = shader->GetParameterByName(TEXT("ViewProj"));

    return shader;
}

Shader* D3D11PixelShader::CreatePixelShader(CTSTR lpShader, CTSTR lpFileName)
{
    String errorString;

    ShaderProcessor shaderProcessor;
    if(!shaderProcessor.ProcessShader(lpShader))
        AppWarning(TEXT("Unable to process pixel shader '%s'"), lpFileName); //don't exit, leave it to the actual shader compiler to tell the errors

    //-----------------------------------------------

    LPSTR lpAnsiShader = tstr_createUTF8(lpShader);
    LPSTR lpAnsiFileName = tstr_createUTF8(lpFileName);

    ID3D10Blob *errorMessages = NULL, *shaderBlob = NULL;
    HRESULT err = D3DX11CompileFromMemory(lpAnsiShader, strlen(lpAnsiShader), lpAnsiFileName, NULL, NULL, "main", "ps_4_0_level_9_3", D3D10_SHADER_OPTIMIZATION_LEVEL3, 0, NULL, &shaderBlob, &errorMessages, NULL);

    Free(lpAnsiFileName);
    Free(lpAnsiShader);

    if(FAILED(err))
    {
        if(errorMessages)
        {
            if(errorMessages->GetBufferSize())
            {
                LPSTR lpErrors = (LPSTR)errorMessages->GetBufferPointer();
                Log(TEXT("Error compiling pixel shader '%s':\r\n\r\n%S\r\n"), lpFileName, lpErrors);
            }

            errorMessages->Release();
        }

        AppWarning(TEXT("Compilation of pixel shader '%s' failed, result = %08lX"), lpFileName, err);
        return NULL;
    }

    //-----------------------------------------------

    ID3D11PixelShader *pShader;

    err = GetD3D()->CreatePixelShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), NULL, &pShader);
    if(FAILED(err))
    {
        AppWarning(TEXT("Unable to create pixel shader '%s', result = %08lX"), lpFileName, err);
        SafeRelease(shaderBlob);
        return NULL;
    }

    shaderBlob->Release();

    //-----------------------------------------------

    D3D11PixelShader *shader = new D3D11PixelShader;
    shader->pixelShader = pShader;
    if(!shader->ProcessData(shaderProcessor, lpFileName))
    {
        delete shader;
        return NULL;
    }

    return shader;
}


D3D11Shader::~D3D11Shader()
{
    for(UINT i=0; i<Samplers.Num(); i++)
        Samplers[i].FreeData();
    for(UINT i=0; i<Params.Num(); i++)
        Params[i].FreeData();

    SafeRelease(constantBuffer);
}

D3D11VertexShader::~D3D11VertexShader()
{
    SafeRelease(vertexShader);
    SafeRelease(inputLayout);
}

D3D11PixelShader::~D3D11PixelShader()
{
    SafeRelease(pixelShader);
}


int    D3D11Shader::NumParams() const
{
    return Params.Num();
}

HANDLE D3D11Shader::GetParameter(UINT parameter) const
{
    if(parameter >= Params.Num())
        return NULL;
    return (HANDLE)(Params+parameter);
}

HANDLE D3D11Shader::GetParameterByName(CTSTR lpName) const
{
    for(UINT i=0; i<Params.Num(); i++)
    {
        ShaderParam &param = Params[i];
        if(param.name == lpName)
            return (HANDLE)&param;
    }

    return NULL;
}

#define GetValidHandle() \
    ShaderParam *param = (ShaderParam*)hObject; \
    if(!hObject) \
    { \
        AppWarning(TEXT("Invalid handle input as shader parameter")); \
        return; \
    }


void   D3D11Shader::GetParameterInfo(HANDLE hObject, ShaderParameterInfo &paramInfo) const
{
    GetValidHandle();

    paramInfo.type = param->type;
    paramInfo.name = param->name;
}


void   D3D11Shader::SetBool(HANDLE hObject, BOOL bValue)
{
    GetValidHandle();

    BOOL bSizeChanged = param->curValue.SetSize(sizeof(BOOL));
    BOOL &curVal = *(BOOL*)param->curValue.Array();

    if(bSizeChanged || curVal != bValue)
    {
        curVal = bValue;
        param->bChanged = TRUE;
    }
}

void   D3D11Shader::SetFloat(HANDLE hObject, float fValue)
{
    GetValidHandle();

    BOOL bSizeChanged = param->curValue.SetSize(sizeof(float));
    float &curVal = *(float*)param->curValue.Array();

    if(bSizeChanged || curVal != fValue)
    {
        curVal = fValue;
        param->bChanged = TRUE;
    }
}

void   D3D11Shader::SetInt(HANDLE hObject, int iValue)
{
    GetValidHandle();

    BOOL bSizeChanged = param->curValue.SetSize(sizeof(int));
    int &curVal = *(int*)param->curValue.Array();

    if(bSizeChanged || curVal != iValue)
    {
        curVal = iValue;
        param->bChanged = TRUE;
    }
}

void   D3D11Shader::SetMatrix(HANDLE hObject, float *matrix)
{
    SetValue(hObject, matrix, sizeof(float)*4*4);
}

void   D3D11Shader::SetVector(HANDLE hObject, const Vect &value)
{
    SetValue(hObject, value.ptr, sizeof(float)*3);
}

void   D3D11Shader::SetVector2(HANDLE hObject, const Vect2 &value)
{
    SetValue(hObject, value.ptr, sizeof(Vect2));
}

void   D3D11Shader::SetVector4(HANDLE hObject, const Vect4 &value)
{
    SetValue(hObject, value.ptr, sizeof(Vect4));
}

void   D3D11Shader::SetTexture(HANDLE hObject, BaseTexture *texture)
{
    GetValidHandle();

    BOOL bSizeChanged = param->curValue.SetSize(sizeof(const BaseTexture*));
    const BaseTexture *&curVal = *(const BaseTexture**)param->curValue.Array();

    if(bSizeChanged || curVal != texture)
    {
        curVal = texture;
        param->bChanged = TRUE;
    }
}

void   D3D11Shader::SetValue(HANDLE hObject, const void *val, DWORD dwSize)
{
    GetValidHandle();

    BOOL bSizeChanged = param->curValue.SetSize(dwSize);

    if(bSizeChanged || !mcmp(param->curValue.Array(), val, dwSize))
    {
        mcpy(param->curValue.Array(), val, dwSize);
        param->bChanged = TRUE;
    }
}

void  D3D11Shader::UpdateParams()
{
    List<BYTE> shaderConstantData;
    bool bUpload = false;

    for(UINT i=0; i<Params.Num(); i++)
    {
        ShaderParam &param = Params[i];

        if(param.type != Parameter_Texture)
        {
            if(!param.curValue.Num())
            {
                AppWarning(TEXT("D3D11Shader::UpdateParams: shader parameter '%s' not set"), param.name.Array());
                bUpload = false;
                break;
            }

            shaderConstantData.AppendList(param.curValue);

            if(param.bChanged)
            {
                bUpload = true;
                param.bChanged = false;
            }
        }
        else
        {
            if(param.curValue.Num())
            {
                Texture *texture = *(Texture**)param.curValue.Array();
                LoadTexture(texture, param.textureID);
            }
        }
    }

    if(shaderConstantData.Num() != constantSize)
    {
        AppWarning(TEXT("D3D11Shader::UpdateParams: invalid parameter specifications, constant size given: %d, constant size expected: %d"), shaderConstantData.Num(), constantSize);
        bUpload = false;
    }

    if(bUpload)
    {
        D3D11_MAPPED_SUBRESOURCE map;

        HRESULT err;
        if(FAILED(err = GetD3DContext()->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map)))
        {
            AppWarning(TEXT("D3D11Shader::UpdateParams: could not map constant buffer, result = %08lX"), err);
            return;
        }

        if(App->SSE2Available())
            SSECopy(map.pData, shaderConstantData.Array(), shaderConstantData.Num());
        else
            mcpy(map.pData, shaderConstantData.Array(), shaderConstantData.Num());

        GetD3DContext()->Unmap(constantBuffer, 0);
    }
}
