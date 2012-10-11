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
#include <time.h>


time_t GetMacTime()
{
    return time(0)+2082844800;
}


struct SampleToChunk
{
    UINT firstChunkID;
    UINT samplesPerChunk;
};

struct OffsetVal
{
    UINT count;
    UINT val;
};

struct MP4VideoFrameInfo
{
    UINT64  fileOffset;
    UINT    size;
    UINT    timestamp;
    INT     compositionOffset;
};

struct MP4AudioFrameInfo
{
    UINT64  fileOffset;
    UINT    size;
};

#define USE_64BIT_MP4 1


void WINAPI ProcessEvents()
{
    MSG msg;
    while(PeekMessage(&msg, NULL, 0, 0, 1))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}


//code annoyance rating: fairly nightmarish

class MP4FileStream : public VideoFileStream
{
    XFileOutputSerializer fileOut;
    String strFile;

    List<MP4VideoFrameInfo> videoFrames;
    List<MP4AudioFrameInfo> audioFrames;

    List<UINT>      IFrameIDs;

    DWORD           lastVideoTimestamp;

    bool            bStreamOpened;
    bool            bMP3;

    List<BYTE>      endBuffer;
    List<UINT>      boxOffsets;

    UINT64 mdatStart, mdatStop;

    bool bSentFirstVideoPacket;
    bool bCancelMP4Build;

    void PushBox(BufferOutputSerializer &output, DWORD boxName)
    {
        boxOffsets.Insert(0, endBuffer.Num());

        output.OutputDword(0);
        output.OutputDword(boxName);
    }

    void PopBox()
    {
        DWORD boxSize = endBuffer.Num()-boxOffsets[0];
        *(DWORD*)(endBuffer.Array()+boxOffsets[0]) = fastHtonl(boxSize);

        boxOffsets.Remove(0);
    }

    static INT_PTR CALLBACK MP4ProgressDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch(message)
        {
            case WM_INITDIALOG:
                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)lParam);
                return TRUE;

            case WM_COMMAND:
                switch(LOWORD(wParam))
                {
                    case IDCANCEL:
                        if(MessageBox(hwnd, Str("MP4ProgressDialog.ConfirmStop"), Str("MP4ProgressDialog.ConfirmStopTitle"), MB_YESNO) == IDYES)
                        {
                            MP4FileStream *fileStream = (MP4FileStream*)GetWindowLongPtr(hwnd, DWLP_USER);
                            fileStream->bCancelMP4Build = true;
                            EndDialog(hwnd, IDCANCEL);
                        }
                        break;
                }
        }
        return 0;
    }

public:
    bool Init(CTSTR lpFile)
    {
        strFile = lpFile;

        if(!fileOut.Open(lpFile, XFILE_CREATEALWAYS, 1024*1024))
            return false;

        fileOut.OutputDword(DWORD_BE(0x20));
        fileOut.OutputDword(DWORD_BE('ftyp'));
        fileOut.OutputDword(DWORD_BE('isom'));
        fileOut.OutputDword(DWORD_BE(0x200));
        fileOut.OutputDword(DWORD_BE('isom'));
        fileOut.OutputDword(DWORD_BE('iso2'));
        fileOut.OutputDword(DWORD_BE('avc1'));
        fileOut.OutputDword(DWORD_BE('mp41'));

        fileOut.OutputDword(DWORD_BE(0x8));
        fileOut.OutputDword(DWORD_BE('free'));

        mdatStart = fileOut.GetPos();
        fileOut.OutputDword(DWORD_BE(0x1));
        fileOut.OutputDword(DWORD_BE('mdat'));
#ifdef USE_64BIT_MP4
        fileOut.OutputQword(0);
#endif

        bMP3 = scmp(App->GetAudioEncoder()->GetCodec(), TEXT("MP3")) == 0;

        bStreamOpened = true;

        return true;
    }

    template<typename T> inline void GetChunkInfo(List<T> &data, List<UINT64> &chunks, List<SampleToChunk> &sampleToChunks)
    {
        UINT64 curChunkOffset;
        UINT64 connectedSampleOffset;
        UINT numSamples = 0;

        for(UINT i=0; i<data.Num(); i++)
        {
            UINT64 curOffset = data[i].fileOffset;
            if(i == 0)
                curChunkOffset = curOffset;
            else
            {
                if(curOffset != connectedSampleOffset)
                {
                    chunks << curChunkOffset;
                    if(!sampleToChunks.Num() || sampleToChunks.Last().samplesPerChunk != numSamples)
                    {
                        SampleToChunk stc;
                        stc.firstChunkID = chunks.Num();
                        stc.samplesPerChunk = numSamples;
                        sampleToChunks << stc;
                    }

                    curChunkOffset = curOffset;
                    numSamples = 0;
                }
            }

            numSamples++;
            connectedSampleOffset = curOffset+data[i].size;
        }

        chunks << curChunkOffset;
        if(!sampleToChunks.Num() || sampleToChunks.Last().samplesPerChunk != numSamples)
        {
            SampleToChunk stc;
            stc.firstChunkID = chunks.Num();
            stc.samplesPerChunk = numSamples;
            sampleToChunks << stc;
        }
    }

    ~MP4FileStream()
    {
        if(!bStreamOpened)
            return;

        HWND hwndProgressDialog = CreateDialog(hinstMain, MAKEINTRESOURCE(IDD_BUILDINGMP4), hwndMain, (DLGPROC)MP4ProgressDialogProc);
        SendMessage(GetDlgItem(hwndProgressDialog, IDC_PROGRESS1), PBM_SETRANGE32, 0, 100);

        mdatStop = fileOut.GetPos();

        BufferOutputSerializer output(endBuffer);

        DWORD macTime = fastHtonl(DWORD(GetMacTime()));
        UINT videoDuration = fastHtonl(lastVideoTimestamp + App->GetFrameTime());
        UINT audioDuration = fastHtonl(lastVideoTimestamp + DWORD(double(App->GetAudioEncoder()->GetFrameSize())/44.1));
        UINT width, height;
        App->GetOutputSize(width, height);

        LPCSTR lpVideoTrack = "videoTrack";
        LPCSTR lpAudioTrack = "audioTrack";

        //-------------------------------------------
        // get video headers
        DataPacket videoHeaders;
        App->GetVideoHeaders(videoHeaders);
        List<BYTE> SPS, PPS;

        LPBYTE lpHeaderData = videoHeaders.lpPacket+11;
        SPS.CopyArray(lpHeaderData+2, fastHtons(*(WORD*)lpHeaderData));

        lpHeaderData += SPS.Num()+3;
        PPS.CopyArray(lpHeaderData+2, fastHtons(*(WORD*)lpHeaderData));

        //-------------------------------------------
        // get AAC headers if using AAC
        List<BYTE> AACHeader;
        if(!bMP3)
        {
            DataPacket data;
            App->GetAudioHeaders(data);
            AACHeader.CopyArray(data.lpPacket+2, data.size-2);
        }

        //-------------------------------------------
        // get chunk info
        List<UINT64> videoChunks, audioChunks;
        List<SampleToChunk> videoSampleToChunk, audioSampleToChunk;

        GetChunkInfo<MP4VideoFrameInfo>(videoFrames, videoChunks, videoSampleToChunk);
        GetChunkInfo<MP4AudioFrameInfo>(audioFrames, audioChunks, audioSampleToChunk);

        //-------------------------------------------
        // build decode time list and composition offset list
        List<OffsetVal> decodeTimes;
        List<OffsetVal> compositionOffsets;

        for(UINT i=0; i<videoFrames.Num(); i++)
        {
            UINT frameTime;
            if(i == videoFrames.Num()-1)
                frameTime = decodeTimes.Last().val;
            else
                frameTime = videoFrames[i+1].timestamp-videoFrames[i].timestamp;

            if(!decodeTimes.Num() || decodeTimes.Last().val != (UINT)frameTime)
            {
                OffsetVal newVal;
                newVal.count = 1;
                newVal.val = (UINT)frameTime;
                decodeTimes << newVal;
            }
            else
                decodeTimes.Last().count++;

            INT compositionOffset = videoFrames[i].compositionOffset;
            if(!compositionOffsets.Num() || compositionOffsets.Last().val != (UINT)compositionOffset)
            {
                OffsetVal newVal;
                newVal.count = 1;
                newVal.val = (UINT)compositionOffset;
                compositionOffsets << newVal;
            }
            else
                compositionOffsets.Last().count++;

            SendMessage(GetDlgItem(hwndProgressDialog, IDC_PROGRESS1), PBM_SETPOS, (i*20)/videoFrames.Num(), 0);
            ProcessEvents();
        }

        SendMessage(GetDlgItem(hwndProgressDialog, IDC_PROGRESS1), PBM_SETPOS, 20, 0);

        //-------------------------------------------
        // sound descriptor thingy.  this part made me die a little inside admittedly.
        UINT maxBitRate = fastHtonl(App->GetAudioEncoder()->GetBitRate()*1024);

        List<BYTE> esDecoderDescriptor;
        BufferOutputSerializer esDecoderOut(esDecoderDescriptor);
        esDecoderOut.OutputByte(bMP3 ? 107 : 64);
        esDecoderOut.OutputByte(0x15); //stream/type flags.  always 0x15 for my purposes.
        esDecoderOut.OutputWord(0); //buffer size (seems ignorable from my testing, so 0)
        esDecoderOut.OutputByte(0);
        esDecoderOut.OutputDword(maxBitRate); //max bit rate (cue bill 'o reily meme for these two)
        esDecoderOut.OutputDword(maxBitRate); //avg bit rate

        if(!bMP3) //if AAC, put in headers
        {
            esDecoderOut.OutputByte(0x5);  //decoder specific descriptor type
            esDecoderOut.OutputByte(0x80); //some stuff that no one should probably care about
            esDecoderOut.OutputByte(0x80);
            esDecoderOut.OutputByte(0x80);
            esDecoderOut.OutputByte(AACHeader.Num());
            esDecoderOut.Serialize((LPVOID)AACHeader.Array(), AACHeader.Num());
        }

        esDecoderOut.OutputByte(0x6);  //config descriptor type
        esDecoderOut.OutputByte(0x80); //some stuff that no one should probably care about
        esDecoderOut.OutputByte(0x80);
        esDecoderOut.OutputByte(0x80);
        esDecoderOut.OutputByte(1); //len
        esDecoderOut.OutputByte(2); //SL value(? always 2)

        List<BYTE> esDescriptor;
        BufferOutputSerializer esOut(esDescriptor);
        esOut.OutputWord(0); //es id
        esOut.OutputByte(0); //stream priority
        esOut.OutputByte(4); //descriptor type
        esOut.OutputByte(0x80); //some stuff that no one should probably care about
        esOut.OutputByte(0x80);
        esOut.OutputByte(0x80);
        esOut.OutputByte(esDecoderDescriptor.Num());
        esOut.Serialize((LPVOID)esDecoderDescriptor.Array(), esDecoderDescriptor.Num());

        //-------------------------------------------

        PushBox(output, DWORD_BE('moov'));

          //------------------------------------------------------
          // header
          PushBox(output, DWORD_BE('mvhd'));
            output.OutputDword(0); //version and flags (none)
            output.OutputDword(macTime); //creation time
            output.OutputDword(macTime); //modified time
            output.OutputDword(DWORD_BE(1000)); //time base (milliseconds, so 1000)
            output.OutputDword(videoDuration); //duration (in time base units)
            output.OutputDword(DWORD_BE(0x00010000)); //fixed point playback speed 1.0
            output.OutputWord(WORD_BE(0x0100)); //fixed point vol 1.0
            output.OutputQword(0); //reserved (10 bytes)
            output.OutputWord(0);
            output.OutputDword(DWORD_BE(0x00010000)); output.OutputDword(DWORD_BE(0x00000000)); output.OutputDword(DWORD_BE(0x00000000)); //window matrix row 1 (1.0, 0.0, 0.0)
            output.OutputDword(DWORD_BE(0x00000000)); output.OutputDword(DWORD_BE(0x00010000)); output.OutputDword(DWORD_BE(0x00000000)); //window matrix row 2 (0.0, 1.0, 0.0)
            output.OutputDword(DWORD_BE(0x00000000)); output.OutputDword(DWORD_BE(0x00000000)); output.OutputDword(DWORD_BE(0x40000000)); //window matrix row 3 (0.0, 0.0, 16384.0)
            output.OutputDword(0); //prevew start time (time base units)
            output.OutputDword(0); //prevew duration (time base units)
            output.OutputDword(0); //still poster frame (timestamp of frame)
            output.OutputDword(0); //selection(?) start time (time base units)
            output.OutputDword(0); //selection(?) duration (time base units)
            output.OutputDword(0); //current time (0, time base units)
            output.OutputDword(DWORD_BE(3)); //next free track id (1-based rather than 0-based)
          PopBox(); //mvhd

          //------------------------------------------------------
          // video track
          PushBox(output, DWORD_BE('trak'));
            PushBox(output, DWORD_BE('tkhd')); //track header
              output.OutputDword(DWORD_BE(0x0000000F)); //version (0) and flags (0xF)
              output.OutputDword(macTime); //creation time
              output.OutputDword(macTime); //modified time
              output.OutputDword(DWORD_BE(1)); //track ID
              output.OutputDword(0); //reserved
              output.OutputDword(videoDuration); //duration (in time base units)
              output.OutputQword(0); //reserved
              output.OutputWord(0); //video layer (0)
              output.OutputWord(0); //quicktime alternate track id (0)
              output.OutputWord(0); //track audio volume (this is video, so 0)
              output.OutputWord(0); //reserved
              output.OutputDword(DWORD_BE(0x00010000)); output.OutputDword(DWORD_BE(0x00000000)); output.OutputDword(DWORD_BE(0x00000000)); //window matrix row 1 (1.0, 0.0, 0.0)
              output.OutputDword(DWORD_BE(0x00000000)); output.OutputDword(DWORD_BE(0x00010000)); output.OutputDword(DWORD_BE(0x00000000)); //window matrix row 2 (0.0, 1.0, 0.0)
              output.OutputDword(DWORD_BE(0x00000000)); output.OutputDword(DWORD_BE(0x00000000)); output.OutputDword(DWORD_BE(0x40000000)); //window matrix row 3 (0.0, 0.0, 16384.0)
              output.OutputDword(fastHtonl(width<<16));  //width (fixed point)
              output.OutputDword(fastHtonl(height<<16)); //height (fixed point)
            PopBox(); //tkhd
            PushBox(output, DWORD_BE('edts'));
              PushBox(output, DWORD_BE('elst'));
                output.OutputDword(0); //version and flags (none)
                output.OutputDword(DWORD_BE(1)); //count
                output.OutputDword(videoDuration); //duration
                output.OutputDword(0); //start time
                output.OutputDword(DWORD_BE(0x00010000)); //playback speed (1.0)
              PopBox(); //elst
            PopBox(); //tdst
            PushBox(output, DWORD_BE('mdia'));
              PushBox(output, DWORD_BE('mdhd'));
                output.OutputDword(0); //version and flags (none)
                output.OutputDword(macTime); //creation time
                output.OutputDword(macTime); //modified time
                output.OutputDword(DWORD_BE(1000)); //time scale
                output.OutputDword(videoDuration);
                output.OutputDword(DWORD_BE(0x55c40000));
              PopBox(); //mdhd
              PushBox(output, DWORD_BE('hdlr'));
                output.OutputDword(0); //version and flags (none)
                output.OutputDword(0); //quicktime type (none)
                output.OutputDword(DWORD_BE('vide')); //media type
                output.OutputDword(0); //manufacturer reserved
                output.OutputDword(0); //quicktime component reserved flags
                output.OutputDword(0); //quicktime component reserved mask
                output.Serialize((LPVOID)lpVideoTrack, (DWORD)strlen(lpVideoTrack)+1); //track name
              PopBox(); //hdlr
              PushBox(output, DWORD_BE('minf'));
                PushBox(output, DWORD_BE('vmhd'));
                  output.OutputDword(DWORD_BE(0x00000001)); //version (0) and flags (1)
                  output.OutputWord(0); //quickdraw graphic mode (copy = 0)
                  output.OutputWord(0); //quickdraw red value
                  output.OutputWord(0); //quickdraw green value
                  output.OutputWord(0); //quickdraw blue value
                PopBox(); //vdhd
                PushBox(output, DWORD_BE('dinf'));
                  PushBox(output, DWORD_BE('dref'));
                    output.OutputDword(0); //version and flags (none)
                    output.OutputDword(DWORD_BE(1)); //count
                    PushBox(output, DWORD_BE('url '));
                      output.OutputDword(DWORD_BE(0x00000001)); //version (0) and flags (1)
                    PopBox(); //url
                  PopBox(); //dref
                PopBox(); //dinf
                PushBox(output, DWORD_BE('stbl'));
                  PushBox(output, DWORD_BE('stsd'));
                    output.OutputDword(0); //version and flags (none)
                    output.OutputDword(DWORD_BE(1)); //count
                    PushBox(output, DWORD_BE('avc1'));
                      output.OutputDword(0); //reserved 6 bytes
                      output.OutputWord(0);
                      output.OutputWord(WORD_BE(1)); //index
                      output.OutputWord(0); //encoding version
                      output.OutputWord(0); //encoding revision level
                      output.OutputDword(0); //encoding vendor
                      output.OutputDword(0); //temporal quality
                      output.OutputDword(0); //spatial quality
                      output.OutputWord(fastHtons(width)); //width
                      output.OutputWord(fastHtons(height)); //height
                      output.OutputDword(DWORD_BE(0x00480000)); //fixed point width pixel resolution (72.0)
                      output.OutputDword(DWORD_BE(0x00480000)); //fixed point height pixel resolution (72.0)
                      output.OutputDword(0); //quicktime video data size 
                      output.OutputWord(WORD_BE(1)); //frame count(?)
                      for(UINT i=0; i<4; i++) //encoding name (byte 1 = string length, 31 bytes = string (whats the point of having a size here?)
                        output.OutputQword(0);
                      output.OutputWord(WORD_BE(24)); //bit depth
                      output.OutputWord(0xFFFF); //quicktime video color table id (none = -1)
                      PushBox(output, DWORD_BE('avcC'));
                        output.OutputByte(1); //version
                        output.OutputByte(100); //h264 profile ID
                        output.OutputByte(0); //h264 compatible profiles
                        output.OutputByte(0x1f); //h264 level
                        output.OutputByte(0xff); //reserved
                        output.OutputByte(0xe1); //first half-byte = no clue. second half = sps count
                        output.OutputWord(fastHtons(SPS.Num())); //sps size
                        output.Serialize(SPS.Array(), SPS.Num()); //sps data
                        output.OutputByte(1); //pps count
                        output.OutputWord(fastHtons(PPS.Num())); //pps size
                        output.Serialize(PPS.Array(), PPS.Num()); //pps data
                      PopBox(); //avcC
                    PopBox(); //avc1
                  PopBox(); //stsd
                  PushBox(output, DWORD_BE('stts')); //frame times
                    output.OutputDword(0); //version and flags (none)
                    output.OutputDword(fastHtonl(decodeTimes.Num()));
                    for(UINT i=0; i<decodeTimes.Num(); i++)
                    {
                        output.OutputDword(fastHtonl(decodeTimes[i].count));
                        output.OutputDword(fastHtonl(decodeTimes[i].val));
                    }
                  PopBox(); //stts

                  SendMessage(GetDlgItem(hwndProgressDialog, IDC_PROGRESS1), PBM_SETPOS, 30, 0);
                  ProcessEvents();

                  PushBox(output, DWORD_BE('stss')); //list of keyframe (i-frame) IDs
                    output.OutputDword(0); //version and flags (none)
                    output.OutputDword(fastHtonl(IFrameIDs.Num()));
                    output.Serialize(IFrameIDs.Array(), IFrameIDs.Num()*sizeof(UINT));
                  PopBox(); //stss
                  PushBox(output, DWORD_BE('ctts')); //list of composition time offsets
                    output.OutputDword(DWORD_BE(0x01000000)); //version (1) and flags (none)
                    output.OutputDword(fastHtonl(compositionOffsets.Num()));
                    for(UINT i=0; i<compositionOffsets.Num(); i++)
                    {
                        output.OutputDword(fastHtonl(compositionOffsets[i].count));
                        output.OutputDword(fastHtonl(compositionOffsets[i].val));
                    }
                  PopBox(); //ctts

                  SendMessage(GetDlgItem(hwndProgressDialog, IDC_PROGRESS1), PBM_SETPOS, 40, 0);
                  ProcessEvents();

                  PushBox(output, DWORD_BE('stsc')); //sample to chunk list
                    output.OutputDword(0); //version and flags (none)
                    output.OutputDword(fastHtonl(videoSampleToChunk.Num()));
                    for(UINT i=0; i<videoSampleToChunk.Num(); i++)
                    {
                        SampleToChunk &stc  = videoSampleToChunk[i];
                        output.OutputDword(fastHtonl(stc.firstChunkID));
                        output.OutputDword(fastHtonl(stc.samplesPerChunk));
                        output.OutputDword(DWORD_BE(1));
                    }
                  PopBox(); //stsc
                  PushBox(output, DWORD_BE('stsz')); //sample sizes
                    output.OutputDword(0); //version and flags (none)
                    output.OutputDword(0); //block size for all (0 if differing sizes)
                    output.OutputDword(fastHtonl(videoFrames.Num()));
                    for(UINT i=0; i<videoFrames.Num(); i++)
                        output.OutputDword(fastHtonl(videoFrames[i].size));
                  PopBox();

                  if(videoChunks.Num() && videoChunks.Last() > 0xFFFFFFFFLL)
                  {
                      PushBox(output, DWORD_BE('co64')); //chunk offsets
                      output.OutputDword(0); //version and flags (none)
                      output.OutputDword(fastHtonl(videoChunks.Num()));
                      for(UINT i=0; i<videoChunks.Num(); i++)
                          output.OutputQword(fastHtonll(videoChunks[i]));
                      PopBox(); //co64
                  }
                  else
                  {
                      PushBox(output, DWORD_BE('stco')); //chunk offsets
                        output.OutputDword(0); //version and flags (none)
                        output.OutputDword(fastHtonl(videoChunks.Num()));
                        for(UINT i=0; i<videoChunks.Num(); i++)
                            output.OutputDword(fastHtonl((DWORD)videoChunks[i]));
                      PopBox(); //stco
                  }
                PopBox(); //stbl
              PopBox(); //minf
            PopBox(); //mdia
          PopBox(); //trak

          SendMessage(GetDlgItem(hwndProgressDialog, IDC_PROGRESS1), PBM_SETPOS, 50, 0);
          ProcessEvents();

          //------------------------------------------------------
          // audio track
          PushBox(output, DWORD_BE('trak'));
            PushBox(output, DWORD_BE('tkhd')); //track header
              output.OutputDword(DWORD_BE(0x0000000F)); //version (0) and flags (0xF)
              output.OutputDword(macTime); //creation time
              output.OutputDword(macTime); //modified time
              output.OutputDword(DWORD_BE(2)); //track ID
              output.OutputDword(0); //reserved
              output.OutputDword(audioDuration); //duration (in time base units)
              output.OutputQword(0); //reserved
              output.OutputWord(0); //video layer (0)
              output.OutputWord(WORD_BE(1)); //quicktime alternate track id
              output.OutputWord(WORD_BE(0x0100)); //volume
              output.OutputWord(0); //reserved
              output.OutputDword(DWORD_BE(0x00010000)); output.OutputDword(DWORD_BE(0x00000000)); output.OutputDword(DWORD_BE(0x00000000)); //window matrix row 1 (1.0, 0.0, 0.0)
              output.OutputDword(DWORD_BE(0x00000000)); output.OutputDword(DWORD_BE(0x00010000)); output.OutputDword(DWORD_BE(0x00000000)); //window matrix row 2 (0.0, 1.0, 0.0)
              output.OutputDword(DWORD_BE(0x00000000)); output.OutputDword(DWORD_BE(0x00000000)); output.OutputDword(DWORD_BE(0x40000000)); //window matrix row 3 (0.0, 0.0, 16384.0)
              output.OutputDword(0); //width (fixed point)
              output.OutputDword(0); //height (fixed point)
            PopBox(); //tkhd
            PushBox(output, DWORD_BE('edts'));
              PushBox(output, DWORD_BE('elst'));
                output.OutputDword(0); //version and flags (none)
                output.OutputDword(DWORD_BE(1)); //count
                output.OutputDword(audioDuration); //duration
                output.OutputDword(0); //start time
                output.OutputDword(DWORD_BE(0x00010000)); //playback speed (1.0)
              PopBox(); //elst
            PopBox(); //tdst
            PushBox(output, DWORD_BE('mdia'));
              PushBox(output, DWORD_BE('mdhd'));
                output.OutputDword(0); //version and flags (none)
                output.OutputDword(macTime); //creation time
                output.OutputDword(macTime); //modified time
                output.OutputDword(DWORD_BE(44100)); //time scale
                output.OutputDword(fastHtonl(audioFrames.Num()*App->GetAudioEncoder()->GetFrameSize()));
                output.OutputDword(DWORD_BE(0x55c40000));
              PopBox(); //mdhd
              PushBox(output, DWORD_BE('hdlr'));
                output.OutputDword(0); //version and flags (none)
                output.OutputDword(0); //quicktime type (none)
                output.OutputDword(DWORD_BE('soun')); //media type
                output.OutputDword(0); //manufacturer reserved
                output.OutputDword(0); //quicktime component reserved flags
                output.OutputDword(0); //quicktime component reserved mask
                output.Serialize((LPVOID)lpAudioTrack, (DWORD)strlen(lpAudioTrack)+1); //track name
              PopBox(); //hdlr
              PushBox(output, DWORD_BE('minf'));
                PushBox(output, DWORD_BE('smhd'));
                  output.OutputDword(0); //version and flags (none)
                  output.OutputDword(0); //balance (fixed point)
                PopBox(); //vdhd
                PushBox(output, DWORD_BE('dinf'));
                  PushBox(output, DWORD_BE('dref'));
                    output.OutputDword(0); //version and flags (none)
                    output.OutputDword(DWORD_BE(1)); //count
                    PushBox(output, DWORD_BE('url '));
                      output.OutputDword(DWORD_BE(0x00000001)); //version (0) and flags (1)
                    PopBox(); //url
                  PopBox(); //dref
                PopBox(); //dinf
                PushBox(output, DWORD_BE('stbl'));
                  PushBox(output, DWORD_BE('stsd'));
                    output.OutputDword(0); //version and flags (none)
                    output.OutputDword(DWORD_BE(1)); //count
                    PushBox(output, DWORD_BE('mp4a'));
                      output.OutputDword(0); //reserved (6 bytes)
                      output.OutputWord(0);
                      output.OutputWord(WORD_BE(1)); //dref index
                      output.OutputWord(0); //quicktime encoding version
                      output.OutputWord(0); //quicktime encoding revision
                      output.OutputDword(0); //quicktime audio encoding vendor
                      output.OutputWord(WORD_BE(2)); //channels
                      output.OutputWord(WORD_BE(16)); //sample size
                      output.OutputWord(0); //quicktime audio compression id
                      output.OutputWord(0); //quicktime audio packet size
                      output.OutputDword(DWORD_BE(44100<<16)); //sample rate (fixed point)
                      PushBox(output, DWORD_BE('esds'));
                        output.OutputDword(0); //version and flags (none)
                        output.OutputByte(3); //ES descriptor type
                        output.OutputByte(0x80);
                        output.OutputByte(0x80);
                        output.OutputByte(0x80);
                        output.OutputByte(esDescriptor.Num());
                        output.Serialize((LPVOID)esDescriptor.Array(), esDescriptor.Num());
                      PopBox();
                    PopBox();
                  PopBox(); //stsd
                  PushBox(output, DWORD_BE('stts')); //list of keyframe (i-frame) IDs
                    output.OutputDword(0); //version and flags (none)
                    output.OutputDword(DWORD_BE(1));
                    output.OutputDword(fastHtonl(audioFrames.Num()));
                    output.OutputDword(fastHtonl(App->GetAudioEncoder()->GetFrameSize()));
                  PopBox(); //stss
                  PushBox(output, DWORD_BE('stsc')); //sample to chunk list
                    output.OutputDword(0); //version and flags (none)
                    output.OutputDword(fastHtonl(audioSampleToChunk.Num()));
                    for(UINT i=0; i<audioSampleToChunk.Num(); i++)
                    {
                        SampleToChunk &stc  = audioSampleToChunk[i];
                        output.OutputDword(fastHtonl(stc.firstChunkID));
                        output.OutputDword(fastHtonl(stc.samplesPerChunk));
                        output.OutputDword(DWORD_BE(1));
                    }
                  PopBox(); //stsc

                  SendMessage(GetDlgItem(hwndProgressDialog, IDC_PROGRESS1), PBM_SETPOS, 60, 0);
                  ProcessEvents();

                  PushBox(output, DWORD_BE('stsz')); //sample sizes
                    output.OutputDword(0); //version and flags (none)
                    output.OutputDword(0); //block size for all (0 if differing sizes)
                    output.OutputDword(fastHtonl(audioFrames.Num()));
                    for(UINT i=0; i<audioFrames.Num(); i++)
                        output.OutputDword(fastHtonl(audioFrames[i].size));
                  PopBox();

                  SendMessage(GetDlgItem(hwndProgressDialog, IDC_PROGRESS1), PBM_SETPOS, 70, 0);
                  ProcessEvents();

                  if(audioChunks.Num() && audioChunks.Last() > 0xFFFFFFFFLL)
                  {
                      PushBox(output, DWORD_BE('co64')); //chunk offsets
                      output.OutputDword(0); //version and flags (none)
                      output.OutputDword(fastHtonl(audioChunks.Num()));
                      for(UINT i=0; i<audioChunks.Num(); i++)
                          output.OutputQword(fastHtonll(audioChunks[i]));
                      PopBox(); //co64
                  }
                  else
                  {
                      PushBox(output, DWORD_BE('stco')); //chunk offsets
                        output.OutputDword(0); //version and flags (none)
                        output.OutputDword(fastHtonl(audioChunks.Num()));
                        for(UINT i=0; i<audioChunks.Num(); i++)
                            output.OutputDword(fastHtonl((DWORD)audioChunks[i]));
                      PopBox(); //stco
                  }
                PopBox(); //stbl
              PopBox(); //minf
            PopBox(); //mdia
          PopBox(); //trak

          SendMessage(GetDlgItem(hwndProgressDialog, IDC_PROGRESS1), PBM_SETPOS, 80, 0);
          ProcessEvents();

          //------------------------------------------------------
          // info thingy
          PushBox(output, DWORD_BE('udta'));
            PushBox(output, DWORD_BE('meta'));
              output.OutputDword(0); //version and flags (none)
              PushBox(output, DWORD_BE('hdlr'));
                output.OutputDword(0); //version and flags (none)
                output.OutputDword(0); //quicktime type
                output.OutputDword(DWORD_BE('mdir')); //metadata type
                output.OutputDword(DWORD_BE('appl')); //quicktime manufacturer reserved thingy
                output.OutputDword(0); //quicktime component reserved flag
                output.OutputDword(0); //quicktime component reserved flag mask
                output.OutputByte(0); //null string
              PopBox(); //hdlr
              PushBox(output, DWORD_BE('ilst'));
                PushBox(output, DWORD_BE('\xa9too'));
                  PushBox(output, DWORD_BE('data'));
                    output.OutputDword(DWORD_BE(1)); //version (1) + flags (0)
                    output.OutputDword(0); //reserved
                    LPSTR lpVersion = OBS_VERSION_STRING_ANSI;
                    output.Serialize(lpVersion, (DWORD)strlen(lpVersion));
                  PopBox(); //data
                PopBox(); //@too
              PopBox(); //ilst
            PopBox(); //meta
          PopBox(); //udta

        PopBox(); //moov

        fileOut.Serialize(endBuffer.Array(), endBuffer.Num());
        fileOut.Close();

        XFile file;
        if(file.Open(strFile, XFILE_WRITE, XFILE_OPENEXISTING))
        {
#ifdef USE_64BIT_MP4
            file.SetPos((INT64)mdatStart+8, XFILE_BEGIN);

            UINT64 size = fastHtonll(mdatStop-mdatStart);
            file.Write(&size, 8);
#else
            file.SetPos((INT64)mdatStart, XFILE_BEGIN);
            UINT size = fastHtonl((DWORD)(mdatStop-mdatStart));
            file.Write(&size, 4);
#endif
            file.Close();
        }

        DestroyWindow(hwndProgressDialog);
    }

    virtual void AddPacket(BYTE *data, UINT size, DWORD timestamp, PacketType type)
    {
        if(!bSentFirstVideoPacket && type != PacketType_Audio)
        {
            bSentFirstVideoPacket = true;

            DataPacket seiData;
            App->GetVideoEncoder()->GetSEI(seiData);

            AddPacket(seiData.lpPacket, seiData.size, timestamp, PacketType_VideoHighest);
        }

        UINT64 offset = fileOut.GetPos();

        if(type == PacketType_Audio)
        {
            UINT copySize;

            if(bMP3)
            {
                copySize = size-1;
                fileOut.Serialize(data+1, copySize);
            }
            else
            {
                copySize = size-2;
                fileOut.Serialize(data+2, copySize);
            }

            MP4AudioFrameInfo audioFrame;
            audioFrame.fileOffset   = offset;
            audioFrame.size         = copySize;
            audioFrames << audioFrame;
        }
        else
        {
            UINT totalCopied = 0;

            if(data[0] == 0x17 && data[1] == 0) //if SPS/PPS
            {
                LPBYTE lpData = data+11;

                UINT spsSize = fastHtons(*(WORD*)lpData);
                fileOut.OutputWord(0);
                fileOut.Serialize(lpData, spsSize+2);

                lpData += spsSize+3;

                UINT ppsSize = fastHtons(*(WORD*)lpData);
                fileOut.OutputWord(0);
                fileOut.Serialize(lpData, ppsSize+2);

                totalCopied = spsSize+ppsSize+8;
            }
            else
            {
                totalCopied = size-5;
                fileOut.Serialize(data+5, totalCopied);
            }

            if(!videoFrames.Num() || timestamp != lastVideoTimestamp)
            {
                INT timeOffset = 0;
                mcpy(((BYTE*)&timeOffset)+1, data+2, 3);
                if(data[2] >= 0x80)
                    timeOffset |= 0xFF;
                timeOffset = (INT)fastHtonl(DWORD(timeOffset));

                if(data[0] == 0x17) //i-frame
                    IFrameIDs << fastHtonl(videoFrames.Num()+1);

                MP4VideoFrameInfo frameInfo;
                frameInfo.fileOffset        = offset;
                frameInfo.size              = totalCopied;
                frameInfo.timestamp         = timestamp;
                frameInfo.compositionOffset = timeOffset;
                videoFrames << frameInfo;
            }
            else
                videoFrames.Last().size += totalCopied;

            lastVideoTimestamp = timestamp;
        }
    }
};


VideoFileStream* CreateMP4FileStream(CTSTR lpFile)
{
    MP4FileStream *fileStream = new MP4FileStream;
    if(fileStream->Init(lpFile))
        return fileStream;

    delete fileStream;
    return NULL;
}
