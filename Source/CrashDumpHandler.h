/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>
                    Richard Stanway

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


#define DBGHELP_TRANSLATE_TCHAR 1

#include <DbgHelp.h>
#include <shellapi.h>

typedef BOOL (WINAPI *ENUMERATELOADEDMODULES64) (HANDLE hProcess, PENUMLOADED_MODULES_CALLBACK64 EnumLoadedModulesCallback, PVOID UserContext);
typedef DWORD (WINAPI *SYMSETOPTIONS) (DWORD SymOptions);
typedef BOOL (WINAPI *SYMINITIALIZE) (HANDLE hProcess, PCTSTR UserSearchPath, BOOL fInvadeProcess);
typedef BOOL (WINAPI *SYMCLEANUP) (HANDLE hProcess);
typedef BOOL (WINAPI *STACKWALK64) (
    DWORD MachineType,
    HANDLE hProcess,
    HANDLE hThread,
    LPSTACKFRAME64 StackFrame,
    PVOID ContextRecord,
    PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
    PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
    PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
    PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress
);

typedef PVOID    (WINAPI *SYMFUNCTIONTABLEACCESS64) (HANDLE hProcess, DWORD64 AddrBase);
typedef DWORD64 (WINAPI *SYMGETMODULEBASE64) (HANDLE hProcess, DWORD64 dwAddr);
typedef BOOL    (WINAPI *SYMFROMADDR) (HANDLE hProcess, DWORD64 Address, PDWORD64 Displacement, PSYMBOL_INFO Symbol);
typedef BOOL    (WINAPI *SYMGETMODULEINFO64) (HANDLE hProcess, DWORD64 dwAddr, PIMAGEHLP_MODULE64 ModuleInfo);

typedef DWORD64 (WINAPI *SYMLOADMODULE64) (HANDLE hProcess, HANDLE hFile, PSTR ImageName, PSTR ModuleName, DWORD64 BaseOfDll, DWORD SizeOfDll);

typedef BOOL (WINAPI *MINIDUMPWRITEDUMP) (
    HANDLE hProcess,
    DWORD ProcessId,
    HANDLE hFile,
    MINIDUMP_TYPE DumpType,
    PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
    PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
    PMINIDUMP_CALLBACK_INFORMATION CallbackParam
);

typedef HINSTANCE (WINAPI *SHELLEXECUTEA) (HWND hwnd, LPCTSTR lpOperation, LPCTSTR lpFile, LPCTSTR lpParameters, LPCTSTR lpDirectory, INT nShowCmd);

typedef struct moduleinfo_s
{
    DWORD64 faultAddress;
    TCHAR   moduleName[MAX_PATH];
} moduleinfo_t;
