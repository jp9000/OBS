/********************************************************************************
 Copyright (C) 2014 Ruwen Hahn <palana@stunned.de>
 
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

#include "Main.h"
#include <functional>
#include <vector>

struct BufferedDataPacket : DataPacket
{
    template <typename Func>
    BufferedDataPacket(Func &&func) : DataPacket(), func(std::forward<Func>(func)) {}
    std::function<void(DataPacket&)> func;

    void InitBuffer()
    {
        if (size || !func)
            return;

        func(*this);
        if (!size)
            return;

        buffer.assign(lpPacket, lpPacket + size);
        lpPacket = buffer.data();
    }

private:
    std::vector<BYTE> buffer;
};

inline BufferedDataPacket GetBufferedSEIPacket()
{
    return [](DataPacket &p)
    {
        VideoEncoder *encoder = App->GetVideoEncoder();
        assert(encoder);
        encoder->GetSEI(p);
    };
};

inline BufferedDataPacket GetBufferedAudioHeadersPacket()
{
    return [](DataPacket &p)
    {
        App->GetAudioHeaders(p);
    };
};

inline BufferedDataPacket GetBufferedVideoHeadersPacket()
{
    return [](DataPacket &p)
    {
        App->GetVideoHeaders(p);
    };
};
