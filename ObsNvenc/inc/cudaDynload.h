/********************************************************************************
Copyright (C) 2014 Timo Rothenpieler <timo@rothenpieler.org>

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

#ifndef H_OBSNVENC_CUDADYNLOAD__H
#define H_OBSNVENC_CUDADYNLOAD__H

typedef enum cudaError_enum {
    CUDA_SUCCESS = 0
} CUresult;
typedef int CUdevice;
typedef void* CUcontext;

#define CUDAAPI __stdcall

typedef CUresult(CUDAAPI *PCUINIT)(unsigned int Flags);
typedef CUresult(CUDAAPI *PCUDEVICEGETCOUNT)(int *count);
typedef CUresult(CUDAAPI *PCUDEVICEGET)(CUdevice *device, int ordinal);
typedef CUresult(CUDAAPI *PCUDEVICEGETNAME)(char *name, int len, CUdevice dev);
typedef CUresult(CUDAAPI *PCUDEVICECOMPUTECAPABILITY)(int *major, int *minor, CUdevice dev);
typedef CUresult(CUDAAPI *PCUCTXCREATE)(CUcontext *pctx, unsigned int flags, CUdevice dev);
typedef CUresult(CUDAAPI *PCUCTXPOPCURRENT)(CUcontext *pctx);
typedef CUresult(CUDAAPI *PCUCTXDESTROY)(CUcontext ctx);

extern PCUINIT cuInit;
extern PCUDEVICEGETCOUNT cuDeviceGetCount;
extern PCUDEVICEGET cuDeviceGet;
extern PCUDEVICEGETNAME cuDeviceGetName;
extern PCUDEVICECOMPUTECAPABILITY cuDeviceComputeCapability;
extern PCUCTXCREATE cuCtxCreate;
extern PCUCTXPOPCURRENT cuCtxPopCurrent;
extern PCUCTXDESTROY cuCtxDestroy;

bool dyLoadCuda();

#endif
