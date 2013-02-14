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

#ifdef _WIN64
typedef unsigned __int64 UPARAM;
#else
typedef unsigned long UPARAM;
#endif

BOOL WINAPI InjectLibrary(HANDLE hProcess, const wchar_t *pDLL, DWORD dwLen)
{
    DWORD   dwTemp, dwSize, lastError;
    BOOL    bSuccess, bRet = 0;
    HANDLE  hThread = NULL;
    LPVOID  pStr = NULL;
    UPARAM  procAddress;
    SIZE_T  writtenSize;

    if (!hProcess) return 0;

    dwSize = (dwLen+1) * sizeof(wchar_t);

    pStr = (LPVOID)VirtualAllocEx(hProcess, NULL, dwSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!pStr) goto end;

    bSuccess = WriteProcessMemory(hProcess, pStr, (LPVOID)pDLL, dwSize, &writtenSize);
    if (!bSuccess) goto end;

    procAddress = (UPARAM)GetProcAddress(GetModuleHandle(TEXT("KERNEL32")), "LoadLibraryW");
    if (!procAddress) goto end;

    hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)procAddress,
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
        VirtualFreeEx(hProcess, pStr, 0, MEM_RELEASE);

    if (!bRet)
        SetLastError(lastError);

    return bRet;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nShowCmd)
{
    LPWSTR pCommandLineW = GetCommandLineW();
    int retVal = 0;
    DWORD procID = 0;
    int numArgs = 0;

#ifdef _WIN64
    const wchar_t pDLLName[] = L"GraphicsCaptureHook64.dll";
#else
    const wchar_t pDLLName[] = L"GraphicsCaptureHook.dll";
#endif

    /* -------------------------- */

    LPWSTR *pCommandLineArgs = CommandLineToArgvW(pCommandLineW, &numArgs);
    if (numArgs > 1)
    {
        procID = wcstoul(pCommandLineArgs[1], NULL, 10);
        if (procID != 0)
        {
            HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procID);
            if (hProcess)
            {
                UINT dirLen = GetCurrentDirectory(0, 0); /* includes null terminator */
                const UINT fileNameLen = (sizeof(pDLLName) / sizeof(wchar_t))-1;
                UINT len = dirLen + fileNameLen + 1; /* 1 for '/' */
                wchar_t *pPath;

                /* -------------------------- */

                if (dirLen)
                {
                    pPath = (wchar_t*)malloc(len * sizeof(wchar_t));
                    memset(pPath, 0, len * sizeof(wchar_t));

                    GetCurrentDirectoryW(dirLen, pPath);
                    pPath[dirLen-1] = '\\';
                    wcsncpy(pPath+dirLen, pDLLName, fileNameLen);

                    if(!InjectLibrary(hProcess, pPath, len-1))
                    {
                        retVal = GetLastError();
                        if(!retVal)
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
            retVal = -2;
    }
    else
        retVal = -1;

    LocalFree(pCommandLineArgs);

    return retVal;
}
