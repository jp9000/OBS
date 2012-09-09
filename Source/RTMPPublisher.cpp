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


void rtmp_log_output(int level, const char *format, va_list vl)
{
    int size = _vscprintf(format, vl);
    LPSTR lpTemp = (LPSTR)Allocate(size+1);
    vsprintf_s(lpTemp, size+1, format, vl);

   // OSDebugOut(TEXT("%S\r\n"), lpTemp);
    Log(TEXT("%S\r\n"), lpTemp);

    Free(lpTemp);
}


struct NetworkPacket
{
    List<BYTE> data;
    DWORD timestamp;
    PacketType type;
};

class RTMPPublisher : public NetworkStream
{
    RTMP *rtmp;

    HANDLE hSendSempahore;
    HANDLE hDataMutex;
    HANDLE hSendThread;

    bool bStopping;

    int packetWaitType;
    List<NetworkPacket> Packets;
    UINT numVideoPackets;
    UINT maxVideoPackets, BFrameThreshold;

    UINT bytesPerSec, totalBytesSentThisSec;

    //UINT numBFramesDumped;

    //all data sending is done in yet another separate thread to prevent blocking in the main capture thread
    void SendLoop()
    {
        traceIn(RTMPPublisher::SendLoop);

        DWORD loopStartTime = OSGetTime();

        while(WaitForSingleObject(hSendSempahore, INFINITE) == WAIT_OBJECT_0 && !bStopping && RTMP_IsConnected(rtmp))
        {
            OSEnterMutex(hDataMutex);
            if(Packets.Num() == 0)
            {
                OSLeaveMutex(hDataMutex);
                continue;
            }

            PacketType type      = Packets[0].type;
            DWORD      timestamp = Packets[0].timestamp;

            List<BYTE> packetData;
            packetData.TransferFrom(Packets[0].data);

            Packets.Remove(0);
            if(type != PacketType_Audio)
                numVideoPackets--;

            OSLeaveMutex(hDataMutex);

            //--------------------------------------------

            //DWORD startTime = OSGetTime();

            RTMPPacket packet;
            packet.m_nChannel = (type == PacketType_Audio) ? 0x5 : 0x4;
            packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
            packet.m_packetType = (type == PacketType_Audio) ? RTMP_PACKET_TYPE_AUDIO : RTMP_PACKET_TYPE_VIDEO;
            packet.m_nTimeStamp = timestamp;
            packet.m_nInfoField2 = rtmp->m_stream_id;
            packet.m_hasAbsTimestamp = TRUE;

            packet.m_nBodySize = packetData.Num()-RTMP_MAX_HEADER_SIZE;
            packet.m_body = (char*)packetData.Array()+RTMP_MAX_HEADER_SIZE;

            if(!RTMP_SendPacket(rtmp, &packet, FALSE))
            {
                if(!RTMP_IsConnected(rtmp))
                {
                    App->PostStopMessage();
                    bStopping = true;
                    break;
                }
            }

            //DWORD totalTime = OSGetTime()-startTime;

            DWORD curTime = OSGetTime();
            DWORD timeElapsed = curTime-loopStartTime;

            if(timeElapsed > 1000 && timeElapsed < 2000)
            {
                loopStartTime += 1000;
                timeElapsed += 1000;

                bytesPerSec = totalBytesSentThisSec;
                totalBytesSentThisSec = 0;
            }
            if(timeElapsed > 1000)
                loopStartTime = curTime;
                

            totalBytesSentThisSec += packetData.Num();

            /*if(totalTime >= 200)
            {
                double blockTime = double(totalTime)*0.001;
                double sec = double(OSGetTime()-loopStartTime)*0.001;
                Log(TEXT("blocked by %g seconds at %g seconds into the stream by a packet trying to send %u bytes, %u packets in queue"), blockTime, sec, size, Packets.Num());
            }*/
        }

        traceOut;
    }

    static DWORD SendThread(RTMPPublisher *publisher)
    {
        publisher->SendLoop();
        return 0;
    }

    //video packet count exceeding maximum.  find lowest priority frame to dump
    void DoIFrameDelay()
    {
        while(packetWaitType < PacketType_VideoHighest)
        {
            if(packetWaitType == PacketType_VideoHigh)
            {
                UINT bestItem = INVALID;
                bool bFoundIFrame = false, bFoundFrameBeforeIFrame = false;

                for(int i=int(Packets.Num())-1; i>=0; i--)
                {
                    NetworkPacket &packet = Packets[i];
                    if(packet.type == PacketType_Audio)
                        continue;

                    if(packet.type == packetWaitType)
                    {
                        if(bFoundIFrame)
                        {
                            bestItem = UINT(i);
                            bFoundFrameBeforeIFrame = true;
                            break;
                        }
                        else if(bestItem == INVALID)
                            bestItem = UINT(i);
                    }
                    else if(packet.type == PacketType_VideoHighest)
                        bFoundIFrame = true;
                }

                if(bestItem != INVALID)
                {
                    NetworkPacket &bestPacket = Packets[bestItem];

                    bestPacket.data.Clear();
                    Packets.Remove(bestItem);
                    numVideoPackets--;

                    //disposing P-frames will corrupt the rest of the frame group, so you have to wait until another I-frame
                    if(!bFoundIFrame || !bFoundFrameBeforeIFrame)
                        packetWaitType = PacketType_VideoHighest;
                    return;
                }
            }
            else
            {
                bool bRemovedPacket = false;

                for(UINT i=0; i<Packets.Num(); i++)
                {
                    NetworkPacket &packet = Packets[i];
                    if(packet.type == PacketType_Audio)
                        continue;

                    if(bRemovedPacket)
                    {
                        if(packet.type >= packetWaitType)
                        {
                            packetWaitType = PacketType_VideoDisposable;
                            break;
                        }
                        else //clear out following dependent packets of lower priority
                        {
                            packet.data.Clear();
                            Packets.Remove(i--);
                            numVideoPackets--;
                            //numBFramesDumped++;
                        }
                    }
                    else if(packet.type == packetWaitType)
                    {
                        packet.data.Clear();
                        Packets.Remove(i--);
                        numVideoPackets--;
                        //numBFramesDumped++;

                        bRemovedPacket = true;
                    }
                }

                if(bRemovedPacket)
                    break;
            }

            packetWaitType++;
        }
    }

    void DumpBFrame()
    {
        int prevWaitType = packetWaitType;

        bool bRemovedPacket = false;

        while(packetWaitType < PacketType_VideoHigh)
        {
            for(UINT i=0; i<Packets.Num(); i++)
            {
                NetworkPacket &packet = Packets[i];
                if(packet.type == PacketType_Audio)
                    continue;

                if(bRemovedPacket)
                {
                    if(packet.type >= packetWaitType)
                    {
                        packetWaitType = PacketType_VideoDisposable;
                        break;
                    }
                    else //clear out following dependent packets of lower priority
                    {
                        packet.data.Clear();
                        Packets.Remove(i--);
                        numVideoPackets--;
                        //numBFramesDumped++;
                    }
                }
                else if(packet.type == packetWaitType)
                {
                    packet.data.Clear();
                    Packets.Remove(i--);
                    numVideoPackets--;
                    //numBFramesDumped++;

                    bRemovedPacket = true;
                }
            }

            if(bRemovedPacket)
                break;

            packetWaitType++;
        }

        if(!bRemovedPacket)
            packetWaitType = prevWaitType;
    }
public:
    RTMPPublisher(RTMP *rtmpIn)
    {
        traceIn(RTMPPublisher::RTMPPublisher);

        rtmp = rtmpIn;

        hSendSempahore = CreateSemaphore(NULL, 0, 0x7FFFFFFFL, NULL);
        if(!hSendSempahore)
            CrashError(TEXT("RTMPPublisher: Could not create semaphore"));

        hDataMutex = OSCreateMutex();
        if(!hDataMutex)
            CrashError(TEXT("RTMPPublisher: Could not create mutex"));

        hSendThread = OSCreateThread((XTHREAD)RTMPPublisher::SendThread, this);
        if(!hSendThread)
            CrashError(TEXT("RTMPPublisher: Could not create thread"));

        packetWaitType = 0;

        BFrameThreshold = 10;
        maxVideoPackets = 50;

        traceOut;
    }

    ~RTMPPublisher()
    {
        traceIn(RTMPPublisher::~RTMPPublisher);

        bStopping = true;
        ReleaseSemaphore(hSendSempahore, 1, NULL);
        OSTerminateThread(hSendThread, 20000);

        CloseHandle(hSendSempahore);
        OSCloseMutex(hDataMutex);

        //--------------------------

        for(UINT i=0; i<Packets.Num(); i++)
            Packets[i].data.Clear();
        Packets.Clear();

        //Log(TEXT("num b frames dumped: %u"), numBFramesDumped);

        //--------------------------

        if(rtmp)
        {
            RTMP_Close(rtmp);
            RTMP_Free(rtmp);
        }

        traceOut;
    }

    void SendPacket(BYTE *data, UINT size, DWORD timestamp, PacketType type)
    {
        traceIn(RTMPPublisher::SendPacket);

        if(!bStopping)
        {
            List<BYTE> paddedData;
            paddedData.SetSize(size+RTMP_MAX_HEADER_SIZE);
            mcpy(paddedData.Array()+RTMP_MAX_HEADER_SIZE, data, size);

            if(type >= packetWaitType)
            {
                if(type != PacketType_Audio)
                {
                    packetWaitType = PacketType_VideoDisposable;

                    if(type <= PacketType_VideoHigh)
                        numVideoPackets++;
                }

                //-----------------

                OSEnterMutex(hDataMutex);

                NetworkPacket &netPacket = *Packets.CreateNew();
                netPacket.type = type;
                netPacket.timestamp = timestamp;
                netPacket.data.TransferFrom(paddedData);

                //begin dumping b frames if there's signs of lag
                if(numVideoPackets > BFrameThreshold)
                    DumpBFrame();

                if(numVideoPackets > maxVideoPackets)
                    DoIFrameDelay();

                OSLeaveMutex(hDataMutex);

                //-----------------

                ReleaseSemaphore(hSendSempahore, 1, NULL);
            }

            /*RTMPPacket packet;
            packet.m_nChannel = bAudio ? 0x5 : 0x4;
            packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
            packet.m_packetType = bAudio ? RTMP_PACKET_TYPE_AUDIO : RTMP_PACKET_TYPE_VIDEO;
            packet.m_nTimeStamp = timestamp;
            packet.m_nInfoField2 = rtmp->m_stream_id;
            packet.m_hasAbsTimestamp = TRUE;

            List<BYTE> paddedData;
            paddedData.SetSize(size+RTMP_MAX_HEADER_SIZE);
            mcpy(paddedData.Array()+RTMP_MAX_HEADER_SIZE, data, size);

            packet.m_nBodySize = size;
            packet.m_body = (char*)paddedData.Array()+RTMP_MAX_HEADER_SIZE;

            if(!RTMP_SendPacket(rtmp, &packet, FALSE))
            {
                if(!RTMP_IsConnected(rtmp))
                    App->PostStopMessage();
            }*/
        }

        traceOut;
    }

    void BeginPublishing()
    {
        traceIn(RTMPPublisher::BeginPublishing);

        RTMPPacket packet;

        char pbuf[2048], *pend = pbuf+sizeof(pbuf);

        packet.m_nChannel = 0x03;     // control channel (invoke)
        packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
        packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
        packet.m_nTimeStamp = 0;
        packet.m_nInfoField2 = rtmp->m_stream_id;
        packet.m_hasAbsTimestamp = TRUE;
        packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

        char *enc = packet.m_body;
        enc = AMF_EncodeString(enc, pend, &av_send);
        enc = AMF_EncodeNumber(enc, pend, ++rtmp->m_numInvokes);
        *enc++ = AMF_NULL;
        enc = AMF_EncodeString(enc, pend, &av_setDataFrame);
        enc = AMF_EncodeString(enc, pend, &av_onMetaData);
        enc = App->EncMetaData(enc, pend);

        packet.m_nBodySize = enc - packet.m_body;
        if(!RTMP_SendPacket(rtmp, &packet, FALSE))
        {
            App->PostStopMessage();
            return;
        }

        //----------------------------------------------

        packet.m_nChannel = 0x04; // source channel (video)
        packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
        packet.m_packetType = RTMP_PACKET_TYPE_VIDEO;

        List<BYTE> packetPadding;

        DataPacket mediaHeaders;
        App->GetVideoHeaders(mediaHeaders);

        packetPadding.SetSize(RTMP_MAX_HEADER_SIZE);
        packetPadding.AppendArray(mediaHeaders.lpPacket, mediaHeaders.size);

        packet.m_body = (char*)packetPadding.Array()+RTMP_MAX_HEADER_SIZE;
        packet.m_nBodySize = mediaHeaders.size;
        if(!RTMP_SendPacket(rtmp, &packet, FALSE))
        {
            App->PostStopMessage();
            return;
        }

        //----------------------------------------------

        packet.m_nChannel = 0x05;     // audio channel
        packet.m_packetType = RTMP_PACKET_TYPE_AUDIO;

        App->GetAudioHeaders(mediaHeaders);

        packetPadding.SetSize(RTMP_MAX_HEADER_SIZE);
        packetPadding.AppendArray(mediaHeaders.lpPacket, mediaHeaders.size);

        packet.m_body = (char*)packetPadding.Array()+RTMP_MAX_HEADER_SIZE;
        packet.m_nBodySize = mediaHeaders.size;
        if(!RTMP_SendPacket(rtmp, &packet, FALSE))
        {
            App->PostStopMessage();
            return;
        }

        traceOut;
    }

    double GetPacketStrain() const
    {
        return double(numVideoPackets)/double(maxVideoPackets)*100.0;
    }

    UINT GetBytesPerSec() const
    {
        return bytesPerSec;
    }
};

NetworkStream* CreateRTMPPublisher()
{
    traceIn(CreateRTMPPublisher);

    //------------------------------------------------------
    // set up URL

    String strURL;

    int    serviceID    = AppConfig->GetInt   (TEXT("Publish"), TEXT("Service"));
    String strServer    = AppConfig->GetString(TEXT("Publish"), TEXT("Server"));
    String strChannel   = AppConfig->GetString(TEXT("Publish"), TEXT("Channel"));
    String strPlayPath  = AppConfig->GetString(TEXT("Publish"), TEXT("PlayPath"));

    if(!strServer.IsValid())
    {
        MessageBox(hwndMain, TEXT("No server specified to connect to"), NULL, MB_ICONERROR);
        return NULL;
    }

    if(!strChannel.IsValid())
    {
        MessageBox(hwndMain, TEXT("No channel specified"), NULL, MB_ICONERROR);
        return NULL;
    }

    if(serviceID != 0)
    {
        XConfig serverData;
        if(!serverData.Open(TEXT("services.xconfig")))
        {
            MessageBox(hwndMain, TEXT("Could not open services.xconfig"), NULL, MB_ICONERROR);
            return NULL;
        }

        XElement *services = serverData.GetElement(TEXT("services"));
        if(!services)
        {
            MessageBox(hwndMain, TEXT("Could not any services in services.xconfig"), NULL, MB_ICONERROR);
            return NULL;
        }

        XElement *service = services->GetElementByID(serviceID-1);
        if(!service)
        {
            MessageBox(hwndMain, TEXT("Could not find the service specified in services.xconfig"), NULL, MB_ICONERROR);
            return NULL;
        }

        XElement *servers = service->GetElement(TEXT("servers"));
        if(!servers)
        {
            MessageBox(hwndMain, TEXT("Could not find any servers for the service specified in services.xconfig"), NULL, MB_ICONERROR);
            return NULL;
        }

        XDataItem *item = servers->GetDataItem(strServer);
        if(!item)
        {
            MessageBox(hwndMain, TEXT("Could not find any server specified for the service specified in services.xconfig"), NULL, MB_ICONERROR);
            return NULL;
        }

        strServer = item->GetData();
    }

    strURL << TEXT("rtmp://") << strServer << TEXT("/") << strChannel;
    if(strPlayPath.IsValid())
        strURL << TEXT("/") << strPlayPath;

    //------------------------------------------------------

    RTMP *rtmp = RTMP_Alloc();
    RTMP_Init(rtmp);
    /*RTMP_LogSetCallback(rtmp_log_output);
    RTMP_LogSetLevel(RTMP_LOGDEBUG2);*/

    LPSTR lpAnsiURL = strURL.CreateUTF8String();

    if(!RTMP_SetupURL(rtmp, lpAnsiURL))
    {
        MessageBox(hwndMain, TEXT("Could not parse URL"), NULL, MB_ICONERROR);
        RTMP_Free(rtmp);
        return NULL;
    }

    RTMP_EnableWrite(rtmp); //set it to publish

    if(!RTMP_Connect(rtmp, NULL))
    {
        MessageBox(hwndMain, TEXT("Could not connect"), NULL, MB_ICONERROR);
        RTMP_Free(rtmp);
        return NULL;
    }

    if(!RTMP_ConnectStream(rtmp, 0))
    {
        MessageBox(hwndMain, TEXT("Invalid stream channel or playpath"), NULL, MB_ICONERROR);
        RTMP_Close(rtmp);
        RTMP_Free(rtmp);
        return NULL;
    }

    Free(lpAnsiURL);

    return new RTMPPublisher(rtmp);

    traceOut;
}
