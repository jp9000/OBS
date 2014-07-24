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

#include "../lame/include/lame.h"


const int audioBlockSize = 4; //output is 2 16bit channels


struct AACDataPacket
{
    List<BYTE> Packet;

    inline void FreeData() {Packet.Clear();}
};

//lame is not lame..  it's godly.
class MP3Encoder : public AudioEncoder
{
    lame_global_flags *lgf;

    List<BYTE> MP3OutputBuffer;
    List<BYTE> header;
    DWORD dwMP3MaxSize;
    bool bFirstPacket;

    UINT outputFrameSize;
    UINT curBitRate;

    List<QWORD> bufferedTimestamps;
    QWORD curEncodeTimestamp;
    DWORD frameCounter;
    bool bFirstFrame;

public:
    MP3Encoder(UINT bitRate)
    {
        curBitRate = bitRate;

        lgf = lame_init();
        if(!lgf)
            CrashError(TEXT("Unable to open mp3 encoder"));

        lame_set_in_samplerate(lgf, App->GetSampleRateHz());
        lame_set_out_samplerate(lgf, App->GetSampleRateHz());
        lame_set_num_channels(lgf, App->NumAudioChannels());
        lame_set_disable_reservoir(lgf, TRUE); //bit reservoir has to be disabled for seamless streaming
        lame_set_quality(lgf, 2);
        lame_set_VBR(lgf, vbr_off);
        lame_set_brate(lgf, bitRate);
        lame_init_params(lgf);

        outputFrameSize = lame_get_framesize(lgf); //1152 usually
        dwMP3MaxSize = DWORD(1.25*double(outputFrameSize*audioBlockSize) + 7200.0);
        MP3OutputBuffer.SetSize(dwMP3MaxSize+1);
        MP3OutputBuffer[0] = 0x2f;

        bFirstPacket = true;

        Log(TEXT("------------------------------------------"));
        Log(TEXT("%s"), GetInfoString().Array());
    }

    ~MP3Encoder()
    {
        lame_close(lgf);
    }

    bool Encode(float *input, UINT numInputFrames, DataPacket &packet, QWORD &timestamp)
    {
        if(bFirstFrame)
        {
            curEncodeTimestamp = timestamp;
            bFirstFrame = false;
        }

        //------------------------------------------------

        UINT lastSampleSize = frameCounter;

        frameCounter += numInputFrames;
        if(frameCounter > outputFrameSize)
        {
            frameCounter -= outputFrameSize;

            bufferedTimestamps << curEncodeTimestamp;
            curEncodeTimestamp = timestamp + ((outputFrameSize-lastSampleSize)*1000/App->GetSampleRateHz());
        }

        int ret = lame_encode_buffer_interleaved_ieee_float(lgf, (float*)input, numInputFrames, MP3OutputBuffer.Array()+1, dwMP3MaxSize);

        if(ret < 0)
        {
            AppWarning(TEXT("MP3 encode failed"));
            return false;
        }

        if(ret > 0)
        {
            if(bFirstPacket)
            {
                header.CopyArray(MP3OutputBuffer.Array(), ret);
                bFirstPacket = false;
                ret = 0;
            }
            else
            {
                packet.lpPacket = MP3OutputBuffer.Array();
                packet.size     = ret+1;

                timestamp = bufferedTimestamps[0];
                bufferedTimestamps.Remove(0);
            }
        }

        return ret > 0;
    }

    UINT GetFrameSize() const
    {
        return outputFrameSize;
    }

    void GetHeaders(DataPacket &packet)
    {
        packet.lpPacket = header.Array();
        packet.size = header.Num();
    }

    int GetBitRate() const {return curBitRate;}
    CTSTR GetCodec() const {return TEXT("MP3");}

    String GetInfoString() const
    {
        String strInfo;
        strInfo << TEXT("Audio Encoding: MP3") <<
                   TEXT("\r\n    bitrate: ") << IntString(curBitRate);

        return strInfo;
    }
};


AudioEncoder* CreateMP3Encoder(UINT bitRate)
{
    return new MP3Encoder(bitRate);
}
