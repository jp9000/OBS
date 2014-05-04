/********************************************************************************
Copyright (C) 2014 jackun <jackun@gmail.com>

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

//http://www.slideshare.net/DevCentralAMD/mm-4094-khaledmammou

#include "stdafx.h"
#include "ObsVCE.h"
#include <../libmfx/include/msdk/include/mfxstructures.h>

extern "C"
{
#define _STDINT_H
#include <../x264/x264.h>
#undef _STDINT_H
}

ConfigFile **VCEAppConfig = 0;
unsigned int encoderRefs = 0;

extern "C" __declspec(dllexport) bool __cdecl InitVCEEncoder(ConfigFile **appConfig, VideoEncoder *enc)
{
    VCEAppConfig = appConfig;
    if (enc)
    {
        if (!((VCEEncoder*)enc)->init())
            return false;
    }
    return true;
}

extern "C" __declspec(dllexport) bool __cdecl CheckVCEHardwareSupport(bool log)
{
    cl_platform_id platform = 0;
    cl_device_id device = 0;

    if (!initOVE() || !getPlatform(platform))
        return false;

    cl_device_type type = CL_DEVICE_TYPE_GPU;
    gpuCheck(platform, &type);
    if (type != CL_DEVICE_TYPE_GPU)
        return false;

    //Might be enough to just check if OVEncodeGetDeviceInfo returns anything.
    uint32_t numDevices = 0;
    if (!OVEncodeGetDeviceInfo(&numDevices, NULL)
            && numDevices == 0)
        return false;

    VCELog(TEXT("Seems to have support for AMD VCE."));
    return true;
}

//Keeping it like x264/qsv/nvenc (for now atleast)
extern "C" __declspec(dllexport) VideoEncoder* __cdecl CreateVCEEncoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR)
{
    if (!initOVE())
        return 0;

    VCEEncoder *enc = new VCEEncoder(fps, width, height, quality, preset, bUse444, colorDesc, maxBitRate, bufferSize, bUseCFR);

    encoderRefs += 1; //Lock?

    return enc;
}

void encoderRefDec()
{
    assert(encoderRefs);

    encoderRefs -= 1;

    if (encoderRefs == 0)
        deinitOVE();
}

bool VCEEncoder::init()
{
    bool status = false;
    if (!initOVE())
        return false;

    prepareConfigMap(mConfigTable, false);
    quickSet(mConfigTable, 0);//Default to speed

    memset(&mConfigCtrl, 0, sizeof (OvConfigCtrl));
    encodeSetParam(&mConfigCtrl, &mConfigTable);

    mConfigCtrl.rateControl.encRateControlFrameRateNumerator = mFps;
    mConfigCtrl.rateControl.encRateControlFrameRateDenominator = 1;
    mConfigCtrl.width = mWidth;
    mConfigCtrl.height = mHeight;
    mConfigCtrl.rateControl.encRateControlTargetBitRate = mMaxBitrate * 1000;
    // Driver resets to bitrate?
    mConfigCtrl.rateControl.encRateControlPeakBitRate = mMaxBitrate * 1000;
    // Always 10Mbps :S
    mConfigCtrl.rateControl.encVBVBufferSize = mBufferSize * 1000;

    //TODO IDR nice for seeking in local files. Intra-refresh better for streaming?
    mConfigCtrl.pictControl.encIDRPeriod = mKeyint * mFps;
    //mConfigCtrl.pictControl.encForceIntraRefresh = mKeyint * mFps;
    //TODO Usually 0 to let VCE manage it.
    mConfigCtrl.rateControl.encGOPSize = mKeyint * mFps;
    mConfigCtrl.priority = OVE_ENCODE_TASK_PRIORITY_LEVEL2;

    int QP = 23;
    //QP = 51 - (mQuality * 9) / 2;
    QP = 40 - (mQuality * 5) / 2; // 40 .. 15
    //QP = (10 - mQuality) * 5; // 0..50
    VCELog(TEXT("Quality: %d => QP %d"), mQuality, QP);

    mConfigCtrl.rateControl.encQP_I =
        mConfigCtrl.rateControl.encQP_P =
        mConfigCtrl.rateControl.encQP_B = QP;

    if (mUseCBR)
        mConfigCtrl.rateControl.encRateControlMethod = RCM_CBR;
    else if (mUseCFR) //Not really
        mConfigCtrl.rateControl.encRateControlMethod = RCM_FQP;
    else
        mConfigCtrl.rateControl.encRateControlMethod = RCM_VBR;

    VCELog(TEXT("Rate control method: %d"), mConfigCtrl.rateControl.encRateControlMethod);

    int numMBs = ((mWidth + 15) / 16) * ((mHeight + 15) / 16);
    mConfigCtrl.pictControl.encNumMBsPerSlice = numMBs;

    if (mDeviceHandle.deviceInfo)
    {
        delete[] mDeviceHandle.deviceInfo;
        mDeviceHandle.deviceInfo = NULL;
    }

    status = getDevice(&mDeviceHandle);
    if (status == false)
    {
        VCELog(TEXT("Failed to query CL platform/devices."));
        return false;
    }

    if (mDeviceHandle.numDevices == 0)
    {
        VCELog(TEXT("No VCE devices found."));
        return false;
    }

    //TODO Hardcoded to first device
    uint32_t device = mDeviceHandle.deviceInfo[0].device_id;

    status = encodeCreate(device) && encodeOpen(device);
    if (!status)
    {
        //Cleanup
        encodeClose();
        return false;
    }

    mIsReady = true;
    return true;
}

//TODO
VCEEncoder::VCEEncoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR)
    : mFps(fps)
    , mWidth(width)
    , mHeight(height)
    , mQuality(quality)
    , mPreset(preset)
    , mColorDesc(colorDesc)
    , mMaxBitrate(maxBitRate)
    , mBufferSize(bufferSize)
    , mUse444(bUse444)
    , mUseCFR(bUseCFR)
    , mFirstFrame(true)
    , frameShift(0)
    , mHdrPacket(NULL)
    , mIsReady(false)
    , mReqKeyframe(false)
{
    frameMutex = OSCreateMutex();

    mUseCBR = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseCBR"), 1) != 0;
    mKeyint = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("KeyframeInterval"), 0);

    headerPacket.SetSize(128);
    memset(&mDeviceHandle, 0, sizeof(mDeviceHandle));
    memset(&mEncodeHandle, 0, sizeof(mEncodeHandle));

    mAlignedSurfaceWidth = ((width + (256 - 1)) & ~(256 - 1));
    mAlignedSurfaceHeight = (true) ? (height + 31) & ~31 :
        (height + 15) & ~15;

    // NV12 is 3/2
    mInputBufSize = mAlignedSurfaceHeight * mAlignedSurfaceWidth * 3 / 2;
}

//TODO
VCEEncoder::~VCEEncoder()
{
    encodeClose();

    cl_int err;
    if ((cl_context)mOveContext)
    {
        err = clReleaseContext((cl_context)mOveContext);
        if (err != CL_SUCCESS)
        {
            VCELog(TEXT("Error releasing cl context"));
        }
    }
    mOveContext = NULL;

    if (mDeviceHandle.deviceInfo)
    {
        delete[] mDeviceHandle.deviceInfo;
        mDeviceHandle.deviceInfo = NULL;
    }

    free(mHdrPacket);
    mHdrPacket = NULL;

    encoderRefDec();
    OSCloseMutex(frameMutex);
}

//TODO
bool VCEEncoder::Encode(LPVOID picIn, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD timestamp)
{
    if (!mIsReady)
        return false;

    cl_int err;
    bool ret = true;
    //cl_event inMapEvt;
    cl_int   status = CL_SUCCESS;
    uint32_t numEventInWaitList = 0;
    OVresult res = 0;
    OPEventHandle eventRunVideoProgram = 0;
    OVE_ENCODE_PARAMETERS_H264 pictureParameter;
    uint32_t numEncodeTaskInputBuffers = 1;
    OVE_INPUT_DESCRIPTION encodeTaskInputBufferList[1];
    OVE_OUTPUT_DESCRIPTION pTaskDescriptionList[1];
    uint32_t iTaskID;
    uint32_t numTaskDescriptionsRequested = 1;
    uint32_t numTaskDescriptionsReturned = 0;

    OSMutexLocker locker(frameMutex);

    packets.Clear();
    packetTypes.Clear();

    //TODO EOS
    if (!picIn)
        return true;

    profileIn("VCE encode")
    mfxFrameSurface1 *inputSurface = (mfxFrameSurface1*)picIn;
    mfxFrameData &data = inputSurface->Data;
    assert(data.MemId);

    unsigned int idx = (unsigned int)data.MemId - 1;

#ifdef _DEBUG
    VCELog(TEXT("Encoding buffer: %d"), idx);
    VCELog(TEXT("Mapped handle: %p"), mEncodeHandle.inputSurfaces[idx].mapPtr);
#endif

    //mEncodeHandle.inputSurfaces[idx].locked = true;
    _InterlockedCompareExchange(&(mEncodeHandle.inputSurfaces[idx].locked), 1, 0);

    encodeTaskInputBufferList[0].bufferType = OVE_BUFFER_TYPE_PICTURE;
    encodeTaskInputBufferList[0].buffer.pPicture = (OVE_SURFACE_HANDLE)
        mEncodeHandle.inputSurfaces[idx].surface;

    // Encoder can't seem to use mapped buffer, all green :(
    unmapBuffer(&mEncodeHandle, idx);

    memset(&pictureParameter, 0, sizeof(OVE_ENCODE_PARAMETERS_H264));
    pictureParameter.size = sizeof(OVE_ENCODE_PARAMETERS_H264);
    pictureParameter.flags.value = 0;
    pictureParameter.flags.flags.reserved = 0;
    pictureParameter.insertSPS = (OVE_BOOL)mFirstFrame;
    pictureParameter.pictureStructure = OVE_PICTURE_STRUCTURE_H264_FRAME;
    pictureParameter.forceRefreshMap = (OVE_BOOL)true;
    pictureParameter.forceIMBPeriod = 0;
    pictureParameter.forcePicType = OVE_PICTURE_TYPE_H264_NONE;

    if (mReqKeyframe || mFirstFrame)
    {
        mReqKeyframe = false;
        pictureParameter.forcePicType = OVE_PICTURE_TYPE_H264_IDR;
    }

    //Enqueue encoding task
    res = OVEncodeTask(mEncodeHandle.session,
        numEncodeTaskInputBuffers,
        encodeTaskInputBufferList,
        &pictureParameter,
        &iTaskID,
        numEventInWaitList,
        NULL,
        &eventRunVideoProgram);
    if (!res)
    {
        VCELog(TEXT("OVEncodeTask returned error %fd"), res);
        ret = false;
        goto fail;
    }

    profileIn("Wait for task(s)")
    //wait for encoder
    err = clWaitForEvents(1, (cl_event *)&(eventRunVideoProgram));
    if (err != CL_SUCCESS)
    {
        VCELog(TEXT("clWaitForEvents returned error %d"), err);
        ret = false;
        goto fail;
    }

    //Retrieve h264 bitstream
    numTaskDescriptionsReturned = 0;
    memset(pTaskDescriptionList, 0, sizeof(OVE_OUTPUT_DESCRIPTION)*numTaskDescriptionsRequested);
    pTaskDescriptionList[0].size = sizeof(OVE_OUTPUT_DESCRIPTION);
    do
    {
        res = OVEncodeQueryTaskDescription(mEncodeHandle.session,
            numTaskDescriptionsRequested,
            &numTaskDescriptionsReturned,
            pTaskDescriptionList);
        if (!res)
        {
            VCELog(TEXT("OVEncodeQueryTaskDescription returned error %fd"), err);
            ret = false;
            goto fail;
        }

    } while (pTaskDescriptionList->status == OVE_TASK_STATUS_NONE);
    profileOut

    profileIn("Remap buffer")
    mapBuffer(&mEncodeHandle, idx, mInputBufSize);
    profileOut

    //mEncodeHandle.inputSurfaces[idx].locked = false;
    _InterlockedCompareExchange(&(mEncodeHandle.inputSurfaces[idx].locked), 0, 1);

    profileIn("Get bitstream")
    //Copy data out from VCE
    for (uint32_t i = 0; i<numTaskDescriptionsReturned; i++)
    {
        if ((pTaskDescriptionList[i].status == OVE_TASK_STATUS_COMPLETE)
            && pTaskDescriptionList[i].size_of_bitstream_data > 0)
        {
            ProcessOutput(&pTaskDescriptionList[i], packets, packetTypes, timestamp);
            if (mFirstFrame)
            {
                free(mHdrPacket);
                mHdrSize = MIN(pTaskDescriptionList[i].size_of_bitstream_data, 128);
                mHdrPacket = (char*)malloc(mHdrSize);
                memcpy(mHdrPacket, pTaskDescriptionList[i].bitstream_data, mHdrSize);
            }

            res = OVEncodeReleaseTask(mEncodeHandle.session, pTaskDescriptionList[i].taskID);
        }
    }
    profileOut

fail:
    if (eventRunVideoProgram)
        clReleaseEvent((cl_event)eventRunVideoProgram);

    mFirstFrame = false;
    profileOut
    return ret;
}

void VCEEncoder::ProcessOutput(OVE_OUTPUT_DESCRIPTION *surf, List<DataPacket> &packets, List<PacketType> &packetTypes, uint64_t timestamp)
{
    encodeData.Clear();

    uint8_t *pstart = (uint8_t*) surf->bitstream_data;

    uint8_t *start = pstart;
    uint8_t *end = start + surf->size_of_bitstream_data;
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

    uint64_t dts = timestamp;// surf->timestamp;
    //uint64_t out_pts = lockParams.outputTimeStamp;

    //FIXME
    int32_t timeOffset = 0;// int(out_pts - dts);
    //timeOffset += frameShift;

    if (nalNum && timeOffset < 0)
    {
        frameShift -= timeOffset;
        timeOffset = 0;
    }

    //timeOffset = htonl(timeOffset);
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

            // P is _HIGH, I is _HIGHEST
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
}

//TODO
void VCEEncoder::RequestBuffers(LPVOID buffers)
{
    cl_event inMapEvt = 0;
    cl_int   status = CL_SUCCESS;

    if (!buffers || !mIsReady)
        return;

    mfxFrameData *buff = (mfxFrameData*)buffers;

#ifdef _DEBUG
    VCELog(TEXT("RequestBuffers: %d"), (unsigned int)buff->MemId - 1);
#endif

    //TODO Threaded NV12 conversion asks same buffer (thread count) times?
    if (buff->MemId && mEncodeHandle.inputSurfaces[(unsigned int)buff->MemId - 1].isMapped &&
        !mEncodeHandle.inputSurfaces[(unsigned int)buff->MemId - 1].locked)
        return;

    //Trying atomic inputSurface.locked instead
    //OSMutexLocker locker(frameMutex);

    for (int i = 0; i < MAX_INPUT_SURFACE; ++i)
    {
        if (mEncodeHandle.inputSurfaces[i].isMapped || 
            mEncodeHandle.inputSurfaces[i].locked ||
            !mEncodeHandle.inputSurfaces[i].surface) //Out of memory?
            continue;

        mapBuffer(&mEncodeHandle, i, mInputBufSize);

        if (!mEncodeHandle.inputSurfaces[i].mapPtr)
            continue;

        buff->Pitch = mAlignedSurfaceWidth;
        buff->Y = (mfxU8*)mEncodeHandle.inputSurfaces[i].mapPtr;
        buff->UV = buff->Y + (mHeight * buff->Pitch);

        buff->MemId = mfxMemId(i + 1);
        //mEncodeHandle.inputSurfaces[i].locked = 0; //false
        _InterlockedCompareExchange(&(mEncodeHandle.inputSurfaces[i].locked), 0, 1);

        return;
    }
}

//TODO
int  VCEEncoder::GetBitRate() const
{
    return mMaxBitrate;
}

//TODO
bool VCEEncoder::DynamicBitrateSupported() const
{
    return true;
}

//TODO
bool VCEEncoder::SetBitRate(DWORD maxBitrate, DWORD bufferSize)
{
    OSMutexLocker locker(frameMutex);

    if (maxBitrate > 0)
    {
        mMaxBitrate = maxBitrate;
        mConfigCtrl.rateControl.encRateControlTargetBitRate = maxBitrate * 1000;
        //TODO
        //if (bufferSize == 0)
        //    mConfigCtrl.rateControl.encVBVBufferSize = maxBitrate * 1000 / 2;
    }

    if (bufferSize > 0)
    {
        mBufferSize = bufferSize;
        mConfigCtrl.rateControl.encVBVBufferSize = bufferSize * 1000;
    }

    //TODO Supported?
    setEncodeConfig(mEncodeHandle.session, &mConfigCtrl);

    return true;
}

void VCEEncoder::GetHeaders(DataPacket &packet)
{
    uint32_t outSize = mHdrSize;
    if (!mHdrPacket)
    {
        VCELog(TEXT("No header packet yet."));
        return;
    }

    char *sps = mHdrPacket;
    unsigned int i_sps = 4, i_pps;

    while (mHdrPacket[i_sps] != 0x00
        || mHdrPacket[i_sps + 1] != 0x00
        || mHdrPacket[i_sps + 2] != 0x00
        || mHdrPacket[i_sps + 3] != 0x01)
    {
        i_sps += 1;
        if (i_sps >= outSize)
        {
            VCELog(TEXT("Invalid SPS/PPS"));
            return;
        }
    }

    i_pps = i_sps + 4;

    while (mHdrPacket[i_pps] != 0x00
        || mHdrPacket[i_pps + 1] != 0x00
        || mHdrPacket[i_pps + 2] != 0x00
        || mHdrPacket[i_pps + 3] != 0x01)
    {
        i_pps += 1;
        if (i_pps >= outSize)
        {
            VCELog(TEXT("Invalid SPS/PPS"));
            return;
        }
    }

    char *pps = mHdrPacket + i_sps;
    //unsigned int 
    i_pps = i_pps - i_sps;

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
    headerOut.OutputWord(fastHtons(i_sps - 4));
    headerOut.Serialize(sps + 4, i_sps - 4);

    headerOut.OutputByte(1);
    headerOut.OutputWord(fastHtons(i_pps - 4));
    headerOut.Serialize(pps + 4, i_pps - 4);

    packet.size = headerPacket.Num();
    packet.lpPacket = headerPacket.Array();
}

void VCEEncoder::RequestKeyframe()
{ 
    OSMutexLocker locker(frameMutex);
    mReqKeyframe = true;
#ifdef _DEBUG
    VCELog(TEXT("Keyframe requested"));
#endif
}
