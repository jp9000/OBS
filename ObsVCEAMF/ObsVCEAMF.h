#pragma once
#include "stdafx.h"
#include <bcrypt.h>
#include "OBS-min.h"
#include <amf\components\VideoEncoderVCECaps.h>
#include <amf\components\ComponentCaps.h>
#include <amf\components\VideoEncoderVCE.h>
#ifdef AMF_CORE_STATIC
#include "DynAMF.h"
#else
extern decltype(&AMFCreateContext) pAMFCreateContext;
extern decltype(&AMFCreateComponent) pAMFCreateComponent;
#endif
#include "amfhelpers\inc\DeviceDX9.h"
#include "amfhelpers\inc\DeviceDX11.h"
#include "amfhelpers\inc\DeviceOpenCL.h"
#include "Types.h"

#define VCELog(...) Log(__VA_ARGS__)
#define AppConfig (*VCEAppConfig)

#define MAX_INPUT_BUFFERS 20
//Sample timestamp is in 100-nanosecond units
#define SEC_TO_100NS     10000000
#define MS_TO_100NS      10000
#define AMF_INFINITE     (0xFFFFFFFF) // Infinite ulTimeout

#define LOGIFFAILED(res, ...) \
	do{if(res != AMF_OK) Log(__VA_ARGS__);}while(0)
#define RETURNIFFAILED(res, ...) \
	do{if(res != AMF_OK) {Log(__VA_ARGS__); return false;}}while(0)
#define HRETURNIFFAILED(hresult, msg) \
	do{if (FAILED(hresult))\
	{\
		VCELog(TEXT(msg) TEXT(" HRESULT: %08X"), hresult);\
		return false;\
	}}while(0)

struct VideoPacket
{
	List<BYTE> Packet;
	inline void FreeData() { Packet.Clear(); }
};

struct AMF_SETTINGS
{
	bool enforceHRD;
	bool fillerData;
	AMF_VIDEO_ENCODER_QUALITY_PRESET_ENUM quality;
	AMF_VIDEO_ENCODER_PROFILE_ENUM profile;
	amf_int64 bitrate;
};

//from nvenc
struct OSMutexLocker
{
	HANDLE& h;
	bool enabled;

	OSMutexLocker(HANDLE &h) : h(h), enabled(true) { OSEnterMutex(h); }
	~OSMutexLocker() { if (enabled) OSLeaveMutex(h); }
	OSMutexLocker(OSMutexLocker &&other) : h(other.h), enabled(other.enabled) { other.enabled = false; }
};

amf_pts AMF_STD_CALL amf_high_precision_clock()
{
	static int state = 0;
	static LARGE_INTEGER Frequency;
	if (state == 0)
	{
		if (QueryPerformanceFrequency(&Frequency))
		{
			state = 1;
		}
		else
		{
			state = 2;
		}
	}
	if (state == 1)
	{
		LARGE_INTEGER PerformanceCount;
		if (QueryPerformanceCounter(&PerformanceCount))
		{
			return static_cast<amf_pts>(PerformanceCount.QuadPart * 10000000.0 / Frequency.QuadPart);
		}
	}
	return GetTickCount() * 10;
}

void AMF_STD_CALL amf_increase_timer_precision()
{
	typedef NTSTATUS(CALLBACK * NTSETTIMERRESOLUTION)(IN ULONG DesiredTime, IN BOOLEAN SetResolution, OUT PULONG ActualTime);
	typedef NTSTATUS(CALLBACK * NTQUERYTIMERRESOLUTION)(OUT PULONG MaximumTime, OUT PULONG MinimumTime, OUT PULONG CurrentTime);

	HINSTANCE hNtDll = LoadLibrary(L"NTDLL.dll");
	if (hNtDll != NULL)
	{
		ULONG MinimumResolution = 0;
		ULONG MaximumResolution = 0;
		ULONG ActualResolution = 0;

		NTQUERYTIMERRESOLUTION NtQueryTimerResolution = (NTQUERYTIMERRESOLUTION)GetProcAddress(hNtDll, "NtQueryTimerResolution");
		NTSETTIMERRESOLUTION NtSetTimerResolution = (NTSETTIMERRESOLUTION)GetProcAddress(hNtDll, "NtSetTimerResolution");

		if (NtQueryTimerResolution != NULL && NtSetTimerResolution != NULL)
		{
			NtQueryTimerResolution(&MinimumResolution, &MaximumResolution, &ActualResolution);
			if (MaximumResolution != 0)
			{
				NtSetTimerResolution(MaximumResolution, TRUE, &ActualResolution);
				NtQueryTimerResolution(&MinimumResolution, &MaximumResolution, &ActualResolution);

				// if call NtQueryTimerResolution() again it will return the same values but precision is actually increased
			}
		}
		FreeLibrary(hNtDll);
	}
}

class VCEEncoder : public VideoEncoder {
public:
	VCEEncoder(int fps, int width, int height, int quality, const TCHAR* preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR, ID3D11Device *d3ddevice);
	~VCEEncoder();

	bool Init();
	bool Stop();
	bool Encode(LPVOID picIn, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD timestamp, DWORD &out_pts);
	void RequestBuffers(LPVOID buffers);
	int  GetBitRate() const;
	bool DynamicBitrateSupported() const { return true; }
	bool SetBitRate(DWORD maxBitrate, DWORD bufferSize);
	void GetHeaders(DataPacket &packet);
	void GetSEI(DataPacket &packet);
	void RequestKeyframe();
	String GetInfoString() const { return String(); }
	bool isQSV() { return true; }
	int GetBufferedFrames() { return mEOF ? 0 : 1; /*(int)mOutputQueue.size();*/ }
	bool HasBufferedFrames() { return !mEOF;/* mOutputQueue.size() > 0;*/ }
	VideoEncoder_Features GetFeatures()
	{
		if(mCanInterop)
			return VideoEncoder_D3D11Interop;
		return VideoEncoder_Unknown;
	}
	void ConvertD3D11(ID3D11Texture2D *d3dtex, void *data, void **state);

private:
	void ProcessBitstream(amf::AMFBufferPtr &buff/*, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD timestamp*/);
	void CopyOutput(List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD &out_pts);
	bool RequestBuffersCL(LPVOID buffers);
	bool RequestBuffersDX11(LPVOID buffers);
	bool RequestBuffersDX9(LPVOID buffers);
	bool RequestBuffersHost(LPVOID buffers);
	void ClearInputBuffer(int idx);
	bool AllocateSurface(InputBuffer &inBuf, mfxFrameData &data);
	void OutputPoll();
	void Submit();
	AMF_RESULT SubmitBuffer(InputBuffer*);
	static DWORD OutputPollThread(VCEEncoder*);
	static DWORD SubmitThread(VCEEncoder* enc);

	amf::AMFContextPtr   mContext;
	amf::AMFComponentPtr mEncoder;
	DeviceDX9    mDX9Device;
	DeviceDX11   mDX11Device;
	DeviceOpenCL mOCLDevice;
	ATL::CComPtr<ID3D11Device> mD3Ddevice;
	//ATL::CComPtr<IDirectXVideoDecoderService> mDxvaService;
	List<VideoPacket> mCurrPackets;
	std::queue<uint64_t> mTSqueue;
	std::queue<amf::AMFSurfacePtr> mSurfaces;

	cl_command_queue mCmdQueue;
	cl_context       mCLContext;
	cl_platform_id   mPlatformId;
	cl_device_id     mClDevice;
	std::vector<cl_mem> mCLMemObjs;

	HANDLE mFrameMutex;
	bool mAlive;
	bool mEOF;
	bool mIsWin8OrGreater;
	// Currently only for D3D11 > OpenCL > AMF
	bool mCanInterop;
	uint32_t   mEngine;
	uint8_t    *mHdrPacket;
	size_t     mHdrSize;
	size_t     mInBuffSize;
	int32_t    mCurrFrame;
	int32_t    mIDRPeriod;
	uint32_t   mIntraMBs;
	int32_t    mLowLatencyKeyInt;
	bool       mReqKeyframe;

	bool       mUseCFR;
	bool       mUseCBR;
	bool       mDiscardFiller;
	int32_t    mKeyint;
	int32_t    mMaxBitrate;
	int32_t    mBufferSize;
	int32_t    mQuality;
	int32_t    mFps;
	int32_t    mWidth;
	int32_t    mHeight;
	size_t     mAlignedSurfaceWidth;
	size_t     mAlignedSurfaceHeight;

	List<BYTE> headerPacket, seiData;
	const TCHAR*     mPreset;
	bool             mUse444;
	ColorDescription mColorDesc;
	bool             mFirstFrame;
	uint64_t         frameShift;
	uint64_t         mPrevTS;
	uint64_t         mOffsetTS;

	HANDLE                  mOutPollThread;
	bool                    mClosing;
	HANDLE                  mSubmitThread;
	HANDLE                  mOutQueueMutex;
	HANDLE                  mSubmitEvent;
	HANDLE                  mSubmitMutex;
	std::queue<OutputList*> mOutputQueue; //< Preallocated(-ish)
	std::queue<OutputList*> mOutputProcessedQueue; //< NAL parsed
	std::queue<OutputList*> mOutputReadyQueue; //< OBS has copied the buffer
	InputBuffer             mInputBuffers[MAX_INPUT_BUFFERS];
	std::queue<InputBuffer*> mSubmitQueue;
	FILE*    mTmpFile;
};
