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

/* required defines for archive based updating:
#define MANIFEST_WITH_ARCHIVES 1
#define MANIFEST_PATH "/updates/org.catchexception.builds.xconfig"
#define MANIFEST_URL "https://builds.catchexception.org/update.json"
#define UPDATER_PATH "/updates/org.catchexception.builds.updater.exe"
#define UPDATE_CHANNEL "master"
*/

#ifndef MANIFEST_WITH_ARCHIVES
#define MANIFEST_WITH_ARCHIVES 0
#endif

#ifndef MANIFEST_PATH
#define MANIFEST_PATH "\\updates\\packages.xconfig"
#endif

#ifndef MANIFEST_URL
#define MANIFEST_URL "https://obsproject.com/update/packages.xconfig"
#endif

#ifndef UPDATER_PATH
#define UPDATER_PATH "\\updates\\updater.exe"
#endif

BOOL FetchUpdaterModule(String const &url, String const &hash=String())
{
    int responseCode;
    TCHAR updateFilePath[MAX_PATH];
    BYTE updateFileHash[20];
    TCHAR extraHeaders[256];

    tsprintf_s (updateFilePath, _countof(updateFilePath)-1, TEXT("%s") TEXT(UPDATER_PATH), lpAppDataPath);

    if (CalculateFileHash(updateFilePath, updateFileHash))
    {
        TCHAR hashString[41];

        HashToString(updateFileHash, hashString);

        if (hash.Compare(hashString))
            return true;

        tsprintf_s (extraHeaders, _countof(extraHeaders)-1, TEXT("If-None-Match: %s"), hashString);
    }
    else
        extraHeaders[0] = 0;

    if (HTTPGetFile(url, updateFilePath, extraHeaders, &responseCode))
    {
        if (responseCode != 200 && responseCode != 304)
            return FALSE;

#if MANIFEST_WITH_ARCHIVES
        if (!CalculateFileHash(updateFilePath, updateFileHash))
            return false;

        TCHAR hashString[41];

        HashToString(updateFileHash, hashString);

        if (!hash.Compare(hashString))
            return false;
#endif
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

#if MANIFEST_WITH_ARCHIVES
bool ParseUpdateArchiveManifest(TCHAR *path, BOOL *updatesAvailable, String &description)
{
    XConfig manifest;

	*updatesAvailable = FALSE;

    if (!manifest.Open(path))
        return false;

    XElement *root = manifest.GetRootElement();

    XElement *updater = root->GetElement(L"updater");
    if (!updater)
        return false;

    String updaterURL = updater->GetString(L"url");
    if (updaterURL.IsEmpty())
        return false;

    String updaterHash = updater->GetString(L"sha1");

    XElement *channel = root->GetElement(TEXT(UPDATE_CHANNEL));
    if (!channel)
        return false;

    XElement *platform = channel->GetElement(
#ifdef _WIN64
        L"win64"
#else
        L"win"
#endif
        );
    if (!platform)
        return false;
        
    String version = platform->GetString(TEXT("version"));
    if (version.Compare(TEXT(OBS_VERSION_SUFFIX)))
        return true;

    String url = platform->GetString(L"url");
    if (url.IsEmpty())
        return false;

    description << "Open Broadcaster Software" << version << L"\n";
    
    if (!FetchUpdaterModule(updaterURL, updaterHash))
        return false;

	*updatesAvailable = TRUE;
    return true;
}
#endif

bool ParseUpdateManifest (TCHAR *path, BOOL *updatesAvailable, String &description)
{
#if MANIFEST_WITH_ARCHIVES
    return ParseUpdateArchiveManifest(path, updatesAvailable, description);
#else

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
        if (!FetchUpdaterModule(L"https://obsproject.com/update/updater.exe"))
            return FALSE;
    }

    if (bestPriority <= 5)
        *updatesAvailable = TRUE;
    else
        *updatesAvailable = FALSE;

    return TRUE;
#endif
}

DWORD WINAPI CheckUpdateThread (VOID *arg)
{
    int responseCode;
    TCHAR extraHeaders[256];
    BYTE manifestHash[20];
    TCHAR manifestPath[MAX_PATH];
    BOOL updatesAvailable = false;

    BOOL notify = (BOOL)arg;

    tsprintf_s (manifestPath, _countof(manifestPath)-1, TEXT("%s") TEXT(MANIFEST_PATH), lpAppDataPath);

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

    if (HTTPGetFile(TEXT(MANIFEST_URL), manifestPath, extraHeaders, &responseCode))
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

                    if (OBSMessageBox (NULL, updateInfo.Array(), Str("Updater.UpdatesAvailable"), MB_ICONQUESTION|MB_YESNO) == IDYES)
                    {
                        if (App->IsRunning())
                        {
                            if (OBSMessageBox (NULL, Str("Updater.RunningWarning"), NULL, MB_ICONEXCLAMATION|MB_YESNO) == IDNO)
                                goto abortUpdate;
                        }

                        TCHAR updateFilePath[MAX_PATH];
                        TCHAR cwd[MAX_PATH];

                        GetModuleFileName(NULL, cwd, _countof(cwd)-1);
                        TCHAR *p = srchr(cwd, '\\');
                        if (p)
                            *p = 0;

                        tsprintf_s (updateFilePath, _countof(updateFilePath)-1, TEXT("%s") TEXT(UPDATER_PATH), lpAppDataPath);

                        //note, can't use CreateProcess to launch as admin.
                        SHELLEXECUTEINFO execInfo;

                        zero(&execInfo, sizeof(execInfo));

                        execInfo.cbSize = sizeof(execInfo);
                        execInfo.lpFile = updateFilePath;
#ifndef UPDATE_CHANNEL
#define UPDATE_ARG_SUFFIX L""
#else
#define UPDATE_ARG_SUFFIX L" " TEXT(UPDATE_CHANNEL)
#endif
#ifndef _WIN64
                        if (bIsPortable)
                            execInfo.lpParameters = L"Win32" UPDATE_ARG_SUFFIX L" Portable";
                        else
                            execInfo.lpParameters = L"Win32" UPDATE_ARG_SUFFIX;
#else
                        if (bIsPortable)
                            execInfo.lpParameters = L"Win64" UPDATE_ARG_SUFFIX L" Portable";
                        else
                            execInfo.lpParameters = L"Win64" UPDATE_ARG_SUFFIX;
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
        OBSMessageBox (hwndMain, Str("Updater.NoUpdatesAvailable"), NULL, MB_ICONINFORMATION);

abortUpdate:

    CryptReleaseContext(hProvider, 0);

    return 0;
}