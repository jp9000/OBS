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


#include "OBSApi.h"


#define MANUAL_BUFFER_SIZE 64

GraphicsSystem *GS = NULL;


GraphicsSystem::GraphicsSystem()
:   curMatrix(0)
{
    MatrixStack << Matrix().SetIdentity();
}

void GraphicsSystem::Init()
{
}

Shader* GraphicsSystem::CreateVertexShaderFromFile(CTSTR lpFileName)
{
    traceIn(GraphicsSystem::CreateVertexShaderFromFile);

    XFile ShaderFile;

    if(!ShaderFile.Open(lpFileName, XFILE_READ, XFILE_OPENEXISTING))
        return NULL;

    String strShader;
    ShaderFile.ReadFileToString(strShader);

    return CreateVertexShader(strShader, lpFileName);

    traceOut;
}

Shader* GraphicsSystem::CreatePixelShaderFromFile(CTSTR lpFileName)
{
    traceIn(GraphicsSystem::CreatePixelShaderFromFile);

    XFile ShaderFile;

    if(!ShaderFile.Open(lpFileName, XFILE_READ, XFILE_OPENEXISTING))
        return NULL;

    String strShader;
    ShaderFile.ReadFileToString(strShader);

    return CreatePixelShader(strShader, lpFileName);

    traceOut;
}


void GraphicsSystem::DrawSprite(Texture *texture, DWORD color, float x, float y, float x2, float y2)
{
    traceIn(GraphicsSystem::DrawSprite);

    assert(texture);

    DrawSpriteEx(texture, color, x, y, x2, y2);

    traceOut;
}


/////////////////////////////////
//manual rendering functions

void GraphicsSystem::StartVertexBuffer()
{
    traceInFast(GraphicsSystem::StartVertexBuffer);

    bNormalSet = FALSE;
    bColorSet = FALSE;
    TexCoordSetList.Clear();

    vbd = new VBData;
    dwCurPointVert = 0;
    dwCurTexVert   = 0;
    dwCurColorVert = 0;
    dwCurNormVert  = 0;

    traceOutFast;
}

VertexBuffer *GraphicsSystem::SaveVertexBuffer()
{
    traceInFast(GraphicsSystem::SaveVertexBuffer);

    if(vbd->VertList.Num())
    {
        VertexBuffer *buffer;

        buffer = CreateVertexBuffer(vbd);

        vbd = NULL;

        return buffer;
    }
    else
    {
        delete vbd;
        vbd = NULL;

        return NULL;
    }

    traceOutFast;
}

void GraphicsSystem::Vertex(float x, float y, float z)
{
    traceInFast(GraphicsSystem::Vertex);

    Vect v(x, y, z);
    Vertex(v);

    traceOutFast;
}

void GraphicsSystem::Vertex(const Vect &v)
{
    traceInFast(GraphicsSystem::Vertex);

    if(!bNormalSet && vbd->NormalList.Num())
        Normal(vbd->NormalList[vbd->NormalList.Num()-1]);
    bNormalSet = 0;

    /////////////////
    if(!bColorSet && vbd->ColorList.Num())
        Color(vbd->ColorList[vbd->ColorList.Num()-1]);
    bColorSet = 0;

    /////////////////
    for(DWORD i=0; i<TexCoordSetList.Num(); i++)
    {
        if(!TexCoordSetList[i] && vbd->UVList[i].Num())
        {
            List<UVCoord> &UVList = vbd->UVList[i];
            TexCoord(UVCoord(UVList[UVList.Num()-1]), i);
        }
        TexCoordSetList.Clear(i);
    }

    vbd->VertList << v;

    ++dwCurPointVert;

    traceOutFast;
}

void GraphicsSystem::Normal(float x, float y, float z)
{
    traceInFast(GraphicsSystem::Normal);

    Vect v(x, y, z);
    Normal(v);

    traceOutFast;
}

void GraphicsSystem::Normal(const Vect &v)
{
    traceInFast(GraphicsSystem::Normal);

    vbd->NormalList << v;

    ++dwCurNormVert;

    bNormalSet = TRUE;

    traceOutFast;
}

void GraphicsSystem::Color(DWORD dwRGBA)
{
    traceInFast(GraphicsSystem::Color);

    vbd->ColorList << dwRGBA;

    ++dwCurColorVert;

    bColorSet = TRUE;

    traceOutFast;
}

void GraphicsSystem::Color(const Color4 &v)
{
    traceInFast(GraphicsSystem::Color);

    Color(Vect4_to_RGBA(v));

    traceOutFast;
}

void GraphicsSystem::TexCoord(float u, float v, int idTexture)
{
    traceInFast(GraphicsSystem::TexCoord);

    UVCoord uv(u, v);
    TexCoord(uv, idTexture);

    traceOutFast;
}

void GraphicsSystem::TexCoord(const UVCoord &uv, int idTexture)
{
    traceInFast(GraphicsSystem::TexCoord);

    if(vbd->UVList.Num() < (DWORD)(idTexture+1))
    {
        vbd->UVList.SetSize(idTexture+1);
        TexCoordSetList.SetSize(idTexture+1);
    }

    vbd->UVList[idTexture] << uv;

    ++dwCurTexVert;

    TexCoordSetList.Set(idTexture);

    traceOutFast;
}


/*========================================
   Matrix Stack functions
=========================================*/

inline void  GraphicsSystem::MatrixPush()
{
    MatrixStack << Matrix(MatrixStack[curMatrix]);
    ++curMatrix;
}

inline void  GraphicsSystem::MatrixPop()
{
    MatrixStack.Remove(curMatrix);
    --curMatrix;

    ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixSet(const Matrix &m)
{
    MatrixStack[curMatrix] = m;
    ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixMultiply(const Matrix &m)
{
    MatrixStack[curMatrix] *= m;
    ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixRotate(float x, float y, float z, float a)
{
    MatrixStack[curMatrix] *= Quat(AxisAngle(x, y, z, a));
    ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixRotate(const AxisAngle &aa)
{
    MatrixStack[curMatrix] *= Quat(aa);
    ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixRotate(const Quat &q)
{
    MatrixStack[curMatrix] *= q;
    ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixTranslate(float x, float y)
{
    MatrixStack[curMatrix] *= Vect(x, y, 0.0f);
    ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixTranslate(const Vect2 &pos)
{
    MatrixStack[curMatrix] *= Vect(pos);
    ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixScale(const Vect2 &scale)
{
    MatrixStack[curMatrix].Scale(scale.x, scale.y, 1.0f);
    ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixScale(float x, float y)
{
    MatrixStack[curMatrix].Scale(x, y, 1.0f);
    ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixTranspose()
{
    MatrixStack[curMatrix].Transpose();
    ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixIdentity()
{
    MatrixStack[curMatrix].SetIdentity();
    ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixGet(Vect &v, Quat &q)
{
    q.CreateFromMatrix(MatrixStack[curMatrix]);
    v = MatrixStack[curMatrix].T;
}

inline void  GraphicsSystem::MatrixGet(Matrix &m)
{
    m = MatrixStack[curMatrix];
}
