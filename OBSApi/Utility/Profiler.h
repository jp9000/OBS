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


// This is loosely based on the hierarchical profiling method from Game Programming Gems 3 by Greg Hjelstrom & Byon Garrabrant.

#pragma once

struct ProfileNodeInfo;

class BASE_EXPORT ProfilerNode
{
    CTSTR lpName;
    QWORD startTime,
          cpuStartTime;
    DWORD parallelCalls;
    HANDLE thread;
    ProfilerNode *parent;
    bool bSingularNode;
    ProfileNodeInfo *info;

public:
    ProfilerNode(CTSTR name, bool bSingularize=false);
    ~ProfilerNode();
    void MonitorThread(HANDLE thread);
    void SetParallelCallCount(DWORD num);
};

//BASE_EXPORT extern ProfilerNode *__curProfilerNode;
BASE_EXPORT extern BOOL bProfilingEnabled;

#define ENABLE_PROFILING 1

#ifdef ENABLE_PROFILING
    #define profileSingularSegment(name)                ProfilerNode _curProfiler(TEXT(name), true);
    #define profileSingularIn(name)                     {ProfilerNode _curProfiler(TEXT(name), true);
    #define profileSegment(name)                        ProfilerNode _curProfiler(TEXT(name));
    #define profileParallelSegment(name, plural, num)   ProfilerNode _curProfiler(num == 1 ? TEXT(name) : TEXT(plural)); _curProfiler.SetParallelCallCount(num);
    #define profileIn(name)                             {ProfilerNode _curProfiler(TEXT(name));
    #define profileOut                                  }
#else
    #define profileSingularSegment(name)
    #define profileSingularIn(name)
    #define profileSegment(name)
    #define profileParallelSegment(name, plural, num)
    #define profileIn(name)
    #define profileOut
#endif

BASE_EXPORT void STDCALL EnableProfiling(BOOL bEnable, float minPercentage=0.0f, float minTime=0.0f);
BASE_EXPORT void STDCALL DumpProfileData();
BASE_EXPORT void STDCALL DumpLastProfileData();
BASE_EXPORT void STDCALL FreeProfileData();
