/* ****************************************************************************** *\

Copyright (C) 2012 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfx_dxva2_device.cpp

\* ****************************************************************************** */

#include <iostream>
#include <sstream>

#define INITGUID
#include <dxgi.h>

#include "mfx_dxva2_device.h"


using namespace MFX;


DXDevice::DXDevice(void)
{
    m_hModule = (HMODULE) 0;

    m_numAdapters = 0;

    m_vendorID = 0;
    m_deviceID = 0;
    m_driverVersion = 0;
    m_luid = 0;

} // DXDevice::DXDevice(void)

DXDevice::~DXDevice(void)
{
    Close();

    // free DX library only when device is destroyed
    UnloadDLLModule();

} // DXDevice::~DXDevice(void)

mfxU32 DXDevice::GetVendorID(void) const
{
    return m_vendorID;

} // mfxU32 DXDevice::GetVendorID(void) const

mfxU32 DXDevice::GetDeviceID(void) const
{
    return m_deviceID;

} // mfxU32 DXDevice::GetDeviceID(void) const

mfxU64 DXDevice::GetDriverVersion(void) const
{
    return m_driverVersion;
    
}// mfxU64 DXDevice::GetDriverVersion(void) const

mfxU64 DXDevice::GetLUID(void) const
{
    return m_luid;

} // mfxU64 DXDevice::GetLUID(void) const

mfxU32 DXDevice::GetAdapterCount(void) const
{
    return m_numAdapters;

} // mfxU32 DXDevice::GetAdapterCount(void) const

void DXDevice::Close(void)
{
    m_numAdapters = 0;

    m_vendorID = 0;
    m_deviceID = 0;
    m_luid = 0;

} // void DXDevice::Close(void)

void DXDevice::LoadDLLModule(const wchar_t *pModuleName)
{
    UINT prevErrorMode;

    // unload the module if it is required
    UnloadDLLModule();

    // set the silent error mode
    prevErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);

    // load specified library
    m_hModule = LoadLibraryW(pModuleName);

    // set the previous error mode
    SetErrorMode(prevErrorMode);

} // void LoadDLLModule(const wchar_t *pModuleName)

void DXDevice::UnloadDLLModule(void)
{
    if (m_hModule)
    {
        FreeLibrary(m_hModule);
        m_hModule = (HMODULE) 0;
    }

} // void DXDevice::UnloaDLLdModule(void)

typedef
HRESULT (WINAPI *DXGICreateFactoryFunc) (REFIID riid, void **ppFactory);

DXGI1Device::DXGI1Device(void)
{
    m_pDXGIFactory1 = (void *) 0;
    m_pDXGIAdapter1 = (void *) 0;

} // DXGI1Device::DXGI1Device(void)

DXGI1Device::~DXGI1Device(void)
{
    Close();

} // DXGI1Device::~DXGI1Device(void)

void DXGI1Device::Close(void)
{
    // release the interfaces
    if (m_pDXGIAdapter1)
    {
        ((IDXGIAdapter1 *) m_pDXGIAdapter1)->Release();
    }

    if (m_pDXGIFactory1)
    {
        ((IDXGIFactory1 *) m_pDXGIFactory1)->Release();
    }

    m_pDXGIFactory1 = (void *) 0;
    m_pDXGIAdapter1 = (void *) 0;

} // void DXGI1Device::Close(void)

bool DXGI1Device::Init(const bool countDisplays, const mfxU32 adapterNum)
{
    // release the object before initialization
    Close();

    // load up the library if it is not loaded
    if (NULL == m_hModule)
    {
        LoadDLLModule(L"dxgi.dll");
    }

    if (m_hModule)
    {
        DXGICreateFactoryFunc pFunc;
        IDXGIFactory1 *pFactory;
        IDXGIAdapter1 *pAdapter;
        DXGI_ADAPTER_DESC1 desc;
        HRESULT hRes;

        // load address of procedure to create DXGI 1.1 factory
        pFunc = (DXGICreateFactoryFunc) GetProcAddress(m_hModule, "CreateDXGIFactory1");
        if (NULL == pFunc)
        {
            return false;
        }

        // create the factory
#if _MSC_VER >= 1400
        hRes = pFunc(__uuidof(IDXGIFactory1), (void**) (&pFactory));
#else
        hRes = pFunc(IID_IDXGIFactory1, (void**) (&pFactory));
#endif
        if (FAILED(hRes))
        {
            return false;
        }
        m_pDXGIFactory1 = pFactory;

        if(countDisplays)
        {
            UINT display = 0;
            for(UINT adapter = 0; pFactory->EnumAdapters1(adapter, &pAdapter) != DXGI_ERROR_NOT_FOUND; adapter++)
            {
                IDXGIOutput *out;
                for(UINT start_display = display; pAdapter->EnumOutputs(display-start_display, &out) != DXGI_ERROR_NOT_FOUND; display++)
                {
                    out->Release();

                    if (display != adapterNum)
                        continue;

                    m_pDXGIAdapter1 = pAdapter;
                    break;
                }

                if(m_pDXGIAdapter1)
                    break;

                pAdapter->Release();
            }

            // there is no required adapter
            if (adapterNum > display)
                return false;
        }
        else
        {
            hRes = pFactory->EnumAdapters1(adapterNum, &pAdapter);
            if(FAILED(hRes))
                return false;

            m_pDXGIAdapter1 = pAdapter;
        }
        pAdapter = (IDXGIAdapter1 *) m_pDXGIAdapter1;

        if(!pAdapter)
            return false;

        // get the adapter's parameters
        hRes = pAdapter->GetDesc1(&desc);
        if (FAILED(hRes))
        {
            return false;
        }

        // save the parameters
        m_vendorID = desc.VendorId;
        m_deviceID = desc.DeviceId;
        *((LUID *) &m_luid) = desc.AdapterLuid;
    }

    return true;

} // bool DXGI1Device::Init(const mfxU32 adapterNum)

DXVA2Device::DXVA2Device(void)
{
    m_numAdapters = 0;

    m_vendorID = 0;
    m_deviceID = 0;

} // DXVA2Device::DXVA2Device(void)

DXVA2Device::~DXVA2Device(void)
{
    Close();

} // DXVA2Device::~DXVA2Device(void)

void DXVA2Device::Close(void)
{
    m_numAdapters = 0;

    m_vendorID = 0;
    m_deviceID = 0;

} // void DXVA2Device::Close(void)

bool DXVA2Device::InitDXGI1(const bool countDisplays, const mfxU32 adapterNum)
{
    DXGI1Device dxgi1Device;
    bool bRes;

    // release the object before initialization
    Close();

    // create modern DXGI device
    bRes = dxgi1Device.Init(countDisplays, adapterNum);
    if (false == bRes)
    {
        return false;
    }

    // save the parameters and ...
    m_vendorID = dxgi1Device.GetVendorID();
    m_deviceID = dxgi1Device.GetDeviceID();
    m_numAdapters = dxgi1Device.GetAdapterCount();

    // ... say goodbye
    return true;

} // bool DXVA2Device::InitDXGI1(const mfxU32 adapterNum)

mfxU32 DXVA2Device::GetVendorID(void) const
{
    return m_vendorID;

} // mfxU32 DXVA2Device::GetVendorID(void) const

mfxU32 DXVA2Device::GetDeviceID(void) const
{
    return m_deviceID;

} // mfxU32 DXVA2Device::GetDeviceID(void) const

mfxU64 DXVA2Device::GetDriverVersion(void) const
{
    return m_driverVersion;
}// mfxU64 DXVA2Device::GetDriverVersion(void) const

mfxU32 DXVA2Device::GetAdapterCount(void) const
{
    return m_numAdapters;

} // mfxU32 DXVA2Device::GetAdapterCount(void) const
