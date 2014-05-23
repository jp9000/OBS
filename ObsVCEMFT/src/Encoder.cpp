#include "stdafx.h"
#include "Encoder.h"
#include <../libmfx/include/msdk/include/mfxstructures.h>

extern "C"
{
#define _STDINT_H
#include <../x264/x264.h>
#undef _STDINT_H
}

VCEEncoder::VCEEncoder(int fps, int width, int height, int quality, const TCHAR* preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR)
: mBuilder(NULL)
, mPVideoType(NULL)
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
, mBufferSize(bufferSize)
, mUse444(bUse444)
, mUseCFR(bUseCFR)
, mFirstFrame(true)
, mHdrPacket(NULL)
, mHdrSize(0)
{
    mInBuffSize = mWidth * mHeight * 3 / 2;
    mBuilder = new msdk_CMftBuilder();
    memset(mInputBuffers, 0, sizeof(mInputBuffers));
}

VCEEncoder::~VCEEncoder()
{
    if (mBuilder)
        delete mBuilder;

    //FIXME _ASSERTE(_pFirstBlock == pHead); ... wat
    if (mHdrPacket)
     delete [] mHdrPacket;
    mHdrPacket = NULL;

    //For some reason wants explicit Release()
    mEncTrans.Release();
    mEventGen.Release();

    while (!mInputQueue.empty())
        mInputQueue.pop();

    for (int i = 0; i < MAX_INPUT_SURFACE; i++)
    {
        if (mInputBuffers[i].pBufferPtr)
        {
            mInputBuffers[i].pBufferPtr = NULL;
            mInputBuffers[i].pBuffer->Unlock();
            mInputBuffers[i].pBuffer.Release();
        }
    }

    if (mComDealloc)
        delete mComDealloc;
    if (mMFDealloc)
        delete mMFDealloc;
}

HRESULT VCEEncoder::Stop()
{
    if (!mAlive)
        return E_UNEXPECTED;

    HRESULT hr = mEncTrans->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
    hr = mEncTrans->ProcessMessage(MFT_MESSAGE_NOTIFY_END_STREAMING, 0);
    //hr = mEncTrans->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
    //TODO _DRAIN the pipe
    /*while (ProcessOutput(blah) != MF_E_TRANSFORM_NEED_MORE_INPUT)
    {
    }*/
    return hr;
}

HRESULT VCEEncoder::Init()
{
    HRESULT hr;
    if (mAlive)
        return S_OK;

    //hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    //if (S_OK != hr && S_FALSE != hr /* already inited */)
    //{
    //    VCELog(TEXT("Com initialization failed with %d"), hr);
    //    return (ERR_COM_INITILIZATION_FAILED | ERR_INITILIZATION_FAILED);
    //}
    //mComDealloc = new FunctionDeallocator< void(__stdcall*)(void) >(CoUninitialize);

    hr = MFStartup(MF_VERSION);
    if (S_OK != hr)
    {
        VCELog(TEXT("MFStartup failed."));
        return (ERR_MFT_INITILIZATION_FAILED | ERR_INITILIZATION_FAILED);
    }
    mMFDealloc = new FunctionDeallocator< HRESULT(__stdcall*)(void) >(MFShutdown);

    //Just in case
    memset(&mPConfigCtrl, 0, sizeof(mPConfigCtrl));

    int bitRateWindow = 50;
    mPConfigCtrl.commonParams.useInterop = 0;
    mPConfigCtrl.commonParams.useSWCodec = 0;
    mPConfigCtrl.height = mHeight;
    mPConfigCtrl.width = mWidth;
    mPConfigCtrl.vidParams.commonQuality = mQuality * 10;
    mPConfigCtrl.vidParams.enableCabac = 1;
    mPConfigCtrl.vidParams.bufSize = mBufferSize * 1000;
    mPConfigCtrl.vidParams.meanBitrate = mMaxBitrate * (1000 - bitRateWindow);
    mPConfigCtrl.vidParams.maxBitrate = mMaxBitrate * (1000 + bitRateWindow);
    mPConfigCtrl.vidParams.idrPeriod = (mKeyint > 0 ? mKeyint : 2) * mFps;
    mPConfigCtrl.vidParams.gopSize = mFps;
    mPConfigCtrl.vidParams.qualityVsSpeed = 50;
    mPConfigCtrl.vidParams.compressionStandard = eAVEncH264VProfile_High;
    mPConfigCtrl.vidParams.numBFrames = 3;
    mPConfigCtrl.vidParams.lowLatencyMode = 0;

    if (mUseCBR)
        mPConfigCtrl.vidParams.rateControlMethod = eAVEncCommonRateControlMode_CBR;
    else if (mUseCFR)
        mPConfigCtrl.vidParams.rateControlMethod = eAVEncCommonRateControlMode_Quality;
    else
        mPConfigCtrl.vidParams.rateControlMethod = eAVEncCommonRateControlMode_PeakConstrainedVBR;

    bool useDx11 = mBuilder->isDxgiSupported();

    hr = createVideoMediaType(NULL, 0, mWidth, mHeight);// , &mPVideoType);
    RETURNIFFAILED(hr);

    CComPtr<IMFMediaType> h264VideoType;
    hr = createH264VideoType(&h264VideoType, mPVideoType);
    RETURNIFFAILED(hr);

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

    // Create H264 encoder transform
    hr = mBuilder->createVideoEncoderNode(h264VideoType,
        mPVideoType, &mEncTrans, deviceManagerPtr,
        &mPConfigCtrl.vidParams, mPConfigCtrl.commonParams.useSWCodec);
    LOGIFFAILED(mLogFile, hr, "Failed to create encoder transform");

    //Usually should check for stream IDs. Assuming static stream count with VCE.
    //{
    //    DWORD inCnt, outCnt;
    //    mEncTrans->GetStreamCount(&inCnt, &outCnt);
    //    std::cout << "in:" << inCnt << " out:" << outCnt << std::endl;

    //    if (hr == E_NOTIMPL) //most likely; static stream count with IDs from 0 to n-1
    //    {
    //        hr = mEncTrans->GetStreamIDs(1, &inCnt, 1, &outCnt);
    //        std::cout << " in stream:" << inCnt << " out stream:" << outCnt << std::endl;
    //    }
    //}

    hr = mEncTrans->QueryInterface(IID_PPV_ARGS(&mEventGen));
    LOGIFFAILED(stderr, hr, "QueryInterface for MediaEventGenerator failed.");
    //Start async. event processing
    /*hr = mEventGen->BeginGetEvent(this, nullptr);
    LOGIFFAILED(stderr, hr, "BeginGetEvent failed.");*/

    /// Get it rolling
    hr = mEncTrans->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    //hr = mEventGen->QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, NULL);

    mAlive = true;
    return hr;
}

//void VCEncoder::EventLoop()
//{
//    HRESULT hr = S_OK;
//    CComPtr<IMFMediaEvent> pEvent;
//    MediaEventType type;
//
//    while (!mShutdown)
//    {
//        hr = mEventGen->GetEvent(0, &pEvent);
//        if (SUCCEEDED(hr))
//        {
//            pEvent->GetType(&type);
//            pEvent.Release(); //Probably
//            hr = ProcessEvent(type);
//            LOGIFFAILED(stderr, hr, "ProcessEvent failed.");
//        }
//    }
//}

//TODO Probably unnecessary or just set output to precreated h264 mediatype again
//Stream format got changed
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

HRESULT VCEEncoder::ProcessEvent(MediaEventType mediaEventType)
{
    HRESULT hr = S_OK;
    switch (mediaEventType)
    {
    case METransformNeedInput:
        std::cout << "METransformNeedInput" << std::endl;
        hr = ProcessInput();
        break;

    case METransformHaveOutput:
        std::cout << "METransformHaveOutput" << std::endl;
        hr = ProcessOutput(NULL);
        break;

    case METransformDrainComplete:
        std::cout << "METransformDrainComplete" << std::endl;
        break;

    default:
        std::cout << "Event: " << mediaEventType << std::endl;
        break;
    }
    return hr;
}

HRESULT VCEEncoder::ProcessInput()
{
    HRESULT hr;
    UINT64 dur = 0;
    CComPtr<IMFSample> pSample;
    BYTE *ppBuffer = NULL;

    DWORD outLen = 0, len = 0;

    if (mInputQueue.empty())
        return MF_E_TRANSFORM_NEED_MORE_INPUT;

    profileIn("ProcessInput")
    MFCreateSample(&pSample);

    InputBuffer *inBuf = mInputQueue.front();
    mInputQueue.pop();

    if (inBuf->pBufferPtr)
        inBuf->pBuffer->Unlock();

    hr = pSample->AddBuffer(inBuf->pBuffer);

    MFFrameRateToAverageTimePerFrame(mFps, 1, &dur);
    pSample->SetSampleDuration((LONGLONG)dur);
    pSample->SetSampleTime(inBuf->timestamp * MS_TO_100NS);

    hr = mEncTrans->ProcessInput(0, pSample, 0);
    inBuf->locked = false;
    inBuf->pBuffer->Lock(&(inBuf->pBufferPtr), &len, &outLen);
    LOGIFFAILED(stderr, hr, "ProcessInput failed.");
    profileOut
    return hr;
}

HRESULT VCEEncoder::ProcessOutput(OutputBuffer *outBuffer)
{
    HRESULT hr;
    UINT64 dur = 0;
    CComPtr<IMFSample> pSample;
    CComPtr<IMFMediaBuffer> pBuffer;
    BYTE *ppBuffer = NULL;

    MFT_OUTPUT_DATA_BUFFER out[1];
    DWORD status = 0, len, currLen, maxLen;

    MFT_OUTPUT_STREAM_INFO si;

    profileIn("ProcessOutput")
    mEncTrans->GetOutputStreamInfo(0, &si);

    //TODO Handle other cases too, maybe
    if (
        !(HASFLAG(si.dwFlags, MFT_OUTPUT_STREAM_WHOLE_SAMPLES)
        && HASFLAG(si.dwFlags, MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER)
        && HASFLAG(si.dwFlags, MFT_OUTPUT_STREAM_PROVIDES_SAMPLES))
        )
    {
        return E_UNEXPECTED;
    }

    hr = mEncTrans->ProcessOutput(0, 1, out, &status);
    if (outBuffer && hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
        return hr;
    LOGIFFAILED(stderr, hr, "ProcessOutput failed.");

    if (HASFLAG(out[0].dwStatus, MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE))
    {
        return OutputFormatChange();
        //return S_OK; //If async then return S_OK?
    }

    out[0].pSample->GetTotalLength(&len);
    out[0].pSample->GetSampleTime((LONGLONG*)&dur);
    
    out[0].pSample->ConvertToContiguousBuffer(&pBuffer);
    hr = pBuffer->Lock(&ppBuffer, &maxLen, &currLen);
    if (SUCCEEDED(hr))
    {
        //FIXME Do something
        if (outBuffer)
        {
            outBuffer->timestamp = dur / MS_TO_100NS;
            outBuffer->size = currLen;
            outBuffer->pBuffer = malloc(currLen);
            memcpy(outBuffer->pBuffer, ppBuffer, currLen);
        }
    }
    hr = pBuffer->Unlock();

    out[0].pSample->Release();
    profileOut

    return hr;
}

bool VCEEncoder::Encode(LPVOID picIn, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD timestamp)
{
    HRESULT hr = S_OK;
    CComCritSecLock<CComMultiThreadModel::AutoCriticalSection> \
        lock(mStateLock, true);

    packets.Clear();
    packetTypes.Clear();

    if (!picIn)
    {
        Stop(); //TODO right?
        //FIXME Drain the pipes!
        return true;
    }

    profileIn("Encode")
    mfxFrameSurface1 *inputSurface = (mfxFrameSurface1*)picIn;
    mfxFrameData &data = inputSurface->Data;
    assert(data.MemId);

    unsigned int idx = (unsigned int)data.MemId - 1;

    //well, need a pointer to vector element :P
    InputBuffer *inBuffer = &(mInputBuffers[idx]);
    if (inBuffer->pBufferPtr)
        inBuffer->pBuffer->Unlock();
    inBuffer->locked = true;
    inBuffer->timestamp = timestamp;
    mInputQueue.push(inBuffer);

    /*if (!mOutputQueue.empty())
    {
        assert(0);
    }*/

    OutputBuffer buffer = { 0 };
    hr = ProcessInput();
    hr = ProcessOutput(&buffer);
    if (SUCCEEDED(hr))
    {
        ProcessBitstream(buffer, packets, packetTypes);
        if (mHdrPacket == NULL)
        {
            mHdrSize = buffer.size;// MIN(buffer.size, 128);
            mHdrPacket = new uint8_t[mHdrSize];
            memcpy(mHdrPacket, buffer.pBuffer, mHdrSize);
        }
        //OSDebugOut(TEXT("Out buf: 0x%08x"), ((int*)buffer.pBuffer)[0]);
        //OSDebugOut(TEXT(" 0x%08x\n"), ((int*)buffer.pBuffer)[1]);
        free(buffer.pBuffer);
        return true;
    }
    profileOut
    return false;
}

void VCEEncoder::ProcessBitstream(OutputBuffer buff, List<DataPacket> &packets, List<PacketType> &packetTypes)
{
    profileIn("ProcessBitstream")
    encodeData.Clear();

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

    //uint64_t dts = timestamp;
    //uint64_t out_pts = timestamp;

    //FIXME VCE outputs I/P frames thus pts and dts can be the same? timeOffset needed with B-frames?
    int32_t timeOffset = 0;// int(out_pts - dts);
    //timeOffset += frameShift;

    /*if (nalNum && timeOffset < 0)
    {
        frameShift -= timeOffset;
        timeOffset = 0;
    }*/

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
        if (mInputBuffers[i].locked || mInputBuffers[i].pBufferPtr)
            continue;

        InputBuffer buffer;
        hr = MFCreateMemoryBuffer(mInBuffSize, &(buffer.pBuffer));
        if (SUCCEEDED(hr))
        {
            buffer.pBuffer->Lock(&(buffer.pBufferPtr), &maxLen, &curLen);
            if (maxLen < mInBuffSize) //Possible?
            {
                VCELog(TEXT("Buffer max length smaller than asked: %d"), maxLen);
                buffer.pBuffer->Unlock();
                buffer.pBuffer.Release();
            }
            else
            {
                buff->Pitch = mWidth;
                buff->Y = (mfxU8*)buffer.pBufferPtr;
                buff->UV = buff->Y + (mHeight * buff->Pitch);
                buff->MemId = mfxMemId(i + 1);

                mInputBuffers[i] = buffer;
                mInputBuffers[i].locked = false;
                return;
            }
        }
    }

    VCELog(TEXT("Max number of input buffers reached."));
}

void VCEEncoder::GetHeaders(DataPacket &packet)
{
    uint32_t outSize = mHdrSize;
    if (!mHdrPacket)
    {
        VCELog(TEXT("No header packet yet."));
        //Garbage, but atleast OBS doesn't crash.
        headerPacket.Clear();
        headerPacket.SetSize(128);
        packet.size = headerPacket.Num();
        packet.lpPacket = headerPacket.Array();
        return;
    }

    uint8_t *start = mHdrPacket;
    uint8_t *end = start + mHdrSize;
    const static uint8_t start_seq[] = { 0, 0, 1 };
    start = std::search(start, end, start_seq, start_seq + 3);

    x264_nal_t nalSPS, nalPPS;

    //May have NAL_AUD, NAL_SPS, NAL_PPS, NAL_SLICE_IDR
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

    headerPacket.Clear();
    BufferOutputSerializer headerOut(headerPacket);

    //While OVE has 4 byte start codes, MFT seems to give 3 byte ones
    headerOut.OutputByte(0x17);
    headerOut.OutputByte(0);
    headerOut.OutputByte(0);
    headerOut.OutputByte(0);
    headerOut.OutputByte(0);
    headerOut.OutputByte(1);
    headerOut.Serialize(nalSPS.p_payload + 4, 3);
    headerOut.OutputByte(0xff);
    headerOut.OutputByte(0xe1);
    headerOut.OutputWord(fastHtons(nalSPS.i_payload - 3));
    headerOut.Serialize(nalSPS.p_payload + 3, nalSPS.i_payload - 3);

    headerOut.OutputByte(1);
    headerOut.OutputWord(fastHtons(nalPPS.i_payload - 3));
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
    VCELog(TEXT("Keyframe requested"));
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
    return false;
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

HRESULT VCEEncoder::GetParameters(DWORD *pdwFlags, DWORD *pdwQueue)
{
    /*
    *pdwQueue = MFASYNC_CALLBACK_QUEUE_STANDARD;
    *pdwFlags = MFASYNC_BLOCKING_CALLBACK;*/
    return E_NOTIMPL;
}

HRESULT VCEEncoder::Invoke(IMFAsyncResult *pAsyncResult)
{
    HRESULT hr;

    CComCritSecLock<CComMultiThreadModel::AutoCriticalSection> \
        lock(mStateLock, true);

    CComPtr<IMFAsyncResult> asyncResult = pAsyncResult;
    CComPtr<IMFMediaEvent> event;
    MediaEventType mediaEventType;

    //Maybe get mEventGen from IMFAsyncResult::GetState
    hr = mEventGen->EndGetEvent(asyncResult, &event);
    hr = event->GetType(&mediaEventType);

    RETURNIFFAILED(hr);

    hr = ProcessEvent(mediaEventType);

    hr = mEventGen->BeginGetEvent(this, nullptr);

    return hr;
}

HRESULT VCEEncoder::createVideoMediaType(BYTE* pUserData, DWORD dwUserData, DWORD dwWidth, DWORD dwHeight)
{
    HRESULT hr = S_OK;
    CComPtr<IMFVideoMediaType> pType;

    //if (nullptr == ppType)
    //    return E_INVALIDARG;

    mVideoFormat.biBitCount = 12;
    mVideoFormat.biBitCount = 0;
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
    uint32 srcWidth, srcHeight;
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
