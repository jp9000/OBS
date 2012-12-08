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
#include <winhttp.h>

DWORD WINAPI CheckUpdateThread(VOID *arg)
{

    return 0;
}

BOOL FetchUpdaterModule()
{
    TCHAR updateFilePath[MAX_PATH];
    BOOL ret = FALSE;

    HINTERNET hSession = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;

    const TCHAR *acceptTypes[] = {
        TEXT("application/octet-stream"),
        NULL
    };

    hSession = WinHttpOpen(OBS_VERSION_STRING, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
        goto failure;

    hConnect = WinHttpConnect(hSession, TEXT("obsproject.com"), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect)
        goto failure;

    hRequest = WinHttpOpenRequest(hConnect, TEXT("GET"), TEXT("/update/updater.exe"), NULL, WINHTTP_NO_REFERER, acceptTypes, WINHTTP_FLAG_SECURE|WINHTTP_FLAG_REFRESH);
    if (!hRequest)
        goto failure;

    BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

    // End the request.
    if (bResults)
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    else
        goto failure;

    TCHAR statusCode[8];
    DWORD statusCodeLen;

    statusCodeLen = sizeof(statusCode);
    if (!WinHttpQueryHeaders (hRequest, WINHTTP_QUERY_STATUS_CODE, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeLen, WINHTTP_NO_HEADER_INDEX))
        goto failure;

    if (!scmp(statusCode, TEXT("200")))
        goto failure;

    if (bResults)
    {
        BYTE buffer[16384];
        DWORD dwSize, dwOutSize;

        XFile updateFile;
        
        tsprintf_s (updateFilePath, _countof(updateFilePath)-1, TEXT("%s\\updates\updater.exe"), lpAppDataPath);

        if (!updateFile.Open(updateFilePath, XFILE_WRITE, CREATE_ALWAYS))
            goto failure;

        do 
        {
            // Check for available data.
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
                goto failure;

            if (!WinHttpReadData(hRequest, (LPVOID)buffer, dwSize, &dwOutSize))
            {
                goto failure;
            }
            else
            {
                if (!updateFile.Write(buffer, dwOutSize))
                    goto failure;
            }
        } while (dwSize > 0);

        updateFile.Close();
    }

    ret = TRUE;

failure:
    if (hSession)
        CloseHandle(hSession);
    if (hConnect)
        CloseHandle(hConnect);
    if (hRequest)
        CloseHandle(hRequest);

    return ret;
}