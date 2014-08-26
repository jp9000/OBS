/********************************************************************************
 XFile.h: The truth is out there
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


#pragma once

#define XFILE_ERROR         0xFFFFFFFF

#define XFILE_READ          0x1
#define XFILE_WRITE         0x2
#define XFILE_SHARED        0x4

#define XFILE_CREATENEW     1
#define XFILE_CREATEALWAYS  2
#define XFILE_OPENEXISTING  3
#define XFILE_OPENALWAYS    4

#define XFILE_BEGIN         1
#define XFILE_CURPOS        2
#define XFILE_END           3


class BASE_EXPORT XFile
{
    HANDLE hFile;
    QWORD qwPos;
    BOOL bHasWritten;

public:
    XFile();
    XFile(CTSTR lpFile, DWORD dwAccess, DWORD dwCreationDisposition);

    ~XFile() {Close();}

    BOOL Open(CTSTR lpFile, DWORD dwAccess, DWORD dwCreationDisposition);
    BOOL IsOpen();
    DWORD Read(LPVOID lpBuffer, DWORD dwBytes);
    DWORD Write(const void *lpBuffer, DWORD dwBytes);
    BOOL WriteStr(LPCSTR lpBuffer);
    BOOL WriteStr(CWSTR lpBuffer);
    BOOL WriteAsUTF8(CTSTR lpBuffer, DWORD dwElements = 0);

    void FlushFileBuffers();

    inline void ReadFileToString(String &strIn)
    {
        if(!IsOpen())
            return;

        SetPos(0, XFILE_BEGIN);
        DWORD dwFileSize = (DWORD)GetFileSize();

        if (dwFileSize >= 3) // remove BOM if present
        {
            char buff[3];
            Read(&buff, 3);
            if (memcmp(buff, "\xEF\xBB\xBF", 3))
                SetPos(0, XFILE_BEGIN);
            else
                dwFileSize -= 3;
        }

        LPSTR lpFileDataUTF8 = (LPSTR)Allocate(dwFileSize+1);
        lpFileDataUTF8[dwFileSize] = 0;
        
        if (Read(lpFileDataUTF8, dwFileSize) != dwFileSize)
            strIn = String(TEXT(""));
        else
            strIn = String(lpFileDataUTF8);

        Free(lpFileDataUTF8);
    }

    BOOL SetFileSize(DWORD dwSize);
    QWORD GetFileSize() const;
    QWORD SetPos(INT64 iPos, DWORD dwMoveMethod);
    inline QWORD GetPos() const {return qwPos;}
    void Close();
};

BASE_EXPORT String GetPathFileName(CTSTR lpPath, BOOL bExtention=FALSE);
BASE_EXPORT String GetPathDirectory(CTSTR lpPath);
BASE_EXPORT String GetPathWithoutExtension(CTSTR lpPath);
BASE_EXPORT String GetPathExtension(CTSTR lpPath);
BASE_EXPORT BOOL  IsSafeFilename (CTSTR path);
BASE_EXPORT BOOL CreatePath(CTSTR lpPath);

class BASE_EXPORT XFileInputSerializer : public Serializer
{
public:
    XFileInputSerializer() {bufferSize = 0; bufferPos = 0;}
    ~XFileInputSerializer() {Close();}

    BOOL IsLoading() {return TRUE;}

    BOOL Open(CTSTR lpFile)
    {
        bufferPos = bufferSize = 0;
        if(!file.Open(lpFile, XFILE_READ, XFILE_OPENEXISTING))
            return FALSE;
        fileSize = file.GetFileSize();
        return TRUE;
    }

    void Close()
    {
        file.Close();
    }

    void Serialize(LPCVOID lpData, DWORD length)
    {
        assert(lpData);

        LPBYTE lpTemp = (LPBYTE)lpData;

        while(length)
        {
            if(!bufferSize || (bufferSize == bufferPos))
                Cache();

            DWORD dwReadSize = MIN(length, bufferSize-bufferPos);

            if(!dwReadSize)
                return;

            mcpy(lpTemp, Buffer+bufferPos, dwReadSize);

            lpTemp += dwReadSize;
            bufferPos += dwReadSize;

            length -= dwReadSize;
        }
    }

    UINT64 Seek(INT64 offset, DWORD seekType=SERIALIZE_SEEK_START)
    {
        QWORD ret;

        if(seekType == SERIALIZE_SEEK_START)
            ret = file.SetPos(INT64(offset), XFILE_BEGIN);
        else if(seekType == SERIALIZE_SEEK_CURRENT)
            ret = file.SetPos(INT64(GetPos())+offset, XFILE_BEGIN);
        else if(seekType == SERIALIZE_SEEK_END)
            ret = file.SetPos(offset, XFILE_END);
        else
            return 0;

        Cache();

        return ret;
    }

    UINT64 GetPos() const
    {
        return (file.GetPos()-bufferSize)+bufferPos;
    }

    BOOL DataPending()
    {
        return file.GetPos() < fileSize;
    }

    const XFile& GetFile() const {return file;}

private:
    void Cache()
    {
        bufferSize = file.Read(Buffer, 2048);
        bufferPos = 0;
    }

    XFile file;

    DWORD bufferPos, bufferSize;
    UINT64 fileSize;
    BYTE Buffer[2048];
};


class XFileOutputSerializer : public Serializer
{
public:
    XFileOutputSerializer() {bufferPos = 0;}

    BOOL IsLoading() {return FALSE;}

    BOOL Open(CTSTR lpFile, DWORD dwCreationDisposition, DWORD bufferSize=(1024*64))
    {
        if(!bufferSize) bufferSize = 1024*64;

        this->bufferSize = bufferSize;
        Buffer = (LPBYTE)Allocate(bufferSize);
        totalWritten = bufferPos = 0;
        return file.Open(lpFile, XFILE_WRITE, dwCreationDisposition);
    }

    void Close()
    {
        Flush();
        file.Close();
        Free(Buffer);
    }

    void Serialize(LPCVOID lpData, DWORD length)
    {
        assert(lpData);

        LPBYTE lpTemp = (LPBYTE)lpData;

        totalWritten += length;

        while(length)
        {
            if(bufferPos == bufferSize)
                Flush();
                
            DWORD dwWriteSize = MIN(length, (bufferSize-bufferPos));

            assert(dwWriteSize);

            if(!dwWriteSize)
                return;

            mcpy(Buffer+bufferPos, lpTemp, dwWriteSize);

            lpTemp += dwWriteSize;
            bufferPos += dwWriteSize;

            length -= dwWriteSize;
        }
    }

    UINT64 Seek(INT64 offset, DWORD seekType=SERIALIZE_SEEK_START)
    {
        Flush();
        if(seekType == SERIALIZE_SEEK_START)
            seekType = XFILE_BEGIN;
        else if(seekType == SERIALIZE_SEEK_CURRENT)
            seekType = XFILE_CURPOS;
        else if(seekType == SERIALIZE_SEEK_END)
            seekType = XFILE_END;

        return file.SetPos(offset, seekType);
    }

    UINT64 GetPos() const
    {
        return file.GetPos()+bufferPos;
    }

    inline QWORD GetTotalWritten() {return totalWritten;}

private:
    void Flush()
    {
        if(bufferPos)
        {
            file.Write(Buffer, bufferPos);
            bufferPos = 0;
        }
    }

    XFile file;

    DWORD bufferPos;
    DWORD bufferSize;

    QWORD totalWritten;
    LPBYTE Buffer;
};
