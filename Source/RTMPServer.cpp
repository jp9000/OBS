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


class RTMPServer;


void rtmp_log_output(int level, const char *format, va_list vl);


//all connections should be handled in the same thread with select, but because the rtmp server is mostly just for
//personal or testing purposes (and because I'm lazy), I'm just putting each socket in their own threads
class Connection
{
    friend class RTMPServer;

    RTMP *rtmp;
    SOCKET socket;
    sockaddr_storage addr;
    char host[NI_MAXHOST];
    char service[NI_MAXSERV];
    bool bReadyForOutput;
    bool bDestroySocket;
    int streamChannel;

    RTMPServer *server;

    HANDLE hThread;

    static DWORD STDCALL SocketThread(Connection *connection);

    void SendMetaData();

public:
    inline Connection(SOCKET socket, RTMPServer *server, sockaddr_storage *addrIn, char *host, char *service)
    {
        this->server = server;
        this->socket = socket;
        mcpy(&addr, addrIn, sizeof(addr));
        strcpy(this->host, host);
        strcpy(this->service, service);

        hThread = OSCreateThread((XTHREAD)SocketThread, (LPVOID)this);
    }

    inline ~Connection()
    {
        if(RTMP_IsConnected(rtmp) && bReadyForOutput)
            SendPlayStop(rtmp);

        if(hThread)
        {
            if(!bDestroySocket)
            {
                bDestroySocket = true;
                OSWaitForThread(hThread, NULL);
            }

            OSCloseThread(hThread);
        }
    }

    inline bool ReadyForOutput() const {return bReadyForOutput;}
    inline RTMP* GetRTMP() const {return rtmp;}

    int DoInvoke(RTMPPacket *packet, unsigned int offset);
};


class RTMPServer : public NetworkStream
{
    friend class Connection;

    SOCKET serverSocket;

    bool bDestroySockets;
    List<Connection*> connections;
    HANDLE hListMutex, hListenThread;


    void ListenLoop()
    {
        traceIn(RTMPServer::ListenLoop);

        fd_set socketlist;
        timeval duration;
        sockaddr_storage addr;
        char host[NI_MAXHOST], service[NI_MAXSERV];

        duration.tv_sec = 0;
        duration.tv_usec = 20000;

        while(!bDestroySockets)
        {
            FD_ZERO(&socketlist);
            FD_SET(serverSocket, &socketlist);

            int ret = select(0, &socketlist, NULL, NULL, &duration);
            if(ret == -1)
            {
                int chi = WSAGetLastError();
                AppWarning(TEXT("Got error %d from a select on the server socket"), chi);
            }
            else if(ret != 0)
            {
                int addrLen = sizeof(addr);
                SOCKET clientSocket = accept(serverSocket, (sockaddr*)&addr, &addrLen);
                if(clientSocket == INVALID_SOCKET)
                    continue;

                getnameinfo((struct sockaddr *)&addr, addrLen,
                    host, sizeof(host),
                    service, sizeof(service),
                    NI_NUMERICHOST);

                OSEnterMutex(hListMutex);
                connections << new Connection(clientSocket, this, &addr, host, service);
                OSLeaveMutex(hListMutex);
            }
        }

        traceOut;
    }

    static DWORD STDCALL ListenThread(RTMPServer *server)
    {
        server->ListenLoop();
        return 0;
    }


public:
    RTMPServer()
    {
        traceIn(RTMPServer::RTMPServer);

        //RTMP_LogSetCallback(rtmp_log_output);
        //RTMP_LogSetLevel(RTMP_LOGDEBUG);

        bDestroySockets = false;

        hListMutex = OSCreateMutex();

        addrinfo *ai_save, *local_ai, hint;
        zero(&hint, sizeof(hint));

        hint.ai_flags       = AI_PASSIVE;
        hint.ai_family      = AF_INET;//AF_UNSPEC;//
        hint.ai_socktype    = SOCK_STREAM;

        DWORD ret = getaddrinfo(NULL, "1935", &hint, &local_ai);
        if(ret)
            return; //this should actually never happen

        ai_save = local_ai;

        serverSocket = -1;
        while(local_ai)
        {
            serverSocket = socket(local_ai->ai_family, local_ai->ai_socktype, local_ai->ai_protocol);
            int err = WSAGetLastError();

            if(serverSocket != INVALID_SOCKET)
            {
                if(bind(serverSocket, local_ai->ai_addr, (int)local_ai->ai_addrlen) == 0)
                    break;

                closesocket(serverSocket);
                serverSocket = -1;
            }

            local_ai = local_ai->ai_next;
        }

        if(serverSocket != INVALID_SOCKET)
        {
            ret = listen(serverSocket, SOMAXCONN);
            if(ret)
            {
                ret = WSAGetLastError();
                AppWarning(TEXT("Failed to set up the server"));
            }
            else
                hListenThread = OSCreateThread((XTHREAD)ListenThread, (LPVOID)this);
        }
        else
            AppWarning(TEXT("Failed to get a server socket"));

        freeaddrinfo(ai_save);

        traceOut;
    }

    ~RTMPServer()
    {
        traceIn(RTMPServer::~RTMPServer);

        if(hListenThread)
        {
            bDestroySockets = true;
            OSWaitForThread(hListenThread, NULL);
            OSCloseThread(hListenThread);
        }

        if(hListMutex)
            OSCloseMutex(hListMutex);

        for(UINT i=0; i<connections.Num(); i++)
            delete connections[i];
        connections.Clear();

        if(serverSocket)
            closesocket(serverSocket);

        traceOut;
    }

    void SendPacket(BYTE *data, UINT size, DWORD timestamp, PacketType type)
    {
        traceIn(RTMPServer::SendPacket);

        RTMPPacket packet;
        packet.m_nChannel = (type == PacketType_Audio) ? 0x5 : 0x4;
        packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
        packet.m_packetType = (type == PacketType_Audio) ? RTMP_PACKET_TYPE_AUDIO : RTMP_PACKET_TYPE_VIDEO;
        packet.m_nTimeStamp = timestamp;
        packet.m_nInfoField2 = 1;
        packet.m_hasAbsTimestamp = TRUE;

        List<BYTE> IAmNotHappyRightNow;
        IAmNotHappyRightNow.SetSize(RTMP_MAX_HEADER_SIZE);
        IAmNotHappyRightNow.AppendArray(data, size);

        packet.m_nBodySize = size;
        packet.m_body = (char*)IAmNotHappyRightNow.Array()+RTMP_MAX_HEADER_SIZE;

        OSEnterMutex(hListMutex);
        for(UINT i=0; i<connections.Num(); i++)
        {
            Connection *connection = connections[i];

            if(connection->ReadyForOutput())
            {
                if(!RTMP_SendPacket(connection->GetRTMP(), &packet, FALSE))
                {
                    delete connection;
                    connections.Remove(i--);
                }
            }
        }
        OSLeaveMutex(hListMutex);

        traceOut;
    }

    double GetPacketStrain() const {return 0.0;}
    UINT GetBytesPerSec()  const {return 0;}
};


int Connection::DoInvoke(RTMPPacket *packet, unsigned int offset)
{
    const char *body;
    unsigned int nBodySize;
    int ret = 0, nRes;

    body = packet->m_body + offset;
    nBodySize = packet->m_nBodySize - offset;

    if (body[0] != 0x02)
        return 0;

    //---------------------------------------------------

    AMFObject obj;
    nRes = AMF_Decode(&obj, body, nBodySize, FALSE);
    if (nRes < 0)
        return 0;

    //---------------------------------------------------

    AVal method;
    AMFProp_GetString(AMF_GetProp(&obj, NULL, 0), &method);
    double txn = AMFProp_GetNumber(AMF_GetProp(&obj, NULL, 1));

    if(AVMATCH(&method, &av_connect))
    {
        AMFObject connectObj;
        AVal pname, pval;

        AMFProp_GetObject(AMF_GetProp(&obj, NULL, 2), &connectObj);
        for(int i=0; i<connectObj.o_num; i++)
        {
            pname = connectObj.o_props[i].p_name;
            pval.av_val = NULL;
            pval.av_len = 0;

            if (connectObj.o_props[i].p_type == AMF_STRING)
                pval = connectObj.o_props[i].p_vu.p_aval;

            pval.av_val = NULL;

            if (AVMATCH(&pname, &av_app))
            {
                rtmp->Link.app = pval;
                if (!rtmp->Link.app.av_val) rtmp->Link.app.av_val = "";
            }
            else if (AVMATCH(&pname, &av_flashVer))
                rtmp->Link.flashVer = pval;
            else if (AVMATCH(&pname, &av_swfUrl))
                rtmp->Link.swfUrl = pval;
            else if (AVMATCH(&pname, &av_tcUrl))
                rtmp->Link.tcUrl = pval;
            else if (AVMATCH(&pname, &av_pageUrl))
                rtmp->Link.pageUrl = pval;
            else if (AVMATCH(&pname, &av_audioCodecs))
                rtmp->m_fAudioCodecs = connectObj.o_props[i].p_vu.p_number;
            else if (AVMATCH(&pname, &av_videoCodecs))
                rtmp->m_fVideoCodecs = connectObj.o_props[i].p_vu.p_number;
            else if (AVMATCH(&pname, &av_objectEncoding))
                rtmp->m_fEncoding = connectObj.o_props[i].p_vu.p_number;
        }

        SendConnectResult(rtmp, txn);
    }
    else if(AVMATCH(&method, &av_createStream))
        SendResultNumber(rtmp, txn, 1.0);
    else if(AVMATCH(&method, &av_deleteStream))
        ret = 1;
    else if(AVMATCH(&method, &av_getStreamLength))
        SendResultNumber(rtmp, txn, 0.0);
    else if(AVMATCH(&method, &av_NetStream_Authenticate_UsherToken))
    {
        AVal usherToken;
        AMFProp_GetString(AMF_GetProp(&obj, NULL, 3), &usherToken);
        AVreplace(&usherToken, &av_dquote, &av_escdquote);
        rtmp->Link.usherToken = usherToken;
    }
    else if(AVMATCH(&method, &av_play))
    {
        RTMP_SendCtrl(rtmp, 0, 1, 0);
        SendPlayStart(rtmp);

        streamChannel = packet->m_nInfoField2;

        SendMetaData();

        //-----------------------------------------------------
        // send video headers

        DataPacket headers;

        App->GetVideoHeaders(headers);

        RTMPPacket headerPacket;
        headerPacket.m_nChannel = 0x04;     // source channel (invoke)
        headerPacket.m_headerType = RTMP_PACKET_SIZE_LARGE;
        headerPacket.m_packetType = RTMP_PACKET_TYPE_VIDEO;
        headerPacket.m_nTimeStamp = 0;
        headerPacket.m_nInfoField2 = 1;
        headerPacket.m_hasAbsTimestamp = 1;

        List<BYTE> IAmNotHappyRightNow;
        IAmNotHappyRightNow.SetSize(RTMP_MAX_HEADER_SIZE);
        IAmNotHappyRightNow.AppendArray(headers.lpPacket, headers.size);

        headerPacket.m_nBodySize = headers.size;
        headerPacket.m_body = (char*)IAmNotHappyRightNow.Array()+RTMP_MAX_HEADER_SIZE;

        RTMP_SendPacket(rtmp, &headerPacket, FALSE);

        //-----------------------------------------------------
        // send audio headers

        headerPacket.m_nChannel = 0x05;     // audio channel
        headerPacket.m_packetType = RTMP_PACKET_TYPE_AUDIO;

        App->GetAudioHeaders(headers);

        IAmNotHappyRightNow.SetSize(RTMP_MAX_HEADER_SIZE);
        IAmNotHappyRightNow.AppendArray(headers.lpPacket, headers.size);

        headerPacket.m_nBodySize = headers.size;
        headerPacket.m_body = (char*)IAmNotHappyRightNow.Array()+RTMP_MAX_HEADER_SIZE;

        RTMP_SendPacket(rtmp, &headerPacket, FALSE);

        //-----------------------------------------------------

        bReadyForOutput = true;
    }
    else if (AVMATCH(&method, &av_FCSubscribe))
    {
        
    }

    return ret;
}

void Connection::SendMetaData()
{
    RTMPPacket packet;

    //---------------------------------------------------

    char pbuf[1024], *pend = pbuf+sizeof(pbuf);

    packet.m_nChannel = 0x03;     // control channel (invoke)
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INFO;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    char *enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_onMetaData);
    enc = App->EncMetaData(enc, pend);

    packet.m_nBodySize = enc - packet.m_body;
    RTMP_SendPacket(rtmp, &packet, FALSE);

    bReadyForOutput = true;
}


DWORD STDCALL Connection::SocketThread(Connection *connection)
{
    fd_set socketlist;
    timeval duration;
    RTMP *clientRtmp;
    RTMPPacket packet;

    clientRtmp = RTMP_Alloc();
    RTMP_Init(clientRtmp);
    zero(&packet, sizeof(packet));

    //-----------------------------------------------

    duration.tv_sec = 0;
    duration.tv_usec = 20000;

    FD_ZERO(&socketlist);
    FD_SET(connection->socket, &socketlist);

    if(select(0, &socketlist, NULL, NULL, &duration) <= 0)
    {
        DWORD chi = WSAGetLastError();
        return 0;
    }
    else
    {
        clientRtmp->m_sb.sb_socket = connection->socket;

        connection->rtmp = clientRtmp;

        if(!RTMP_Serve(clientRtmp))
        {
            RTMP_Close(clientRtmp);
            RTMP_Free(clientRtmp);
            return 0;
        }
    }

    //-----------------------------------------------

    while(!connection->server->bDestroySockets && !connection->bDestroySocket && RTMP_IsConnected(clientRtmp) && RTMP_ReadPacket(clientRtmp, &packet))
    {
        if(!RTMPPacket_IsReady(&packet))
            continue;

        switch(packet.m_packetType)
        {
        case RTMP_PACKET_TYPE_CHUNK_SIZE:
            //ChangeChunkSize(?);
            break;

        case RTMP_PACKET_TYPE_CONTROL:
            //SetControl(?);
            break;

        case RTMP_PACKET_TYPE_SERVER_BW:
            //ServerBandwidth(?);
            break;
        case RTMP_PACKET_TYPE_CLIENT_BW:
            //ClientBandwidth(?);
            break;

        case RTMP_PACKET_TYPE_FLEX_MESSAGE:
            //Invoke?
            break;

        case RTMP_PACKET_TYPE_INFO:
            //meta data thingy, shouldn't even get it, at least I think
            break;

        case RTMP_PACKET_TYPE_INVOKE:
            if(connection->DoInvoke(&packet, 0))
                connection->bDestroySocket = true;
            break;
        }

        RTMPPacket_Free(&packet);
    }

    if(!connection->bDestroySocket) //if we received a request to stop stream, automatically delete self
    {
        OSEnterMutex(connection->server->hListMutex);

        connection->bDestroySocket = true;
        connection->server->connections.RemoveItem(connection);
        delete connection;

        OSLeaveMutex(connection->server->hListMutex);
    }

    RTMP_Close(clientRtmp);
    RTMP_Free(clientRtmp);

    connection->rtmp = NULL;

    return 0;
}


NetworkStream* CreateRTMPServer()
{
    return new RTMPServer;
}

