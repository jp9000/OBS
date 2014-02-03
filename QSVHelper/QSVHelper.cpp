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
    const struct impl_parameters
    {
        mfxIMPL type,
                intf;
        mfxVersion version;
    } valid_impl[] = {
        { MFX_IMPL_HARDWARE_ANY,    MFX_IMPL_VIA_D3D11, {6, 1} },
        { MFX_IMPL_HARDWARE,        MFX_IMPL_VIA_D3D11, {6, 1} },
        { MFX_IMPL_HARDWARE_ANY,    MFX_IMPL_VIA_D3D9,  {6, 1} }, //Ivy Bridge+ with non-functional D3D11 support?
        { MFX_IMPL_HARDWARE,        MFX_IMPL_VIA_D3D9,  {6, 1} },
        { MFX_IMPL_HARDWARE_ANY,    MFX_IMPL_VIA_D3D9,  {4, 1} }, //Sandy Bridge
        { MFX_IMPL_HARDWARE,        MFX_IMPL_VIA_D3D9,  {4, 1} },
    };

    std::wofstream log_file;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nShowCmd)
{
    using namespace std;

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
        return 200;

    ipc_init_request init_req(event_prefix + INIT_REQUEST);

    if(!init_req.is_signalled(INFINITE))
        return 1;

    if(init_req->mode == init_req->MODE_QUERY)
    {
        MFXVideoSession session;
        for(auto impl = begin(valid_impl); impl != std::end(valid_impl); impl++)
        {
            auto ver = impl->version;
            auto result = session.Init(impl->intf | impl->type, &ver);
            if(result == MFX_ERR_NONE)
                return 0;
        }
        return 3;
    }

    safe_handle obs_handle(OpenProcess(SYNCHRONIZE, false, init_req->obs_process_id));
    if(!obs_handle)
        return 2;

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
        decltype(begin(valid_impl)) best = nullptr;
        for(auto impl = begin(valid_impl); impl != std::end(valid_impl); impl++)
        {
            auto result = encoder.InitializeMFX(*impl);
            if(result == MFX_WRN_PARTIAL_ACCELERATION && !best)
                best = impl;
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
        return 6;

    init_res->version = encoder.version;
    init_res->requested_impl = encoder.requested;
    init_res->actual_impl = encoder.actual;

    encoder.InitializeEncoder();

    encoder.InitializeBuffers(init_res);

    init_res.signal();

    log_file << "Using " << encoder.encode_tasks.size() << " encode tasks and " << encoder.surfaces.size() << " internal frame buffers\n";

    encoder.RequestSPSPPS();

    ipc_stop stop(event_prefix + STOP_REQUEST);

    return encoder.EncodeLoop(stop, obs_handle);
}
