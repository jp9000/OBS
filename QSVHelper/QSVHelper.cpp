/********************************************************************************
 Copyright (C) 2013 Ruwen Hahn <palana@stunned.de>

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


#include <stdio.h>
#include <windows.h>
#include <shellapi.h>

#include <fstream>
#include <sstream>
#include <utility>

#include <mfxvideo++.h>

#include "Encoder.h"
#include "IPCInfo.h"
#include "QSVStuff.h"
#include "SupportStuff.h"
#include "WindowsStuff.h"

namespace {

    enum qsv_cpu_platform
    {
        QSV_CPU_PLATFORM_UNKNOWN,
        QSV_CPU_PLATFORM_SNB = 1 << 0,
        QSV_CPU_PLATFORM_IVB = 1 << 1,
        QSV_CPU_PLATFORM_HSW = 1 << 2,
    };

    qsv_cpu_platform qsv_get_cpu_platform()
    {
        using std::string;

        int cpuInfo[4];
        __cpuid(cpuInfo, 0);

        string vendor;
        vendor += string((char*)&cpuInfo[1], 4);
        vendor += string((char*)&cpuInfo[3], 4);
        vendor += string((char*)&cpuInfo[2], 4);

        if (vendor != "GenuineIntel")
            return QSV_CPU_PLATFORM_UNKNOWN;

        __cpuid(cpuInfo, 1);
        BYTE model = ((cpuInfo[0] >> 4) & 0xF) + ((cpuInfo[0] >> 12) & 0xF0);
        BYTE family = ((cpuInfo[0] >> 8) & 0xF) + ((cpuInfo[0] >> 20) & 0xFF);

        // See Intel 64 and IA-32 Architectures Software Developer's Manual, Vol 3C Table 35-1
        if (family != 6)
            return QSV_CPU_PLATFORM_UNKNOWN;

        switch (model)
        {
        case 0x2a:
        case 0x2d:
            return QSV_CPU_PLATFORM_SNB;

        case 0x3a:
        case 0x3e:
            return QSV_CPU_PLATFORM_IVB;

        case 0x3c:
        case 0x45:
        case 0x46:
            return QSV_CPU_PLATFORM_HSW;
        }

        return QSV_CPU_PLATFORM_UNKNOWN;
    }

    const struct impl_parameters
    {
        mfxIMPL type,
                intf;
        mfxVersion version;
        int platforms;
    } valid_impl[] = {
        { MFX_IMPL_HARDWARE_ANY,    MFX_IMPL_VIA_D3D11, {8, 1},  QSV_CPU_PLATFORM_IVB | QSV_CPU_PLATFORM_HSW },
        { MFX_IMPL_HARDWARE_ANY,    MFX_IMPL_VIA_D3D9,  {8, 1},  QSV_CPU_PLATFORM_IVB | QSV_CPU_PLATFORM_HSW }, //Ivy Bridge+ with non-functional D3D11 support?
        { MFX_IMPL_HARDWARE_ANY,    MFX_IMPL_VIA_D3D11, {7, 1},  QSV_CPU_PLATFORM_IVB | QSV_CPU_PLATFORM_HSW },
        { MFX_IMPL_HARDWARE_ANY,    MFX_IMPL_VIA_D3D9,  {7, 1},  QSV_CPU_PLATFORM_IVB | QSV_CPU_PLATFORM_HSW }, //Ivy Bridge+ with non-functional D3D11 support?
        { MFX_IMPL_HARDWARE_ANY,    MFX_IMPL_VIA_D3D11, {6, 1},  QSV_CPU_PLATFORM_IVB | QSV_CPU_PLATFORM_HSW },
        { MFX_IMPL_HARDWARE_ANY,    MFX_IMPL_VIA_D3D9,  {6, 1},  QSV_CPU_PLATFORM_IVB | QSV_CPU_PLATFORM_HSW }, //Ivy Bridge+ with non-functional D3D11 support?
        { MFX_IMPL_HARDWARE_ANY,    MFX_IMPL_VIA_D3D9,  {4, 1},  QSV_CPU_PLATFORM_IVB | QSV_CPU_PLATFORM_HSW | QSV_CPU_PLATFORM_SNB }, //Sandy Bridge
    };

    std::wofstream log_file;

    bool HasIntelGraphics()
    {
        REFIID iidVal = __uuidof(IDXGIFactory1);

        ComPtr<IDXGIFactory1> factory;
        if (!SUCCEEDED(CreateDXGIFactory1(iidVal, (void**)factory.Assign())))
            return false;

        UINT i = 0;
        ComPtr<IDXGIAdapter1> adapter;

        while (factory->EnumAdapters1(i++, adapter.Assign()) == S_OK)
        {
            DXGI_ADAPTER_DESC adapterDesc;
            if (!SUCCEEDED(adapter->GetDesc(&adapterDesc)))
                continue;

            if (adapterDesc.VendorId == 0x8086)
                return true;
        }

        return false;
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nShowCmd)
{
    using namespace std;

#if defined _M_X64 && _MSC_VER == 1800
    //workaround AVX2 bug in VS2013, http://connect.microsoft.com/VisualStudio/feedback/details/811093
    _set_FMA3_enable(0);
#endif

    wstringstream cmdss(GetCommandLineW());
    wstring wstr;
    cmdss >> wstr;
    wstringstream wss;
    wss << wstr << GetCurrentProcessId();
    wstring event_prefix;
    wss >> event_prefix;


    std::string log_path = GetCommandLineA();
    log_path.erase(log_path.begin(), find(log_path.begin(), log_path.end(), ' ')+1);
    log_path += "/pluginData/QSVHelper.log";

    log_file.open(log_path, ios::out | ios::trunc);
    if(!log_file.is_open())
        return EXIT_LOG_FILE_OPEN_FAILED;

    ipc_init_request init_req(event_prefix + INIT_REQUEST);

    if(!init_req.is_signalled(INFINITE))
        return EXIT_INIT_IPC_FAILED;

    if (!HasIntelGraphics())
        return EXIT_NO_INTEL_GRAPHICS;

    if(init_req->mode == init_req->MODE_QUERY)
    {
        MFXVideoSession session;
        int platform = qsv_get_cpu_platform();
        for(auto impl = begin(valid_impl); impl != std::end(valid_impl); impl++)
        {
            if (platform != QSV_CPU_PLATFORM_UNKNOWN && !(impl->platforms & platform))
                continue;

            auto ver = impl->version;
            auto result = session.Init(impl->intf | impl->type, &ver);
            if(result == MFX_ERR_NONE)
                return 0;
        }
        return EXIT_INIT_QUERY_FAILED;
    }

    safe_handle obs_handle(OpenProcess(SYNCHRONIZE, false, init_req->obs_process_id));
    if(!obs_handle)
        return EXIT_IPC_OBS_HANDLE_FAILED;

    ipc_init_response init_res(event_prefix + INIT_RESPONSE);
    zero(*&init_res);

    Encoder encoder(init_req, event_prefix, log_file);

    init_res->using_custom_impl = false;
    if(init_req->use_custom_impl)
    {
        impl_parameters p;
        p.intf = init_req->custom_intf;
        p.type = init_req->custom_impl;
        p.version = init_req->custom_version;
        auto result = encoder.InitializeMFX(p, true);
        if(result >= MFX_ERR_NONE)
        {
            init_res->using_custom_impl = true;
            init_res->actual_impl = p.intf | p.type;
            init_res->version = p.version;
        }
    }
    
    if(!init_res->using_custom_impl || !encoder)
    {
        init_res->using_custom_impl = false;
        decltype(&valid_impl[0]) best = nullptr;
        int platform = qsv_get_cpu_platform();
        for(auto &impl : valid_impl)
        {
            if (platform != QSV_CPU_PLATFORM_UNKNOWN  && !(impl.platforms & platform))
                continue;

            auto result = encoder.InitializeMFX(impl);
            if(result == MFX_WRN_PARTIAL_ACCELERATION && !best)
                best = &impl;
            if(result == MFX_ERR_NONE)
                break;
        }

        if(!encoder)
        {
            if(!best)
                return EXIT_NO_VALID_CONFIGURATION;
            auto ver = best->version;
            encoder.InitializeMFX(*best);
            log_file << "No valid implementation detected, using best implementation instead\n";
        }
    }

    if(!encoder)
        return EXIT_ENCODER_INIT_FAILED;

    init_res->version = encoder.version;
    init_res->requested_impl = encoder.requested;
    init_res->actual_impl = encoder.actual;

    if (encoder.InitializeEncoder() < MFX_ERR_NONE)
        return EXIT_ENCODER_INIT_FAILED;

    encoder.InitializeBuffers(init_res);

    init_res.signal();

    log_file << "Using " << encoder.encode_tasks.size() << " encode tasks and " << encoder.surfaces.size() << " internal frame buffers\n";

    encoder.RequestSPSPPS();

    ipc_stop stop(event_prefix + STOP_REQUEST);

    return encoder.EncodeLoop(stop, obs_handle);
}
