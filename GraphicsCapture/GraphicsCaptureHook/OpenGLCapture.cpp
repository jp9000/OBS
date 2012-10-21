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


typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef signed char GLbyte;
typedef short GLshort;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned long GLulong;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void GLvoid;
typedef ptrdiff_t GLintptrARB;
typedef ptrdiff_t GLsizeiptrARB;

#define GL_FRONT 0x0404
#define GL_BACK 0x0405

#define GL_UNSIGNED_BYTE 0x1401

#define GL_RGB 0x1907
#define GL_RGBA 0x1908

#define GL_BGR 0x80E0
#define GL_BGRA 0x80E1

#define GL_READ_ONLY 0x88B8
#define GL_WRITE_ONLY 0x88B9
#define GL_READ_WRITE 0x88BA
#define GL_BUFFER_ACCESS 0x88BB
#define GL_BUFFER_MAPPED 0x88BC
#define GL_BUFFER_MAP_POINTER 0x88BD
#define GL_STREAM_DRAW 0x88E0
#define GL_STREAM_READ 0x88E1
#define GL_STREAM_COPY 0x88E2
#define GL_STATIC_DRAW 0x88E4
#define GL_STATIC_READ 0x88E5
#define GL_STATIC_COPY 0x88E6
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_DYNAMIC_READ 0x88E9
#define GL_DYNAMIC_COPY 0x88EA
#define GL_PIXEL_PACK_BUFFER 0x88EB
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#define GL_PIXEL_PACK_BUFFER_BINDING 0x88ED
#define GL_PIXEL_UNPACK_BUFFER_BINDING 0x88EF

//------------------------------------------------

typedef void   (WINAPI * GLREADBUFFERPROC)(GLenum);
typedef void   (WINAPI * GLREADPIXELSPROC)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLvoid*);
typedef GLenum (WINAPI * GLGETERRORPROC)();
typedef BOOL   (WINAPI * WGLSWAPLAYERBUFFERSPROC)(HDC, UINT);
typedef BOOL   (WINAPI * WGLDELETECONTEXTPROC)(HGLRC);
typedef PROC   (WINAPI * WGLGETPROCADDRESSPROC)(LPCSTR);
typedef BOOL   (WINAPI * WGLMAKECURRENTPROC)(HDC, HGLRC);
typedef HGLRC  (WINAPI * WGLCREATECONTEXTPROC)(HDC);

GLREADBUFFERPROC                pglReadBuffer       = NULL;
GLREADPIXELSPROC                pglReadPixels       = NULL;
GLGETERRORPROC                  pglGetError         = NULL;
WGLSWAPLAYERBUFFERSPROC         pwglSwapLayerBuffers= NULL;
WGLDELETECONTEXTPROC            pwglDeleteContext   = NULL;
WGLGETPROCADDRESSPROC           pwglGetProcAddress  = NULL;
WGLMAKECURRENTPROC              pwglMakeCurrent     = NULL;
WGLCREATECONTEXTPROC            pwglCreateContext   = NULL;

#define glReadBuffer            (*pglReadBuffer)
#define glReadPixels            (*pglReadPixels)
#define glGetError              (*pglGetError)
#define jimglSwapLayerBuffers   (*pwglSwapLayerBuffers)
#define jimglDeleteContext      (*pwglDeleteContext)
#define jimglGetProcAddress     (*pwglGetProcAddress)
#define jimglMakeCurrent        (*pwglMakeCurrent)
#define jimglCreateContext      (*pwglCreateContext)

//------------------------------------------------

typedef void (WINAPI * GLBUFFERDATAARBPROC) (GLenum target, GLsizeiptrARB size, const GLvoid* data, GLenum usage);
typedef void (WINAPI * GLDELETEBUFFERSARBPROC)(GLsizei n, const GLuint* buffers);
typedef void (WINAPI * GLGENBUFFERSARBPROC)(GLsizei n, GLuint* buffers);
typedef GLvoid* (WINAPI * GLMAPBUFFERPROC)(GLenum target, GLenum access);
typedef GLboolean (WINAPI * GLUNMAPBUFFERPROC)(GLenum target);
typedef void (WINAPI * GLBINDBUFFERPROC)(GLenum target, GLuint buffer);

GLBUFFERDATAARBPROC     pglBufferData       = NULL;
GLDELETEBUFFERSARBPROC  pglDeleteBuffers    = NULL;
GLGENBUFFERSARBPROC     pglGenBuffers       = NULL;
GLMAPBUFFERPROC         pglMapBuffer        = NULL;
GLUNMAPBUFFERPROC       pglUnmapBuffer      = NULL;
GLBINDBUFFERPROC        pglBindBuffer       = NULL;

#define glBufferData    (*pglBufferData)
#define glDeleteBuffers (*pglDeleteBuffers)
#define glGenBuffers    (*pglGenBuffers)
#define glMapBuffer     (*pglMapBuffer)
#define glUnmapBuffer   (*pglUnmapBuffer)
#define glBindBuffer    (*pglBindBuffer)

//------------------------------------------------

HookData                glHookSwapBuffers;
HookData                glHookSwapLayerBuffers;
HookData                glHookDeleteContext;

GLuint                  gltextures[2] = {0, 0};
HDC                     hdcAcquiredDC = NULL;
HWND                    hwndTarget = NULL;

extern MemoryCopyData   *copyData;
extern LPBYTE           textureBuffers[2];
extern DWORD            curCapture;
extern BOOL             bHasTextures;

CaptureInfo             glcaptureInfo;


void KillGLTextures()
{
    if(bHasTextures)
    {
        glDeleteBuffers(2, gltextures);
        bHasTextures = FALSE;
    }

    DestroySharedMemory();
}

void HandleGLSceneUpdate(HDC hDC)
{
    if(!bTargetAcquired && hdcAcquiredDC == NULL)
    {
        PIXELFORMATDESCRIPTOR pfd;

        hwndTarget = WindowFromDC(hDC);

        int pixFormat = GetPixelFormat(hDC);
        DescribePixelFormat(hDC, pixFormat, sizeof(pfd), &pfd);

        if(pfd.cColorBits == 32 && hwndTarget)
        {
            bTargetAcquired = true;
            hdcAcquiredDC = hDC;
            glcaptureInfo.format = GS_BGR;
        }
    }

    if(hDC == hdcAcquiredDC)
    {
        RECT rc;
        GetClientRect(hwndTarget, &rc);

        if(!bHasTextures || rc.right != glcaptureInfo.cx || rc.bottom != glcaptureInfo.cy)
        {
            if(!hwndReceiver)
                hwndReceiver = FindWindow(RECEIVER_WINDOWCLASS, NULL);

            if(hwndReceiver)
            {
                if(bHasTextures)
                    glDeleteBuffers(2, gltextures);

                glcaptureInfo.cx = rc.right;
                glcaptureInfo.cy = rc.bottom;

                glGenBuffers(2, gltextures);

                DWORD dwSize = glcaptureInfo.cx*glcaptureInfo.cy*4;

                bool bSuccess = true;
                for(UINT i=0; i<2; i++)
                {
                    UINT test = 0;

                    glBindBuffer(GL_PIXEL_PACK_BUFFER, gltextures[i]);
                    glBufferData(GL_PIXEL_PACK_BUFFER, dwSize, 0, GL_STREAM_READ);
                }

                glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

                if(bSuccess)
                {
                    glcaptureInfo.mapID = InitializeSharedMemory(dwSize, &glcaptureInfo.mapSize, &copyData, textureBuffers);
                    if(!glcaptureInfo.mapID)
                        bSuccess = false;
                }

                if(bSuccess)
                {
                    bHasTextures = true;
                    glcaptureInfo.captureType = CAPTURETYPE_MEMORY;
                    glcaptureInfo.hwndSender = hwndSender;
                    glcaptureInfo.pitch = glcaptureInfo.cx*4;
                    glcaptureInfo.bFlip = TRUE;
                    PostMessage(hwndReceiver, RECEIVER_NEWCAPTURE, 0, (LPARAM)&glcaptureInfo);
                }
                else
                    glDeleteBuffers(2, gltextures);
            }
            else
                KillGLTextures();
        }

        if(bHasTextures)
        {
            if(bCapturing)
            {
                GLuint texture = gltextures[curCapture];
                DWORD nextCapture = curCapture == 0 ? 1 : 0;

                glReadBuffer(GL_BACK);
                glBindBuffer(GL_PIXEL_PACK_BUFFER, texture);
                glReadPixels(0, 0, glcaptureInfo.cx, glcaptureInfo.cy, GL_BGRA, GL_UNSIGNED_BYTE, 0);

                glBindBuffer(GL_PIXEL_PACK_BUFFER, gltextures[nextCapture]);
                GLubyte *data = (GLubyte*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
                if(data)
                {
                    DWORD pitch = glcaptureInfo.cx*4;
                    int lastRendered = -1;

                    //under no circumstances do we -ever- allow this function to stall
                    if(WaitForSingleObject(textureMutexes[curCapture], 0) == WAIT_OBJECT_0)
                        lastRendered = curCapture;
                    else if(WaitForSingleObject(textureMutexes[nextCapture], 0) == WAIT_OBJECT_0)
                        lastRendered = nextCapture;

                    LPBYTE outData = NULL;
                    if(lastRendered != -1)
                    {
                        SSECopy(textureBuffers[lastRendered], data, pitch*glcaptureInfo.cy);
                        textureBuffers[lastRendered][0] = 0x1a;
                        ReleaseMutex(textureMutexes[lastRendered]);
                    }

                    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
                    copyData->lastRendered = (UINT)lastRendered;
                }

                glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

                curCapture = nextCapture;
            }
            else
                KillGLTextures();
        }
    }
}

void HandleGLSceneDestroy()
{
    hdcAcquiredDC = NULL;
    bTargetAcquired = false;
}


BOOL WINAPI SwapBuffersHook(HDC hDC)
{
    HandleGLSceneUpdate(hDC);

    glHookSwapBuffers.Unhook();
    BOOL bResult = SwapBuffers(hDC);
    glHookSwapBuffers.Rehook(); 

    return bResult;
}

BOOL WINAPI wglSwapLayerBuffersHook(HDC hDC, UINT fuPlanes)
{
    HandleGLSceneUpdate(hDC);

    glHookSwapLayerBuffers.Unhook();
    BOOL bResult = jimglSwapLayerBuffers(hDC, fuPlanes);
    glHookSwapLayerBuffers.Rehook();

    return bResult;
}

BOOL WINAPI wglDeleteContextHook(HGLRC hRC)
{
    HandleGLSceneDestroy();

    glHookDeleteContext.Unhook();
    BOOL bResult = jimglDeleteContext(hRC);
    glHookDeleteContext.Rehook();

    return bResult;
}

bool InitGLCapture()
{
    bool bSuccess = false;

    HMODULE hGL = GetModuleHandle(TEXT("opengl32.dll"));
    if(hGL)
    {
        pglReadBuffer       = (GLREADBUFFERPROC)        GetProcAddress(hGL, "glReadBuffer");
        pglReadPixels       = (GLREADPIXELSPROC)        GetProcAddress(hGL, "glReadPixels");
        pglGetError         = (GLGETERRORPROC)          GetProcAddress(hGL, "glGetError");
        pwglSwapLayerBuffers= (WGLSWAPLAYERBUFFERSPROC) GetProcAddress(hGL, "wglSwapLayerBuffers");
        pwglDeleteContext   = (WGLDELETECONTEXTPROC)    GetProcAddress(hGL, "wglDeleteContext");
        pwglGetProcAddress  = (WGLGETPROCADDRESSPROC)   GetProcAddress(hGL, "wglGetProcAddress");
        pwglMakeCurrent     = (WGLMAKECURRENTPROC)      GetProcAddress(hGL, "wglMakeCurrent");
        pwglCreateContext   = (WGLCREATECONTEXTPROC)    GetProcAddress(hGL, "wglCreateContext");

        if( !pglReadBuffer || !pglReadPixels || !pglGetError || !pwglSwapLayerBuffers || !pwglDeleteContext || 
            !pwglGetProcAddress || !pwglMakeCurrent || !pwglCreateContext)
        {
            return false;
        }

        HDC hDC = CreateDC(TEXT("DISPLAY"), NULL, NULL, NULL);
        if(hDC)
        {
            PIXELFORMATDESCRIPTOR pfd;
            ZeroMemory(&pfd, sizeof(pfd));
            pfd.nSize = sizeof(pfd);
            pfd.nVersion = 1;
            pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
            pfd.iPixelType = PFD_TYPE_RGBA;
            pfd.cColorBits = 32;
            pfd.cDepthBits = 32;
            pfd.cAccumBits = 32;
            pfd.iLayerType = PFD_MAIN_PLANE;
            SetPixelFormat(hDC, ChoosePixelFormat(hDC, &pfd), &pfd);

            HGLRC hGlrc = jimglCreateContext(hDC);
            if(hGlrc)
            {
                jimglMakeCurrent(hDC, hGlrc);

                pglBufferData       = (GLBUFFERDATAARBPROC)     jimglGetProcAddress("glBufferData");
                pglDeleteBuffers    = (GLDELETEBUFFERSARBPROC)  jimglGetProcAddress("glDeleteBuffers");
                pglGenBuffers       = (GLGENBUFFERSARBPROC)     jimglGetProcAddress("glGenBuffers");
                pglMapBuffer        = (GLMAPBUFFERPROC)         jimglGetProcAddress("glMapBuffer");
                pglUnmapBuffer      = (GLUNMAPBUFFERPROC)       jimglGetProcAddress("glUnmapBuffer");
                pglBindBuffer       = (GLBINDBUFFERPROC)        jimglGetProcAddress("glBindBuffer");

                UINT lastErr = GetLastError();

                if(pglBufferData && pglDeleteBuffers && pglGenBuffers && pglMapBuffer && pglUnmapBuffer && pglBindBuffer)
                {
                    glHookSwapBuffers.Hook((FARPROC)SwapBuffers, (FARPROC)SwapBuffersHook);
                    glHookSwapLayerBuffers.Hook((FARPROC)jimglSwapLayerBuffers, (FARPROC)wglSwapLayerBuffersHook);
                    glHookDeleteContext.Hook((FARPROC)jimglDeleteContext, (FARPROC)wglDeleteContextHook);
                    bSuccess = true;
                }

                jimglMakeCurrent(NULL, NULL);

                jimglDeleteContext(hGlrc);

                if(bSuccess)
                {
                    glHookSwapBuffers.Rehook();
                    glHookSwapLayerBuffers.Rehook();
                    glHookDeleteContext.Rehook();
                }
            }

            DeleteDC(hDC);
        }
    }

    return bSuccess;
}

void FreeGLCapture()
{
    glHookSwapBuffers.Unhook();
    glHookSwapLayerBuffers.Unhook();
    glHookDeleteContext.Unhook();
}