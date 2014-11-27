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
 * @file <DeviceDX11.cpp>
 *
 * @brief Source file for DX11 Device
 *
 *******************************************************************************
 */
#include "stdafx.h"
#include "..\inc\CmdLogger.h"
#include "..\inc\DeviceDX11.h"
#include <set>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

DeviceDX11::DeviceDX11()
    :m_adaptersCount(0)
{
    memset(m_adaptersIndexes, 0, sizeof(m_adaptersIndexes));
}

DeviceDX11::~DeviceDX11()
{
    Terminate();
}

ATL::CComPtr<ID3D11Device>      DeviceDX11::GetDevice()
{
    return m_pD3DDevice;
}

AMF_RESULT DeviceDX11::Init(amf_uint32 adapterID, bool onlyWithOutputs)
{
    HRESULT hr = S_OK;
    AMF_RESULT err = AMF_OK;
    // find adapter
    ATL::CComPtr<IDXGIAdapter> pAdapter;

    EnumerateAdapters(onlyWithOutputs);
    CHECK_RETURN(m_adaptersCount > adapterID, AMF_INVALID_ARG, L"Invalid Adapter ID");

    //convert logical id to real index
    adapterID = m_adaptersIndexes[adapterID];

    ATL::CComPtr<IDXGIFactory> pFactory;
    hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void **)&pFactory);
    if(FAILED(hr))
    {
        Log(TEXT("CreateDXGIFactory failed. Error: %08X"), hr);
        return AMF_FAIL;
    }

    if(pFactory->EnumAdapters(adapterID, &pAdapter) == DXGI_ERROR_NOT_FOUND)
    {
		Log(TEXT("AdapterID = %d not found."), adapterID);
        return AMF_FAIL;
    }

    DXGI_ADAPTER_DESC desc;
    pAdapter->GetDesc(&desc);

    char strDevice[100];
    _snprintf_s(strDevice, 100, "%X", desc.DeviceId);

    Log(TEXT("AMF DX11 : Using device %d: Device ID: %S [%s]"), adapterID, strDevice, desc.Description);

    ATL::CComPtr<IDXGIOutput> pOutput;
    if(SUCCEEDED(pAdapter->EnumOutputs(0, &pOutput)))
    {
        DXGI_OUTPUT_DESC outputDesc;
        pOutput->GetDesc(&outputDesc);
        m_displayDeviceName = outputDesc.DeviceName;
    }
    ATL::CComPtr<ID3D11Device> pD3D11Device;
    ATL::CComPtr<ID3D11DeviceContext>  pD3D11Context;
    UINT createDeviceFlags = 0;

    HMONITOR hMonitor = NULL;
    DWORD vp = 0;

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };
    D3D_FEATURE_LEVEL featureLevel;

    D3D_DRIVER_TYPE eDriverType = pAdapter != NULL ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;
    hr = D3D11CreateDevice(pAdapter, eDriverType, NULL, createDeviceFlags, featureLevels, _countof(featureLevels),
                D3D11_SDK_VERSION, &pD3D11Device, &featureLevel, &pD3D11Context);
	bool isDx111 = true;
    if(FAILED(hr))
    {
		isDx111 = false;
        Log(L"InitDX11() failed to create HW DX11.1 device ");
        hr = D3D11CreateDevice(pAdapter, eDriverType, NULL, createDeviceFlags, featureLevels + 1, _countof(featureLevels) - 1,
                    D3D11_SDK_VERSION, &pD3D11Device, &featureLevel, &pD3D11Context);
    }
    else
    {
        Log(L"InitDX11() created HW DX11.1 device");
    }
    if(FAILED(hr))
    {
        Log(L"InitDX11() failed to create HW DX11 device ");
        hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_SOFTWARE, NULL, createDeviceFlags, featureLevels, _countof(featureLevels),
                    D3D11_SDK_VERSION, &pD3D11Device, &featureLevel, &pD3D11Context);
    }
	else if (!isDx111)
    {
        Log(L"InitDX11() created HW DX11 device");
    }

    if(FAILED(hr))
    {
        Log(L"InitDX11() failed to create SW DX11.1 device ");
        hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_SOFTWARE, NULL, createDeviceFlags, featureLevels + 1, _countof(featureLevels) - 1,
                    D3D11_SDK_VERSION, &pD3D11Device, &featureLevel, &pD3D11Context);
    }
    if(FAILED(hr))
    {
        Log(L"InitDX11() failed to create SW DX11 device ");
    }

    m_pD3DDevice = pD3D11Device;

    return AMF_OK;
}

AMF_RESULT DeviceDX11::Terminate()
{
    m_pD3DDevice.Release();
    return AMF_OK;
}

void DeviceDX11::EnumerateAdapters(bool onlyWithOutputs)
{
    ATL::CComPtr<IDXGIFactory> pFactory;
    HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void **)&pFactory);
    if(FAILED(hr))
    {
        Log(TEXT("CreateDXGIFactory failed. Error: %08X"), hr);
        return;
    }

    Log(TEXT("DX11: List of adapters:"));
    UINT count = 0;
    m_adaptersCount = 0;
    while(true)
    {
        ATL::CComPtr<IDXGIAdapter> pAdapter;
        if(pFactory->EnumAdapters(count, &pAdapter) == DXGI_ERROR_NOT_FOUND)
        {
            break;
        }

        DXGI_ADAPTER_DESC desc;
        pAdapter->GetDesc(&desc);

        if(desc.VendorId != 0x1002)
        {
            count++;
            continue;
        }
        ATL::CComPtr<IDXGIOutput> pOutput;
        if(onlyWithOutputs && pAdapter->EnumOutputs(0, &pOutput) == DXGI_ERROR_NOT_FOUND)
        {
            count++;
            continue;
        }
        char strDevice[100];
        _snprintf_s(strDevice, 100, "%X", desc.DeviceId);

        Log(TEXT("    %d: Device ID: %S [%s]"), m_adaptersCount, strDevice, desc.Description);
        m_adaptersIndexes[m_adaptersCount] = count;
        m_adaptersCount++;
        count++;
    }
}

