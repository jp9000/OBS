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

File Name: mfx_dxva2_device.h

\* ****************************************************************************** */

#if !defined(__MFX_DXVA2_DEVICE_H)
#define __MFX_DXVA2_DEVICE_H

#include <windows.h>

#include <mfxdefs.h>

#ifdef DXVA2DEVICE_LOG
#include <stdio.h>
#define DXVA2DEVICE_TRACE(expr) printf expr;
#define DXVA2DEVICE_TRACE_OPERATION(expr) expr;
#else
#define DXVA2DEVICE_TRACE(expr)
#define DXVA2DEVICE_TRACE_OPERATION(expr) 
#endif

namespace MFX
{

class DXDevice
{
public:
    // Default constructor
    DXDevice(void);
    // Destructor
    virtual
    ~DXDevice(void) = 0;

    // Initialize device using DXGI 1.1 interface
    virtual
    bool Init(const mfxU32 adapterNum) = 0;

    // Obtain graphic card's parameter
    mfxU32 GetVendorID(void) const;
    mfxU32 GetDeviceID(void) const;
    mfxU64 GetDriverVersion(void) const;
    mfxU64 GetLUID(void) const;

    // Provide the number of available adapters
    mfxU32 GetAdapterCount(void) const;

    // Close the object
    virtual
    void Close(void);

    // Load the required DLL module
    void LoadDLLModule(const wchar_t *pModuleName);

protected:

    // Free DLL module
    void UnloadDLLModule(void);

    // Handle to the DLL library
    HMODULE m_hModule;

    // Number of adapters available
    mfxU32 m_numAdapters;

    // Vendor ID
    mfxU32 m_vendorID;
    // Device ID
    mfxU32 m_deviceID;
    // x.x.x.x each x of two bytes
    mfxU64 m_driverVersion;
    // LUID
    mfxU64 m_luid;

private:
    // unimplemented by intent to make this class and its descendants non-copyable
    DXDevice(const DXDevice &);
    void operator=(const DXDevice &);
};

class DXGI1Device : public DXDevice
{
public:
    // Default constructor
    DXGI1Device(void);
    // Destructor
    virtual
    ~DXGI1Device(void);

    // Initialize device
    virtual
    bool Init(const mfxU32 adapterNum);

    // Close the object
    virtual
    void Close(void);

protected:

    // Pointer to the DXGI1 factory
    void *m_pDXGIFactory1;
    // Pointer to the current DXGI1 adapter
    void *m_pDXGIAdapter1;

};

class DXVA2Device
{
public:
    // Default constructor
    DXVA2Device(void);
    // Destructor
    ~DXVA2Device(void);

    // Initialize device using DXGI 1.1 interface
    bool InitDXGI1(const mfxU32 adapterNum);

    // Obtain graphic card's parameter
    mfxU32 GetVendorID(void) const;
    mfxU32 GetDeviceID(void) const;
    mfxU64 GetDriverVersion(void) const;

    // Provide the number of available adapters
    mfxU32 GetAdapterCount(void) const;

    void Close(void);

protected:

    // Number of adapters available
    mfxU32 m_numAdapters;

    // Vendor ID
    mfxU32 m_vendorID;
    // Device ID
    mfxU32 m_deviceID;
    //x.x.x.x
    mfxU64 m_driverVersion;

private:
    // unimplemented by intent to make this class non-copyable
    DXVA2Device(const DXVA2Device &);
    void operator=(const DXVA2Device &);
};

} // namespace MFX

#endif // __MFX_DXVA2_DEVICE_H
