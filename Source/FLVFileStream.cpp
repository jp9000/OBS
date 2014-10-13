/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

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
#include "RTMPStuff.h"

#include "DataPacketHelpers.h"



class FLVFileStream : public VideoFileStream
{
    XFileOutputSerializer fileOut;
    String strFile;

    UINT64 metaDataPos;
    DWORD lastTimeStamp, initialTimestamp;

    decltype(GetBufferedSEIPacket()) sei = GetBufferedSEIPacket();
    decltype(GetBufferedAudioHeadersPacket()) audioHeaders = GetBufferedAudioHeadersPacket();
    decltype(GetBufferedVideoHeadersPacket()) videoHeaders = GetBufferedVideoHeadersPacket();

    bool bSentFirstPacket, bSentSEI;

    void AppendFLVPacket(const BYTE *lpData, UINT size, BYTE type, DWORD timestamp)
    {
        if (!bSentSEI && type == 9 && lpData[0] == 0x17 && lpData[1] == 0x1) { //send SEI with first keyframe packet
            UINT networkDataSize  = fastHtonl(size+sei.size);
            UINT networkTimestamp = fastHtonl(timestamp);
            UINT streamID = 0;
            fileOut.OutputByte(type);
            fileOut.Serialize(((LPBYTE)(&networkDataSize))+1,  3);
            fileOut.Serialize(((LPBYTE)(&networkTimestamp))+1, 3);
            fileOut.Serialize(&networkTimestamp, 1);
            fileOut.Serialize(&streamID, 3);
            fileOut.Serialize(lpData, 5);
            fileOut.Serialize(sei.lpPacket, sei.size);
            fileOut.Serialize(lpData+5, size-5);
            fileOut.OutputDword(fastHtonl(size+sei.size+14));

            bSentSEI = true;
        } else {
            UINT networkDataSize  = fastHtonl(size);
            UINT networkTimestamp = fastHtonl(timestamp);
            UINT streamID = 0;
            fileOut.OutputByte(type);
            fileOut.Serialize(((LPBYTE)(&networkDataSize))+1,  3);
            fileOut.Serialize(((LPBYTE)(&networkTimestamp))+1, 3);
            fileOut.Serialize(&networkTimestamp, 1);
            fileOut.Serialize(&streamID, 3);
            fileOut.Serialize(lpData, size);
            fileOut.OutputDword(fastHtonl(size+14));
        }

        lastTimeStamp = timestamp;
    }

public:
    bool Init(CTSTR lpFile)
    {
        strFile = lpFile;
        initialTimestamp = -1;

        if(!fileOut.Open(lpFile, XFILE_CREATEALWAYS, 1024*1024))
            return false;

        fileOut.OutputByte('F');
        fileOut.OutputByte('L');
        fileOut.OutputByte('V');
        fileOut.OutputByte(1);
        fileOut.OutputByte(5); //bit 0 = (hasAudio), bit 2 = (hasAudio)
        fileOut.OutputDword(DWORD_BE(9));
        fileOut.OutputDword(0);

        metaDataPos = fileOut.GetPos();

        char  metaDataBuffer[2048];
        char *enc = metaDataBuffer;
        char *pend = metaDataBuffer+sizeof(metaDataBuffer);

        enc = AMF_EncodeString(enc, pend, &av_onMetaData);
        char *endMetaData  = App->EncMetaData(enc, pend, true);
        UINT  metaDataSize = endMetaData-metaDataBuffer;

        AppendFLVPacket((LPBYTE)metaDataBuffer, metaDataSize, 18, 0);
        return true;
    }

    ~FLVFileStream()
    {
        UINT64 fileSize = fileOut.GetPos();
        fileOut.Close();

        XFile file;
        if(file.Open(strFile, XFILE_WRITE, XFILE_OPENEXISTING))
        {
            double doubleFileSize = double(fileSize);
            double doubleDuration = double(lastTimeStamp/1000);

            file.SetPos(metaDataPos+0x28, XFILE_BEGIN);
            QWORD outputVal = *reinterpret_cast<QWORD*>(&doubleDuration);
            outputVal = fastHtonll(outputVal);
            file.Write(&outputVal, 8);

            file.SetPos(metaDataPos+0x3B, XFILE_BEGIN);
            outputVal = *reinterpret_cast<QWORD*>(&doubleFileSize);
            outputVal = fastHtonll(outputVal);
            file.Write(&outputVal, 8);

            file.Close();
        }
    }

    void InitBufferedPackets()
    {
        sei.InitBuffer();
        audioHeaders.InitBuffer();
        videoHeaders.InitBuffer();
    }

    virtual void AddPacket(const BYTE *data, UINT size, DWORD timestamp, DWORD /*pts*/, PacketType type) override
    {
        InitBufferedPackets();

        if(!bSentFirstPacket)
        {
            bSentFirstPacket = true;

            AppendFLVPacket(audioHeaders.lpPacket, audioHeaders.size, 8, 0);
            AppendFLVPacket(videoHeaders.lpPacket, videoHeaders.size, 9, 0);
        }

        if(initialTimestamp == -1 && data[0] != 0x17)
            return;
        else if(initialTimestamp == -1 && data[0] == 0x17) {
            initialTimestamp = timestamp;
        }

        AppendFLVPacket(data, size, (type == PacketType_Audio) ? 8 : 9, timestamp-initialTimestamp);
    }
};


VideoFileStream* CreateFLVFileStream(CTSTR lpFile)
{
    FLVFileStream *fileStream = new FLVFileStream;
    if(fileStream->Init(lpFile))
        return fileStream;

    delete fileStream;
    return NULL;
}
