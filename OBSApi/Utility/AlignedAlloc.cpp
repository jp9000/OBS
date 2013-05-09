/********************************************************************************
 Copyright (C) 2001-2012 Hugh Bailey <obs.jim@gmail.com>

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


#include "XT.h"
#include "malloc.h"

void* __restrict AlignedAlloc::_Allocate(size_t dwSize)
{
    return _aligned_malloc(dwSize, 16);
}

void* AlignedAlloc::_ReAllocate(LPVOID lpData, size_t dwSize)
{
    return (!lpData) ? _aligned_malloc(dwSize, 16) : _aligned_realloc(lpData, dwSize, 16);
}

void AlignedAlloc::_Free(LPVOID lpData)
{
    _aligned_free(lpData);
}

void AlignedAlloc::ErrorTermination()
{
}