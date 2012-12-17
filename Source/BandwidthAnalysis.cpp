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


class BandwidthAnalyzer : public NetworkStream
{
    DWORD lastTime;
    QWORD totalBytesTransmitted;
    DWORD numSeconds;
    DWORD highestBytes;

    DWORD curBytes;

    HANDLE hTimer;

public:
    BandwidthAnalyzer()
    {
        lastTime = OSGetTime();
    }

    ~BandwidthAnalyzer()
    {
        QWORD bytesPerSec = totalBytesTransmitted/MAX(numSeconds, 1);

        String strReport;
        strReport << TEXT("Stream report:\r\n\r\n");

        /*strReport << App->GetVideoEncoder()->GetInfoString() << TEXT("\r\n\r\n");
        strReport << App->GetAudioEncoder()->GetInfoString() << TEXT("\r\n\r\n");*/

        strReport << TEXT("Total Bytes transmitted: ") << UInt64String(totalBytesTransmitted, 10) <<
                     TEXT("\r\nTotal time of stream in seconds: ") << UIntString(numSeconds) <<
                     TEXT("\r\nAverage Bytes/Bits per second: ") << UInt64String(bytesPerSec, 10) << TEXT(", ") << UInt64String(bytesPerSec*8, 10) <<
                     TEXT("\r\nHighest Bytes/Bits in a second: ") << UIntString(highestBytes) << TEXT(", ") << UIntString(highestBytes*8);

        App->SetStreamReport(strReport);
    }

    void SendPacket(BYTE *data, UINT size, DWORD timestamp, PacketType type)
    {
        DWORD curTime = OSGetTime();

        curBytes += size+8;  //just assume a header of 8 bytes

        if((curTime-lastTime) > 1000)
        {
            totalBytesTransmitted += curBytes;
            numSeconds++;

            if(curBytes > highestBytes)
                highestBytes = curBytes;

            curBytes = 0;
            lastTime += 1000;
        }
    }

    double GetPacketStrain() const {return 0;}
    QWORD GetCurrentSentBytes() {return 0;}
    DWORD NumDroppedFrames() const {return 0;}
    DWORD NumTotalVideoFrames() const {return 1;}
};


NetworkStream* CreateBandwidthAnalyzer()
{
    return new BandwidthAnalyzer;
}
