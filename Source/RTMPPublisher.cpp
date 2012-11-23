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


void rtmp_log_output(int level, const char *format, va_list vl)
{
    int size = _vscprintf(format, vl);
    LPSTR lpTemp = (LPSTR)Allocate(size+1);
    vsprintf_s(lpTemp, size+1, format, vl);

   // OSDebugOut(TEXT("%S\r\n"), lpTemp);
    Log(TEXT("%S\r\n"), lpTemp);

    Free(lpTemp);
}


RTMPPublisher::RTMPPublisher(RTMP *rtmpIn, BOOL bUseSendBuffer, UINT sendBufferSize)
{
    rtmp = rtmpIn;

    sendBuffer.SetSize(sendBufferSize);
    curSendBufferLen = 0;

    this->bUseSendBuffer = bUseSendBuffer;

    if(bUseSendBuffer)
    {
        Log(TEXT("Send Buffer Size: %u"), sendBufferSize);

        rtmp->m_customSendFunc = (CUSTOMSEND)RTMPPublisher::BufferedSend;
        rtmp->m_customSendParam = this;
        rtmp->m_bCustomSend = TRUE;
    }
    else
        Log(TEXT("Not using send buffering"));

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

    BFrameThreshold = App->GetFPS(); //when it starts cutting out b frames
    maxVideoPackets = App->GetFPS()*2; //when it starts cutting out p frames
    revertThreshold = 2;  //when it reverts to normal
}

RTMPPublisher::~RTMPPublisher()
{
    bStopping = true;
    ReleaseSemaphore(hSendSempahore, 1, NULL);
    OSTerminateThread(hSendThread, 20000);

    CloseHandle(hSendSempahore);
    OSCloseMutex(hDataMutex);

    //flush the last of the data
    if(bUseSendBuffer && RTMP_IsConnected(rtmp))
        FlushSendBuffer();

    //--------------------------

    for(UINT i=0; i<Packets.Num(); i++)
        Packets[i].data.Clear();
    Packets.Clear();

    Log(TEXT("Number of b-frames dropped: %u, Number of p-frames dropped: %u"), numBFramesDumped, numPFramesDumped);

    //--------------------------

    if(rtmp)
    {
        RTMP_Close(rtmp);
        RTMP_Free(rtmp);
    }

    sendBuffer.Clear();
}

void RTMPPublisher::SendPacket(BYTE *data, UINT size, DWORD timestamp, PacketType type)
{
    if(!bStopping)
    {
        List<BYTE> paddedData;
        paddedData.SetSize(size+RTMP_MAX_HEADER_SIZE);
        mcpy(paddedData.Array()+RTMP_MAX_HEADER_SIZE, data, size);

        if(bPacketDumpMode && Packets.Num() <= revertThreshold)
            bPacketDumpMode = false;

        bool bAddPacket = false;
        if(type >= packetWaitType)
        {
            if(type != PacketType_Audio)
            {
                packetWaitType = (bPacketDumpMode) ? PacketType_VideoLow : PacketType_VideoDisposable;

                if(type <= PacketType_VideoHigh)
                    numVideoPackets++;
            }

            bAddPacket = true;
        }

        if(bAddPacket)
        {
            OSEnterMutex(hDataMutex);

            NetworkPacket &netPacket = *Packets.CreateNew();
            netPacket.type = type;
            netPacket.timestamp = timestamp;
            netPacket.data.TransferFrom(paddedData);

            //begin dumping b frames if there's signs of lag
            if(numVideoPackets > BFrameThreshold)
            {
                if(!bPacketDumpMode)
                {
                    bPacketDumpMode = true;
                    if(packetWaitType == PacketType_VideoDisposable)
                        packetWaitType = PacketType_VideoLow;
                }

                DumpBFrame();
            }

            //begin dumping p frames if b frames aren't enough
            if(numVideoPackets > maxVideoPackets)
                DoIFrameDelay();

            OSLeaveMutex(hDataMutex);

            //-----------------

            ReleaseSemaphore(hSendSempahore, 1, NULL);
        }
        else
            numBFramesDumped++;

        /*RTMPPacket packet;
        packet.m_nChannel = 0x4;
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
}

void RTMPPublisher::BeginPublishing()
{
    RTMPPacket packet;

    char pbuf[2048], *pend = pbuf+sizeof(pbuf);

    packet.m_nChannel = 0x03;     // control channel (invoke)
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_INFO;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = rtmp->m_stream_id;
    packet.m_hasAbsTimestamp = TRUE;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    char *enc = packet.m_body;
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

    List<BYTE> packetPadding;
    DataPacket mediaHeaders;

    //----------------------------------------------

    packet.m_nChannel = 0x05; // source channel
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

    //----------------------------------------------

    packet.m_nChannel = 0x04; // source channel
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_VIDEO;

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
}

double RTMPPublisher::GetPacketStrain() const
{
    return double(numVideoPackets)/double(maxVideoPackets)*100.0;
}

QWORD RTMPPublisher::GetCurrentSentBytes()
{
    return bytesSent;
}

DWORD RTMPPublisher::NumDroppedFrames() const
{
    return numBFramesDumped+numPFramesDumped;
}


void RTMPPublisher::SendLoop()
{
    while(WaitForSingleObject(hSendSempahore, INFINITE) == WAIT_OBJECT_0 && !bStopping && RTMP_IsConnected(rtmp))
    {
        /*//--------------------------------------------
        // read

        DWORD pendingBytes = 0;
        ioctlsocket(rtmp->m_sb.sb_socket, FIONREAD, &pendingBytes);
        if(pendingBytes)
        {
            RTMPPacket packet;
            zero(&packet, sizeof(packet));

            while(RTMP_ReadPacket(rtmp, &packet) && !RTMPPacket_IsReady(&packet) && RTMP_IsConnected(rtmp));

            if(!RTMP_IsConnected(rtmp))
            {
                App->PostStopMessage();
                bStopping = true;
                break;
            }

            RTMPPacket_Free(&packet);
        }*/

        //--------------------------------------------
        // send

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
        if(type <= PacketType_VideoHigh)
            numVideoPackets--;

        OSLeaveMutex(hDataMutex);

        //--------------------------------------------

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

        //make sure to flush the send buffer if surpassing max latency to keep frame data synced up on the server
        if(bUseSendBuffer && type != PacketType_Audio)
        {
            if(!numVideoPacketsBuffered)
                firstBufferedVideoFrameTimestamp = timestamp;

            DWORD bufferedTime = timestamp-firstBufferedVideoFrameTimestamp;
            if(bufferedTime > maxBufferTime)
            {
                if(!FlushSendBuffer())
                {
                    App->PostStopMessage();
                    bStopping = true;
                    break;
                }

                firstBufferedVideoFrameTimestamp = timestamp;
            }

            numVideoPacketsBuffered++;
        }

        bytesSent += packetData.Num();
    }
}

DWORD RTMPPublisher::SendThread(RTMPPublisher *publisher)
{
    publisher->SendLoop();
    return 0;
}

//video packet count exceeding maximum.  find lowest priority frame to dump
void RTMPPublisher::DoIFrameDelay()
{
    int prevWaitType = packetWaitType;

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
                else 
                    packetWaitType = prevWaitType;

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
                        numPFramesDumped++;
                    }
                }
                else if(packet.type == packetWaitType)
                {
                    packet.data.Clear();
                    Packets.Remove(i--);
                    numVideoPackets--;
                    numPFramesDumped++;

                    bRemovedPacket = true;
                }
            }

            if(bRemovedPacket)
                break;
        }

        packetWaitType++;
    }
}

void RTMPPublisher::DumpBFrame()
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
                    packetWaitType = (bPacketDumpMode) ? PacketType_VideoLow : PacketType_VideoDisposable;
                    break;
                }
                else //clear out following dependent packets of lower priority
                {
                    packet.data.Clear();
                    Packets.Remove(i--);
                    numVideoPackets--;
                    numBFramesDumped++;
                }
            }
            else if(packet.type == packetWaitType)
            {
                packet.data.Clear();
                Packets.Remove(i--);
                numVideoPackets--;
                numBFramesDumped++;

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

int RTMPPublisher::FlushSendBuffer()
{
    if(!curSendBufferLen)
        return 1;

    SOCKET sb_socket = rtmp->m_sb.sb_socket;

    BYTE *lpTemp = sendBuffer.Array();
    int totalBytesSent = curSendBufferLen;
    while(totalBytesSent > 0)
    {
        int nBytes = send(sb_socket, (const char*)lpTemp, totalBytesSent, 0);
        if(nBytes < 0)
            return nBytes;
        if(nBytes == 0)
            return 0;

        totalBytesSent -= nBytes;
        lpTemp += nBytes;
    }

    int prevSendBufferSize = curSendBufferLen;
    curSendBufferLen = 0;

    return prevSendBufferSize;
}

int RTMPPublisher::BufferedSend(RTMPSockBuf *sb, const char *buf, int len, RTMPPublisher *network)
{
    bool bComplete = false;
    int fullLen = len;

    do 
    {
        int newTotal = network->curSendBufferLen+len;

        //buffer full, send
        if(newTotal >= int(network->sendBuffer.Num()))
        {
            int pendingBytes = newTotal-network->sendBuffer.Num();
            int copyCount    = network->sendBuffer.Num()-network->curSendBufferLen;

            mcpy(network->sendBuffer.Array()+network->curSendBufferLen, buf, copyCount);

            BYTE *lpTemp = network->sendBuffer.Array();
            int totalBytesSent = network->sendBuffer.Num();
            while(totalBytesSent > 0)
            {
                int nBytes = send(sb->sb_socket, (const char*)lpTemp, totalBytesSent, 0);
                if(nBytes < 0)
                    return nBytes;
                if(nBytes == 0)
                    return 0;

                totalBytesSent -= nBytes;
                lpTemp += nBytes;
            }

            network->curSendBufferLen = 0;

            if(pendingBytes)
            {
                buf += copyCount;
                len -= copyCount;
            }
            else
                bComplete = true;
        }
        else
        {
            if(len)
            {
                mcpy(network->sendBuffer.Array()+network->curSendBufferLen, buf, len);
                network->curSendBufferLen = newTotal;
            }

            bComplete = true;
        }
    } while(!bComplete);

    return fullLen;
}


NetworkStream* CreateRTMPPublisher(String &failReason, bool &bCanRetry)
{
    //------------------------------------------------------
    // set up URL

    bCanRetry = false;

    int    serviceID    = AppConfig->GetInt   (TEXT("Publish"), TEXT("Service"));
    String strURL       = AppConfig->GetString(TEXT("Publish"), TEXT("URL"));
    String strPlayPath  = AppConfig->GetString(TEXT("Publish"), TEXT("PlayPath"));

    if(!strURL.IsValid())
    {
        failReason = TEXT("No server specified to connect to");
        return NULL;
    }

    if(serviceID != 0)
    {
        XConfig serverData;
        if(!serverData.Open(TEXT("services.xconfig")))
        {
            failReason = TEXT("Could not open services.xconfig");
            return NULL;
        }

        XElement *services = serverData.GetElement(TEXT("services"));
        if(!services)
        {
            failReason = TEXT("Could not any services in services.xconfig");
            return NULL;
        }

        XElement *service = services->GetElementByID(serviceID-1);
        if(!service)
        {
            failReason = TEXT("Could not find the service specified in services.xconfig");
            return NULL;
        }

        XElement *servers = service->GetElement(TEXT("servers"));
        if(!servers)
        {
            failReason = TEXT("Could not find any servers for the service specified in services.xconfig");
            return NULL;
        }

        XDataItem *item = servers->GetDataItem(strURL);
        if(!item)
            item = servers->GetDataItemByID(0);

        strURL = item->GetData();
    }

    //------------------------------------------------------

    RTMP *rtmp = RTMP_Alloc();
    RTMP_Init(rtmp);
    /*RTMP_LogSetCallback(rtmp_log_output);
    RTMP_LogSetLevel(RTMP_LOGDEBUG2);*/

    LPSTR lpAnsiURL = strURL.CreateUTF8String();
    LPSTR lpAnsiPlaypath = strPlayPath.CreateUTF8String();

    if(!RTMP_SetupURL2(rtmp, lpAnsiURL, lpAnsiPlaypath))
    {
        failReason = Str("Connection.CouldNotParseURL");
        RTMP_Free(rtmp);
        Free(lpAnsiURL);
        Free(lpAnsiPlaypath);
        return NULL;
    }

    RTMP_EnableWrite(rtmp); //set it to publish

    /*rtmp->Link.swfUrl.av_len = rtmp->Link.tcUrl.av_len;
    rtmp->Link.swfUrl.av_val = rtmp->Link.tcUrl.av_val;
    rtmp->Link.pageUrl.av_len = rtmp->Link.tcUrl.av_len;
    rtmp->Link.pageUrl.av_val = rtmp->Link.tcUrl.av_val;*/
    rtmp->Link.flashVer.av_val = "FMLE/3.0 (compatible; FMSc/1.0)";
    rtmp->Link.flashVer.av_len = (int)strlen(rtmp->Link.flashVer.av_val);

    BOOL bUseSendBuffer = AppConfig->GetInt(TEXT("Publish"), TEXT("UseSendBuffer"), 1);
    UINT sendBufferSize = AppConfig->GetInt(TEXT("Publish"), TEXT("SendBufferSize"), 1460);

    if(sendBufferSize > 32120)
        sendBufferSize = 32120;
    else if(sendBufferSize < 536)
        sendBufferSize = 536;

    rtmp->m_outChunkSize = 4096;//RTMP_DEFAULT_CHUNKSIZE;//
    rtmp->m_bSendChunkSizeInfo = TRUE;

    if (!bUseSendBuffer)
        rtmp->m_bUseNagle = TRUE;

    if(!RTMP_Connect(rtmp, NULL))
    {
        failReason = Str("Connection.CouldNotConnect");
        RTMP_Free(rtmp);
        bCanRetry = true;
        Free(lpAnsiURL);
        Free(lpAnsiPlaypath);
        return NULL;
    }

    if(!RTMP_ConnectStream(rtmp, 0))
    {
        failReason = Str("Connection.InvalidStream");
        RTMP_Close(rtmp);
        RTMP_Free(rtmp);
        Free(lpAnsiURL);
        Free(lpAnsiPlaypath);
        return NULL;
    }

    Free(lpAnsiURL);
    Free(lpAnsiPlaypath);

    return new RTMPPublisher(rtmp, bUseSendBuffer, sendBufferSize);
}
