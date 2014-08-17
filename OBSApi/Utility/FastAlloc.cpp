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





struct MemInfo;
struct Pool;

struct FreeMemInfo
{
    FreeMemInfo *lpPrev;
    FreeMemInfo *lpNext;    //pointer to next free data
    Pool        *lpPool;
    int         num;        //number of free blocks
};

struct Pool
{
    DWORD       blocksUsed;
    size_t      bytesTotal;
    MemInfo     *meminfo;
    FreeMemInfo *firstFreeMem; //pointer to first free memory block
    FreeMemInfo *lastFreeMem; //pointer to last free memory block
    LPVOID      lpMem;
};

struct MemInfo
{
    FreeMemInfo *nextFree;
    size_t minBlockSize;
    size_t maxBlockSize;
    DWORD maxBlocks;
};

MemInfo MemInfoList[12],*SizeToMemInfo[0x8001];

size_t stPageSize=0;

Pool *PoolList[256];
#define GetMemInfo(size)    SizeToMemInfo[size]
#define align(address)      ((address+stPageSize-1)&(~(stPageSize-1)))

void STDCALL OpenLogFile();


FastAlloc::FastAlloc()
{
    stPageSize = (size_t)OSGetSysPageSize();

    DWORD from=1,to;

    for(int i=1; i<12; i++)
    {
        to   = (8<<(i+1))+1;

        MemInfoList[i].maxBlockSize = to-1;
        MemInfoList[i].minBlockSize = from;
        MemInfoList[i].maxBlocks = 0x10000/(DWORD)MemInfoList[i].maxBlockSize;
        MemInfoList[i].nextFree = NULL;

        for(DWORD j=from; j<to; j++)
            SizeToMemInfo[j] = &MemInfoList[i];

        from = to;
    }

    hAllocationMutex = OSCreateMutex();
}

FastAlloc::~FastAlloc()
{
    OSCloseMutex(hAllocationMutex);

    Pool *pool;
    BOOL bHasLeaks = 0;

    for(int i=0; i<256; i++)
    {
        if(PoolList[i])
        {
            for(int j=0; j<256; j++)
            {
                pool = &PoolList[i][j];
                if(pool->lpMem)
                {
                    if(!bHasLeaks)
                    {
                        Log(TEXT("Memory Leaks Were Detected.\r\n"));
                        bHasLeaks = 1;
                    }

                    OSVirtualFree(pool->lpMem);
                }
            }
            OSVirtualFree(PoolList[i]);
            PoolList[i] = NULL;
        }
    }
}

void   FastAlloc::ErrorTermination()
{
    Pool *pool;

    for(int i=0; i<256; i++)
    {
        if(PoolList[i])
        {
            for(int j=0; j<256; j++)
            {
                pool = &PoolList[i][j];
                if(pool->lpMem)
                    OSVirtualFree(pool->lpMem);
            }
            OSVirtualFree(PoolList[i]);
        }
    }
}

void * __restrict FastAlloc::_Allocate(size_t dwSize)
{
    OSEnterMutex(hAllocationMutex);

    //assert(dwSize);
    if(!dwSize) dwSize = 1;

    LPVOID lpMemory;
    Pool *pool;

    if(dwSize < 0x8001)
    {
        MemInfo *meminfo = GetMemInfo(dwSize);

        if(!meminfo->nextFree) //no pools have been created for this section
        {
            lpMemory = OSVirtualAlloc(0x10000);
            if (!lpMemory) DumpError(TEXT("Out of memory while trying to allocate %d bytes at %p"), dwSize, ReturnAddress());

            Pool *&poollist = PoolList[PtrTo32(lpMemory)>>24];
            if(!poollist)
            {
                poollist = (Pool*)OSVirtualAlloc(sizeof(Pool)*256);
                if (!poollist) DumpError(TEXT("Out of memory while trying to allocate %d bytes at %p"), dwSize, ReturnAddress());
                zero(poollist, sizeof(Pool)*256);
            }
            pool = &poollist[(PtrTo32(lpMemory)>>16)&0xFF];

            pool->lpMem = lpMemory;
            pool->bytesTotal = 0x10000;
            pool->meminfo = meminfo;
            pool->firstFreeMem = (FreeMemInfo*)lpMemory;
            pool->lastFreeMem = (FreeMemInfo*)lpMemory;

            meminfo->nextFree = (FreeMemInfo*)lpMemory;
            meminfo->nextFree->num = meminfo->maxBlocks;
            meminfo->nextFree->lpPool = pool;
            meminfo->nextFree->lpPrev = meminfo->nextFree->lpNext = NULL;
        }
        else
            pool = meminfo->nextFree->lpPool;

        assert(pool);

        assert(pool->bytesTotal);
    
        lpMemory = meminfo->nextFree;

        assert(meminfo->nextFree->num);

        ++pool->blocksUsed;

        if(pool->blocksUsed == meminfo->maxBlocks)
        {
            pool->firstFreeMem = NULL;
            pool->lastFreeMem = NULL;
        }
        else if(meminfo->nextFree->num == 1)
            pool->firstFreeMem = meminfo->nextFree->lpNext;


        if(meminfo->nextFree->num > 1)
        {
            FreeMemInfo *next = (FreeMemInfo*)(((LPBYTE)meminfo->nextFree)+meminfo->maxBlockSize);
            if(pool->firstFreeMem == meminfo->nextFree)
                pool->firstFreeMem = next;
            if(pool->lastFreeMem == meminfo->nextFree)
                pool->lastFreeMem = next;

            mcpy(next, meminfo->nextFree, sizeof(FreeMemInfo));

            if(next->lpPrev)
                next->lpPrev->lpNext = next;
            if(next->lpNext)
                next->lpNext->lpPrev = next;

            --next->num;
            meminfo->nextFree = next;
        }
        else
        {
            FreeMemInfo *freemem = meminfo->nextFree;
            if(freemem->lpNext)
                freemem->lpNext->lpPrev = freemem->lpPrev;
            if(freemem->lpPrev)
                freemem->lpPrev->lpNext = freemem->lpNext;
            meminfo->nextFree = freemem->lpNext;
        }

        //zero(lpMemory, dwSize);
    }
    else
    {
        dwSize = align(dwSize);
        lpMemory = OSVirtualAlloc(dwSize);
        if (!lpMemory) DumpError(TEXT("Out of memory while trying to allocate %d bytes at %p"), dwSize, ReturnAddress());

        //zero(lpMemory, dwSize);

        Pool *&poollist = PoolList[PtrTo32(lpMemory)>>24];
        if(!poollist)
        {
            poollist = (Pool*)OSVirtualAlloc(sizeof(Pool)*256);
            if (!poollist) DumpError(TEXT("Out of memory while trying to allocate %d bytes at %p"), dwSize, ReturnAddress());
            zero(poollist, sizeof(Pool)*256);
        }
        pool = &poollist[(PtrTo32(lpMemory)>>16)&0xFF];

        assert(pool->blocksUsed == 0)
        pool->blocksUsed = 1;
        pool->bytesTotal = dwSize;
        pool->lpMem = lpMemory;
        pool->meminfo = NULL;
        pool->firstFreeMem = pool->lastFreeMem = NULL;
    }

    OSLeaveMutex(hAllocationMutex);

    return lpMemory;
}

void * FastAlloc::_ReAllocate(LPVOID lpMemory, size_t dwSize)
{
    assert(dwSize);
    //assert(lpMemory);

    if(!lpMemory)
        return _Allocate(dwSize);

    if(!dwSize)
    {
        _Free(lpMemory);
        return NULL;
    }

    Pool *pool = &PoolList[PtrTo32(lpMemory)>>24][(PtrTo32(lpMemory)>>16)&0xFF];

    if(pool->meminfo)
    {
        if((dwSize >= pool->meminfo->minBlockSize) && (dwSize <= pool->meminfo->maxBlockSize))
            return lpMemory;
    }
    else if(align(dwSize) == pool->bytesTotal)
        return lpMemory;

    LPVOID lpNew = _Allocate(dwSize);
    if (!lpNew) DumpError(TEXT("Out of memory while trying to reallocate %d bytes at %p"), dwSize, ReturnAddress());

    if(pool->meminfo)
        mcpy(lpNew, lpMemory, MIN(dwSize, pool->meminfo->maxBlockSize));
    else
        mcpy(lpNew, lpMemory, MIN(dwSize, pool->bytesTotal));

    _Free(lpMemory);

    return lpNew;
}

void FastAlloc::_Free(LPVOID lpMemory)
{
    OSEnterMutex(hAllocationMutex);

    if(lpMemory)
    {
        Pool *pool = &PoolList[PtrTo32(lpMemory)>>24][(PtrTo32(lpMemory)>>16)&0xFF];
        MemInfo *meminfo = pool->meminfo;

        if(meminfo && pool->blocksUsed == 1)
        {
            FreeMemInfo *prevPoolFreeMem = pool->firstFreeMem->lpPrev;
            FreeMemInfo *nextPoolFreeMem = pool->lastFreeMem->lpNext;

            if(prevPoolFreeMem)
                prevPoolFreeMem->lpNext = nextPoolFreeMem;
            if(nextPoolFreeMem)
                nextPoolFreeMem->lpPrev = prevPoolFreeMem;

            if(meminfo->nextFree && (meminfo->nextFree->lpPool == pool))
                meminfo->nextFree = nextPoolFreeMem;
        }

        assert(pool->blocksUsed);

        FreeMemInfo *freemem = (FreeMemInfo*)lpMemory;

        if(--pool->blocksUsed)
        {
            freemem->lpPool = pool;
            freemem->num = 1;

            if(pool->blocksUsed == (meminfo->maxBlocks-1))
            {
                pool->firstFreeMem = pool->lastFreeMem = (FreeMemInfo*)lpMemory;
                if(meminfo->nextFree)
                {
                    freemem->lpNext = meminfo->nextFree;
                    freemem->lpPrev = meminfo->nextFree->lpPrev;

                    if(freemem->lpPrev)
                        freemem->lpPrev->lpNext = freemem;

                    meminfo->nextFree->lpPrev = freemem;
                    meminfo->nextFree = freemem;
                }
                else
                {
                    freemem->lpPrev = freemem->lpNext = NULL;
                    meminfo->nextFree = freemem;
                }
            }
            else
            {
                freemem->lpNext = pool->firstFreeMem;
                freemem->lpPrev = pool->firstFreeMem->lpPrev;

                pool->firstFreeMem->lpPrev = freemem;
                pool->firstFreeMem = freemem;

                if(freemem->lpPrev)
                    freemem->lpPrev->lpNext = freemem;

                if(!meminfo->nextFree || (pool <= meminfo->nextFree->lpPool))
                    meminfo->nextFree = freemem;
            }
        }
        else
        {
            assert(pool->bytesTotal);
            assert(pool->lpMem);
            OSVirtualFree(pool->lpMem);
            zero(pool, sizeof(Pool));
        }
    }

    OSLeaveMutex(hAllocationMutex);
}
