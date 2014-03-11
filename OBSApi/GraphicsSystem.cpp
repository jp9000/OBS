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

#pragma warning(disable: 4530)

#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace std;

#define MANUAL_BUFFER_SIZE 64

GraphicsSystem *GS = NULL;

namespace
{
    struct HandleCloser
    {
        void operator()(HANDLE h) { if (!h) return; CloseHandle(h); }
    };

    struct MutexCloser
    {
        void operator()(HANDLE h) { if (!h) return; OSLeaveMutex(h); OSCloseMutex(h); }
    };

    struct MutexLock
    {
        bool locked, unlock;
        HANDLE h;
        MutexLock(unique_ptr<void, MutexCloser> const &mutex, bool tryLock = false, bool autounlock = true) : locked(false), unlock(autounlock), h(mutex.get())
        {
            if (!h) return;
            if (tryLock && !OSTryEnterMutex(h)) return;
            if (!tryLock) OSEnterMutex(h);
            locked = true;
        }
        ~MutexLock() { if (locked && unlock) OSLeaveMutex(h); }
    };
}

struct FutureShaderContainer
{
    struct FutureShaderContext
    {
        Shader *sharedShader;
        unique_ptr<Shader> shader;
        unique_ptr<void, HandleCloser> readyEvent;
        unique_ptr<void, HandleCloser> thread;
        wstring fileName;
    };
    map<wstring, unique_ptr<FutureShaderContext>> contexts;
    unique_ptr<void, MutexCloser> lock;
    FutureShaderContainer() : lock(OSCreateMutex()) {}
};

GraphicsSystem::GraphicsSystem()
:   curMatrix(0)
{
    MatrixStack << Matrix().SetIdentity();
    futureShaders = new FutureShaderContainer;
}

GraphicsSystem::~GraphicsSystem()
{
    delete futureShaders;
}

void GraphicsSystem::Init()
{
}

Shader* GraphicsSystem::CreateVertexShaderFromFile(CTSTR lpFileName)
{
    XFile ShaderFile;

    if(!ShaderFile.Open(lpFileName, XFILE_READ|XFILE_SHARED, XFILE_OPENEXISTING))
        return NULL;

    String strShader;
    ShaderFile.ReadFileToString(strShader);

    return CreateVertexShader(strShader, lpFileName);
}

Shader* GraphicsSystem::CreatePixelShaderFromFile(CTSTR lpFileName)
{
    XFile ShaderFile;

    if(!ShaderFile.Open(lpFileName, XFILE_READ|XFILE_SHARED, XFILE_OPENEXISTING))
        return NULL;

    String strShader;
    ShaderFile.ReadFileToString(strShader);

    return CreatePixelShader(strShader, lpFileName);
}

DWORD STDCALL CreatePixelShaderThread(void *arg)
{
    FutureShaderContainer::FutureShaderContext &c = *(FutureShaderContainer::FutureShaderContext*)arg;

    QWORD start = GetQPCTime100NS();

    c.shader.reset(GS->CreatePixelShaderFromFile(c.fileName.c_str()));

    c.sharedShader = c.shader.get();

    SetEvent(c.readyEvent.get());

    Log(L"Compilation for %s took %llu", c.fileName.c_str(), (GetQPCTime100NS() - start) / 10000);

    return 0;
}

FutureShader GraphicsSystem::CreatePixelShaderFromFileAsync(CTSTR fileName)
{
    wstring const fn = fileName;
    auto &cs = futureShaders->contexts;

    MutexLock m(futureShaders->lock);

    bool initialized = cs.find(fn) != end(cs);
    if (!initialized)
        cs[fn].reset(new FutureShaderContainer::FutureShaderContext);
    auto &c = *cs[fn];

    if (!initialized)
    {
        c.readyEvent.reset(CreateEvent(nullptr, true, false, nullptr));
        c.fileName = fn;
        c.thread.reset(OSCreateThread(CreatePixelShaderThread, &c));
    }

    if (c.thread && WaitForSingleObject(c.readyEvent.get(), 0) == WAIT_OBJECT_0)
        c.thread.reset();
    
    if (c.thread)
        return FutureShader(c.readyEvent.get(), &c.sharedShader);
    
    return FutureShader(c.shader.get());
}


void GraphicsSystem::DrawSprite(Texture *texture, DWORD color, float x, float y, float x2, float y2)
{
    assert(texture);

    DrawSpriteEx(texture, color, x, y, x2, y2, 0.0f, 0.0f, 1.0f, 1.0f);
}


/////////////////////////////////
//manual rendering functions

void GraphicsSystem::StartVertexBuffer()
{
    bNormalSet = FALSE;
    bColorSet = FALSE;
    TexCoordSetList.Clear();

    vbd = new VBData;
    dwCurPointVert = 0;
    dwCurTexVert   = 0;
    dwCurColorVert = 0;
    dwCurNormVert  = 0;
}

VertexBuffer *GraphicsSystem::SaveVertexBuffer()
{
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
}

void GraphicsSystem::Vertex(float x, float y, float z)
{
    Vect v(x, y, z);
    Vertex(v);
}

void GraphicsSystem::Vertex(const Vect &v)
{
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
}

void GraphicsSystem::Normal(float x, float y, float z)
{
    Vect v(x, y, z);
    Normal(v);
}

void GraphicsSystem::Normal(const Vect &v)
{
    vbd->NormalList << v;

    ++dwCurNormVert;

    bNormalSet = TRUE;
}

void GraphicsSystem::Color(DWORD dwRGBA)
{
    vbd->ColorList << dwRGBA;

    ++dwCurColorVert;

    bColorSet = TRUE;
}

void GraphicsSystem::Color(const Color4 &v)
{
    Color(Vect4_to_RGBA(v));
}

void GraphicsSystem::TexCoord(float u, float v, int idTexture)
{
    UVCoord uv(u, v);
    TexCoord(uv, idTexture);
}

void GraphicsSystem::TexCoord(const UVCoord &uv, int idTexture)
{
    if(vbd->UVList.Num() < (DWORD)(idTexture+1))
    {
        vbd->UVList.SetSize(idTexture+1);
        TexCoordSetList.SetSize(idTexture+1);
    }

    vbd->UVList[idTexture] << uv;

    ++dwCurTexVert;

    TexCoordSetList.Set(idTexture);
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
