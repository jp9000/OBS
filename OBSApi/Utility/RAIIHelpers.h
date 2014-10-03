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

#include "XT.h"

#pragma warning(disable: 4530)
#include <memory>

struct EventDeleter
{
    void operator()(HANDLE h) const { if (!h) return; OSCloseEvent(h); }
};

struct MutexDeleter
{
    void operator()(HANDLE h) const { if (!h) return; OSCloseMutex(h); }
};

struct ThreadCloser
{
    void operator()(HANDLE h) const { if (!h) return; OSCloseThread(h); }
};

template <int ... args>
struct ThreadDeleter
{
    static_assert(sizeof...(args) == 0 || sizeof...(args) == 1, "ThreadDeleter takes 0 or 1 template parameter to specify the timeout to OSTerminateThread");
    void operator()(HANDLE h) const { if (!h) return; OSTerminateThread(h, args ...); }
};

template <DWORD exitCode=0>
struct ThreadTerminator
{
    void operator()(HANDLE h) const { if (!h) return; Log(L"Terminating 0x%x", GetThreadId(h)); TerminateThread(h, exitCode); }
};

struct ScopedLock
{
    bool locked, unlock;
    HANDLE h;
    ScopedLock(std::unique_ptr<void, MutexDeleter> const &mutex, bool tryLock = false, bool autounlock = true) : ScopedLock(mutex.get(), tryLock, autounlock)
    {}
    ScopedLock(HANDLE mutex, bool tryLock = false, bool autounlock = true) : locked(false), unlock(autounlock), h(mutex)
    {
        if (!h) return;
        if (tryLock && !OSTryEnterMutex(h)) return;
        if (!tryLock) OSEnterMutex(h);
        locked = true;
    }
    ~ScopedLock() { if (locked && unlock) OSLeaveMutex(h); }
};

template <class Fun>
struct ScopeGuard
{
    ScopeGuard(Fun fun) : fun(std::move(fun)), active(true) {}
    ~ScopeGuard() { if (active) fun(); }

    void dismiss() { active = false; }

    ScopeGuard(ScopeGuard &&o) : fun(std::move(o.fun)), active(o.active) { o.dismiss(); }

private:
    ScopeGuard() = delete;
    ScopeGuard(ScopeGuard const &) = delete;
    ScopeGuard &operator=(ScopeGuard const &) = delete;

    Fun fun;
    bool active;
};

template <class Fun>
ScopeGuard<Fun> GuardScope(Fun &&fun) { return ScopeGuard<Fun>(std::move(fun)); }
