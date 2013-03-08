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



inline DXGI_FORMAT FixCopyTextureFormat(DXGI_FORMAT format)
{
    switch(format)
    {
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    return format;
}

inline GSColorFormat ConvertGIBackBufferFormat(DXGI_FORMAT format)
{
    switch(format)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        return GS_RGBA;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        return GS_BGRA;
    case DXGI_FORMAT_R10G10B10A2_UNORM: return GS_R10G10B10A2;
    case DXGI_FORMAT_B8G8R8X8_UNORM:    return GS_BGR;
    case DXGI_FORMAT_B5G5R5A1_UNORM:    return GS_B5G5R5A1;
    case DXGI_FORMAT_B5G6R5_UNORM:      return GS_B5G6R5;
    }

    return GS_UNKNOWNFORMAT;
}
