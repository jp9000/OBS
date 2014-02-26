/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

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

#define WINVER         0x0600
#define _WIN32_WINDOWS 0x0600
#define _WIN32_WINNT   0x0600
#define NTDDI_VERSION  NTDDI_VISTASP1

#define WIN32_LEAN_AND_MEAN
#define ISOLATION_AWARE_ENABLED 1
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include <xmmintrin.h>
#include <emmintrin.h>

#define USE_TRACE 1
#include "Utility/XT.h"

//-------------------------------------------

BASE_EXPORT void LocalizeWindow(HWND hwnd, LocaleStringLookup *lookup=NULL);
BASE_EXPORT void LocalizeMenu(HMENU hMenu, LocaleStringLookup *lookup=NULL);

BASE_EXPORT int OBSMessageBox(HWND hwnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT flags);
BASE_EXPORT INT_PTR OBSDialogBox(HINSTANCE hInstance, LPCWSTR lpTemplateName, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam = 0);
BASE_EXPORT HWND OBSCreateDialog(HINSTANCE hInstance, LPCWSTR lpTemplateName, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam = 0);

BASE_EXPORT String GetLBText(HWND hwndList, UINT id=LB_ERR);
BASE_EXPORT String GetLVText(HWND hwndList, UINT id);
BASE_EXPORT String GetCBText(HWND hwndCombo, UINT id=CB_ERR);
BASE_EXPORT String GetEditText(HWND hwndEdit);

BASE_EXPORT LPBYTE GetCursorData(HICON hIcon, ICONINFO &ii, UINT &width, UINT &height);


#define SafeReleaseLogRef(var) if(var) {ULONG chi = var->Release(); OSDebugOut(TEXT("releasing %s, %d refs were left\r\n"), L#var, chi); var = NULL;}
#define SafeRelease(var) if(var) {var->Release(); var = NULL;}

//big endian conversion functions
#define QWORD_BE(val) (((val>>56)&0xFF) | (((val>>48)&0xFF)<<8) | (((val>>40)&0xFF)<<16) | (((val>>32)&0xFF)<<24) | \
    (((val>>24)&0xFF)<<32) | (((val>>16)&0xFF)<<40) | (((val>>8)&0xFF)<<48) | ((val&0xFF)<<56))
#define DWORD_BE(val) (((val>>24)&0xFF) | (((val>>16)&0xFF)<<8) | (((val>>8)&0xFF)<<16) | ((val&0xFF)<<24))
#define WORD_BE(val)  (((val>>8)&0xFF) | ((val&0xFF)<<8))

__forceinline QWORD fastHtonll(QWORD qw) {return QWORD_BE(qw);}
__forceinline DWORD fastHtonl (DWORD dw) {return DWORD_BE(dw);}
__forceinline  WORD fastHtons (WORD  w)  {return  WORD_BE(w);}

//arghh I hate defines like this
#define RUNONCE static bool bRunOnce = false; if(!bRunOnce && (bRunOnce = true))

inline BOOL CloseDouble(double f1, double f2, double precision=0.001)
{
    return fabs(f1-f2) <= precision;
}

inline QWORD GetQWDif(QWORD val1, QWORD val2)
{
    return (val1 > val2) ? (val1-val2) : (val2-val1);
}

BASE_EXPORT QWORD GetQPCTimeNS();
BASE_EXPORT QWORD GetQPCTime100NS();
BASE_EXPORT QWORD GetQPCTimeMS();
BASE_EXPORT void MixAudio(float *bufferDest, float *bufferSrc, UINT totalFloats, bool bForceMono);

//-------------------------------------------

#include "GraphicsSystem.h"
#include "Scene.h"
#include "SettingsPane.h"
#include "APIInterface.h"
#include "AudioFilter.h"
#include "AudioSource.h"
#include "HotkeyControlEx.h"
#include "ColorControl.h"
#include "VolumeControl.h"
#include "VolumeMeter.h"
