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

#include <Windows.h>

#include <cassert>
#include <cstdint>

#include <fstream>
#include <memory>
#include <string>
#include <vector>


struct safe_handle
{
    HANDLE h;
    operator bool() const { return h != nullptr; }
    operator HANDLE() { return h; }

    void reset(HANDLE h_=nullptr) { if(h) CloseHandle(h); h = h_; }

    bool operator!() const { return !h; }

    safe_handle &operator=(safe_handle &&other) { reset(other.h); other.h = nullptr; return *this; }

    safe_handle(HANDLE h=nullptr) : h(h) {}
    ~safe_handle() { if(h) CloseHandle(h); }
    safe_handle(safe_handle &&other) : h(other.h) { other.h = nullptr; };
private:
    safe_handle(const safe_handle&);
};

struct NamedSharedMemory
{
    std::wstring name;
    uint64_t size;
    void *memory;
    safe_handle file;

    bool operator!() const { return !memory || !file; }
    void *operator&() { return memory; }

    template <class T>
    T &as() { return *reinterpret_cast<T*>(memory); }

    NamedSharedMemory &operator=(NamedSharedMemory &&other)
    { FreeMemory(); name = other.name; size = other.size; memory = other.memory; other.memory = nullptr; file = std::move(other.file); return *this; }

    //NamedSharedMemory(const NamedSharedMemory& other) : name(other.name), size(other.size), memory(nullptr) { InitMemory(); }
    NamedSharedMemory(NamedSharedMemory&& other)
        : name(std::move(other.name)), size(std::move(other.size)), memory(std::move(other.memory)), file(std::move(other.file)) { other.memory = nullptr; }
    ~NamedSharedMemory() { FreeMemory(); }
    NamedSharedMemory(std::wstring name, uint64_t size=1) : name(name), size(size), memory(nullptr) { InitMemory(); }
    NamedSharedMemory() : memory(nullptr) {}

private:
    void FreeMemory() { if(memory) UnmapViewOfFile(memory); memory = nullptr; }
    void InitMemory()
    {
        if(!size)
            size = 1;
        file.reset(CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, size>>32, size & 0xffffffff, name.c_str()));
        if(file)
            memory = MapViewOfFile(file, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    }
};

template <bool manual>
struct IPCSignal
{
    safe_handle signal_;

    bool is_signalled(DWORD timeout=0) { return WaitForSingleObject(signal_, timeout) == WAIT_OBJECT_0; }
    void signal() { SetEvent(signal_); }
    void reset() { ResetEvent(signal_); }

    bool operator!() const { return !signal_; }
    
    IPCSignal &operator=(IPCSignal &&other) { signal_ = std::move(other.signal_); return *this; }

    IPCSignal(std::wstring name) { signal_.reset(CreateEvent(nullptr, manual, false, name.c_str())); }
    IPCSignal() {}
};

struct IPCWaiter
{
    std::vector<HANDLE> list;
    void push_back(const HANDLE &h) { list.push_back(h); }
    bool wait(DWORD timeout=0) { if(!list.size()) return false; auto res = wait_for_multiple_objects(timeout); return WAIT_OBJECT_0 <= res && res < (WAIT_OBJECT_0+list.size()); }
    bool wait_for(DWORD object, DWORD timeout=0) { if(!list.size()) return false; return wait_for_multiple_objects(timeout) == (WAIT_OBJECT_0 + object); }
    bool wait_for_two(DWORD first, DWORD second, DWORD timeout=0) { if(!list.size()) return false; auto res = wait_for_multiple_objects(timeout); return res == (WAIT_OBJECT_0 + first) || res == (WAIT_OBJECT_0 + second); }
    bool wait_timeout(DWORD timeout=0) { if(!list.size()) return false; return wait_for_multiple_objects(timeout) == WAIT_TIMEOUT; }

private:
    DWORD wait_for_multiple_objects(DWORD timeout) { return WaitForMultipleObjects(static_cast<DWORD>(list.size()), &list.front(), false, timeout); }
};

struct IPCMutex
{
    safe_handle mutex_;

    void lock() { if(mutex_) WaitForSingleObject(mutex_, INFINITE); }
    void unlock() { if(mutex_) ReleaseMutex(mutex_); }

    bool operator!() { return !mutex_; }

    IPCMutex &operator=(IPCMutex &&other) { mutex_ = std::move(other.mutex_); return *this; }

    IPCMutex(std::wstring name) { mutex_.reset(CreateMutex(nullptr, false, name.c_str())); }
    IPCMutex() {}
};

template <class T>
struct IPCMutexLock
{
    T& t;
    bool enabled;

    IPCMutexLock(T &t) : t(t), enabled(true) { t.lock(); }
    ~IPCMutexLock() { if(enabled) t.unlock(); }
    IPCMutexLock(IPCMutexLock &&other) : t(other.t), enabled(other.enabled) { other.enabled = false; }
};

template <class T>
IPCMutexLock<T> lock_mutex(T &t) { return IPCMutexLock<T>(t); }

template <class T, class F>
void with_locked(T &t, F &f) { IPCMutexLock<T> lock(t); f(); }

template <class T>
struct IPCType
{
    NamedSharedMemory memory;

    bool operator!() const { return !memory; }
    operator T() { return memory.as<T>(); }
    T *operator&() { return &memory.as<T>(); }
    T *operator->() { return &memory.as<T>(); }
    T &operator*() { return memory.as<T>(); }

    IPCType &operator=(IPCType &&other) { memory = std::move(other.memory); return *this; }

    IPCType(std::wstring name) : memory(name, sizeof(T)) {}
    IPCType() {}
};

template <class T>
struct IPCSignalledType : IPCType<T>, IPCSignal<false>
{
    bool operator!() const { return !memory || !signal_; }

    IPCSignalledType &operator=(IPCSignalledType &&other) { memory = std::move(other.memory); signal_ = std::move(other.signal_); return *this; }

    IPCSignalledType(std::wstring name) : IPCType(name), IPCSignal(name+L"Signal") {}
    IPCSignalledType() {}
};

template <class T>
struct IPCLockedSignalledType : IPCSignalledType<T>, IPCMutex
{
    bool operator!() const { return !memory || !signal_ || !mutex_; }

    IPCLockedSignalledType &operator=(IPCLockedSignalledType &&other) { memory = std::move(other.memory); signal_ = std::move(other.signal_); mutex_ = std::move(other.mutex_); return *this; }

    IPCLockedSignalledType(std::wstring name) : IPCSignalledType(name), IPCMutex(name+L"Lock") {}
    IPCLockedSignalledType() {}
};

template <class T>
struct IPCArray
{
    NamedSharedMemory memory;
    size_t size;
    
    bool operator!() const { return !memory; }
    operator T*() { return static_cast<T*>(&memory); }

    IPCArray &operator=(IPCArray &&other) { memory = std::move(other.memory); size = other.size; return *this; }

    IPCArray(std::wstring name, size_t size) : memory(name, sizeof(T)*size), size(size) {}
    IPCArray() {}
};

template <class T>
struct IPCSignalledArray : IPCArray<T>, IPCSignal<false>
{
    bool operator!() const { return !memory || !signal_; }

    IPCSignalledArray &operator=(IPCSignalledArray &&other) { memory = std::move(other.memory); signal_ = std::move(other.signal_); size = other.size; return *this; }

    IPCSignalledArray(std::wstring name, size_t size) : IPCArray(name, size), IPCSignal(name+L"Signal") {}
    IPCSignalledArray() {}
};

template <class T>
struct IPCLockedSignalledArray : IPCSignalledArray<T>, IPCMutex
{
    bool operator!() { return !memory || !signal_ || !mutex_; }

    IPCLockedSignalledArray &operator=(IPCLockedSignalledArray &&other)
    { memory = std::move(other.memory); signal_ = std::move(other.signal_); mutex_ = std::move(other.mutex_); size = other.size; return *this; }

    IPCLockedSignalledArray(std::wstring name, size_t size) : IPCSignalledArray(name, size), IPCMutex(name+L"Lock") {}
    IPCLockedSignalledArray() {}
};