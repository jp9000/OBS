/* ****************************************************************************** *\

Copyright (C) 2007-2013 Intel Corporation.  All rights reserved.

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


File Name: mfxplugin++.h

\* ****************************************************************************** */

#ifndef __MFXPLUGINPLUSPLUS_H
#define __MFXPLUGINPLUSPLUS_H

#include "mfxvideo.h"
#include "mfxplugin.h"

class MFXPlugin
{
public:
    virtual mfxStatus mfxPluginInit(mfxCoreInterface *core) = 0;
    virtual mfxStatus mfxPluginClose() = 0;
    virtual mfxStatus mfxGetPluginParam(mfxPluginParam *par) = 0;
    virtual mfxStatus mfxSubmit(const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxThreadTask *task) = 0;
    virtual mfxStatus mfxExecute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a) = 0;
    virtual mfxStatus mfxFreeResources(mfxThreadTask task, mfxStatus sts) = 0;
};

/* Class adapter between "C" structure mfxPlugin and C++ interface MFXPlugin */
class MFXPluginAdapter : public mfxPlugin
{
public:
    MFXPluginAdapter(MFXPlugin *pPlugin)
    {
        pthis = pPlugin;
        PluginInit = MFXPluginAdapter::_PluginInit;
        PluginClose = MFXPluginAdapter::_PluginClose;
        GetPluginParam = MFXPluginAdapter::_GetPluginParam;
        Submit = MFXPluginAdapter::_Submit;
        Execute = MFXPluginAdapter::_Execute;
        FreeResources = MFXPluginAdapter::_FreeResources;
    }

private:
    static mfxStatus _PluginInit(mfxHDL pthis, mfxCoreInterface *core) { return ((MFXPlugin*)pthis)->mfxPluginInit(core); }
    static mfxStatus _PluginClose(mfxHDL pthis) { return ((MFXPlugin*)pthis)->mfxPluginClose(); }
    static mfxStatus _GetPluginParam(mfxHDL pthis, mfxPluginParam *par) { return ((MFXPlugin*)pthis)->mfxGetPluginParam(par); }
    static mfxStatus _Submit(mfxHDL pthis, const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxThreadTask *task) { return ((MFXPlugin*)pthis)->mfxSubmit(in, in_num, out, out_num, task); }
    static mfxStatus _Execute(mfxHDL pthis, mfxThreadTask task, mfxU32 thread_id, mfxU32 call_count) { return ((MFXPlugin*)pthis)->mfxExecute(task, thread_id, call_count); }
    static mfxStatus _FreeResources(mfxHDL pthis, mfxThreadTask task, mfxStatus sts) { return ((MFXPlugin*)pthis)->mfxFreeResources(task, sts); }
};

#endif // __MFXPLUGINPLUSPLUS_H
