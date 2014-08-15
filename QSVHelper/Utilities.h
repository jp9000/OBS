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

#pragma once

#include <limits>

template <class T>
static inline void zero(T& t, size_t size=sizeof(T))
{
    memset(&t, 0, size);
}

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

template <class T, class U>
static inline T saturate(U val)
{
    if (val > std::numeric_limits<T>::max())
        return std::numeric_limits<T>::max();
    if (val < std::numeric_limits<T>::min())
        return std::numeric_limits<T>::min();
    return val;
}

template <class T, class U>
static inline void saturate(T &t, U val)
{
    if (val > std::numeric_limits<T>::max())
        t = std::numeric_limits<T>::max();
    else if (val < std::numeric_limits<T>::min())
        t = std::numeric_limits<T>::min();
    else
        t = static_cast<T>(val);
}

template <typename T, typename U, typename V>
static inline T clamp(T t, U u, V v)
{
    if (t < u)
        return u;
    return t > v ? v : t;
}

static bool valid_method(decltype(mfxInfoMFX::RateControlMethod) method)
{
    switch (method)
    {
    case MFX_RATECONTROL_CBR:
    case MFX_RATECONTROL_VBR:
    case MFX_RATECONTROL_CQP:
    case MFX_RATECONTROL_AVBR:
    case MFX_RATECONTROL_LA:
    case MFX_RATECONTROL_ICQ:
    case MFX_RATECONTROL_VCM:
    case MFX_RATECONTROL_LA_ICQ:
        return true;
    }
    return false;
}
