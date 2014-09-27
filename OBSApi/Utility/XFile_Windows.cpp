/********************************************************************************
 XFile.cpp: The truth is out there
 Copyright (C) 2001-2012 Hugh Bailey <obs.jim@gmail.com>

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


#ifdef WIN32

#define _WIN32_WINDOWS 0x0410
#define _WIN32_WINNT   0x0403
#include <windows.h>
#include "XT.h"



XFile::XFile()
{
    hFile = INVALID_HANDLE_VALUE;
    qwPos = 0;
    bHasWritten = false;
}

XFile::XFile(CTSTR lpFile, DWORD dwAccess, DWORD dwCreationDisposition)
{
    assert(lpFile);
    bHasWritten = false;
    Open(lpFile, dwAccess, dwCreationDisposition);
}

BOOL XFile::Open(CTSTR lpFile, DWORD dwAccess, DWORD dwCreationDisposition)
{
    qwPos = 0;

    DWORD dwFileAccess = 0;
    DWORD dwShareAccess = 0;

    if(dwAccess & XFILE_READ)
    {
        dwFileAccess |= GENERIC_READ;
        if(dwAccess & XFILE_SHARED)
            dwShareAccess |= FILE_SHARE_READ | FILE_SHARE_WRITE;
    }
    if(dwAccess & XFILE_WRITE)
    {
        dwFileAccess |= GENERIC_WRITE;
        if(dwAccess & XFILE_SHARED)
            dwShareAccess |= FILE_SHARE_READ;
    }

    assert(lpFile);
    if((hFile = CreateFile(lpFile, dwFileAccess, dwShareAccess, NULL, dwCreationDisposition, 0, NULL)) == INVALID_HANDLE_VALUE)
        return 0;
    return 1;
}

BOOL XFile::IsOpen()
{
    return hFile != INVALID_HANDLE_VALUE;
}

DWORD XFile::Read(LPVOID lpBuffer, DWORD dwBytes)
{
    DWORD dwRet;

    assert(lpBuffer);

    if(!lpBuffer) return XFILE_ERROR;

    if(!hFile) return XFILE_ERROR;

    qwPos += dwBytes;

    ReadFile(hFile, lpBuffer, dwBytes, &dwRet, NULL);
    return dwRet;
}

DWORD XFile::Write(const void *lpBuffer, DWORD dwBytes)
{
    DWORD dwRet;

    assert(lpBuffer);

    if(!hFile) return XFILE_ERROR;

    qwPos += dwBytes;

    WriteFile(hFile, lpBuffer, dwBytes, &dwRet, NULL);

    if(dwRet)
        bHasWritten = TRUE;

    return dwRet;
}

BOOL XFile::WriteStr(CWSTR lpBuffer)
{
    assert(lpBuffer);

    if(!hFile) return XFILE_ERROR;

    DWORD dwElements = (DWORD)wcslen(lpBuffer);

    char lpDest[4096];
    DWORD dwBytes = (DWORD)wchar_to_utf8(lpBuffer, dwElements, lpDest, 4095, 0);
    DWORD retVal = (Write(lpDest, dwBytes) == dwBytes);

    return retVal;
}

void XFile::FlushFileBuffers()
{
    ::FlushFileBuffers(hFile);
}

BOOL XFile::WriteStr(LPCSTR lpBuffer)
{
    assert(lpBuffer);

    if (!hFile) return false;

    DWORD dwElements = (DWORD)strlen(lpBuffer);

    return (Write(lpBuffer, dwElements) == dwElements);
}

BOOL XFile::WriteAsUTF8(CTSTR lpBuffer, DWORD dwElements)
{
    DWORD retVal = 0;

    if(!lpBuffer)
        return false;

    if (!hFile) return false;

    if (!lpBuffer[0])
        return true;

    if(!dwElements)
        dwElements = slen(lpBuffer);

#ifdef UNICODE
    DWORD dwBytes = (DWORD)wchar_to_utf8_len(lpBuffer, dwElements, 0);
    LPSTR lpDest = (LPSTR)Allocate(dwBytes+1);

    if (wchar_to_utf8(lpBuffer, dwElements, lpDest, dwBytes, 0))
        retVal = (Write(lpDest, dwBytes) == dwBytes);
    else
        Log(TEXT("XFile::WriteAsUTF8: wchar_to_utf8 failed: %d"), GetLastError());

    Free(lpDest);
#else
    DWORD retVal = (Write(lpBuffer, dwElements) == dwBytes);
#endif

    return retVal;
}

BOOL XFile::SetFileSize(DWORD dwSize)
{
    assert(hFile != INVALID_HANDLE_VALUE);

    if(!hFile) return 0;

    if(qwPos > dwSize)
        qwPos = dwSize;

    SetPos(dwSize, XFILE_BEGIN);
    return SetEndOfFile(hFile);
}

QWORD XFile::GetFileSize() const
{
    UINT64 size = 0;
    size |= ::GetFileSize(hFile, (DWORD*)(((BYTE*)&size)+4));

    return size;
}

UINT64 XFile::SetPos(INT64 iPos, DWORD dwMoveMethod)  //uses the SetFilePointer 4th parameter flags
{
    assert(hFile != INVALID_HANDLE_VALUE);

    if(!hFile) return 0;

    DWORD moveValue = FILE_CURRENT;

    switch(dwMoveMethod)
    {
        //case XFILE_CURPOS:
        //    moveValue = FILE_CURRENT;
        //    break;

        case XFILE_END:
            moveValue = FILE_END;
            break;

        case XFILE_BEGIN:
            moveValue = FILE_BEGIN;
            break;
    }

    XLARGE_INT largeInt;
    largeInt.largeVal = iPos;

    DWORD newPosLow = SetFilePointer(hFile, LONG(largeInt.lowVal), &largeInt.highVal, moveValue);
    largeInt.lowVal = newPosLow;

    return largeInt.largeVal;
}

void XFile::Close()
{
    if(hFile != INVALID_HANDLE_VALUE)
    {
        //this is not really necessary and kills performance
        //if(bHasWritten)
        //    ::FlushFileBuffers(hFile);
        CloseHandle(hFile);
        hFile = INVALID_HANDLE_VALUE;
    }
}

String GetPathFileName(CTSTR lpPath, BOOL bExtension)
{
    assert(lpPath);
    if(!lpPath)
        return String();

    OSFindData ofd;
    HANDLE hFind = OSFindFirstFile(lpPath, ofd);

    if(!hFind)
        ofd.bDirectory = FALSE;
    else
        OSFindClose(hFind);

    if(!ofd.bDirectory)
    {
        CTSTR lpDirectoryEnd = srchr(lpPath, '/');

        if(!lpDirectoryEnd)
            lpDirectoryEnd = srchr(lpPath, '/');

        if(lpDirectoryEnd)
            lpPath = lpDirectoryEnd+1;
    }

    String newPath = lpPath;
    if(newPath.IsValid())
    {
        if(!bExtension)
        {
            TSTR pDot = srchr(newPath, '.');
            if(pDot)
                newPath.SetLength((int)((((UPARAM)pDot)-((UPARAM)newPath.Array()))/sizeof(TCHAR)));
        }
    }

    return newPath;
}

String GetPathDirectory(CTSTR lpPath)
{
    assert(lpPath);
    if(!lpPath)
        return String();

    OSFindData ofd;
    HANDLE hFind = OSFindFirstFile(lpPath, ofd);

    if(!hFind)
        ofd.bDirectory = FALSE;
    else
        OSFindClose(hFind);

    int nDirectoryEnd;

    if(!ofd.bDirectory)
    {
        CTSTR lpDirectoryEnd = srchr(lpPath, '/');

        if(!lpDirectoryEnd)
            lpDirectoryEnd = srchr(lpPath, '\\');

        if(lpDirectoryEnd)
            nDirectoryEnd = (int)((((UPARAM)lpDirectoryEnd)-((UPARAM)lpPath))/sizeof(TCHAR));
        else
            nDirectoryEnd = slen(lpPath);
    }
    else
        nDirectoryEnd = slen(lpPath);

    String newPath = lpPath;

    newPath.SetLength(nDirectoryEnd);
    return newPath;
}

String GetPathWithoutExtension(CTSTR lpPath)
{
    assert(lpPath);
    if(!lpPath)
        return String();

    TSTR lpExtensionStart = srchr(lpPath, '.');
    if(lpExtensionStart)
    {
        UINT newLength = (UINT)(UPARAM)(lpExtensionStart-lpPath);
        if(!newLength)
            return String();

        String newString;
        newString.SetLength(newLength);
        scpy_n(newString, lpPath, newLength);

        return newString;
    }
    else
        return String(lpPath);
}

String GetPathExtension(CTSTR lpPath)
{
    assert(lpPath);
    if(!lpPath)
        return String();

    TSTR lpExtensionStart = srchr(lpPath, '.');
    if(lpExtensionStart)
        return String(lpExtensionStart+1);
    else
        return String();
}

BOOL IsSafeFilename(CTSTR path)
{
    const TCHAR *p;

    p = path;

    if (!*p)
        return FALSE;

    if (sstr(path, TEXT("..")))
        return FALSE;

    if (*p == '/')
        return FALSE;

    while (*p)
    {
        if (!isalnum(*p) && *p != ' ' && *p != '.' && *p != '/' && *p != '_' && *p != '-')
            return FALSE;
        p++;
    }

    return TRUE;
}


BOOL CreatePath(CTSTR lpPath)
{
    if(OSFileExists(lpPath))
        return true;
    if(OSCreateDirectory(lpPath))
        return true;
    else
    {
        String parent = GetPathDirectory(lpPath);
        if (parent == lpPath)
            return false;
        if (!CreatePath(parent))
            return false;
    }
    return OSCreateDirectory(lpPath);
}

#endif
