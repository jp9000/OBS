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
#include "RTMPPublisher.h"

NetworkStream* CreateRTMPPublisher(String &failReason, bool &bCanRetry);


class DelayedPublisher : public NetworkStream
{
    DWORD delayTime;
    List<NetworkPacket> queuedPackets;

    NetworkStream *outputStream;

    bool bPublishingStarted;
    bool bConnecting, bConnected;


    static DWORD WINAPI CreateConnectionThread(DelayedPublisher *publisher)
    {
        String strFailReason;
        bool bRetry = false;

        publisher->outputStream = CreateRTMPPublisher(strFailReason, bRetry);
        if(!publisher->outputStream)
        {
            App->SetStreamReport(strFailReason);
            PostMessage(hwndMain, OBS_REQUESTSTOP, 0, 0);
        }
        else
        {
            publisher->bConnected = true;
            publisher->bConnecting = false;
        }

        return 0;
    }

public:
    ~DelayedPublisher()
    {
        for(UINT i=0; i<queuedPackets.Num(); i++)
            queuedPackets[i].data.Clear();

        delete outputStream;
    }

    void SendPacket(BYTE *data, UINT size, DWORD timestamp, PacketType type)
    {
        if(timestamp > delayTime)
        {
            if(!bConnected)
            {
                if(!bConnecting)
                {
                    HANDLE hThread = OSCreateThread((XTHREAD)CreateConnectionThread, this);
                    OSCloseThread(hThread);

                    bConnecting = true;
                }
            }
            else
            {
                if(!bPublishingStarted)
                    outputStream->BeginPublishing();

                DWORD sendTime = timestamp-delayTime;
                for(UINT i=0; i<queuedPackets.Num(); i++)
                {
                    NetworkPacket &packet = queuedPackets[i];
                    if(packet.timestamp <= sendTime)
                    {
                        outputStream->SendPacket(packet.data.Array(), packet.data.Num(), packet.timestamp, packet.type);
                        packet.data.Clear();
                        queuedPackets.Remove(i--);
                    }
                }
            }
        }

        NetworkPacket *newPacket = queuedPackets.CreateNew();
        newPacket->data.CopyArray(data, size);
        newPacket->timestamp = timestamp;
        newPacket->type = type;
    }

    void BeginPublishing() {}

    double GetPacketStrain() const {return outputStream->GetPacketStrain();}
    QWORD GetCurrentSentBytes() {return outputStream->GetCurrentSentBytes();}
    DWORD NumDroppedFrames() const {return outputStream->NumDroppedFrames();}
};


NetworkStream* CreateDelayedPublisher()
{
    return new DelayedPublisher;
}
