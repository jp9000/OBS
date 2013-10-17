/* ****************************************************************************** *\

Copyright (C) 2012-2013 Intel Corporation.  All rights reserved.

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

File Name: mfx_library_iterator.cpp

\* ****************************************************************************** */

#include "mfx_library_iterator.h"

#include "mfx_dispatcher.h"
#include "mfx_dispatcher_log.h"

#include "mfx_dxva2_device.h"

namespace MFX
{

enum
{
    MFX_MAX_MERIT               = 0x7fffffff
};

//
// declare registry keys
//

const
wchar_t rootDispPath[] = L"Software\\Intel\\MediaSDK\\Dispatch";
const
wchar_t vendorIDKeyName[] = L"VendorID";
const
wchar_t deviceIDKeyName[] = L"DeviceID";
const
wchar_t meritKeyName[] = L"Merit";
const
wchar_t pathKeyName[] = L"Path";
const
wchar_t apiVersionName[] = L"APIVersion";

MFXLibraryIterator::MFXLibraryIterator(void)
{
    m_implType = MFX_LIB_PSEUDO;

    m_vendorID = 0;
    m_deviceID = 0;

    m_lastLibIndex = 0;
    m_lastLibMerit = MFX_MAX_MERIT;

} // MFXLibraryIterator::MFXLibraryIterator(void)

MFXLibraryIterator::~MFXLibraryIterator(void)
{
    Release();

} // MFXLibraryIterator::~MFXLibraryIterator(void)

void MFXLibraryIterator::Release(void)
{
    m_implType = MFX_LIB_PSEUDO;

    m_vendorID = 0;
    m_deviceID = 0;

    m_lastLibIndex = 0;
    m_lastLibMerit = MFX_MAX_MERIT;

} // void MFXLibraryIterator::Release(void)

mfxStatus MFXLibraryIterator::Init(eMfxImplType implType, mfxIMPL implInterface, const mfxU32 adapterNum, int storageID)
{
    DXVA2Device dxvaDevice;
    HKEY rootHKey;
    bool bRes;

    // check error(s)
    if ((MFX_LIB_SOFTWARE != implType) &&
        (MFX_LIB_HARDWARE != implType))
    {
        return MFX_ERR_UNSUPPORTED;
    }

    // release the object before initialization
    Release();

    // open required registry key
    rootHKey = (MFX_LOCAL_MACHINE_KEY == storageID) ? (HKEY_LOCAL_MACHINE) : (HKEY_CURRENT_USER);
    bRes = m_baseRegKey.Open(rootHKey, rootDispPath, KEY_READ);
    if (false == bRes)
    {
        DISPATCHER_LOG_WRN((("Can't open %s\\%S : RegOpenKeyExA()==0x%x\n"),
                       (MFX_LOCAL_MACHINE_KEY == storageID) ? ("HKEY_LOCAL_MACHINE") : ("HKEY_CURRENT_USER"),
                       rootDispPath, GetLastError()))
        return MFX_ERR_UNKNOWN;
    }

    // set the required library's implementation type
    m_implType = implType;
    m_implInterface = implInterface != 0 
                    ? implInterface
                    : MFX_IMPL_VIA_ANY;

    if (MFX_IMPL_VIA_D3D9 == m_implInterface)
    {
        // try to create the Direct3D 9 device and find right adapter
        if (!dxvaDevice.InitD3D9(adapterNum))
        {
            DISPATCHER_LOG_INFO((("dxvaDevice.InitD3D9(%d) Failed "), adapterNum ));
            return MFX_ERR_UNSUPPORTED;
        }        
    }
    else if (MFX_IMPL_VIA_D3D11 == m_implInterface)
    {
        // try to open DXGI 1.1 device to get hardware ID
        if (!dxvaDevice.InitDXGI1(adapterNum))
        {
            DISPATCHER_LOG_INFO((("dxvaDevice.InitDXGI1(%d) Failed "), adapterNum ));
            return MFX_ERR_UNSUPPORTED;
        }
    } 
    else if (MFX_IMPL_VIA_ANY == m_implInterface)
    {
        // try the Direct3D 9 device
        if (dxvaDevice.InitD3D9(adapterNum))
        {
            m_implInterface = MFX_IMPL_VIA_D3D9; // store value for GetImplementationType() call
        }
        // else try to open DXGI 1.1 device to get hardware ID
        else if (dxvaDevice.InitDXGI1(adapterNum))
        {
            m_implInterface = MFX_IMPL_VIA_D3D11; // store value for GetImplementationType() call
        }
        else
        {
            DISPATCHER_LOG_INFO((("Unsupported adapter %d "), adapterNum ));
            return MFX_ERR_UNSUPPORTED;
        }
    }
    else
    {
        DISPATCHER_LOG_ERROR((("Unknown implementation type %d "), m_implInterface ));
        return MFX_ERR_UNSUPPORTED;
    }

    // obtain card's parameters
    m_vendorID = dxvaDevice.GetVendorID();
    m_deviceID = dxvaDevice.GetDeviceID();

    DISPATCHER_LOG_INFO((("Inspecting %s\\%S\n"),
                   (MFX_LOCAL_MACHINE_KEY == storageID) ? ("HKEY_LOCAL_MACHINE") : ("HKEY_CURRENT_USER"),
                   rootDispPath))

    return MFX_ERR_NONE;

} // mfxStatus MFXLibraryIterator::Init(eMfxImplType implType, const mfxU32 adapterNum, int storageID)

mfxStatus MFXLibraryIterator::SelectDLLVersion(wchar_t *pPath, size_t pathSize,
                                               eMfxImplType *pImplType, mfxVersion minVersion)
{
    wchar_t libPath[MFX_MAX_DLL_PATH];
    DWORD libIndex = 0;
    DWORD libMerit = 0;
    DWORD index;
    bool enumRes;

    // main query cycle
    index = 0;
    do
    {
        WinRegKey subKey;
        wchar_t subKeyName[MFX_MAX_VALUE_NAME];
        DWORD subKeyNameSize = sizeof(subKeyName) / sizeof(subKeyName[0]);

        // query next value name
        enumRes = m_baseRegKey.EnumKey(index, subKeyName, &subKeyNameSize);
        if (!enumRes)
        {
            DISPATCHER_LOG_WRN((("no more subkeys : RegEnumKeyExA()==0x%x\n"), GetLastError()))
        }
        else
        {
            DISPATCHER_LOG_INFO((("found subkey: %S\n"), subKeyName))

            bool bRes;

            // open the sub key
            bRes = subKey.Open(m_baseRegKey, subKeyName, KEY_READ);
            if (!bRes)
            {
                DISPATCHER_LOG_WRN((("error opening key %S :RegOpenKeyExA()==0x%x\n"), subKeyName, GetLastError()));
            }
            else
            {
                DISPATCHER_LOG_INFO((("opened key: %S\n"), subKeyName));

                mfxU32 vendorID = 0, deviceID = 0, merit = 0, version;
                DWORD size;

                // query version value
                size = sizeof(version);
                bRes = subKey.Query(apiVersionName, REG_DWORD, (LPBYTE) &version, &size);
                if (!bRes)
                {
                    DISPATCHER_LOG_WRN((("querying %S : RegQueryValueExA()==0x%x\n"), apiVersionName, GetLastError()));
                }
                else
                {
                    mfxVersion libVersion;

                    // there is complex conversion for registry stored version to
                    // the mfxVersion structure
                    libVersion.Minor = (mfxU16) (version & 0x0ff);
                    libVersion.Major = (mfxU16) (version >> 8);

                    if ((libVersion.Major != minVersion.Major) ||
                        (libVersion.Minor < minVersion.Minor))
                    {
                        // there is a version conflict
                        bRes = false;
                        DISPATCHER_LOG_WRN((("version conflict: loaded : %u.%u required = %u.%u\n"), libVersion.Major, libVersion.Minor, minVersion.Major, minVersion.Minor));
                    }
                    else
                    {
                        DISPATCHER_LOG_INFO((("loaded %S : %u.%u \n"), apiVersionName, libVersion.Major, libVersion.Minor));
                    }
                }
                // query vendor and device IDs
                if (bRes)
                {
                    size = sizeof(vendorID);
                    bRes = subKey.Query(vendorIDKeyName, REG_DWORD, (LPBYTE) &vendorID, &size);
                    DISPATCHER_LOG_OPERATION({
                        if (bRes)
                        {
                            DISPATCHER_LOG_INFO((("loaded %S : 0x%x\n"), vendorIDKeyName, vendorID));
                        }
                        else
                        {
                            DISPATCHER_LOG_WRN((("querying %S : RegQueryValueExA()==0x%x\n"), vendorIDKeyName, GetLastError()));
                        }
                    })
                }
                if (bRes)
                {
                    size = sizeof(deviceID);
                    bRes = subKey.Query(deviceIDKeyName, REG_DWORD, (LPBYTE) &deviceID, &size);
                    DISPATCHER_LOG_OPERATION({
                        if (bRes)
                        {
                            DISPATCHER_LOG_INFO((("loaded %S : 0x%x\n"), deviceIDKeyName, deviceID));
                        }
                        else
                        {
                            DISPATCHER_LOG_WRN((("querying %S : RegQueryValueExA()==0x%x\n"), deviceIDKeyName, GetLastError()));
                        }
                    })
                }
                // query merit value
                if (bRes)
                {
                    size = sizeof(merit);
                    bRes = subKey.Query(meritKeyName, REG_DWORD, (LPBYTE) &merit, &size);
                    DISPATCHER_LOG_OPERATION({
                        if (bRes)
                        {
                            DISPATCHER_LOG_INFO((("loaded %S : %d\n"), meritKeyName, merit));
                        }
                        else
                        {
                            DISPATCHER_LOG_WRN((("querying %S : RegQueryValueExA()==0x%x\n"), meritKeyName, GetLastError()));
                        }
                    })
                }

                // if the library fits required parameters,
                // query the library's path
                if (bRes)
                {
                    // compare device's and library's IDs
                    if (MFX_LIB_HARDWARE == m_implType)
                    {
                        if (m_vendorID != vendorID) 
                        {
                            bRes = false;
                            DISPATCHER_LOG_WRN((("%S conflict, actual = 0x%x : required = 0x%x\n"), vendorIDKeyName, m_vendorID, vendorID));
                        }
                        if (bRes && m_deviceID != deviceID)
                        {
                            bRes = false;
                            DISPATCHER_LOG_WRN((("%S conflict, actual = 0x%x : required = 0x%x\n"), deviceIDKeyName, m_deviceID, deviceID));
                        }
                    }
                    else if (MFX_LIB_SOFTWARE == m_implType)
                    {
                        if (0 != vendorID) 
                        {
                            bRes = false;
                            DISPATCHER_LOG_WRN((("%S conflict, required = 0x%x shoul be 0 for software implementation\n"), vendorIDKeyName, vendorID));
                        }
                        if (bRes && 0 != deviceID)
                        {
                            bRes = false;
                            DISPATCHER_LOG_WRN((("%S conflict, required = 0x%x should be 0 for software implementation\n"), deviceIDKeyName, deviceID));
                        }
                    }

                    DISPATCHER_LOG_OPERATION({
                    if (bRes)
                    {
                        if (!(((m_lastLibMerit > merit) || ((m_lastLibMerit == merit) && (m_lastLibIndex < index))) &&
                             (libMerit < merit)))
                        {
                            DISPATCHER_LOG_WRN((("merit conflict: lastMerit = 0x%x, requiredMerit = 0x%x, libraryMerit = 0x%x, lastindex = %d, index = %d\n")
                                        , m_lastLibMerit, merit, libMerit, m_lastLibIndex, index));
                        }
                    }})

                    if ((bRes) &&
                        ((m_lastLibMerit > merit) || ((m_lastLibMerit == merit) && (m_lastLibIndex < index))) &&
                        (libMerit < merit))
                    {
                        wchar_t tmpPath[MFX_MAX_DLL_PATH];
                        DWORD tmpPathSize = sizeof(tmpPath) / sizeof(tmpPath[0]);

                        bRes = subKey.Query(pathKeyName, REG_SZ, (LPBYTE) tmpPath, &tmpPathSize);
                        if (!bRes)
                        {
                            DISPATCHER_LOG_WRN((("error querying %S : RegQueryValueExA()==0x%x\n"), pathKeyName, GetLastError()));
                        }
                        else
                        {
                            DISPATCHER_LOG_INFO((("loaded %S : %S\n"), pathKeyName, tmpPath));
                         
                            // copy the library's path
#if _MSC_VER >= 1400
                            wcscpy_s(libPath, sizeof(libPath) / sizeof(libPath[0]), tmpPath);
#else
                            wcscpy(libPath, tmpPath);
#endif

                            libMerit = merit;
                            libIndex = index;

                            // set the library's type
                            if ((0 == vendorID) || (0 == deviceID))
                            {
                                *pImplType = MFX_LIB_SOFTWARE;
                                
                                DISPATCHER_LOG_INFO((("Library type is MFX_LIB_SOFTWARE\n")));
                            }
                            else
                            {
                                *pImplType = MFX_LIB_HARDWARE;
                                DISPATCHER_LOG_INFO((("Library type is MFX_LIB_HARDWARE\n")));
                            }
                        }
                    }
                }
            }
        }

        // advance key index
        index += 1;

    } while (enumRes);

    // if the library's path was successfully read,
    // the merit variable holds valid value
    if (0 == libMerit)
    {
        return MFX_ERR_NOT_FOUND;
    }

#if _MSC_VER >= 1400
    wcscpy_s(pPath, pathSize, libPath);
#else
    wcscpy(pPath, libPath);
#endif

    m_lastLibIndex = libIndex;
    m_lastLibMerit = libMerit;

    return MFX_ERR_NONE;

} // mfxStatus MFXLibraryIterator::SelectDLLVersion(wchar_t *pPath, size_t pathSize, eMfxImplType *pImplType, mfxVersion minVersion)

mfxIMPL MFXLibraryIterator::GetImplementationType()
{
    return m_implInterface;
} // mfxIMPL MFXLibraryIterator::GetImplementationType()

} // namespace MFX

