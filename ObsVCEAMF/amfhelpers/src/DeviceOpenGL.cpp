/*******************************************************************************
 Copyright ©2014 Advanced Micro Devices, Inc. All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1   Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.
 2   Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/

/**
 *******************************************************************************
 * @file <DeviceOpenGL.cpp>
 *
 * @brief Source file for OpenGL Device
 *
 *******************************************************************************
 */

#include "DeviceOpenGL.h"
#include "CmdLogger.h"
#include <set>

#pragma comment(lib, "opengl32.lib")


static bool ReportGLErrors(const char* file, int line)
{
    unsigned int err = glGetError();
    if(err != GL_NO_ERROR)
    {
        std::wstringstream ss;
        ss << "GL failed. Error numbers: ";
        while(err != GL_NO_ERROR)
        {
            ss  << std::hex << err << ", ";
            err = glGetError();
        }
        ss << std::dec << " file: " << file << ", line: " << line;
        LOG_ERROR(ss.str());
        return false;
    }
    return true;
}


#define CALL_GL(exp) \
{ \
    exp; \
    if( !ReportGLErrors(__FILE__, __LINE__) ) return AMF_FAIL; \
}

DeviceOpenGL::DeviceOpenGL() : 
    m_hDC(NULL),
    m_hContextOGL(NULL),
    m_hWnd(NULL),
    m_adaptersCount(0)
{
    memset(m_adaptersIndexes, 0, sizeof(m_adaptersIndexes));
}

DeviceOpenGL::~DeviceOpenGL()
{
    Terminate();
}

AMF_RESULT DeviceOpenGL::Init(HWND hWnd, const wchar_t* displayDeviceName)
{
    DWORD error = 0;

    if(hWnd == ::GetDesktopWindow() && displayDeviceName)
    {
        LOG_INFO("OPENGL : Choosen Device " << displayDeviceName);
        m_hDC = CreateDC(displayDeviceName, NULL, NULL, NULL);
    }
    else
    {
        m_hWnd = hWnd;
        m_hDC = GetDC(hWnd);
    }
    if(!m_hDC) 
    {
        LOG_ERROR("Could not GetDC. Error: " << GetLastError());
        return AMF_FAIL;
    }

    static    PIXELFORMATDESCRIPTOR pfd=                // pfd Tells Windows How We Want Things To Be
    {
        sizeof(PIXELFORMATDESCRIPTOR),                  // Size Of This Pixel Format Descriptor
        1,                                              // Version Number
        PFD_DRAW_TO_WINDOW |                            // Format Must Support Window
        PFD_SUPPORT_OPENGL |                            // Format Must Support OpenGL
        PFD_DOUBLEBUFFER,                               // Must Support Double Buffering
        PFD_TYPE_RGBA,                                  // Request An RGBA Format
        24,                                             // Select Our Color Depth
        0, 0, 0, 0, 0, 0,                               // Color Bits Ignored
        0,                                              // No Alpha Buffer
        0,                                              // Shift Bit Ignored
        0,                                              // No Accumulation Buffer
        0, 0, 0, 0,                                     // Accumulation Bits Ignored
        16,                                             // 16Bit Z-Buffer (Depth Buffer)  
        0,                                              // No Stencil Buffer
        0,                                              // No Auxiliary Buffer
        PFD_MAIN_PLANE,                                 // Main Drawing Layer
        0,                                              // Reserved
        0, 0, 0                                         // Layer Masks Ignored
    };

    GLuint pixelFormat = (GLuint)ChoosePixelFormat(m_hDC, &pfd);

    BOOL res = SetPixelFormat(m_hDC, (int)pixelFormat, &pfd);
    if(!res) 
    {
        LOG_ERROR("SetPixelFormat() failed. Error: " << GetLastError());
        return AMF_FAIL;
    }

    m_hContextOGL= wglCreateContext(m_hDC);
    if(!m_hContextOGL) 
    {
        LOG_ERROR("wglCreateContext()  failed. Error: " << GetLastError());
        return AMF_FAIL;
    }
    return AMF_OK;
}

AMF_RESULT DeviceOpenGL::Terminate()
{
    if(m_hContextOGL)
    {
        wglMakeCurrent( NULL , NULL );
        wglDeleteContext(m_hContextOGL);
        m_hContextOGL = NULL;
    }
    if(m_hDC)
    {
        if(m_hWnd == NULL)
        {
            ::DeleteDC(m_hDC);
        }
        else
        {
            ::ReleaseDC(m_hWnd, m_hDC);
        }
        m_hDC = NULL;
        m_hWnd = NULL;
    }
    return AMF_OK;
}
