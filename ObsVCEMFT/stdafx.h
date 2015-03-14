#pragma once

//Min supported is Win7
//Redef in obsapi.h
#pragma warning(disable: 4005)
#define WINVER         0x0601
#define _WIN32_WINDOWS 0x0601
#define _WIN32_WINNT   0x0601
#define NTDDI_VERSION  NTDDI_WIN7
//#define WIN32_LEAN_AND_MEAN
#define ISOLATION_AWARE_ENABLED 1

#include "targetver.h"
#include <cstdint>
#include <queue>
#include <vector>
//#include <ErrorCodes.h>
#include "MftUtils.h"
#include "OBS-min.h"