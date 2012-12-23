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

#include <intrin.h>

//-----------------------------------------
//warnings we don't want to hear over and over that have no relevance
//-----------------------------------------
#pragma warning(disable : 4251)  //class '' needs to have dll-interface to be used by clients of class ''


//-----------------------------------------
//defines
//-----------------------------------------
#if defined(NO_IMPORT)
    #define BASE_EXPORT
#elif !defined(BASE_EXPORTING)
    #define BASE_EXPORT     __declspec(dllimport)
#else
    #define BASE_EXPORT     __declspec(dllexport)
#endif

#define STDCALL                 __stdcall
#define strcmpi                 lstrcmpi

#define ProgramBreak() __debugbreak()
#define ReturnAddress() _ReturnAddress()

#ifndef FORCE_TRACE
    #ifdef _DEBUG
        #undef USE_TRACE
    #endif
#else
    #define USE_TRACE 1
#endif


#ifndef USE_TRACE
    #define traceIn(name)
    #define traceOut
    #define traceInFast(name)
    #define traceOutFast
    #define traceOutStop
#else
    #define traceIn(name)       {static const TCHAR *__FUNC_NAME__ = TEXT(#name); try{
    #define traceOut            }catch(...){TraceCrash(__FUNC_NAME__); throw;}}
    #define traceOutStop        }catch(...){TraceCrash(__FUNC_NAME__); TraceCrashEnd();}}
#endif

#ifdef USE_TRACE
    #ifdef FULL_TRACE
        #define traceInFast(name)   traceIn(name)
        #define traceOutFast        traceOut
    #else
        #define traceInFast(name)
        #define traceOutFast
    #endif
#endif


//-----------------------------------------
//typedefs
//-----------------------------------------
typedef void (STDCALL* DEFPROC)();
typedef DWORD (STDCALL* XTHREAD)(LPVOID);


//-----------------------------------------
//forwards
//-----------------------------------------
struct DISPLAYMODE;
template<typename T> class List;
