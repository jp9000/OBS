#include "stdafx.h"
#include "Encoder.h"
#include <../libmfx/include/msdk/include/mfxstructures.h>

extern ConfigFile **VCEAppConfig;

#ifndef htonl
#define htons(n) (((((uint16_t)(n) & 0xFF)) << 8) | (((uint16_t)(n) & 0xFF00) >> 8))
#define htonl(n) (((((uint32_t)(n) & 0xFF)) << 24) | \
    ((((uint32_t)(n)& 0xFF00)) << 8) | \
    ((((uint32_t)(n)& 0xFF0000)) >> 8) | \
    ((((uint32_t)(n)& 0xFF000000)) >> 24))
#endif

extern "C"
{
#define _STDINT_H
#include <../x264/x264.h>
#undef _STDINT_H
}

VCEEncoder::VCEEncoder(int fps, int width, int height, int quality, const TCHAR* preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR)
: mBuilder(nullptr)
, mPVideoType(nullptr)
, mLogFile(stderr) //TODO remove
, mAlive(false)
, mRefCount(1)
, mFps(fps)
, mWidth(width)
, mHeight(height)
, mQuality(quality)
, mPreset(preset)
, mColorDesc(colorDesc)
, mMaxBitrate(maxBitRate)
, mNewBitrate(maxBitRate)
, mBufferSize(bufferSize)
, mUse444(bUse444)
, mUseCFR(bUseCFR)
, mFirstFrame(true)
, mHdrPacket(nullptr)
, mHdrSize(0)
, frameShift(0)
, mIDevMgr(nullptr)
, mCurrBucketSize(0)
, mMaxBucketSize(0)
, mDesiredPacketSize(0)
, mCurrFrame(0)
//, mCanReuseSample(false)
{
    mInBuffSize = mWidth * mHeight * 3 / 2;
    mBuilder = new msdk_CMftBuilder();
    memset(mInputBuffers, 0, sizeof(mInputBuffers));

    mUseCBR = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseCBR"), 1) != 0;
    mKeyint = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("KeyframeInterval"), 0);
    bool bUsePad = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("PadCBR"), 0) != 0;
    if (bUsePad)
    {
        mMaxBucketSize = mMaxBitrate * (1000 / 8);
        mDesiredPacketSize = mMaxBucketSize / mFps;
    }
}

VCEEncoder::~VCEEncoder()
{
    delete mBuilder;
    mBuilder = nullptr;

     delete [] mHdrPacket;
     mHdrPacket = nullptr;

    //Wants explicit Release() because Detach()
    mEncTrans.Release();

    //while (!mInputQueue.empty())
    //    mInputQueue.pop();
    mInputQueue = {};

    for (int i = 0; i < MAX_INPUT_SURFACE; i++)
    {
        if (mInputBuffers[i].pBufferPtr)
        {
            mInputBuffers[i].pBufferPtr = nullptr;
            mInputBuffers[i].pBuffer->Unlock();
            mInputBuffers[i].pBuffer.Release();
        }
    }

    while (!mOutputQueue.empty())
    {
        OutputList *out = mOutputQueue.front();
        mOutputQueue.pop();
        out->pBuffer.Clear();
        delete out;
    }

    if (mIDevMgr)
        mIDevMgr->Release();

    /*delete mComDealloc;
    mComDealloc = nullptr;*/

    delete mMFDealloc;
    mMFDealloc = nullptr;
}

HRESULT VCEEncoder::Stop()
{
    if (!mAlive)
        return E_UNEXPECTED;

    HRESULT hr = mEncTrans->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
    hr = mEncTrans->ProcessMessage(MFT_MESSAGE_NOTIFY_END_STREAMING, 0);
    hr = mEncTrans->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
    return hr;
}

HRESULT VCEEncoder::Init()
{
    HRESULT hr;
    if (mAlive)
        return S_OK;

    //OBS has initialized
    //hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    //if (S_OK != hr && S_FALSE != hr /* already inited */)
    //{
    //    VCELog(TEXT("Com initialization failed with %d"), hr);
    //    return (0x10000 | 0x0001);
    //}
    //mComDealloc = new FunctionDeallocator< void(__stdcall*)(void) >(CoUninitialize);

    hr = MFStartup(MF_VERSION);
    if (S_OK != hr)
    {
        VCELog(TEXT("MFStartup failed."));
		return (0x20000 | 0x0001);
    }
    mMFDealloc = new FunctionDeallocator< HRESULT(__stdcall*)(void) >(MFShutdown);

    memset(&mPConfigCtrl, 0, sizeof(mPConfigCtrl));

    int iNumRefs = 3, QvsS = 0;
    int bitRateWindow = 50;
    int gopSize = mFps * (mKeyint == 0 ? 2 : mKeyint);
    gopSize -= gopSize % 6;

    int picSize = mWidth * mHeight * mFps;
    if (picSize <= 1280 * 720 * 60)
        QvsS = 100;
    else if (picSize <= 1920 * 1080 * 30)
        QvsS = 50;
    else
        QvsS = 0;

    OSDebugOut(TEXT("QvsS: %d\n"), QvsS);

    String x264prof = AppConfig->GetString(TEXT("Video Encoding"), TEXT("X264Profile"));
    eAVEncH264VProfile profile = eAVEncH264VProfile_Main;
    if (x264prof.Compare(TEXT("high")))
        profile = eAVEncH264VProfile_High;
    //Not in advanced setting but for completeness sake
    else if (x264prof.Compare(TEXT("base")))
        profile = eAVEncH264VProfile_Base;

    mPConfigCtrl.commonParams.useInterop = 0;
    mPConfigCtrl.commonParams.useSWCodec = 0;
    mPConfigCtrl.height = mHeight;
    mPConfigCtrl.width = mWidth;
    mPConfigCtrl.vidParams.commonQuality = mQuality * 10;
    mPConfigCtrl.vidParams.enableCabac = 1;

    // http://msdn.microsoft.com/en-us/library/windows/desktop/dd317651%28v=vs.85%29.aspx bits or bytes??
    mPConfigCtrl.vidParams.bufSize = mBufferSize * 1000;
    mPConfigCtrl.vidParams.meanBitrate = mMaxBitrate * 1000;// (1000 + bitRateWindow);
    mPConfigCtrl.vidParams.maxBitrate = mMaxBitrate * (1000 - bitRateWindow);
    mPConfigCtrl.vidParams.idrPeriod = (mKeyint > 0 ? mKeyint : 2) * mFps;
    mPConfigCtrl.vidParams.gopSize = gopSize;
    mPConfigCtrl.vidParams.qualityVsSpeed = QvsS;
    mPConfigCtrl.vidParams.compressionStandard = profile;
    mPConfigCtrl.vidParams.numBFrames = 0;
    mPConfigCtrl.vidParams.lowLatencyMode = AppConfig->GetInt(TEXT("VCE Settings"), TEXT("LowLatency"), 0);

    if (mUseCBR)
        mPConfigCtrl.vidParams.rateControlMethod = eAVEncCommonRateControlMode_CBR;
    else if (mUseCFR)
        mPConfigCtrl.vidParams.rateControlMethod = eAVEncCommonRateControlMode_Quality;
    else
        mPConfigCtrl.vidParams.rateControlMethod = eAVEncCommonRateControlMode_PeakConstrainedVBR;

    //Apply user's custom settings that are set when creating encoder transform
    if (AppConfig->GetInt(TEXT("VCE Settings"), TEXT("UseCustom"), 0))
    {
#define APPCFG(x,y) x = AppConfig->GetInt(TEXT("VCE Settings"), TEXT(y), x)
        APPCFG(mPConfigCtrl.vidParams.gopSize, "GOPSize");
        APPCFG(mPConfigCtrl.vidParams.idrPeriod, "IDRPeriod");
        APPCFG(mPConfigCtrl.vidParams.qualityVsSpeed, "QualityVsSpeed");
        APPCFG(mPConfigCtrl.vidParams.enableCabac, "CABAC");
        iNumRefs = AppConfig->GetInt(TEXT("VCE Settings"), TEXT("NumRefs"), 3);
#undef APPCFG
    }

    bool useDx11 = mBuilder->isDxgiSupported();

    hr = createVideoMediaType(NULL, 0, mWidth, mHeight);// , &mPVideoType);
    RETURNIFFAILED(hr);

    CComPtr<IMFMediaType> h264VideoType;
    hr = createH264VideoType(&h264VideoType, mPVideoType);
    RETURNIFFAILED(hr);

    //h264VideoType->SetUINT32(MF_MT_MAX_KEYFRAME_SPACING, gopSize);
    //h264VideoType->SetUINT32(MF_MT_YUV_MATRIX, MFVideoTransferMatrix_BT709);

    ULONG_PTR deviceManagerPtr;
    if (useDx11)
    {
        // Create DX11 device
        hr = mBuilder->createDxgiDeviceManagerPtr(&deviceManagerPtr);
        LOGIFFAILED(mLogFile, hr, "Failed create Dx11 device");
    }
    else
    {
        // DirectX 9 requires a window.
        HWND hWnd = GetDesktopWindow();
        HRESULT hr = mBuilder->createDirect3DDeviceManagerPtr(hWnd,
            &deviceManagerPtr);
        LOGIFFAILED(mLogFile, hr, "Failed create Dx9 device");
    }

    mIDevMgr = reinterpret_cast<IUnknown*>(deviceManagerPtr);

    // Create H264 encoder transform
    hr = mBuilder->createVideoEncoderNode(h264VideoType,
        mPVideoType, &mEncTrans, deviceManagerPtr,
        &mPConfigCtrl.vidParams, mPConfigCtrl.commonParams.useSWCodec);
    LOGIFFAILED(mLogFile, hr, "Failed to create encoder transform");

    //MF_MT_AVG_BITRATE
    hr = mBuilder->setEncoderValue(&CODECAPI_AVEncCommonMeanBitRate,
        (uint32_t)mPConfigCtrl.vidParams.meanBitrate, mEncTrans);
    if (hr != S_OK)
    {
        VCELog(TEXT("Failed to set CODECAPI_AVEncCommonMeanBitRate"));
    }

    //Fails
    /*if (mUseCBR)
    {
        hr = mBuilder->setEncoderValue(&CODECAPI_AVEncCommonMinBitRate,
            (uint32_t)mPConfigCtrl.vidParams.meanBitrate, mEncTrans);
        if (hr != S_OK)
        {
            VCELog(TEXT("Failed to set CODECAPI_AVEncCommonMinBitRate"));
        }
    }*/

    hr = mBuilder->setEncoderValue(&CODECAPI_AVEncVideoMaxNumRefFrame,
        (uint32_t)iNumRefs, mEncTrans);
    if (hr != S_OK)
    {
        VCELog(TEXT("Failed to set CODECAPI_AVEncVideoMaxNumRefFrame"));
    }

    MFFrameRateToAverageTimePerFrame(mFps, 1, &mFrameDur);

    /// Get it rolling
    hr = mEncTrans->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    hr = mEncTrans->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);

    mAlive = true;
    return hr;
}

HRESULT VCEEncoder::OutputFormatChange()
{
    //Either like MSDN says..
    //CComPtr<IMFMediaType> pType = NULL;
    //GUID guid;
    //for (int i = 0; mEncTrans->GetOutputAvailableType(0, i, &pType) == S_OK; i++)
    //{
    //    pType->GetGUID(MF_MT_SUBTYPE, &guid);
    //    if (IsEqualGUID(guid, MFVideoFormat_H264))
    //        break;
    //}

    //if (pType) //.p)
    //    return mEncTrans->SetOutputType(0, pType, 0);

    //return MF_E_TRANSFORM_TYPE_NOT_SET; //or something

    //or ignore the noise and re-set our own type
    CComPtr<IMFMediaType> h264VideoType;
    HRESULT hr = createH264VideoType(&h264VideoType, mPVideoType);
    RETURNIFFAILED(hr);
    return mEncTrans->SetOutputType(0, h264VideoType, 0);
}

HRESULT VCEEncoder::CreateSample(InputBuffer &inBuf, IMFSample **sample)
{
    HRESULT hr;
    UINT64 dur = 0;
    //MSDN: Usually MFT holds onto IMFSample, so it can't be reused anyway.
    //CComPtr<IMFSample> sample;

    DWORD maxLen = 0, currLen = 0;

    profileIn("CreateSample")

    hr = MFCreateSample(sample);
    LOGIFFAILED(mLogFile, hr, "MFCreateSample failed.");

    inBuf.pBuffer->Unlock();
    hr = (*sample)->AddBuffer(inBuf.pBuffer);
    inBuf.locked = false;
    inBuf.pBuffer->Lock(&(inBuf.pBufferPtr), &currLen, &maxLen);

    LOGIFFAILED(mLogFile, hr, "ProcessInput: failed to add buffer to sample");

    (*sample)->SetSampleDuration((LONGLONG)mFrameDur);
    (*sample)->SetSampleTime(LONGLONG(inBuf.timestamp) * MS_TO_100NS);
    profileOut
    return hr;
}

HRESULT VCEEncoder::ProcessInput(IMFSample *sample)
{
    HRESULT hr = S_OK;
    profileIn("ProcessInput")
    hr = mEncTrans->ProcessInput(0, sample, 0);

    if (MF_E_NOTACCEPTING == hr)
    {
        //VCELog(
        //    TEXT("ProcessInput: MF_E_NOTACCEPTING.")
        //    //TEXT(" If this is logged too often, probably lower your encode quality settings."));
        OSDebugOut(TEXT("ProcessInput: MF_E_NOTACCEPTING.\n"));
        return hr;
    }
    else
        LOGIFFAILED(mLogFile, hr, "ProcessInput failed.");
    profileOut
    return hr;
}

HRESULT VCEEncoder::ProcessOutput(List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD timestamp, DWORD &out_pts)
{
    HRESULT hr;
    UINT64 dur = 0;
    CComPtr<IMFMediaBuffer> pBuffer(NULL);
    BYTE *ppBuffer = NULL;

    MFT_OUTPUT_DATA_BUFFER out[1] = { 0 };
    DWORD status = 0, currLen, maxLen;

    MFT_OUTPUT_STREAM_INFO si = { 0 };

    profileIn("ProcessOutput")
    mEncTrans->GetOutputStreamInfo(0, &si);

    //TODO Handle other cases too, maybe
    if (!HASFLAG(si.dwFlags, 
            MFT_OUTPUT_STREAM_WHOLE_SAMPLES |
            MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER |
            MFT_OUTPUT_STREAM_PROVIDES_SAMPLES)) //Otherwise need to allocate own IMFSample
    {
        return S_FALSE;
    }

    //E_UNEXPECTED if no METransformHaveOutput event
    hr = mEncTrans->ProcessOutput(0, 1, out, &status);
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
        return hr;

    //Even if GetOutputStatus gives OK, ProcessOutput may fail with E_UNEXPECTED
    if (FAILED(hr))
        return hr;

    if (out[0].dwStatus & MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE)
    {
        return OutputFormatChange();
    }

    if (out[0].pEvents)
    {
        out[0].pEvents->Release();
        out[0].pEvents = nullptr;
    }

    out[0].pSample->GetSampleTime((LONGLONG*)&dur);
    
    out[0].pSample->ConvertToContiguousBuffer(&pBuffer);
    hr = pBuffer->Lock(&ppBuffer, &maxLen, &currLen);
    if (SUCCEEDED(hr))
    {
        if (nullptr == mHdrPacket)
        {
            mHdrSize = maxLen;
            mHdrPacket = new uint8_t[mHdrSize];
            memcpy(mHdrPacket, ppBuffer, mHdrSize);
        }

        OutputBuffer buf;
        buf.pBuffer = ppBuffer;
        buf.size = maxLen;
        buf.timestamp = dur / MS_TO_100NS;
        out_pts = buf.timestamp;
        ProcessBitstream(buf, packets, packetTypes, timestamp);
    }
    hr = pBuffer->Unlock();
    pBuffer.Release();

    out[0].pSample->Release();
    profileOut

    return hr;
}

void VCEEncoder::DrainOutput(List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD timestamp, DWORD &out_pts)
{
    HRESULT hr = S_OK;
    DWORD status = 0;

    profileIn("Drain")
    while (true)//packets.Num() == 0)
    {
        /*hr = mEncTrans->GetOutputStatus(&status);
        if ((hr != S_OK && hr != E_NOTIMPL) || status != MFT_OUTPUT_STATUS_SAMPLE_READY)
            break;*/

        hr = ProcessOutput(packets, packetTypes, timestamp, out_pts);
        if (hr != S_OK)
            break;
    }
    profileOut

#if _DEBUG
    OSDebugOut(TEXT("Got %d output frames\n"), packets.Num());
#endif
}

//OBS doesn't use return value, instead checks packets count
bool VCEEncoder::Encode(LPVOID picIn, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD timestamp, DWORD &out_pts)
{
    HRESULT hr = S_OK;

    CComCritSecLock<CComMultiThreadModel::AutoCriticalSection> \
        lock(mStateLock, true);

    packets.Clear();
    packetTypes.Clear();

    profileIn("Encode")

    //Remove OutputLists from previous Encode() call.
    //OBS::BufferVideoData() should have copied anything it wanted by now.
    while (!mOutputQueue.empty())
    {
        OutputList *out = mOutputQueue.front();
        mOutputQueue.pop();
        delete out; //dtor should call Clear()
    }

    if (!picIn)
    {
        Stop();
        //FIXME can't do multiple out_pts
        ProcessOutput(packets, packetTypes, timestamp, out_pts);
        //DrainOutput(packets, packetTypes, timestamp);
        OSDebugOut(TEXT("Drained %d frame(s)\n"), packets.Num());
        return true;
    }

    mfxFrameSurface1 *inputSurface = (mfxFrameSurface1*)picIn;
    mfxFrameData &data = inputSurface->Data;
    assert(data.MemId);

    unsigned int idx = (unsigned int)data.MemId - 1;

    InputBuffer &inBuffer = mInputBuffers[idx];
    inBuffer.locked = true;
    inBuffer.timestamp = timestamp;

    CComPtr<IMFSample> sample;
    hr = CreateSample(inBuffer, &sample);

    //OSDebugOut(TEXT("IN queue %d\n"), mInputQueue.size());

    if (mReqKeyframe)
    {
        //MSDN says it is reset for next frame automatically
        hr = mBuilder->setEncoderValue(&CODECAPI_AVEncVideoForceKeyFrame,
            (uint32_t)1, mEncTrans);
        if (FAILED(hr))
        {
            VCELog(TEXT("Manual keyframe was requested, but failed to set encoder property. hr: %08X, ts: %d"), hr, timestamp);
        }
        mReqKeyframe = false;
    }

    HRESULT hrIn = S_OK;
    hr = S_OK;
    while (true)
    {
        hrIn = ProcessInput(sample);
        profileIn("Output")
        OSSleep(1000/(mFps + 10));
        while (/*packets.Num() == 0 &&*/ SUCCEEDED(hrIn))
        {

            // Useless, MFT_OUTPUT_STATUS_SAMPLE_READY and ProcessOutput still returns E_UNEXPECTED, wtf
            //DWORD flags = 0;
            //if (SUCCEEDED(mEncTrans->GetOutputStatus(&flags)) && !flags)
            //    continue;

            hr = ProcessOutput(packets, packetTypes, timestamp, out_pts);
            if (hr != E_UNEXPECTED)
                OSDebugOut(TEXT("ProcessOutput %d ts %d hr %08X out_pts %d\n"), packets.Num(), timestamp, hr, out_pts);

            if (hr != E_UNEXPECTED && FAILED(hr))
                break;

            if (packets.Num())
                break;

            OSSleep100NS(mFrameDur / 100);
        }
        profileOut

        if (SUCCEEDED(hrIn) || FAILED(hr) ||
            (hrIn != MF_E_NOTACCEPTING && FAILED(hrIn)))
            break;
    }

    while (hrIn == MF_E_NOTACCEPTING && packets.Num())
    {
        hrIn = ProcessInput(sample);
    }
    sample.Release();

    OSDebugOut(TEXT("Encode: hr %08X ts %d out pkts %d frame dropped: %d\n"), hrIn, timestamp, packets.Num(), hrIn == MF_E_NOTACCEPTING);
    profileOut
    return false;
}

void VCEEncoder::ProcessBitstream(OutputBuffer &buff, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD timestamp)
{
    profileIn("ProcessBitstream")

    OutputList *bufferedOut = new OutputList;

    uint8_t *start = (uint8_t *)buff.pBuffer;
    uint8_t *end = start + buff.size;
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

    //FIXME
    uint64_t dts = buff.timestamp;
    uint64_t out_pts = timestamp;

    int32_t timeOffset = 0;// int(out_pts - dts);
    timeOffset += frameShift;

    if (nalNum && timeOffset < 0)
    {
        frameShift -= timeOffset;
        timeOffset = 0;
    }

    //OSDebugOut(TEXT("Frame shifts: %d %d %d %lld\n"), frameShift, timeOffset, timestamp, buff.timestamp);

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
                    BufferOutputSerializer packetOut(bufferedOut->pBuffer);

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

            BufferOutputSerializer packetOut(bufferedOut->pBuffer);

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
                bufferedOut->pBuffer.Insert(0, (nal.i_type == NAL_SLICE_IDR) ? 0x17 : 0x27);
                bufferedOut->pBuffer.Insert(1, 1);
                bufferedOut->pBuffer.InsertArray(2, timeOffsetAddr, 3);

                bFoundFrame = true;
            }

            int newPayloadSize = (nal.i_payload - skipBytes);
            BufferOutputSerializer packetOut(bufferedOut->pBuffer);

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

    //Add filler
    if (mUseCBR && mDesiredPacketSize > 0)
    {
        if (mCurrFrame == 0)
        {
            if (mMaxBitrate != mNewBitrate)
            {
                mMaxBitrate = mNewBitrate;
                mMaxBucketSize = mMaxBitrate * 1000 / 8;
                mDesiredPacketSize = mMaxBucketSize / mFps;
            }

            //Random, trying to get OBS bitrate meter stay at mMaxBitrate
            if (mCurrBucketSize < -mMaxBucketSize)
                mCurrBucketSize = 0;

            mCurrBucketSize = mMaxBucketSize -
                (mCurrBucketSize < 0 ? -mCurrBucketSize : mCurrBucketSize);
        }

        if (mDesiredPacketSize > bufferedOut->pBuffer.Num() + 5
            && mCurrBucketSize > 0)
        {

            uint32_t fillerSize = mDesiredPacketSize - bufferedOut->pBuffer.Num() - 5;
            BufferOutputSerializer packetOut(bufferedOut->pBuffer);

            packetOut.OutputDword(htonl(fillerSize));
            packetOut.OutputByte(NAL_FILLER);//Disposable i_ref_idc

            uint32_t currSize = bufferedOut->pBuffer.Num();
            bufferedOut->pBuffer.SetSize(currSize + fillerSize);
            BYTE *ptr = bufferedOut->pBuffer.Array() + currSize;
            memset(ptr, 0xFF, fillerSize);

            mCurrBucketSize -= mDesiredPacketSize;
        }
        else
        {
            mCurrBucketSize -= bufferedOut->pBuffer.Num();
        }

        mCurrFrame++;
        if (mCurrFrame >= mFps)
            mCurrFrame = 0;
    }

    DataPacket packet;
    packet.lpPacket = bufferedOut->pBuffer.Array();
    packet.size = bufferedOut->pBuffer.Num();
    bufferedOut->timestamp = buff.timestamp;
    mOutputQueue.push(bufferedOut);

    packetTypes << bestType;
    packets << packet;
    nalOut.Clear();

    profileOut
}

//TODO RequestBuffers
void VCEEncoder::RequestBuffers(LPVOID buffers)
{
    HRESULT hr = S_OK;
    DWORD maxLen, curLen;
    mfxFrameData *buff = (mfxFrameData*)buffers;

    CComCritSecLock<CComMultiThreadModel::AutoCriticalSection> \
        lock(mStateLock, true);

    //TODO Threaded NV12 conversion asks same buffer (thread count) times?
    if (buff->MemId && mInputBuffers[(unsigned int)buff->MemId - 1].pBufferPtr
        && !mInputBuffers[(unsigned int)buff->MemId - 1].locked
        )
        return;

    for (int i = 0; i < MAX_INPUT_SURFACE; i++)
    {
        InputBuffer& inBuf = mInputBuffers[i];
        if (inBuf.locked)
            continue;

        if (!inBuf.pBufferPtr)
        {
            hr = MFCreateMemoryBuffer(mInBuffSize, &(inBuf.pBuffer));
            if (FAILED(hr))
            {
                VCELog(TEXT("MFCreateMemoryBuffer failed."));
                //FIXME now crash....
                buff->Y = nullptr;
                buff->Pitch = 0;
                buff->MemId = 0;
                return;
            }
            inBuf.pBuffer->SetCurrentLength(mInBuffSize);
        }
        else
            inBuf.pBuffer->Unlock();

        hr = inBuf.pBuffer->Lock(&(inBuf.pBufferPtr), &maxLen, &curLen);
        if (SUCCEEDED(hr))
        {
            if (maxLen < mInBuffSize) //Possible?
            {
                VCELog(TEXT("Buffer max length smaller than asked: %d"), maxLen);
                inBuf.pBufferPtr = nullptr;
                inBuf.pBuffer->Unlock();
                inBuf.pBuffer.Release();
                inBuf.locked = false;
                continue; //Try next
            }
            else
            {
                buff->Pitch = mWidth;
                buff->Y = (mfxU8*)inBuf.pBufferPtr;
                buff->UV = buff->Y + (mHeight * buff->Pitch);
                buff->MemId = mfxMemId(i + 1);
#if _DEBUG
                OSDebugOut(TEXT("Giving buffer id %d\n"), i + 1);
#endif
                inBuf.locked = false;
                return;
            }
        }
    }

    VCELog(TEXT("Max number of input buffers reached."));
    OSDebugOut(TEXT("Max number of input buffers reached.\n"));
}

void VCEEncoder::GetHeaders(DataPacket &packet)
{
    if (!mHdrPacket)
    {
        VCELog(TEXT("No header packet yet. Generating..."));
        List<DataPacket> vidPackets;
        List<PacketType> packetTypes;
        mfxFrameSurface1 tmp;
        DWORD out_pts;

        ZeroMemory(&tmp, sizeof(mfxFrameSurface1));
        RequestBuffers(&(tmp.Data));
        Encode(&tmp, vidPackets, packetTypes, 0, out_pts);

        // Clean up like nothing happened
        mInputBuffers[(int)tmp.Data.MemId - 1].pBuffer->Unlock();
        mInputBuffers[(int)tmp.Data.MemId - 1].pBuffer.Release();
        mInputBuffers[(int)tmp.Data.MemId - 1].pBufferPtr = nullptr;
        mReqKeyframe = true;
    }

    x264_nal_t nalSPS = { 0 }, nalPPS = { 0 };
    uint8_t *start = mHdrPacket;
    uint8_t *end = start + mHdrSize;
    const static uint8_t start_seq[] = { 0, 0, 1 };

    if (start)
    {
        start = std::search(start, end, start_seq, start_seq + 3);

        //Usually has NAL_AUD, NAL_SPS, NAL_PPS, NAL_SLICE_IDR
        while (start != end)
        {
            decltype(start) next = std::search(start + 1, end, start_seq, start_seq + 3);

            x264_nal_t nal;

            nal.i_ref_idc = (start[3] >> 5) & 3;
            nal.i_type = start[3] & 0x1f;

            nal.p_payload = start;
            nal.i_payload = int(next - start);
            start = next;

            if (nal.i_type == NAL_PPS)
                nalPPS = nal;
            else if (nal.i_type == NAL_SPS)
                nalSPS = nal;
        }
    }
    else
    {
        nalSPS.i_type = NAL_SPS;
        nalPPS.i_type = NAL_PPS;
        nalSPS.i_payload = 3;
        nalPPS.i_payload = 3;
    }

    headerPacket.Clear();
    BufferOutputSerializer headerOut(headerPacket);

    //While OVE has 4 byte start codes, MFT seems to give 3 byte ones
    headerOut.OutputByte(0x17);
    headerOut.OutputByte(0);
    headerOut.OutputByte(0);
    headerOut.OutputByte(0);
    headerOut.OutputByte(0);
    headerOut.OutputByte(1);
    if (nalSPS.p_payload)
        headerOut.Serialize(nalSPS.p_payload + 4, 3);
    headerOut.OutputByte(0xff);
    headerOut.OutputByte(0xe1);
    headerOut.OutputWord(fastHtons(nalSPS.i_payload - 3));
    if (nalSPS.p_payload)
        headerOut.Serialize(nalSPS.p_payload + 3, nalSPS.i_payload - 3);

    headerOut.OutputByte(1);
    headerOut.OutputWord(fastHtons(nalPPS.i_payload - 3));
    if(nalPPS.p_payload)
        headerOut.Serialize(nalPPS.p_payload + 3, nalPPS.i_payload - 3);

    packet.size = headerPacket.Num();
    packet.lpPacket = headerPacket.Array();
}

void VCEEncoder::RequestKeyframe()
{
    CComCritSecLock<CComMultiThreadModel::AutoCriticalSection> \
        lock(mStateLock, true);
    mReqKeyframe = true;
#ifdef _DEBUG
    OSDebugOut(TEXT("Keyframe requested\n"));
#endif
}

//TODO GetSEI
void VCEEncoder::GetSEI(DataPacket &packet)
{
    packet.size = 0;
    packet.lpPacket = NULL;
}

//TODO SetBitRate
bool VCEEncoder::SetBitRate(DWORD maxBitrate, DWORD bufferSize)
{
    HRESULT hr = S_OK;
    bool ok = true;
    if (maxBitrate > 0)
    {
        mPConfigCtrl.vidParams.meanBitrate = maxBitrate * 1000;
        hr = mBuilder->setEncoderValue(&CODECAPI_AVEncCommonMeanBitRate,
            (uint32_t)mPConfigCtrl.vidParams.meanBitrate, mEncTrans);
        if (hr != S_OK)
        {
            ok = false;
            VCELog(TEXT("Failed to set CODECAPI_AVEncCommonMeanBitRate"));
        }

        uint32_t val = mNewBitrate;
        hr = mBuilder->getEncoderValue(&CODECAPI_AVEncCommonMeanBitRate,
            &val, mEncTrans);
        if (SUCCEEDED(hr))
        {
            mNewBitrate = int32_t(val) / 1000;
        }
    }

    if (bufferSize > 0)
    {
        mPConfigCtrl.vidParams.bufSize = bufferSize * 1000;
        hr = mBuilder->setEncoderValue(&CODECAPI_AVEncCommonBufferSize,
            (uint32_t)mPConfigCtrl.vidParams.bufSize, mEncTrans);
        if (hr != S_OK)
        {
            ok = false;
            VCELog(TEXT("Failed to set CODECAPI_AVEncCommonBufferSize"));
        }
    }
    return ok;
}

//TODO GetBitRate
int  VCEEncoder::GetBitRate() const
{
    return mMaxBitrate;
}

//TODO DynamicBitrateSupported
bool VCEEncoder::DynamicBitrateSupported() const
{
    return true;
}

HRESULT VCEEncoder::createVideoMediaType(BYTE* pUserData, DWORD dwUserData, DWORD dwWidth, DWORD dwHeight)
{
    HRESULT hr = S_OK;
    CComPtr<IMFVideoMediaType> pType;

    //if (nullptr == ppType)
    //    return E_INVALIDARG;

    mVideoFormat.biBitCount = 12;
    mVideoFormat.biClrUsed = 0;
    mVideoFormat.biCompression = MAKEFOURCC('N', 'V', '1', '2');
    mVideoFormat.biHeight = dwHeight;
    mVideoFormat.biPlanes = 1;
    mVideoFormat.biWidth = dwWidth;
    mVideoFormat.biXPelsPerMeter = 0;
    mVideoFormat.biSizeImage = (dwWidth * dwHeight * 3 / 2);
    mVideoFormat.biSize = 40;

    DWORD original4CC = mVideoFormat.biCompression;

    /**************************************************************************
    * construct the media type from the BitMapInfoHeader                     *
    **************************************************************************/
    hr = MFCreateVideoMediaTypeFromBitMapInfoHeaderEx(
        &mVideoFormat,                // video info header to convert
        mVideoFormat.biSize,          // size of the header structure
        1,                            // pixel aspect ratio X
        1,                            // pixel aspect ratio Y
        MFVideoInterlace_Progressive, // interlace mode
        0,                            // video flags
        mFps,                         // FPS numerator
        1,                            // FPS denominator
        0,                            // max bitrate, set to 0 if unknown
        &pType);                      // result - out
    RETURNIFFAILED(hr);

    //TODO necessary?
    hr = pType->SetUINT32(MF_MT_ORIGINAL_4CC, original4CC);
    RETURNIFFAILED(hr);

    // Format to NV12
    GUID subType = MFVideoFormat_NV12;
    hr = pType->SetGUID(MF_MT_SUBTYPE, subType);
    RETURNIFFAILED(hr);

    hr = MFSetAttributeSize(pType, MF_MT_FRAME_SIZE, dwWidth, dwHeight);
    RETURNIFFAILED(hr);

    /**************************************************************************
    * store any extra video information data in the media type               *
    **************************************************************************/
    if (pUserData != NULL && dwUserData > 0)
    {
        hr = pType->SetBlob(MF_MT_USER_DATA, pUserData, dwUserData);
        RETURNIFFAILED(hr);
    }

    mPVideoType = pType;

    return hr;
}

HRESULT VCEEncoder::createH264VideoType(
    IMFMediaType** encodedVideoType, IMFMediaType* sourceVideoType)
{
    uint32_t srcWidth, srcHeight;
    if (nullptr == encodedVideoType)
    {
        return E_POINTER;
    }

    HRESULT hr;

    /**************************************************************************
    *  Get width and height from the source media type                       *
    **************************************************************************/
    hr = MFGetAttributeSize(sourceVideoType, MF_MT_FRAME_SIZE, &srcWidth,
        &srcHeight);
    LOGIFFAILED(mLogFile, hr, "Failed to get attributes @ %s %d \n", \
        __FILE__, __LINE__);
    /**************************************************************************
    *  Create video type for storing the commpressed bit stream              *
    **************************************************************************/
    CComPtr<IMFMediaType> videoType;
    hr = mBuilder->createVideoType(&videoType, MFVideoFormat_H264,
        FALSE, FALSE, nullptr, nullptr, srcWidth, srcHeight,
        MFVideoInterlace_Progressive);
    LOGIFFAILED(mLogFile, hr, "Failed to create video type @ %s %d \n", \
        __FILE__, __LINE__);

    /**************************************************************************
    *  Set output bit rate                                                   *
    **************************************************************************/
    hr = videoType->SetUINT32(MF_MT_AVG_BITRATE,
        mPConfigCtrl.vidParams.meanBitrate);
    LOGIFFAILED(mLogFile, hr, "Failed to set attributes @ %s %d \n", \
        __FILE__, __LINE__);

    /**************************************************************************
    *  Set encoding profile                                                  *
    **************************************************************************/
    hr = videoType->SetUINT32(MF_MT_MPEG2_PROFILE,
        mPConfigCtrl.vidParams.compressionStandard);
    LOGIFFAILED(mLogFile, hr, "Failed to set attributes @ %s %d \n", \
        __FILE__, __LINE__);

    UINT32 numerator;
    UINT32 denominator;
    hr = MFGetAttributeRatio(sourceVideoType, MF_MT_FRAME_RATE, &numerator,
        &denominator);
    LOGIFFAILED(mLogFile, hr, "Failed to get attributes @ %s %d \n", \
        __FILE__, __LINE__);
    MFSetAttributeRatio(videoType, MF_MT_FRAME_RATE, numerator, denominator);
    LOGIFFAILED(mLogFile, hr, "Failed to set attributes @ %s %d \n", \
        __FILE__, __LINE__);

    /**************************************************************************
    * get pixel aspect ratio from source media type and set the same to      *
    * output video type                                                      *
    **************************************************************************/
    hr = MFGetAttributeRatio(sourceVideoType, MF_MT_PIXEL_ASPECT_RATIO,
        &numerator, &denominator);
    LOGIFFAILED(mLogFile, hr, "Failed to get attributes @ %s %d \n", \
        __FILE__, __LINE__);

    MFSetAttributeRatio(videoType, MF_MT_PIXEL_ASPECT_RATIO, numerator,
        denominator);
    LOGIFFAILED(mLogFile, hr, "Failed to set attributes @ %s %d \n", \
        __FILE__, __LINE__);

    *encodedVideoType = videoType.Detach();

    return S_OK;
}
