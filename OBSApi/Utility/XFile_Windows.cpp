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
    traceInFast(XFile::XFile);

    assert(lpFile);
    Open(lpFile, dwAccess, dwCreationDisposition);

    traceOutFast;
}

BOOL XFile::Open(CTSTR lpFile, DWORD dwAccess, DWORD dwCreationDisposition)
{
    traceInFast(XFile::Open);

    qwPos = 0;

    assert(lpFile);
    if((hFile = CreateFile(lpFile, dwAccess, 0, NULL, dwCreationDisposition, 0, NULL)) == INVALID_HANDLE_VALUE)
        return 0;
    return 1;

    traceOutFast;
}

BOOL XFile::IsOpen()
{
    return hFile != INVALID_HANDLE_VALUE;
}

DWORD XFile::Read(LPVOID lpBuffer, DWORD dwBytes)
{
    traceInFast(XFile::Read);

    DWORD dwRet;

    assert(lpBuffer);

    if(!lpBuffer) return XFILE_ERROR;

    if(!hFile) return XFILE_ERROR;

    qwPos += dwBytes;

    ReadFile(hFile, lpBuffer, dwBytes, &dwRet, NULL);
    return dwRet;

    traceOutFast;
}

DWORD XFile::Write(const void *lpBuffer, DWORD dwBytes)
{
    traceInFast(XFile::Write);

    DWORD dwRet;

    assert(lpBuffer);

    if(!hFile) return XFILE_ERROR;

    qwPos += dwBytes;

    WriteFile(hFile, lpBuffer, dwBytes, &dwRet, NULL);

    if(dwRet)
        bHasWritten = TRUE;

    return dwRet;

    traceOutFast;
}

DWORD XFile::WriteStr(CWSTR lpBuffer)
{
    traceInFast(XFile::WriteStr);

    assert(lpBuffer);

    if(!hFile) return XFILE_ERROR;

    DWORD dwElements = (DWORD)wcslen(lpBuffer);

    char lpDest[4096];
    DWORD dwBytes = (DWORD)wchar_to_utf8(lpBuffer, dwElements, lpDest, 4095, 0);
    DWORD retVal = Write(lpDest, dwBytes);

    return retVal;

    traceOutFast;
}

void XFile::FlushFileBuffers()
{
    ::FlushFileBuffers(hFile);
}

DWORD XFile::WriteStr(LPCSTR lpBuffer)
{
    traceInFast(XFile::WriteStr);

    assert(lpBuffer);

    if(!hFile) return XFILE_ERROR;

    DWORD dwElements = (DWORD)strlen(lpBuffer);

    return Write(lpBuffer, dwElements);

    traceOutFast;
}

DWORD XFile::WriteAsUTF8(CTSTR lpBuffer, DWORD dwElements)
{
    traceInFast(XFile::WriteAsUTF8);

    if(!lpBuffer)
        return 0;

    if(!hFile) return XFILE_ERROR;

    if(!dwElements)
        dwElements = slen(lpBuffer);

#ifdef UNICODE
    DWORD dwBytes = (DWORD)wchar_to_utf8_len(lpBuffer, dwElements, 0);
    LPSTR lpDest = (LPSTR)Allocate(dwBytes+1);
    wchar_to_utf8(lpBuffer, dwElements, lpDest, dwBytes, 0);
    DWORD retVal = Write(lpDest, dwBytes);
    Free(lpDest);
#else
    DWORD retVal = Write(lpBuffer, dwElements);
#endif

    return retVal;

    traceOutFast;
}

BOOL XFile::SetFileSize(DWORD dwSize)
{
    traceInFast(XFile::SetFileSize);

    assert(hFile != INVALID_HANDLE_VALUE);

    if(!hFile) return 0;

    if(qwPos > dwSize)
        qwPos = dwSize;

    SetPos(dwSize, XFILE_BEGIN);
    return SetEndOfFile(hFile);

    traceOutFast;
}

QWORD XFile::GetFileSize() const
{
    traceInFast(XFile::GetFileSize);

    UINT64 size = 0;
    size |= ::GetFileSize(hFile, (DWORD*)(((BYTE*)&size)+4));

    return size;

    traceOutFast;
}

UINT64 XFile::SetPos(INT64 iPos, DWORD dwMoveMethod)  //uses the SetFilePointer 4th parameter flags
{
    traceInFast(XFile::SetPos);

    assert(hFile != INVALID_HANDLE_VALUE);

    if(!hFile) return 0;

    DWORD moveValue;

    switch(dwMoveMethod)
    {
        case XFILE_CURPOS:
            moveValue = FILE_CURRENT;
            break;

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

    traceOutFast;
}

void XFile::Close()
{
    traceInFast(XFile::Close);

    if(hFile != INVALID_HANDLE_VALUE)
    {
        if(bHasWritten)
            ::FlushFileBuffers(hFile);
        CloseHandle(hFile);
        hFile = INVALID_HANDLE_VALUE;
    }

    traceOutFast;
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

    if(!bExtension)
    {
        TSTR pDot = srchr(newPath, '.');
        if(pDot)
            newPath.SetLength((int)((((UPARAM)pDot)-((UPARAM)newPath.Array()))/sizeof(TCHAR)));
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
            lpDirectoryEnd = srchr(lpPath, '/');

        if(lpDirectoryEnd)
            nDirectoryEnd = (int)((((UPARAM)lpDirectoryEnd)-((UPARAM)lpPath))/sizeof(TCHAR));
        else
            nDirectoryEnd = slen(lpPath);
    }
    else
        nDirectoryEnd = slen(lpPath);

    String newPath = lpPath;

    newPath[nDirectoryEnd] = 0;
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

#endif
