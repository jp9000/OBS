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

#pragma pack(push, r1, 1)

typedef struct {
    BLOBHEADER blobheader;
    RSAPUBKEY rsapubkey;
} PUBLICKEYHEADER;

#pragma pack(pop, r1)

// Hard coded 4096 bit RSA public key for obsproject.com in PEM format
const unsigned char obs_pub[] = {
    0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x42, 0x45, 0x47, 0x49, 0x4e, 0x20, 0x50,
    0x55, 0x42, 0x4c, 0x49, 0x43, 0x20, 0x4b, 0x45, 0x59, 0x2d, 0x2d, 0x2d,
    0x2d, 0x2d, 0x0a, 0x4d, 0x49, 0x49, 0x43, 0x49, 0x6a, 0x41, 0x4e, 0x42,
    0x67, 0x6b, 0x71, 0x68, 0x6b, 0x69, 0x47, 0x39, 0x77, 0x30, 0x42, 0x41,
    0x51, 0x45, 0x46, 0x41, 0x41, 0x4f, 0x43, 0x41, 0x67, 0x38, 0x41, 0x4d,
    0x49, 0x49, 0x43, 0x43, 0x67, 0x4b, 0x43, 0x41, 0x67, 0x45, 0x41, 0x70,
    0x63, 0x51, 0x4a, 0x33, 0x57, 0x44, 0x62, 0x43, 0x62, 0x66, 0x6e, 0x4d,
    0x75, 0x48, 0x4b, 0x50, 0x77, 0x53, 0x5a, 0x0a, 0x56, 0x36, 0x77, 0x49,
    0x2f, 0x67, 0x36, 0x58, 0x50, 0x65, 0x46, 0x36, 0x62, 0x4c, 0x62, 0x72,
    0x53, 0x66, 0x71, 0x39, 0x66, 0x53, 0x4e, 0x68, 0x61, 0x47, 0x50, 0x37,
    0x55, 0x6e, 0x67, 0x4e, 0x2b, 0x44, 0x46, 0x66, 0x53, 0x36, 0x30, 0x62,
    0x35, 0x31, 0x42, 0x2f, 0x58, 0x6e, 0x6a, 0x67, 0x37, 0x51, 0x38, 0x70,
    0x35, 0x77, 0x4f, 0x57, 0x34, 0x39, 0x44, 0x46, 0x65, 0x62, 0x33, 0x73,
    0x0a, 0x75, 0x65, 0x41, 0x2b, 0x49, 0x42, 0x50, 0x77, 0x6d, 0x71, 0x62,
    0x32, 0x46, 0x39, 0x4a, 0x7a, 0x63, 0x71, 0x67, 0x73, 0x6a, 0x74, 0x46,
    0x76, 0x63, 0x37, 0x74, 0x4b, 0x4a, 0x6c, 0x70, 0x66, 0x59, 0x6b, 0x42,
    0x39, 0x71, 0x67, 0x47, 0x2b, 0x48, 0x4a, 0x54, 0x30, 0x53, 0x50, 0x69,
    0x64, 0x66, 0x53, 0x69, 0x63, 0x51, 0x32, 0x6e, 0x79, 0x35, 0x67, 0x6d,
    0x37, 0x69, 0x4d, 0x6f, 0x71, 0x0a, 0x35, 0x34, 0x36, 0x6a, 0x69, 0x53,
    0x75, 0x41, 0x75, 0x41, 0x63, 0x49, 0x71, 0x74, 0x33, 0x39, 0x37, 0x67,
    0x41, 0x47, 0x56, 0x6f, 0x45, 0x46, 0x58, 0x30, 0x58, 0x47, 0x46, 0x41,
    0x4c, 0x64, 0x63, 0x38, 0x35, 0x35, 0x2f, 0x34, 0x46, 0x74, 0x61, 0x63,
    0x68, 0x79, 0x39, 0x70, 0x59, 0x34, 0x77, 0x58, 0x39, 0x41, 0x59, 0x57,
    0x69, 0x50, 0x42, 0x4e, 0x48, 0x52, 0x65, 0x78, 0x45, 0x51, 0x0a, 0x54,
    0x46, 0x75, 0x55, 0x73, 0x69, 0x76, 0x44, 0x64, 0x51, 0x6c, 0x50, 0x64,
    0x76, 0x6a, 0x79, 0x59, 0x6e, 0x72, 0x63, 0x30, 0x4c, 0x50, 0x2f, 0x4f,
    0x71, 0x4a, 0x2f, 0x71, 0x2f, 0x42, 0x58, 0x7a, 0x4f, 0x4b, 0x66, 0x33,
    0x69, 0x4e, 0x6b, 0x58, 0x70, 0x72, 0x63, 0x68, 0x50, 0x61, 0x67, 0x52,
    0x72, 0x61, 0x43, 0x34, 0x64, 0x41, 0x74, 0x34, 0x62, 0x42, 0x4a, 0x54,
    0x34, 0x43, 0x7a, 0x0a, 0x56, 0x6a, 0x66, 0x55, 0x33, 0x69, 0x53, 0x2f,
    0x57, 0x74, 0x68, 0x7a, 0x54, 0x50, 0x49, 0x51, 0x34, 0x4b, 0x34, 0x4c,
    0x47, 0x57, 0x35, 0x4f, 0x70, 0x4e, 0x2b, 0x46, 0x35, 0x70, 0x67, 0x6d,
    0x6f, 0x69, 0x44, 0x4e, 0x30, 0x64, 0x36, 0x4a, 0x35, 0x43, 0x49, 0x47,
    0x6d, 0x37, 0x62, 0x5a, 0x78, 0x50, 0x58, 0x61, 0x32, 0x6a, 0x73, 0x64,
    0x52, 0x30, 0x6c, 0x31, 0x6a, 0x4d, 0x6a, 0x48, 0x0a, 0x6a, 0x53, 0x52,
    0x74, 0x68, 0x73, 0x59, 0x33, 0x62, 0x33, 0x31, 0x75, 0x73, 0x5a, 0x34,
    0x43, 0x64, 0x6b, 0x6a, 0x48, 0x4f, 0x68, 0x64, 0x76, 0x32, 0x67, 0x4a,
    0x43, 0x4d, 0x52, 0x65, 0x71, 0x41, 0x67, 0x78, 0x4a, 0x77, 0x54, 0x39,
    0x2f, 0x48, 0x6a, 0x69, 0x6c, 0x57, 0x67, 0x4b, 0x72, 0x51, 0x72, 0x42,
    0x34, 0x31, 0x50, 0x46, 0x4d, 0x4e, 0x71, 0x47, 0x42, 0x66, 0x4c, 0x4b,
    0x61, 0x0a, 0x32, 0x6b, 0x69, 0x50, 0x74, 0x58, 0x48, 0x30, 0x35, 0x46,
    0x50, 0x52, 0x35, 0x71, 0x6f, 0x56, 0x79, 0x42, 0x6b, 0x65, 0x77, 0x63,
    0x45, 0x65, 0x75, 0x41, 0x35, 0x73, 0x6b, 0x78, 0x5a, 0x31, 0x67, 0x33,
    0x7a, 0x4a, 0x30, 0x67, 0x48, 0x35, 0x6f, 0x76, 0x51, 0x64, 0x42, 0x6a,
    0x54, 0x34, 0x59, 0x68, 0x32, 0x4a, 0x37, 0x50, 0x39, 0x68, 0x7a, 0x76,
    0x73, 0x62, 0x6a, 0x39, 0x76, 0x56, 0x0a, 0x4a, 0x75, 0x68, 0x44, 0x6e,
    0x45, 0x34, 0x52, 0x38, 0x2f, 0x65, 0x38, 0x45, 0x74, 0x61, 0x66, 0x53,
    0x41, 0x68, 0x44, 0x53, 0x32, 0x47, 0x74, 0x38, 0x6c, 0x77, 0x62, 0x4f,
    0x52, 0x64, 0x43, 0x55, 0x48, 0x4d, 0x48, 0x59, 0x49, 0x57, 0x4e, 0x47,
    0x59, 0x30, 0x46, 0x6e, 0x61, 0x4d, 0x79, 0x41, 0x74, 0x72, 0x37, 0x54,
    0x75, 0x48, 0x2f, 0x53, 0x43, 0x44, 0x77, 0x4c, 0x6a, 0x6a, 0x52, 0x0a,
    0x6e, 0x41, 0x54, 0x64, 0x30, 0x44, 0x43, 0x30, 0x48, 0x49, 0x74, 0x59,
    0x31, 0x59, 0x32, 0x53, 0x49, 0x43, 0x7a, 0x42, 0x61, 0x41, 0x66, 0x4c,
    0x41, 0x6a, 0x78, 0x79, 0x38, 0x67, 0x58, 0x61, 0x49, 0x53, 0x6c, 0x52,
    0x69, 0x79, 0x6b, 0x37, 0x70, 0x4b, 0x5a, 0x62, 0x4b, 0x6e, 0x61, 0x67,
    0x6b, 0x35, 0x4c, 0x49, 0x6f, 0x63, 0x52, 0x69, 0x73, 0x79, 0x36, 0x4e,
    0x6e, 0x76, 0x41, 0x31, 0x0a, 0x59, 0x66, 0x56, 0x7a, 0x4f, 0x69, 0x6a,
    0x46, 0x4b, 0x55, 0x72, 0x48, 0x6c, 0x76, 0x76, 0x38, 0x38, 0x59, 0x39,
    0x41, 0x32, 0x43, 0x77, 0x78, 0x6d, 0x77, 0x4c, 0x78, 0x34, 0x7a, 0x78,
    0x4a, 0x73, 0x4f, 0x77, 0x74, 0x68, 0x75, 0x42, 0x57, 0x79, 0x76, 0x31,
    0x59, 0x64, 0x34, 0x30, 0x4d, 0x68, 0x67, 0x71, 0x54, 0x75, 0x72, 0x7a,
    0x39, 0x51, 0x51, 0x6e, 0x39, 0x32, 0x6e, 0x6f, 0x72, 0x0a, 0x46, 0x46,
    0x58, 0x2f, 0x58, 0x54, 0x30, 0x6e, 0x65, 0x67, 0x74, 0x4a, 0x4f, 0x56,
    0x44, 0x67, 0x49, 0x41, 0x30, 0x76, 0x49, 0x61, 0x6b, 0x43, 0x41, 0x77,
    0x45, 0x41, 0x41, 0x51, 0x3d, 0x3d, 0x0a, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d,
    0x45, 0x4e, 0x44, 0x20, 0x50, 0x55, 0x42, 0x4c, 0x49, 0x43, 0x20, 0x4b,
    0x45, 0x59, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x0a
};
const unsigned int obs_pub_len = 800;

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

VOID HexToByteArray(TCHAR *hexStr, int hexLen, BYTE *out)
{
    TCHAR ptr[3];

    ptr[2] = 0;

    for (int i = 0; i < hexLen; i += 2)
    {
        ptr[0] = hexStr[i];
        ptr[1] = hexStr[i + 1];
        out[i / 2] = (BYTE)wcstoul(ptr, NULL, 16);
    }
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

BOOL VerifyDigitalSignature(BYTE *buff, DWORD len, BYTE *signature, DWORD signatureLen)
{
    // ASN of PEM public key
    BYTE binaryKey[1024];
    DWORD binaryKeyLen = sizeof(binaryKey);

    // Windows X509 public key info from ASN
    CERT_PUBLIC_KEY_INFO *pbPublicPBLOB = NULL;
    DWORD iPBLOBSize;

    // RSA BLOB info from X509 public key
    PUBLICKEYHEADER *rsaPublicBLOB = NULL;
    DWORD rsaPublicBLOBSize;

    // Handle to public key
    HCRYPTKEY keyOut = NULL;

    // Handle to hash context
    HCRYPTHASH hHash = NULL;

    // Signature in little-endian format
    BYTE *reversedSignature = NULL;

    BOOL ret = FALSE;

    if (!CryptStringToBinaryA((LPCSTR)obs_pub, obs_pub_len, CRYPT_STRING_BASE64HEADER, binaryKey, &binaryKeyLen, NULL, NULL))
        goto cleanup;

    if (!CryptDecodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO, binaryKey, binaryKeyLen, CRYPT_ENCODE_ALLOC_FLAG, NULL, &pbPublicPBLOB, &iPBLOBSize))
        goto cleanup;

    if (!CryptDecodeObjectEx(X509_ASN_ENCODING, RSA_CSP_PUBLICKEYBLOB, pbPublicPBLOB->PublicKey.pbData, pbPublicPBLOB->PublicKey.cbData, CRYPT_ENCODE_ALLOC_FLAG, NULL, &rsaPublicBLOB, &rsaPublicBLOBSize))
        goto cleanup;

    if (!CryptImportKey(hProvider, (const BYTE *)rsaPublicBLOB, rsaPublicBLOBSize, NULL, 0, &keyOut))
        goto cleanup;

    if (!CryptCreateHash(hProvider, CALG_SHA_512, 0, 0, &hHash))
        goto cleanup;

    if (!CryptHashData(hHash, buff, len, 0))
        goto cleanup;

    // Windows requires signature in little-endian. Every other crypto provider is big-endian of course.
    reversedSignature = (BYTE *)HeapAlloc(GetProcessHeap(), 0, signatureLen);

    for (DWORD i = 0; i < signatureLen; i++)
        reversedSignature[i] = signature[signatureLen - i - 1];

    if (!CryptVerifySignature(hHash, reversedSignature, signatureLen, keyOut, NULL, 0))
    {
        DWORD i = GetLastError();
        goto cleanup;
    }

    ret = TRUE;

cleanup:
    if (keyOut != NULL)
        CryptDestroyKey(keyOut);

    if (hHash != NULL)
        CryptDestroyHash(hHash);

    if (pbPublicPBLOB)
        LocalFree(pbPublicPBLOB);

    if (rsaPublicBLOB)
        LocalFree(rsaPublicBLOB);

    if (reversedSignature)
        HeapFree(GetProcessHeap(), 0, reversedSignature);

    return ret;
}

BOOL CheckSignature(TCHAR *path, TCHAR *hexSignature, int signatureLength)
{
    BYTE *fileContents = NULL;
    BYTE *signature = NULL;
    BOOL ret = FALSE;
    XFile updateFile;

    if (signatureLength == 0 || signatureLength > 0xFFFF || (signatureLength & 1) != 0)
    {
        Log(TEXT("WARNING: Missing or invalid signature for %s."), path);
        goto cleanup;
    }

    signature = (BYTE *)Allocate(signatureLength);

    // Convert TCHAR signature to byte array
    HexToByteArray(hexSignature, signatureLength, signature);
    signatureLength /= 2;

    if (updateFile.Open(path, XFILE_READ, XFILE_OPENEXISTING))
    {
        DWORD fileLength = (DWORD)updateFile.GetFileSize();

        fileContents = (BYTE *)Allocate(fileLength);
        if (updateFile.Read(fileContents, fileLength) != fileLength)
            goto cleanup;

        if (!VerifyDigitalSignature(fileContents, fileLength, signature, signatureLength))
        {
            Log(TEXT("WARNING: Signature check failed for %s."), path);
            goto cleanup;
        }

        updateFile.Close();
    }
    else
    {
        goto cleanup;
    }

    ret = TRUE;

cleanup:
    if (fileContents)
        Free(fileContents);

    if (signature)
        Free(signature);

    return ret;
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

    TCHAR hexSignature[4096];
    DWORD signatureLength = sizeof(hexSignature);

    if (HTTPGetFile(url, updateFilePath, extraHeaders, &responseCode, hexSignature, &signatureLength))
    {
        if (responseCode != 200 && responseCode != 304)
            return FALSE;

        // A new file must be digitally signed.
        if (responseCode == 200)
        {
            if (!CheckSignature(updateFilePath, hexSignature, signatureLength))
            {
                DeleteFile(updateFilePath);
                return FALSE;
            }
        }

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

    if (!CryptAcquireContext(&hProvider, NULL, MS_ENH_RSA_AES_PROV, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
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

    TCHAR hexSignature[4096];
    DWORD signatureLength = sizeof(hexSignature);

    if (HTTPGetFile(TEXT(MANIFEST_URL), manifestPath, extraHeaders, &responseCode, hexSignature, &signatureLength))
    {
        if (responseCode == 200 || responseCode == 304)
        {
            String updateInfo;

            // A new file must be digitally signed.
            if (responseCode == 200)
            {
                if (!CheckSignature(manifestPath, hexSignature, signatureLength))
                {
                    DeleteFile(manifestPath);
                    return FALSE;
                }
            }

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