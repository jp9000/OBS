#pragma once

// DX10 error defines redef. dxsdk2010 vs Windows Kits/8.1
#pragma warning(disable: 4005)

//#include "targetver.h"

//#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
//#include <windows.h>
#include <string>
#include <map>
#include <Main.h>
#include <algorithm>
#include "CL\cl.h"
#include "CL\cl_ext.h"
#include "CL\cl_d3d10.h"