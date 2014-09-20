#pragma once

//Min supported is Win7
//Redef in obsapi.h
#pragma warning(disable: 4005)
#define WINVER         0x0601
#define _WIN32_WINDOWS 0x0601
#define _WIN32_WINNT   0x0601
#define NTDDI_VERSION  NTDDI_WIN7
#define WIN32_LEAN_AND_MEAN
#define ISOLATION_AWARE_ENABLED 1
#include <d3d11.h>
#include <Windows.h>
#include <d3d9.h>
#include <d3d9types.h>

#include "targetver.h"
#include <cstdint>
#include <queue>
#include <vector>
//#include <ErrorCodes.h>
#include <CL\cl.h>
#include <CL\cl_ext.h>

//Include before OBS
#include <atlbase.h>
#include <atlcomcli.h>

#include "VersionHelpers.h"
#include "OBS-min.h"
//#define VCELog(...) Log(__VA_ARGS__)
//#define AppConfig (*VCEAppConfig)