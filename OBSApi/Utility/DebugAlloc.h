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


#pragma once

//used for finding out precisely where memory leaks are occuring; don't use if you have serious memory issues.  it's crappy though.
class BASE_EXPORT DebugAlloc : public FastAlloc
{
    HANDLE hDebugMutex;

public:
    DebugAlloc();
    virtual ~DebugAlloc();

    virtual void * __restrict _Allocate(size_t dwSize);
    virtual void * _ReAllocate(LPVOID lpData, size_t dwSize);
    virtual void   _Free(LPVOID lpData);
};

//use this function to begin tracking of memory in some spot of the application.
//you -really- don't want to place this at the start of the app because your
//results will usually be random.  instead, place it somewhere right before where
//the memory leaks start occuring.
BASE_EXPORT void EnableMemoryTracking(BOOL bEnable, int id=0);
BASE_EXPORT void SetMemoryBreakID(int id);


//goes to malloc/realloc/free
class BASE_EXPORT DefaultAlloc : public Alloc
{
public:
    virtual void * __restrict _Allocate(size_t dwSize);

    virtual void * _ReAllocate(LPVOID lpData, size_t dwSize);

    virtual void   _Free(LPVOID lpData);

    virtual void   ErrorTermination();
};

class BASE_EXPORT SeriousMemoryDebuggingAlloc : public DefaultAlloc
{
public:
    SeriousMemoryDebuggingAlloc();
};
