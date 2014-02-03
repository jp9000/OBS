/********************************************************************************
 Copyright (C) 2014 Timo Rothenpieler <timo@rothenpieler.org>
 
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

#include "nvmain.h"
#include "NVENCEncoder.h"
#include "license.h"

#include <ws2tcpip.h>

#include <../libmfx/include/msdk/include/mfxstructures.h>

extern "C"
{
#define _STDINT_H
#include <../x264/x264.h>
#undef _STDINT_H
}


NVENCEncoder::NVENCEncoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR)
    : alive(false)
    , cuContext(0)
    , encoder(0)
    , fps(fps)
    , width(width)
    , height(height)
    , quality(quality)
    , preset(preset)
    , bUse444(bUse444)
    , colorDesc(colorDesc)
    , maxBitRate(maxBitRate)
    , bufferSize(bufferSize)
    , bUseCFR(bUseCFR)
    , frameShift(0)
    , pstart(0)
{
    bUseCBR = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseCBR"), 1) != 0;
    keyint = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("KeyframeInterval"), 0);

    int numMBs = ((width + 15) / 16) * ((height + 15) / 16);
    maxSurfaceCount = (numMBs >= 8160) ? 16 : 32;

    inputSurfaces = new NVENCEncoderSurface[maxSurfaceCount];
    outputSurfaces = new NVENCEncoderOutputSurface[maxSurfaceCount];

    headerPacket.SetSize(128);
    
    frameMutex = OSCreateMutex();

    pstart = new uint8_t[1024 * 1024];

    init();
}

NVENCEncoder::~NVENCEncoder()
{
    if (alive)
    {
        for (int i = 0; i < maxSurfaceCount; ++i)
        {
            if (inputSurfaces[i].locked)
                pNvEnc->nvEncUnlockInputBuffer(encoder, inputSurfaces[i].inputSurface);

            pNvEnc->nvEncDestroyInputBuffer(encoder, inputSurfaces[i].inputSurface);
            pNvEnc->nvEncDestroyBitstreamBuffer(encoder, outputSurfaces[i].outputSurface);
        }

        pNvEnc->nvEncDestroyEncoder(encoder);
        cuCtxDestroy(cuContext);

        NvLog(TEXT("Encoder closed"));
    }

    outputSurfaceQueueReady = std::queue<NVENCEncoderOutputSurface*>();
    outputSurfaceQueue = std::queue<NVENCEncoderOutputSurface*>();
    delete[] inputSurfaces;
    delete[] outputSurfaces;

    encoderRefDec();

    OSCloseMutex(frameMutex);

    delete[] pstart;
}

bool NVENCEncoder::populateEncodePresetGUIDs()
{
    uint32_t count = 0;
    NVENCSTATUS nvStatus = pNvEnc->nvEncGetEncodePresetCount(encoder, NV_ENC_CODEC_H264_GUID, &count);
    if (nvStatus != NV_ENC_SUCCESS)
    {
        NvLog(TEXT("Failed getting preset guid list count: 0x%x"), nvStatus);
        return false;
    }

    encodePresetGUIDs.SetSize(count);

    nvStatus = pNvEnc->nvEncGetEncodePresetGUIDs(encoder, NV_ENC_CODEC_H264_GUID, encodePresetGUIDs.Array(), encodePresetGUIDs.Num(), &count);
    if (nvStatus != NV_ENC_SUCCESS)
    {
        NvLog(TEXT("Failed getting preset guid list: 0x%x"), nvStatus);
        return false;
    }

    encodePresetGUIDs.SetSize(count);

    NvLog(TEXT("NVENC supports %d h264 presets"), count);

    return true;
}

bool NVENCEncoder::checkPresetSupport(const GUID &preset)
{
    for (unsigned int i = 0; i < encodePresetGUIDs.Num(); ++i)
    {
        const GUID &pr = encodePresetGUIDs[i];
        if (memcmp(&pr, &preset, sizeof(GUID)) == 0)
            return true;
    }

    return false;
}

void NVENCEncoder::init()
{
    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS stEncodeSessionParams = { 0 };
    NV_ENC_PRESET_CONFIG presetConfig = { 0 };
    GUID clientKey = NV_CLIENT_KEY;
    CUcontext cuContextCurr;
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
    int surfaceCount = 0;

    GUID encoderPreset = NV_ENC_PRESET_HQ_GUID;

    String profileString = AppConfig->GetString(TEXT("Video Encoding"), TEXT("X264Profile"), TEXT("high"));

    String presetString = AppConfig->GetString(TEXT("Video Encoding"), TEXT("NVENCPreset"), TEXT("High Quality"));

    if (presetString == TEXT("High Performance"))
        encoderPreset = NV_ENC_PRESET_HP_GUID;
    else if (presetString == TEXT("High Quality"))
        encoderPreset = NV_ENC_PRESET_HQ_GUID;
    else if (presetString == TEXT("Bluray Disk"))
        encoderPreset = NV_ENC_PRESET_BD_GUID;
    else if (presetString == TEXT("Low Latency"))
        encoderPreset = NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID;
    else if (presetString == TEXT("High Performance Low Latency"))
        encoderPreset = NV_ENC_PRESET_LOW_LATENCY_HP_GUID;
    else if (presetString == TEXT("High Quality Low Latency"))
        encoderPreset = NV_ENC_PRESET_LOW_LATENCY_HQ_GUID;
    else if (presetString == TEXT("Default"))
        encoderPreset = NV_ENC_PRESET_DEFAULT_GUID;

    TCHAR envClientKey[128] = { 0 };
    DWORD envRes = GetEnvironmentVariable(TEXT("NVENC_KEY"), envClientKey, 128);
    if (envRes > 0 && envRes <= 128)
    {
        if (stringToGuid(envClientKey, &clientKey))
            NvLog(TEXT("Got NVENC key from environment"));
        else
            NvLog(TEXT("NVENC_KEY environment variable has invalid format"));
    }

    mset(&initEncodeParams, 0, sizeof(NV_ENC_INITIALIZE_PARAMS));
    mset(&encodeConfig, 0, sizeof(NV_ENC_CONFIG));

    encodeConfig.version = NV_ENC_CONFIG_VER;
    presetConfig.version = NV_ENC_PRESET_CONFIG_VER;
    initEncodeParams.version = NV_ENC_INITIALIZE_PARAMS_VER;

    stEncodeSessionParams.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
    stEncodeSessionParams.apiVersion = NVENCAPI_VERSION;
    stEncodeSessionParams.clientKeyPtr = &clientKey;

    cuContext = 0;
    checkCudaErrors(cuCtxCreate(&cuContext, 0, pNvencDevices[iNvencUseDeviceID]));
    checkCudaErrors(cuCtxPopCurrent(&cuContextCurr));

    stEncodeSessionParams.device = (void*)cuContext;
    stEncodeSessionParams.deviceType = NV_ENC_DEVICE_TYPE_CUDA;

    nvStatus = pNvEnc->nvEncOpenEncodeSessionEx(&stEncodeSessionParams, &encoder);
    if (nvStatus != NV_ENC_SUCCESS)
    {
        encoder = 0;

        MessageBox(NULL,
            TEXT("OpenEncodeSessionEx failed!\r\n\r\n")
            TEXT("You most likely don't have a valid license key.\r\n")
            TEXT("Either put one in the environment variable NVENC_KEY\r\n")
            TEXT("or download a prebuilt ObsNvenc.dll with a\r\n")
            TEXT("license key included from the OBS forums."),
            NULL,
            MB_OK | MB_ICONERROR);

        NvLog(TEXT("nvEncOpenEncodeSessionEx failed - invalid license key?"));
        goto error;
    }

    if (!populateEncodePresetGUIDs())
        goto error;

    if (!checkPresetSupport(encoderPreset))
    {
        NvLog(TEXT("Preset is not supported!"));
        goto error;
    }

    nvStatus = pNvEnc->nvEncGetEncodePresetConfig(encoder, NV_ENC_CODEC_H264_GUID, encoderPreset, &presetConfig);
    if (nvStatus != NV_ENC_SUCCESS)
    {
        NvLog(TEXT("nvEncGetEncodePresetConfig failed"));
        goto error;
    }

    initEncodeParams.encodeGUID = NV_ENC_CODEC_H264_GUID;
    initEncodeParams.encodeHeight = height;
    initEncodeParams.encodeWidth = width;
    initEncodeParams.darHeight = height;
    initEncodeParams.darWidth = width;
    initEncodeParams.frameRateNum = fps;
    initEncodeParams.frameRateDen = 1;

    initEncodeParams.enableEncodeAsync = 0;
    initEncodeParams.enablePTD = 1;

    initEncodeParams.presetGUID = encoderPreset;
    initEncodeParams.encodeConfig = &encodeConfig;
    memcpy(&encodeConfig, &presetConfig.presetCfg, sizeof(NV_ENC_CONFIG));

    if (keyint != 0)
        encodeConfig.gopLength = keyint * fps;

    if (maxBitRate != -1)
    {
        encodeConfig.rcParams.averageBitRate = maxBitRate * 1000;
        encodeConfig.rcParams.maxBitRate = maxBitRate * 1000;
    }

    if (bufferSize != -1)
        encodeConfig.rcParams.vbvBufferSize = bufferSize * 1000;

    if (bUseCBR)
        encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;

    encodeConfig.encodeCodecConfig.h264Config.enableVFR = bUseCFR ? 0 : 1;

    encodeConfig.frameFieldMode = NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME;

    if (profileString.CompareI(TEXT("main")))
        encodeConfig.profileGUID = NV_ENC_H264_PROFILE_MAIN_GUID;
    else if (profileString.CompareI(TEXT("high")))
        encodeConfig.profileGUID = NV_ENC_H264_PROFILE_HIGH_GUID;

    encodeConfig.encodeCodecConfig.h264Config.bdirectMode = encodeConfig.frameIntervalP > 1 ? NV_ENC_H264_BDIRECT_MODE_TEMPORAL : NV_ENC_H264_BDIRECT_MODE_DISABLE;

    encodeConfig.encodeCodecConfig.h264Config.idrPeriod = encodeConfig.gopLength;
    encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.videoSignalTypePresentFlag = 1;
    encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.colourDescriptionPresentFlag = 1;
    encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.colourMatrix = colorDesc.matrix;
    encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.videoFullRangeFlag = colorDesc.fullRange;
    encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.videoFormat = 5;
    encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.colourPrimaries = colorDesc.primaries;
    encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.transferCharacteristics = colorDesc.transfer;

    tryParseEncodeConfig();
    dumpEncodeConfig();

    nvStatus = pNvEnc->nvEncInitializeEncoder(encoder, &initEncodeParams);

    if (nvStatus != NV_ENC_SUCCESS)
    {
        NvLog(TEXT("nvEncInitializeEncoder failed"));
        goto error;
    }

    for (surfaceCount = 0; surfaceCount < maxSurfaceCount; ++surfaceCount)
    {
        NV_ENC_CREATE_INPUT_BUFFER allocSurf = { 0 };
        allocSurf.version = NV_ENC_CREATE_INPUT_BUFFER_VER;

        allocSurf.width = (width + 31) & ~31;
        allocSurf.height = (height + 31) & ~31;

        allocSurf.memoryHeap = NV_ENC_MEMORY_HEAP_SYSMEM_CACHED;
        allocSurf.bufferFmt = NV_ENC_BUFFER_FORMAT_NV12_PL;

        nvStatus = pNvEnc->nvEncCreateInputBuffer(encoder, &allocSurf);

        if (nvStatus != NV_ENC_SUCCESS)
        {
            NvLog(TEXT("nvEncCreateInputBuffer #%d failed"), surfaceCount);
            goto error;
        }

        inputSurfaces[surfaceCount].width = allocSurf.width;
        inputSurfaces[surfaceCount].height = allocSurf.height;
        inputSurfaces[surfaceCount].locked = false;
        inputSurfaces[surfaceCount].useCount = 0;
        inputSurfaces[surfaceCount].inputSurface = allocSurf.inputBuffer;


        NV_ENC_CREATE_BITSTREAM_BUFFER allocOut = { 0 };
        allocOut.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
        allocOut.size = 1024 * 1024;
        allocOut.memoryHeap = NV_ENC_MEMORY_HEAP_SYSMEM_CACHED;

        nvStatus = pNvEnc->nvEncCreateBitstreamBuffer(encoder, &allocOut);

        if (nvStatus != NV_ENC_SUCCESS)
        {
            NvLog(TEXT("Failed allocating bitstream #%d buffer"), surfaceCount);

            outputSurfaces[surfaceCount].outputSurface = 0;
            surfaceCount += 1;

            goto error;
        }

        outputSurfaces[surfaceCount].outputSurface = allocOut.bitstreamBuffer;
        outputSurfaces[surfaceCount].size = allocOut.size;
        outputSurfaces[surfaceCount].busy = false;
    }

    NvLog(TEXT("------------------------------------------"));
    NvLog(TEXT("%s"), GetInfoString().Array());
    NvLog(TEXT("------------------------------------------"));

    alive = true;

    return;

error:
    alive = false;

    for (int i = 0; i < surfaceCount; ++i)
    {
        pNvEnc->nvEncDestroyInputBuffer(encoder, inputSurfaces[i].inputSurface);
        if (outputSurfaces[i].outputSurface != 0)
            pNvEnc->nvEncDestroyBitstreamBuffer(encoder, outputSurfaces[i].outputSurface);
    }
    surfaceCount = 0;

    if (encoder)
        pNvEnc->nvEncDestroyEncoder(encoder);
    encoder = 0;

    if (cuContext)
        cuCtxDestroy(cuContext);
}

void NVENCEncoder::RequestBuffers(LPVOID buffers)
{
    if (!buffers)
        return;
    
    OSMutexLocker locker(frameMutex);

    mfxFrameData *buff = (mfxFrameData*)buffers;

    if (buff->MemId && inputSurfaces[(unsigned int)buff->MemId - 1].locked)
        return;

    for (int i = 0; i < maxSurfaceCount; ++i)
    {
        if (inputSurfaces[i].locked || inputSurfaces[i].useCount > 0)
            continue;

        NV_ENC_LOCK_INPUT_BUFFER lockBufferParams = { 0 };

        lockBufferParams.version = NV_ENC_LOCK_INPUT_BUFFER_VER;
        lockBufferParams.inputBuffer = inputSurfaces[i].inputSurface;

        NVENCSTATUS nvStatus = pNvEnc->nvEncLockInputBuffer(encoder, &lockBufferParams);

        if (nvStatus != NV_ENC_SUCCESS)
        {
            NvLog(TEXT("nvEncLockInputBuffer failed for surface #%d"), i);

            return;
        }

        buff->Pitch = lockBufferParams.pitch;
        buff->Y = (mfxU8*)lockBufferParams.bufferDataPtr;
        buff->UV = buff->Y + (inputSurfaces[i].height * buff->Pitch);

        buff->MemId = mfxMemId(i + 1);
        inputSurfaces[i].locked = true;

        return;
    }

    NvLog(TEXT("No unlocked frame found"));
}

bool NVENCEncoder::Encode(LPVOID picIn, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD timestamp)
{
    NVENCSTATUS nvStatus;
    int i = -1;

    NV_ENC_PIC_PARAMS picParams = { 0 };
    picParams.version = NV_ENC_PIC_PARAMS_VER;

    OSMutexLocker locker(frameMutex);

    packets.Clear();
    packetTypes.Clear();
    
    if (picIn)
    {
        mfxFrameSurface1 *inputSurface = (mfxFrameSurface1*)picIn;
        mfxFrameData &data = inputSurface->Data;
        assert(data.MemId);

        NVENCEncoderSurface *surf = &inputSurfaces[(unsigned int)data.MemId - 1];
        
        if (surf->locked)
        {
            nvStatus = pNvEnc->nvEncUnlockInputBuffer(encoder, surf->inputSurface);
            if (nvStatus != NV_ENC_SUCCESS)
            {
                NvLog(TEXT("Unlocking surface failed"));
                return false;
            }
        }

        for (i = 0; i < maxSurfaceCount; ++i)
        {
            if (!outputSurfaces[i].busy)
                break;
        }

        if (i == maxSurfaceCount)
        {
            NvLog(TEXT("Out of output buffers!"));
            surf->locked = false;
            return false;
        }

        surf->locked = false;
        surf->useCount += 1;

        outputSurfaces[i].timestamp = timestamp;
        outputSurfaces[i].inputTimestamp = data.TimeStamp;
        outputSurfaces[i].inSurf = surf;

        picParams.inputBuffer = surf->inputSurface;
        picParams.bufferFmt = NV_ENC_BUFFER_FORMAT_NV12_PL;
        picParams.inputWidth = width;
        picParams.inputHeight = height;
        picParams.outputBitstream = outputSurfaces[i].outputSurface;
        picParams.completionEvent = 0;
        picParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME; // At least i hope so
        picParams.encodePicFlags = 0;
        picParams.inputTimeStamp = data.TimeStamp;
        picParams.inputDuration = 0;
        picParams.codecPicParams.h264PicParams.sliceMode = encodeConfig.encodeCodecConfig.h264Config.sliceMode;
        picParams.codecPicParams.h264PicParams.sliceModeData = encodeConfig.encodeCodecConfig.h264Config.sliceModeData;
        memcpy(&picParams.rcParams, &encodeConfig.rcParams, sizeof(NV_ENC_RC_PARAMS));
    }
    else
    {
        picParams.encodePicFlags = NV_ENC_PIC_FLAG_EOS;
    }

    nvStatus = pNvEnc->nvEncEncodePicture(encoder, &picParams);

    if (picIn && nvStatus == NV_ENC_ERR_NEED_MORE_INPUT)
    {
        outputSurfaceQueue.push(&outputSurfaces[i]);
        outputSurfaces[i].busy = true;
    }

    if (nvStatus != NV_ENC_SUCCESS && nvStatus != NV_ENC_ERR_NEED_MORE_INPUT)
    {
        NvLog(TEXT("nvEncEncodePicture failed with error 0x%x"), nvStatus);
        return false;
    }

    if (nvStatus != NV_ENC_ERR_NEED_MORE_INPUT)
    {
        while (!outputSurfaceQueue.empty())
        {
            NVENCEncoderOutputSurface *qSurf = outputSurfaceQueue.front();
            outputSurfaceQueue.pop();
            outputSurfaceQueueReady.push(qSurf);
        }

        if (picIn)
        {
            outputSurfaceQueueReady.push(&outputSurfaces[i]);
            outputSurfaces[i].busy = true;
        }
    }

    if (!outputSurfaceQueueReady.empty())
    {
        NVENCEncoderOutputSurface *qSurf = outputSurfaceQueueReady.front();
        outputSurfaceQueueReady.pop();

        ProcessOutput(qSurf, packets, packetTypes);

        qSurf->busy = false;

        assert(qSurf->inSurf->useCount);
        qSurf->inSurf->useCount -= 1;
    }
    
    return true;
}

void NVENCEncoder::ProcessOutput(NVENCEncoderOutputSurface *surf, List<DataPacket> &packets, List<PacketType> &packetTypes)
{
    List<uint32_t> sliceOffsets;
    sliceOffsets.SetSize(encodeConfig.encodeCodecConfig.h264Config.sliceModeData);
    
    NV_ENC_LOCK_BITSTREAM lockParams = { 0 };
    lockParams.version = NV_ENC_LOCK_BITSTREAM_VER;

    encodeData.Clear();

    lockParams.doNotWait = 0;
    lockParams.outputBitstream = surf->outputSurface;
    lockParams.sliceOffsets = sliceOffsets.Array();

    NVENCSTATUS nvStatus = pNvEnc->nvEncLockBitstream(encoder, &lockParams);
    if (nvStatus != NV_ENC_SUCCESS)
    {
        NvLog(TEXT("Failed locking bitstream"));
        return;
    }

    memcpy(pstart, lockParams.bitstreamBufferPtr, lockParams.bitstreamSizeInBytes);

    uint8_t *start = pstart;
    uint8_t *end = start + lockParams.bitstreamSizeInBytes;
    const static uint8_t start_seq[] = { 0, 0, 1 };
    start = std::search(start, end, start_seq, start_seq + 3);

    List<x264_nal_t> nalOut;
    while (start != end)
    {
        decltype(start) next = std::search(start + 1, end, start_seq, start_seq + 3);
                
        x264_nal_t nal;

        nal.i_ref_idc = (start[3] >> 5) & 3;
        nal.i_type = start[3] & 0x1f;

        nal.p_payload = start;
        nal.i_payload = int(next - start);
        nalOut << nal;
        start = next;
    }
    size_t nalNum = nalOut.Num();

    uint64_t dts = surf->timestamp;
    uint64_t out_pts = lockParams.outputTimeStamp;

    int32_t timeOffset = int(out_pts - dts);
    timeOffset += frameShift;

    if (nalNum && timeOffset < 0)
    {
        frameShift -= timeOffset;
        timeOffset = 0;
    }

    timeOffset = htonl(timeOffset);
    BYTE *timeOffsetAddr = ((BYTE*)&timeOffset) + 1;

    PacketType bestType = PacketType_VideoDisposable;
    bool bFoundFrame = false;

    for (unsigned int i = 0; i < nalNum; i++)
    {
        x264_nal_t &nal = nalOut[i];

        if (nal.i_type == NAL_SEI)
        {
            BYTE *end = nal.p_payload + nal.i_payload;
            BYTE *skip = nal.p_payload;
            while (*(skip++) != 0x1);
            int skipBytes = (int)(skip - nal.p_payload);

            int newPayloadSize = (nal.i_payload - skipBytes);
            BYTE *sei_start = skip + 1;
            while (sei_start < end)
            {
                BYTE *sei = sei_start;
                int sei_type = 0;
                while (*sei == 0xff)
                {
                    sei_type += 0xff;
                    sei += 1;
                }
                sei_type += *sei++;

                int payload_size = 0;
                while (*sei == 0xff)
                {
                    payload_size += 0xff;
                    sei += 1;
                }
                payload_size += *sei++;

                const static BYTE emulation_prevention_pattern[] = { 0, 0, 3 };
                BYTE *search = sei;
                for (BYTE *search = sei;;)
                {
                    search = std::search(search, sei + payload_size, emulation_prevention_pattern, emulation_prevention_pattern + 3);
                    if (search == sei + payload_size)
                        break;
                    payload_size += 1;
                    search += 3;
                }

                int sei_size = (int)(sei - sei_start) + payload_size;
                sei_start[-1] = NAL_SEI;

                if (sei_type == 0x5 /* SEI_USER_DATA_UNREGISTERED */)
                {
                    seiData.Clear();
                    BufferOutputSerializer packetOut(seiData);

                    packetOut.OutputDword(htonl(sei_size + 2));
                    packetOut.Serialize(sei_start - 1, sei_size + 1);
                    packetOut.OutputByte(0x80);
                }
                else
                {
                    BufferOutputSerializer packetOut(encodeData);

                    packetOut.OutputDword(htonl(sei_size + 2));
                    packetOut.Serialize(sei_start - 1, sei_size + 1);
                    packetOut.OutputByte(0x80);
                }
                sei_start += sei_size;

                if (*sei_start == 0x80 && std::find_if_not(sei_start + 1, end, [](uint8_t val) { return val == 0; }) == end) //find rbsp_trailing_bits
                    break;
            }
        }
        else if (nal.i_type == NAL_AUD || nal.i_type == NAL_FILLER)
        {
            BYTE *skip = nal.p_payload;
            while (*(skip++) != 0x1);
            int skipBytes = (int)(skip - nal.p_payload);

            int newPayloadSize = (nal.i_payload - skipBytes);

            BufferOutputSerializer packetOut(encodeData);

            packetOut.OutputDword(htonl(newPayloadSize));
            packetOut.Serialize(nal.p_payload + skipBytes, newPayloadSize);
        }
        else if (nal.i_type == NAL_SLICE_IDR || nal.i_type == NAL_SLICE)
        {
            BYTE *skip = nal.p_payload;
            while (*(skip++) != 0x1);
            int skipBytes = (int)(skip - nal.p_payload);

            if (!bFoundFrame)
            {
                encodeData.Insert(0, (nal.i_type == NAL_SLICE_IDR) ? 0x17 : 0x27);
                encodeData.Insert(1, 1);
                encodeData.InsertArray(2, timeOffsetAddr, 3);

                bFoundFrame = true;
            }

            int newPayloadSize = (nal.i_payload - skipBytes);
            BufferOutputSerializer packetOut(encodeData);

            packetOut.OutputDword(htonl(newPayloadSize));
            packetOut.Serialize(nal.p_payload + skipBytes, newPayloadSize);

            switch (nal.i_ref_idc)
            {
            case NAL_PRIORITY_DISPOSABLE:   bestType = MAX(bestType, PacketType_VideoDisposable);  break;
            case NAL_PRIORITY_LOW:          bestType = MAX(bestType, PacketType_VideoLow);         break;
            case NAL_PRIORITY_HIGH:         bestType = MAX(bestType, PacketType_VideoHigh);        break;
            case NAL_PRIORITY_HIGHEST:      bestType = MAX(bestType, PacketType_VideoHighest);     break;
            }
        }
        else
            continue;
    }

    DataPacket packet;
    packet.lpPacket = encodeData.Array();
    packet.size = encodeData.Num();

    packetTypes << bestType;
    packets << packet;

    nvStatus = pNvEnc->nvEncUnlockBitstream(encoder, surf->outputSurface);
    if (nvStatus != NV_ENC_SUCCESS)
    {
        NvLog(TEXT("Failed unlocking bitstream"));
        return;
    }
}

void NVENCEncoder::GetHeaders(DataPacket &packet)
{
    uint32_t outSize = 0;

    char tmpHeader[128];

    NV_ENC_SEQUENCE_PARAM_PAYLOAD payload = { 0 };
    payload.version = NV_ENC_SEQUENCE_PARAM_PAYLOAD_VER;

    payload.spsppsBuffer = tmpHeader;
    payload.inBufferSize = 128;
    payload.outSPSPPSPayloadSize = &outSize;

    NVENCSTATUS nvStatus = pNvEnc->nvEncGetSequenceParams(encoder, &payload);
    if (nvStatus != NV_ENC_SUCCESS)
    {
        NvLog(TEXT("nvEncGetSequenceParams failed with 0x%x"), nvStatus);
        packet.lpPacket = 0;
        packet.size = 0;
        return;
    }

    char *sps = tmpHeader;
    unsigned int i_sps = 4;
    
    while (tmpHeader[i_sps    ] != 0x00
        || tmpHeader[i_sps + 1] != 0x00
        || tmpHeader[i_sps + 2] != 0x00
        || tmpHeader[i_sps + 3] != 0x01)
    {
        i_sps += 1;
        if (i_sps >= outSize)
        {
            NvLog(TEXT("Invalid SPS/PPS"));
            return;
        }
    }

    char *pps = tmpHeader + i_sps;
    unsigned int i_pps = outSize - i_sps;

    headerPacket.Clear();
    BufferOutputSerializer headerOut(headerPacket);

    headerOut.OutputByte(0x17);
    headerOut.OutputByte(0);
    headerOut.OutputByte(0);
    headerOut.OutputByte(0);
    headerOut.OutputByte(0);
    headerOut.OutputByte(1);
    headerOut.Serialize(sps + 5, 3);
    headerOut.OutputByte(0xff);
    headerOut.OutputByte(0xe1);
    headerOut.OutputWord(htons(i_sps - 4));
    headerOut.Serialize(sps + 4, i_sps - 4);

    headerOut.OutputByte(1);
    headerOut.OutputWord(htons(i_pps - 4));
    headerOut.Serialize(pps + 4, i_pps - 4);
    
    packet.size = headerPacket.Num();
    packet.lpPacket = headerPacket.Array();
}

void NVENCEncoder::GetSEI(DataPacket &packet)
{
    packet.lpPacket = seiData.Array() ? seiData.Array() : (LPBYTE)1;
    packet.size = seiData.Num();
}

void NVENCEncoder::RequestKeyframe()
{
    OSMutexLocker locker(frameMutex);

    NV_ENC_RECONFIGURE_PARAMS params = { 0 };
    params.version = NV_ENC_RECONFIGURE_PARAMS_VER;

    params.forceIDR = 1;
    mcpy(&params.reInitEncodeParams, &initEncodeParams, sizeof(NV_ENC_INITIALIZE_PARAMS));

    NVENCSTATUS nvStatus = pNvEnc->nvEncReconfigureEncoder(encoder, &params);
    if (nvStatus != NV_ENC_SUCCESS)
    {
        NvLog(TEXT("Failed requesting a Keyframe"));
    }
}

bool NVENCEncoder::SetBitRate(DWORD maxBitrate, DWORD bufferSize)
{
    OSMutexLocker locker(frameMutex);

    NV_ENC_RECONFIGURE_PARAMS params = { 0 };
    params.version = NV_ENC_RECONFIGURE_PARAMS_VER;

    if (maxBitrate > 0)
    {
        encodeConfig.rcParams.maxBitRate = maxBitrate * 1000;
        encodeConfig.rcParams.averageBitRate = maxBitrate * 1000;
    }

    if (bufferSize > 0)
    {
        encodeConfig.rcParams.vbvBufferSize = bufferSize * 1000;
    }

    params.resetEncoder = 1;
    params.forceIDR = 1;
    mcpy(&params.reInitEncodeParams, &initEncodeParams, sizeof(NV_ENC_INITIALIZE_PARAMS));

    NVENCSTATUS nvStatus = pNvEnc->nvEncReconfigureEncoder(encoder, &params);
    if (nvStatus != NV_ENC_SUCCESS)
    {
        NvLog(TEXT("Failed reconfiguring nvenc encoder"));
        return false;
    }

    if (maxBitrate > 0)
        this->maxBitRate = maxBitrate;

    if (bufferSize > 0)
        this->bufferSize = bufferSize;

    return true;
}

int NVENCEncoder::GetBitRate() const
{
    return maxBitRate;
}

bool NVENCEncoder::DynamicBitrateSupported() const
{
    return true;
}

String NVENCEncoder::GetInfoString() const
{
    String strInfo;

    String preset = "unknown";
    if (dataEqual(initEncodeParams.presetGUID, NV_ENC_PRESET_HP_GUID))
        preset = "hp";
    else if (dataEqual(initEncodeParams.presetGUID, NV_ENC_PRESET_HQ_GUID))
        preset = "hq";
    else if (dataEqual(initEncodeParams.presetGUID, NV_ENC_PRESET_BD_GUID))
        preset = "bd";
    else if (dataEqual(initEncodeParams.presetGUID, NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID))
        preset = "ll";
    else if (dataEqual(initEncodeParams.presetGUID, NV_ENC_PRESET_LOW_LATENCY_HP_GUID))
        preset = "llhp";
    else if (dataEqual(initEncodeParams.presetGUID, NV_ENC_PRESET_LOW_LATENCY_HQ_GUID))
        preset = "llhq";
    else if (dataEqual(initEncodeParams.presetGUID, NV_ENC_PRESET_DEFAULT_GUID))
        preset = "default";

    String profile = "unknown";
    if (dataEqual(encodeConfig.profileGUID, NV_ENC_CODEC_PROFILE_AUTOSELECT_GUID))
        profile = "autoselect";
    else if (dataEqual(encodeConfig.profileGUID, NV_ENC_H264_PROFILE_BASELINE_GUID))
        profile = "baseline";
    else if (dataEqual(encodeConfig.profileGUID, NV_ENC_H264_PROFILE_MAIN_GUID))
        profile = "main";
    else if (dataEqual(encodeConfig.profileGUID, NV_ENC_H264_PROFILE_HIGH_GUID))
        profile = "high";
    else if (dataEqual(encodeConfig.profileGUID, NV_ENC_H264_PROFILE_STEREO_GUID))
        profile = "stereo";
    else if (dataEqual(encodeConfig.profileGUID, NV_ENC_H264_PROFILE_SVC_TEMPORAL_SCALABILTY))
        profile = "svc temporal";

    String cbr = "no";
    if (encodeConfig.rcParams.rateControlMode == NV_ENC_PARAMS_RC_CBR
        || encodeConfig.rcParams.rateControlMode == NV_ENC_PARAMS_RC_CBR2)
    {
        cbr = "yes";
    }

    String cfr = "yes";
    if (encodeConfig.encodeCodecConfig.h264Config.enableVFR)
        cfr = "no";

    String level = "unknown";
    if (encodeConfig.encodeCodecConfig.h264Config.level == 0)
        level = "autoselect";
    else if (encodeConfig.encodeCodecConfig.h264Config.level <= 51)
    {
        level = IntString(encodeConfig.encodeCodecConfig.h264Config.level / 10);
        level << ".";
        level << IntString(encodeConfig.encodeCodecConfig.h264Config.level % 10);
    }

    strInfo << TEXT("Video Encoding: NVENC") <<
        TEXT("\r\n    fps: ") << IntString(initEncodeParams.frameRateNum / initEncodeParams.frameRateDen) <<
        TEXT("\r\n    width: ") << IntString(initEncodeParams.encodeWidth) << TEXT(", height: ") << IntString(initEncodeParams.encodeHeight) <<
        TEXT("\r\n    preset: ") << preset <<
        TEXT("\r\n    profile: ") << profile <<
        TEXT("\r\n    level: ") << level <<
        TEXT("\r\n    keyint: ") << IntString(encodeConfig.gopLength) <<
        TEXT("\r\n    CBR: ") << cbr <<
        TEXT("\r\n    CFR: ") << cfr <<
        TEXT("\r\n    max bitrate: ") << IntString(encodeConfig.rcParams.maxBitRate / 1000) <<
        TEXT("\r\n    buffer size: ") << IntString(encodeConfig.rcParams.vbvBufferSize / 1000);

    return strInfo;
}

bool NVENCEncoder::isQSV()
{
    return true;
}

int NVENCEncoder::GetBufferedFrames()
{
    return (int)(outputSurfaceQueue.size() + outputSurfaceQueueReady.size());
}

bool NVENCEncoder::HasBufferedFrames()
{
    return !(outputSurfaceQueue.empty() && outputSurfaceQueueReady.empty());
}

void NVENCEncoder::dumpEncodeConfig()
{
    String dumpFilePath = API->GetAppDataPath();
    dumpFilePath << TEXT("\\last_nvenc_config.xconfig");

    XConfig dumpConfig(dumpFilePath.Array());

    if (!dumpConfig.IsOpen())
    {
        NvLog(TEXT("Failed opening %s"), dumpFilePath.Array());
        return;
    }

    dumpConfig.GetRootElement()->RemoveElement(TEXT("initEncodeParams"));

    XElement *iepElement = dumpConfig.GetRootElement()->CreateElement(TEXT("initEncodeParams"));
    iepElement->AddString(TEXT("encodeGUID"), guidToString(initEncodeParams.encodeGUID));
    iepElement->AddString(TEXT("presetGUID"), guidToString(initEncodeParams.presetGUID));
    iepElement->AddInt(TEXT("encodeWidth"), initEncodeParams.encodeWidth);
    iepElement->AddInt(TEXT("encodeHeight"), initEncodeParams.encodeHeight);
    iepElement->AddInt(TEXT("darWidth"), initEncodeParams.darWidth);
    iepElement->AddInt(TEXT("darHeight"), initEncodeParams.darHeight);
    iepElement->AddInt(TEXT("frameRateNum"), initEncodeParams.frameRateNum);
    iepElement->AddInt(TEXT("frameRateDen"), initEncodeParams.frameRateDen);
    iepElement->AddInt(TEXT("enableEncodeAsync"), initEncodeParams.enableEncodeAsync);
    iepElement->AddInt(TEXT("enablePTD"), initEncodeParams.enablePTD);
    iepElement->AddInt(TEXT("reportSliceOffsets"), initEncodeParams.reportSliceOffsets);
    iepElement->AddInt(TEXT("enableSubFrameWrite"), initEncodeParams.enableSubFrameWrite);
    iepElement->AddInt(TEXT("enableExternalMEHints"), initEncodeParams.enableExternalMEHints);
    iepElement->AddInt(TEXT("maxEncodeWidth"), initEncodeParams.maxEncodeWidth);
    iepElement->AddInt(TEXT("maxEncodeHeight"), initEncodeParams.maxEncodeHeight);

    XElement *mehint1Element = iepElement->CreateElement(TEXT("maxMEHintCountsPerBlock1"));
    mehint1Element->AddInt(TEXT("numCandsPerBlk16x16"), initEncodeParams.maxMEHintCountsPerBlock[0].numCandsPerBlk16x16);
    mehint1Element->AddInt(TEXT("numCandsPerBlk16x8"), initEncodeParams.maxMEHintCountsPerBlock[0].numCandsPerBlk16x8);
    mehint1Element->AddInt(TEXT("numCandsPerBlk8x16"), initEncodeParams.maxMEHintCountsPerBlock[0].numCandsPerBlk8x16);
    mehint1Element->AddInt(TEXT("numCandsPerBlk8x8"), initEncodeParams.maxMEHintCountsPerBlock[0].numCandsPerBlk8x8);

    XElement *mehint2Element = iepElement->CreateElement(TEXT("maxMEHintCountsPerBlock2"));
    mehint2Element->AddInt(TEXT("numCandsPerBlk16x16"), initEncodeParams.maxMEHintCountsPerBlock[1].numCandsPerBlk16x16);
    mehint2Element->AddInt(TEXT("numCandsPerBlk16x8"), initEncodeParams.maxMEHintCountsPerBlock[1].numCandsPerBlk16x8);
    mehint2Element->AddInt(TEXT("numCandsPerBlk8x16"), initEncodeParams.maxMEHintCountsPerBlock[1].numCandsPerBlk8x16);
    mehint2Element->AddInt(TEXT("numCandsPerBlk8x8"), initEncodeParams.maxMEHintCountsPerBlock[1].numCandsPerBlk8x8);

    XElement *ecElement = iepElement->CreateElement(TEXT("encodeConfig"));
    ecElement->AddString(TEXT("profileGUID"), guidToString(encodeConfig.profileGUID));
    ecElement->AddInt(TEXT("gopLength"), encodeConfig.gopLength);
    ecElement->AddInt(TEXT("frameIntervalP"), encodeConfig.frameIntervalP);
    ecElement->AddInt(TEXT("monoChromeEncoding"), encodeConfig.monoChromeEncoding);
    ecElement->AddInt(TEXT("frameFieldMode"), encodeConfig.frameFieldMode);
    ecElement->AddInt(TEXT("mvPrecision"), encodeConfig.mvPrecision);
    
    XElement *rcElement = ecElement->CreateElement(TEXT("rcParams"));
    rcElement->AddInt(TEXT("rateControlMode"), encodeConfig.rcParams.rateControlMode);
    rcElement->AddInt(TEXT("constQP_interB"), encodeConfig.rcParams.constQP.qpInterB);
    rcElement->AddInt(TEXT("constQP_interP"), encodeConfig.rcParams.constQP.qpInterP);
    rcElement->AddInt(TEXT("constQP_intra"), encodeConfig.rcParams.constQP.qpIntra);
    rcElement->AddInt(TEXT("averageBitRate"), encodeConfig.rcParams.averageBitRate);
    rcElement->AddInt(TEXT("maxBitRate"), encodeConfig.rcParams.maxBitRate);
    rcElement->AddInt(TEXT("vbvBufferSize"), encodeConfig.rcParams.vbvBufferSize);
    rcElement->AddInt(TEXT("vbvInitialDelay"), encodeConfig.rcParams.vbvInitialDelay);
    rcElement->AddInt(TEXT("enableMinQP"), encodeConfig.rcParams.enableMinQP);
    rcElement->AddInt(TEXT("enableMaxQP"), encodeConfig.rcParams.enableMaxQP);
    rcElement->AddInt(TEXT("enableInitialRCQP"), encodeConfig.rcParams.enableInitialRCQP);
    rcElement->AddInt(TEXT("minQP_interB"), encodeConfig.rcParams.minQP.qpInterB);
    rcElement->AddInt(TEXT("minQP_interP"), encodeConfig.rcParams.minQP.qpInterP);
    rcElement->AddInt(TEXT("minQP_intra"), encodeConfig.rcParams.minQP.qpIntra);
    rcElement->AddInt(TEXT("maxQP_interB"), encodeConfig.rcParams.maxQP.qpInterB);
    rcElement->AddInt(TEXT("maxQP_interP"), encodeConfig.rcParams.maxQP.qpInterP);
    rcElement->AddInt(TEXT("maxQP_intra"), encodeConfig.rcParams.maxQP.qpIntra);
    rcElement->AddInt(TEXT("initialRCQP_interB"), encodeConfig.rcParams.initialRCQP.qpInterB);
    rcElement->AddInt(TEXT("initialRCQP_interP"), encodeConfig.rcParams.initialRCQP.qpInterP);
    rcElement->AddInt(TEXT("initialRCQP_intra"), encodeConfig.rcParams.initialRCQP.qpIntra);
    rcElement->AddInt(TEXT("temporallayerIdxMask"), encodeConfig.rcParams.temporallayerIdxMask);
    rcElement->AddInt(TEXT("rateControlMode"), encodeConfig.rcParams.rateControlMode);
    rcElement->AddInt(TEXT("temporalLayerQP_0"), encodeConfig.rcParams.temporalLayerQP[0]);
    rcElement->AddInt(TEXT("temporalLayerQP_1"), encodeConfig.rcParams.temporalLayerQP[1]);
    rcElement->AddInt(TEXT("temporalLayerQP_2"), encodeConfig.rcParams.temporalLayerQP[2]);
    rcElement->AddInt(TEXT("temporalLayerQP_3"), encodeConfig.rcParams.temporalLayerQP[3]);
    rcElement->AddInt(TEXT("temporalLayerQP_4"), encodeConfig.rcParams.temporalLayerQP[4]);
    rcElement->AddInt(TEXT("temporalLayerQP_5"), encodeConfig.rcParams.temporalLayerQP[5]);
    rcElement->AddInt(TEXT("temporalLayerQP_6"), encodeConfig.rcParams.temporalLayerQP[6]);
    rcElement->AddInt(TEXT("temporalLayerQP_7"), encodeConfig.rcParams.temporalLayerQP[7]);

    XElement *h264Element = ecElement->CreateElement(TEXT("encodeCodecConfig"));
    h264Element->AddInt(TEXT("enableTemporalSVC"), encodeConfig.encodeCodecConfig.h264Config.enableTemporalSVC);
    h264Element->AddInt(TEXT("enableStereoMVC"), encodeConfig.encodeCodecConfig.h264Config.enableStereoMVC);
    h264Element->AddInt(TEXT("hierarchicalPFrames"), encodeConfig.encodeCodecConfig.h264Config.hierarchicalPFrames);
    h264Element->AddInt(TEXT("hierarchicalBFrames"), encodeConfig.encodeCodecConfig.h264Config.hierarchicalBFrames);
    h264Element->AddInt(TEXT("outputBufferingPeriodSEI"), encodeConfig.encodeCodecConfig.h264Config.outputBufferingPeriodSEI);
    h264Element->AddInt(TEXT("outputPictureTimingSEI"), encodeConfig.encodeCodecConfig.h264Config.outputPictureTimingSEI);
    h264Element->AddInt(TEXT("outputAUD"), encodeConfig.encodeCodecConfig.h264Config.outputAUD);
    h264Element->AddInt(TEXT("disableSPSPPS"), encodeConfig.encodeCodecConfig.h264Config.disableSPSPPS);
    h264Element->AddInt(TEXT("outputFramePackingSEI"), encodeConfig.encodeCodecConfig.h264Config.outputFramePackingSEI);
    h264Element->AddInt(TEXT("outputRecoveryPointSEI"), encodeConfig.encodeCodecConfig.h264Config.outputRecoveryPointSEI);
    h264Element->AddInt(TEXT("enableIntraRefresh"), encodeConfig.encodeCodecConfig.h264Config.enableIntraRefresh);
    h264Element->AddInt(TEXT("enableConstrainedEncoding"), encodeConfig.encodeCodecConfig.h264Config.enableConstrainedEncoding);
    h264Element->AddInt(TEXT("repeatSPSPPS"), encodeConfig.encodeCodecConfig.h264Config.repeatSPSPPS);
    h264Element->AddInt(TEXT("enableVFR"), encodeConfig.encodeCodecConfig.h264Config.enableVFR);
    h264Element->AddInt(TEXT("enableLTR"), encodeConfig.encodeCodecConfig.h264Config.enableLTR);
    h264Element->AddInt(TEXT("reservedBitFields"), encodeConfig.encodeCodecConfig.h264Config.reservedBitFields);
    h264Element->AddInt(TEXT("level"), encodeConfig.encodeCodecConfig.h264Config.level);
    h264Element->AddInt(TEXT("idrPeriod"), encodeConfig.encodeCodecConfig.h264Config.idrPeriod);
    h264Element->AddInt(TEXT("separateColourPlaneFlag"), encodeConfig.encodeCodecConfig.h264Config.separateColourPlaneFlag);
    h264Element->AddInt(TEXT("disableDeblockingFilterIDC"), encodeConfig.encodeCodecConfig.h264Config.disableDeblockingFilterIDC);
    h264Element->AddInt(TEXT("numTemporalLayers"), encodeConfig.encodeCodecConfig.h264Config.numTemporalLayers);
    h264Element->AddInt(TEXT("spsId"), encodeConfig.encodeCodecConfig.h264Config.spsId);
    h264Element->AddInt(TEXT("ppsId"), encodeConfig.encodeCodecConfig.h264Config.ppsId);
    h264Element->AddInt(TEXT("adaptiveTransformMode"), encodeConfig.encodeCodecConfig.h264Config.adaptiveTransformMode);
    h264Element->AddInt(TEXT("fmoMode"), encodeConfig.encodeCodecConfig.h264Config.fmoMode);
    h264Element->AddInt(TEXT("bdirectMode"), encodeConfig.encodeCodecConfig.h264Config.bdirectMode);
    h264Element->AddInt(TEXT("entropyCodingMode"), encodeConfig.encodeCodecConfig.h264Config.entropyCodingMode);
    h264Element->AddInt(TEXT("stereoMode"), encodeConfig.encodeCodecConfig.h264Config.stereoMode);
    h264Element->AddInt(TEXT("intraRefreshPeriod"), encodeConfig.encodeCodecConfig.h264Config.intraRefreshPeriod);
    h264Element->AddInt(TEXT("intraRefreshCnt"), encodeConfig.encodeCodecConfig.h264Config.intraRefreshCnt);
    h264Element->AddInt(TEXT("maxNumRefFrames"), encodeConfig.encodeCodecConfig.h264Config.maxNumRefFrames);
    h264Element->AddInt(TEXT("sliceMode"), encodeConfig.encodeCodecConfig.h264Config.sliceMode);
    h264Element->AddInt(TEXT("sliceModeData"), encodeConfig.encodeCodecConfig.h264Config.sliceModeData);
    h264Element->AddInt(TEXT("ltrNumFrames"), encodeConfig.encodeCodecConfig.h264Config.ltrNumFrames);
    h264Element->AddInt(TEXT("ltrTrustMode"), encodeConfig.encodeCodecConfig.h264Config.ltrTrustMode);

    XElement *extElem = h264Element->CreateElement(TEXT("h264Extension"));

    XElement *svcElement = extElem->CreateElement(TEXT("svcTemporalConfig"));
    svcElement->AddInt(TEXT("numTemporalLayers"), encodeConfig.encodeCodecConfig.h264Config.h264Extension.svcTemporalConfig.numTemporalLayers);
    svcElement->AddInt(TEXT("basePriorityID"), encodeConfig.encodeCodecConfig.h264Config.h264Extension.svcTemporalConfig.basePriorityID);

    XElement *mvcElement = extElem->CreateElement(TEXT("mvcConfig"));

    XElement *vuiElement = h264Element->CreateElement(TEXT("h264VUIParameters"));
    vuiElement->AddInt(TEXT("overscanInfoPresentFlag"), encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.overscanInfoPresentFlag);
    vuiElement->AddInt(TEXT("overscanInfo"), encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.overscanInfo);
    vuiElement->AddInt(TEXT("videoSignalTypePresentFlag"), encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.videoSignalTypePresentFlag);
    vuiElement->AddInt(TEXT("videoFormat"), encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.videoFormat);
    vuiElement->AddInt(TEXT("videoFullRangeFlag"), encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.videoFullRangeFlag);
    vuiElement->AddInt(TEXT("colourDescriptionPresentFlag"), encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.colourDescriptionPresentFlag);
    vuiElement->AddInt(TEXT("colourPrimaries"), encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.colourPrimaries);
    vuiElement->AddInt(TEXT("transferCharacteristics"), encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.transferCharacteristics);
    vuiElement->AddInt(TEXT("colourMatrix"), encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.colourMatrix);
    vuiElement->AddInt(TEXT("chromaSampleLocationFlag"), encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.chromaSampleLocationFlag);
    vuiElement->AddInt(TEXT("chromaSampleLocationTop"), encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.chromaSampleLocationTop);
    vuiElement->AddInt(TEXT("chromaSampleLocationBot"), encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.chromaSampleLocationBot);
    vuiElement->AddInt(TEXT("bitstreamRestrictionFlag"), encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.bitstreamRestrictionFlag);

    dumpConfig.Close(true);
}

#define CHECK_GET_INT(element, target, name) \
    if (element->HasItem(TEXT(#name))) \
        target.name = (decltype(target.name))element->GetInt(TEXT(#name))

#define CHECK_GET_INT2(element, target, name) \
    if (element->HasItem(TEXT(#name))) \
        target = element->GetInt(TEXT(#name))

void NVENCEncoder::tryParseEncodeConfig()
{
    String dumpFilePath = API->GetAppDataPath();
    dumpFilePath << TEXT("\\nvenc_config.xconfig");

    XConfig dumpConfig(dumpFilePath.Array());

    if (!dumpConfig.IsOpen())
    {
        NvLog(TEXT("Failed opening %s"), dumpFilePath.Array());
        return;
    }

    XElement *iepElement = dumpConfig.GetElement(TEXT("initEncodeParams"));
    if (!iepElement)
        return;

    if (iepElement->HasItem(TEXT("encodeGUID")))
        stringToGuid(iepElement->GetString(TEXT("encodeGUID")), &initEncodeParams.encodeGUID);
    if (iepElement->HasItem(TEXT("presetGUID")))
        stringToGuid(iepElement->GetString(TEXT("presetGUID")), &initEncodeParams.presetGUID);

    CHECK_GET_INT(iepElement, initEncodeParams, encodeWidth);
    CHECK_GET_INT(iepElement, initEncodeParams, encodeHeight);
    CHECK_GET_INT(iepElement, initEncodeParams, darWidth);
    CHECK_GET_INT(iepElement, initEncodeParams, darHeight);
    CHECK_GET_INT(iepElement, initEncodeParams, frameRateNum);
    CHECK_GET_INT(iepElement, initEncodeParams, frameRateDen);
    //CHECK_GET_INT(iepElement, initEncodeParams, enableEncodeAsync); // Overriding this would horribly break everything
    //CHECK_GET_INT(iepElement, initEncodeParams, enablePTD); // Same as above
    CHECK_GET_INT(iepElement, initEncodeParams, reportSliceOffsets);
    CHECK_GET_INT(iepElement, initEncodeParams, enableSubFrameWrite);
    CHECK_GET_INT(iepElement, initEncodeParams, enableExternalMEHints);
    CHECK_GET_INT(iepElement, initEncodeParams, maxEncodeWidth);
    CHECK_GET_INT(iepElement, initEncodeParams, maxEncodeHeight);
    
    XElement *mehint1Element = iepElement->GetElement(TEXT("maxMEHintCountsPerBlock1"));
    if (mehint1Element)
    {
        CHECK_GET_INT(mehint1Element, initEncodeParams.maxMEHintCountsPerBlock[0], numCandsPerBlk16x16);
        CHECK_GET_INT(mehint1Element, initEncodeParams.maxMEHintCountsPerBlock[0], numCandsPerBlk16x8);
        CHECK_GET_INT(mehint1Element, initEncodeParams.maxMEHintCountsPerBlock[0], numCandsPerBlk8x16);
        CHECK_GET_INT(mehint1Element, initEncodeParams.maxMEHintCountsPerBlock[0], numCandsPerBlk8x8);
    }

    XElement *mehint2Element = iepElement->GetElement(TEXT("maxMEHintCountsPerBlock2"));
    if (mehint2Element)
    {
        CHECK_GET_INT(mehint2Element, initEncodeParams.maxMEHintCountsPerBlock[1], numCandsPerBlk16x16);
        CHECK_GET_INT(mehint2Element, initEncodeParams.maxMEHintCountsPerBlock[1], numCandsPerBlk16x8);
        CHECK_GET_INT(mehint2Element, initEncodeParams.maxMEHintCountsPerBlock[1], numCandsPerBlk8x16);
        CHECK_GET_INT(mehint2Element, initEncodeParams.maxMEHintCountsPerBlock[1], numCandsPerBlk8x8);
    }

    XElement *ecElement = iepElement->GetElement(TEXT("encodeConfig"));
    if (!ecElement)
        return;

    if (ecElement->HasItem(TEXT("profileGUID")))
        stringToGuid(ecElement->GetString(TEXT("profileGUID")), &encodeConfig.profileGUID);

    CHECK_GET_INT(ecElement, encodeConfig, gopLength);
    CHECK_GET_INT(ecElement, encodeConfig, frameIntervalP);
    CHECK_GET_INT(ecElement, encodeConfig, monoChromeEncoding);
    CHECK_GET_INT(ecElement, encodeConfig, frameFieldMode);
    CHECK_GET_INT(ecElement, encodeConfig, mvPrecision);

    XElement *rcElement = ecElement->GetElement(TEXT("rcParams"));
    if (rcElement)
    {
        CHECK_GET_INT(rcElement, encodeConfig.rcParams, rateControlMode);
        CHECK_GET_INT2(rcElement, encodeConfig.rcParams.constQP.qpInterB, constQP_interB);
        CHECK_GET_INT2(rcElement, encodeConfig.rcParams.constQP.qpInterP, constQP_interP);
        CHECK_GET_INT2(rcElement, encodeConfig.rcParams.constQP.qpIntra, constQP_intra);
        CHECK_GET_INT(rcElement, encodeConfig.rcParams, averageBitRate);
        CHECK_GET_INT(rcElement, encodeConfig.rcParams, maxBitRate);
        CHECK_GET_INT(rcElement, encodeConfig.rcParams, vbvBufferSize);
        CHECK_GET_INT(rcElement, encodeConfig.rcParams, vbvInitialDelay);
        CHECK_GET_INT(rcElement, encodeConfig.rcParams, enableMinQP);
        CHECK_GET_INT(rcElement, encodeConfig.rcParams, enableMaxQP);
        CHECK_GET_INT(rcElement, encodeConfig.rcParams, enableInitialRCQP);
        CHECK_GET_INT2(rcElement, encodeConfig.rcParams.minQP.qpInterB, minQP_interB);
        CHECK_GET_INT2(rcElement, encodeConfig.rcParams.minQP.qpInterP, minQP_interP);
        CHECK_GET_INT2(rcElement, encodeConfig.rcParams.minQP.qpIntra, minQP_intra);
        CHECK_GET_INT2(rcElement, encodeConfig.rcParams.maxQP.qpInterB, maxQP_interB);
        CHECK_GET_INT2(rcElement, encodeConfig.rcParams.maxQP.qpInterP, maxQP_interP);
        CHECK_GET_INT2(rcElement, encodeConfig.rcParams.maxQP.qpIntra, maxQP_intra);
        CHECK_GET_INT2(rcElement, encodeConfig.rcParams.initialRCQP.qpInterB, initialRCQP_interB);
        CHECK_GET_INT2(rcElement, encodeConfig.rcParams.initialRCQP.qpInterP, initialRCQP_interP);
        CHECK_GET_INT2(rcElement, encodeConfig.rcParams.initialRCQP.qpIntra, initialRCQP_intra);
        CHECK_GET_INT(rcElement, encodeConfig.rcParams, temporallayerIdxMask);
        CHECK_GET_INT(rcElement, encodeConfig.rcParams, rateControlMode);
        CHECK_GET_INT2(rcElement, encodeConfig.rcParams.temporalLayerQP[0], temporalLayerQP_0);
        CHECK_GET_INT2(rcElement, encodeConfig.rcParams.temporalLayerQP[1], temporalLayerQP_1);
        CHECK_GET_INT2(rcElement, encodeConfig.rcParams.temporalLayerQP[2], temporalLayerQP_2);
        CHECK_GET_INT2(rcElement, encodeConfig.rcParams.temporalLayerQP[3], temporalLayerQP_3);
        CHECK_GET_INT2(rcElement, encodeConfig.rcParams.temporalLayerQP[4], temporalLayerQP_4);
        CHECK_GET_INT2(rcElement, encodeConfig.rcParams.temporalLayerQP[5], temporalLayerQP_5);
        CHECK_GET_INT2(rcElement, encodeConfig.rcParams.temporalLayerQP[6], temporalLayerQP_6);
        CHECK_GET_INT2(rcElement, encodeConfig.rcParams.temporalLayerQP[7], temporalLayerQP_7);
    }

    XElement *h264Element = ecElement->GetElement(TEXT("encodeCodecConfig"));
    if (!h264Element)
        return;

    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, enableTemporalSVC);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, enableStereoMVC);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, hierarchicalPFrames);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, hierarchicalBFrames);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, outputBufferingPeriodSEI);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, outputPictureTimingSEI);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, outputAUD);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, disableSPSPPS);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, outputFramePackingSEI);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, outputRecoveryPointSEI);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, enableIntraRefresh);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, enableConstrainedEncoding);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, repeatSPSPPS);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, enableVFR);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, enableLTR);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, reservedBitFields);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, level);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, idrPeriod);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, separateColourPlaneFlag);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, disableDeblockingFilterIDC);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, numTemporalLayers);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, spsId);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, ppsId);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, adaptiveTransformMode);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, fmoMode);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, bdirectMode);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, entropyCodingMode);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, stereoMode);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, intraRefreshPeriod);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, intraRefreshCnt);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, maxNumRefFrames);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, sliceMode);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, sliceModeData);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, ltrNumFrames);
    CHECK_GET_INT(h264Element, encodeConfig.encodeCodecConfig.h264Config, ltrTrustMode);

    XElement *vuiElement = h264Element->GetElement(TEXT("h264VUIParameters"));
    if (vuiElement)
    {
        CHECK_GET_INT(vuiElement, encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters, overscanInfoPresentFlag);
        CHECK_GET_INT(vuiElement, encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters, overscanInfo);
        CHECK_GET_INT(vuiElement, encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters, videoSignalTypePresentFlag);
        CHECK_GET_INT(vuiElement, encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters, videoFormat);
        CHECK_GET_INT(vuiElement, encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters, videoFullRangeFlag);
        CHECK_GET_INT(vuiElement, encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters, colourDescriptionPresentFlag);
        CHECK_GET_INT(vuiElement, encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters, colourPrimaries);
        CHECK_GET_INT(vuiElement, encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters, transferCharacteristics);
        CHECK_GET_INT(vuiElement, encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters, colourMatrix);
        CHECK_GET_INT(vuiElement, encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters, chromaSampleLocationFlag);
        CHECK_GET_INT(vuiElement, encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters, chromaSampleLocationTop);
        CHECK_GET_INT(vuiElement, encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters, chromaSampleLocationBot);
        CHECK_GET_INT(vuiElement, encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters, bitstreamRestrictionFlag);
    }

    XElement *extElem = h264Element->GetElement(TEXT("h264Extension"));
    if (!extElem)
        return;

    XElement *svcElement = extElem->GetElement(TEXT("svcTemporalConfig"));
    if (!svcElement)
        return;

    CHECK_GET_INT(svcElement, encodeConfig.encodeCodecConfig.h264Config.h264Extension.svcTemporalConfig, numTemporalLayers);
    CHECK_GET_INT(svcElement, encodeConfig.encodeCodecConfig.h264Config.h264Extension.svcTemporalConfig, basePriorityID);
}
