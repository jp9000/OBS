#pragma once
#include "stdafx.h"

//TODO Some optimal count
#define MAX_INPUT_SURFACE      8
//Sample timestamp is in in 100-nanosecond units
#define SEC_TO_100NS     10000000
#define MS_TO_100NS      10000

#define VCELog(...) Log(__VA_ARGS__)
#define AppConfig (*VCEAppConfig)
#define HASFLAG(x, y) (((x) & (y)) == (y))

//Nifty class from DisplayPi
template <typename K>
class FunctionDeallocator
{
public:
    FunctionDeallocator(K pDeallocator) : m_pDeallocator(pDeallocator)
    {
    }
    virtual ~FunctionDeallocator()
    {
        m_pDeallocator();
    }
private:
    K m_pDeallocator;
};

typedef struct ConfigCtrl
{
    DWORD width;
    DWORD height;
    AMF_MFT_VIDEOENCODERPARAMS vidParams;
    AMF_MFT_COMMONPARAMS commonParams;
} ConfigCtrl, far * pConfig;

typedef struct InputBuffer
{
    CComPtr<IMFMediaBuffer> pBuffer;
    uint8_t *pBufferPtr;
    bool locked;
    DWORD timestamp;
} InputBuffer;

//TODO Preallocate maybe
typedef struct OutputBuffer
{
    void *pBuffer;
    size_t size;
    uint64_t timestamp;
} OutputBuffer;

class VCEEncoder : public VideoEncoder, public IMFAsyncCallback
{
public:
    VCEEncoder(int fps, int width, int height, int quality, const TCHAR* preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR);
    ~VCEEncoder();

    HRESULT Init();
    HRESULT Stop();
    bool Encode(LPVOID picIn, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD timestamp);
    void RequestBuffers(LPVOID buffers);
    int  GetBitRate() const;
    //HW MFT must support dynamic stream changes, so yes?
    bool DynamicBitrateSupported() const;
    bool SetBitRate(DWORD maxBitrate, DWORD bufferSize);
    void GetHeaders(DataPacket &packet);
    void GetSEI(DataPacket &packet);
    void RequestKeyframe();
    String GetInfoString() const { return String(); }
    bool isQSV() { return true; }
    int GetBufferedFrames() { return mOutputQueue.size(); }
    bool HasBufferedFrames() { return mOutputQueue.size() > 0; }

    HRESULT STDMETHODCALLTYPE GetParameters(DWORD *pdwFlags, DWORD *pdwQueue);
    HRESULT STDMETHODCALLTYPE Invoke(IMFAsyncResult *pAsyncResult);

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv)
    {
        if (riid == IID_IMFAsyncCallback)
        {
            *ppv = static_cast<IMFAsyncCallback*>(this);
            AddRef();
            return S_OK;
        }
        if (riid == IID_IUnknown)
        {
            *ppv = static_cast<IUnknown*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef()
    {
        return InterlockedIncrement(&mRefCount);
    }

    ULONG STDMETHODCALLTYPE Release()
    {
        unsigned long refCount = InterlockedDecrement(&mRefCount);
        if (0 == refCount)
        {
            delete this;
        }
        return refCount;
    };

private:
    //Create input media type
    HRESULT createVideoMediaType(BYTE* pUserData, DWORD dwUserData, DWORD dwWidth, DWORD dwHeight);
    //Create output media type
    HRESULT createH264VideoType(IMFMediaType** encodedVideoType, IMFMediaType* sourceVideoType);
    HRESULT ProcessInput();
    HRESULT ProcessOutput(OutputBuffer *outBuffer);
    HRESULT ProcessEvent(MediaEventType mediaEventType);
    HRESULT OutputFormatChange();
    void ProcessBitstream(OutputBuffer, List<DataPacket> &packets, List<PacketType> &packetTypes);
    void DrainOutput(List<DataPacket> &packets, List<PacketType> &packetTypes);

    msdk_CMftBuilder *mBuilder;
    CComPtr<IMFMediaType> mPVideoType;
    CComPtr<IMFTransform> mEncTrans;
    CComPtr<IMFMediaEventGenerator> mEventGen;

    FunctionDeallocator< void(__stdcall*)(void) > *mComDealloc;
    FunctionDeallocator< HRESULT(__stdcall*)(void) > *mMFDealloc;
    CComMultiThreadModel::AutoCriticalSection mStateLock;

    BITMAPINFOHEADER mVideoFormat;
    FILE* mLogFile; //TODO remove
    ConfigCtrl mPConfigCtrl;
    bool mAlive;
    ULONG mRefCount;

    //TODO CComPtr it up?
    std::queue<CComPtr<IMFSample> > mOutputQueue;
    std::queue<InputBuffer*>        mInputQueue;
    InputBuffer                     mInputBuffers[MAX_INPUT_SURFACE];

    List<BYTE> encodeData, headerPacket, seiData;
    uint8_t    *mHdrPacket;
    size_t     mHdrSize;
    uint32_t   mInBuffSize;
    bool       mReqKeyframe;
    bool       mUseCFR;
    bool       mUseCBR;
    int32_t    mKeyint;
    int32_t    mMaxBitrate;
    int32_t    mBufferSize;
    int32_t    mQuality;
    int32_t    mFps;
    int32_t    mWidth;
    int32_t    mHeight;

    const TCHAR*    mPreset;
    bool     mUse444; //Max VCE 2.0 can do is 422 probably
    ColorDescription mColorDesc;
    bool     mFirstFrame;
};