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

#include "DataPacketHelpers.h"


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
    UINT    timestamp;
};

#define USE_64BIT_MP4 1

inline UINT64 ConvertToAudioTime(DWORD timestamp, UINT64 minVal)
{
    UINT val = UINT64(timestamp)*App->GetSampleRateHz()/1000;
    return MAX(val, minVal);
}


//code annoyance rating: nightmarish

class MP4FileStream : public VideoFileStream
{
    XFileOutputSerializer fileOut;
    String strFile;

    List<MP4VideoFrameInfo> videoFrames;
    List<MP4AudioFrameInfo> audioFrames;

    List<UINT>      IFrameIDs;

    DWORD           lastVideoTimestamp, initialTimeStamp;

    bool            bStreamOpened;
    bool            bMP3;

    List<BYTE>      endBuffer;
    List<UINT>      boxOffsets;

    //chunk stuiff
    UINT64 connectedAudioSampleOffset, connectedVideoSampleOffset;
    UINT64 curVideoChunkOffset, curAudioChunkOffset;
    UINT numVideoSamples, numAudioSamples;
    List<UINT64> videoChunks, audioChunks;
    List<SampleToChunk> videoSampleToChunk, audioSampleToChunk;

    //decode times and composition offsets
    UINT64 lastAudioTimeVal;
    UINT64 audioFrameSize;
    List<OffsetVal> videoDecodeTimes, audioDecodeTimes;
    List<OffsetVal> compositionOffsets;

    UINT64 mdatStart, mdatStop;

    bool bCancelMP4Build;

    bool bSentSEI;

    void PushBox(BufferOutputSerializer &output, DWORD boxName)
    {
        boxOffsets.Insert(0, (UINT)output.GetPos());

        output.OutputDword(0);
        output.OutputDword(boxName);
    }

    void PopBox(BufferOutputSerializer &output)
    {
        DWORD boxSize = (DWORD)output.GetPos()-boxOffsets[0];
        *(DWORD*)(endBuffer.Array()+boxOffsets[0]) = fastHtonl(boxSize);

        boxOffsets.Remove(0);
    }

    static INT_PTR CALLBACK MP4ProgressDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch(message)
        {
            case WM_INITDIALOG:
                LocalizeWindow(hwnd);
                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)lParam);
                return TRUE;

            case WM_COMMAND:
                switch(LOWORD(wParam))
                {
                    case IDCANCEL:
                        if(OBSMessageBox(hwnd, Str("MP4ProgressDialog.ConfirmStop"), Str("MP4ProgressDialog.ConfirmStopTitle"), MB_YESNO) == IDYES)
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

        initialTimeStamp = -1;

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

        audioFrameSize = App->GetAudioEncoder()->GetFrameSize();

        CopyMetadata();

        bStreamOpened = true;

        return true;
    }

    template<typename T> void GetChunkInfo(const T &data, UINT index,
                                                  List<UINT64> &chunks, List<SampleToChunk> &sampleToChunks,
                                                  UINT64 &curChunkOffset, UINT64 &connectedSampleOffset, UINT &numSamples)
    {
        UINT64 curOffset = data.fileOffset;
        if(index == 0)
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
        connectedSampleOffset = curOffset+data.size;
    }

    inline void EndChunkInfo(List<UINT64> &chunks, List<SampleToChunk> &sampleToChunks, UINT64 &curChunkOffset, UINT &numSamples)
    {
        chunks << curChunkOffset;
        if(!sampleToChunks.Num() || sampleToChunks.Last().samplesPerChunk != numSamples)
        {
            SampleToChunk stc;
            stc.firstChunkID = chunks.Num();
            stc.samplesPerChunk = numSamples;
            sampleToChunks << stc;
        }
    }

    void GetVideoDecodeTime(MP4VideoFrameInfo &videoFrame, bool bLast)
    {
        UINT frameTime;

        if(bLast)
            frameTime = videoDecodeTimes.Last().val;
        else
            frameTime = videoFrame.timestamp-videoFrames.Last().timestamp;

        if(!videoDecodeTimes.Num() || videoDecodeTimes.Last().val != (UINT)frameTime)
        {
            OffsetVal newVal;
            newVal.count = 1;
            newVal.val = (UINT)frameTime;
            videoDecodeTimes << newVal;
        }
        else
            videoDecodeTimes.Last().count++;

        INT compositionOffset = videoFrames.Last().compositionOffset;
        if(!compositionOffsets.Num() || compositionOffsets.Last().val != (UINT)compositionOffset)
        {
            OffsetVal newVal;
            newVal.count = 1;
            newVal.val = (UINT)compositionOffset;
            compositionOffsets << newVal;
        }
        else
            compositionOffsets.Last().count++;
    }

    void GetAudioDecodeTime(MP4AudioFrameInfo &audioFrame, bool bLast)
    {
        UINT frameTime;
        if(bLast)
            frameTime = audioDecodeTimes.Last().val;
        else
        {
            UINT64 newTimeVal = lastAudioTimeVal+audioFrameSize;
            if(audioFrames.Num() > 1)
            {
                UINT64 convertedTime = ConvertToAudioTime(audioFrame.timestamp, audioFrameSize*audioFrames.Num());
                if(convertedTime > newTimeVal)
                    newTimeVal = convertedTime;
            }

            frameTime = UINT(newTimeVal - lastAudioTimeVal);
            lastAudioTimeVal = newTimeVal;
        }

        if(!audioDecodeTimes.Num() || audioDecodeTimes.Last().val != (UINT)frameTime)
        {
            OffsetVal newVal;
            newVal.count = 1;
            newVal.val = (UINT)frameTime;
            audioDecodeTimes << newVal;
        }
        else
            audioDecodeTimes.Last().count++;
    }

    UINT frameTime = 0;
    UINT sampleRateHz = 0;
    UINT width = 0, height = 0;
    UINT maxBitRate = 0;
    void CopyMetadata()
    {
        frameTime = App->GetFrameTime();
        sampleRateHz = App->GetSampleRateHz();
        App->GetOutputSize(width, height);

        //-------------------------------------------
        // get AAC headers if using AAC
        maxBitRate = fastHtonl(App->GetAudioEncoder()->GetBitRate() * 1000);

        InitBufferedPackets();
    }

    decltype(GetBufferedSEIPacket()) sei = GetBufferedSEIPacket();
    decltype(GetBufferedAudioHeadersPacket()) audioHeaders = GetBufferedAudioHeadersPacket();
    decltype(GetBufferedVideoHeadersPacket()) videoHeaders = GetBufferedVideoHeadersPacket();

    void InitBufferedPackets()
    {
        sei.InitBuffer();
        if (!bMP3)
            audioHeaders.InitBuffer();
        videoHeaders.InitBuffer();
    }

    ~MP4FileStream()
    {
        if(!bStreamOpened)
            return;

        App->EnableSceneSwitching(false);

        //---------------------------------------------------

        //HWND hwndProgressDialog = CreateDialog(hinstMain, MAKEINTRESOURCE(IDD_BUILDINGMP4), hwndMain, (DLGPROC)MP4ProgressDialogProc);
        //SendMessage(GetDlgItem(hwndProgressDialog, IDC_PROGRESS1), PBM_SETRANGE32, 0, 100);

        mdatStop = fileOut.GetPos();

        BufferOutputSerializer output(endBuffer);

        //set a reasonable initial buffer size
        endBuffer.SetSize((videoFrames.Num() + audioFrames.Num()) * 20 + 131072);

        DWORD macTime = fastHtonl(DWORD(GetMacTime()));
        UINT videoDuration = fastHtonl(lastVideoTimestamp + frameTime);
        UINT audioDuration = fastHtonl(lastVideoTimestamp + DWORD(double(audioFrameSize)*1000.0/sampleRateHz));

        LPCSTR lpVideoTrack = "Video Media Handler";
        LPCSTR lpAudioTrack = "Sound Media Handler";

        const char videoCompressionName[31] = "AVC Coding";

        //-------------------------------------------
        // get video headers
        List<BYTE> SPS, PPS;

        LPBYTE lpHeaderData = videoHeaders.lpPacket+11;
        SPS.CopyArray(lpHeaderData+2, fastHtons(*(WORD*)lpHeaderData));

        lpHeaderData += SPS.Num()+3;
        PPS.CopyArray(lpHeaderData+2, fastHtons(*(WORD*)lpHeaderData));

        //-------------------------------------------

        EndChunkInfo(videoChunks, videoSampleToChunk, curVideoChunkOffset, numVideoSamples);
        EndChunkInfo(audioChunks, audioSampleToChunk, curAudioChunkOffset, numAudioSamples);

        if (numVideoSamples > 1)
            GetVideoDecodeTime(videoFrames.Last(), true);

        if (numAudioSamples > 1)
            GetAudioDecodeTime(audioFrames.Last(), true);

        UINT audioUnitDuration = fastHtonl(UINT(lastAudioTimeVal));

        //SendMessage(GetDlgItem(hwndProgressDialog, IDC_PROGRESS1), PBM_SETPOS, 25, 0);

        //-------------------------------------------
        // sound descriptor thingy.  this part made me die a little inside admittedly.

        List<BYTE> esDecoderDescriptor;
        BufferOutputSerializer esDecoderOut(esDecoderDescriptor);
        esDecoderOut.OutputByte(bMP3 ? 107 : 64);
        esDecoderOut.OutputByte(0x15); //stream/type flags.  always 0x15 for my purposes.
        esDecoderOut.OutputByte(0); //buffer size, just set it to 1536 for both mp3 and aac
        esDecoderOut.OutputWord(WORD_BE(0x600)); 
        esDecoderOut.OutputDword(maxBitRate); //max bit rate (cue bill 'o reily meme for these two)
        esDecoderOut.OutputDword(maxBitRate); //avg bit rate

        if(!bMP3) //if AAC, put in headers
        {
            esDecoderOut.OutputByte(0x5);  //decoder specific descriptor type
            /*esDecoderOut.OutputByte(0x80); //some stuff that no one should probably care about
            esDecoderOut.OutputByte(0x80);
            esDecoderOut.OutputByte(0x80);*/
            assert(audioHeaders.size >= 2);
            esDecoderOut.OutputByte(audioHeaders.size - 2);
            esDecoderOut.Serialize(audioHeaders.lpPacket + 2, audioHeaders.size - 2);
        }


        List<BYTE> esDescriptor;
        BufferOutputSerializer esOut(esDescriptor);
        esOut.OutputWord(0); //es id
        esOut.OutputByte(0); //stream priority
        esOut.OutputByte(4); //descriptor type
        /*esOut.OutputByte(0x80); //some stuff that no one should probably care about
        esOut.OutputByte(0x80);
        esOut.OutputByte(0x80);*/
        esOut.OutputByte(esDecoderDescriptor.Num());
        esOut.Serialize((LPVOID)esDecoderDescriptor.Array(), esDecoderDescriptor.Num());
        esOut.OutputByte(0x6);  //config descriptor type
        /*esOut.OutputByte(0x80); //some stuff that no one should probably care about
        esOut.OutputByte(0x80);
        esOut.OutputByte(0x80);*/
        esOut.OutputByte(1); //len
        esOut.OutputByte(2); //SL value(? always 2)

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
          PopBox(output); //mvhd

          //------------------------------------------------------
          // audio track
          PushBox(output, DWORD_BE('trak'));
            PushBox(output, DWORD_BE('tkhd')); //track header
              output.OutputDword(DWORD_BE(0x00000007)); //version (0) and flags (0xF)
              output.OutputDword(macTime); //creation time
              output.OutputDword(macTime); //modified time
              output.OutputDword(DWORD_BE(1)); //track ID
              output.OutputDword(0); //reserved
              output.OutputDword(audioDuration); //duration (in time base units)
              output.OutputQword(0); //reserved
              output.OutputWord(0); //video layer (0)
              output.OutputWord(WORD_BE(0)); //quicktime alternate track id
              output.OutputWord(WORD_BE(0x0100)); //volume
              output.OutputWord(0); //reserved
              output.OutputDword(DWORD_BE(0x00010000)); output.OutputDword(DWORD_BE(0x00000000)); output.OutputDword(DWORD_BE(0x00000000)); //window matrix row 1 (1.0, 0.0, 0.0)
              output.OutputDword(DWORD_BE(0x00000000)); output.OutputDword(DWORD_BE(0x00010000)); output.OutputDword(DWORD_BE(0x00000000)); //window matrix row 2 (0.0, 1.0, 0.0)
              output.OutputDword(DWORD_BE(0x00000000)); output.OutputDword(DWORD_BE(0x00000000)); output.OutputDword(DWORD_BE(0x40000000)); //window matrix row 3 (0.0, 0.0, 16384.0)
              output.OutputDword(0); //width (fixed point)
              output.OutputDword(0); //height (fixed point)
            PopBox(output); //tkhd
            /*PushBox(output, DWORD_BE('edts'));
              PushBox(output, DWORD_BE('elst'));
                output.OutputDword(0); //version and flags (none)
                output.OutputDword(DWORD_BE(1)); //count
                output.OutputDword(audioDuration); //duration
                output.OutputDword(0); //start time
                output.OutputDword(DWORD_BE(0x00010000)); //playback speed (1.0)
              PopBox(); //elst
            PopBox(); //tdst*/
            PushBox(output, DWORD_BE('mdia'));
              PushBox(output, DWORD_BE('mdhd'));
                output.OutputDword(0); //version and flags (none)
                output.OutputDword(macTime); //creation time
                output.OutputDword(macTime); //modified time
                output.OutputDword(DWORD_BE(sampleRateHz)); //time scale
                output.OutputDword(audioUnitDuration);
                output.OutputDword(bMP3 ? DWORD_BE(0x55c40000) : DWORD_BE(0x15c70000));
              PopBox(output); //mdhd
              PushBox(output, DWORD_BE('hdlr'));
                output.OutputDword(0); //version and flags (none)
                output.OutputDword(0); //quicktime type (none)
                output.OutputDword(DWORD_BE('soun')); //media type
                output.OutputDword(0); //manufacturer reserved
                output.OutputDword(0); //quicktime component reserved flags
                output.OutputDword(0); //quicktime component reserved mask
                output.Serialize((LPVOID)lpAudioTrack, (DWORD)strlen(lpAudioTrack)+1); //track name
              PopBox(output); //hdlr
              PushBox(output, DWORD_BE('minf'));
                PushBox(output, DWORD_BE('smhd'));
                  output.OutputDword(0); //version and flags (none)
                  output.OutputDword(0); //balance (fixed point)
                PopBox(output); //vdhd
                PushBox(output, DWORD_BE('dinf'));
                  PushBox(output, DWORD_BE('dref'));
                    output.OutputDword(0); //version and flags (none)
                    output.OutputDword(DWORD_BE(1)); //count
                    PushBox(output, DWORD_BE('url '));
                      output.OutputDword(DWORD_BE(0x00000001)); //version (0) and flags (1)
                    PopBox(output); //url
                  PopBox(output); //dref
                PopBox(output); //dinf
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
                      output.OutputWord(0); //channels (ignored)
                      output.OutputWord(WORD_BE(16)); //sample size
                      output.OutputWord(0); //quicktime audio compression id
                      output.OutputWord(0); //quicktime audio packet size
                      output.OutputDword(DWORD_BE((sampleRateHz<<16))); //sample rate (fixed point)
                      PushBox(output, DWORD_BE('esds'));
                        output.OutputDword(0); //version and flags (none)
                        output.OutputByte(3); //ES descriptor type
                        /*output.OutputByte(0x80);
                        output.OutputByte(0x80);
                        output.OutputByte(0x80);*/
                        output.OutputByte(esDescriptor.Num());
                        output.Serialize((LPVOID)esDescriptor.Array(), esDescriptor.Num());
                      PopBox(output);
                    PopBox(output);
                  PopBox(output); //stsd
                  PushBox(output, DWORD_BE('stts')); //list of keyframe (i-frame) IDs
                    output.OutputDword(0); //version and flags (none)
                    output.OutputDword(fastHtonl(audioDecodeTimes.Num()));
                    for(UINT i=0; i<audioDecodeTimes.Num(); i++)
                    {
                        output.OutputDword(fastHtonl(audioDecodeTimes[i].count));
                        output.OutputDword(fastHtonl(audioDecodeTimes[i].val));
                    }
                  PopBox(output); //stss
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
                  PopBox(output); //stsc

                  //SendMessage(GetDlgItem(hwndProgressDialog, IDC_PROGRESS1), PBM_SETPOS, 30, 0);
                  //ProcessEvents();

                  PushBox(output, DWORD_BE('stsz')); //sample sizes
                    output.OutputDword(0); //version and flags (none)
                    output.OutputDword(0); //block size for all (0 if differing sizes)
                    output.OutputDword(fastHtonl(audioFrames.Num()));
                    for(UINT i=0; i<audioFrames.Num(); i++)
                        output.OutputDword(fastHtonl(audioFrames[i].size));
                  PopBox(output);

                  //SendMessage(GetDlgItem(hwndProgressDialog, IDC_PROGRESS1), PBM_SETPOS, 40, 0);
                  //ProcessEvents();

                  if(audioChunks.Num() && audioChunks.Last() > 0xFFFFFFFFLL)
                  {
                      PushBox(output, DWORD_BE('co64')); //chunk offsets
                      output.OutputDword(0); //version and flags (none)
                      output.OutputDword(fastHtonl(audioChunks.Num()));
                      for(UINT i=0; i<audioChunks.Num(); i++)
                          output.OutputQword(fastHtonll(audioChunks[i]));
                      PopBox(output); //co64
                  }
                  else
                  {
                      PushBox(output, DWORD_BE('stco')); //chunk offsets
                        output.OutputDword(0); //version and flags (none)
                        output.OutputDword(fastHtonl(audioChunks.Num()));
                        for(UINT i=0; i<audioChunks.Num(); i++)
                            output.OutputDword(fastHtonl((DWORD)audioChunks[i]));
                      PopBox(output); //stco
                  }
                PopBox(output); //stbl
              PopBox(output); //minf
            PopBox(output); //mdia
          PopBox(output); //trak

          //SendMessage(GetDlgItem(hwndProgressDialog, IDC_PROGRESS1), PBM_SETPOS, 50, 0);
          //ProcessEvents();

          //------------------------------------------------------
          // video track
          PushBox(output, DWORD_BE('trak'));
            PushBox(output, DWORD_BE('tkhd')); //track header
              output.OutputDword(DWORD_BE(0x00000007)); //version (0) and flags (0x7)
              output.OutputDword(macTime); //creation time
              output.OutputDword(macTime); //modified time
              output.OutputDword(DWORD_BE(2)); //track ID
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
            PopBox(output); //tkhd
            /*PushBox(output, DWORD_BE('edts'));
              PushBox(output, DWORD_BE('elst'));
                output.OutputDword(0); //version and flags (none)
                output.OutputDword(DWORD_BE(1)); //count
                output.OutputDword(videoDuration); //duration
                output.OutputDword(0); //start time
                output.OutputDword(DWORD_BE(0x00010000)); //playback speed (1.0)
              PopBox(); //elst
            PopBox(); //tdst*/
            PushBox(output, DWORD_BE('mdia'));
              PushBox(output, DWORD_BE('mdhd'));
                output.OutputDword(0); //version and flags (none)
                output.OutputDword(macTime); //creation time
                output.OutputDword(macTime); //modified time
                output.OutputDword(DWORD_BE(1000)); //time scale
                output.OutputDword(videoDuration);
                output.OutputDword(DWORD_BE(0x55c40000));
              PopBox(output); //mdhd
              PushBox(output, DWORD_BE('hdlr'));
                output.OutputDword(0); //version and flags (none)
                output.OutputDword(0); //quicktime type (none)
                output.OutputDword(DWORD_BE('vide')); //media type
                output.OutputDword(0); //manufacturer reserved
                output.OutputDword(0); //quicktime component reserved flags
                output.OutputDword(0); //quicktime component reserved mask
                output.Serialize((LPVOID)lpVideoTrack, (DWORD)strlen(lpVideoTrack)+1); //track name
              PopBox(output); //hdlr
              PushBox(output, DWORD_BE('minf'));
                PushBox(output, DWORD_BE('vmhd'));
                  output.OutputDword(DWORD_BE(0x00000001)); //version (0) and flags (1)
                  output.OutputWord(0); //quickdraw graphic mode (copy = 0)
                  output.OutputWord(0); //quickdraw red value
                  output.OutputWord(0); //quickdraw green value
                  output.OutputWord(0); //quickdraw blue value
                PopBox(output); //vdhd
                PushBox(output, DWORD_BE('dinf'));
                  PushBox(output, DWORD_BE('dref'));
                    output.OutputDword(0); //version and flags (none)
                    output.OutputDword(DWORD_BE(1)); //count
                    PushBox(output, DWORD_BE('url '));
                      output.OutputDword(DWORD_BE(0x00000001)); //version (0) and flags (1)
                    PopBox(output); //url
                  PopBox(output); //dref
                PopBox(output); //dinf
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
                      output.OutputByte((BYTE)strlen(videoCompressionName)); //compression name length
                      output.Serialize(videoCompressionName, 31); //31 bytes for the name
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
                      PopBox(output); //avcC
                    PopBox(output); //avc1
                  PopBox(output); //stsd
                  PushBox(output, DWORD_BE('stts')); //frame times
                    output.OutputDword(0); //version and flags (none)
                    output.OutputDword(fastHtonl(videoDecodeTimes.Num()));
                    for(UINT i=0; i<videoDecodeTimes.Num(); i++)
                    {
                        output.OutputDword(fastHtonl(videoDecodeTimes[i].count));
                        output.OutputDword(fastHtonl(videoDecodeTimes[i].val));
                    }
                  PopBox(output); //stts

                  //SendMessage(GetDlgItem(hwndProgressDialog, IDC_PROGRESS1), PBM_SETPOS, 60, 0);
                  //ProcessEvents();

                  if (IFrameIDs.Num())
                  {
                      PushBox(output, DWORD_BE('stss')); //list of keyframe (i-frame) IDs
                        output.OutputDword(0); //version and flags (none)
                        output.OutputDword(fastHtonl(IFrameIDs.Num()));
                        output.Serialize(IFrameIDs.Array(), IFrameIDs.Num()*sizeof(UINT));
                      PopBox(output); //stss
                  }
                  PushBox(output, DWORD_BE('ctts')); //list of composition time offsets
                    output.OutputDword(0); //version (0) and flags (none)
                    //output.OutputDword(DWORD_BE(0x01000000)); //version (1) and flags (none)

                    output.OutputDword(fastHtonl(compositionOffsets.Num()));
                    for(UINT i=0; i<compositionOffsets.Num(); i++)
                    {
                        output.OutputDword(fastHtonl(compositionOffsets[i].count));
                        output.OutputDword(fastHtonl(compositionOffsets[i].val));
                    }
                  PopBox(output); //ctts

                  //SendMessage(GetDlgItem(hwndProgressDialog, IDC_PROGRESS1), PBM_SETPOS, 70, 0);
                  //ProcessEvents();

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
                  PopBox(output); //stsc
                  PushBox(output, DWORD_BE('stsz')); //sample sizes
                    output.OutputDword(0); //version and flags (none)
                    output.OutputDword(0); //block size for all (0 if differing sizes)
                    output.OutputDword(fastHtonl(videoFrames.Num()));
                    for(UINT i=0; i<videoFrames.Num(); i++)
                        output.OutputDword(fastHtonl(videoFrames[i].size));
                  PopBox(output);

                  if(videoChunks.Num() && videoChunks.Last() > 0xFFFFFFFFLL)
                  {
                      PushBox(output, DWORD_BE('co64')); //chunk offsets
                      output.OutputDword(0); //version and flags (none)
                      output.OutputDword(fastHtonl(videoChunks.Num()));
                      for(UINT i=0; i<videoChunks.Num(); i++)
                          output.OutputQword(fastHtonll(videoChunks[i]));
                      PopBox(output); //co64
                  }
                  else
                  {
                      PushBox(output, DWORD_BE('stco')); //chunk offsets
                        output.OutputDword(0); //version and flags (none)
                        output.OutputDword(fastHtonl(videoChunks.Num()));
                        for(UINT i=0; i<videoChunks.Num(); i++)
                            output.OutputDword(fastHtonl((DWORD)videoChunks[i]));
                      PopBox(output); //stco
                  }
                PopBox(output); //stbl
              PopBox(output); //minf
            PopBox(output); //mdia
          PopBox(output); //trak

          //SendMessage(GetDlgItem(hwndProgressDialog, IDC_PROGRESS1), PBM_SETPOS, 80, 0);
          //ProcessEvents();

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
              PopBox(output); //hdlr
              PushBox(output, DWORD_BE('ilst'));
                PushBox(output, DWORD_BE('\xa9too'));
                  PushBox(output, DWORD_BE('data'));
                    output.OutputDword(DWORD_BE(1)); //version (1) + flags (0)
                    output.OutputDword(0); //reserved
                    LPSTR lpVersion = OBS_VERSION_STRING_ANSI;
                    output.Serialize(lpVersion, (DWORD)strlen(lpVersion));
                  PopBox(output); //data
                PopBox(output); //@too
              PopBox(output); //ilst
            PopBox(output); //meta
          PopBox(output); //udta

        PopBox(output); //moov

        fileOut.Serialize(endBuffer.Array(), (DWORD)output.GetPos());
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

        App->EnableSceneSwitching(true);

        //DestroyWindow(hwndProgressDialog);
    }

    virtual void AddPacket(const BYTE *data, UINT size, DWORD timestamp, DWORD /*pts*/, PacketType type) override
    {
        InitBufferedPackets();

        UINT64 offset = fileOut.GetPos();

        if(initialTimeStamp == -1 && data[0] != 0x17)
            return;
        else if(initialTimeStamp == -1 && data[0] == 0x17) {
            initialTimeStamp = timestamp;
        }

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
            audioFrame.timestamp    = timestamp-initialTimeStamp;

            GetChunkInfo<MP4AudioFrameInfo>(audioFrame, audioFrames.Num(), audioChunks, audioSampleToChunk,
                                            curAudioChunkOffset, connectedAudioSampleOffset, numAudioSamples);

            if(audioFrames.Num())
                GetAudioDecodeTime(audioFrames.Last(), false);

            audioFrames << audioFrame;
        }
        else
        {
            UINT totalCopied = 0;

            if(data[0] == 0x17 && data[1] == 0) //if SPS/PPS
            {
                const BYTE *lpData = data+11;

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
                if (!bSentSEI) {
                    if (sei.size > 0)
                    {
                        fileOut.Serialize(sei.lpPacket, sei.size);
                        totalCopied += sei.size;

                        bSentSEI = true;
                    }
                }

                totalCopied += size-5;
                fileOut.Serialize(data+5, size-5);
            }

            if(!videoFrames.Num() || (timestamp-initialTimeStamp) != lastVideoTimestamp)
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
                frameInfo.timestamp         = timestamp-initialTimeStamp;
                frameInfo.compositionOffset = timeOffset;

                GetChunkInfo<MP4VideoFrameInfo>(frameInfo, videoFrames.Num(), videoChunks, videoSampleToChunk,
                                                curVideoChunkOffset, connectedVideoSampleOffset, numVideoSamples);

                if(videoFrames.Num())
                    GetVideoDecodeTime(frameInfo, false);

                videoFrames << frameInfo;
            }
            else
                videoFrames.Last().size += totalCopied;

            lastVideoTimestamp = timestamp-initialTimeStamp;
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
