/********************************************************************************
 Copyright (C) 2014 Ruwen Hahn <palana@stunned.de>

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
#include "LogUploader.h"
#include "HTTPClient.h"

#include <winhttp.h>

#include <map>

#include <memory>

using namespace std;

namespace
{
    struct HTTPHandleDeleter
    {
        void operator()(HINTERNET h) { WinHttpCloseHandle(h); }
    };

    struct HTTPHandle : unique_ptr<void, HTTPHandleDeleter>
    {
        explicit HTTPHandle(HINTERNET h) : unique_ptr(h) {}
        operator HINTERNET() { return get(); }
        bool operator!() { return get() == nullptr; }
    };

    bool HTTPPostData(String url, void *data, size_t length, int &response, List<BYTE> *resultBody, String const &headers=String())
    {
        URL_COMPONENTS  urlComponents;

        String hostName, path;

        const TCHAR *acceptTypes[] = {
            TEXT("*/*"),
            NULL
        };

        hostName.SetLength(256);
        path.SetLength(1024);

        zero(&urlComponents, sizeof(urlComponents));

        urlComponents.dwStructSize = sizeof(urlComponents);

        urlComponents.lpszHostName = hostName;
        urlComponents.dwHostNameLength = hostName.Length();

        urlComponents.lpszUrlPath = path;
        urlComponents.dwUrlPathLength = path.Length();

        WinHttpCrackUrl(url, 0, 0, &urlComponents);

        bool secure = false;
        if (urlComponents.nPort == INTERNET_DEFAULT_HTTPS_PORT)
            secure = true;

        HTTPHandle session(WinHttpOpen(OBS_VERSION_STRING, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        if (!session)
            return false;

        HTTPHandle connect(WinHttpConnect(session, hostName, secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0));
        if (!connect)
            return false;

        HTTPHandle request(WinHttpOpenRequest(connect, L"POST", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0));
        if (!request)
            return false;

        // End the request.
        if (!WinHttpSendRequest(request, headers.Array(), headers.IsEmpty() ? 0 : -1, data, (DWORD)length, (DWORD)length, 0))
            return false;

        if (!WinHttpReceiveResponse(request, NULL))
            return false;

        TCHAR statusCode[8];
        DWORD statusCodeLen;

        statusCodeLen = sizeof(statusCode);
        if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeLen, WINHTTP_NO_HEADER_INDEX))
            return false;

        response = wcstoul(statusCode, NULL, 10);

        if (!resultBody)
            return true;

        DWORD size = 0, read = 0, offset = 0;
        do
        {
            if (!WinHttpQueryDataAvailable(request, &size) || size > 16386)
                return false;

            resultBody->SetSize(size + resultBody->Num());

            if (!WinHttpReadData(request, (LPVOID)(resultBody->Array() + offset), size, &read))
                return false;
            offset += read;
        } while (size && read);

        return true;
    }

    bool HTTPPostData(String url, String data, int &response, List<BYTE> *resultBody=nullptr, String const &headers=String())
    {
        OutputDebugString(data.Array());
        LPSTR str = data.CreateUTF8String();
        bool result = HTTPPostData(url, str, strlen(str), response, resultBody, headers);
        Free(str);
        return result;
    }

    void StringEscapeJson(String &str)
    {
        str.FindReplace(L"\\", L"\\\\");
        str.FindReplace(L"\"", L"\\\"");
        str.FindReplace(L"\n", L"\\n");
        str.FindReplace(L"\r", L"\\r");
        str.FindReplace(L"\f", L"\\f");
        str.FindReplace(L"\b", L"\\b");
        str.FindReplace(L"\t", L"\\t");
        str.FindReplace(L"/", L"\\/");
    }
}

bool UploadLogGitHub(String filename, String logData, String &result)
{
    StringEscapeJson(filename);
    StringEscapeJson(logData);
    String json = FormattedString(L"{ \"public\": false, \"description\": \"%s log file uploaded at %s (local time)\", \"files\": { \"%s\": { \"content\": \"%s\" } } }",
        OBS_VERSION_STRING, CurrentDateTimeString().Array(), filename.Array(), logData.Array());

    int response = 0;
    List<BYTE> body;
    if (!HTTPPostData(String(L"https://api.github.com/gists"), json, response, &body)) {
        result << Str("LogUpload.CommunicationError");
        return false;
    }

    if (response != 201) {
        result << FormattedString(Str("LogUpload.ServiceReturnedError"), response)
               << FormattedString(Str("LogUpload.ServiceExpectedResponse"), 201);
        return false;
    }

    auto invalid_response = [&]() -> bool { result << Str("LogUpload.ServiceReturnedInvalidResponse"); return false; };

    if (body.Num() < 1)
        return invalid_response();

    TSTR wideBody = utf8_createTstr((char const*)body.Array());
    String bodyStr(wideBody);
    Free(wideBody);

    TSTR pos = sstr(bodyStr.Array(), L"\"html_url\"");
    if (!pos)
        return invalid_response();

    pos = schr(pos + slen(L"\"html_url\""), '"');
    if (!pos)
        return invalid_response();

    pos += 1;

    TSTR end = schr(pos, '"');
    if (!end)
        return invalid_response();

    if ((end - pos) < 4)
        return invalid_response();

    result = bodyStr.Mid((UINT)(pos - bodyStr.Array()), (UINT)(end - bodyStr.Array()));
    return true;
}

// Game Capture log is always appended, as requested by Jim (yes, this can result in two game capture logs in one upload)
static void AppendGameCaptureLog(String &data)
{
    String path = FormattedString(L"%s\\captureHookLog.txt", OBSGetPluginDataPath().Array());
    XFile f(path.Array(), XFILE_READ, XFILE_OPENEXISTING);
    if (!f.IsOpen())
        return;

    String append;
    f.ReadFileToString(append);
    data << L"\r\n\r\nLast Game Capture Log:\r\n" << append;
}

bool UploadCurrentLog(String &result)
{
    String data;
    ReadLog(data);
    if (data.IsEmpty()) {
        result << Str("LogUpload.EmptyLog");
        return false;
    }

    AppendGameCaptureLog(data);

    String filename = CurrentLogFilename();

    return UploadLogGitHub(GetPathFileName(filename.FindReplace(L"\\", L"/").Array(), true), data, result);
}

bool UploadLog(String filename, String &result)
{
    String path = FormattedString(L"%s\\logs\\%s", OBSGetAppDataPath(), filename.Array());
    XFile f(path.Array(), XFILE_READ, XFILE_OPENEXISTING);
    if (!f.IsOpen()) {
        result << FormattedString(Str("LogUpload.CannotOpenFile"), path.Array());
        return false;
    }

    String data;
    f.ReadFileToString(data);
    if (data.IsEmpty()) {
        result << Str("LogUpload.EmptyLog");
        return false;
    }

    AppendGameCaptureLog(data);

    return UploadLogGitHub(GetPathFileName(filename.FindReplace(L"\\", L"/").Array(), true), data, result);
}