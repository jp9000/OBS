/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

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


#pragma once


void WINAPI FreeMediaType(AM_MEDIA_TYPE& mt);
HRESULT WINAPI CopyMediaType(AM_MEDIA_TYPE *pmtTarget, const AM_MEDIA_TYPE *pmtSource);

inline void DeleteMediaType(AM_MEDIA_TYPE *pmt)
{
    if(pmt != NULL)
    {
        FreeMediaType(*pmt);
        CoTaskMemFree(pmt);
    }
}


const GUID MEDIASUBTYPE_I420 = {0x30323449, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

enum VideoOutputType
{
    VideoOutputType_None,
    VideoOutputType_RGB24,
    VideoOutputType_RGB32,
    VideoOutputType_ARGB32,

    VideoOutputType_I420,
    VideoOutputType_YV12,

    VideoOutputType_Y41P,
    VideoOutputType_YVU9,

    VideoOutputType_YVYU,
    VideoOutputType_YUY2,
    VideoOutputType_UYVY,
    VideoOutputType_HDYC,

    VideoOutputType_MPEG2_VIDEO,
    VideoOutputType_H264,

    VideoOutputType_dvsl,
    VideoOutputType_dvsd,
    VideoOutputType_dvhd,

    VideoOutputType_MJPG
};

static const CTSTR EnumToName[] =
{
    TEXT("None"),
    TEXT("RGB24"),
    TEXT("RGB32"),
    TEXT("RGBA32"),

    TEXT("I420"),
    TEXT("YV12"),

    TEXT("Y41P"),
    TEXT("YVU9"),

    TEXT("YVYU"),
    TEXT("YUY2"),
    TEXT("UYVY"),
    TEXT("HDYC"),

    TEXT("MPEG2_VIDEO"),
    TEXT("H264"),

    TEXT("dvsl"),
    TEXT("dvsd"),
    TEXT("dvhd"),

    TEXT("MJPG"),
};

struct MediaOutputInfo
{
    VideoOutputType videoType;
    AM_MEDIA_TYPE *mediaType;
    UINT64 minFrameInterval, maxFrameInterval;
    UINT minCX, minCY;
    UINT maxCX, maxCY;
    UINT xGranularity, yGranularity;
    bool bUsingFourCC;

    inline void FreeData()
    {
        FreeMediaType(*mediaType);
        CoTaskMemFree(mediaType);
    }
};

inline BITMAPINFOHEADER* GetVideoBMIHeader(const AM_MEDIA_TYPE *pMT)
{
    return (pMT->formattype == FORMAT_VideoInfo) ?
        &reinterpret_cast<VIDEOINFOHEADER*>(pMT->pbFormat)->bmiHeader : 
        &reinterpret_cast<VIDEOINFOHEADER2*>(pMT->pbFormat)->bmiHeader;
}

VideoOutputType GetVideoOutputTypeFromFourCC(DWORD fourCC);
VideoOutputType GetVideoOutputType(const AM_MEDIA_TYPE &media_type);
bool GetVideoOutputTypes(const List<MediaOutputInfo> &outputList, UINT width, UINT height, UINT64 frameInterval, List<VideoOutputType> &types);
MediaOutputInfo* GetBestMediaOutput(const List<MediaOutputInfo> &outputList, UINT width, UINT height, UINT preferredType, UINT64 &frameInterval);
