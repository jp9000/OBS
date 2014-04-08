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


//wow, this thing is horrible.  this thing is so horrible.  I seriously need to fix it.





//-----------------------------------------
//Allocation tracking struct
struct Allocation
{
    DWORD allocationID;
    LPVOID Address;
    TCHAR lpFile[1024];
    DWORD dwLine;
};

Allocation *AllocationList = NULL;
DWORD numAllocations = 0;
DWORD totalAllocations = 0;

DWORD allocationCounter = 0;

unsigned int dwAllocCurLine;
TCHAR *       lpAllocCurFile;

BOOL bEnableTracking = FALSE;

int memoryBreakID;


void EnableMemoryTracking(BOOL bEnable, int id)
{
    bEnableTracking = bEnable;
    memoryBreakID = id;
}

void SetMemoryBreakID(int id)
{
    memoryBreakID = id;
}


DebugAlloc::DebugAlloc()
{
    hDebugMutex = OSCreateMutex();
}

DebugAlloc::~DebugAlloc()
{
    OSCloseMutex(hDebugMutex);

    if(numAllocations)
    {
        Log(TEXT("%d Memory leaks detected on exit!\r\n"), numAllocations);

        Log(TEXT("Allocation Tracking Results: Memory Leaks:\r\n=========================================================\r\n"));
        for(DWORD i=0;i<numAllocations;i++)
        {
            if(AllocationList[i].allocationID != INVALID)
                Log(TEXT("\tID: %d\r\n\tAddress: 0x%lX\r\n\tDeclared in file %s on line %d\r\n"), AllocationList[i].allocationID, AllocationList[i].Address, AllocationList[i].lpFile, AllocationList[i].dwLine);
            else
                Log(TEXT("\tID: Track point was not enabled when allocation was made\r\n\tAddress: 0x%lX\r\n\tDeclared in file %s on line %d\r\n"), AllocationList[i].Address, AllocationList[i].lpFile, AllocationList[i].dwLine);
        }
        Log(TEXT("=========================================================\r\n"));

        /*tsprintf_s(temp, 4095, TEXT("%d Memory leaks detected on exit!\r\n"), numAllocations);
        LogFile.WriteStr(temp);

        LogFile.WriteStr(TEXT("Allocation Tracking Results: Memory Leaks:\r\n=========================================================\r\n"));
        for(DWORD i=0;i<numAllocations;i++)
        {
            if(AllocationList[i].allocationID != INVALID)
                tsprintf_s(temp, 4095, TEXT("\tID: %d\r\n\tAddress: 0x%lX\r\n\tDeclared in file %s on line %d\r\n"), AllocationList[i].allocationID, AllocationList[i].Address, AllocationList[i].lpFile, AllocationList[i].dwLine);
            else
                tsprintf_s(temp, 4095, TEXT("\tID: Track point was not enabled when allocation was made\r\n\tAddress: 0x%lX\r\n\tDeclared in file %s on line %d\r\n"), AllocationList[i].Address, AllocationList[i].lpFile, AllocationList[i].dwLine);
            LogFile.WriteStr(temp);
        }
        LogFile.WriteStr(TEXT("=========================================================\r\n"));*/
    }
}

void * __restrict DebugAlloc::_Allocate(size_t dwSize)
{
    if(!dwSize) return NULL;

    OSEnterMutex(hDebugMutex);

    LPVOID lpRet;

    if(bEnableTracking)
    {
        ++allocationCounter;
        ++totalAllocations;
    }

    if(bEnableTracking && allocationCounter == memoryBreakID)
        ProgramBreak();

    if((lpRet=FastAlloc::_Allocate(dwSize)) && bEnableTracking)
    {
        Allocation allocTemp;

        Allocation *new_array = (Allocation*)FastAlloc::_Allocate(sizeof(Allocation)*++numAllocations);
        zero(new_array, sizeof(Allocation)*numAllocations);


        allocTemp.Address = lpRet;
        if(lpAllocCurFile)
            scpy(allocTemp.lpFile, lpAllocCurFile);
        allocTemp.dwLine = dwAllocCurLine;

        if(bEnableTracking)
            allocTemp.allocationID = allocationCounter;
        else
            allocTemp.allocationID = INVALID;

        if(AllocationList)
            mcpy(new_array, AllocationList, sizeof(Allocation)*(numAllocations-1));

        FastAlloc::_Free(AllocationList);

        AllocationList = new_array;

        mcpy(&AllocationList[numAllocations-1], &allocTemp, sizeof(Allocation));
    }

    OSLeaveMutex(hDebugMutex);

    return lpRet;
}

void * DebugAlloc::_ReAllocate(LPVOID lpData, size_t dwSize)
{
    LPVOID lpRet;

    if(!lpData)
    {
        lpRet = _Allocate(dwSize);
        return lpRet;
    }

    OSEnterMutex(hDebugMutex);

    if(bEnableTracking)
    {
        ++allocationCounter;
        ++totalAllocations;
    }

    if(bEnableTracking && allocationCounter == memoryBreakID)
        ProgramBreak();

    lpRet = FastAlloc::_ReAllocate(lpData, dwSize);

    /*if(bEnableTracking)
    {*/
        for(DWORD i=0;i<numAllocations;i++)
        {
            if(AllocationList[i].Address == lpData)
            {
                if(bEnableTracking)
                    AllocationList[i].allocationID = allocationCounter;
                else
                    AllocationList[i].allocationID = INVALID;

                AllocationList[i].Address = lpRet;
                break;
            }
        }
    //}

    OSLeaveMutex(hDebugMutex);

    return lpRet;
}

DWORD freeCount = 0;

void DebugAlloc::_Free(LPVOID lpData)
{
    //assert(lpData);

    if(lpData)
    {
        OSEnterMutex(hDebugMutex);

        FastAlloc::_Free(lpData);

        /*if(!bEnableTracking)
            return;*/

        for(DWORD i=0;i<numAllocations;i++)
        {
            if(AllocationList[i].Address == lpData)
            {
                if (!--numAllocations) { FastAlloc::_Free(AllocationList); AllocationList = NULL; OSLeaveMutex(hDebugMutex); return; }

                if(freeCount++ == 40)
                {
                    Allocation *new_array = (Allocation*)FastAlloc::_Allocate(sizeof(Allocation)*numAllocations);
                    zero(new_array, sizeof(Allocation)*numAllocations);
                    mcpy(new_array, AllocationList, sizeof(Allocation)*i);
                    mcpy(new_array+i, AllocationList+i+1, sizeof(Allocation)*(numAllocations-i));

                    FastAlloc::_Free(AllocationList);
                    AllocationList = new_array;

                    freeCount = 0;
                }
                else
                    mcpy(AllocationList+i, AllocationList+i+1, sizeof(Allocation)*(numAllocations-i));
                break;
            }
        }

        OSLeaveMutex(hDebugMutex);
    }
}


void* __restrict DefaultAlloc::_Allocate(size_t dwSize)
{
    return _aligned_malloc(dwSize, 16);
}

void* DefaultAlloc::_ReAllocate(LPVOID lpData, size_t dwSize)
{
    return (!lpData) ? _aligned_malloc(dwSize, 16) : _aligned_realloc(lpData, dwSize, 16);
}

void DefaultAlloc::_Free(LPVOID lpData)
{
    _aligned_free(lpData);
}

void DefaultAlloc::ErrorTermination()
{
}


SeriousMemoryDebuggingAlloc::SeriousMemoryDebuggingAlloc()
{
    _CrtSetDbgFlag(_CRTDBG_CHECK_ALWAYS_DF|_CRTDBG_ALLOC_MEM_DF);
}
