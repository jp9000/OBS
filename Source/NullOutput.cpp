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


class NullVideoEncoder : public VideoEncoder
{
public:
    virtual bool Encode(LPVOID picIn, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD timestamp, int &ctsOffset) {return true;}
    virtual void GetHeaders(DataPacket &packet) {}
   // virtual void GetSEI(DataPacket &packet) {}
    virtual int  GetBitRate() const {return 0;}
    virtual String GetInfoString() const {return String();}

    virtual bool DynamicBitrateSupported() const {return false;}
    virtual bool SetBitRate(DWORD maxBitrate, DWORD bufferSize) {return false;}
};

class NullAudioEncoder : public AudioEncoder
{
public:
    virtual bool    Encode(float *input, UINT numInputFrames, DataPacket &packet, DWORD &timestamp) {return true;}
    virtual void    GetHeaders(DataPacket &packet) {}
    virtual UINT    GetFrameSize() const {return 0;}
    virtual int     GetBitRate() const {return 0;}
    virtual CTSTR   GetCodec() const {return NULL;}
    virtual String  GetInfoString() const {return String();}
};

class NullNetwork : public NetworkStream
{
    virtual void SendPacket(BYTE *data, UINT size, DWORD timestamp, PacketType type) {}

    double GetPacketStrain() const {return 0;}
    QWORD GetCurrentSentBytes() {return 0;}
    virtual DWORD NumDroppedFrames() const {return 0;}
};


VideoEncoder*  CreateNullVideoEncoder() {return new NullVideoEncoder;}
AudioEncoder*  CreateNullAudioEncoder() {return new NullAudioEncoder;}
NetworkStream* CreateNullNetwork()      {return new NullNetwork;}
