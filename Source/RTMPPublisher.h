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


struct NetworkPacket
{
    List<BYTE> data;
    DWORD timestamp;
    PacketType type;
    UINT distanceFromDroppedFrame;
};

//max latency in milliseconds allowed when using the send buffer
const DWORD maxBufferTime = 600;

struct PacketTimeSize
{
    inline PacketTimeSize(DWORD timestamp, DWORD size) : timestamp(timestamp), size(size) {}

    DWORD timestamp;
    DWORD size;
};

class RTMPPublisher : public NetworkStream
{
    friend class DelayedPublisher;

    List<PacketTimeSize> packetSizeRecord;
    DWORD outputRateSize;

    bool numStartFrames, bNetworkStrain;
    double dNetworkStrain;
    int ignoreCount;
    DWORD currentBufferSize, sendTime;
    DWORD bufferTime, outputRateWindowTime, dropThreshold, connectTime;
    List<NetworkPacket> queuedPackets;

    bool bStreamStarted;
    bool bConnecting, bConnected;

    static DWORD WINAPI CreateConnectionThread(RTMPPublisher *publisher);

    void BeginPublishingInternal();

    int FlushSendBuffer();
    static int BufferedSend(RTMPSockBuf *sb, const char *buf, int len, RTMPPublisher *network);

protected:
    RTMP *rtmp;

    HANDLE hSendSempahore;
    HANDLE hDataMutex;
    HANDLE hSendThread;

    bool bStopping;

    int packetWaitType;

    QWORD bytesSent;

    UINT totalFrames;
    UINT numPFramesDumped;
    UINT numBFramesDumped;

    BOOL bUseSendBuffer;
    List<BYTE> sendBuffer;
    int curSendBufferLen;

    DWORD numVideoPacketsBuffered;
    DWORD firstBufferedVideoFrameTimestamp;

    void SendLoop();
    static DWORD SendThread(RTMPPublisher *publisher);

    void DropFrame(UINT id);
    bool DoIFrameDelay(bool bBFramesOnly);

    void ProcessPackets(DWORD timestamp);

public:
    RTMPPublisher();
    bool Init(RTMP *rtmpIn, UINT tcpBufferSize, BOOL bUseSendBuffer, UINT sendBufferSize);
    ~RTMPPublisher();

    void SendPacket(BYTE *data, UINT size, DWORD timestamp, PacketType type);

    void BeginPublishing();

    double GetPacketStrain() const;
    QWORD GetCurrentSentBytes();
    DWORD NumDroppedFrames() const;
};