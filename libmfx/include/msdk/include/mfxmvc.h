/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010 - 2012 Intel Corporation. All Rights Reserved.

File Name: mfxmvc.h

*******************************************************************************/
#ifndef __MFXMVC_H__
#define __MFXMVC_H__

#include "mfxdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CodecProfile, CodecLevel */
enum {
    /* MVC profiles */
    MFX_PROFILE_AVC_MULTIVIEW_HIGH =118,
    MFX_PROFILE_AVC_STEREO_HIGH    =128
};

/* Extended Buffer Ids */
enum {
    MFX_EXTBUFF_MVC_SEQ_DESC =   MFX_MAKEFOURCC('M','V','C','D'),
    MFX_EXTBUFF_MVC_TARGET_VIEWS    =   MFX_MAKEFOURCC('M','V','C','T')
};

typedef struct  {
    mfxU16 ViewId;

    mfxU16 NumAnchorRefsL0;
    mfxU16 NumAnchorRefsL1;
    mfxU16 AnchorRefL0[16];
    mfxU16 AnchorRefL1[16];

    mfxU16 NumNonAnchorRefsL0;
    mfxU16 NumNonAnchorRefsL1;
    mfxU16 NonAnchorRefL0[16];
    mfxU16 NonAnchorRefL1[16];
} mfxMVCViewDependency;

typedef struct {
    mfxU16 TemporalId;
    mfxU16 LevelIdc;

    mfxU16 NumViews;
    mfxU16 NumTargetViews;
    mfxU16 *TargetViewId;
} mfxMVCOperationPoint;

typedef struct  {
    mfxExtBuffer Header;

    mfxU32 NumView;
    mfxU32 NumViewAlloc;
    mfxMVCViewDependency *View;

    mfxU32 NumViewId;
    mfxU32 NumViewIdAlloc;
    mfxU16 *ViewId;

    mfxU32 NumOP;
    mfxU32 NumOPAlloc;
    mfxMVCOperationPoint *OP;

    mfxU16 NumRefsTotal;
    mfxU32 Reserved[16];

} mfxExtMVCSeqDesc;

typedef struct {
    mfxExtBuffer    Header;

    mfxU16 TemporalId;
    mfxU32 NumView;
    mfxU16 ViewId[1024];
} mfxExtMVCTargetViews ;


#ifdef __cplusplus
} // extern "C"
#endif

#endif

