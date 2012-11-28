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

NetworkStream* CreateRTMPPublisher();


class DelayedPublisher : public NetworkStream
{
    DWORD delayTime;
    DWORD lastTimestamp;
    List<NetworkPacket> queuedPackets;

    RTMPPublisher *outputStream;

    bool bConnected;
    bool bStreamEnding, bCancelEnd;

    static INT_PTR CALLBACK EndDelayProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch(message)
        {
            case WM_INITDIALOG:
                LocalizeWindow(hwnd);
                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)lParam);
                return TRUE;

            case WM_COMMAND:
                if(LOWORD(wParam) == IDCANCEL)
                {
                    DelayedPublisher *publisher = (DelayedPublisher*)GetWindowLongPtr(hwnd, DWLP_USER);
                    publisher->bCancelEnd = true;
                }
                break;

            case WM_CLOSE:
                {
                    DelayedPublisher *publisher = (DelayedPublisher*)GetWindowLongPtr(hwnd, DWLP_USER);
                    publisher->bCancelEnd = true;
                }
        }
        return 0;
    }

    void ProcessPackets(DWORD timestamp)
    {
        if(bCancelEnd)
            return;

        if(timestamp >= delayTime)
        {
            if(!bConnected)
            {
                outputStream = (RTMPPublisher*)CreateRTMPPublisher();
                bConnected = true;
            }

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

public:
    inline DelayedPublisher(DWORD delayTime)
    {
        this->delayTime = delayTime;
    }

    ~DelayedPublisher()
    {
        if(!outputStream || !outputStream->bStopping)
        {
            bStreamEnding = true;
            HWND hwndProgressDialog = CreateDialogParam(hinstMain, MAKEINTRESOURCE(IDD_ENDINGDELAY), hwndMain, (DLGPROC)EndDelayProc, (LPARAM)this);
            ProcessEvents();

            ShowWindow(hwndProgressDialog, TRUE);

            DWORD totalTimeLeft = delayTime;

            String strTimeLeftVal = Str("EndingDelay.TimeLeft");

            DWORD lastTimeLeft = -1;

            DWORD firstTime = OSGetTime();
            while(queuedPackets.Num() && !bCancelEnd)
            {
                ProcessEvents();

                DWORD timeElapsed = (OSGetTime()-firstTime);

                DWORD timeLeft = (totalTimeLeft-timeElapsed)/1000;
                DWORD timeLeftMinutes = timeLeft/60;
                DWORD timeLeftSeconds = timeLeft%60;

                if(timeLeft != lastTimeLeft)
                {
                    String strTimeLeft = strTimeLeftVal;
                    strTimeLeft.FindReplace(TEXT("$1"), FormattedString(TEXT("%u:%02u"), timeLeftMinutes, timeLeftSeconds));
                    SetWindowText(GetDlgItem(hwndProgressDialog, IDC_TIMELEFT), strTimeLeft);
                    lastTimeLeft = timeLeft;
                }

                ProcessPackets(lastTimestamp+timeElapsed);
                if(outputStream && outputStream->bStopping)
                    bCancelEnd = true;

                Sleep(10);
            }

            DestroyWindow(hwndProgressDialog);
        }

        for(UINT i=0; i<queuedPackets.Num(); i++)
            queuedPackets[i].data.Clear();

        delete outputStream;
    }

    void SendPacket(BYTE *data, UINT size, DWORD timestamp, PacketType type)
    {
        ProcessPackets(timestamp);

        NetworkPacket *newPacket = queuedPackets.CreateNew();
        newPacket->data.CopyArray(data, size);
        newPacket->timestamp = timestamp;
        newPacket->type = type;

        lastTimestamp = timestamp;
    }

    void BeginPublishing() {}

    double GetPacketStrain() const {return (outputStream) ? outputStream->GetPacketStrain() : 0;}
    QWORD GetCurrentSentBytes() {return (outputStream) ? outputStream->GetCurrentSentBytes() : 0;}
    DWORD NumDroppedFrames() const {return (outputStream) ? outputStream->NumDroppedFrames() : 0;}
};


NetworkStream* CreateDelayedPublisher(DWORD delayTime)
{
    return new DelayedPublisher(delayTime*1000);
}
