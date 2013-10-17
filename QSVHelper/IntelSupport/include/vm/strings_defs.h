/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2011-2013 Intel Corporation. All Rights Reserved.
//
//
*/

#ifndef __STRING_DEFS_H__
#define __STRING_DEFS_H__

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include <tchar.h>

#define MSDK_STRING(x) _T(x)
#define MSDK_CHAR(x) _T(x)
typedef TCHAR msdk_char;

#define msdk_printf   _tprintf
#define msdk_fprintf  _ftprintf
#define msdk_sprintf  _stprintf_s
#define msdk_vprintf  _vtprintf
#define msdk_strlen   _tcslen
#define msdk_strcmp   _tcscmp
#define msdk_strncmp  _tcsnicmp
#define msdk_strstr   _tcsstr
#define msdk_sscanf   _stscanf_s
#define msdk_atoi     _ttoi
#define msdk_strtol   _tcstol
#define msdk_strtod   _tcstod
#define msdk_strchr   _tcschr
#define msdk_itoa_decimal(value, str)   _itow_s(value, str, 4, 10)

// msdk_strcopy is intended to be used with 2 parmeters, i.e. msdk_strcopy(dst, src)
// for _tcscpy_s that's possible if DST is declared as: TCHAR DST[n];
#define msdk_strcopy _tcscpy_s

#endif //__STRING_DEFS_H__
