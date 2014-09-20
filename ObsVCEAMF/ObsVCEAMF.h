#pragma once
#include "stdafx.h"
#include <bcrypt.h>
#include "OBS-min.h"
#include <amf\components\VideoEncoderVCE.h>
#include "amfhelpers\inc\DeviceDX9.h"
#include "amfhelpers\inc\DeviceDX11.h"
#include "amfhelpers\inc\DeviceOpenCL.h"
#include "Types.h"
#define VCELog(...) Log(__VA_ARGS__)
#define AppConfig (*VCEAppConfig)

#define MAX_INPUT_BUFFERS 3
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
	VCEEncoder(int fps, int width, int height, int quality, const TCHAR* preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR, ID3D10Device *d3d10device);
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
		return VideoEncoder_D3D10Interop;
	}
	void ConvertD3D10(ID3D10Texture2D *d3dtex, void *data, void **state);

private:
	void ProcessBitstream(amf::AMFBufferPtr &buff, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD timestamp);
	bool VCEEncoder::RequestBuffersCL(LPVOID buffers);
	bool VCEEncoder::RequestBuffersDX11(LPVOID buffers);
	bool VCEEncoder::RequestBuffersHost(LPVOID buffers);
	void ClearInputBuffer(int idx);

	amf::AMFContextPtr   mContext;
	amf::AMFComponentPtr mEncoder;
	DeviceDX9    mDX9Device;
	DeviceDX11   mDX11Device;
	DeviceOpenCL mOCLDevice;
	ATL::CComPtr<ID3D10Device> mD3D10device;

	cl_command_queue mCmdQueue;
	cl_context       mCLContext;
	cl_platform_id   mPlatformId;
	cl_device_id     mClDevice;
	std::vector<cl_mem> mCLMemObjs;

	HANDLE mFrameMutex;
	bool mAlive;
	bool mEOF;
	bool mIsWin8OrGreater;
	uint8_t    *mHdrPacket;
	size_t     mHdrSize;
	uint32_t   mInBuffSize;
	uint32_t   mCurrFrame;
	uint32_t   mIDRPeriod;
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

	List<BYTE> headerPacket, seiData;
	const TCHAR*    mPreset;
	bool     mUse444;
	ColorDescription mColorDesc;
	bool     mFirstFrame;
	int32_t  frameShift;

	std::queue<OutputList*> mOutputQueue;
	InputBuffer             mInputBuffers[MAX_INPUT_BUFFERS];

};
