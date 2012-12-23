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

RTMPPublisher::RTMPPublisher()
{
    hSendSempahore = CreateSemaphore(NULL, 0, 0x7FFFFFFFL, NULL);
    if(!hSendSempahore)
        CrashError(TEXT("RTMPPublisher: Could not create semaphore"));

    hDataMutex = OSCreateMutex();
    if(!hDataMutex)
        CrashError(TEXT("RTMPPublisher: Could not create mutex"));

    //------------------------------------------

    bufferTime = (DWORD)AppConfig->GetInt(TEXT("Publish"), TEXT("FrameDropBufferTime"), 1400);
    if(bufferTime < 1000) bufferTime = 1000;
    else if(bufferTime > 10000) bufferTime = 10000;

    connectTime = bufferTime-600; //connect about 600 milliseconds before we start sending
    outputRateWindowTime = (DWORD)AppConfig->GetInt(TEXT("Publish"), TEXT("OutputRateWindowTime"), 1000);
    if(outputRateWindowTime < 200)       outputRateWindowTime = 200;
    else if(outputRateWindowTime > 4000) outputRateWindowTime = 4000;

    dropThreshold = (DWORD)AppConfig->GetInt(TEXT("Publish"), TEXT("FrameDropThreshold"), 500);
    if(dropThreshold < 50)        dropThreshold = 50;
    else if(dropThreshold > 1000) dropThreshold = 1000;
    dropThreshold += bufferTime;
}

bool RTMPPublisher::Init(RTMP *rtmpIn, UINT tcpBufferSize, BOOL bUseSendBuffer, UINT sendBufferSize)
{
    rtmp = rtmpIn;

    //------------------------------------------

    sendBuffer.SetSize(sendBufferSize);
    curSendBufferLen = 0;

    this->bUseSendBuffer = bUseSendBuffer;

    if(1)
    {
        //Log(TEXT("Using Send Buffer Size: %u"), sendBufferSize);

        rtmp->m_customSendFunc = (CUSTOMSEND)RTMPPublisher::BufferedSend;
        rtmp->m_customSendParam = this;
        rtmp->m_bCustomSend = TRUE;
    }

    //------------------------------------------

    int curTCPBufSize, curTCPBufSizeSize = 4;
    getsockopt (rtmp->m_sb.sb_socket, SOL_SOCKET, SO_SNDBUF, (char *)&curTCPBufSize, &curTCPBufSizeSize);

    if(curTCPBufSize < int(tcpBufferSize))
    {
        setsockopt (rtmp->m_sb.sb_socket, SOL_SOCKET, SO_SNDBUF, (const char *)&tcpBufferSize, sizeof(tcpBufferSize));
        getsockopt (rtmp->m_sb.sb_socket, SOL_SOCKET, SO_SNDBUF, (char *)&curTCPBufSize, &curTCPBufSizeSize);
        if(curTCPBufSize != tcpBufferSize)
            Log(TEXT("Could not set SO_SNDBUF to %u, value is now %u"), tcpBufferSize, curTCPBufSize);
    }
    else
        Log(TEXT("SO_SNDBUF already at %u"), curTCPBufSize);

    //------------------------------------------

    hSendThread = OSCreateThread((XTHREAD)RTMPPublisher::SendThread, this);
    if(!hSendThread)
        CrashError(TEXT("RTMPPublisher: Could not create send thread"));

    hBufferEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    hBufferSpaceAvailableEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    hDataBufferMutex = OSCreateMutex();

    dataBufferSize = (App->GetVideoEncoder()->GetBitRate() + App->GetAudioEncoder()->GetBitRate()) / 8 * 1024;

    dataBuffer = (BYTE *)Allocate(dataBufferSize);

    hSocketThread = OSCreateThread((XTHREAD)RTMPPublisher::SocketThread, this);
    if(!hSocketThread)
        CrashError(TEXT("RTMPPublisher: Could not create send thread"));

    //------------------------------------------

    packetWaitType = 0;

    return true;
}

RTMPPublisher::~RTMPPublisher()
{
    bStopping = true;

    //we're in the middle of connecting! wait for that to happen to avoid all manner of race conditions
    if (hConnectionThread)
    {
        WaitForSingleObject(hConnectionThread, INFINITE);
        OSCloseThread(hConnectionThread);
    }

    if(hSendThread)
    {
        ReleaseSemaphore(hSendSempahore, 1, NULL);
        OSTerminateThread(hSendThread, 20000);
    }

    if(hSendSempahore)
        CloseHandle(hSendSempahore);

    if(hDataMutex)
        OSCloseMutex(hDataMutex);

    //wake up and shut down the buffered sender
    SetEvent(hWriteEvent);
    SetEvent(hBufferEvent);

    if (hSocketThread)
    {
        OSTerminateThread(hSocketThread, 20000);

        //at this point nothing new should be coming in to the buffer, flush out what remains
        FlushDataBuffer();
    }

    if(rtmp)
    {
        //disable the buffered send, so RTMP_Close writes directly to the net
        rtmp->m_bCustomSend = 0;
        RTMP_Close(rtmp);
    }

    if (dataBuffer)
        Free(dataBuffer);

    if (hDataBufferMutex)
        OSCloseMutex(hDataBufferMutex);

    if (hBufferEvent)
        CloseHandle(hBufferEvent);

    if (hBufferSpaceAvailableEvent)
        CloseHandle(hBufferSpaceAvailableEvent);

    if (hWriteEvent)
        CloseHandle(hWriteEvent);

    if(rtmp)
        RTMP_Free(rtmp);

    //--------------------------

    for(UINT i=0; i<queuedPackets.Num(); i++)
        queuedPackets[i].data.Clear();
    queuedPackets.Clear();

    double dTotalFrames = double(totalFrames);
    double dBFrameDropPercentage = double(numBFramesDumped)/dTotalFrames*100.0;
    double dPFrameDropPercentage = double(numPFramesDumped)/dTotalFrames*100.0;

    Log(TEXT("Number of b-frames dropped: %u (%0.2g%%), Number of p-frames dropped: %u (%0.2g%%), Total %u (%0.2g%%)"),
        numBFramesDumped, dBFrameDropPercentage,
        numPFramesDumped, dPFrameDropPercentage,
        numBFramesDumped+numPFramesDumped, dBFrameDropPercentage+dPFrameDropPercentage);

    /*if(totalCalls)
        Log(TEXT("average send time: %u"), totalTime/totalCalls);*/

    //--------------------------
}

void RTMPPublisher::ProcessPackets(DWORD timestamp)
{
    if(queuedPackets.Num())
    {
        DWORD queueTime = queuedPackets.Last().timestamp-queuedPackets[0].timestamp;
        DWORD adjustedOutputRateSize = outputRateSize*bufferTime/outputRateWindowTime;

        //Log(TEXT("queueTime: %u, adjustedOutputRateSize: %u, currentBufferSize: %u, queuedPackets.Num: %u, timestamp: %u"), queueTime, adjustedOutputRateSize, currentBufferSize, queuedPackets.Num(), timestamp);

        if (queueTime)
            dNetworkStrain = (double(queueTime)/double(dropThreshold));

        if( bStreamStarted &&
            !ignoreCount &&
            queueTime > dropThreshold)
        {
            if(currentBufferSize > adjustedOutputRateSize)
            {
                //Log(TEXT("killing frames"));
                while(currentBufferSize > adjustedOutputRateSize)
                {
                    if(!DoIFrameDelay(false))
                        break;
                }

                bNetworkStrain = true;
            }
            /*else
                Log(TEXT("ignoring frames"));*/

            ignoreCount = int(queuedPackets.Num());
        }
    }

    if(!bConnected && !bConnecting && timestamp > connectTime && !bStopping)
    {
        hConnectionThread = OSCreateThread((XTHREAD)CreateConnectionThread, this);
        bConnecting = true;
    }

    if(timestamp >= bufferTime)
    {
        if(bConnected)
        {
            if(!bStreamStarted)
            {
                BeginPublishingInternal();
                bStreamStarted = true;

                DWORD timeAdjust = timestamp-bufferTime;
                bufferTime += timeAdjust;
                dropThreshold += timeAdjust;

                Log(TEXT("bufferTime: %u, outputRateWindowTime: %u, dropThreshold: %u"), bufferTime, outputRateWindowTime, dropThreshold);
            }

            sendTime = timestamp-bufferTime;
            ReleaseSemaphore(hSendSempahore, 1, NULL);
        }
    }
}

void RTMPPublisher::SendPacket(BYTE *data, UINT size, DWORD timestamp, PacketType type)
{
    if(!bStopping)
    {
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

        OSEnterMutex(hDataMutex);

        if(bAddPacket)
        {
            List<BYTE> paddedData;
            paddedData.SetSize(size+RTMP_MAX_HEADER_SIZE);
            mcpy(paddedData.Array()+RTMP_MAX_HEADER_SIZE, data, size);

            currentBufferSize += paddedData.Num();

            UINT droppedFrameVal = queuedPackets.Num() ? queuedPackets.Last().distanceFromDroppedFrame+1 : 10000;

            NetworkPacket *queuedPacket = queuedPackets.CreateNew();
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

        ProcessPackets(timestamp);

        OSLeaveMutex(hDataMutex);
    }
}

void RTMPPublisher::BeginPublishingInternal()
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
        bStopping = true;
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
        bStopping = true;
        return;
    }
}

void RTMPPublisher::BeginPublishing()
{
}

DWORD WINAPI RTMPPublisher::CreateConnectionThread(RTMPPublisher *publisher)
{
    //------------------------------------------------------
    // set up URL

    bool bRetry = false;
    bool bSuccess = false;
    bool bCanRetry = false;

    String failReason;
    String strBindIP;

    int    serviceID    = AppConfig->GetInt   (TEXT("Publish"), TEXT("Service"));
    String strURL       = AppConfig->GetString(TEXT("Publish"), TEXT("URL"));
    String strPlayPath  = AppConfig->GetString(TEXT("Publish"), TEXT("PlayPath"));

    LPSTR lpAnsiURL = NULL, lpAnsiPlaypath = NULL;
    RTMP *rtmp = NULL;

    if(!strURL.IsValid())
    {
        failReason = TEXT("No server specified to connect to");
        goto end;
    }

    if(serviceID != 0)
    {
        XConfig serverData;
        if(!serverData.Open(TEXT("services.xconfig")))
        {
            failReason = TEXT("Could not open services.xconfig");
            goto end;
        }

        XElement *services = serverData.GetElement(TEXT("services"));
        if(!services)
        {
            failReason = TEXT("Could not any services in services.xconfig");
            goto end;
        }

        XElement *service = services->GetElementByID(serviceID-1);
        if(!service)
        {
            failReason = TEXT("Could not find the service specified in services.xconfig");
            goto end;
        }

        XElement *servers = service->GetElement(TEXT("servers"));
        if(!servers)
        {
            failReason = TEXT("Could not find any servers for the service specified in services.xconfig");
            goto end;
        }

        XDataItem *item = servers->GetDataItem(strURL);
        if(!item)
            item = servers->GetDataItemByID(0);

        strURL = item->GetData();

        Log(TEXT("Using RTMP service: %s"), service->GetName());
        Log(TEXT("  Server selection: %s"), strURL);
    }

    //------------------------------------------------------

    rtmp = RTMP_Alloc();
    RTMP_Init(rtmp);
    /*RTMP_LogSetCallback(rtmp_log_output);
    RTMP_LogSetLevel(RTMP_LOGDEBUG2);*/

    lpAnsiURL = strURL.CreateUTF8String();
    lpAnsiPlaypath = strPlayPath.CreateUTF8String();

    if(!RTMP_SetupURL2(rtmp, lpAnsiURL, lpAnsiPlaypath))
    {
        failReason = Str("Connection.CouldNotParseURL");
        goto end;
    }

    RTMP_EnableWrite(rtmp); //set it to publish

    /*rtmp->Link.swfUrl.av_len = rtmp->Link.tcUrl.av_len;
    rtmp->Link.swfUrl.av_val = rtmp->Link.tcUrl.av_val;
    rtmp->Link.pageUrl.av_len = rtmp->Link.tcUrl.av_len;
    rtmp->Link.pageUrl.av_val = rtmp->Link.tcUrl.av_val;*/
    rtmp->Link.flashVer.av_val = "FMLE/3.0 (compatible; FMSc/1.0)";
    rtmp->Link.flashVer.av_len = (int)strlen(rtmp->Link.flashVer.av_val);

    //-----------------------------------------

    BOOL bUseSendBuffer = AppConfig->GetInt(TEXT("Publish"), TEXT("UseSendBuffer"));
    UINT sendBufferSize = AppConfig->GetInt(TEXT("Publish"), TEXT("SendBufferSize"), 5840);

    if(sendBufferSize > 32120)
        sendBufferSize = 32120;
    else if(sendBufferSize < 536)
        sendBufferSize = 536;

    //-----------------------------------------

    UINT tcpBufferSize = AppConfig->GetInt(TEXT("Publish"), TEXT("TCPBufferSize"), 64*1024);

    if(tcpBufferSize < 8196)
        tcpBufferSize = 8196;
    else if(tcpBufferSize > 128*1024)
        tcpBufferSize = 128*1024;

    rtmp->m_outChunkSize = 4096;//RTMP_DEFAULT_CHUNKSIZE;//
    rtmp->m_bSendChunkSizeInfo = TRUE;

    rtmp->m_bUseNagle = TRUE;

    strBindIP = AppConfig->GetString(TEXT("Publish"), TEXT("BindToIP"), TEXT("Default"));
    if (scmp(strBindIP, TEXT("Default")))
    {
        rtmp->m_bindIP.addr.sin_family = AF_INET;
        rtmp->m_bindIP.addrLen = sizeof(rtmp->m_bindIP.addr);
        if (WSAStringToAddress(strBindIP.Array(), AF_INET, NULL, (LPSOCKADDR)&rtmp->m_bindIP.addr, &rtmp->m_bindIP.addrLen) == SOCKET_ERROR)
        {
            // no localization since this should rarely/never happen
            failReason = TEXT("WSAStringToAddress: Could not parse address");
            goto end;
        }
    }

    //-----------------------------------------

    if(!RTMP_Connect(rtmp, NULL))
    {
        failReason = Str("Connection.CouldNotConnect");
        bCanRetry = true;
        goto end;
    }

    if(!RTMP_ConnectStream(rtmp, 0))
    {
        failReason = Str("Connection.InvalidStream");
        goto end;
    }

    bSuccess = true;

end:

    if (lpAnsiURL)
        Free(lpAnsiURL);

    if (lpAnsiPlaypath)
        Free(lpAnsiPlaypath);

    if(!bSuccess)
    {
        if(rtmp)
        {
            RTMP_Close(rtmp);
            RTMP_Free(rtmp);
        }

        if(failReason.IsValid())
            App->SetStreamReport(failReason);

        if(!publisher->bStopping)
            PostMessage(hwndMain, OBS_REQUESTSTOP, 1, 0);

        publisher->bStopping = true;
    }
    else
    {
        publisher->Init(rtmp, tcpBufferSize, bUseSendBuffer, sendBufferSize);
        publisher->bConnected = true;
        publisher->bConnecting = false;
    }

    return 0;
}

double RTMPPublisher::GetPacketStrain() const
{
    return (curDataBufferLen / (double)dataBufferSize) * 100.0;
    if(packetWaitType >= PacketType_VideoHigh)
        return min(100.0, dNetworkStrain*100.0);
    else if(bNetworkStrain)
        return dNetworkStrain*66.0;

    return dNetworkStrain*33.0;
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
    
    //make it blocking again
    ioctlsocket(rtmp->m_sb.sb_socket, FIONBIO, &zero);

    OSEnterMutex(hDataBufferMutex);
    int ret = send(rtmp->m_sb.sb_socket, (const char *)dataBuffer, curDataBufferLen, 0);
    curDataBufferLen = 0;
    OSLeaveMutex(hDataBufferMutex);

    return ret;
}

void RTMPPublisher::SocketLoop()
{
    WSANETWORKEVENTS networkEvents;

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    WSAEventSelect(rtmp->m_sb.sb_socket, hWriteEvent, FD_READ|FD_WRITE|FD_CLOSE);

    do
    {
        int status = WaitForSingleObject(hWriteEvent, INFINITE);
        if (status == WAIT_ABANDONED || status == WAIT_FAILED)
        {
            Log(TEXT("RTMPPublisher::SocketLoop: Aborting due to hWriteEvent mutex"));
            return;
        }

        if (WSAEnumNetworkEvents (rtmp->m_sb.sb_socket, NULL, &networkEvents))
        {
            Log(TEXT("RTMPPublisher::SocketLoop: Aborting due to WSAEnumNetworkEvents failure, %d"), WSAGetLastError());
            App->PostStopMessage();
            bStopping = true;
            return;
        }

        if (networkEvents.lNetworkEvents & FD_CLOSE)
        {
            Log(TEXT("RTMPPublisher::SocketLoop: Aborting due to FD_CLOSE, error %d"), networkEvents.iErrorCode[FD_CLOSE_BIT]);
            App->PostStopMessage();
            bStopping = true;
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
                    fatalError = TRUE;
                }

                if (fatalError)
                {
                    Log(TEXT("RTMPPublisher::SocketLoop: Socket error, recv() returned %d, GetLastError() %d"), ret, errorCode);
                    OSLeaveMutex(hDataBufferMutex);
                    App->PostStopMessage();
                    bStopping = true;
                    return;
                }
            }

            /*RTMPPacket packet;
            zero(&packet, sizeof(packet));

            do
            {
                RTMP_ReadPacket(rtmp, &packet);
            } while (!RTMPPacket_IsReady(&packet) && RTMP_IsConnected(rtmp));

            if(!RTMP_IsConnected(rtmp))
            {
                App->PostStopMessage();
                bStopping = true;
            }

            RTMPPacket_Free(&packet);*/
        }
        
        if (networkEvents.lNetworkEvents & FD_WRITE)
        {
            while (!bStopping)
            {
                status = WaitForSingleObject(hBufferEvent, INFINITE);
                if (status == WAIT_ABANDONED || status == WAIT_FAILED)
                {
                    Log(TEXT("RTMPPublisher::SocketLoop: Aborting due to hBufferEvent mutex"));
                    return;
                }

                if (bStopping)
                {
                    Log(TEXT("RTMPPublisher::SocketLoop: Aborting due to bStopping"));
                    return;
                }

                OSEnterMutex(hDataBufferMutex);

                if (!curDataBufferLen)
                {
                    OSLeaveMutex(hDataBufferMutex);
                    Log(TEXT("RTMPPublisher::SocketLoop: Trying to send, but no data available?!"));
                    continue;
                }

                int ret = send(rtmp->m_sb.sb_socket, (const char *)dataBuffer, curDataBufferLen, 0);
                if (ret > 0)
                {
                    if (curDataBufferLen - ret)
                        memmove(dataBuffer, dataBuffer + ret, curDataBufferLen - ret);
                    curDataBufferLen -= ret;

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
                            OSLeaveMutex(hDataBufferMutex);
                            break;
                        }

                        fatalError = TRUE;
                    }
                    else if (ret == 0)
                    {
                        fatalError = TRUE;
                    }

                    if (fatalError)
                    {
                        //connection closed, or connection was aborted / socket closed / etc, that's a fatal error for us.
                        Log(TEXT("RTMPPublisher::SocketLoop: Socket error, send() returned %d, GetLastError() %d"), ret, errorCode);
                        OSLeaveMutex(hDataBufferMutex);
                        App->PostStopMessage();
                        bStopping = true;
                        return;
                    }
                }

                if (curDataBufferLen == 0)
                    ResetEvent(hBufferEvent);

                OSLeaveMutex(hDataBufferMutex);
            }
        }
    } while (!bStopping);

    Log(TEXT("RTMPPublisher::SocketLoop: Aborting due to loop exit"));
}

void RTMPPublisher::SendLoop()
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    while(WaitForSingleObject(hSendSempahore, INFINITE) == WAIT_OBJECT_0)
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

        while(true)
        {
            if(bStopping)
                return;

            OSEnterMutex(hDataMutex);
            if(queuedPackets.Num() == 0)
            {
                OSLeaveMutex(hDataMutex);
                break;
            }

            bool bFoundPacket = false;

            if(queuedPackets[0].timestamp < sendTime)
            {
                NetworkPacket &packet = queuedPackets[0];

                bFoundPacket = true;
                currentBufferSize -= packet.data.Num();
            }
            else
            {
                OSLeaveMutex(hDataMutex);
                break;
            }

            List<BYTE> packetData;
            PacketType type       = queuedPackets[0].type;
            DWORD      timestamp  = queuedPackets[0].timestamp;
            packetData.TransferFrom(queuedPackets[0].data);

            queuedPackets.Remove(0);

            OSLeaveMutex(hDataMutex);

            //--------------------------------------------

            if(ignoreCount > 0)
            {
                if(--ignoreCount == 0)
                {
                    //Log(TEXT("ignore complete"));
                    bNetworkStrain = false;
                }
            }

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
                if(!RTMP_IsConnected(rtmp))
                {
                    App->PostStopMessage();
                    bStopping = true;
                    break;
                }

                RUNONCE Log(TEXT("okay, this is strange"));
            }

            /*QWORD sendTimeTotal = OSGetTimeMicroseconds()-sendTimeStart;
            Log(TEXT("send time: %llu"), sendTimeTotal);
            totalTime += sendTimeTotal;
            totalCalls++;*/

            //----------------------------------------------------------

            outputRateSize += packetData.Num();
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
            }

            //----------------------------------------------------------
            //make sure to flush the send buffer if surpassing max latency to keep frame data synced up on the server

            if(bUseSendBuffer)
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

                    numVideoPacketsBuffered = 0;
                }

                numVideoPacketsBuffered++;
            }

            //----------------------------------------------------------

            bytesSent += packetData.Num();
        }
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
    bool bWasEmpty = false;
    bool bComplete = false;
    int fullLen = len;

retrySend:

    OSEnterMutex(network->hDataBufferMutex);

    if (network->curDataBufferLen + len >= network->dataBufferSize)
    {
        Log(TEXT("RTMPPublisher::BufferedSend: Buffer is full (%d / %d bytes), waiting to send %d bytes"), network->curDataBufferLen, network->dataBufferSize, len);
        OSLeaveMutex(network->hDataBufferMutex);
        int status = WaitForSingleObject(network->hBufferSpaceAvailableEvent, INFINITE);
        if (status == WAIT_ABANDONED || status == WAIT_FAILED)
            return 0;
        goto retrySend;
    }

    if (!network->curDataBufferLen)
        bWasEmpty = true;

    mcpy(network->dataBuffer + network->curDataBufferLen, buf, len);
    network->curDataBufferLen += len;

    if (bWasEmpty)
        SetEvent (network->hBufferEvent);

    OSLeaveMutex(network->hDataBufferMutex);

    return len;

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

            network->numVideoPacketsBuffered = 0;
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

NetworkStream* CreateRTMPPublisher()
{
    return new RTMPPublisher;
}
