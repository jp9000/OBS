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


#pragma once

//-------------------------------------------
// sockets and whatever

#include <winsock2.h>
#include <ws2tcpip.h>

#include <inttypes.h>
#include "../librtmp/rtmp.h"
#include "../librtmp/log.h"


//just so you know, I'm fairly disgusted with all this stuff.
//librtmp is a nice library, but they really need to work on making things function more..  cleanly.

#define SAVC(x)    static const AVal av_##x = AVC(#x)
#define STR2AVAL(av,str) av.av_val = str; av.av_len = (int)strlen(av.av_val)

SAVC(app);
SAVC(connect);
SAVC(flashVer);
SAVC(swfUrl);
SAVC(pageUrl);
SAVC(tcUrl);
SAVC(fpad);
SAVC(capabilities);
SAVC(audioCodecs);
SAVC(videoCodecs);
SAVC(videoFunction);
SAVC(objectEncoding);
SAVC(_result);
SAVC(FCSubscribe);
SAVC(onFCSubscribe);
SAVC(createStream);
SAVC(deleteStream);
SAVC(getStreamLength);
SAVC(play);
SAVC(fmsVer);
SAVC(mode);
SAVC(level);
SAVC(code);
SAVC(description);
SAVC(secureToken);

SAVC(send);

SAVC(onMetaData);
SAVC(duration);
SAVC(width);
SAVC(height);
SAVC(videocodecid);
SAVC(videodatarate);
SAVC(framerate);
SAVC(audiocodecid);
SAVC(audiodatarate);
SAVC(audiosamplerate);
SAVC(audiosamplesize);
SAVC(audiochannels);
SAVC(stereo);
SAVC(encoder);
SAVC(fileSize);

SAVC(onStatus);
SAVC(status);
static const AVal av_NetStream_Play_Start = AVC("NetStream.Play.Start");
static const AVal av_Started_playing = AVC("Started playing");
static const AVal av_NetStream_Play_Stop = AVC("NetStream.Play.Stop");
static const AVal av_Stopped_playing = AVC("Stopped playing");
SAVC(details);
SAVC(clientid);
static const AVal av_NetStream_Authenticate_UsherToken = AVC("NetStream.Authenticate.UsherToken");

static const AVal av_setDataFrame = AVC("@setDataFrame");

static const AVal av_dquote = AVC("\"");
static const AVal av_escdquote = AVC("\\\"");

static const AVal av_OBSVersion = AVC(OBS_VERSION_STRING_ANSI);
SAVC(avc1);
SAVC(mp4a);
static const AVal av_mp3 = AVC("mp3 ");

int SendResultNumber(RTMP *r, double txn, double ID);
int SendConnectResult(RTMP *r, double txn);
void AVreplace(AVal *src, const AVal *orig, const AVal *repl);
int SendPlayStart(RTMP *r);
int SendPlayStop(RTMP *r);
char* EncMetaData(char *enc, char *pend);
