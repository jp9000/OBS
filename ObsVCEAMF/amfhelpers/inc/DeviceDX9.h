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
 * @file <DeviceDX9.h>
 *
 * @brief Header file for DX9 Device
 *
 *******************************************************************************
 */

#pragma once
#include <atlbase.h>
#include <string>
#include "d3d9.h"
#include "..\..\amf\core\Result.h"

class DeviceDX9
{
public:
    DeviceDX9();
    virtual ~DeviceDX9();

    AMF_RESULT Init(bool dx9ex, amf_uint32 adapterID, bool bFullScreen, amf_int32 width, amf_int32 height);
    AMF_RESULT Terminate();

    ATL::CComPtr<IDirect3DDevice9>     GetDevice();
    std::wstring GetDisplayDeviceName() { return m_displayDeviceName; }
private:
    AMF_RESULT EnumerateAdapters();

    ATL::CComPtr<IDirect3D9>            m_pD3D;
    ATL::CComPtr<IDirect3DDevice9>      m_pD3DDevice;

    static const amf_uint32             MAXADAPTERS = 128;
    amf_uint32                          m_adaptersCount;
    amf_uint32                          m_adaptersIndexes[MAXADAPTERS];
    std::wstring                        m_displayDeviceName;
};
