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

#define MAX_BUFFERED_PACKETS 10

String RTMPPublisher::strRTMPErrors;

//QWORD totalCalls = 0, totalTime = 0;

void rtmp_log_output(int level, const char *format, va_list vl)
{
    int size = _vscprintf(format, vl);
    LPSTR lpTemp = (LPSTR)Allocate(size+1);
    vsprintf_s(lpTemp, size+1, format, vl);

   // OSDebugOut(TEXT("%S\r\n"), lpTemp);
    Log(TEXT("%S\r\n"), lpTemp);

    Free(lpTemp);
}

#ifdef _DEBUG
DWORD quickHash (BYTE *data, UINT len)
{
    DWORD hash = 276277;

    for (unsigned i=0; i<len; ++i) hash = 33*hash + data[i];
    return hash;
}
#endif

void RTMPPublisher::librtmpErrorCallback(int level, const char *format, va_list vl)
{
    char ansiStr[1024];
    TCHAR logStr[1024];

    if (level > RTMP_LOGERROR)
        return;

    vsnprintf(ansiStr, sizeof(ansiStr)-1, format, vl);
    ansiStr[sizeof(ansiStr)-1] = 0;

    MultiByteToWideChar(CP_UTF8, 0, ansiStr, -1, logStr, _countof(logStr)-1);

    Log (TEXT("librtmp error: %s"), logStr);

    strRTMPErrors << logStr << TEXT("\n");
}

String RTMPPublisher::GetRTMPErrors()
{
    return strRTMPErrors;
}

RTMPPublisher::RTMPPublisher()
{
    //bufferedPackets.SetBaseSize(MAX_BUFFERED_PACKETS);

    bFirstKeyframe = true;

    hSendSempahore = CreateSemaphore(NULL, 0, 0x7FFFFFFFL, NULL);
    if(!hSendSempahore)
        CrashError(TEXT("RTMPPublisher: Could not create semaphore"));

    hDataMutex = OSCreateMutex();
    if(!hDataMutex)
        CrashError(TEXT("RTMPPublisher: Could not create mutex"));

    hRTMPMutex = OSCreateMutex();

    //------------------------------------------

    bframeDropThreshold = AppConfig->GetInt(TEXT("Publish"), TEXT("BFrameDropThreshold"), 400);
    if(bframeDropThreshold < 50)        bframeDropThreshold = 50;
    else if(bframeDropThreshold > 1000) bframeDropThreshold = 1000;

    dropThreshold = AppConfig->GetInt(TEXT("Publish"), TEXT("FrameDropThreshold"), 600);
    if(dropThreshold < 50)        dropThreshold = 50;
    else if(dropThreshold > 1000) dropThreshold = 1000;

    if (AppConfig->GetInt(TEXT("Publish"), TEXT("LowLatencyMode"), 0))
    {
        if (AppConfig->GetInt(TEXT("Publish"), TEXT("LowLatencyMethod"), 0) == 0)
        {
            latencyFactor = AppConfig->GetInt(TEXT("Publish"), TEXT("LatencyFactor"), 20);

            if (latencyFactor < 3)
                latencyFactor = 3;

            lowLatencyMode = LL_MODE_FIXED;
            Log(TEXT("Using fixed low latency mode, factor %d"), latencyFactor);
        }
        else
        {
            lowLatencyMode = LL_MODE_AUTO;
            Log(TEXT("Using automatic low latency mode"));
        }
    }
    else
        lowLatencyMode = LL_MODE_NONE;
    
    bFastInitialKeyframe = AppConfig->GetInt(TEXT("Publish"), TEXT("FastInitialKeyframe"), 0) == 1;

    strRTMPErrors.Clear();
}

bool RTMPPublisher::Init(UINT tcpBufferSize)
{
    //------------------------------------------

    //Log(TEXT("Using Send Buffer Size: %u"), sendBufferSize);

    rtmp->m_customSendFunc = (CUSTOMSEND)RTMPPublisher::BufferedSend;
    rtmp->m_customSendParam = this;
    rtmp->m_bCustomSend = TRUE;

    //------------------------------------------

    int curTCPBufSize, curTCPBufSizeSize = sizeof(curTCPBufSize);
    
    if (!getsockopt(rtmp->m_sb.sb_socket, SOL_SOCKET, SO_SNDBUF, (char *)&curTCPBufSize, &curTCPBufSizeSize))
    {
        Log(TEXT("SO_SNDBUF was at %u"), curTCPBufSize);

        if (curTCPBufSize < int(tcpBufferSize))
        {
            if (!setsockopt(rtmp->m_sb.sb_socket, SOL_SOCKET, SO_SNDBUF, (const char *)&tcpBufferSize, sizeof(tcpBufferSize)))
            {
                if (!getsockopt(rtmp->m_sb.sb_socket, SOL_SOCKET, SO_SNDBUF, (char *)&curTCPBufSize, &curTCPBufSizeSize))
                {
                    if (curTCPBufSize != tcpBufferSize)
                        Log(TEXT("Could not raise SO_SNDBUF to %u, value is now %d"), tcpBufferSize, curTCPBufSize);

                    Log(TEXT("SO_SNDBUF is now %d"), curTCPBufSize);
                }
                else
                {
                    Log(TEXT("getsockopt: Failed to query SO_SNDBUF, error %d"), WSAGetLastError());
                }
            }
            else
            {
                Log(TEXT("setsockopt: Failed to raise SO_SNDBUF to %u, error %d"), tcpBufferSize, WSAGetLastError());
            }
        }
    }
    else
    {
        Log(TEXT("getsockopt: Failed to query SO_SNDBUF, error %d"), WSAGetLastError());
    }

    //------------------------------------------

    hSendThread = OSCreateThread((XTHREAD)RTMPPublisher::SendThread, this);
    if(!hSendThread)
        CrashError(TEXT("RTMPPublisher: Could not create send thread"));

    hBufferEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    hBufferSpaceAvailableEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    hSendLoopExit = CreateEvent(NULL, TRUE, FALSE, NULL);
    hSocketLoopExit = CreateEvent(NULL, TRUE, FALSE, NULL);
    hSendBacklogEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    hDataBufferMutex = OSCreateMutex();

    dataBuffer = (BYTE *)Allocate(dataBufferSize);

    hSocketThread = OSCreateThread((XTHREAD)RTMPPublisher::SocketThread, this);
    if(!hSocketThread)
        CrashError(TEXT("RTMPPublisher: Could not create send thread"));

    //------------------------------------------

    packetWaitType = 0;

    return true;
}

void RTMPPublisher::InitEncoderData()
{
    if (encoderDataInitialized)
        return;

    encoderDataInitialized = true;

    dataBufferSize = (App->GetVideoEncoder()->GetBitRate() + App->GetAudioEncoder()->GetBitRate()) / 8 * 1024;
    if (dataBufferSize < 131072)
        dataBufferSize = 131072;

    metaDataPacketBuffer.resize(2048);

    char *enc = metaDataPacketBuffer.data() + RTMP_MAX_HEADER_SIZE;
    char *pend = metaDataPacketBuffer.data() + metaDataPacketBuffer.size();
    enc = AMF_EncodeString(enc, pend, &av_setDataFrame);
    enc = AMF_EncodeString(enc, pend, &av_onMetaData);
    enc = App->EncMetaData(enc, pend);
    metaDataPacketBuffer.resize(enc - metaDataPacketBuffer.data());

    App->GetAudioHeaders(audioHeaders);

    App->GetVideoHeaders(videoHeaders);
}

RTMPPublisher::~RTMPPublisher()
{
    //OSDebugOut (TEXT("*** ~RTMPPublisher (%d queued, %d buffered, %d data)\n"), queuedPackets.Num(), bufferedPackets.Num(), curDataBufferLen);
    bStopping = true;

    //we're in the middle of connecting! wait for that to happen to avoid all manner of race conditions
    if (hConnectionThread)
    {
        //the connect thread could be stalled in a blocking call, kill the socket to ensure it wakes up
        if (WaitForSingleObject(hConnectionThread, 0) == WAIT_TIMEOUT)
        {
            OSEnterMutex(hRTMPMutex);
            if (rtmp && rtmp->m_sb.sb_socket != -1)
            {
                closesocket(rtmp->m_sb.sb_socket);
                rtmp->m_sb.sb_socket = -1;
            }
            OSLeaveMutex(hRTMPMutex);
        }

        WaitForSingleObject(hConnectionThread, INFINITE);
        OSCloseThread(hConnectionThread);
    }

    DWORD startTime = OSGetTime();

    //send all remaining buffered packets, this may block since it respects timestamps
    FlushBufferedPackets ();

    Log(TEXT("~RTMPPublisher: Packet flush completed in %d ms"), OSGetTime() - startTime);

    //OSDebugOut (TEXT("%d queued after flush\n"), queuedPackets.Num());

    if(hSendThread)
    {
        startTime = OSGetTime();

        //this marks the thread to exit after current work is done
        SetEvent(hSendLoopExit);

        //these wake up the thread
        ReleaseSemaphore(hSendSempahore, 1, NULL);
        SetEvent(hBufferSpaceAvailableEvent);

        //wait 50 sec for all data to finish sending
        if (WaitForSingleObject(hSendThread, 50000) == WAIT_TIMEOUT)
        {
            Log(TEXT("~RTMPPublisher: Network appears stalled with %d / %d buffered, dropping connection!"), curDataBufferLen, dataBufferSize);
            FatalSocketShutdown();

            //this will wake up and flush the sendloop if it's still trying to send out stuff
            ReleaseSemaphore(hSendSempahore, 1, NULL);
            SetEvent(hBufferSpaceAvailableEvent);
        }

        OSTerminateThread(hSendThread, 10000);

        Log(TEXT("~RTMPPublisher: Send thread terminated in %d ms"), OSGetTime() - startTime);
    }

    if(hSendSempahore)
        CloseHandle(hSendSempahore);

    //OSDebugOut (TEXT("*** ~RTMPPublisher hSendThread terminated (%d queued, %d buffered, %d data)\n"), queuedPackets.Num(), bufferedPackets.Num(), curDataBufferLen);

    if (hSocketThread)
    {
        startTime = OSGetTime();

        //mark the socket loop to shut down after the buffer is empty
        SetEvent(hSocketLoopExit);

        //wake it up in case it already is empty
        SetEvent(hBufferEvent);

        //wait 60 sec for it to exit
        OSTerminateThread(hSocketThread, 60000);

        Log(TEXT("~RTMPPublisher: Socket thread terminated in %d ms"), OSGetTime() - startTime);
    }

    //OSDebugOut (TEXT("*** ~RTMPPublisher hSocketThread terminated (%d queued, %d buffered, %d data)\n"), queuedPackets.Num(), bufferedPackets.Num(), curDataBufferLen);

    if(rtmp)
    {
        if (RTMP_IsConnected(rtmp))
        {
            startTime = OSGetTime();

            //at this point nothing should be in the buffer, flush out what remains to the net and make it blocking
            FlushDataBuffer();

            //disable the buffered send, so RTMP_* functions write directly to the net (and thus block)
            rtmp->m_bCustomSend = 0;

            //manually shut down the stream and issue a graceful socket shutdown
            RTMP_DeleteStream(rtmp);

            shutdown(rtmp->m_sb.sb_socket, SD_SEND);

            //this waits for the socket shutdown to complete gracefully
            for (;;)
            {
                char buff[1024];
                int ret;

                ret = recv(rtmp->m_sb.sb_socket, buff, sizeof(buff), 0);
                if (!ret)
                    break;
                else if (ret == -1)
                {
                    Log(TEXT("~RTMPublisher: Received error %d while waiting for graceful shutdown."), WSAGetLastError());
                    break;
                }
            }

            Log(TEXT("~RTMPPublisher: Final socket shutdown completed in %d ms"), OSGetTime() - startTime);

            //OSDebugOut(TEXT("Graceful shutdown complete.\n"));
        }

        //this closes the socket if not already done
        RTMP_Close(rtmp);
    }

    if(hDataMutex)
        OSCloseMutex(hDataMutex);

    while (bufferedPackets.Num())
    {
        //this should not happen any more...
        bufferedPackets[0].data.Clear();
        bufferedPackets.Remove(0);
    }

    if (dataBuffer)
        Free(dataBuffer);

    if (hDataBufferMutex)
        OSCloseMutex(hDataBufferMutex);

    if (hBufferEvent)
        CloseHandle(hBufferEvent);

    if (hSendLoopExit)
        CloseHandle(hSendLoopExit);

    if (hSocketLoopExit)
        CloseHandle(hSocketLoopExit);

    if (hSendBacklogEvent)
        CloseHandle(hSendBacklogEvent);

    if (hBufferSpaceAvailableEvent)
        CloseHandle(hBufferSpaceAvailableEvent);

    if (hWriteEvent)
        CloseHandle(hWriteEvent);

    if(rtmp)
    {
        if (rtmp->Link.pubUser.av_val)
            Free(rtmp->Link.pubUser.av_val);
        if (rtmp->Link.pubPasswd.av_val)
            Free(rtmp->Link.pubPasswd.av_val);
        RTMP_Free(rtmp);
    }

    //--------------------------

    for(UINT i=0; i<queuedPackets.Num(); i++)
        queuedPackets[i].data.Clear();
    queuedPackets.Clear();

    double dBFrameDropPercentage = double(numBFramesDumped)/max(1, NumTotalVideoFrames())*100.0;
    double dPFrameDropPercentage = double(numPFramesDumped)/max(1, NumTotalVideoFrames())*100.0;

    if (totalSendCount)
        Log(TEXT("Average send payload: %d bytes, average send interval: %d ms"), (DWORD)(totalSendBytes / totalSendCount), totalSendPeriod / totalSendCount);

    Log(TEXT("Number of times waited to send: %d, Waited for a total of %d bytes"), totalTimesWaited, totalBytesWaited);

    Log(TEXT("Number of b-frames dropped: %u (%0.2g%%), Number of p-frames dropped: %u (%0.2g%%), Total %u (%0.2g%%)"),
        numBFramesDumped, dBFrameDropPercentage,
        numPFramesDumped, dPFrameDropPercentage,
        numBFramesDumped+numPFramesDumped, dBFrameDropPercentage+dPFrameDropPercentage);

    Log(TEXT("Number of bytes sent: %llu"), totalSendBytes);


    /*if(totalCalls)
        Log(TEXT("average send time: %u"), totalTime/totalCalls);*/

    strRTMPErrors.Clear();

    //--------------------------
}

UINT RTMPPublisher::FindClosestQueueIndex(DWORD timestamp)
{
    UINT index;
    for (index=0; index<queuedPackets.Num(); index++) {
        if (queuedPackets[index].timestamp > timestamp)
            break;
    }

    return index;
}

UINT RTMPPublisher::FindClosestBufferIndex(DWORD timestamp)
{
    UINT index;
    for (index=0; index<bufferedPackets.Num(); index++) {
        if (bufferedPackets[index].timestamp > timestamp)
            break;
    }

    return index;
}

void RTMPPublisher::InitializeBuffer()
{
    bool bFirstAudio = true;
    for (UINT i=0; i<bufferedPackets.Num(); i++) {
        TimedPacket &packet = bufferedPackets[i];

        //first, get the audio time offset from the first audio packet
        if (packet.type == PacketType_Audio) {
            if (bFirstAudio) {
                audioTimeOffset = packet.timestamp;
                OSDebugOut(TEXT("Set audio offset: %d\n"), audioTimeOffset);
                bFirstAudio = false;
            }

            DWORD newTimestamp = packet.timestamp-audioTimeOffset;

            UINT newIndex = FindClosestBufferIndex(newTimestamp);
            if (newIndex < i) {
                bufferedPackets.MoveItem(i, newIndex);
                bufferedPackets[newIndex].timestamp = newTimestamp;
            } else {
                bufferedPackets[i].timestamp = newTimestamp;
            }
        }
    }
}

void RTMPPublisher::FlushBufferedPackets()
{
    if (!bufferedPackets.Num())
        return;

    QWORD startTime = GetQPCTimeMS();
    DWORD baseTimestamp = bufferedPackets[0].timestamp;

    for (unsigned int i = 0; i < bufferedPackets.Num(); i++)
    {
        TimedPacket &packet = bufferedPackets[i];

        QWORD curTime;
        do
        {
            curTime = GetQPCTimeMS();
            OSSleep (1);
        } while (curTime - startTime < packet.timestamp - baseTimestamp);

        SendPacketForReal(packet.data.Array(), packet.data.Num(), packet.timestamp, packet.type);

        packet.data.Clear();
    }

    bufferedPackets.Clear();
}

void RTMPPublisher::ProcessPackets()
{
    if(!bStreamStarted && !bStopping)
    {
        BeginPublishingInternal();
        bStreamStarted = true;
    }

    //never drop frames if we're in the shutdown sequence, just wait it out
    if (!bStopping)
    {
        if (queuedPackets.Num() && minFramedropTimestsamp < queuedPackets[0].timestamp)
        {
            DWORD queueDuration = (queuedPackets.Last().timestamp - queuedPackets[0].timestamp);

            DWORD curTime = OSGetTime();

            if (queueDuration >= dropThreshold + audioTimeOffset)
            {
                minFramedropTimestsamp = queuedPackets.Last().timestamp;

                OSDebugOut(TEXT("dropped all at %u, threshold is %u, total duration is %u, %d in queue\r\n"), currentBufferSize, dropThreshold + audioTimeOffset, queueDuration, queuedPackets.Num());

                //what the hell, just flush it all for now as a test and force a keyframe 1 second after
                while (DoIFrameDelay(false));

                if(packetWaitType > PacketType_VideoLow)
                    RequestKeyframe(1000);
            }
            else if (queueDuration >= bframeDropThreshold + audioTimeOffset && curTime-lastBFrameDropTime >= dropThreshold + audioTimeOffset)
            {
                OSDebugOut(TEXT("dropped b-frames at %u, threshold is %u, total duration is %u\r\n"), currentBufferSize, bframeDropThreshold + audioTimeOffset, queueDuration);

                while (DoIFrameDelay(true));

                lastBFrameDropTime = curTime;
            }
        }
    }

    if(queuedPackets.Num())
        ReleaseSemaphore(hSendSempahore, 1, NULL);
}

void RTMPPublisher::SendPacket(BYTE *data, UINT size, DWORD timestamp, PacketType type)
{
    InitEncoderData();

    if(!bConnected && !bConnecting && !bStopping)
    {
        hConnectionThread = OSCreateThread((XTHREAD)CreateConnectionThread, this);
        bConnecting = true;
    }

    if (bFastInitialKeyframe)
    {
        if (!bConnected)
        {
            //while not connected, keep at most one keyframe buffered
            if (type != PacketType_VideoHighest)
                return;
        
            bufferedPackets.Clear();
        }

        if (bConnected && bFirstKeyframe)
        {
            bFirstKeyframe = false;
            firstTimestamp = timestamp;

            //send out our buffered keyframe immediately, unless this packet happens to also be a keyframe
            if (type != PacketType_VideoHighest && bufferedPackets.Num() == 1)
            {
                TimedPacket packet;
                mcpy(&packet, &bufferedPackets[0], sizeof(TimedPacket));
                bufferedPackets.Remove(0);
                packet.timestamp = 0;

                SendPacketForReal(packet.data.Array(), packet.data.Num(), packet.timestamp, packet.type);
            }
            else
                bufferedPackets.Clear();
        }
    }
    else
    {
        if (bFirstKeyframe)
        {
            if (!bConnected || type != PacketType_VideoHighest)
                return;

            firstTimestamp = timestamp;
            bFirstKeyframe = false;
        }
    }

    //OSDebugOut (TEXT("%u: SendPacket (%d bytes - %08x @ %u)\n"), OSGetTime(), size, quickHash(data,size), timestamp);

    if (bufferedPackets.Num() == MAX_BUFFERED_PACKETS)
    {
        if (!bBufferFull)
        {
            InitializeBuffer();
            bBufferFull = true;
        }

        TimedPacket packet;
        mcpy(&packet, &bufferedPackets[0], sizeof(TimedPacket));
        bufferedPackets.Remove(0);

        SendPacketForReal(packet.data.Array(), packet.data.Num(), packet.timestamp, packet.type);
    }

    timestamp -= firstTimestamp;

    TimedPacket *packet;
    
    if (type == PacketType_Audio)
    {
        UINT newID;
        timestamp -= audioTimeOffset;

        newID = FindClosestBufferIndex(timestamp);
        packet = bufferedPackets.InsertNew(newID);
    }
    else
    {
        packet = bufferedPackets.CreateNew();
    }

    packet->data.CopyArray(data, size);
    packet->timestamp = timestamp;
    packet->type = type;

    /*for (UINT i=0; i<bufferedPackets.Num(); i++)
    {
        if (bufferedPackets[i].data.Array() == 0)
            nop();
    }*/
}

void RTMPPublisher::SendPacketForReal(BYTE *data, UINT size, DWORD timestamp, PacketType type)
{
    //OSDebugOut (TEXT("%u: SendPacketForReal (%d bytes - %08x @ %u, type %d)\n"), OSGetTime(), size, quickHash(data,size), timestamp, type);
    //Log(TEXT("packet| timestamp: %u, type: %u, bytes: %u"), timestamp, (UINT)type, size);

    OSEnterMutex(hDataMutex);

    if(bConnected)
    {
        ProcessPackets();

        bool bSend = bSentFirstKeyframe;

        if(!bSentFirstKeyframe)
        {
            if(type == PacketType_VideoHighest)
            {
                bSend = true;

                OSDebugOut(TEXT("got keyframe: %u\r\n"), OSGetTime());
            }
        }

        if(bSend)
        {
            if(!bSentFirstAudio && type == PacketType_Audio)
            {
                timestamp = 0;
                bSentFirstAudio = true;
            }

            totalFrames++;
            if(type != PacketType_Audio)
                totalVideoFrames++;

            bool bAddPacket = false;
            if(type >= packetWaitType)
            {
                if(type != PacketType_Audio)
                    packetWaitType = PacketType_VideoDisposable;

                bAddPacket = true;
            }

            if(bAddPacket)
            {
                List<BYTE> paddedData;
                paddedData.SetSize(size+RTMP_MAX_HEADER_SIZE);
                mcpy(paddedData.Array()+RTMP_MAX_HEADER_SIZE, data, size);

                if(!bSentFirstKeyframe)
                {
                    DataPacket sei;
                    App->GetVideoEncoder()->GetSEI(sei);
                    paddedData.InsertArray(RTMP_MAX_HEADER_SIZE+5, sei.lpPacket, sei.size);

                    bSentFirstKeyframe = true;
                }

                currentBufferSize += paddedData.Num();

                UINT droppedFrameVal = queuedPackets.Num() ? queuedPackets.Last().distanceFromDroppedFrame+1 : 10000;

                UINT id = FindClosestQueueIndex(timestamp);

                NetworkPacket *queuedPacket = queuedPackets.InsertNew(id);
                queuedPacket->distanceFromDroppedFrame = droppedFrameVal;
                queuedPacket->data.TransferFrom(paddedData);
                queuedPacket->timestamp = timestamp;
                queuedPacket->type = type;
            }
            else
            {
                if(type < PacketType_VideoHigh)
                    numBFramesDumped++;
                else
                    numPFramesDumped++;
            }
        }
    }

    OSLeaveMutex(hDataMutex);
}

void RTMPPublisher::BeginPublishingInternal()
{
    RTMPPacket packet;

    packet.m_nChannel = 0x03;     // control channel (invoke)
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_INFO;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = rtmp->m_stream_id;
    packet.m_hasAbsTimestamp = TRUE;
    packet.m_body = metaDataPacketBuffer.data() + RTMP_MAX_HEADER_SIZE;

    packet.m_nBodySize = metaDataPacketBuffer.size() - RTMP_MAX_HEADER_SIZE;
    if(!RTMP_SendPacket(rtmp, &packet, FALSE))
    {
        App->PostStopMessage();
        return;
    }

    //----------------------------------------------

    List<BYTE> packetPadding;

    //----------------------------------------------

    packet.m_nChannel = 0x05; // source channel
    packet.m_packetType = RTMP_PACKET_TYPE_AUDIO;

    packetPadding.SetSize(RTMP_MAX_HEADER_SIZE);
    packetPadding.AppendArray(audioHeaders.lpPacket, audioHeaders.size);

    packet.m_body = (char*)packetPadding.Array()+RTMP_MAX_HEADER_SIZE;
    packet.m_nBodySize = audioHeaders.size;
    if(!RTMP_SendPacket(rtmp, &packet, FALSE))
    {
        App->PostStopMessage();
        return;
    }

    //----------------------------------------------

    packet.m_nChannel = 0x04; // source channel
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_VIDEO;


    packetPadding.SetSize(RTMP_MAX_HEADER_SIZE);
    packetPadding.AppendArray(videoHeaders.lpPacket, videoHeaders.size);

    packet.m_body = (char*)packetPadding.Array()+RTMP_MAX_HEADER_SIZE;
    packet.m_nBodySize = videoHeaders.size;
    if(!RTMP_SendPacket(rtmp, &packet, FALSE))
    {
        App->PostStopMessage();
        return;
    }
}

void RTMPPublisher::BeginPublishing()
{
}

void LogInterfaceType (RTMP *rtmp)
{
    MIB_IPFORWARDROW    route;
    DWORD               destAddr, sourceAddr;
    CHAR                hostname[256];

    if (rtmp->Link.hostname.av_len >= sizeof(hostname)-1)
        return;

    strncpy (hostname, rtmp->Link.hostname.av_val, sizeof(hostname)-1);
    hostname[rtmp->Link.hostname.av_len] = 0;

    HOSTENT *h = gethostbyname(hostname);
    if (!h)
        return;

    destAddr = *(DWORD *)h->h_addr_list[0];

    if (rtmp->m_bindIP.addrLen == 0)
        sourceAddr = 0;
    else if (rtmp->m_bindIP.addr.ss_family == AF_INET)
        sourceAddr = (*(struct sockaddr_in *)&rtmp->m_bindIP).sin_addr.S_un.S_addr;
    else
        return; // getting route for IPv6 is far more complex, ignore for now

    if (!GetBestRoute (destAddr, sourceAddr, &route))
    {
        MIB_IFROW row;
        zero (&row, sizeof(row));
        row.dwIndex = route.dwForwardIfIndex;

        if (!GetIfEntry (&row))
        {
            DWORD speed = row.dwSpeed / 1000000;
            TCHAR *type;
            String otherType;

            if (row.dwType == IF_TYPE_ETHERNET_CSMACD)
                type = TEXT("ethernet");
            else if (row.dwType == IF_TYPE_IEEE80211)
                type = TEXT("802.11");
            else
            {
                otherType = FormattedString (TEXT("type %d"), row.dwType);
                type = otherType.Array();
            }

            Log (TEXT("  Interface: %S (%s, %d mbps)"), row.bDescr, type, speed);
        }
    }
}

DWORD WINAPI RTMPPublisher::CreateConnectionThread(RTMPPublisher *publisher)
{
    //------------------------------------------------------
    // set up URL

    bool bSuccess = false;
    bool bCanRetry = false;

    RTMP *rtmp = nullptr;

    String failReason;
    String strBindIP;

    String strURL       = AppConfig->GetString(TEXT("Publish"), TEXT("URL"));
    String strPlayPath  = AppConfig->GetString(TEXT("Publish"), TEXT("PlayPath"));

    strURL.KillSpaces();
    strPlayPath.KillSpaces();

    LPSTR lpAnsiURL = NULL, lpAnsiPlaypath = NULL;

    //--------------------------------
    // unbelievably disgusting hack for elgato devices (should no longer be necessary)

    /*String strOldDirectory;
    UINT dirSize = GetCurrentDirectory(0, 0);
    strOldDirectory.SetLength(dirSize);
    GetCurrentDirectory(dirSize, strOldDirectory.Array());

    OSSetCurrentDirectory(API->GetAppPath());*/

    //--------------------------------

    ServiceIdentifier sid = GetCurrentService();

    //--------------------------------

    if(!strURL.IsValid())
    {
        failReason = TEXT("No server specified to connect to");
        goto end;
    }

    // A service ID implies the settings have come from the xconfig file.
    if(sid.id != 0 || sid.file.IsValid())
    {
        auto serviceData = LoadService(&failReason);
        auto service = serviceData.second;

        if(!service)
        {
            if (failReason.IsEmpty())
                failReason = TEXT("Could not find the service specified in services.xconfig");
            goto end;
        }

        // Each service can have many ingestion servers. Look up a server for a particular service.
        XElement *servers = service->GetElement(TEXT("servers"));
        if(!servers)
        {
            failReason = TEXT("Could not find any servers for the service specified in services.xconfig");
            goto end;
        }

        // Got the server node now so can look up the ingestion URL.
        XDataItem *item = servers->GetDataItem(strURL);
        if(!item)
            item = servers->GetDataItemByID(0);

        strURL = item->GetData();

        // Stream urls start with RTMP. If there's an HTTP(S) then assume this is a web API call
        // to get the proper data.
        if ((strURL.Left(5).MakeLower() == "https") || (strURL.Left(4).MakeLower() == "http"))
        {
            // Query the web API for stream details
            String web_url = strURL + strPlayPath;

            int responseCode;
            TCHAR extraHeaders[256];

            extraHeaders[0] = 0;

            String response = HTTPGetString(web_url, extraHeaders, &responseCode);

            if (responseCode != 200 && responseCode != 304)
            {
                failReason = TEXT("Webserver failed to respond with valid stream details.");
                goto end;
            }

            XConfig apiData;

            // Expecting a response from the web API to look like this:
            // {"data":{"stream_url":"rtmp://some_url", "stream_name": "some-name"}}
            // A nice bit of JSON which is basically the same as the structure for XConfig.
            if(!apiData.ParseString(response))
            {
                failReason = TEXT("Could not understand response from webserver.");
                goto end;
            }

            // We could have read an error string back from the server.
            // So we need to trap any missing bits of data.

            XElement *p_data = apiData.GetElement(TEXT("data"));

            if (p_data == NULL)
            {
                failReason = TEXT("No valid data returned from web server.");
                goto end;
            }

            XDataItem *p_stream_url_data = p_data->GetDataItem(TEXT("stream_url"));

            if (p_stream_url_data == NULL)
            {
                failReason = TEXT("No valid broadcast stream URL returned from web server.");
                goto end;
            }

            strURL = p_stream_url_data->GetData();

            XDataItem *p_stream_name_data = p_data->GetDataItem(TEXT("stream_name"));

            if (p_stream_name_data == NULL)
            {
                failReason = TEXT("No valid stream name/path returned from web server.");
                goto end;
            }

            strPlayPath = p_stream_name_data->GetData();

            Log(TEXT("Web API returned URL: %s"), strURL.Array());
        }

        Log(TEXT("Using RTMP service: %s"), service->GetName());
        Log(TEXT("  Server selection: %s"), strURL.Array());
    }

    //------------------------------------------------------
    // now back to the elgato directory if it needs the directory changed still to function *sigh*

    //OSSetCurrentDirectory(strOldDirectory);

    //------------------------------------------------------

    OSEnterMutex(publisher->hRTMPMutex);
    publisher->rtmp = RTMP_Alloc();

    rtmp = publisher->rtmp;
    RTMP_Init(rtmp);

    RTMP_LogSetCallback(librtmpErrorCallback);

    OSLeaveMutex(publisher->hRTMPMutex);

    //RTMP_LogSetLevel(RTMP_LOGERROR);

    lpAnsiURL = strURL.CreateUTF8String();
    lpAnsiPlaypath = strPlayPath.CreateUTF8String();

    if(!RTMP_SetupURL2(rtmp, lpAnsiURL, lpAnsiPlaypath))
    {
        failReason = Str("Connection.CouldNotParseURL");
        goto end;
    }

    // A user name and password can be kept in the .ini file
    // If there's some credentials there then they'll be used in the RTMP channel
    char *rtmpUser = AppConfig->GetString(TEXT("Publish"), TEXT("Username")).CreateUTF8String();
    char *rtmpPass = AppConfig->GetString(TEXT("Publish"), TEXT("Password")).CreateUTF8String();

    if (rtmpUser)
    {
        rtmp->Link.pubUser.av_val = rtmpUser;
        rtmp->Link.pubUser.av_len = (int)strlen(rtmpUser);
    }

    if (rtmpPass)
    {
        rtmp->Link.pubPasswd.av_val = rtmpPass;
        rtmp->Link.pubPasswd.av_len = (int)strlen(rtmpPass);
    }

    RTMP_EnableWrite(rtmp); //set it to publish

    rtmp->Link.swfUrl.av_len = rtmp->Link.tcUrl.av_len;
    rtmp->Link.swfUrl.av_val = rtmp->Link.tcUrl.av_val;
    /*rtmp->Link.pageUrl.av_len = rtmp->Link.tcUrl.av_len;
    rtmp->Link.pageUrl.av_val = rtmp->Link.tcUrl.av_val;*/
    rtmp->Link.flashVer.av_val = "FMLE/3.0 (compatible; FMSc/1.0)";
    rtmp->Link.flashVer.av_len = (int)strlen(rtmp->Link.flashVer.av_val);

    //-----------------------------------------

    UINT tcpBufferSize = AppConfig->GetInt(TEXT("Publish"), TEXT("TCPBufferSize"), 64*1024);

    if(tcpBufferSize < 8192)
        tcpBufferSize = 8192;
    else if(tcpBufferSize > 1024*1024)
        tcpBufferSize = 1024*1024;

    rtmp->m_outChunkSize = 4096;//RTMP_DEFAULT_CHUNKSIZE;//
    rtmp->m_bSendChunkSizeInfo = TRUE;

    rtmp->m_bUseNagle = TRUE;

    strBindIP = AppConfig->GetString(TEXT("Publish"), TEXT("BindToIP"), TEXT("Default"));
    if (scmp(strBindIP, TEXT("Default")))
    {
        Log(TEXT("  Binding to non-default IP %s"), strBindIP.Array());

        if (schr(strBindIP.Array(), ':'))
            rtmp->m_bindIP.addr.ss_family = AF_INET6;
        else
            rtmp->m_bindIP.addr.ss_family = AF_INET;
        rtmp->m_bindIP.addrLen = sizeof(rtmp->m_bindIP.addr);

        if (WSAStringToAddress(strBindIP.Array(), rtmp->m_bindIP.addr.ss_family, NULL, (LPSOCKADDR)&rtmp->m_bindIP.addr, &rtmp->m_bindIP.addrLen) == SOCKET_ERROR)
        {
            // no localization since this should rarely/never happen
            failReason = TEXT("WSAStringToAddress: Could not parse address");
            goto end;
        }
    }

    LogInterfaceType(rtmp);

    //-----------------------------------------

    DWORD startTime = OSGetTime();

    if(!RTMP_Connect(rtmp, NULL))
    {
        failReason = Str("Connection.CouldNotConnect");
        failReason << TEXT("\r\n\r\n") << RTMPPublisher::GetRTMPErrors();
        bCanRetry = true;
        goto end;
    }

    Log(TEXT("Completed handshake with %s in %u ms."), strURL.Array(), OSGetTime() - startTime);

    if(!RTMP_ConnectStream(rtmp, 0))
    {
        failReason = Str("Connection.InvalidStream");
        failReason << TEXT("\r\n\r\n") << RTMPPublisher::GetRTMPErrors();
        bCanRetry = true;
        goto end;
    }

    //-----------------------------------------

    OSDebugOut(TEXT("Connected: %u\r\n"), OSGetTime());

    publisher->RequestKeyframe(1000);

    //-----------------------------------------

    bSuccess = true;

end:

    if (lpAnsiURL)
        Free(lpAnsiURL);

    if (lpAnsiPlaypath)
        Free(lpAnsiPlaypath);

    if(!bSuccess)
    {
        OSEnterMutex(publisher->hRTMPMutex);
        if(rtmp)
        {
            RTMP_Close(rtmp);
            RTMP_Free(rtmp);
            publisher->rtmp = NULL;
        }
        OSLeaveMutex(publisher->hRTMPMutex);

        if(failReason.IsValid())
            App->SetStreamReport(failReason);

        if(!publisher->bStopping)
            PostMessage(hwndMain, OBS_REQUESTSTOP, bCanRetry ? 0 : 1, 0);

        Log(TEXT("Connection to %s failed: %s"), strURL.Array(), failReason.Array());

        publisher->bStopping = true;
    }
    else
    {
        publisher->Init(tcpBufferSize);
        publisher->bConnected = true;
        publisher->bConnecting = false;
    }

    return 0;
}

double RTMPPublisher::GetPacketStrain() const
{
    return (curDataBufferLen / (double)dataBufferSize) * 100.0;
    /*if(packetWaitType >= PacketType_VideoHigh)
        return min(100.0, dNetworkStrain*100.0);
    else if(bNetworkStrain)
        return dNetworkStrain*66.0;

    return dNetworkStrain*33.0;*/
}

QWORD RTMPPublisher::GetCurrentSentBytes()
{
    return bytesSent;
}

DWORD RTMPPublisher::NumDroppedFrames() const
{
    return numBFramesDumped+numPFramesDumped;
}

int RTMPPublisher::FlushDataBuffer()
{
    unsigned long zero = 0;

    //OSDebugOut (TEXT("*** ~RTMPPublisher FlushDataBuffer (%d)\n"), curDataBufferLen);
    
    //make it blocking again
    WSAEventSelect(rtmp->m_sb.sb_socket, NULL, 0);
    ioctlsocket(rtmp->m_sb.sb_socket, FIONBIO, &zero);

    OSEnterMutex(hDataBufferMutex);
    int ret = send(rtmp->m_sb.sb_socket, (const char *)dataBuffer, curDataBufferLen, 0);
    curDataBufferLen = 0;
    OSLeaveMutex(hDataBufferMutex);

    return ret;
}

void RTMPPublisher::SetupSendBacklogEvent()
{
    zero (&sendBacklogOverlapped, sizeof(sendBacklogOverlapped));

    ResetEvent (hSendBacklogEvent);
    sendBacklogOverlapped.hEvent = hSendBacklogEvent;

    idealsendbacklognotify (rtmp->m_sb.sb_socket, &sendBacklogOverlapped, NULL);
}

void RTMPPublisher::FatalSocketShutdown()
{
    //We close the socket manually to avoid trying to run cleanup code during the shutdown cycle since
    //if we're being called the socket is already in an unusable state.
    closesocket(rtmp->m_sb.sb_socket);
    rtmp->m_sb.sb_socket = -1;

    //anything buffered is invalid now
    curDataBufferLen = 0;

    if (!bStopping)
    {
        if (AppConfig->GetInt(TEXT("Publish"), TEXT("ExperimentalReconnectMode")) == 1 && AppConfig->GetInt(TEXT("Publish"), TEXT("Delay")) == 0)
            App->NetworkFailed();
        else
            App->PostStopMessage();
    }
}

void RTMPPublisher::SocketLoop()
{
    bool canWrite = false;

    int delayTime;
    int latencyPacketSize;
    DWORD lastSendTime = 0;

    WSANETWORKEVENTS networkEvents;

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    WSAEventSelect(rtmp->m_sb.sb_socket, hWriteEvent, FD_READ|FD_WRITE|FD_CLOSE);

    //Low latency mode works by delaying delayTime ms between calls to send() and only sending
    //a buffer as large as latencyPacketSize at once. This causes keyframes and other data bursts
    //to be sent over several sends instead of one large one.
    if (lowLatencyMode == LL_MODE_AUTO)
    {
        //Auto mode aims for a constant rate of whatever the stream bitrate is and segments into
        //MTU sized packets (test packet captures indicated that despite nagling being enabled,
        //the size of the send() buffer is still important for some reason). Note that delays
        //become very short at this rate, and it can take a while for the buffer to empty after
        //a keyframe.
        delayTime = 1400.0f / (dataBufferSize / 1000.0f);
        latencyPacketSize = 1460;
    }
    else if (lowLatencyMode == LL_MODE_FIXED)
    {
        //We use latencyFactor - 2 to guarantee we're always sending at a slightly higher
        //rate than the maximum expected data rate so we don't get backed up
        latencyPacketSize = dataBufferSize / (latencyFactor - 2);
        delayTime = 1000 / latencyFactor;
    }
    else
    {
        latencyPacketSize = dataBufferSize;
        delayTime = 0;
    }

    if (AppConfig->GetInt (TEXT("Publish"), TEXT("DisableSendWindowOptimization"), 0) == 0)
        SetupSendBacklogEvent ();
    else
        Log (TEXT("RTMPPublisher::SocketLoop: Send window optimization disabled by user."));

    HANDLE hObjects[3];

    hObjects[0] = hWriteEvent;
    hObjects[1] = hBufferEvent;
    hObjects[2] = hSendBacklogEvent;

    for (;;)
    {
        if (bStopping && WaitForSingleObject(hSocketLoopExit, 0) != WAIT_TIMEOUT)
        {
            OSEnterMutex(hDataBufferMutex);
            if (curDataBufferLen == 0)
            {
                //OSDebugOut (TEXT("Exiting on empty buffer.\n"));
                OSLeaveMutex(hDataBufferMutex);
                break;
            }

            //OSDebugOut (TEXT("Want to exit, but %d bytes remain.\n"), curDataBufferLen);
            OSLeaveMutex(hDataBufferMutex);
        }

        int status = WaitForMultipleObjects (3, hObjects, FALSE, INFINITE);
        if (status == WAIT_ABANDONED || status == WAIT_FAILED)
        {
            Log(TEXT("RTMPPublisher::SocketLoop: Aborting due to WaitForMultipleObjects failure"));
            App->PostStopMessage();
            return;
        }

        if (status == WAIT_OBJECT_0)
        {
            //Socket event
            if (WSAEnumNetworkEvents (rtmp->m_sb.sb_socket, NULL, &networkEvents))
            {
                Log(TEXT("RTMPPublisher::SocketLoop: Aborting due to WSAEnumNetworkEvents failure, %d"), WSAGetLastError());
                App->PostStopMessage();
                return;
            }

            if (networkEvents.lNetworkEvents & FD_WRITE)
                canWrite = true;

            if (networkEvents.lNetworkEvents & FD_CLOSE)
            {
                if (lastSendTime)
                {
                    DWORD diff = OSGetTime() - lastSendTime;
                    Log(TEXT("RTMPPublisher::SocketLoop: Received FD_CLOSE, %u ms since last send (buffer: %d / %d)"), diff, curDataBufferLen, dataBufferSize);
                }

                if (bStopping)
                    Log(TEXT("RTMPPublisher::SocketLoop: Aborting due to FD_CLOSE during shutdown, %d bytes lost, error %d"), curDataBufferLen, networkEvents.iErrorCode[FD_CLOSE_BIT]);
                else
                    Log(TEXT("RTMPPublisher::SocketLoop: Aborting due to FD_CLOSE, error %d"), networkEvents.iErrorCode[FD_CLOSE_BIT]);
                FatalSocketShutdown ();
                return;
            }

            if (networkEvents.lNetworkEvents & FD_READ)
            {
                BYTE discard[16384];
                int ret, errorCode;
                BOOL fatalError = FALSE;

                for (;;)
                {
                    ret = recv(rtmp->m_sb.sb_socket, (char *)discard, sizeof(discard), 0);
                    if (ret == -1)
                    {
                        errorCode = WSAGetLastError();

                        if (errorCode == WSAEWOULDBLOCK)
                            break;

                        fatalError = TRUE;
                    }
                    else if (ret == 0)
                    {
                        errorCode = 0;
                        fatalError = TRUE;
                    }

                    if (fatalError)
                    {
                        Log(TEXT("RTMPPublisher::SocketLoop: Socket error, recv() returned %d, GetLastError() %d"), ret, errorCode);
                        FatalSocketShutdown ();
                        return;
                    }
                }
            }
        }
        else if (status == WAIT_OBJECT_0 + 2)
        {
            //Ideal send backlog event
            ULONG idealSendBacklog;

            if (!idealsendbacklogquery(rtmp->m_sb.sb_socket, &idealSendBacklog))
            {
                int curTCPBufSize, curTCPBufSizeSize = sizeof(curTCPBufSize);
                if (!getsockopt(rtmp->m_sb.sb_socket, SOL_SOCKET, SO_SNDBUF, (char *)&curTCPBufSize, &curTCPBufSizeSize))
                {
                    if (curTCPBufSize < (int)idealSendBacklog)
                    {
                        int bufferSize = (int)idealSendBacklog;
                        setsockopt(rtmp->m_sb.sb_socket, SOL_SOCKET, SO_SNDBUF, (const char *)&bufferSize, sizeof(bufferSize));
                        Log(TEXT("RTMPPublisher::SocketLoop: Increasing send buffer to ISB %d (buffer: %d / %d)"), idealSendBacklog, curDataBufferLen, dataBufferSize);
                    }
                }
                else
                    Log(TEXT("RTMPPublisher::SocketLoop: Got hSendBacklogEvent but getsockopt() returned %d"), WSAGetLastError());
            }
            else
                Log(TEXT("RTMPPublisher::SocketLoop: Got hSendBacklogEvent but WSAIoctl() returned %d"), WSAGetLastError());

            SetupSendBacklogEvent ();
            continue;
        }
        
        if (canWrite)
        {
            bool exitLoop = false;
            do
            {
                OSEnterMutex(hDataBufferMutex);

                if (!curDataBufferLen)
                {
                    //this is now an expected occasional condition due to use of auto-reset events, we could end up emptying the buffer
                    //as it's filled in a previous loop cycle, especially if using low latency mode.
                    OSLeaveMutex(hDataBufferMutex);
                    //Log(TEXT("RTMPPublisher::SocketLoop: Trying to send, but no data available?!"));
                    break;
                }
                
                int ret;
                if (lowLatencyMode != LL_MODE_NONE)
                {
                    int sendLength = min (latencyPacketSize, curDataBufferLen);
                    ret = send(rtmp->m_sb.sb_socket, (const char *)dataBuffer, sendLength, 0);
                }
                else
                {
                    ret = send(rtmp->m_sb.sb_socket, (const char *)dataBuffer, curDataBufferLen, 0);
                }

                if (ret > 0)
                {
                    if (curDataBufferLen - ret)
                        memmove(dataBuffer, dataBuffer + ret, curDataBufferLen - ret);
                    curDataBufferLen -= ret;

                    bytesSent += ret;

                    if (lastSendTime)
                    {
                        DWORD diff = OSGetTime() - lastSendTime;

                        if (diff >= 1500)
                            Log(TEXT("RTMPPublisher::SocketLoop: Stalled for %u ms to write %d bytes (buffer: %d / %d), unstable connection?"), diff, ret, curDataBufferLen, dataBufferSize);

                        totalSendPeriod += diff;
                        totalSendBytes += ret;
                        totalSendCount++;
                    }

                    lastSendTime = OSGetTime();

                    SetEvent(hBufferSpaceAvailableEvent);
                }
                else
                {
                    int errorCode;
                    BOOL fatalError = FALSE;

                    if (ret == -1)
                    {
                        errorCode = WSAGetLastError();

                        if (errorCode == WSAEWOULDBLOCK)
                        {
                            canWrite = false;
                            OSLeaveMutex(hDataBufferMutex);
                            break;
                        }

                        fatalError = TRUE;
                    }
                    else if (ret == 0)
                    {
                        errorCode = 0;
                        fatalError = TRUE;
                    }

                    if (fatalError)
                    {
                        //connection closed, or connection was aborted / socket closed / etc, that's a fatal error for us.
                        Log(TEXT("RTMPPublisher::SocketLoop: Socket error, send() returned %d, GetLastError() %d"), ret, errorCode);
                        OSLeaveMutex(hDataBufferMutex);
                        FatalSocketShutdown ();
                        return;
                    }
                }

                //finish writing for now
                if (curDataBufferLen <= 1000)
                    exitLoop = true;

                OSLeaveMutex(hDataBufferMutex);

                if (delayTime)
                    Sleep (delayTime);
            } while (!exitLoop);
        }
    }

    Log(TEXT("RTMPPublisher::SocketLoop: Graceful loop exit"));
}

void RTMPPublisher::SendLoop()
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    while(WaitForSingleObject(hSendSempahore, INFINITE) == WAIT_OBJECT_0)
    {
        while(true)
        {
            OSEnterMutex(hDataMutex);
            if(queuedPackets.Num() == 0)
            {
                OSLeaveMutex(hDataMutex);
                break;
            }

            List<BYTE> packetData;
            PacketType type       = queuedPackets[0].type;
            DWORD      timestamp  = queuedPackets[0].timestamp;
            packetData.TransferFrom(queuedPackets[0].data);

            currentBufferSize -= packetData.Num();

            queuedPackets.Remove(0);

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

            //QWORD sendTimeStart = OSGetTimeMicroseconds();
            if(!RTMP_SendPacket(rtmp, &packet, FALSE))
            {
                //should never reach here with the new shutdown sequence.
                RUNONCE Log(TEXT("RTMP_SendPacket failure, should not happen!"));
                if(!RTMP_IsConnected(rtmp))
                {
                    App->PostStopMessage();
                    break;
                }
            }

            //----------------------------------------------------------

            /*outputRateSize += packetData.Num();
            packetSizeRecord << PacketTimeSize(timestamp, packetData.Num());
            if(packetSizeRecord.Num())
            {
                UINT packetID=0;
                for(; packetID<packetSizeRecord.Num(); packetID++)
                {
                    if(timestamp-packetSizeRecord[packetID].timestamp < outputRateWindowTime)
                        break;
                    else
                        outputRateSize -= packetSizeRecord[packetID].size;
                }

                if(packetID != 0)
                    packetSizeRecord.RemoveRange(0, packetID);
            }*/

            //bytesSent += packetData.Num();
        }

        if (bStopping && WaitForSingleObject(hSendLoopExit, 0) == WAIT_OBJECT_0)
            return;
    }
}

DWORD RTMPPublisher::SendThread(RTMPPublisher *publisher)
{
    publisher->SendLoop();
    return 0;
}

DWORD RTMPPublisher::SocketThread(RTMPPublisher *publisher)
{
    publisher->SocketLoop();
    return 0;
}

void RTMPPublisher::DropFrame(UINT id)
{
    NetworkPacket &dropPacket = queuedPackets[id];
    currentBufferSize -= dropPacket.data.Num();
    PacketType type = dropPacket.type;
    dropPacket.data.Clear();

    if(dropPacket.type < PacketType_VideoHigh)
        numBFramesDumped++;
    else
        numPFramesDumped++;

    for(UINT i=id+1; i<queuedPackets.Num(); i++)
    {
        UINT distance = (i-id);
        if(queuedPackets[i].distanceFromDroppedFrame <= distance)
            break;

        queuedPackets[i].distanceFromDroppedFrame = distance;
    }

    for(int i=int(id)-1; i>=0; i--)
    {
        UINT distance = (id-UINT(i));
        if(queuedPackets[i].distanceFromDroppedFrame <= distance)
            break;

        queuedPackets[i].distanceFromDroppedFrame = distance;
    }

    bool bSetPriority = true;
    for(UINT i=id+1; i<queuedPackets.Num(); i++)
    {
        NetworkPacket &packet = queuedPackets[i];
        if(packet.type < PacketType_Audio)
        {
            if(type >= PacketType_VideoHigh)
            {
                if(packet.type < PacketType_VideoHighest)
                {
                    currentBufferSize -= packet.data.Num();
                    packet.data.Clear();
                    queuedPackets.Remove(i--);

                    if(packet.type < PacketType_VideoHigh)
                        numBFramesDumped++;
                    else
                        numPFramesDumped++;
                }
                else
                {
                    bSetPriority = false;
                    break;
                }
            }
            else
            {
                if(packet.type >= type)
                {
                    bSetPriority = false;
                    break;
                }
            }
        }
    }

    if(bSetPriority)
    {
        if(type >= PacketType_VideoHigh)
            packetWaitType = PacketType_VideoHighest;
        else
        {
            if(packetWaitType < type)
                packetWaitType = type;
        }
    }
}

//video packet count exceeding maximum.  find lowest priority frame to dump
bool RTMPPublisher::DoIFrameDelay(bool bBFramesOnly)
{
    int curWaitType = PacketType_VideoDisposable;

    while(!bBFramesOnly && curWaitType < PacketType_VideoHighest ||
           bBFramesOnly && curWaitType < PacketType_VideoHigh)
    {
        UINT bestPacket = INVALID;
        UINT bestPacketDistance = 0;

        if(curWaitType == PacketType_VideoHigh)
        {
            bool bFoundIFrame = false;

            for(int i=int(queuedPackets.Num())-1; i>=0; i--)
            {
                NetworkPacket &packet = queuedPackets[i];
                if(packet.type == PacketType_Audio)
                    continue;

                if(packet.type == curWaitType)
                {
                    if(bFoundIFrame)
                    {
                        bestPacket = UINT(i);
                        break;
                    }
                    else if(bestPacket == INVALID)
                        bestPacket = UINT(i);
                }
                else if(packet.type == PacketType_VideoHighest)
                    bFoundIFrame = true;
            }
        }
        else
        {
            for(UINT i=0; i<queuedPackets.Num(); i++)
            {
                NetworkPacket &packet = queuedPackets[i];
                if(packet.type <= curWaitType)
                {
                    if(packet.distanceFromDroppedFrame > bestPacketDistance)
                    {
                        bestPacket = i;
                        bestPacketDistance = packet.distanceFromDroppedFrame;
                    }
                }
            }
        }

        if(bestPacket != INVALID)
        {
            DropFrame(bestPacket);
            queuedPackets.Remove(bestPacket);
            return true;
        }

        curWaitType++;
    }

    return false;
}

void RTMPPublisher::RequestKeyframe(int waitTime)
{
    App->RequestKeyframe(waitTime);
}

int RTMPPublisher::BufferedSend(RTMPSockBuf *sb, const char *buf, int len, RTMPPublisher *network)
{
    //NOTE: This function is called from the SendLoop thread, be careful of race conditions.

retrySend:

    //We may have been disconnected mid-shutdown or something, just pretend we wrote the data
    //to avoid blocking if the socket loop exited.
    if (!RTMP_IsConnected(network->rtmp))
        return len;

    OSEnterMutex(network->hDataBufferMutex);

    if (network->curDataBufferLen + len >= network->dataBufferSize)
    {
        //Log(TEXT("RTMPPublisher::BufferedSend: Socket buffer is full (%d / %d bytes), waiting to send %d bytes"), network->curDataBufferLen, network->dataBufferSize, len);
        ++network->totalTimesWaited;
        network->totalBytesWaited += len;

        OSLeaveMutex(network->hDataBufferMutex);

        int status = WaitForSingleObject(network->hBufferSpaceAvailableEvent, INFINITE);
        if (status == WAIT_ABANDONED || status == WAIT_FAILED)
            return 0;
        goto retrySend;
    }

    mcpy(network->dataBuffer + network->curDataBufferLen, buf, len);
    network->curDataBufferLen += len;

    OSLeaveMutex(network->hDataBufferMutex);

    SetEvent (network->hBufferEvent);

    return len;
}

NetworkStream* CreateRTMPPublisher()
{
    return new RTMPPublisher;
}
