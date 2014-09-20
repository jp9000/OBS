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
 * @file <DeviceDX11.h>
 *
 * @brief Header file for DX11 Device
 *
 *******************************************************************************
 */

#pragma once
//#include <SDKDDKVer.h>
#include <string>
#include <atlbase.h>
#include <d3d11.h>
#include "..\..\amf\core\Result.h"

class DeviceDX11
{
public:
    DeviceDX11();
    virtual ~DeviceDX11();

    AMF_RESULT Init(amf_uint32 adapterID, bool onlyWithOutputs = false);
    AMF_RESULT Terminate();

    ATL::CComPtr<ID3D11Device>      GetDevice();
    std::wstring GetDisplayDeviceName() { return m_displayDeviceName; }
private:
    void EnumerateAdapters(bool onlyWithOutputs);

    ATL::CComPtr<ID3D11Device>          m_pD3DDevice;

    static const amf_uint32             MAXADAPTERS = 128;
    amf_uint32                          m_adaptersCount;
    amf_uint32                          m_adaptersIndexes[MAXADAPTERS];
    std::wstring                        m_displayDeviceName;
};
