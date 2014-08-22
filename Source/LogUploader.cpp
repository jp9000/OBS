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

namespace
{
    struct HTTPHandleDeleter
    {
        void operator()(HINTERNET h) { WinHttpCloseHandle(h); }
    };

    struct HTTPHandle : std::unique_ptr<void, HTTPHandleDeleter>
    {
        explicit HTTPHandle(HINTERNET h=nullptr) : unique_ptr(h) {}
        operator HINTERNET() { return get(); }
        bool operator!() { return get() == nullptr; }
    };

    bool HTTPProlog(String url, String &path, HTTPHandle &session, HTTPHandle &connect, bool &secure)
    {
        URL_COMPONENTS  urlComponents;

        String hostName;

        hostName.SetLength(256);
        path.SetLength(1024);

        zero(&urlComponents, sizeof(urlComponents));

        urlComponents.dwStructSize = sizeof(urlComponents);

        urlComponents.lpszHostName = hostName;
        urlComponents.dwHostNameLength = hostName.Length();

        urlComponents.lpszUrlPath = path;
        urlComponents.dwUrlPathLength = path.Length();

        WinHttpCrackUrl(url, 0, 0, &urlComponents);
        if (urlComponents.nPort == INTERNET_DEFAULT_HTTPS_PORT)
            secure = true;
        else
            secure = false;

        session.reset(WinHttpOpen(OBS_VERSION_STRING, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        if (!session)
            return false;

        connect.reset(WinHttpConnect(session, hostName, secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0));

        return !!connect;
    }

    bool HTTPReceiveStatus(HTTPHandle &request, int &status)
    {
        if (!WinHttpReceiveResponse(request, NULL))
            return false;

        TCHAR statusCode[8];
        DWORD statusCodeLen;

        statusCodeLen = sizeof(statusCode);
        if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeLen, WINHTTP_NO_HEADER_INDEX))
            return false;

        status = wcstoul(statusCode, NULL, 10);
        return true;
    }

    bool HTTPPostData(String url, void *data, size_t length, int &response, List<BYTE> *resultBody, String const &headers=String())
    {
        HTTPHandle session, connect;
        bool secure;
        String path;

        if (!HTTPProlog(url, path, session, connect, secure))
            return false;

        HTTPHandle request(WinHttpOpenRequest(connect, L"POST", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0));
        if (!request)
            return false;

        // End the request.
        if (!WinHttpSendRequest(request, headers.Array(), headers.IsEmpty() ? 0 : -1, data, (DWORD)length, (DWORD)length, 0))
            return false;

        if (!HTTPReceiveStatus(request, response))
            return false;

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
        LPSTR str = data.CreateUTF8String();
        bool result = HTTPPostData(url, str, strlen(str), response, resultBody, headers);
        Free(str);
        return result;
    }

    bool HTTPFindRedirect(String url, String &location)
    {
        HTTPHandle session, connect;
        bool secure;
        String path;

        if (!HTTPProlog(url, path, session, connect, secure))
            return false;

        HTTPHandle request(WinHttpOpenRequest(connect, L"HEAD", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0));
        if (!request)
            return false;

        // End the request.
        if (!WinHttpSendRequest(request, nullptr, 0, nullptr, 0, 0, 0))
            return false;

        int status;
        if (!HTTPReceiveStatus(request, status))
            return false;

        location.SetLength(MAX_PATH);
        DWORD length = MAX_PATH;
        if (WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, location.Array(), &length, WINHTTP_NO_HEADER_INDEX))
        {
            location.SetLength(length);
            return true;
        }

        length = MAX_PATH;
        if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM, L"X-Gist-Url", location.Array(), &length, WINHTTP_NO_HEADER_INDEX))
        {
            location.SetLength(length);
            location = FormattedString(L"https://gist.github.com%s", location.Array());
            return true;
        }

        return false;
    }

    String LogFileAge(String &name)
    {
        FILETIME now_ft, log_ft;
        SYSTEMTIME now_st, log_st;

        zero(&log_st, sizeof log_st);

        if (swscanf_s(name.Array(), L"%u-%02u-%02u-%02u%02u-%02u", &log_st.wYear, &log_st.wMonth, &log_st.wDay, &log_st.wHour, &log_st.wMinute, &log_st.wSecond) != 6)
            return String();

        GetLocalTime(&now_st);
        SystemTimeToFileTime(&now_st, &now_ft);
        SystemTimeToFileTime(&log_st, &log_ft);

        ULARGE_INTEGER now_, log_, diff;
        now_.LowPart = now_ft.dwLowDateTime;
        now_.HighPart = now_ft.dwHighDateTime;
        log_.LowPart = log_ft.dwLowDateTime;
        log_.HighPart = log_ft.dwHighDateTime;

        if (now_.QuadPart <= log_.QuadPart)
            return String();

        diff.QuadPart = now_.QuadPart - log_.QuadPart;
        diff.QuadPart /= 10000000;
        diff.QuadPart /= 60;

        if (diff.QuadPart < 1)
            return "less than a minute";
        
        unsigned minutes = diff.QuadPart % 60;
        diff.QuadPart /= 60;
        unsigned hours = diff.QuadPart % 24;
        diff.QuadPart /= 24;
        unsigned days = diff.QuadPart % 7;
        unsigned weeks = (diff.QuadPart/7) % 52;
        diff.QuadPart /= 365; //losing accuracy ...
        unsigned years = (unsigned)diff.QuadPart;

        StringList ages;
#define N_FORMAT(name, num) if (num > 0) ages << FormattedString(L"%u " name L"%s", num, num == 1 ? L"" : L"s")
        N_FORMAT(L"year", years);
        N_FORMAT(L"week", weeks);
        N_FORMAT(L"day", days);
        N_FORMAT(L"hour", hours);
        N_FORMAT(L"minute", minutes);
#undef N_FORMAT
        if (ages.Num() == 1)
            return ages.Last();

        return ages[0] << L" and " << ages[1];
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

        // github api chokes when we submit unescaped %, so encode it and 
        // any other lower and higher characters just to be safe
        String newStr;
        int len = slen(str);

        for (int i = 0; i < len; i++)
        {
            if (str[i] < ' ' || str[i] > 255 || str[i] == '%')
                newStr << FormattedString(L"\\u%04X", (int)str[i]);
            else
                newStr << str[i];
        }

        str = newStr;
    }
}

bool UploadLogGitHub(String filename, String logData, LogUploadResult &result)
{
    String description = FormattedString(OBS_VERSION_STRING L" log file uploaded at %s (local time).", CurrentDateTimeString().Array());
    String age = LogFileAge(filename);
    if (age.IsValid())
        description << FormattedString(L" The log file was approximately %s old at the time it was uploaded.", age.Array());

    StringEscapeJson(description);
    StringEscapeJson(filename);
    StringEscapeJson(logData);

    String json = FormattedString(L"{ \"public\": false, \"description\": \"%s\", \"files\": { \"%s\": { \"content\": \"%s\" } } }",
        description.Array(), filename.Array(), logData.Array());

    int response = 0;
    List<BYTE> body;
    if (!HTTPPostData(String(L"https://api.github.com/gists"), json, response, &body)) {
        result.errors << Str("LogUpload.CommunicationError");
        return false;
    }

    if (response != 201) {
        result.errors << FormattedString(Str("LogUpload.ServiceReturnedError"), response)
                      << FormattedString(Str("LogUpload.ServiceExpectedResponse"), 201);
        return false;
    }

    auto invalid_response = [&]() -> bool { result.errors << Str("LogUpload.ServiceReturnedInvalidResponse"); return false; };

    if (body.Num() < 1)
        return invalid_response();

    //make sure it's null terminated since we run string ops on it below
    body.Add (0);

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

    result.url = bodyStr.Mid((UINT)(pos - bodyStr.Array()), (UINT)(end - bodyStr.Array()));

    if (!HTTPFindRedirect(result.url, result.analyzerURL)) //the basic url doesn't work with the analyzer, so query the fully redirected url
        result.analyzerURL = result.url;

    return true;
}

// Game Capture log is always appended, as requested by Jim (yes, this can result in two game capture logs in one upload)
static void AppendGameCaptureLog(String &data)
{
    String path = FormattedString(L"%s\\captureHookLog.txt", OBSGetPluginDataPath().Array());
    XFile f(path.Array(), XFILE_READ | XFILE_SHARED, XFILE_OPENEXISTING);
    if (!f.IsOpen())
        return;

    String append;
    f.ReadFileToString(append);
    data << L"\r\n\r\nLast Game Capture Log:\r\n" << append;
}

bool UploadCurrentLog(LogUploadResult &result)
{
    String data;
    ReadLog(data);
    if (data.IsEmpty()) {
        result.errors << Str("LogUpload.EmptyLog");
        return false;
    }

    AppendGameCaptureLog(data);

    String filename = CurrentLogFilename();

    return UploadLogGitHub(GetPathFileName(filename.FindReplace(L"\\", L"/").Array(), true), data, result);
}

bool UploadLog(String filename, LogUploadResult &result)
{
    String path = FormattedString(L"%s\\logs\\%s", OBSGetAppDataPath(), filename.Array());
    XFile f(path.Array(), XFILE_READ, XFILE_OPENEXISTING);
    if (!f.IsOpen()) {
        result.errors << FormattedString(Str("LogUpload.CannotOpenFile"), path.Array());
        return false;
    }

    String data;
    f.ReadFileToString(data);
    if (data.IsEmpty()) {
        result.errors << Str("LogUpload.EmptyLog");
        return false;
    }

    AppendGameCaptureLog(data);

    return UploadLogGitHub(filename.Array(), data, result);
}