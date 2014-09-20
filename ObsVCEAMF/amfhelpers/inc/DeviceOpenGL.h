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
 * @file <DeviceOpenGL.h>
 *
 * @brief Header file for OpenGL Device
 *
 *******************************************************************************
 */

#pragma once
#include <windows.h>
#include <gl/gl.h>
#include <gl/glext.h>
#include "Result.h"

class DeviceOpenGL
{
public:
    DeviceOpenGL();
    virtual ~DeviceOpenGL();

    AMF_RESULT Init(HWND hWnd, const wchar_t* diplayName);
    AMF_RESULT Terminate();

    HDC                                 GetDCOpenGL() { return m_hDC; }
    HGLRC                               GetContextOpenGL() { return m_hContextOGL; } 
    HWND                                GetWindowOpenGL() { return m_hWnd; } 

private:
    HWND                                m_hWnd;
    HDC                                 m_hDC;
    HGLRC                               m_hContextOGL;

    static const amf_uint32             MAXADAPTERS = 128;
    amf_uint32                          m_adaptersCount;
    amf_uint32                          m_adaptersIndexes[MAXADAPTERS];
};

