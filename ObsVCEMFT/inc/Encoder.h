#pragma once
#include "stdafx.h"

//TODO Some optimal count
#define MAX_INPUT_SURFACE      8
//Sample timestamp is in 100-nanosecond units
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
    LPBYTE pBufferPtr;
    //size_t size;
    bool locked;
    DWORD timestamp;
} InputBuffer;

typedef struct OutputBuffer
{
    void *pBuffer;
    size_t size;
    uint64_t timestamp;
} OutputBuffer;

typedef struct OutputList
{
    List<BYTE> pBuffer;
    uint64_t timestamp;
} OutputList;

class VCEEncoder : public VideoEncoder
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

private:
    //Create input media type
    HRESULT createVideoMediaType(BYTE* pUserData, DWORD dwUserData, DWORD dwWidth, DWORD dwHeight);
    //Create output media type
    HRESULT createH264VideoType(IMFMediaType** encodedVideoType, IMFMediaType* sourceVideoType);
    HRESULT ProcessInput();
    HRESULT ProcessOutput(List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD timestamp);
    HRESULT OutputFormatChange();
    void ProcessBitstream(OutputBuffer&, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD timestamp);

    //Just loop and call ProcessOutput if any samples
    void DrainOutput(List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD timestamp);

    msdk_CMftBuilder *mBuilder;
    CComPtr<IMFMediaType> mPVideoType;
    CComPtr<IMFTransform> mEncTrans;
    CComPtr<IMFSample> mSample;
    IUnknown *mIDevMgr;

    FunctionDeallocator< void(__stdcall*)(void) > *mComDealloc;
    FunctionDeallocator< HRESULT(__stdcall*)(void) > *mMFDealloc;
    CComMultiThreadModel::AutoCriticalSection mStateLock;

    BITMAPINFOHEADER mVideoFormat;
    FILE* mLogFile; //TODO remove
    ConfigCtrl mPConfigCtrl;
    bool mAlive;
    ULONG mRefCount;

    std::queue<OutputList*>  mOutputQueue;
    std::queue<InputBuffer*> mInputQueue;
    InputBuffer              mInputBuffers[MAX_INPUT_SURFACE];

    List<BYTE> headerPacket, seiData;
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
    UINT64     mFrameDur;

    const TCHAR*    mPreset;
    bool     mUse444; //How..
    ColorDescription mColorDesc;
    bool     mFirstFrame;
    int32_t  frameShift;
};