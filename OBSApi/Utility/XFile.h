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

#define XFILE_READ          0x80000000
#define XFILE_WRITE         0x40000000

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
    DWORD dwPos;
    BOOL bHasWritten;

public:
    XFile();
    XFile(CTSTR lpFile, DWORD dwAccess, DWORD dwCreationDisposition);

    ~XFile() {Close();}

    BOOL Open(CTSTR lpFile, DWORD dwAccess, DWORD dwCreationDisposition);
    BOOL IsOpen();
    DWORD Read(LPVOID lpBuffer, DWORD dwBytes);
    DWORD Write(const void *lpBuffer, DWORD dwBytes);
    DWORD WriteStr(LPCSTR lpBuffer);
    DWORD WriteStr(CWSTR lpBuffer);
    DWORD WriteAsUTF8(CTSTR lpBuffer, DWORD dwElements=0);

    void FlushFileBuffers();

    inline void ReadFileToString(String &strIn)
    {
        if(!IsOpen())
            return;

        SetPos(0, XFILE_BEGIN);
        DWORD dwFileSize = GetFileSize();
        LPSTR lpFileDataUTF8 = (LPSTR)Allocate(dwFileSize+1);
        lpFileDataUTF8[dwFileSize] = 0;
        Read(lpFileDataUTF8, dwFileSize);

        strIn = String(lpFileDataUTF8);
        Free(lpFileDataUTF8);
    }

    BOOL SetFileSize(DWORD dwSize);
    DWORD GetFileSize() const;
    UINT64 GetFileSize64() const;
    DWORD SetPos(int iPos, DWORD dwMoveMethod);
    inline DWORD GetPos() const {return dwPos;}
    void Close();
};

BASE_EXPORT String GetPathFileName(CTSTR lpPath, BOOL bExtention=FALSE);
BASE_EXPORT String GetPathDirectory(CTSTR lpPath);
BASE_EXPORT String GetPathWithoutExtension(CTSTR lpPath);
BASE_EXPORT String GetPathExtension(CTSTR lpPath);

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

    void Serialize(LPVOID lpData, DWORD length)
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

    DWORD Seek(long offset, DWORD seekType=SERIALIZE_SEEK_START)
    {
        DWORD ret;

        if(seekType == SERIALIZE_SEEK_START)
            ret = file.SetPos(offset, XFILE_BEGIN);
        else if(seekType == SERIALIZE_SEEK_CURRENT)
            ret = file.SetPos(long(GetPos())+offset, XFILE_BEGIN);
        else if(seekType == SERIALIZE_SEEK_END)
            ret = file.SetPos(offset, XFILE_END);

        Cache();

        return ret;
    }

    DWORD GetPos() const
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

    DWORD bufferPos, bufferSize, fileSize;
    BYTE Buffer[2048];
};


class XFileOutputSerializer : public Serializer
{
public:
    XFileOutputSerializer() {bufferPos = 0;}

    BOOL IsLoading() {return FALSE;}

    BOOL Open(CTSTR lpFile, DWORD dwCreationDisposition)
    {
        totalWritten = bufferPos = 0;
        return file.Open(lpFile, XFILE_WRITE, dwCreationDisposition);
    }

    void Close()
    {
        Flush();
        file.Close();
    }

    void Serialize(LPVOID lpData, DWORD length)
    {
        assert(lpData);

        LPBYTE lpTemp = (LPBYTE)lpData;

        totalWritten += length;

        while(length)
        {
            if(bufferPos == 2048)
                Flush();
                
            DWORD dwWriteSize = MIN(length, (2048-bufferPos));

            assert(dwWriteSize);

            if(!dwWriteSize)
                return;

            mcpy(Buffer+bufferPos, lpTemp, dwWriteSize);

            lpTemp += dwWriteSize;
            bufferPos += dwWriteSize;

            length -= dwWriteSize;
        }
    }

    DWORD Seek(long offset, DWORD seekType=SERIALIZE_SEEK_START)
    {
        Flush();
        if(seekType == SERIALIZE_SEEK_START)
            seekType = XFILE_BEGIN;
        else if(seekType == SERIALIZE_SEEK_CURRENT)
            seekType = XFILE_CURPOS;
        else if(seekType == SERIALIZE_SEEK_END)
            seekType = XFILE_END;

        return file.SetPos(offset, seekType);;
    }

    DWORD GetPos() const
    {
        return file.GetPos()+bufferPos;
    }

    inline DWORD GetTotalWritten() {return totalWritten;}

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

    DWORD bufferPos, totalWritten;
    BYTE Buffer[2048];
};
