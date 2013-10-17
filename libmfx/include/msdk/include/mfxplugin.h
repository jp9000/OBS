/******************************************************************************* *\

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

File Name: mfxplugin.h

*******************************************************************************/
#ifndef __MFXPLUGIN_H__
#define __MFXPLUGIN_H__
#include "mfxvideo.h"

#pragma warning(disable: 4201)

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

typedef enum {
    MFX_THREADPOLICY_SERIAL    = 0,
    MFX_THREADPOLICY_PARALLEL    = 1
} mfxThreadPolicy;

typedef struct mfxPluginParam {
    mfxU32  reserved[14];
    mfxThreadPolicy ThreadPolicy;
    mfxU32  MaxThreadNum;
} mfxPluginParam;

typedef struct mfxCoreParam{
    mfxU32  reserved[13];
    mfxIMPL Impl;
    mfxVersion Version;
    mfxU32  NumWorkingThread;
} mfxCoreParam;

typedef struct mfxCoreInterface {
    mfxHDL pthis;

    mfxHDL reserved1[2];
    mfxFrameAllocator FrameAllocator;
    mfxBufferAllocator reserved3;

    mfxStatus (MFX_CDECL *GetCoreParam)(mfxHDL pthis, mfxCoreParam *par);
    mfxStatus (MFX_CDECL *GetHandle) (mfxHDL pthis, mfxHandleType type, mfxHDL *handle);
    mfxStatus (MFX_CDECL *IncreaseReference) (mfxHDL pthis, mfxFrameData *fd);
    mfxStatus (MFX_CDECL *DecreaseReference) (mfxHDL pthis, mfxFrameData *fd);
    mfxStatus (MFX_CDECL *CopyFrame) (mfxHDL pthis, mfxFrameSurface1 *dst, mfxFrameSurface1 *src);
    mfxStatus (MFX_CDECL *CopyBuffer)(mfxHDL pthis, mfxU8 *dst, mfxU32 size, mfxFrameSurface1 *src);

    mfxStatus (MFX_CDECL *MapOpaqueSurface)(mfxHDL pthis, mfxU32  num, mfxU32  type, mfxFrameSurface1 **op_surf);
    mfxStatus (MFX_CDECL *UnmapOpaqueSurface)(mfxHDL pthis, mfxU32  num, mfxU32  type, mfxFrameSurface1 **op_surf);

    mfxStatus (MFX_CDECL *GetRealSurface)(mfxHDL pthis, mfxFrameSurface1 *op_surf, mfxFrameSurface1 **surf);
    mfxStatus (MFX_CDECL *GetOpaqueSurface)(mfxHDL pthis, mfxFrameSurface1 *surf, mfxFrameSurface1 **op_surf);

    mfxHDL reserved4[4];
} mfxCoreInterface;

typedef struct mfxPlugin{
    mfxHDL pthis;

    mfxStatus (MFX_CDECL *PluginInit) (mfxHDL pthis, mfxCoreInterface *core);
    mfxStatus (MFX_CDECL *PluginClose) (mfxHDL pthis);

    mfxStatus (MFX_CDECL *GetPluginParam)(mfxHDL pthis, mfxPluginParam *par);

    mfxStatus (MFX_CDECL *Submit)(mfxHDL pthis, const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxThreadTask *task);
    mfxStatus (MFX_CDECL *Execute)(mfxHDL pthis, mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a);
    mfxStatus (MFX_CDECL *FreeResources)(mfxHDL pthis, mfxThreadTask task, mfxStatus sts);

    mfxHDL reserved[9];
} mfxPlugin;



mfxStatus MFX_CDECL MFXVideoUSER_Register(mfxSession session, mfxU32 type, const mfxPlugin *par);
mfxStatus MFX_CDECL MFXVideoUSER_Unregister(mfxSession session, mfxU32 type);

mfxStatus MFX_CDECL MFXVideoUSER_ProcessFrameAsync(mfxSession session, const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxSyncPoint *syncp);

#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */

#endif /* __MFXPLUGIN_H__ */
