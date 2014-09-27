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

#include <Iphlpapi.h>

struct NetworkPacket
{
    List<BYTE> data;
    DWORD timestamp;
    PacketType type;
    UINT distanceFromDroppedFrame;
};

//max latency in milliseconds allowed when using the send buffer
const DWORD maxBufferTime = 600;

typedef enum
{
    LL_MODE_NONE = 0,
    LL_MODE_FIXED,
    LL_MODE_AUTO,
} latencymode_t;

/*struct PacketTimeSize
{
    inline PacketTimeSize(DWORD timestamp, DWORD size) : timestamp(timestamp), size(size) {}

    DWORD timestamp;
    DWORD size;
};*/

class RTMPPublisher : public NetworkStream
{
    friend class DelayedPublisher;

    /*List<PacketTimeSize> packetSizeRecord;
    DWORD outputRateSize;*/

    //-----------------------------------------------

    static DWORD WINAPI CreateConnectionThread(RTMPPublisher *publisher);

    void BeginPublishingInternal();

    static int BufferedSend(RTMPSockBuf *sb, const char *buf, int len, RTMPPublisher *network);

    static String strRTMPErrors;

    static void librtmpErrorCallback(int level, const char *format, va_list vl);
    static String GetRTMPErrors();

protected:

    bool numStartFrames, bNetworkStrain;
    double dNetworkStrain;

    //-----------------------------------------------
    // stream startup stuff
    bool bStreamStarted;
    bool bConnecting, bConnected;
    DWORD firstTimestamp;
    bool bSentFirstKeyframe, bSentFirstAudio;

    List<TimedPacket> bufferedPackets;
    DWORD audioTimeOffset;
    bool bBufferFull;

    bool bFirstKeyframe;
    UINT FindClosestQueueIndex(DWORD timestamp);
    UINT FindClosestBufferIndex(DWORD timestamp);
    void InitializeBuffer();
    void SendPacketForReal(BYTE *data, UINT size, DWORD timestamp, PacketType type);

    bool encoderDataInitialized = false;
    std::vector<char> metaDataPacketBuffer;
    DataPacket audioHeaders, videoHeaders;
    void InitEncoderData();

    //-----------------------------------------------
    // frame drop stuff

    DWORD minFramedropTimestsamp;
    DWORD dropThreshold, bframeDropThreshold;
    List<NetworkPacket> queuedPackets;
    UINT currentBufferSize;//, outputRateWindowTime;
    UINT lastBFrameDropTime;

    //-----------------------------------------------

    RTMP *rtmp;

    HANDLE hSendSempahore;
    HANDLE hDataMutex;
    HANDLE hSendThread;
    HANDLE hSocketThread;
    HANDLE hWriteEvent;
    HANDLE hBufferEvent;
    HANDLE hBufferSpaceAvailableEvent;
    HANDLE hDataBufferMutex;
    HANDLE hRTMPMutex;
    HANDLE hConnectionThread;

    HANDLE hSendLoopExit;
    HANDLE hSocketLoopExit;

    HANDLE hSendBacklogEvent;
    OVERLAPPED sendBacklogOverlapped;

    bool bStopping;

    int packetWaitType;

    QWORD bytesSent;

    UINT totalFrames;
    UINT totalVideoFrames;
    UINT numPFramesDumped;
    UINT numBFramesDumped;

    BYTE *dataBuffer;
    int dataBufferSize;

    int curDataBufferLen;

    latencymode_t lowLatencyMode;
    int latencyFactor;
    int totalTimesWaited;
    int totalBytesWaited;

    QWORD totalSendBytes;
    DWORD totalSendPeriod;
    DWORD totalSendCount;

    bool bFastInitialKeyframe;

    void SendLoop();
    void SocketLoop();
    int FlushDataBuffer();
    void SetupSendBacklogEvent();
    void FatalSocketShutdown();
    static DWORD SendThread(RTMPPublisher *publisher);
    static DWORD SocketThread(RTMPPublisher *publisher);

    void DropFrame(UINT id);
    bool DoIFrameDelay(bool bBFramesOnly);

    virtual void ProcessPackets();
    virtual void FlushBufferedPackets();

    virtual void RequestKeyframe(int waitTime);

public:
    RTMPPublisher();
    bool Init(UINT tcpBufferSize);
    ~RTMPPublisher();

    void SendPacket(BYTE *data, UINT size, DWORD timestamp, PacketType type);

    void BeginPublishing();

    double GetPacketStrain() const;
    QWORD GetCurrentSentBytes();
    DWORD NumDroppedFrames() const;
    DWORD NumTotalVideoFrames() const {return totalVideoFrames;}
};
