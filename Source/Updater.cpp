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
#include <Wincrypt.h>
#include <shellapi.h>

HCRYPTPROV hProvider;

VOID HashToString (BYTE *in, TCHAR *out)
{
    const char alphabet[] = "0123456789abcdef";

    for (int i = 0; i != 20; ++i)
    {
        out[2*i]     = alphabet[in[i] / 16];
        out[2*i + 1] = alphabet[in[i] % 16];
    }

    out[40] = 0;
}

VOID GenerateGUID(String &strGUID)
{
    BYTE junk[20];

    if (!CryptGenRandom(hProvider, sizeof(junk), junk))
        return;
    
    strGUID.SetLength(41);
    HashToString(junk, strGUID.Array());
}

BOOL CalculateFileHash (TCHAR *path, BYTE *hash)
{
    HCRYPTHASH hHash;

    if (!CryptCreateHash(hProvider, CALG_SHA1, 0, 0, &hHash))
        return FALSE;

    XFile file;

    if (file.Open(path, XFILE_READ, OPEN_EXISTING))
    {
        BYTE buff[65536];

        for (;;)
        {
            DWORD read = file.Read(buff, sizeof(buff));

            if (!read)
                break;

            if (!CryptHashData(hHash, buff, read, 0))
            {
                CryptDestroyHash(hHash);
                file.Close();
                return FALSE;
            }
        }
    }
    else
    {
        CryptDestroyHash(hHash);
        return FALSE;
    }

    file.Close();

    DWORD hashLength = 20;
    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLength, 0))
        return FALSE;

    CryptDestroyHash(hHash);
    return TRUE;
}

BOOL FetchUpdaterModule()
{
    int responseCode;
    TCHAR updateFilePath[MAX_PATH];
    BYTE updateFileHash[20];
    TCHAR extraHeaders[256];

    tsprintf_s (updateFilePath, _countof(updateFilePath)-1, TEXT("%s\\updates\\updater.exe"), lpAppDataPath);

    if (CalculateFileHash(updateFilePath, updateFileHash))
    {
        TCHAR hashString[41];

        HashToString(updateFileHash, hashString);

        tsprintf_s (extraHeaders, _countof(extraHeaders)-1, TEXT("If-None-Match: %s"), hashString);
    }
    else
        extraHeaders[0] = 0;

    if (HTTPGetFile(TEXT("https://obsproject.com/update/updater.exe"), updateFilePath, extraHeaders, &responseCode))
    {
        if (responseCode != 200 && responseCode != 304)
            return FALSE;
    }

    return TRUE;
}

BOOL IsSafePath (CTSTR path)
{
    const TCHAR *p;

    p = path;

    if (!*p)
        return TRUE;

    if (!isalnum(*p))
        return FALSE;

    while (*p)
    {
        if (*p == '.' || *p == '\\')
            return FALSE;
        p++;
    }
    
    return TRUE;
}

BOOL ParseUpdateManifest (TCHAR *path, BOOL *updatesAvailable, String &description)
{
    XConfig manifest;
    XElement *root;

    if (!manifest.Open(path))
        return FALSE;

    root = manifest.GetRootElement();

    DWORD numPackages = root->NumElements();
    DWORD totalUpdatableFiles = 0;

    int priority, bestPriority = 999;

    for (DWORD i = 0; i < numPackages; i++)
    {
        XElement *package;
        package = root->GetElementByID(i);
        CTSTR packageName = package->GetName();

        //find out if this package is relevant to us
        String platform = package->GetString(TEXT("platform"));
        if (!platform)
            continue;

        if (scmp(platform, TEXT("all")))
        {
#ifndef _WIN64
            if (scmp(platform, TEXT("Win32")))
                continue;
#else
            if (scmp(platform, TEXT("Win64")))
                continue;
#endif
        }

        //what is it?
        String name = package->GetString(TEXT("name"));
        String version = package->GetString(TEXT("version"));

        //figure out where the files belong
        XDataItem *pathElement = package->GetDataItem(TEXT("path"));
        if (!pathElement)
            continue;

        CTSTR path = pathElement->GetData();

        if (path == NULL)
            path = TEXT("");

        if (!IsSafePath(path))
            continue;

        priority = package->GetInt(TEXT("priority"), 999);

        //get the file list for this package
        XElement *files = package->GetElement(TEXT("files"));
        if (!files)
            continue;

        DWORD numFiles = files->NumElements();
        DWORD numUpdatableFiles = 0;
        for (DWORD j = 0; j < numFiles; j++)
        {
            XElement *file = files->GetElementByID(j);

            String hash = file->GetString(TEXT("hash"));
            if (!hash || hash.Length() != 40)
                continue;

            String fileName = file->GetName();
            if (!fileName)
                continue;

            if (!IsSafeFilename(fileName))
                continue;

            String filePath;

            filePath << path;
            filePath << fileName;

            if (OSFileExists(filePath))
            {
                BYTE fileHash[20];
                TCHAR fileHashString[41];

                if (!CalculateFileHash(filePath, fileHash))
                    continue;
                
                HashToString(fileHash, fileHashString);
                if (!scmp(fileHashString, hash))
                    continue;
            }

            numUpdatableFiles++;
        }

        if (numUpdatableFiles)
        {
            if (version.Length())
                description << name << TEXT(" (") << version << TEXT(")\r\n");
            else
                description << name << TEXT("\r\n");

            if (priority < bestPriority)
                bestPriority = priority;
        }

        totalUpdatableFiles += numUpdatableFiles;
        numUpdatableFiles = 0;
    }

    manifest.Close();

    if (totalUpdatableFiles)
    {
        if (!FetchUpdaterModule())
            return FALSE;
    }

    if (bestPriority <= 5)
        *updatesAvailable = TRUE;
    else
        *updatesAvailable = FALSE;

    return TRUE;
}

DWORD WINAPI CheckUpdateThread (VOID *arg)
{
    int responseCode;
    TCHAR extraHeaders[256];
    BYTE manifestHash[20];
    TCHAR manifestPath[MAX_PATH];
    BOOL updatesAvailable;

    BOOL notify = (BOOL)arg;

    tsprintf_s (manifestPath, _countof(manifestPath)-1, TEXT("%s\\updates\\packages.xconfig"), lpAppDataPath);

    if (!CryptAcquireContext(&hProvider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        Log (TEXT("Updater: CryptAcquireContext failed: %08x"), GetLastError());
        return 1;
    }

    extraHeaders[0] = 0;

    if (CalculateFileHash(manifestPath, manifestHash))
    {
        TCHAR hashString[41];

        HashToString(manifestHash, hashString);

        tsprintf_s (extraHeaders, _countof(extraHeaders)-1, TEXT("If-None-Match: %s"), hashString);
    }
    
    //this is an arbitrary random number that we use to count the number of unique OBS installations
    //and is not associated with any kind of identifiable information
    String strGUID = GlobalConfig->GetString(TEXT("General"), TEXT("InstallGUID"));
    if (strGUID.IsEmpty())
    {
        GenerateGUID(strGUID);

        if (strGUID.IsValid())
            GlobalConfig->SetString(TEXT("General"), TEXT("InstallGUID"), strGUID);
    }

    if (strGUID.IsValid())
    {
        if (extraHeaders[0])
            scat(extraHeaders, TEXT("\n"));

        scat(extraHeaders, TEXT("X-OBS-GUID: "));
        scat(extraHeaders, strGUID);
    }

    if (HTTPGetFile(TEXT("https://obsproject.com/update/packages.xconfig"), manifestPath, extraHeaders, &responseCode))
    {
        if (responseCode == 200 || responseCode == 304)
        {
            String updateInfo;

            updateInfo = Str("Updater.NewUpdates");

            if (ParseUpdateManifest(manifestPath, &updatesAvailable, updateInfo))
            {
                if (updatesAvailable)
                {
                    updateInfo << TEXT("\r\n") << Str("Updater.DownloadNow");

                    if (MessageBox (NULL, updateInfo.Array(), Str("Updater.UpdatesAvailable"), MB_ICONQUESTION|MB_YESNO) == IDYES)
                    {
                        if (App->IsRunning())
                        {
                            if (MessageBox (NULL, Str("Updater.RunningWarning"), NULL, MB_ICONEXCLAMATION|MB_YESNO) == IDNO)
                                goto abortUpdate;
                        }

                        TCHAR updateFilePath[MAX_PATH];
                        TCHAR cwd[MAX_PATH];

                        GetModuleFileName(NULL, cwd, _countof(cwd)-1);
                        TCHAR *p = srchr(cwd, '\\');
                        if (p)
                            *p = 0;

                        tsprintf_s (updateFilePath, _countof(updateFilePath)-1, TEXT("%s\\updates\\updater.exe"), lpAppDataPath);

                        //note, can't use CreateProcess to launch as admin.
                        SHELLEXECUTEINFO execInfo;

                        zero(&execInfo, sizeof(execInfo));

                        execInfo.cbSize = sizeof(execInfo);
                        execInfo.lpFile = updateFilePath;
#ifndef _WIN64
                        if (bIsPortable)
                            execInfo.lpParameters = TEXT("Win32 Portable");
                        else
                            execInfo.lpParameters = TEXT("Win32");
#else
                        if (bIsPortable)
                            execInfo.lpParameters = TEXT("Win64 Portable");
                        else
                            execInfo.lpParameters = TEXT("Win64");
#endif
                        execInfo.lpDirectory = cwd;
                        execInfo.nShow = SW_SHOWNORMAL;

                        if (!ShellExecuteEx (&execInfo))
                        {
                            AppWarning(TEXT("Can't launch updater '%s': %d"), updateFilePath, GetLastError());
                            goto abortUpdate;
                        }

                        //force OBS to perform another update check immediately after updating in case of issues
                        //with the new version
                        GlobalConfig->SetInt(TEXT("General"), OBS_CONFIG_UPDATE_KEY, 0);

                        //since we're in a separate thread we can't just PostQuitMessage ourselves
                        SendMessage(hwndMain, WM_CLOSE, 0, 0);
                    }
                }
            }
        }
    }

    if (notify && !updatesAvailable)
        MessageBox (hwndMain, Str("Updater.NoUpdatesAvailable"), NULL, MB_ICONINFORMATION);

abortUpdate:

    CryptReleaseContext(hProvider, 0);

    return 0;
}