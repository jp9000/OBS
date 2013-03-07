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


#include "OBSApi.h"
#include <Audioclient.h>
#include "../libsamplerate/samplerate.h"

#define KSAUDIO_SPEAKER_4POINT1     (KSAUDIO_SPEAKER_QUAD|SPEAKER_LOW_FREQUENCY)
#define KSAUDIO_SPEAKER_2POINT1     (KSAUDIO_SPEAKER_STEREO|SPEAKER_LOW_FREQUENCY)


inline QWORD GetQWDif(QWORD val1, QWORD val2)
{
    return (val1 > val2) ? (val1-val2) : (val2-val1);
}

void MultiplyAudioBuffer(float *buffer, int totalFloats, float mulVal)
{
    float sum = 0.0f;
    int totalFloatsStore = totalFloats;

    if((UPARAM(buffer) & 0xF) == 0)
    {
        UINT alignedFloats = totalFloats & 0xFFFFFFFC;
        __m128 sseMulVal = _mm_set_ps1(mulVal);

        for(UINT i=0; i<alignedFloats; i += 4)
        {
            __m128 sseScaledVals = _mm_mul_ps(_mm_load_ps(buffer+i), sseMulVal);
            _mm_store_ps(buffer+i, sseScaledVals);
        }

        buffer      += alignedFloats;
        totalFloats -= alignedFloats;
    }

    for(int i=0; i<totalFloats; i++)
        buffer[i] *= mulVal;
}


AudioSource::AudioSource()
{
    sourceVolume = 1.0f;
}

AudioSource::~AudioSource()
{
    if(bResample)
        src_delete((SRC_STATE*)resampler);

    for(UINT i=0; i<audioSegments.Num(); i++)
        delete audioSegments[i];
}

union TripleToLong
{
    LONG val;
    struct 
    {
        WORD wVal;
        BYTE tripleVal;
        BYTE lastByte;
    };
};

void AudioSource::InitAudioData(bool bFloat, UINT channels, UINT samplesPerSec, UINT bitsPerSample, UINT blockSize, DWORD channelMask)
{
    this->bFloat = bFloat;
    inputChannels = channels;
    inputSamplesPerSec = samplesPerSec;
    inputBitsPerSample = bitsPerSample;
    inputBlockSize = blockSize;
    inputChannelMask = channelMask;

    //-----------------------------

    if(inputSamplesPerSec != 44100)
    {
        int errVal;

        int converterType = API->UseHighQualityResampling() ? SRC_SINC_FASTEST : SRC_LINEAR;
        resampler = src_new(converterType, 2, &errVal);//SRC_SINC_FASTEST//SRC_ZERO_ORDER_HOLD
        if(!resampler)
            CrashError(TEXT("AudioSource::InitAudioData: Could not initiate resampler"));

        resampleRatio = 44100.0 / double(inputSamplesPerSec);
        bResample = true;

        //----------------------------------------------------
        // hack to get rid of that weird first quirky resampled packet size
        // (always returns a non-441 sized packet on the first resample)

        SRC_DATA data;
        data.src_ratio = resampleRatio;

        List<float> blankBuffer;
        blankBuffer.SetSize(inputSamplesPerSec/100*2);

        data.data_in = blankBuffer.Array();
        data.input_frames = inputSamplesPerSec/100;

        UINT frameAdjust = UINT((double(data.input_frames) * resampleRatio) + 1.0);
        UINT newFrameSize = frameAdjust*2;

        tempResampleBuffer.SetSize(newFrameSize);

        data.data_out = tempResampleBuffer.Array();
        data.output_frames = frameAdjust;

        data.end_of_input = 0;

        int err = src_process((SRC_STATE*)resampler, &data);

        nop();
    }

    //-------------------------------------------------------------------------

    if(inputChannels > 2)
    {
        if(inputChannelMask == 0)
        {
            switch(inputChannels)
            {
                case 3: inputChannelMask = KSAUDIO_SPEAKER_2POINT1; break;
                case 4: inputChannelMask = KSAUDIO_SPEAKER_QUAD;    break;
                case 5: inputChannelMask = KSAUDIO_SPEAKER_4POINT1; break;
                case 6: inputChannelMask = KSAUDIO_SPEAKER_5POINT1; break;
                case 8: inputChannelMask = KSAUDIO_SPEAKER_7POINT1; break;
            }
        }

        switch(inputChannelMask)
        {
            case KSAUDIO_SPEAKER_QUAD:              Log(TEXT("Using quad speaker setup"));                          break; //ocd anyone?
            case KSAUDIO_SPEAKER_2POINT1:           Log(TEXT("Using 2.1 speaker setup"));                           break;
            case KSAUDIO_SPEAKER_4POINT1:           Log(TEXT("Using 4.1 speaker setup"));                           break;
            case KSAUDIO_SPEAKER_SURROUND:          Log(TEXT("Using basic surround speaker setup"));                break;
            case KSAUDIO_SPEAKER_5POINT1:           Log(TEXT("Using 5.1 speaker setup"));                           break;
            case KSAUDIO_SPEAKER_5POINT1_SURROUND:  Log(TEXT("Using 5.1 surround speaker setup"));                  break;
            case KSAUDIO_SPEAKER_7POINT1:           Log(TEXT("Using 7.1 speaker setup (experimental)"));            break;
            case KSAUDIO_SPEAKER_7POINT1_SURROUND:  Log(TEXT("Using 7.1 surround speaker setup (experimental)"));   break;

            default:
                Log(TEXT("Using unknown speaker setup: 0x%lX"), inputChannelMask);
                CrashError(TEXT("Speaker setup not yet implemented -- dear god of all the audio APIs, the one I -have- to use doesn't support resampling or downmixing.  fabulous."));
                break;
        }
    }
}


const float dbMinus3    = 0.7071067811865476f;
const float dbMinus6    = 0.5f;
const float dbMinus9    = 0.3535533905932738f;

//not entirely sure if these are the correct coefficients for downmixing,
//I'm fairly new to the whole multi speaker thing
const float surroundMix = dbMinus3;
const float centerMix   = dbMinus6;
const float lowFreqMix  = dbMinus3;

const float surroundMix4 = dbMinus6;

const float attn5dot1 = 1.0f / (1.0f + centerMix + surroundMix);
const float attn4dotX = 1.0f / (1.0f + surroundMix4);

void AudioSource::AddAudioSegment(AudioSegment *newSegment, float curVolume)
{
    for (UINT i=0; i<audioFilters.Num(); i++)
    {
        if (newSegment)
            newSegment = audioFilters[i]->Process(newSegment);
    }

    if (newSegment)
    {
        MultiplyAudioBuffer(newSegment->audioData.Array(), newSegment->audioData.Num(), curVolume*sourceVolume);
        audioSegments << newSegment;
    }
}

UINT AudioSource::QueryAudio(float curVolume)
{
    LPVOID buffer;
    UINT numAudioFrames;
    QWORD newTimestamp;

    if(GetNextBuffer((void**)&buffer, &numAudioFrames, &newTimestamp))
    {
        //------------------------------------------------------------
        // convert to float

        float *captureBuffer;

        if(!bFloat)
        {
            UINT totalSamples = numAudioFrames*inputChannels;
            if(convertBuffer.Num() < totalSamples)
                convertBuffer.SetSize(totalSamples);

            if(inputBitsPerSample == 8)
            {
                float *tempConvert = convertBuffer.Array();
                char *tempSByte = (char*)buffer;

                while(totalSamples--)
                {
                    *(tempConvert++) = float(*(tempSByte++))/127.0f;
                }
            }
            else if(inputBitsPerSample == 16)
            {
                float *tempConvert = convertBuffer.Array();
                short *tempShort = (short*)buffer;

                while(totalSamples--)
                {
                    *(tempConvert++) = float(*(tempShort++))/32767.0f;
                }
            }
            else if(inputBitsPerSample == 24)
            {
                float *tempConvert = convertBuffer.Array();
                BYTE *tempTriple = (BYTE*)buffer;
                TripleToLong valOut;

                while(totalSamples--)
                {
                    TripleToLong &valIn  = (TripleToLong&)tempTriple;

                    valOut.wVal = valIn.wVal;
                    valOut.tripleVal = valIn.tripleVal;
                    if(valOut.tripleVal > 0x7F)
                        valOut.lastByte = 0xFF;

                    *(tempConvert++) = float(double(valOut.val)/8388607.0);
                    tempTriple += 3;
                }
            }
            else if(inputBitsPerSample == 32)
            {
                float *tempConvert = convertBuffer.Array();
                long *tempShort = (long*)buffer;

                while(totalSamples--)
                {
                    *(tempConvert++) = float(double(*(tempShort++))/2147483647.0);
                }
            }

            captureBuffer = convertBuffer.Array();
        }
        else
            captureBuffer = (float*)buffer;

        //------------------------------------------------------------
        // channel upmix/downmix

        if(tempBuffer.Num() < numAudioFrames*2)
            tempBuffer.SetSize(numAudioFrames*2);

        float *dataOutputBuffer = tempBuffer.Array();
        float *tempOut = dataOutputBuffer;

        if(inputChannels == 1)
        {
            UINT  numFloats   = numAudioFrames;
            float *inputTemp  = (float*)captureBuffer;
            float *outputTemp = dataOutputBuffer;

            if((UPARAM(inputTemp) & 0xF) == 0 && (UPARAM(outputTemp) & 0xF) == 0)
            {
                UINT alignedFloats = numFloats & 0xFFFFFFFC;
                for(UINT i=0; i<alignedFloats; i += 4)
                {
                    __m128 inVal   = _mm_load_ps(inputTemp+i);

                    __m128 outVal1 = _mm_unpacklo_ps(inVal, inVal);
                    __m128 outVal2 = _mm_unpackhi_ps(inVal, inVal);

                    _mm_store_ps(outputTemp+(i*2),   outVal1);
                    _mm_store_ps(outputTemp+(i*2)+4, outVal2);
                }

                numFloats  -= alignedFloats;
                inputTemp  += alignedFloats;
                outputTemp += alignedFloats*2;
            }

            while(numFloats--)
            {
                float inputVal = *inputTemp;
                *(outputTemp++) = inputVal;
                *(outputTemp++) = inputVal;

                inputTemp++;
            }
        }
        else if(inputChannels == 2) //straight up copy
        {
            SSECopy(dataOutputBuffer, captureBuffer, numAudioFrames*2*sizeof(float));
        }
        else
        {
            //todo: downmix optimization, also support for other speaker configurations than ones I can merely "think" of.  ugh.
            float *inputTemp  = (float*)captureBuffer;
            float *outputTemp = dataOutputBuffer;

            if(inputChannelMask == KSAUDIO_SPEAKER_QUAD)
            {
                UINT numFloats = numAudioFrames*4;
                float *endTemp = inputTemp+numFloats;

                while(inputTemp < endTemp)
                {
                    float left      = inputTemp[0];
                    float right     = inputTemp[1];
                    float rearLeft  = inputTemp[2]*surroundMix4;
                    float rearRight = inputTemp[3]*surroundMix4;

                    // When in doubt, use only left and right .... and rear left and rear right :) 
                    // Same idea as with 5.1 downmix

                    *(outputTemp++) = (left  + rearLeft)  * attn4dotX;
                    *(outputTemp++) = (right + rearRight) * attn4dotX;

                    inputTemp  += 4;
                }
            }
            else if(inputChannelMask == KSAUDIO_SPEAKER_2POINT1)
            {
                UINT numFloats = numAudioFrames*3;
                float *endTemp = inputTemp+numFloats;

                while(inputTemp < endTemp)
                {
                    float left      = inputTemp[0];
                    float right     = inputTemp[1];
                   
                    // Drop LFE since we don't need it
                    //float lfe       = inputTemp[2]*lowFreqMix;

                    *(outputTemp++) = left;
                    *(outputTemp++) = right;

                    inputTemp  += 3;
                }
            }
            else if(inputChannelMask == KSAUDIO_SPEAKER_4POINT1)
            {
                UINT numFloats = numAudioFrames*5;
                float *endTemp = inputTemp+numFloats;

                while(inputTemp < endTemp)
                {
                    float left      = inputTemp[0];
                    float right     = inputTemp[1];

                    // Skip LFE , we don't really need it.
                    //float lfe       = inputTemp[2];

                    float rearLeft  = inputTemp[3]*surroundMix4;
                    float rearRight = inputTemp[4]*surroundMix4;

                    // Same idea as with 5.1 downmix

                    *(outputTemp++) = (left  + rearLeft)  * attn4dotX;
                    *(outputTemp++) = (right + rearRight) * attn4dotX;

                    inputTemp  += 5;
                }
            }
            else if(inputChannelMask == KSAUDIO_SPEAKER_SURROUND)
            {
                UINT numFloats = numAudioFrames*4;
                float *endTemp = inputTemp+numFloats;

                while(inputTemp < endTemp)
                {
                    float left      = inputTemp[0];
                    float right     = inputTemp[1];
                    float frontCenter    = inputTemp[2];
                    float rearCenter     = inputTemp[3];
                    
                    // When in doubt, use only left and right :) Seriously.
                    // THIS NEEDS TO BE PROPERLY IMPLEMENTED!

                    *(outputTemp++) = left;
                    *(outputTemp++) = right;

                    inputTemp  += 4;
                }
            }
            // Both speakers configs share the same format, the difference is in rear speakers position 
            // See: http://msdn.microsoft.com/en-us/library/windows/hardware/ff537083(v=vs.85).aspx
            // Probably for KSAUDIO_SPEAKER_5POINT1_SURROUND we will need a different coefficient for rear left/right
            else if(inputChannelMask == KSAUDIO_SPEAKER_5POINT1 || inputChannelMask == KSAUDIO_SPEAKER_5POINT1_SURROUND)
            {
                UINT numFloats = numAudioFrames*6;
                float *endTemp = inputTemp+numFloats;

                while(inputTemp < endTemp)
                {
                    float left      = inputTemp[0];
                    float right     = inputTemp[1];
                    float center    = inputTemp[2]*centerMix;
                    
                    //We don't need LFE channel so skip it (see below)
                    //float lowFreq   = inputTemp[3]*lowFreqMix;
                    
                    float rearLeft  = inputTemp[4]*surroundMix;
                    float rearRight = inputTemp[5]*surroundMix;
                    
                    // According to ITU-R  BS.775-1 recommendation, the downmix from a 3/2 source to stereo
                    // is the following:
                    // L = FL + k0*C + k1*RL
                    // R = FR + k0*C + k1*RR
                    // FL = front left
                    // FR = front right
                    // C  = center
                    // RL = rear left
                    // RR = rear right
                    // k0 = centerMix   = dbMinus3 = 0.7071067811865476 [for k0 we can use dbMinus6 = 0.5 too, probably it's better]
                    // k1 = surroundMix = dbMinus3 = 0.7071067811865476

                    // The output (L,R) can be out of (-1,1) domain so we attenuate it [ attn5dot1 = 1/(1 + centerMix + surroundMix) ]
                    // Note: this method of downmixing is far from "perfect" (pretty sure it's not the correct way) but the resulting downmix is "okayish", at least no more bleeding ears.
                    // (maybe have a look at http://forum.doom9.org/archive/index.php/t-148228.html too [ 5.1 -> stereo ] the approach seems almost the same [but different coefficients])

                    
                    // http://acousticsfreq.com/blog/wp-content/uploads/2012/01/ITU-R-BS775-1.pdf
                    // http://ir.lib.nctu.edu.tw/bitstream/987654321/22934/1/030104001.pdf

                    *(outputTemp++) = (left  + center  + rearLeft) * attn5dot1;
                    *(outputTemp++) = (right + center  + rearRight) * attn5dot1;

                    inputTemp  += 6;
                }
            }

            // According to http://msdn.microsoft.com/en-us/library/windows/hardware/ff537083(v=vs.85).aspx
            // KSAUDIO_SPEAKER_7POINT1 is obsolete and no longer supported in Windows Vista and later versions of Windows
            // Not sure what to do about it, meh , drop front left of center/front right of center -> 5.1 -> stereo; 

            else if(inputChannelMask == KSAUDIO_SPEAKER_7POINT1)
            {
                UINT numFloats = numAudioFrames*8;
                float *endTemp = inputTemp+numFloats;

                while(inputTemp < endTemp)
                {
                    float left          = inputTemp[0];
                    float right         = inputTemp[1];
                    
                    float center        = inputTemp[2] * centerMix;
                    
                    // Drop LFE since we don't need it
                    //float lowFreq       = inputTemp[3]*lowFreqMix;
                    
                    float rearLeft      = inputTemp[4] * surroundMix;
                    float rearRight     = inputTemp[5] * surroundMix;

                    // Drop SPEAKER_FRONT_LEFT_OF_CENTER , SPEAKER_FRONT_RIGHT_OF_CENTER
                    //float centerLeft    = inputTemp[6];
                    //float centerRight   = inputTemp[7];
                    
                    // Downmix from 5.1 to stereo
                    *(outputTemp++) = (left  + center  + rearLeft)  * attn5dot1;
                    *(outputTemp++) = (right + center  + rearRight) * attn5dot1;

                    inputTemp  += 8;
                }
            }

            // Downmix to 5.1 (easy stuff) then downmix to stereo as done in KSAUDIO_SPEAKER_5POINT1
            else if(inputChannelMask == KSAUDIO_SPEAKER_7POINT1_SURROUND)
            {
                UINT numFloats = numAudioFrames*8;
                float *endTemp = inputTemp+numFloats;

                while(inputTemp < endTemp)
                {
                    float left      = inputTemp[0];
                    float right     = inputTemp[1];
                    float center    = inputTemp[2] * centerMix;

                    // Skip LFE we don't need it
                    //float lowFreq   = inputTemp[3]*lowFreqMix;

                    float rearLeft  = inputTemp[4];
                    float rearRight = inputTemp[5];
                    float sideLeft  = inputTemp[6];
                    float sideRight = inputTemp[7];

                    // combine the rear/side channels first , baaam! 5.1
                    rearLeft  = (rearLeft  + sideLeft)  * 0.5f;
                    rearRight = (rearRight + sideRight) * 0.5f;
                    
                    // downmix to stereo as in 5.1 case
                    *(outputTemp++) = (left  + center + rearLeft  * surroundMix) * attn5dot1;
                    *(outputTemp++) = (right + center + rearRight * surroundMix) * attn5dot1;

                    inputTemp  += 8;
                }
            }
        }

        ReleaseBuffer();

        //------------------------------------------------------------
        // resample

        if(bResample)
        {
            UINT frameAdjust = UINT((double(numAudioFrames) * resampleRatio) + 1.0);
            UINT newFrameSize = frameAdjust*2;

            if(tempResampleBuffer.Num() < newFrameSize)
                tempResampleBuffer.SetSize(newFrameSize);

            SRC_DATA data;
            data.src_ratio = resampleRatio;

            data.data_in = tempBuffer.Array();
            data.input_frames = numAudioFrames;

            data.data_out = tempResampleBuffer.Array();
            data.output_frames = frameAdjust;

            data.end_of_input = 0;

            int err = src_process((SRC_STATE*)resampler, &data);
            if(err)
            {
                RUNONCE AppWarning(TEXT("AudioSource::QueryAudio: Was unable to resample audio for device '%s'"), GetDeviceName());
                return NoAudioAvailable;
            }

            if(data.input_frames_used != numAudioFrames)
            {
                RUNONCE AppWarning(TEXT("AudioSource::QueryAudio: Failed to downsample buffer completely, which shouldn't actually happen because it should be using 10ms of samples"));
                return NoAudioAvailable;
            }

            numAudioFrames = data.output_frames_gen;
        }

        //-----------------------------------------------------------------------------
        // sort all audio frames into 10 millisecond increments (done because not all devices output in 10ms increments)
        // NOTE: 0.457+ - instead of using the timestamps from windows, just compare and make sure it stays within a 100ms of their timestamps

        if(!bFirstBaseFrameReceived)
        {
            lastUsedTimestamp = newTimestamp;
            bFirstBaseFrameReceived = true;
        }

        float *newBuffer = (bResample) ? tempResampleBuffer.Array() : tempBuffer.Array();

        if(storageBuffer.Num() == 0 && numAudioFrames == 441)
        {
            lastUsedTimestamp += 10;

            QWORD difVal = GetQWDif(newTimestamp, lastUsedTimestamp);
            if(difVal > 70)
            {
                //Log(TEXT("----------------------------1\r\nlastUsedTimestamp before: %llu"), lastUsedTimestamp);
                lastUsedTimestamp = newTimestamp;
                //Log(TEXT("lastUsedTimestamp after: %llu"), lastUsedTimestamp);
            }

            if(lastUsedTimestamp > lastSentTimestamp)
            {
                QWORD adjustVal = (lastUsedTimestamp-lastSentTimestamp);
                if(adjustVal < 10)
                    lastUsedTimestamp += 10-adjustVal;

                AudioSegment *newSegment = new AudioSegment(newBuffer, numAudioFrames*2, lastUsedTimestamp);
                AddAudioSegment(newSegment, curVolume*sourceVolume);

                lastSentTimestamp = lastUsedTimestamp;
            }
        }
        else
        {
            UINT storedFrames = storageBuffer.Num();

            storageBuffer.AppendArray(newBuffer, numAudioFrames*2);
            if(storageBuffer.Num() >= (441*2))
            {
                lastUsedTimestamp += 10;

                QWORD difVal = GetQWDif(newTimestamp, lastUsedTimestamp);
                if(difVal > 70)
                {
                    //Log(TEXT("----------------------------2\r\nlastUsedTimestamp before: %llu"), lastUsedTimestamp);
                    lastUsedTimestamp = newTimestamp - (QWORD(storedFrames)/2*1000/44100);
                    //Log(TEXT("lastUsedTimestamp after: %llu"), lastUsedTimestamp);
                }

                //------------------------
                // add new data

                bool bFirst = true;
                while(storageBuffer.Num() >= (441*2))
                {
                    if(!bFirst)
                        lastUsedTimestamp += 10;

                    if(lastUsedTimestamp > lastSentTimestamp)
                    {
                        QWORD adjustVal = (lastUsedTimestamp-lastSentTimestamp);
                        if(adjustVal < 10)
                            lastUsedTimestamp += 10-adjustVal;

                        AudioSegment *newSegment = new AudioSegment(storageBuffer.Array(), 441*2, lastUsedTimestamp);
                        AddAudioSegment(newSegment, curVolume*sourceVolume);
                        storageBuffer.RemoveRange(0, (441*2));

                        if(!bFirst)
                            lastSentTimestamp = lastUsedTimestamp;
                    }

                    if(bFirst)
                        bFirst = false;
                }
            }
        }

        //-----------------------------------------------------------------------------

        return AudioAvailable;
    }

    return NoAudioAvailable;
}

bool AudioSource::GetEarliestTimestamp(QWORD &timestamp)
{
    if(audioSegments.Num())
    {
        timestamp = audioSegments[0]->timestamp;
        return true;
    }

    return false;
}

bool AudioSource::GetBuffer(float **buffer, UINT *numFrames, QWORD targetTimestamp)
{
    bool bSuccess = false;
    outputBuffer.Clear();

    while(audioSegments.Num())
    {
        if(audioSegments[0]->timestamp < targetTimestamp)
        {
            Log(TEXT("Audio timestamp for device '%s' was behind target timestamp by %llu!  Had to delete audio segment."),
                                                              GetDeviceName(), targetTimestamp-audioSegments[0]->timestamp);
            delete audioSegments[0];
            audioSegments.Remove(0);
        }
        else
            break;
    }

    if(audioSegments.Num())
    {
        bool bUseSegment = false;

        AudioSegment *segment = audioSegments[0];

        QWORD difference = (segment->timestamp-targetTimestamp);
        if(difference <= 10)
        {
            //Log(TEXT("segment.timestamp: %llu, targetTimestamp: %llu"), segment.timestamp, targetTimestamp);
            outputBuffer.TransferFrom(segment->audioData);

            delete segment;
            audioSegments.Remove(0);

            bSuccess = true;
        }
    }

    outputBuffer.SetSize(441*2);

    *buffer = outputBuffer.Array();
    *numFrames = outputBuffer.Num()/2;

    return bSuccess;
}

bool AudioSource::GetNewestFrame(float **buffer, UINT *numFrames)
{
    if(buffer && numFrames)
    {
        if(audioSegments.Num())
        {
            List<float> &data = audioSegments.Last()->audioData;
            *buffer = data.Array();
            *numFrames = data.Num()/2;
            return true;
        }
    }

    return false;
}

QWORD AudioSource::GetBufferedTime()
{
    if(audioSegments.Num())
        return audioSegments.Last()->timestamp - audioSegments[0]->timestamp;

    return 0;
}

void AudioSource::StartCapture() {}
void AudioSource::StopCapture() {}

UINT AudioSource::GetChannelCount() const {return inputChannels;}
UINT AudioSource::GetSamplesPerSec() const {return inputSamplesPerSec;}

int  AudioSource::GetTimeOffset() const {return timeOffset;}
void AudioSource::SetTimeOffset(int newOffset) {timeOffset = newOffset;}

void AudioSource::SetVolume(float fVal) {sourceVolume = fabsf(fVal);}
float AudioSource::GetVolume() const {return sourceVolume;}

UINT AudioSource::NumAudioFilters() const {return audioFilters.Num();}
AudioFilter* AudioSource::GetAudioFilter(UINT id) {if(audioFilters.Num() > id) return audioFilters[id]; return NULL;}

void AudioSource::AddAudioFilter(AudioFilter *filter) {audioFilters << filter;}
void AudioSource::InsertAudioFilter(UINT pos, AudioFilter *filter) {audioFilters.Insert(pos, filter);}
void AudioSource::RemoveAudioFilter(AudioFilter *filter) {audioFilters.RemoveItem(filter);}
void AudioSource::RemoveAudioFilter(UINT id) {if(audioFilters.Num() > id) audioFilters.Remove(id);}
