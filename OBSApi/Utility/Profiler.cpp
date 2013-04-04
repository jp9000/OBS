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


// This is loosely based on the hierarchical profiling method from Game Programming Gems 3 by Greg Hjelstrom & Byon Garrabrant.




inline double MicroToMS(DWORD microseconds)
{
    return double(microseconds)*0.001;
}

float minPercentage, minTime;


struct BASE_EXPORT ProfileNodeInfo
{
    inline ~ProfileNodeInfo()
    {
        FreeData();
    }

    void FreeData()
    {
        for(unsigned int i=0; i<Children.Num(); i++)
            delete Children[i];
        Children.Clear();
    }

    CTSTR lpName;

    DWORD numCalls;
    DWORD avgTimeElapsed;
    double avgPercentage;
    double childPercentage;
    double unaccountedPercentage;

    bool bSingular;

    QWORD totalTimeElapsed;

    ProfileNodeInfo *parent;
    List<ProfileNodeInfo*> Children;

    void calculateProfileData(int rootCallCount)
    {
        avgTimeElapsed = (DWORD)(totalTimeElapsed/(QWORD)rootCallCount);
        
        if(parent)  avgPercentage = (double(avgTimeElapsed)/double(parent->avgTimeElapsed))*parent->avgPercentage;
        else        avgPercentage = 100.0f;

        childPercentage = 0.0;

        if(Children.Num())
        {
            for(unsigned int i=0; i<Children.Num(); i++)
            {
                Children[i]->parent = this;
                Children[i]->calculateProfileData(rootCallCount);

                if(!Children[i]->bSingular)
                    childPercentage += Children[i]->avgPercentage;
            }

            unaccountedPercentage = avgPercentage-childPercentage;
        }
    }

    void dumpData(int rootCallCount, int indent=0)
    {
        if(indent == 0)
            calculateProfileData(rootCallCount);

        String indentStr;
        for(int i=0; i<indent; i++)
            indentStr << TEXT("| ");

        CTSTR lpIndent = indent == 0 ? TEXT("") : indentStr.Array();

        int perFrameCalls = numCalls/rootCallCount;

        float fTimeTaken = (float)MicroToMS(avgTimeElapsed);

        if(avgPercentage >= minPercentage && fTimeTaken >= minTime)
        {
            if(Children.Num())
                Log(TEXT("%s%s - [%.3g%%] [avg time: %g ms] [avg calls per frame: %d] [children: %.3g%%] [unaccounted: %.3g%%]"), lpIndent, lpName, avgPercentage, fTimeTaken, perFrameCalls, childPercentage, unaccountedPercentage);
            else
                Log(TEXT("%s%s - [%.3g%%] [avg time: %g ms] [avg calls per frame: %d]"), lpIndent, lpName, avgPercentage, fTimeTaken, perFrameCalls);
        }

        for(unsigned int i=0; i<Children.Num(); i++)
            Children[i]->dumpData(rootCallCount, indent+1);
    }

    ProfileNodeInfo* FindSubProfile(CTSTR lpName)
    {
        for(unsigned int i=0; i<Children.Num(); i++)
        {
            if(Children[i]->lpName == lpName)
                return Children[i];
        }

        return NULL;
    }

    static ProfileNodeInfo* FindProfile(CTSTR lpName)
    {
        for(unsigned int i=0; i<profilerData.Num(); i++)
        {
            if(profilerData[i].lpName == lpName)
                return profilerData+i;
        }

        return NULL;
    }

    static List<ProfileNodeInfo> profilerData;
};


ProfilerNode *__curProfilerNode = NULL;
List<ProfileNodeInfo> ProfileNodeInfo::profilerData;
BOOL bProfilingEnabled = FALSE;
HANDLE hProfilerTimer = NULL;


void STDCALL EnableProfiling(BOOL bEnable, float pminPercentage, float pminTime)
{
    //if(engine && !engine->InEditor())
    bProfilingEnabled = bEnable;

    minPercentage = pminPercentage;
    minTime = pminTime;
}

void STDCALL DumpProfileData()
{
    if(ProfileNodeInfo::profilerData.Num())
    {
        Log(TEXT("\r\nProfiler results:\r\n"));
        Log(TEXT("=============================================================="));
        for(unsigned int i=0; i<ProfileNodeInfo::profilerData.Num(); i++)
            ProfileNodeInfo::profilerData[i].dumpData(ProfileNodeInfo::profilerData[i].numCalls);
        Log(TEXT("==============================================================\r\n"));
    }
}

void STDCALL FreeProfileData()
{
    for(unsigned int i=0; i<ProfileNodeInfo::profilerData.Num(); i++)
        ProfileNodeInfo::profilerData[i].FreeData();
    ProfileNodeInfo::profilerData.Clear();
}

ProfilerNode::ProfilerNode(CTSTR lpName, bool bSingularize)
{
    info = NULL;
    this->lpName = NULL;

    parent = __curProfilerNode;

    if(bSingularNode = bSingularize)
    {
        if(!parent)
            return;

        while(parent->parent != NULL)
            parent = parent->parent;
    }
    else
        __curProfilerNode = this;

    if(parent)
    {
        if(!parent->lpName) return; //profiling was disabled when parent was created, so exit to avoid inconsistent results

        ProfileNodeInfo *parentInfo = parent->info;
        if(parentInfo)
        {
            info = parentInfo->FindSubProfile(lpName);
            if(!info)
            {
                info = new ProfileNodeInfo;
                parentInfo->Children << info;
                info->lpName = lpName;
                info->bSingular = bSingularize;
            }
        }
    }
    else if(bProfilingEnabled)
    {
        info = ProfileNodeInfo::FindProfile(lpName);
        if(!info)
        {
            info = ProfileNodeInfo::profilerData.CreateNew();
            info->lpName = lpName;
        }
    }
    else
        return;

    if (info)
        ++info->numCalls;

    this->lpName = lpName;

    startTime = OSGetTimeMicroseconds();
}

ProfilerNode::~ProfilerNode()
{
    QWORD newTime = OSGetTimeMicroseconds();

    //profiling was diabled when created
    if(lpName)
    {
        DWORD curTime = (DWORD)(newTime-startTime);
        info->totalTimeElapsed += curTime;
    }

    if(!bSingularNode)
        __curProfilerNode = parent;
}
