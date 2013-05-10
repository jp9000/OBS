/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010 - 2012 Intel Corporation. All Rights Reserved.

File Name: mfxjpeg.h

*******************************************************************************/
#ifndef __MFX_JPEG_H__
#define __MFX_JPEG_H__

#include "mfxdefs.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* CodecId */
enum {
    MFX_CODEC_JPEG    = MFX_MAKEFOURCC('J','P','E','G')
};

/* CodecProfile, CodecLevel */
enum
{
    MFX_PROFILE_JPEG_BASELINE      = 1
};

enum
{
    MFX_ROTATION_0      = 0,
    MFX_ROTATION_90     = 1,
    MFX_ROTATION_180    = 2,
    MFX_ROTATION_270    = 3
};

enum {
    MFX_EXTBUFF_JPEG_QT     =   MFX_MAKEFOURCC('J','P','G','Q'),
    MFX_EXTBUFF_JPEG_HUFFMAN     =   MFX_MAKEFOURCC('J','P','G','H')
};

enum {
    MFX_JPEG_COLORFORMAT_UNKNOWN = 0,
    MFX_JPEG_COLORFORMAT_YCbCr = 1,
    MFX_JPEG_COLORFORMAT_RGB = 2
};

typedef struct {
    mfxExtBuffer    Header;

    mfxU16  reserved[7];
    mfxU16  NumTable;

    mfxU16    Qm[4][64];
} mfxExtJPEGQuantTables;

typedef struct {
    mfxExtBuffer    Header;

    mfxU16  reserved[2];
    mfxU16  NumDCTable;
    mfxU16  NumACTable;

    struct {
        mfxU8   Bits[16];
        mfxU8   Values[12];
    } DCTables[4];

    struct {
        mfxU8   Bits[16];
        mfxU8   Values[162];
    } ACTables[4];
} mfxExtJPEGHuffmanTables;

#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */

#endif // __MFX_JPEG_H__
