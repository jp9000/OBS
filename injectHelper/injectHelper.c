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


#include <stdio.h>
#include <windows.h>
#include <shellapi.h>
#include <stdbool.h>
#include <math.h>

#ifdef _WIN64
typedef unsigned __int64 UPARAM;
#else
typedef unsigned long UPARAM;
#endif

BOOL LoadSeDebugPrivilege()
{
    DWORD   err;
    HANDLE  hToken;
    LUID    Val;
    TOKEN_PRIVILEGES tp;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
        err = GetLastError();
        return FALSE;
    }

    if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &Val))
    {
        err = GetLastError();
        CloseHandle(hToken);
        return FALSE;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = Val;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof (tp), NULL, NULL))
    {
        err = GetLastError();
        CloseHandle(hToken);
        return FALSE;
    }

    CloseHandle(hToken);

    return TRUE;
}

typedef HHOOK (WINAPI *SWHEXPROC)(int, HOOKPROC, HINSTANCE, DWORD);

BOOL WINAPI InjectLibrarySafe(DWORD threadID, const wchar_t *pDLL, DWORD dwLen)
{
    HMODULE hLib = LoadLibraryW(pDLL);
    char pSWHEXStr[18];
    SWHEXPROC setWindowsHookEx;
    LPVOID proc;
    HHOOK hook;
    HMODULE hU32;
    int i;

    if (!hLib)
        return FALSE;

    memcpy(pSWHEXStr, "RewUljci~{CebgJvF", 18);         //SetWindowsHookEx with each character obfuscated

#ifdef _WIN64
    proc = GetProcAddress(hLib, "DummyDebugProc");
#else
    proc = GetProcAddress(hLib, "_DummyDebugProc@12");
#endif
    if (!proc)
    {
        FreeLibrary(hLib);
        return FALSE;
    }

    for (i = 0; i < 17; i++) pSWHEXStr[i] ^= i ^ 1;

    hU32 = GetModuleHandle(TEXT("USER32"));
    setWindowsHookEx = (SWHEXPROC)GetProcAddress(hU32, pSWHEXStr);

    /* this is terrible. */
    hook = setWindowsHookEx(WH_GETMESSAGE, (HOOKPROC)proc, hLib, threadID);
    if (!hook)
        return FALSE;

    for (int i = 0; i < 20; i++)
        PostThreadMessage(threadID, WM_USER + 432, 0, (LPARAM)hook);
    Sleep(1000);
    for (int i = 0; i < 20; i++)
        PostThreadMessage(threadID, WM_USER + 432, 0, (LPARAM)hook);
    Sleep(1000);

    FreeLibrary(hLib);
    return TRUE;
}

typedef HANDLE(WINAPI *CRTPROC)(HANDLE, LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
typedef BOOL(WINAPI *WPMPROC)(HANDLE, LPVOID, LPCVOID, SIZE_T, SIZE_T*);
typedef LPVOID(WINAPI *VAEPROC)(HANDLE, LPVOID, SIZE_T, DWORD, DWORD);
typedef BOOL(WINAPI *VFEPROC)(HANDLE, LPVOID, SIZE_T, DWORD);
typedef HANDLE(WINAPI *OPPROC) (DWORD, BOOL, DWORD);

BOOL WINAPI InjectLibrary(HANDLE hProcess, const wchar_t *pDLL, DWORD dwLen)
{
    DWORD   dwTemp, dwSize, lastError;
    BOOL    bSuccess, bRet = 0;
    HANDLE  hThread = NULL;
    LPVOID  pStr = NULL;
    UPARAM  procAddress;
    SIZE_T  writtenSize;

    WPMPROC pWriteProcessMemory;
    CRTPROC pCreateRemoteThread;
    VAEPROC pVirtualAllocEx;
    VFEPROC pVirtualFreeEx;
    HMODULE hK32;
    char pWPMStr[19], pCRTStr[19], pVAEStr[15], pVFEStr[14], pLLStr[13];
    int obfSize = 12;
    int i;

    /*--------------------------------------------------------*/

    if (!hProcess) return 0;

    dwSize = (dwLen + 1) * sizeof(wchar_t);

    /*--------------------------------------------------------*/


    memcpy(pWPMStr, "RvnrdPqmni|}Dmfegm", 19); //WriteProcessMemory with each character obfuscated
    memcpy(pCRTStr, "FvbgueQg`c{k]`yotp", 19); //CreateRemoteThread with each character obfuscated
    memcpy(pVAEStr, "WiqvpekGeddiHt", 15);     //VirtualAllocEx with each character obfuscated
    memcpy(pVFEStr, "Wiqvpek@{mnOu", 14);      //VirtualFreeEx with each character obfuscated
    memcpy(pLLStr, "MobfImethzr", 12);         //LoadLibrary with each character obfuscated

#ifdef UNICODE
    pLLStr[11] = 'W';
#else
    pLLStr[11] = 'A';
#endif
    pLLStr[12] = 0;

    obfSize += 6;
    for (i = 0; i<obfSize; i++) pWPMStr[i] ^= i ^ 5;
    for (i = 0; i<obfSize; i++) pCRTStr[i] ^= i ^ 5;

    obfSize -= 4;
    for (i = 0; i<obfSize; i++) pVAEStr[i] ^= i ^ 1;

    obfSize -= 1;
    for (i = 0; i<obfSize; i++) pVFEStr[i] ^= i ^ 1;

    obfSize -= 2;
    for (i = 0; i<obfSize; i++) pLLStr[i] ^= i ^ 1;

    hK32 = GetModuleHandle(TEXT("KERNEL32"));
    pWriteProcessMemory = (WPMPROC)GetProcAddress(hK32, pWPMStr);
    pCreateRemoteThread = (CRTPROC)GetProcAddress(hK32, pCRTStr);
    pVirtualAllocEx = (VAEPROC)GetProcAddress(hK32, pVAEStr);
    pVirtualFreeEx = (VFEPROC)GetProcAddress(hK32, pVFEStr);

    /*--------------------------------------------------------*/

    pStr = (LPVOID)(*pVirtualAllocEx)(hProcess, NULL, dwSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!pStr) goto end;

    bSuccess = (*pWriteProcessMemory)(hProcess, pStr, (LPVOID)pDLL, dwSize, &writtenSize);
    if (!bSuccess) goto end;

    procAddress = (UPARAM)GetProcAddress(hK32, pLLStr);
    if (!procAddress) goto end;

    hThread = (*pCreateRemoteThread)(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)procAddress,
        pStr, 0, &dwTemp);
    if (!hThread) goto end;

    if (WaitForSingleObject(hThread, 200) == WAIT_OBJECT_0)
    {
        DWORD dw;
        GetExitCodeThread(hThread, &dw);
        bRet = dw != 0;

        SetLastError(0);
    }

end:
    if (!bRet)
        lastError = GetLastError();

    if (hThread)
        CloseHandle(hThread);

    if (pStr)
        (*pVirtualFreeEx)(hProcess, pStr, 0, MEM_RELEASE);

    if (!bRet)
        SetLastError(lastError);

    return bRet;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nShowCmd)
{
    LPWSTR pCommandLineW = GetCommandLineW();
    int retVal = 0;
    DWORD id = 0;
    int numArgs = 0;
    bool safe = false;

#ifdef _WIN64
    const wchar_t pDLLName[] = L"GraphicsCaptureHook64.dll";
#else
    const wchar_t pDLLName[] = L"GraphicsCaptureHook.dll";
#endif

#if defined _M_X64 && _MSC_VER == 1800
    //workaround AVX2 bug in VS2013, http://connect.microsoft.com/VisualStudio/feedback/details/811093
    _set_FMA3_enable(0);
#endif

    /* -------------------------- */

    LPWSTR *pCommandLineArgs = CommandLineToArgvW(pCommandLineW, &numArgs);

    LoadSeDebugPrivilege();

    if (numArgs > 2)
    {
        safe = *pCommandLineArgs[2] == '1';
        id = wcstoul(pCommandLineArgs[1], NULL, 10);
        if (id != 0)
        {
            if (!safe)
            {
                OPPROC pOpenProcess;
                HANDLE hProcess;
                char pOPStr[12];
                int i;

                memcpy(pOPStr, "NpflUvhel{x", 12); //OpenProcess obfuscated
                for (i = 0; i<11; i++) pOPStr[i] ^= i ^ 1;

                pOpenProcess = (OPPROC)GetProcAddress(GetModuleHandle(TEXT("KERNEL32")), pOPStr);

                hProcess = (*pOpenProcess)(PROCESS_ALL_ACCESS, FALSE, id);
                if (hProcess)
                {
                    UINT dirLen = GetCurrentDirectory(0, 0); /* includes null terminator */
                    const UINT fileNameLen = (sizeof(pDLLName) / sizeof(wchar_t)) - 1;
                    UINT len = dirLen + fileNameLen + 1; /* 1 for '/' */
                    wchar_t *pPath;

                    /* -------------------------- */

                    if (dirLen)
                    {
                        pPath = (wchar_t*)malloc(len * sizeof(wchar_t));
                        memset(pPath, 0, len * sizeof(wchar_t));

                        GetCurrentDirectoryW(dirLen, pPath);
                        pPath[dirLen - 1] = '\\';
                        wcsncpy_s(pPath + dirLen, len - dirLen, pDLLName, fileNameLen);

                        if (!InjectLibrary(hProcess, pPath, len - 1))
                        {
                            retVal = GetLastError();
                            if (!retVal)
                                retVal = -5;
                        }

                        free(pPath);
                    }
                    else
                        retVal = -4;

                    CloseHandle(hProcess);
                }
                else
                    retVal = -3;
            }
            else
            {
                UINT dirLen = GetCurrentDirectory(0, 0); /* includes null terminator */
                const UINT fileNameLen = (sizeof(pDLLName) / sizeof(wchar_t)) - 1;
                UINT len = dirLen + fileNameLen + 1; /* 1 for '/' */
                wchar_t *pPath;

                /* -------------------------- */

                if (dirLen)
                {
                    pPath = (wchar_t*)malloc(len * sizeof(wchar_t));
                    memset(pPath, 0, len * sizeof(wchar_t));

                    GetCurrentDirectoryW(dirLen, pPath);
                    pPath[dirLen - 1] = '\\';
                    wcsncpy_s(pPath + dirLen, len - dirLen, pDLLName, fileNameLen);

                    if (!InjectLibrarySafe(id, pPath, len - 1))
                    {
                        retVal = GetLastError();
                        if (!retVal)
                            retVal = -7;
                    }

                    free(pPath);
                }
                else
                    retVal = -6;
            }
        }
        else
            retVal = -2;
    }
    else
        retVal = -1;

    LocalFree(pCommandLineArgs);

    return retVal;
}
