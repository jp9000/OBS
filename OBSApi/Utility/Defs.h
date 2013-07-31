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

#undef UINT
#undef LPUINT
#undef ULONG
#undef DWORD
#undef LPDWORD
#undef USHORT
#undef WORD
#undef LPWORD
#undef UCHAR
#undef BYTE
#undef LPBYTE
#undef BOOL
#undef INT
#undef INT8
#undef INT16
#undef INT32
#undef INT64
#undef LPINT
#undef LONG
#undef LPLONG
#undef SHORT
#undef LPSHORT
#undef CHAR
#undef TCHAR
#undef TSTR
#undef LPCHAR
#undef VOID
#undef HANDLE
#undef LONG64
#undef ULONG64

#undef TRUE
#undef FALSE

#undef HIWORD
#undef LOWORD

#undef TEXT2
#undef TEXT


#ifdef WIN32
typedef unsigned long       ULONG,DWORD,*LPDWORD;
typedef long                LONG,*LPLONG;
#else
#endif

typedef signed char         INT8;
typedef signed short        INT16;

typedef unsigned char       UINT8;
typedef unsigned short      UINT16;

typedef int                 BOOL,INT,*LPINT;
typedef unsigned int        UINT,*LPUINT;

typedef short               SHORT,*LPSHORT;
typedef unsigned short      USHORT,WORD,*LPWORD;

typedef char                CHAR,*LPCHAR;
typedef unsigned char       UCHAR,BYTE,*LPBYTE;

typedef long long           LONG64,INT64,LONGLONG;
typedef unsigned long long  QWORD,ULONG64,UINT64,ULONGLONG;

#ifdef C64
typedef long long           PARAM;
typedef unsigned long long  UPARAM;
#else
typedef long                PARAM;
typedef unsigned long       UPARAM;
#endif


#ifdef UNICODE
typedef wchar_t TCHAR;
#define TEXT2(val)  L ## val
#define TEXT(val)  TEXT2(val)
#else
typedef char TCHAR;
#define TEXT(val)  val
#endif

typedef char                *LPSTR;
typedef const char          *LPCSTR;
typedef wchar_t             *WSTR;
typedef const wchar_t       *CWSTR;
typedef TCHAR               *TSTR;
typedef const TCHAR         *CTSTR;

typedef void                VOID,*LPVOID,*HANDLE;
typedef const void          *LPCVOID;


#define HIWORD(l)  ((WORD)((l) >> 16))
#define LOWORD(l)  ((WORD)(l))

#define TRUE  1
#define FALSE 0

#define INVALID     0xFFFFFFFF
