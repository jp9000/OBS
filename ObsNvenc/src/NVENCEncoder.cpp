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

    int useCustomPreset = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseCustomSettings"));
    if (useCustomPreset)
    {
        String presetString = AppConfig->GetString(TEXT("Video Encoding"), TEXT("CustomSettings"), TEXT("default")).MakeLower();

        if (presetString == TEXT("hp"))
            encoderPreset = NV_ENC_PRESET_HP_GUID;
        else if (presetString == TEXT("hq"))
            encoderPreset = NV_ENC_PRESET_HQ_GUID;
        else if (presetString == TEXT("bd"))
            encoderPreset = NV_ENC_PRESET_BD_GUID;
        else if (presetString == TEXT("ll"))
            encoderPreset = NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID;
        else if (presetString == TEXT("llhp"))
            encoderPreset = NV_ENC_PRESET_LOW_LATENCY_HP_GUID;
        else if (presetString == TEXT("llhq"))
            encoderPreset = NV_ENC_PRESET_LOW_LATENCY_HQ_GUID;
        else
            encoderPreset = NV_ENC_PRESET_DEFAULT_GUID;
    }

    TCHAR envClientKey[128] = { 0 };
    DWORD envRes = GetEnvironmentVariable(TEXT("NVENC_KEY"), envClientKey, 128);
    if (envRes > 0 && envRes <= 128)
    {
        if (CLSIDFromString(envClientKey, &clientKey) == NOERROR)
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

    encodeConfig.encodeCodecConfig.h264Config.bdirectMode = encodeConfig.frameIntervalP > 1 ? NV_ENC_H264_BDIRECT_MODE_TEMPORAL : NV_ENC_H264_BDIRECT_MODE_DISABLE;

    encodeConfig.encodeCodecConfig.h264Config.idrPeriod = encodeConfig.gopLength;
    encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.colourMatrix = colorDesc.matrix;
    encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.videoFullRangeFlag = colorDesc.fullRange;
    encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.colourPrimaries = colorDesc.primaries;
    encodeConfig.encodeCodecConfig.h264Config.h264VUIParameters.transferCharacteristics = colorDesc.transfer;

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
