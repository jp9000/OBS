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
typedef BOOL   (WINAPI * WGLSWAPBUFFERSPROC)(HDC);
typedef BOOL   (WINAPI * WGLDELETECONTEXTPROC)(HGLRC);
typedef PROC   (WINAPI * WGLGETPROCADDRESSPROC)(LPCSTR);
typedef BOOL   (WINAPI * WGLMAKECURRENTPROC)(HDC, HGLRC);
typedef HGLRC  (WINAPI * WGLCREATECONTEXTPROC)(HDC);

GLREADBUFFERPROC                pglReadBuffer       = NULL;
GLREADPIXELSPROC                pglReadPixels       = NULL;
GLGETERRORPROC                  pglGetError         = NULL;
WGLSWAPLAYERBUFFERSPROC         pwglSwapLayerBuffers= NULL;
WGLSWAPBUFFERSPROC              pwglSwapBuffers     = NULL;
WGLDELETECONTEXTPROC            pwglDeleteContext   = NULL;
WGLGETPROCADDRESSPROC           pwglGetProcAddress  = NULL;
WGLMAKECURRENTPROC              pwglMakeCurrent     = NULL;
WGLCREATECONTEXTPROC            pwglCreateContext   = NULL;

#define glReadBuffer            (*pglReadBuffer)
#define glReadPixels            (*pglReadPixels)
#define glGetError              (*pglGetError)
#define jimglSwapLayerBuffers   (*pwglSwapLayerBuffers)
#define jimglSwapBuffers        (*pwglSwapBuffers)
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
HookData                glHookwglSwapBuffers;
HookData                glHookDeleteContext;

//2 buffers turns out to be more optimal than 3 -- seems that cache has something to do with this in GL
#define                 NUM_BUFFERS 2
#define                 ZERO_ARRAY {0, 0}

GLuint                  gltextures[NUM_BUFFERS] = ZERO_ARRAY;
HDC                     hdcAcquiredDC = NULL;
HWND                    hwndTarget = NULL;

extern HANDLE           hCopyThread;
extern HANDLE           hCopyEvent;
extern bool             bKillThread;
HANDLE                  glDataMutexes[NUM_BUFFERS];
extern void             *pCopyData;
extern DWORD            curCPUTexture;

bool                    glLockedTextures[NUM_BUFFERS];
extern MemoryCopyData   *copyData;
extern LPBYTE           textureBuffers[2];
extern DWORD            curCapture;
extern BOOL             bHasTextures;
extern LONGLONG         frameTime;
extern DWORD            fps;
extern DWORD            copyWait;
extern LONGLONG         lastTime;

CaptureInfo             glcaptureInfo;


void ClearGLData()
{
    if(copyData)
        copyData->lastRendered = -1;

    if(hCopyThread)
    {
        bKillThread = true;
        SetEvent(hCopyEvent);
        if(WaitForSingleObject(hCopyThread, 500) != WAIT_OBJECT_0)
            TerminateThread(hCopyThread, -1);

        CloseHandle(hCopyThread);
        CloseHandle(hCopyEvent);

        hCopyThread = NULL;
        hCopyEvent = NULL;
    }

    for(int i=0; i<NUM_BUFFERS; i++)
    {
        if(glLockedTextures[i])
        {
            OSEnterMutex(glDataMutexes[i]);

            glBindBuffer(GL_PIXEL_PACK_BUFFER, gltextures[i]);
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

            glLockedTextures[i] = false;

            OSLeaveMutex(glDataMutexes[i]);
        }
    }

    if(bHasTextures)
    {
        glDeleteBuffers(NUM_BUFFERS, gltextures);
        bHasTextures = false;

        ZeroMemory(gltextures, sizeof(gltextures));
    }

    for(int i=0; i<NUM_BUFFERS; i++)
    {
        if(glDataMutexes[i])
        {
            OSCloseMutex(glDataMutexes[i]);
            glDataMutexes[i] = NULL;
        }
    }

    DestroySharedMemory();
    copyData = NULL;
    copyWait = 0;
    lastTime = 0;
    fps = 0;
    frameTime = 0;
    curCapture = 0;
    curCPUTexture = 0;
    pCopyData = NULL;
}

DWORD CopyGLCPUTextureThread(LPVOID lpUseless);

void DoGLCPUHook(RECT &rc)
{
    glcaptureInfo.cx = rc.right;
    glcaptureInfo.cy = rc.bottom;

    glGenBuffers(NUM_BUFFERS, gltextures);

    DWORD dwSize = glcaptureInfo.cx*glcaptureInfo.cy*4;

    BOOL bSuccess = true;
    for(UINT i=0; i<NUM_BUFFERS; i++)
    {
        UINT test = 0;

        glBindBuffer(GL_PIXEL_PACK_BUFFER, gltextures[i]);
        glBufferData(GL_PIXEL_PACK_BUFFER, dwSize, 0, GL_STREAM_READ);

        if(!(glDataMutexes[i] = OSCreateMutex()))
        {
            logOutput << "DoGLCPUHook: OSCreateMutex " << i << " failed, GetLastError = " << GetLastError();
            bSuccess = false;
            break;
        }
    }

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    if(bSuccess)
    {
        bKillThread = false;

        if(hCopyThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CopyGLCPUTextureThread, NULL, 0, NULL))
        {
            if(!(hCopyEvent = CreateEvent(NULL, FALSE, FALSE, NULL)))
            {
                logOutput << "DoGLCPUHook: CreateEvent failed, GetLastError = " << GetLastError();
                bSuccess = false;
            }
        }
        else
        {
            logOutput << "DoGLCPUHook: CreateThread failed, GetLastError = " << GetLastError();
            bSuccess = false;
        }
    }

    if(bSuccess)
    {
        glcaptureInfo.mapID = InitializeSharedMemoryCPUCapture(dwSize, &glcaptureInfo.mapSize, &copyData, textureBuffers);
        if(!glcaptureInfo.mapID)
            bSuccess = false;
    }

    if(bSuccess)
        bSuccess = IsWindow(hwndReceiver);

    if(bSuccess)
    {
        bHasTextures = true;
        glcaptureInfo.captureType = CAPTURETYPE_MEMORY;
        glcaptureInfo.hwndSender = hwndSender;
        glcaptureInfo.hwndCapture = hwndTarget;
        glcaptureInfo.pitch = glcaptureInfo.cx*4;
        glcaptureInfo.bFlip = TRUE;
        fps = (DWORD)SendMessage(hwndReceiver, RECEIVER_NEWCAPTURE, 0, (LPARAM)&glcaptureInfo);
        frameTime = (fps) ? 1000000/LONGLONG(fps) : 0;

        logOutput << "DoGLCPUHook: success, fps = " << fps << ", frameTime = " << frameTime << endl;

        OSInitializeTimer();
    }
    else
        ClearGLData();
}

DWORD CopyGLCPUTextureThread(LPVOID lpUseless)
{
    int sharedMemID = 0;

    HANDLE hEvent = NULL;
    if(!DuplicateHandle(GetCurrentProcess(), hCopyEvent, GetCurrentProcess(), &hEvent, NULL, FALSE, DUPLICATE_SAME_ACCESS))
    {
        logOutput << "CopyGLCPUTextureThread: couldn't duplicate handle" << endl;
        return 0;
    }

    while(WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0)
    {
        if(bKillThread)
            break;

        int nextSharedMemID = sharedMemID == 0 ? 1 : 0;

        DWORD copyTex = curCPUTexture;
        LPVOID data = pCopyData;
        if(copyTex < NUM_BUFFERS && data != NULL)
        {
            OSEnterMutex(glDataMutexes[copyTex]);

            int lastRendered = -1;

            //copy to whichever is available
            if(WaitForSingleObject(textureMutexes[sharedMemID], 0) == WAIT_OBJECT_0)
                lastRendered = (int)sharedMemID;
            else if(WaitForSingleObject(textureMutexes[nextSharedMemID], 0) == WAIT_OBJECT_0)
                lastRendered = (int)nextSharedMemID;

            if(lastRendered != -1)
            {
                SSECopy(textureBuffers[lastRendered], data, glcaptureInfo.pitch*glcaptureInfo.cy);
                ReleaseMutex(textureMutexes[lastRendered]);
                copyData->lastRendered = (UINT)lastRendered;
            }

            OSLeaveMutex(glDataMutexes[copyTex]);
        }

        sharedMemID = nextSharedMemID;
    }

    CloseHandle(hEvent);
    return 0;
}

bool bReacquiring = false;
LONGLONG reacquireStart = 0;
LONGLONG reacquireTime = 0;

LONG lastCX=0, lastCY=0;

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

        OSInitializeTimer();
    }

    if(hDC == hdcAcquiredDC)
    {
        if(bCapturing && bStopRequested)
        {
            ClearGLData();
            bStopRequested = false;
            bReacquiring = false;
        }

        RECT rc;
        GetClientRect(hwndTarget, &rc);

        if(bCapturing && bReacquiring)
        {
            if(lastCX != rc.right || lastCY != rc.bottom) //reset if continuing to size within the 3 seconds
            {
                reacquireStart = OSGetTimeMicroseconds();
                lastCX = rc.right;
                lastCY = rc.bottom;
            }

            if(OSGetTimeMicroseconds()-reacquireTime >= 3000000) //3 second to reacquire
                bReacquiring = false;
            else
                return;
        }

        if(bCapturing && (!bHasTextures || rc.right != glcaptureInfo.cx || rc.bottom != glcaptureInfo.cy))
        {
            if (!rc.right || !rc.bottom)
                return;

            if(!hwndReceiver)
                hwndReceiver = FindWindow(RECEIVER_WINDOWCLASS, NULL);

            if(bHasTextures) //resizing
            {
                ClearGLData();
                bReacquiring = true;
                reacquireStart = OSGetTimeMicroseconds();
                lastCX = rc.right;
                lastCY = rc.bottom;
                return;
            }
            else
            {
                if(hwndReceiver)
                    DoGLCPUHook(rc);
                else
                    ClearGLData();
            }
        }

        if(bHasTextures)
        {
            if(bCapturing)
            {
                LONGLONG timeVal = OSGetTimeMicroseconds();
                LONGLONG timeElapsed = timeVal-lastTime;

                if(timeElapsed >= frameTime)
                {
                    lastTime += frameTime;
                    if(timeElapsed > frameTime*2)
                        lastTime = timeVal;

                    GLuint texture = gltextures[curCapture];
                    DWORD nextCapture = (curCapture == NUM_BUFFERS-1) ? 0 : (curCapture+1);

                    glReadBuffer(GL_BACK);
                    glBindBuffer(GL_PIXEL_PACK_BUFFER, texture);

                    if(glLockedTextures[curCapture])
                    {
                        OSEnterMutex(glDataMutexes[curCapture]);

                        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
                        glLockedTextures[curCapture] = false;

                        OSLeaveMutex(glDataMutexes[curCapture]);
                    }

                    glReadPixels(0, 0, glcaptureInfo.cx, glcaptureInfo.cy, GL_BGRA, GL_UNSIGNED_BYTE, 0);

                    //----------------------------------

                    glBindBuffer(GL_PIXEL_PACK_BUFFER, gltextures[nextCapture]);
                    pCopyData = (void*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
                    if(pCopyData)
                    {
                        curCPUTexture = nextCapture;
                        glLockedTextures[nextCapture] = true;

                        SetEvent(hCopyEvent);
                    }

                    //----------------------------------

                    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

                    curCapture = nextCapture;
                }
            }
            else
                ClearGLData();
        }
    }
}

void HandleGLSceneDestroy()
{
    ClearGLData();
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

BOOL WINAPI wglSwapBuffersHook(HDC hDC)
{
    HandleGLSceneUpdate(hDC);

    glHookwglSwapBuffers.Unhook();
    BOOL bResult = jimglSwapBuffers(hDC);
    glHookwglSwapBuffers.Rehook();

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
    static HWND hwndOpenGLSetupWindow = NULL;
    bool bSuccess = false;

    if(!hwndOpenGLSetupWindow)
    {
        WNDCLASSEX windowClass;

        ZeroMemory(&windowClass, sizeof(windowClass));

        windowClass.cbSize = sizeof(windowClass);
        windowClass.style = CS_OWNDC;
        windowClass.lpfnWndProc = DefWindowProc;
        windowClass.lpszClassName = TEXT("OBSOGLHookClass");
        windowClass.hInstance = hinstMain;

        if(RegisterClassEx(&windowClass))
        {
            hwndOpenGLSetupWindow = CreateWindowEx (0,
                TEXT("OBSOGLHookClass"),
                TEXT("OBS OpenGL Context Window"),
                WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                0, 0,
                1, 1,
                NULL,
                NULL,
                hinstMain,
                NULL
                );
        }
    }

    HMODULE hGL = GetModuleHandle(TEXT("opengl32.dll"));
    if(hGL && hwndOpenGLSetupWindow)
    {
        pglReadBuffer       = (GLREADBUFFERPROC)        GetProcAddress(hGL, "glReadBuffer");
        pglReadPixels       = (GLREADPIXELSPROC)        GetProcAddress(hGL, "glReadPixels");
        pglGetError         = (GLGETERRORPROC)          GetProcAddress(hGL, "glGetError");
        pwglSwapLayerBuffers= (WGLSWAPLAYERBUFFERSPROC) GetProcAddress(hGL, "wglSwapLayerBuffers");
        pwglSwapBuffers=      (WGLSWAPBUFFERSPROC)      GetProcAddress(hGL, "wglSwapBuffers");
        pwglDeleteContext   = (WGLDELETECONTEXTPROC)    GetProcAddress(hGL, "wglDeleteContext");
        pwglGetProcAddress  = (WGLGETPROCADDRESSPROC)   GetProcAddress(hGL, "wglGetProcAddress");
        pwglMakeCurrent     = (WGLMAKECURRENTPROC)      GetProcAddress(hGL, "wglMakeCurrent");
        pwglCreateContext   = (WGLCREATECONTEXTPROC)    GetProcAddress(hGL, "wglCreateContext");

        if( !pglReadBuffer || !pglReadPixels || !pglGetError || !pwglSwapLayerBuffers || !pwglSwapBuffers ||
            !pwglDeleteContext || !pwglGetProcAddress || !pwglMakeCurrent || !pwglCreateContext)
        {
            return false;
        }

        HDC hDC = GetDC(hwndOpenGLSetupWindow);
        if(hDC)
        {
            PIXELFORMATDESCRIPTOR pfd;
            ZeroMemory(&pfd, sizeof(pfd));
            pfd.nSize = sizeof(pfd);
            pfd.nVersion = 1;
            pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_GENERIC_ACCELERATED;
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
                    glHookwglSwapBuffers.Hook((FARPROC)jimglSwapBuffers, (FARPROC)wglSwapBuffersHook);
                    glHookDeleteContext.Hook((FARPROC)jimglDeleteContext, (FARPROC)wglDeleteContextHook);
                    bSuccess = true;
                }

                jimglMakeCurrent(NULL, NULL);

                jimglDeleteContext(hGlrc);

                ReleaseDC(hwndOpenGLSetupWindow, hDC);

                if(bSuccess)
                {
                    glHookSwapBuffers.Rehook();
                    glHookSwapLayerBuffers.Rehook();
                    glHookwglSwapBuffers.Rehook();
                    glHookDeleteContext.Rehook();

                    DestroyWindow(hwndOpenGLSetupWindow);
                    hwndOpenGLSetupWindow = NULL;

                    UnregisterClass(TEXT("OBSOGLHookClass"), hinstMain);
                }
            }

            if(hwndOpenGLSetupWindow)
                ReleaseDC(hwndOpenGLSetupWindow, hDC);
        }
    }

    return bSuccess;
}

void FreeGLCapture()
{
    glHookSwapBuffers.Unhook();
    glHookSwapLayerBuffers.Unhook();
    glHookwglSwapBuffers.Unhook();
    glHookDeleteContext.Unhook();
}