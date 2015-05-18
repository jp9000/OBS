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


#include "Main.h"
#include "CrashDumpHandler.h"

BOOL CALLBACK EnumerateLoadedModulesProcInfo (PCTSTR ModuleName, DWORD64 ModuleBase, ULONG ModuleSize, PVOID UserContext)
{
    moduleinfo_t *moduleInfo = (moduleinfo_t *)UserContext;
    if (moduleInfo->faultAddress >= ModuleBase && moduleInfo->faultAddress <= ModuleBase + ModuleSize)
    {
        scpy_n(moduleInfo->moduleName, ModuleName, _countof(moduleInfo->moduleName)-1);
        return FALSE;
    }
    return TRUE;
}

BOOL CALLBACK RecordAllLoadedModules (PCTSTR ModuleName, DWORD64 ModuleBase, ULONG ModuleSize, PVOID UserContext)
{
    String &str = *(String*)UserContext;

#ifdef _WIN64
    str << FormattedString(TEXT("%016I64X-%016I64X %s\r\n"), ModuleBase, ModuleBase+ModuleSize, ModuleName);
#else
    str << FormattedString(TEXT("%08.8I64X-%08.8I64X %s\r\n"), ModuleBase, ModuleBase+ModuleSize, ModuleName);
#endif
    return TRUE;
}

LONG CALLBACK OBSExceptionHandler (PEXCEPTION_POINTERS exceptionInfo)
{
    HANDLE  hProcess;

    HMODULE hDbgHelp;

    MINIDUMP_EXCEPTION_INFORMATION miniInfo;

    STACKFRAME64        frame = {0};
    CONTEXT             context = *exceptionInfo->ContextRecord;
    SYMBOL_INFO         *symInfo;
    DWORD64             fnOffset;
    TCHAR               logPath[MAX_PATH];

    OSVERSIONINFOEX     osInfo;
    SYSTEMTIME          timeInfo;

    ENUMERATELOADEDMODULES64    fnEnumerateLoadedModules64;
    SYMSETOPTIONS               fnSymSetOptions;
    SYMINITIALIZE               fnSymInitialize;
    STACKWALK64                 fnStackWalk64;
    SYMFUNCTIONTABLEACCESS64    fnSymFunctionTableAccess64;
    SYMGETMODULEBASE64          fnSymGetModuleBase64;
    SYMFROMADDR                 fnSymFromAddr;
    SYMCLEANUP                  fnSymCleanup;
    MINIDUMPWRITEDUMP           fnMiniDumpWriteDump;
    SYMGETMODULEINFO64          fnSymGetModuleInfo64;

    DWORD                       i;
    DWORD64                     InstructionPtr;
    DWORD                       imageType;

    TCHAR                       searchPath[MAX_PATH], *p;

    static BOOL                 inExceptionHandler = FALSE;

    moduleinfo_t                moduleInfo;

    //always break into a debugger if one is present
    if (IsDebuggerPresent ())
        return EXCEPTION_CONTINUE_SEARCH;

    //exception codes < 0x80000000 are typically informative only and not crash worthy
    //0xe06d7363 indicates a c++ exception was thrown, let's just hope it was caught.
    //this is no longer needed since we're an unhandled handler vs a vectored handler
    
    /*if (exceptionInfo->ExceptionRecord->ExceptionCode < 0x80000000 || exceptionInfo->ExceptionRecord->ExceptionCode == 0xe06d7363 ||
        exceptionInfo->ExceptionRecord->ExceptionCode == 0x800706b5)
        return EXCEPTION_CONTINUE_SEARCH;*/

    //uh oh, we're crashing inside ourselves... this is really bad!
    if (inExceptionHandler)
        return EXCEPTION_CONTINUE_SEARCH;

    inExceptionHandler = TRUE;

    //load dbghelp dynamically
    hDbgHelp = LoadLibrary (TEXT("DBGHELP"));

    if (!hDbgHelp)
        return EXCEPTION_CONTINUE_SEARCH;

    fnEnumerateLoadedModules64 = (ENUMERATELOADEDMODULES64)GetProcAddress (hDbgHelp, "EnumerateLoadedModulesW64");
    fnSymSetOptions = (SYMSETOPTIONS)GetProcAddress (hDbgHelp, "SymSetOptions");
    fnSymInitialize = (SYMINITIALIZE)GetProcAddress (hDbgHelp, "SymInitialize");
    fnSymFunctionTableAccess64 = (SYMFUNCTIONTABLEACCESS64)GetProcAddress (hDbgHelp, "SymFunctionTableAccess64");
    fnSymGetModuleBase64 = (SYMGETMODULEBASE64)GetProcAddress (hDbgHelp, "SymGetModuleBase64");
    fnStackWalk64 = (STACKWALK64)GetProcAddress (hDbgHelp, "StackWalk64");
    fnSymFromAddr = (SYMFROMADDR)GetProcAddress (hDbgHelp, "SymFromAddrW");
    fnSymCleanup = (SYMCLEANUP)GetProcAddress (hDbgHelp, "SymCleanup");
    fnSymGetModuleInfo64 = (SYMGETMODULEINFO64)GetProcAddress (hDbgHelp, "SymGetModuleInfo64");
    fnMiniDumpWriteDump = (MINIDUMPWRITEDUMP)GetProcAddress (hDbgHelp, "MiniDumpWriteDump");

    if (!fnEnumerateLoadedModules64 || !fnSymSetOptions || !fnSymInitialize || !fnSymFunctionTableAccess64 ||
        !fnSymGetModuleBase64 || !fnStackWalk64 || !fnSymFromAddr || !fnSymCleanup || !fnSymGetModuleInfo64)
    {
        FreeLibrary (hDbgHelp);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    hProcess = GetCurrentProcess();

    fnSymSetOptions (SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_LOAD_ANYTHING);

    GetModuleFileName (NULL, searchPath, _countof(searchPath)-1);
    p = srchr (searchPath, '\\');
    if (p)
        *p = 0;

    //create a log file
    GetSystemTime (&timeInfo);
    for (i = 1;;)
    {
        tsprintf_s (logPath, _countof(logPath)-1, TEXT("%s\\crashDumps\\OBSCrashLog%.4d-%.2d-%.2d_%d.txt"), lpAppDataPath, timeInfo.wYear, timeInfo.wMonth, timeInfo.wDay, i);
        if (GetFileAttributes(logPath) == INVALID_FILE_ATTRIBUTES)
            break;
        i++;
    }

    XFile   crashDumpLog;

    if (!crashDumpLog.Open(logPath, XFILE_WRITE, XFILE_CREATENEW))
    {
        FreeLibrary (hDbgHelp);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    //initialize debug symbols
    fnSymInitialize (hProcess, NULL, TRUE);

#ifdef _WIN64
    InstructionPtr = context.Rip;
    frame.AddrPC.Offset = InstructionPtr;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrStack.Offset = context.Rsp;
    imageType = IMAGE_FILE_MACHINE_AMD64;
#else
    InstructionPtr = context.Eip;
    frame.AddrPC.Offset = InstructionPtr;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrStack.Offset = context.Esp;
    imageType = IMAGE_FILE_MACHINE_I386;
#endif

    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    symInfo = (SYMBOL_INFO *)LocalAlloc (LPTR, sizeof(*symInfo) + 256);
    symInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    symInfo->MaxNameLen = 256;
    fnOffset = 0;

    //get os info
    memset (&osInfo, 0, sizeof(osInfo));
    osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

    if (!GetVersionEx ((OSVERSIONINFO *)&osInfo))
    {
        osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx ((OSVERSIONINFO *)&osInfo);
    }

    String cpuInfo;
    HKEY key;

    // get cpu info
    if(RegOpenKey(HKEY_LOCAL_MACHINE, TEXT("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"), &key) == ERROR_SUCCESS)
    {
        DWORD dwSize = 1024;
        cpuInfo.SetLength(dwSize);
        if (RegQueryValueEx(key, TEXT("ProcessorNameString"), NULL, NULL, (LPBYTE)cpuInfo.Array(), &dwSize) != ERROR_SUCCESS)
            cpuInfo = TEXT("<unable to query>");
        RegCloseKey(key);
    }
    else
        cpuInfo = TEXT("<unable to query>");

    //determine which module the crash occured in
    scpy (moduleInfo.moduleName, TEXT("<unknown>"));
    moduleInfo.faultAddress = InstructionPtr;
    fnEnumerateLoadedModules64 (hProcess, (PENUMLOADED_MODULES_CALLBACK64)EnumerateLoadedModulesProcInfo, (VOID *)&moduleInfo);
    slwr (moduleInfo.moduleName);

    BOOL isPlugin = FALSE;

    if (sstr (moduleInfo.moduleName, TEXT("plugins\\")))
        isPlugin = TRUE;

    String strModuleInfo;
    String crashMessage;

    fnEnumerateLoadedModules64(hProcess, (PENUMLOADED_MODULES_CALLBACK64)RecordAllLoadedModules, (VOID *)&strModuleInfo);

    crashMessage << 
        TEXT("OBS has encountered an unhandled exception and has terminated. If you are able to\r\n")
        TEXT("reproduce this crash, please submit this crash report on the forums at\r\n")
        TEXT("https://obsproject.com/ - include the contents of this crash log and the\r\n")
        TEXT("minidump .dmp file (if available) as well as your regular OBS log files and\r\n")
        TEXT("a description of what you were doing at the time of the crash.\r\n")
        TEXT("\r\n")
        TEXT("This crash appears to have occured in the '") << moduleInfo.moduleName << TEXT("' module.\r\n\r\n");

    crashDumpLog.WriteStr(crashMessage.Array());

    crashDumpLog.WriteStr(FormattedString(TEXT("**** UNHANDLED EXCEPTION: %x\r\nFault address: %I64p (%s)\r\n"), exceptionInfo->ExceptionRecord->ExceptionCode, InstructionPtr, moduleInfo.moduleName));

    crashDumpLog.WriteStr(TEXT("OBS version: ") OBS_VERSION_STRING TEXT("\r\n"));
    crashDumpLog.WriteStr(FormattedString(TEXT("Windows version: %d.%d (Build %d) %s\r\nCPU: %s\r\n\r\n"), osInfo.dwMajorVersion, osInfo.dwMinorVersion, osInfo.dwBuildNumber, osInfo.szCSDVersion, cpuInfo.Array()));

    crashDumpLog.WriteStr(TEXT("Crashing thread stack trace:\r\n"));
#ifdef _WIN64
    crashDumpLog.WriteStr(TEXT("Stack            EIP              Arg0             Arg1             Arg2             Arg3             Address\r\n"));
#else
    crashDumpLog.WriteStr(TEXT("Stack    EIP      Arg0     Arg1     Arg2     Arg3     Address\r\n"));
#endif
    crashDumpLog.FlushFileBuffers();

    while (fnStackWalk64 (imageType, hProcess, GetCurrentThread(), &frame, &context, NULL, (PFUNCTION_TABLE_ACCESS_ROUTINE64)fnSymFunctionTableAccess64, (PGET_MODULE_BASE_ROUTINE64)fnSymGetModuleBase64, NULL))
    {
        scpy (moduleInfo.moduleName, TEXT("<unknown>"));
        moduleInfo.faultAddress = frame.AddrPC.Offset;
        fnEnumerateLoadedModules64 (hProcess, (PENUMLOADED_MODULES_CALLBACK64)EnumerateLoadedModulesProcInfo, (VOID *)&moduleInfo);
        slwr (moduleInfo.moduleName);

        p = srchr (moduleInfo.moduleName, '\\');
        if (p)
            p++;
        else
            p = moduleInfo.moduleName;

#ifdef _WIN64
        if (fnSymFromAddr (hProcess, frame.AddrPC.Offset, &fnOffset, symInfo) && !(symInfo->Flags & SYMFLAG_EXPORT))
        {
            crashDumpLog.WriteStr(FormattedString(TEXT("%016I64X %016I64X %016I64X %016I64X %016I64X %016I64X %s!%s+0x%I64x\r\n"),
                frame.AddrStack.Offset,
                frame.AddrPC.Offset,
                frame.Params[0],
                frame.Params[1],
                frame.Params[2],
                frame.Params[3],
                p,
                symInfo->Name,
                fnOffset));
        }
        else
        {
            crashDumpLog.WriteStr(FormattedString(TEXT("%016I64X %016I64X %016I64X %016I64X %016I64X %016I64X %s!0x%I64x\r\n"),
                frame.AddrStack.Offset,
                frame.AddrPC.Offset,
                frame.Params[0],
                frame.Params[1],
                frame.Params[2],
                frame.Params[3],
                p,
                frame.AddrPC.Offset));
        }
#else
        if (fnSymFromAddr (hProcess, frame.AddrPC.Offset, &fnOffset, symInfo) && !(symInfo->Flags & SYMFLAG_EXPORT))
        {
            crashDumpLog.WriteStr(FormattedString(TEXT("%08.8I64X %08.8I64X %08.8X %08.8X %08.8X %08.8X %s!%s+0x%I64x\r\n"),
                frame.AddrStack.Offset,
                frame.AddrPC.Offset,
                (DWORD)frame.Params[0],
                (DWORD)frame.Params[1],
                (DWORD)frame.Params[2],
                (DWORD)frame.Params[3],
                p,
                symInfo->Name,
                fnOffset));
        }
        else
        {
            crashDumpLog.WriteStr(FormattedString(TEXT("%08.8I64X %08.8I64X %08.8X %08.8X %08.8X %08.8X %s!0x%I64x\r\n"),
                frame.AddrStack.Offset,
                frame.AddrPC.Offset,
                (DWORD)frame.Params[0],
                (DWORD)frame.Params[1],
                (DWORD)frame.Params[2],
                (DWORD)frame.Params[3],
                p,
                frame.AddrPC.Offset
                ));
        }
#endif

        crashDumpLog.FlushFileBuffers();
    }

    //if we manually crashed due to a deadlocked thread, record some extra info
    if (exceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT)
    {
        HANDLE hVideoThread = NULL, hEncodeThread = NULL;
        if (App)
            App->GetThreadHandles (&hVideoThread, &hEncodeThread);

        if (hVideoThread)
        {
            crashDumpLog.WriteStr(TEXT("\r\nVideo thread stack trace:\r\n"));
#ifdef _WIN64
            crashDumpLog.WriteStr(TEXT("Stack            EIP              Arg0             Arg1             Arg2             Arg3             Address\r\n"));
#else
            crashDumpLog.WriteStr(TEXT("Stack    EIP      Arg0     Arg1     Arg2     Arg3     Address\r\n"));
#endif
            crashDumpLog.FlushFileBuffers();

            context.ContextFlags = CONTEXT_ALL;
            GetThreadContext (hVideoThread, &context);
            ZeroMemory (&frame, sizeof(frame));
#ifdef _WIN64
            InstructionPtr = context.Rip;
            frame.AddrPC.Offset = InstructionPtr;
            frame.AddrFrame.Offset = context.Rbp;
            frame.AddrStack.Offset = context.Rsp;
            imageType = IMAGE_FILE_MACHINE_AMD64;
#else
            InstructionPtr = context.Eip;
            frame.AddrPC.Offset = InstructionPtr;
            frame.AddrFrame.Offset = context.Ebp;
            frame.AddrStack.Offset = context.Esp;
            imageType = IMAGE_FILE_MACHINE_I386;
#endif

            frame.AddrFrame.Mode = AddrModeFlat;
            frame.AddrPC.Mode = AddrModeFlat;
            frame.AddrStack.Mode = AddrModeFlat;
            while (fnStackWalk64 (imageType, hProcess, hVideoThread, &frame, &context, NULL, (PFUNCTION_TABLE_ACCESS_ROUTINE64)fnSymFunctionTableAccess64, (PGET_MODULE_BASE_ROUTINE64)fnSymGetModuleBase64, NULL))
            {
                scpy (moduleInfo.moduleName, TEXT("<unknown>"));
                moduleInfo.faultAddress = frame.AddrPC.Offset;
                fnEnumerateLoadedModules64 (hProcess, (PENUMLOADED_MODULES_CALLBACK64)EnumerateLoadedModulesProcInfo, (VOID *)&moduleInfo);
                slwr (moduleInfo.moduleName);

                p = srchr (moduleInfo.moduleName, '\\');
                if (p)
                    p++;
                else
                    p = moduleInfo.moduleName;

#ifdef _WIN64
                if (fnSymFromAddr (hProcess, frame.AddrPC.Offset, &fnOffset, symInfo) && !(symInfo->Flags & SYMFLAG_EXPORT))
                {
                    crashDumpLog.WriteStr(FormattedString(TEXT("%016I64X %016I64X %016I64X %016I64X %016I64X %016I64X %s!%s+0x%I64x\r\n"),
                        frame.AddrStack.Offset,
                        frame.AddrPC.Offset,
                        frame.Params[0],
                        frame.Params[1],
                        frame.Params[2],
                        frame.Params[3],
                        p,
                        symInfo->Name,
                        fnOffset));
                }
                else
                {
                    crashDumpLog.WriteStr(FormattedString(TEXT("%016I64X %016I64X %016I64X %016I64X %016I64X %016I64X %s!0x%I64x\r\n"),
                        frame.AddrStack.Offset,
                        frame.AddrPC.Offset,
                        frame.Params[0],
                        frame.Params[1],
                        frame.Params[2],
                        frame.Params[3],
                        p,
                        frame.AddrPC.Offset));
                }
#else
                if (fnSymFromAddr (hProcess, frame.AddrPC.Offset, &fnOffset, symInfo) && !(symInfo->Flags & SYMFLAG_EXPORT))
                {
                    crashDumpLog.WriteStr(FormattedString(TEXT("%08.8I64X %08.8I64X %08.8X %08.8X %08.8X %08.8X %s!%s+0x%I64x\r\n"),
                        frame.AddrStack.Offset,
                        frame.AddrPC.Offset,
                        (DWORD)frame.Params[0],
                        (DWORD)frame.Params[1],
                        (DWORD)frame.Params[2],
                        (DWORD)frame.Params[3],
                        p,
                        symInfo->Name,
                        fnOffset));
                }
                else
                {
                    crashDumpLog.WriteStr(FormattedString(TEXT("%08.8I64X %08.8I64X %08.8X %08.8X %08.8X %08.8X %s!0x%I64x\r\n"),
                        frame.AddrStack.Offset,
                        frame.AddrPC.Offset,
                        (DWORD)frame.Params[0],
                        (DWORD)frame.Params[1],
                        (DWORD)frame.Params[2],
                        (DWORD)frame.Params[3],
                        p,
                        frame.AddrPC.Offset
                        ));
                }
#endif

                crashDumpLog.FlushFileBuffers();
            }
        }

        if (hEncodeThread)
        {
            crashDumpLog.WriteStr(TEXT("\r\nEncode thread stack trace:\r\n"));
#ifdef _WIN64
            crashDumpLog.WriteStr(TEXT("Stack            EIP              Arg0             Arg1             Arg2             Arg3             Address\r\n"));
#else
            crashDumpLog.WriteStr(TEXT("Stack    EIP      Arg0     Arg1     Arg2     Arg3     Address\r\n"));
#endif
            crashDumpLog.FlushFileBuffers();

            context.ContextFlags = CONTEXT_ALL;
            GetThreadContext (hEncodeThread, &context);
            ZeroMemory (&frame, sizeof(frame));
#ifdef _WIN64
            InstructionPtr = context.Rip;
            frame.AddrPC.Offset = InstructionPtr;
            frame.AddrFrame.Offset = context.Rbp;
            frame.AddrStack.Offset = context.Rsp;
            imageType = IMAGE_FILE_MACHINE_AMD64;
#else
            InstructionPtr = context.Eip;
            frame.AddrPC.Offset = InstructionPtr;
            frame.AddrFrame.Offset = context.Ebp;
            frame.AddrStack.Offset = context.Esp;
            imageType = IMAGE_FILE_MACHINE_I386;
#endif

            frame.AddrFrame.Mode = AddrModeFlat;
            frame.AddrPC.Mode = AddrModeFlat;
            frame.AddrStack.Mode = AddrModeFlat;
            while (fnStackWalk64 (imageType, hProcess, hEncodeThread, &frame, &context, NULL, (PFUNCTION_TABLE_ACCESS_ROUTINE64)fnSymFunctionTableAccess64, (PGET_MODULE_BASE_ROUTINE64)fnSymGetModuleBase64, NULL))
            {
                scpy (moduleInfo.moduleName, TEXT("<unknown>"));
                moduleInfo.faultAddress = frame.AddrPC.Offset;
                fnEnumerateLoadedModules64 (hProcess, (PENUMLOADED_MODULES_CALLBACK64)EnumerateLoadedModulesProcInfo, (VOID *)&moduleInfo);
                slwr (moduleInfo.moduleName);

                p = srchr (moduleInfo.moduleName, '\\');
                if (p)
                    p++;
                else
                    p = moduleInfo.moduleName;

#ifdef _WIN64
                if (fnSymFromAddr (hProcess, frame.AddrPC.Offset, &fnOffset, symInfo) && !(symInfo->Flags & SYMFLAG_EXPORT))
                {
                    crashDumpLog.WriteStr(FormattedString(TEXT("%016I64X %016I64X %016I64X %016I64X %016I64X %016I64X %s!%s+0x%I64x\r\n"),
                        frame.AddrStack.Offset,
                        frame.AddrPC.Offset,
                        frame.Params[0],
                        frame.Params[1],
                        frame.Params[2],
                        frame.Params[3],
                        p,
                        symInfo->Name,
                        fnOffset));
                }
                else
                {
                    crashDumpLog.WriteStr(FormattedString(TEXT("%016I64X %016I64X %016I64X %016I64X %016I64X %016I64X %s!0x%I64x\r\n"),
                        frame.AddrStack.Offset,
                        frame.AddrPC.Offset,
                        frame.Params[0],
                        frame.Params[1],
                        frame.Params[2],
                        frame.Params[3],
                        p,
                        frame.AddrPC.Offset));
                }
#else
                if (fnSymFromAddr (hProcess, frame.AddrPC.Offset, &fnOffset, symInfo) && !(symInfo->Flags & SYMFLAG_EXPORT))
                {
                    crashDumpLog.WriteStr(FormattedString(TEXT("%08.8I64X %08.8I64X %08.8X %08.8X %08.8X %08.8X %s!%s+0x%I64x\r\n"),
                        frame.AddrStack.Offset,
                        frame.AddrPC.Offset,
                        (DWORD)frame.Params[0],
                        (DWORD)frame.Params[1],
                        (DWORD)frame.Params[2],
                        (DWORD)frame.Params[3],
                        p,
                        symInfo->Name,
                        fnOffset));
                }
                else
                {
                    crashDumpLog.WriteStr(FormattedString(TEXT("%08.8I64X %08.8I64X %08.8X %08.8X %08.8X %08.8X %s!0x%I64x\r\n"),
                        frame.AddrStack.Offset,
                        frame.AddrPC.Offset,
                        (DWORD)frame.Params[0],
                        (DWORD)frame.Params[1],
                        (DWORD)frame.Params[2],
                        (DWORD)frame.Params[3],
                        p,
                        frame.AddrPC.Offset
                        ));
                }
#endif

                crashDumpLog.FlushFileBuffers();
            }
        }

    }

    //generate a minidump if possible
    if (fnMiniDumpWriteDump)
    {
        TCHAR     dumpPath[MAX_PATH];
        HANDLE    hFile;

        tsprintf_s (dumpPath, _countof(dumpPath)-1, TEXT("%s\\crashDumps\\OBSCrashDump%.4d-%.2d-%.2d_%d.dmp"), lpAppDataPath, timeInfo.wYear, timeInfo.wMonth, timeInfo.wDay, i);

        hFile = CreateFile (dumpPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            MINIDUMP_TYPE dumpFlags = (MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithUnloadedModules | MiniDumpWithProcessThreadData);

            miniInfo.ClientPointers = TRUE;
            miniInfo.ExceptionPointers = exceptionInfo;
            miniInfo.ThreadId = GetCurrentThreadId ();

            if (fnMiniDumpWriteDump (hProcess, GetCurrentProcessId(), hFile, dumpFlags, &miniInfo, NULL, NULL))
            {
                crashDumpLog.WriteStr(FormattedString(TEXT("\r\nA minidump was saved to %s.\r\nPlease include this file when posting a crash report.\r\n"), dumpPath));
            }
            else
            {
                CloseHandle (hFile);
                DeleteFile (dumpPath);
            }
        }
    }
    else
    {
        crashDumpLog.WriteStr(TEXT("\r\nA minidump could not be created. Please check dbghelp.dll is present.\r\n"));
    }

    crashDumpLog.WriteStr("\r\nList of loaded modules:\r\n");
#ifdef _WIN64
    crashDumpLog.WriteStr("Base Address                      Module\r\n");
#else
    crashDumpLog.WriteStr("Base Address      Module\r\n");
#endif
    crashDumpLog.WriteStr(strModuleInfo);

    crashDumpLog.Close();

    LocalFree (symInfo);

    fnSymCleanup (hProcess);

    if (OBSMessageBox(hwndMain, TEXT("Woops! OBS has crashed. Would you like to view a crash report?"), NULL, MB_ICONERROR | MB_YESNO) == IDYES)
        ShellExecute(NULL, NULL, logPath, NULL, searchPath, SW_SHOWDEFAULT);

    FreeLibrary (hDbgHelp);

    //we really shouldn't be returning here, if we're at the bottom of the VEH chain this is a pretty legitimate crash
    //and if we return we could end up invoking a second crash handler or other weird / annoying things
    //ExitProcess(exceptionInfo->ExceptionRecord->ExceptionCode);
    return EXCEPTION_CONTINUE_SEARCH;
}
