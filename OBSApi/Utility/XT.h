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

#pragma warning(disable: 4996)

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <crtdbg.h>
#include <new>

#include <math.h>
#include <float.h>

#include <typeinfo.h>

#include <time.h>

#if defined(_WIN64) && !defined(C64)
    #define C64
#endif

/*#if defined(WIN32) && defined(C64)
    #define USE_SSE
#endif*/

#define USE_SSE 1

#ifdef USE_SSE
    #include <xmmintrin.h>
#endif


#include "Defs.h"


//-----------------------------------------
//base defines/functions/etc
//-----------------------------------------
#if defined WIN32
    #include "XT_Windows.h"
#elif defined __UNIX__
    #include "XT_Unix.h"
#else
    #error Unknown Operating System
#endif


//-----------------------------------------
//OS-independant functions
//-----------------------------------------
class StringList;
class String;

struct OSFindData
{
    TCHAR fileName[260];
    BOOL bDirectory;
    BOOL bHidden;
};

struct XRect
{
    int x;
    int y;
    int cx;
    int cy;
};

struct OSFileChangeData;

struct OSDirectoryMonitorData;
typedef void (*OSDirectoryMonitorCallback)();

#define WAIT_INFINITE 0xFFFFFFFF

BASE_EXPORT void   STDCALL OSLogSystemStats();
BASE_EXPORT DWORD  STDCALL OSGetSysPageSize();
BASE_EXPORT LPVOID STDCALL OSVirtualAlloc(size_t dwSize);
BASE_EXPORT void   STDCALL OSVirtualFree(LPVOID lpData);
BASE_EXPORT void   STDCALL OSExitProgram();
BASE_EXPORT void   STDCALL OSCriticalExit();
BASE_EXPORT int    STDCALL OSProcessEvent();

BASE_EXPORT BOOL   STDCALL OSDeleteFile(CTSTR lpFile);
BASE_EXPORT BOOL   STDCALL OSCopyFile(CTSTR lpFileDest, CTSTR lpFileSrc);
BASE_EXPORT BOOL   STDCALL OSRenameFile(CTSTR oldPath, CTSTR newPath);
BASE_EXPORT BOOL   STDCALL OSCreateDirectory(CTSTR lpDirectory);
BASE_EXPORT BOOL   STDCALL OSSetCurrentDirectory(CTSTR lpDirectory);
BASE_EXPORT BOOL   STDCALL OSFileExists(CTSTR lpFile);
BASE_EXPORT bool   STDCALL OSFileIsDirectory(CTSTR file);

BASE_EXPORT QWORD  STDCALL OSGetFileModificationTime(String path);

BASE_EXPORT OSFileChangeData * STDCALL OSMonitorFileStart(String path, bool suppressLogging = false);
BASE_EXPORT BOOL   STDCALL OSFileHasChanged (OSFileChangeData *data);
BASE_EXPORT VOID   STDCALL OSMonitorFileDestroy (OSFileChangeData *data);

BASE_EXPORT OSDirectoryMonitorData *OSMonitorDirectoryCallback(String path, OSDirectoryMonitorCallback callback);
BASE_EXPORT void                    OSMonitorDirectoryCallbackStop(OSDirectoryMonitorData *data);

BASE_EXPORT String STDCALL OSGetDefaultVideoSavePath(CTSTR append=nullptr);

BASE_EXPORT BOOL   STDCALL OSCreateMainWindow(int x, int y, int cx, int cy);
BASE_EXPORT void   STDCALL OSDestroyMainWindow();
BASE_EXPORT void   STDCALL OSSetWindowSize(int cx, int cy);
BASE_EXPORT void   STDCALL OSSetWindowFullscreen();
BASE_EXPORT void   STDCALL OSSetWindowPos(int x, int y);

BASE_EXPORT void   STDCALL OSShowCursor(BOOL bShow);
BASE_EXPORT void   STDCALL OSSetCursorPos(int x, int y);
BASE_EXPORT void   STDCALL OSGetCursorPos(int &x, int &y);

BASE_EXPORT HANDLE STDCALL OSFindFirstFile(CTSTR lpFileName, OSFindData &findData);
BASE_EXPORT BOOL   STDCALL OSFindNextFile(HANDLE hFind, OSFindData &findData);
BASE_EXPORT void   STDCALL OSFindClose(HANDLE hFind);

BASE_EXPORT BOOL   STDCALL OSChangeDisplaySettings(DISPLAYMODE *dm);
BASE_EXPORT void   STDCALL OSGetDisplaySettings(DISPLAYMODE *dm);
BASE_EXPORT void   STDCALL OSEnumDisplaySettings(List<DISPLAYMODE> &displayModes);

BASE_EXPORT HANDLE STDCALL OSLoadLibrary(CTSTR lpFile);
BASE_EXPORT DEFPROC STDCALL OSGetProcAddress(HANDLE hLibrary, LPCSTR lpProcedure);
BASE_EXPORT void   STDCALL OSFreeLibrary(HANDLE hLibrary);

BASE_EXPORT void   STDCALL OSSleep(DWORD dwMSeconds);
BASE_EXPORT void   STDCALL OSSleepSubMillisecond(double fMSeconds);
BASE_EXPORT void   STDCALL OSSleepMicrosecond(QWORD qwMicroseconds);
BASE_EXPORT void   STDCALL OSSleep100NS(QWORD qw100NSTime); //why

BASE_EXPORT int    STDCALL OSGetVersion();
BASE_EXPORT int    STDCALL OSGetTotalCores();
BASE_EXPORT int    STDCALL OSGetLogicalCores();
BASE_EXPORT HANDLE STDCALL OSCreateThread(XTHREAD lpThreadFunc, LPVOID param);
BASE_EXPORT HANDLE STDCALL OSGetCurrentThread();
BASE_EXPORT BOOL   STDCALL OSWaitForThread(HANDLE hThread, LPDWORD ret);
BASE_EXPORT BOOL   STDCALL OSCloseThread(HANDLE hThread);
BASE_EXPORT BOOL   STDCALL OSTerminateThread(HANDLE hThread, DWORD waitMS=100);

BASE_EXPORT HANDLE STDCALL OSCreateMutex();
BASE_EXPORT void   STDCALL OSEnterMutex(HANDLE hMutex);
BASE_EXPORT BOOL   STDCALL OSTryEnterMutex(HANDLE hMutex);
BASE_EXPORT void   STDCALL OSLeaveMutex(HANDLE hMutex);
BASE_EXPORT void   STDCALL OSCloseMutex(HANDLE hMutex);

BASE_EXPORT void           OSCloseEvent(HANDLE event);

BASE_EXPORT void   STDCALL OSSetMainAppWindow(HANDLE window);

BASE_EXPORT DWORD  STDCALL OSGetTime();
BASE_EXPORT QWORD  STDCALL OSGetTimeMicroseconds();
BASE_EXPORT QWORD  STDCALL OSGetThreadTime(HANDLE hThread);

BASE_EXPORT void __cdecl   OSMessageBoxva(const TCHAR *format, va_list argptr);
BASE_EXPORT void __cdecl   OSMessageBox(const TCHAR *format, ...);

BASE_EXPORT BOOL   STDCALL OSDebuggerPresent();
BASE_EXPORT void __cdecl   OSDebugOutva(const TCHAR *format, va_list argptr);
BASE_EXPORT void __cdecl   OSDebugOut(const TCHAR *format, ...);

BASE_EXPORT CTSTR STDCALL  OSGetErrorString(DWORD errorCode);

BASE_EXPORT BOOL   STDCALL OSGetLoadedModuleList(HANDLE hProcess, StringList &ModuleList);
BASE_EXPORT BOOL   STDCALL OSIncompatibleModulesLoaded();
BASE_EXPORT BOOL   STDCALL OSIncompatiblePatchesLoaded(String &errors);
BASE_EXPORT void   STDCALL OSCheckForBuggyDLLs ();

BASE_EXPORT void           OSRaiseException(DWORD code);

BASE_EXPORT void __cdecl   LogRaw(const TCHAR *text, UINT len=0);
BASE_EXPORT void __cdecl   Logva(const TCHAR *format, va_list argptr);
BASE_EXPORT void __cdecl   Log(const TCHAR *format, ...);

BASE_EXPORT __declspec(noreturn) void __cdecl   CrashError(const TCHAR *format, ...);
BASE_EXPORT __declspec(noreturn) void __cdecl   DumpError(const TCHAR *format, ...);

BASE_EXPORT void __cdecl   AppWarning(const TCHAR *format, ...);

BASE_EXPORT void   STDCALL TraceCrash(const TCHAR *trackName);
BASE_EXPORT void   STDCALL TraceCrashEnd();

BASE_EXPORT String CurrentTimeString();
BASE_EXPORT String CurrentDateTimeString();

typedef void(*LogUpdateCallback)();

BASE_EXPORT String CurrentLogFilename();
BASE_EXPORT void ReadLog(String &data); // do not call this while other threads use any logging functions
BASE_EXPORT void ReadLogPartial(String &data, unsigned &start, unsigned maxLength=UINT_MAX);
BASE_EXPORT void ResetLogUpdateCallback(LogUpdateCallback = nullptr);

//-----------------------------------------
//Base functions
//-----------------------------------------
BASE_EXPORT BOOL STDCALL InitXT(CTSTR logFile=NULL, CTSTR allocatorName=NULL);
BASE_EXPORT void STDCALL InitXTLog(CTSTR logFile);
BASE_EXPORT void STDCALL ResetXTAllocator(CTSTR lpAllocator);
BASE_EXPORT void STDCALL TerminateXT();

BASE_EXPORT extern BOOL bDebugBreak;


//-----------------------------------------
//defines
//-----------------------------------------
#define MIN(a, b)               (((a) < (b)) ? (a) : (b))
#define MAX(a, b)               (((a) > (b)) ? (a) : (b))
#define MAKEDWORD(low, high)    ((DWORD)(low) | ((DWORD)(high) << 16))
#define MAKEQUAD(low, high)     ((QWORD)(low) | ((QWORD)(high) << 32))
#define LODW(quad)              ((DWORD)(quad))
#define HIDW(quad)              ((DWORD)((quad) >> 32))
#define HIWORD(l)               ((WORD)((l) >> 16))
#define LOWORD(l)               ((WORD)(l))

#ifndef assert
    #ifdef _DEBUG
        #define assert(check)           if(!(check)) CrashError(TEXT("Assertion Failiure: (") TEXT(#check) TEXT(") failed\r\nFile: %s, line %d"), TEXT(__FILE__), __LINE__);
    #else
        #define assert(check)
    #endif
#endif

#ifndef assertmsg
    #ifdef _DEBUG
        #define assertmsg(check, msg)   if(!(check)) CrashError(TEXT("Assertion Failiure: %s\r\nFile: %s, line %d"), (TSTR)msg, TEXT(__FILE__), __LINE__);
    #else
        #define assertmsg(check, msg)
    #endif
#endif


//-----------------------------------------
//color defines
//-----------------------------------------
#define Color_White              0xFFFFFFFF  //255,255,255
#define Color_LightGray          0xFFE0E0E0  //224,224,224
#define Color_Gray               0xFFC0C0C0  //192,192,192
#define Color_DarkGray           0xFF808080  //128,128,128
#define Color_Black              0           //0,0,0      
                                               
#define Color_Cyan               0xFF00FFFF  //0,255,255  
#define Color_DarkCyan           0xFF008080  //0,128,128  
#define Color_Purple             0xFFFF00FF  //255,0,255  
#define Color_DarkPurple         0xFF800080  //128,0,128  
#define Color_Yellow             0xFFFFFF00  //255,255,0  
#define Color_DarkYellow         0xFF808000  //128,128,0  
                                               
#define Color_Red                0xFFFF0000  //255,0,0    
#define Color_DarkRed            0xFF800000  //128,0,0    
#define Color_Green              0xFF00FF00  //0,255,0    
#define Color_DarkGreen          0xFF008000  //0,128,0    
#define Color_Blue               0xFF0000FF  //0,0,255    
#define Color_DarkBlue           0xFF000080  //0,0,128    

#define Color_Orange             0xFFFF8000

#define RGB_A(rgba)         (((DWORD)(rgba) & 0xFF000000) >> 24)
#define RGB_R(rgb)          (((DWORD)(rgb) & 0xFF0000) >> 16)
#define RGB_G(rgb)          (((DWORD)(rgb) & 0x00FF00) >> 8)
#define RGB_B(rgb)          ((DWORD)(rgb) & 0x0000FF)

#define RGB_Af(rgba)        (((float)RGB_A(rgba)) / 255.0f)
#define RGB_Rf(rgb)         (((float)RGB_R(rgb)) / 255.0f)
#define RGB_Gf(rgb)         (((float)RGB_G(rgb)) / 255.0f)
#define RGB_Bf(rgb)         (((float)RGB_B(rgb)) / 255.0f)

#define MAKERGBA(r,g,b,a)   ((((DWORD)a) << 24)|(((DWORD)r) << 16)|(((DWORD)g) << 8)|((DWORD)b))
#define MAKEBGRA(r,g,b,a)   ((((DWORD)a) << 24)|(((DWORD)b) << 16)|(((DWORD)g) << 8)|((DWORD)r))
#define MAKERGB(r,g,b)      ((((DWORD)r << 16)|((DWORD)g << 8)|(DWORD)b))
#define MAKEBGR(r,g,b)      ((((DWORD)b << 16)|((DWORD)g << 8)|(DWORD)r))

#define REVERSE_COLOR(col)  MAKERGB(RGB_B(col), RGB_G(col), RGB_R(col))

#define Vect4_to_RGBA(v)    (MAKERGBA(((v).x*255.0f), ((v).y*255.0f), ((v).z*255.0f), ((v).w*255.0f)))
#define Vect_to_RGB(v)      (MAKERGB(((v).x*255.0f), ((v).y*255.0f), ((v).z*255.0f)))
#define RGBA_to_Vect4(dw)   Vect4(RGB_Rf(dw), RGB_Gf(dw), RGB_Bf(dw), RGB_Af(dw))
#define RGB_to_Vect(dw)     Vect(RGB_Rf(dw), RGB_Gf(dw), RGB_Bf(dw))
#define RGB_to_VectExp(dw)  Vect((RGB_Rf(dw)-0.5f)*2.0f, (RGB_Gf(dw)-0.5f)*2.0f, (RGB_Bf(dw)-0.5f)*2.0f)

#define Color4_to_RGBA(c)   (MAKERGBA(((c).x*255.0f), ((c).y*255.0f), ((c).z*255.0f), ((c).w*255.0f)))
#define Color_to_RGB(c)     (MAKERGB(((c).x*255.0f), ((c).y*255.0f), ((c).z*255.0f)))
#define RGBA_to_Color4(dw)  Color4(RGB_Rf(dw), RGB_Gf(dw), RGB_Bf(dw), RGB_Af(dw))
#define RGB_to_Color(dw)    Color(RGB_Rf(dw), RGB_Gf(dw), RGB_Bf(dw))
#define RGB_to_ColorExp(dw) Color((RGB_Rf(dw)-0.5f)*2.0f, (RGB_Gf(dw)-0.5f)*2.0f, (RGB_Bf(dw)-0.5f)*2.0f)

//-----------------------------------------
//includes
//-----------------------------------------
#include "utf8.h"
#include "Serializer.h"
#include "Inline.h"
#include "Alloc.h"
#include "FastAlloc.h"
#include "DebugAlloc.h"
#include "Template.h"
#include "XString.h"
#include "XMath.h"
#include "ConfigFile.h"
#include "XFile.h"
#include "Profiler.h"
#include "XTLocalization.h"
#include "XConfig.h"

#include "RAIIHelpers.h"
#include "ComPtr.hpp"
