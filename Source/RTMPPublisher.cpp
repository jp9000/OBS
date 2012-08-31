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

    String chi(lpTemp);
    chi << TEXT("\r\n");

    OSDebugOut(TEXT("%s"), chi);
    Log(TEXT("%s"), chi);

    Free(lpTemp);
}


struct NetworkPacket
{
    List<BYTE> data;
    DWORD timestamp;
    bool bAudio;
};

class RTMPPublisher : public NetworkStream
{
    RTMP *rtmp;

    HANDLE hSendSempahore;
    HANDLE hDataMutex;
    HANDLE hSendThread;

    bool bStopping;

    //all data sending is done in yet another separate thread to prevent blocking in the main capture thread
    void SendLoop()
    {
        traceIn(RTMPPublisher::SendLoop);

        while(WaitForSingleObject(hSendSempahore, INFINITE) == WAIT_OBJECT_0 && !bStopping && RTMP_IsConnected(rtmp))
        {
            OSEnterMutex(hDataMutex);
            if(!Packets.Num())
                ProgramBreak();

            LPBYTE     data      = Packets[0].data.Array();
            UINT       size      = Packets[0].data.Num();
            bool       bAudio    = Packets[0].bAudio;
            DWORD      timestamp = Packets[0].timestamp;
            OSLeaveMutex(hDataMutex);

            //--------------------------------------------

            RTMPPacket packet;
            packet.m_nChannel = bAudio ? 0x5 : 0x4;
            packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
            packet.m_packetType = bAudio ? RTMP_PACKET_TYPE_AUDIO : RTMP_PACKET_TYPE_VIDEO;
            packet.m_nTimeStamp = timestamp;
            packet.m_nInfoField2 = rtmp->m_stream_id;
            packet.m_hasAbsTimestamp = TRUE;

            packet.m_nBodySize = size-RTMP_MAX_HEADER_SIZE;
            packet.m_body = (char*)data+RTMP_MAX_HEADER_SIZE;

            if(!RTMP_SendPacket(rtmp, &packet, FALSE))
            {
                if(!RTMP_IsConnected(rtmp))
                {
                    App->PostStopMessage();
                    break;
                }
            }

            //--------------------------------------------

            OSEnterMutex(hDataMutex);
            Packets[0].data.Clear();
            Packets.Remove(0);
            OSLeaveMutex(hDataMutex);
        }

        traceOut;
    }

    static DWORD SendThread(RTMPPublisher *publisher)
    {
        publisher->SendLoop();
        return 0;
    }

    List<NetworkPacket> Packets;

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

        //--------------------------

        if(rtmp)
        {
            RTMP_Close(rtmp);
            RTMP_Free(rtmp);
        }

        traceOut;
    }

    void SendPacket(BYTE *data, UINT size, DWORD timestamp, bool bAudio)
    {
        traceIn(RTMPPublisher::SendPacket);

        if(!bStopping)
        {
            List<BYTE> paddedData;
            paddedData.SetSize(size+RTMP_MAX_HEADER_SIZE);
            mcpy(paddedData.Array()+RTMP_MAX_HEADER_SIZE, data, size);

            OSEnterMutex(hDataMutex);
            NetworkPacket &netPacket = *Packets.CreateNew();
            netPacket.bAudio = bAudio;
            netPacket.timestamp = timestamp;
            netPacket.data.TransferFrom(paddedData);
            OSLeaveMutex(hDataMutex);

            ReleaseSemaphore(hSendSempahore, 1, NULL);

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

        DataPacket mediaHeaders;
        App->GetVideoHeaders(mediaHeaders);

        List<BYTE> packetPadding;
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
    //RTMP_LogSetCallback(rtmp_log_output);
    //RTMP_LogSetLevel(RTMP_LOGDEBUG);

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
