#include "stdafx.h"
#include <../libmfx/include/msdk/include/mfxstructures.h>

#undef profileIn
#define profileIn(x) {

//#pragma comment(lib, "dxva2")
#ifndef AMF_CORE_STATIC
#if _WIN64
#pragma comment(lib, "amf-core-windesktop64")
#else
#pragma comment(lib, "amf-core-windesktop32")
#endif
#endif

#include "ObsVCEAMF.h"
#include "VCECL.h"

extern "C"
{
#define _STDINT_H
#include <../x264/x264.h>
#undef _STDINT_H
}

/*#ifndef htonl
#define htons(n) (((((uint16_t)(n) & 0xFF)) << 8) | (((uint16_t)(n) & 0xFF00) >> 8))
#define htonl(n) (((((uint32_t)(n) & 0xFF)) << 24) | \
    ((((uint32_t)(n)& 0xFF00)) << 8) | \
    ((((uint32_t)(n)& 0xFF0000)) >> 8) | \
    ((((uint32_t)(n)& 0xFF000000)) >> 24))
#endif*/

ConfigFile **VCEAppConfig = 0;
unsigned int encoderRefs = 0;
bool loggedSupport = false;
const TCHAR* STR_FAILED_TO_SET_PROPERTY = TEXT("Failed to set '%s' property.");

extern "C" __declspec(dllexport) bool __cdecl InitVCEEncoder(ConfigFile **appConfig, VideoEncoder *enc)
{
	VCEAppConfig = appConfig;
	if (enc)
	{
		if (!reinterpret_cast<VCEEncoder*>(enc)->Init())
			return false;
	}
	return true;
}

extern "C" __declspec(dllexport) bool __cdecl CheckVCEHardwareSupport(bool log)
{
	cl_platform_id platform = 0;
	cl_device_id device = 0;

	if (!IsWindowsVistaOrGreater())
		return false;

	if (!getPlatform(platform))
		return false;

	cl_device_type type = CL_DEVICE_TYPE_GPU;
	gpuCheck(platform, &type);
	if (type != CL_DEVICE_TYPE_GPU)
		return false;

#ifdef AMF_CORE_STATIC
	if (!BindAMF())
		return false;

	UnbindAMF();
#endif

	if (!loggedSupport)
	{
		VCELog(TEXT("Possible support for AMD VCE."));
		loggedSupport = true;
	}

	return true;
}

extern "C" __declspec(dllexport) VideoEncoder* __cdecl CreateVCEEncoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR, ID3D10Device *d3d10device)
{
	VCEEncoder *enc = new VCEEncoder(fps, width, height, quality, preset, bUse444, colorDesc, maxBitRate, bufferSize, bUseCFR, d3d10device);

	encoderRefs += 1; //Lock?

	return enc;
}

void encoderRefDec()
{
	assert(encoderRefs);

	encoderRefs -= 1;

	if (encoderRefs == 0)
	{ }
}

static inline void PlaneCopy(const amf_uint8 *src, amf_int32 srcStride, amf_int32 srcHeight, amf_uint8 *dst, amf_int32 dstStride, amf_int32 dstHeight)
{
	amf_int32 minHeight = MIN(srcHeight, dstHeight);
	if (srcStride == dstStride)
	{
		memcpy(dst, src, minHeight * srcStride);
	}
	else
	{
		int minStride = MIN(srcStride, dstStride);
		for (int y = 0; y < minHeight; y++)
		{
			memcpy(dst + dstStride*y, src + srcStride*y, minStride);
		}
	}
}

static void NV12PicCopy(const amf_uint8 *src, amf_int32 srcStride, amf_int32 srcHeight, amf_uint8 *dst, amf_int32 dstStride, amf_int32 dstHeight, amf_int32 dstHeightStride)
{
#ifdef _DEBUG
	OSDebugOut(TEXT("NV12PicCopy dst: %p\n"), dst);
#endif
	// Y- plane
	PlaneCopy(src, srcStride, srcHeight, dst, dstStride, dstHeight);
	// UV - plane
	amf_int32 srcYSize = srcHeight * srcStride;
	amf_int32 dstYSize = dstHeightStride * dstStride;
	PlaneCopy(src + srcYSize, srcStride, srcHeight / 2, dst + dstYSize, dstStride, dstHeight / 2);
}

void PrintProps(amf::AMFPropertyStorage *props)
{

	amf::AMFBuffer* buffer = nullptr;
	amf_int32 count = props->GetPropertyCount();
	for (amf_int32 i = 0; i < count; i++)
	{
		wchar_t name[4096];
		amf::AMFVariant var;
		if (AMF_OK == props->GetPropertyAt(i, name, 4096, &var))
		{
			switch (var.type)
			{
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_EMPTY:
				VCELog(TEXT("%s = <empty>"), name);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_BOOL:
				VCELog(TEXT("%s = <bool>%d"), name, var.boolValue);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_INT64:
				VCELog(TEXT("%s = %lld"), name, var.int64Value);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_STRING:
				VCELog(TEXT("%s = <str>%s"), name, var.stringValue);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_WSTRING:
				VCELog(TEXT("%s = <wstr>%s"), name, var.wstringValue);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_SIZE:
				VCELog(TEXT("%s = <size>%dx%d"), name, var.sizeValue.width, var.sizeValue.height);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_RATE:
				VCELog(TEXT("%s = <rate>%d/%d"), name, var.rateValue.num, var.rateValue.den);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_RATIO:
				VCELog(TEXT("%s = <ratio>%d/%d"), name, var.ratioValue.num, var.ratioValue.den);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_INTERFACE:
				VCELog(TEXT("%s = <interface>"), name);
				break;
			default:
				VCELog(TEXT("%s = <type %d>"), name, var.type);
			}
		}
		else
		{
			VCELog(TEXT("%s = <failed>"), name, i);
		}
	}
}

VCEEncoder::VCEEncoder(int fps, int width, int height, int quality, const TCHAR* preset, bool bUse444, 
	ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR, ID3D10Device *d3ddevice)
	: mAlive(false)
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
	, mHdrPacket(nullptr)
	, mHdrSize(0)
	, mEOF(false)
	, mFrameMutex(0)
	, mCmdQueue(nullptr)
	, mCLContext(nullptr)
	, mPlatformId(nullptr)
	, mClDevice(nullptr)
	, mCurrFrame(0)
	, mD3Ddevice(d3ddevice)
	, mCanInterop(false)
	, mTmpFile(nullptr)
	, mOutPollThread(nullptr)
	, mSubmitThread(nullptr)
	, mSubmitMutex(nullptr)
	, mSubmitEvent(nullptr)
	, mClosing(false)
	, mPrevTS(0)
	, mOffsetTS(0)
	, mDiscardFiller(false)
{
	mFrameMutex = OSCreateMutex();
	mOutQueueMutex = OSCreateMutex();
	mSubmitMutex = OSCreateMutex();
	mSubmitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	mIsWin8OrGreater = IsWindows8OrGreater();
	mUseCBR = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseCBR"), 1) != 0;
	mKeyint = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("KeyframeInterval"), 0);
	mDiscardFiller = AppConfig->GetInt(TEXT("VCE Settings"), TEXT("DiscardFiller"), 0) != 0;

	if (AppConfig->GetInt(TEXT("VCE Settings"), TEXT("NoInterop"), 0) != 0)
	{
		mD3Ddevice.Release();
		//TODO nulling unnecessery?
		mD3Ddevice = nullptr;
		mCanInterop = false;
	}

	//OpenCL wants aligned surfaces
	mAlignedSurfaceWidth = ((mWidth + (256 - 1)) & ~(256 - 1));
	mAlignedSurfaceHeight = (mHeight + 31) & ~31;

	mInBuffSize = mAlignedSurfaceWidth * mAlignedSurfaceHeight * 3 / 2;

	memset(mInputBuffers, 0, sizeof(mInputBuffers));

#ifdef _DEBUG
	//errno_t err = fopen_s(&mTmpFile, "obs_times.bin", "wb");
	/*errno_t err = fopen_s(&mTmpFile, "amf.h264", "wb");
	if (err) OSDebugOut(TEXT("Couldn't open profiling file. Ignored.\n"));*/
#endif
}

VCEEncoder::~VCEEncoder()
{
	Stop();

	while (!mOutputQueue.empty())
	{
		OutputList *out = mOutputQueue.front();
		mOutputQueue.pop();
		delete out; //dtor should call Clear()
	}

	while (!mOutputReadyQueue.empty())
	{
		OutputList *out = mOutputReadyQueue.front();
		mOutputReadyQueue.pop();
		delete out;
	}

	while (!mOutputProcessedQueue.empty())
	{
		OutputList *out = mOutputProcessedQueue.front();
		mOutputProcessedQueue.pop();
		delete out;
	}

	for (int i = 0; i < MAX_INPUT_BUFFERS; i++)
	{
		ClearInputBuffer(i);
	}

	if (mEncoder) mEncoder->Terminate();
	////mContext->Terminate();

	for (auto mem : mCLMemObjs)
		clReleaseMemObject(mem);
	mCLMemObjs.clear();

	mTSqueue = {};
	mOCLDevice.Terminate();
	mDX11Device.Terminate();
	mDX9Device.Terminate();
	mD3Ddevice.Release();

	OSCloseMutex(mFrameMutex);
	OSCloseMutex(mOutQueueMutex);
	OSCloseMutex(mSubmitMutex);
	OSCloseEvent(mSubmitEvent);

	if(mTmpFile) fclose(mTmpFile);
#ifdef AMF_CORE_STATIC
	UnbindAMF();
#endif
}

void VCEEncoder::ClearInputBuffer(int i)
{
	OSDebugOut(TEXT("Clearing buffer %d type %d\n"), i, mInputBuffers[i].mem_type);
	if (mInputBuffers[i].pBuffer)
	{
		if (mInputBuffers[i].mem_type == amf::AMF_MEMORY_DX11)
		{
			ID3D11DeviceContext *d3dcontext = nullptr;
			mDX11Device.GetDevice()->GetImmediateContext(&d3dcontext);
			d3dcontext->Unmap(mInputBuffers[i].pTex, 0);
			mInputBuffers[i].pTex->Release();
			mInputBuffers[i].pTex = nullptr;
			d3dcontext->Release();
		}
		else if (mInputBuffers[i].mem_type == amf::AMF_MEMORY_HOST)
		{
			delete[] mInputBuffers[i].pBuffer;
		}
		else if (mInputBuffers[i].mem_type == amf::AMF_MEMORY_DX9)
		{
			if (mInputBuffers[i].pBuffer > (uint8_t*)1)
				delete[] mInputBuffers[i].pBuffer;

			if (mInputBuffers[i].pSurface9)
			{
				mInputBuffers[i].pSurface9->UnlockRect();
				mInputBuffers[i].pSurface9->Release();
				mInputBuffers[i].pSurface9 = nullptr;
			}
		}

		mInputBuffers[i].pBuffer = nullptr;
	}

	if (mInputBuffers[i].surface)
	{
		clReleaseMemObject(mInputBuffers[i].surface);
		mInputBuffers[i].surface = nullptr;
	}

	for (int k = 0; k < 2; k++)
	{
		if (mInputBuffers[i].yuv_surfaces[k])
		{
			clReleaseMemObject(mInputBuffers[i].yuv_surfaces[k]);
			mInputBuffers[i].yuv_surfaces[k] = nullptr;
		}
	}
	mInputBuffers[i].isMapped = false;
	mInputBuffers[i].locked = 0;
	mInputBuffers[i].inUse = 0;

	mInputBuffers[i].mem_type = amf::AMF_MEMORY_UNKNOWN;
}

bool VCEEncoder::Init()
{
#ifdef AMF_CORE_STATIC
	if (!BindAMF())
		return false;

	//FIXME this crashes in (inlined) AMFVariant ctor
	amf::AMFVariant v(567);
#endif

	AMF_RESULT res = AMF_OK;
	int iInt;
	cl_int status = CL_SUCCESS;
	VCELog(TEXT("Build date ") TEXT(__DATE__) TEXT(" ") TEXT(__TIME__));

#define USERCFG(x,y) \
	if(userCfg) x = static_cast<decltype(x)>(AppConfig->GetInt(TEXT("VCE Settings"), TEXT(y), x))

	bool userCfg = AppConfig->GetInt(TEXT("VCE Settings"), TEXT("UseCustom"), 0) != 0;
	int adapterId = AppConfig->GetInt(TEXT("Video"), TEXT("Adapter"), 0);
	// OBS sets timer to 1ms interval
	//amf_increase_timer_precision();

	res = pAMFCreateContext(&mContext);
	RETURNIFFAILED(res, TEXT("AMFCreateContext failed. %d"), res);

	// Select engine. DX11 looks be best for D3D11 -> OpenCL -> AMF interop.
	mEngine = (uint32_t)AppConfig->GetInt(TEXT("VCE Settings"), TEXT("AMFEngine"), 2);
	if (mEngine > 2) mEngine = 2;

	if (mEngine == 2)
	{
		res = mDX11Device.Init(adapterId, false);
		RETURNIFFAILED(res, TEXT("D3D11 device init failed. %d\n"), res);
		//TODO Cannot find encoder with mD3Ddevice because "Device removed"
		res = mContext->InitDX11(mDX11Device.GetDevice(), amf::AMF_DX11_0);
		RETURNIFFAILED(res, TEXT("AMF context init with D3D11 failed. %d\n"), res);
		mCanInterop = true;
		if (AppConfig->GetInt(TEXT("VCE Settings"), TEXT("NoInterop"), 0) != 0)
			mCanInterop = false;
	}
	else if (mEngine == 1)
	{
		res = mDX9Device.Init(true, adapterId, false, 1, 1);
		RETURNIFFAILED(res, TEXT("DX9 device init failed. %d"), res);
		res = mContext->InitDX9(mDX9Device.GetDevice());
		RETURNIFFAILED(res, TEXT("AMF context init with D3D9 failed. %d\n"), res);

		/*if (FAILED(DXVA2CreateVideoService(mDX9Device.GetDevice(),
			__uuidof(IDirectXVideoDecoderService), (void **)&mDxvaService)))
		{
			VCELog(TEXT("Failed to create IDirectXVideoDecoderService"));
		}*/
	}

	//Only with DX11
	if (mCanInterop)
	{
		res = mOCLDevice.Init(nullptr, mD3Ddevice, mDX11Device.GetDevice());
		RETURNIFFAILED(res, TEXT("OpenCL device init failed. %d"), res);

		mCmdQueue = mOCLDevice.GetCommandQueue();
		status = clGetCommandQueueInfo(mCmdQueue, CL_QUEUE_CONTEXT,
			sizeof(mCLContext), &mCLContext, nullptr);
		if (status != CL_SUCCESS)
			VCELog(TEXT("Failed to get CL context from command queue."));

		res = mContext->InitOpenCL(mOCLDevice.GetCommandQueue());
		RETURNIFFAILED(res, TEXT("AMF context init with OpenCL failed. %d\n"), res);
	}

	/*************************************************
	*
	* Create AMF encoder and set properties.
	*
	**************************************************/
	res = pAMFCreateComponent(mContext, AMFVideoEncoderVCE_AVC, &mEncoder);
	RETURNIFFAILED(res, TEXT("AMFCreateComponent(encoder) failed. %d"), res);

	// USAGE is a "preset" property so set before anything else
	amf_int64 usage = 0; //AMF_VIDEO_ENCODER_USAGE_ENUM::AMF_VIDEO_ENCODER_USAGE_LOW_LATENCY;//AMF_VIDEO_ENCODER_USAGE_TRANSCONDING; // typos
	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_USAGE, usage);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_USAGE);

	//-----------------------
	String x264prof = AppConfig->GetString(TEXT("Video Encoding"), TEXT("X264Profile"));
	AMF_VIDEO_ENCODER_PROFILE_ENUM profile = AMF_VIDEO_ENCODER_PROFILE_MAIN;

	if (x264prof.Compare(TEXT("high")))
		profile = AMF_VIDEO_ENCODER_PROFILE_HIGH;
	//Not in advanced setting but for completeness sake
	else if (x264prof.Compare(TEXT("base")))
		profile = AMF_VIDEO_ENCODER_PROFILE_BASELINE;

	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_PROFILE, (amf_int64)profile);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_PROFILE);

	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_PROFILE_LEVEL, (amf_int64)41);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_PROFILE_LEVEL);

	//-----------------------
	/*res = mEncoder->SetProperty(L"QualityEnhancementMode", 0);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, L"QualityEnhancementMode");*/

	//-----------------------
	int quality = AMF_VIDEO_ENCODER_QUALITY_PRESET_SPEED;
	int picSize = mWidth * mHeight * mFps;
	if (picSize <= 1280 * 720 * 30)
		quality = AMF_VIDEO_ENCODER_QUALITY_PRESET_QUALITY;
	else if (picSize <= 1920 * 1080 * 30)
		quality = AMF_VIDEO_ENCODER_QUALITY_PRESET_BALANCED;

	USERCFG(quality, "AMFPreset");

	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_QUALITY_PRESET, quality);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_QUALITY_PRESET);

	//TODO Is memory type? Also override type set by ENCODER_USAGE
	//mEncoder->SetProperty(L"EngineType", mIsWin8OrGreater ? 0 : 1 /*1 - DX9*/);
	//LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, L"EngineType");

	//-----------------------
	res = mEncoder->Init(amf::AMF_SURFACE_NV12, mWidth, mHeight);
	RETURNIFFAILED(res, TEXT("Encoder init failed. %d"), res);

	int gopSize = mFps * (mKeyint == 0 ? 2 : mKeyint);
	//gopSize -= gopSize % 6;
	USERCFG(gopSize, "GOPSize");
	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_GOP_SIZE, gopSize);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_GOP_SIZE);

	//res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_SLICES_PER_FRAME, 1);
	//LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_SLICES_PER_FRAME);

	mIDRPeriod = mFps * (mKeyint == 0 ? 2 : mKeyint);
	USERCFG(mIDRPeriod, "IDRPeriod");

	if (mIDRPeriod == 0)
		mIDRPeriod = mFps * (mKeyint == 0 ? 2 : mKeyint);
	else if (mIDRPeriod < 0) //Allow manual override
		mIDRPeriod = 0;

	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_IDR_PERIOD, static_cast<amf_int64>(mIDRPeriod));
	RETURNIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_IDR_PERIOD);

	// FIXME This hack probably screws things up, heh.
	// LOW_LATENCY USAGE seems to do intra-refresh only, force IDR.
	if (usage == AMF_VIDEO_ENCODER_USAGE_LOW_LATENCY)
		mLowLatencyKeyInt = mIDRPeriod;
	else
		mLowLatencyKeyInt = -1;

	//VCELog(TEXT("keyin: %d"), mLowLatencyKeyInt);

	VCELog(TEXT("Rate control method:"));
	iInt = 0;
	USERCFG(iInt, "RCM");

	AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_ENUM rcm = AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_CBR;
	if (!userCfg)
	{
		if (!mUseCBR)
			rcm = AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_CONSTRAINED_QP;
	}
	else
	{
		switch (iInt)
		{
		case 1:
			rcm = AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_PEAK_CONSTRAINED_VBR;
			break;
		case 2:
			rcm = AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_LATENCY_CONSTRAINED_VBR;
			break;
		case 3:
			rcm = AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_CONSTRAINED_QP;
			break;
		default:
			rcm = AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_CBR;
			break;
		}
	}

	if (rcm == AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_CBR)
	{
		iInt = 1;
		USERCFG(iInt, "EnforceHRD");
		res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_ENFORCE_HRD, !!iInt);
		LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_ENFORCE_HRD);

		bool bUsePad = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("PadCBR"), 0) != 0;
		if (bUsePad)
		{
			mEncoder->SetProperty(AMF_VIDEO_ENCODER_FILLER_DATA_ENABLE, true);
			LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_FILLER_DATA_ENABLE);
		}
	}

	switch (rcm)
	{
	case AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_CONSTRAINED_QP:
		VCELog(TEXT("    Constrained QP (%d)"), rcm);
		break;
	case AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_CBR:
		VCELog(TEXT("    CBR (%d)"), rcm);
		break;
	case AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_PEAK_CONSTRAINED_VBR:
		VCELog(TEXT("    Peak constrained VBR (%d)"), rcm);
		break;
	case AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_LATENCY_CONSTRAINED_VBR:
		VCELog(TEXT("    Latency constrained VBR (%d)"), rcm);
		break;
	default:
		VCELog(TEXT("    <unknown> (%d)"), rcm);
		break;
	}
	//-----------------------
	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD, rcm);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD);

	int window = 0;// 50;
	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_TARGET_BITRATE, mMaxBitrate * 1000);
	RETURNIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_TARGET_BITRATE);

	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_PEAK_BITRATE, mMaxBitrate * (1000 + window));
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_PEAK_BITRATE);

	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_VBV_BUFFER_SIZE, mBufferSize * 1000);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_VBV_BUFFER_SIZE);

	AMFRate fps = { mFps, 1 };
	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_FRAMERATE, fps);
	RETURNIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_FRAMERATE);

	int qp = 23, qpI, qpP, qpB, qpBDelta = 4;
	qp = 40 - (mQuality * 5) / 2; // 40 .. 15
	//qp = 22 + (10 - mQuality); //Matching x264 CRF
	qpI = qpP = qpB = qp;
	USERCFG(qpI, "QPI");
	USERCFG(qpP, "QPP");
	USERCFG(qpB, "QPB");
	USERCFG(qpBDelta, "QPBDelta");

	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_QP_I, qpI);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_QP_I);
	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_QP_P, qpP);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_QP_P);
	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_QP_B, qpB);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_QP_B);

	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_B_PIC_DELTA_QP, qpBDelta);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_B_PIC_DELTA_QP);

	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_REF_B_PIC_DELTA_QP, qpBDelta);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_REF_B_PIC_DELTA_QP);

	qp = 18;
	USERCFG(qp, "MinQP");
	mEncoder->SetProperty(AMF_VIDEO_ENCODER_MIN_QP, qp);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_MIN_QP);

	qp = 51;
	USERCFG(qp, "MaxQP");
	mEncoder->SetProperty(AMF_VIDEO_ENCODER_MAX_QP, qp);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_MAX_QP);

	iInt = 0;
	USERCFG(iInt, "FrameSkip");
	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_RATE_CONTROL_SKIP_FRAME_ENABLE, !!iInt);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_RATE_CONTROL_SKIP_FRAME_ENABLE);

	iInt = 1;
	USERCFG(iInt, "CABAC");
	res = mEncoder->SetProperty(L"CABACEnable", !!iInt);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, L"CABACEnable");

	//B frames are not supported yet. Needs Composition Time fix.
	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_B_REFERENCE_ENABLE, true);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_B_REFERENCE_ENABLE);
	iInt = 0;
	USERCFG(iInt, "BFrames");
	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_B_PIC_PATTERN, iInt);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_B_PIC_PATTERN);

	if (false && iInt > 0)
	{
		res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_HEADER_INSERTION_SPACING, mIDRPeriod < 1001 ? mIDRPeriod : 1000);
		LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_HEADER_INSERTION_SPACING);
	}

	mEncoder->GetProperty(AMF_VIDEO_ENCODER_INTRA_REFRESH_NUM_MBS_PER_SLOT, &mIntraMBs);
	//res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_INTRA_REFRESH_NUM_MBS_PER_SLOT, ((mHeight + 15) & ~15) / 16);
	//LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_INTRA_REFRESH_NUM_MBS_PER_SLOT);
	mEncoder->SetProperty(AMF_VIDEO_ENCODER_DE_BLOCKING_FILTER, true);

	//------------------------
	// Print few caps etc.
#pragma region Print few caps etc.
	amf::H264EncoderCapsPtr encCaps;
	if (mEncoder->QueryInterface(amf::AMFH264EncoderCaps::IID(), (void**)&encCaps) == AMF_OK)
	{
		VCELog(TEXT("Capabilities:"));
		TCHAR* accelType[] = {
			TEXT("NOT_SUPPORTED"),
			TEXT("HARDWARE"),
			TEXT("GPU"),
			TEXT("SOFTWARE")
		};
		VCELog(TEXT("  Accel type: %s"), accelType[(encCaps->GetAccelerationType() + 1) % 4]);
		VCELog(TEXT("  Max bitrate: %d"), encCaps->GetMaxBitrate());
		//VCELog(TEXT("  Max priority: %d"), encCaps->GetMaxSupportedJobPriority());

		String str;
		for (int i = 0; i < encCaps->GetNumOfSupportedLevels(); i++)
		{
			str << encCaps->GetLevel(i) << L" ";
		}
		VCELog(TEXT("  Levels: %s"), str.Array());

		str.Clear();
		for (int i = 0; i < encCaps->GetNumOfSupportedProfiles(); i++)
		{
			str << encCaps->GetProfile(i) << " ";
		}
		VCELog(TEXT("  Profiles: %s"), str.Array());

		amf::AMFIOCapsPtr iocaps;
		encCaps->GetInputCaps(&iocaps);
		VCELog(TEXT("  Input mem types:"));
		for (int i = iocaps->GetNumOfMemoryTypes() - 1; i >= 0; i--)
		{
			bool native;
			amf::AMF_MEMORY_TYPE memType;
			iocaps->GetMemoryTypeAt(i, &memType, &native);
			VCELog(TEXT("    %s, native: %d"), amf::AMFGetMemoryTypeName(memType), native);
		}

		amf_int32 imin, imax;
		iocaps->GetWidthRange(&imin, &imax);
		VCELog(TEXT("  Width min/max: %d/%d"), imin, imax);
		iocaps->GetHeightRange(&imin, &imax);
		VCELog(TEXT("  Height min/max: %d/%d"), imin, imax);
	}

	PrintProps(mEncoder);
#pragma endregion

	if (!(mOutPollThread = OSCreateThread((XTHREAD)VCEEncoder::OutputPollThread, this)))
	{
		VCELog(TEXT("Failed to create output polling thread!"));
		return false;
	}

	if(!(mSubmitThread = OSCreateThread((XTHREAD)VCEEncoder::SubmitThread, this)))
	{
		VCELog(TEXT("Failed to create buffer submitter thread!"));
		Stop();
		return false;
	}

	mReqKeyframe = true;
	mAlive = true;
	return true;
}

void VCEEncoder::ConvertD3DTex(ID3D10Texture2D *d3dtex, void *data, void **state)
{
	assert(d3dtex && data && state);
	mfxFrameData *mfx = static_cast<mfxFrameData*>(data);
	int idx = (int)(mfx->MemId);
	assert(idx);
	InputBuffer &inBuf = mInputBuffers[idx - 1];
	assert(inBuf.mem_type == amf::AMF_MEMORY_OPENCL);

	if (inBuf.mem_type != amf::AMF_MEMORY_OPENCL)
	{
		VCELog(TEXT("Cannot convert buffer, incorrect memory type!"));
		return;
	}

	cl_mem clD3D = static_cast<cl_mem>(*state);
	if (!clD3D)
	{
		clD3D = mOCLDevice.CreateTexture2D(d3dtex);
		*state = clD3D;
		assert(clD3D);
		mCLMemObjs.push_back(clD3D);
	}

	mOCLDevice.AcquireD3DObj(clD3D);
	mOCLDevice.RunKernels(clD3D, inBuf.yuv_surfaces[0], inBuf.yuv_surfaces[1], mWidth, mHeight);
	mOCLDevice.ReleaseD3DObj(clD3D);
	clFinish(mOCLDevice.GetCommandQueue());

	//amf::AMFSurfacePtr pSurf;
	//mContext->CreateSurfaceFromOpenCLNative(amf::AMF_SURFACE_NV12, mWidth, mHeight, 
	//	(void**)&inBuf.yuv_surfaces, &pSurf, nullptr);
	//mSurfaces.push(pSurf);
}

bool VCEEncoder::Stop()
{
	mClosing = true;
	if (mSubmitThread)
	{
		SetEvent(mSubmitEvent);
		OSTerminateThread(mSubmitThread, 15000);
		mSubmitThread = nullptr;
	}

	if (mOutPollThread)
	{
		OSTerminateThread(mOutPollThread, 15001);
		mOutPollThread = nullptr;
	}

	if (mEncoder) mEncoder->Drain();
	return true;
}

void VCEEncoder::CopyOutput(List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD &out_pts)
{
	profileIn("CopyOutput")
	OSMutexLocker lock(mOutQueueMutex);
	if (!mOutputProcessedQueue.empty())
	{
		OutputList *out = mOutputProcessedQueue.front();
		mOutputProcessedQueue.pop();

		DataPacket packet;
		packet.lpPacket = out->pBuffer.Array();
		packet.size = out->pBuffer.Num();
		packets << packet;
		packetTypes << out->type;
		out_pts = out->timestamp;
		mOutputReadyQueue.push(out);
	}
	profileOut
}

bool VCEEncoder::Encode(LPVOID picIn, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD outputTimestamp, DWORD &out_pts)
{
	if (!picIn)
	{
		mClosing = true;
	}

	OSMutexLocker locker(mFrameMutex);

	packets.Clear();
	packetTypes.Clear();

	OSEnterMutex(mOutQueueMutex);
	while (!mOutputReadyQueue.empty())
	{
		auto q = mOutputReadyQueue.front();
		mOutputReadyQueue.pop();
		mOutputQueue.push(q);
	}
	OSLeaveMutex(mOutQueueMutex);

	if (!picIn)
	{
		AMF_RESULT res = AMF_REPEAT;
		amf::AMFDataPtr amfdata;

		Stop();
		if (!mOutputProcessedQueue.empty())
		{
			CopyOutput(packets, packetTypes, out_pts);
		}
		else
		{
			do
			{
				OSSleep(1);
				res = mEncoder->QueryOutput(&amfdata);
			} while (res == AMF_REPEAT);

			if (res == AMF_OK)
			{
				amf::AMFBufferPtr buf(amfdata);
				ProcessBitstream(buf);
				CopyOutput(packets, packetTypes, out_pts);
			}
			else
				mEOF = true;
		}
		OSDebugOut(TEXT("Drained %d frame(s)\n"), packets.Num());
		return true;
	}

//#ifdef _DEBUG
	QWORD start = GetQPCTime100NS();
//#endif

	mfxFrameSurface1 *inputSurface = (mfxFrameSurface1*)picIn;
	mfxFrameData &data = inputSurface->Data;
	assert(data.MemId);

	unsigned int idx = (unsigned int)data.MemId - 1;
#ifdef _DEBUG
	OSDebugOut(TEXT("Encoding buffer: %d\n"), idx);
#endif

	InputBuffer &inBuf = mInputBuffers[idx];
	inBuf.frameData = &(inputSurface->Data);
	_InterlockedCompareExchange(&(inBuf.locked), 1, 0);
	//TODO what about outputTimestamp
	inBuf.timestamp = data.TimeStamp;
	inBuf.outputTimestamp = data.TimeStamp - mPrevTS;
	mPrevTS = data.TimeStamp;
	//mTSqueue.push(data.TimeStamp);

	OSEnterMutex(mSubmitMutex);
	mSubmitQueue.push(&mInputBuffers[idx]);
	OSLeaveMutex(mSubmitMutex);
	SetEvent(mSubmitEvent);

	CopyOutput(packets, packetTypes, out_pts);

#ifdef _DEBUG
	OSEnterMutex(mOutQueueMutex);
	OSDebugOut(TEXT("Output queues: Sent %d, Ready: %d, Unused: %d, submit: %d out: %d\n"),
		mOutputReadyQueue.size(),
		mOutputProcessedQueue.size(),
		mOutputQueue.size(), mSubmitQueue.size(), packets.Num());
	OSLeaveMutex(mOutQueueMutex);
#endif

	//OSDebugOut(TEXT("TS %d: %lld %d %d\n"), packets.Num(), data.TimeStamp, outputTimestamp, out_pts);

#ifdef _DEBUG
	start = GetQPCTime100NS() - start;
	//OSDebugOut(TEXT("Process time: %fms\n"), float(start) / MS_TO_100NS);
#endif

	if (mTmpFile)
	{
#ifndef _DEBUG
		start = GetQPCTime100NS() - start;
#endif
		//fwrite(&start, sizeof(start), 1, mTmpFile);
	}

	return true;
}


DWORD VCEEncoder::OutputPollThread(VCEEncoder* enc)
{
	enc->OutputPoll();
	return 0;
}

void VCEEncoder::OutputPoll()
{
	amf::AMFDataPtr data;
	AMF_RESULT res = AMF_REPEAT;

	while (mSubmitThread)
	{
		res = mEncoder->QueryOutput(&data);
		if (res == AMF_OK)
		{
			profileIn("Query output")
			amf::AMFBufferPtr buffer(data);
			ProcessBitstream(buffer);
			profileOut
		}
		OSSleep(5);
	}
}

DWORD VCEEncoder::SubmitThread(VCEEncoder* enc)
{
	enc->Submit();
	return 0;
}

void VCEEncoder::Submit()
{
	int resubmitCount = 0;
	QWORD sleep = QWORD((1000.f / mFps / 5) * 1000);
	//QWORD sleep = QWORD(200000 / mFps);
	QWORD lastTime = GetQPCTimeMS();
	while (true)
	{
		//TODO .empty() thread-safetying
		bool empty = true;
		OSEnterMutex(mSubmitMutex);
		empty = mSubmitQueue.empty();

		if (empty)
		{
			OSLeaveMutex(mSubmitMutex);
			if (mClosing)
				break;
			else
				WaitForSingleObject(mSubmitEvent, INFINITE);
		}
		else
		{
			profileIn("Submit buffer")
			InputBuffer *inBuf = mSubmitQueue.front();

			AMF_RESULT res = SubmitBuffer(inBuf);
			if (res != AMF_INPUT_FULL)
			{
				mSubmitQueue.pop();
				if (resubmitCount > 0 && (GetQPCTimeMS() - lastTime) > 500)
				{
					VCELog(TEXT("Failed to submit input buffer multiple times already. VCE is probably too slow for current settings."));
					lastTime = GetQPCTimeMS();
				}
				resubmitCount = 0;
			}
			else
			{
				resubmitCount++;
				OSSleepMicrosecond(sleep);
			}
			OSLeaveMutex(mSubmitMutex);
			profileOut
		}
	}
}

AMF_RESULT VCEEncoder::SubmitBuffer(InputBuffer *inBuf)
{
	AMF_RESULT res = AMF_FAIL, inRes = AMF_FAIL;
	amf::AMFSurfacePtr pSurface;
	amf::AMFDataPtr amfdata;
	amf::AMFPlanePtr plane;

	switch (inBuf->mem_type)
	{
	case amf::AMF_MEMORY_DX11:
	{
		profileIn("CopyResource (dx11)")
		res = mContext->CreateSurfaceFromDX11Native(inBuf->pTex, &pSurface, &mObserver);
		if(res != AMF_OK)
			VCELog(TEXT("Failed to create surface from DX11 texture: %s"), amf::AMFGetResultText(res));
		profileOut
	}
		break;

	case amf::AMF_MEMORY_OPENCL:
		res = mContext->CreateSurfaceFromOpenCLNative(amf::AMF_SURFACE_NV12,
			mWidth, mHeight, (void**)&inBuf->yuv_surfaces, &pSurface, &mObserver);
		break;

	case amf::AMF_MEMORY_HOST:

		//TODO doesn't work with NV12?
		res = mContext->CreateSurfaceFromHostNative(amf::AMF_SURFACE_NV12,
			mWidth, mHeight, mWidth, mHeight,
			inBuf->pBuffer, &pSurface, &mObserver);
		break;
	default:
		//return AMF_FAIL; //commented, crash here instead
		break;
	}

	if (res != AMF_OK)
		VCELog(TEXT("Failed to allocate surface: %s"), amf::AMFGetResultText(res));

	assert(pSurface);
	pSurface->SetProperty(L"InputBuffer", (amf_int64)inBuf);

	if (mReqKeyframe || (mLowLatencyKeyInt > 0 && mCurrFrame >= mLowLatencyKeyInt))
	{
		res = pSurface->SetProperty(AMF_VIDEO_ENCODER_FORCE_PICTURE_TYPE, AMF_VIDEO_ENCODER_PICTURE_TYPE_IDR);
		LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_FORCE_PICTURE_TYPE);
		mReqKeyframe = true;
		if (mCurrFrame >= mLowLatencyKeyInt)
			mCurrFrame = 0;
		if(mLowLatencyKeyInt > 0)
			res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_INTRA_REFRESH_NUM_MBS_PER_SLOT, 0);
		OSDebugOut(TEXT("Keyframe requested %d\n"), res);
	}
	else
		res = pSurface->SetProperty(AMF_VIDEO_ENCODER_FORCE_PICTURE_TYPE, AMF_VIDEO_ENCODER_PICTURE_TYPE_NONE);

	// Copy one set of SPS/PPS NALs and disable them now.
	// Works with local recordings, not so much when streaming
	/*if (mCurrFrame == 0)
	{
		res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_HEADER_INSERTION_SPACING, 0);
		LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_HEADER_INSERTION_SPACING);
	}*/

	mCurrFrame++;
	pSurface->SetFrameType(amf::AMF_FRAME_PROGRESSIVE);
	pSurface->SetDuration(1000 * MS_TO_100NS / mFps); //TODO precalc
	pSurface->SetPts(inBuf->timestamp * MS_TO_100NS);
	//pSurface->SetProperty(L"OUTPUTTS", inBuf->outputTimestamp);
	//mTSqueue.push(data.TimeStamp);

	inRes = mEncoder->SubmitInput(pSurface);

#ifdef _DEBUG
	if (inRes != AMF_OK)
		OSDebugOut(TEXT("Failed to submit data to encoder: %d %s\n"), inRes, amf::AMFGetResultText(inRes));
#endif

	// Try again
	if (inRes == AMF_INPUT_FULL)
		return AMF_INPUT_FULL;

	if (mReqKeyframe)
	{
		mReqKeyframe = false;
		if (mLowLatencyKeyInt > 0)
			res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_INTRA_REFRESH_NUM_MBS_PER_SLOT, mIntraMBs);
	}

	_InterlockedCompareExchange(&(inBuf->inUse), 0, 1);
	//_InterlockedCompareExchange(&(inBuf->locked), 0, 1);
	
	/*InputBuffer &in = mInputBuffers[(int)inBuf->frameData->MemId - 1];
	assert(in.locked == 0);*/

	return inRes;
}

void VCEEncoder::ProcessBitstream(amf::AMFBufferPtr &buff)
{
	profileIn("ProcessBitstream")

	int frameType = 0;
	buff->GetProperty(L"OutputDataType", &frameType);
	static TCHAR *frameTypes[] = { TEXT("IDR"), TEXT("I"), TEXT("P"), TEXT("B") };

	OutputList *bufferedOut = nullptr;

	OSMutexLocker lock(mOutQueueMutex);

	//TODO Only using 1 output buffer so remove this probably
	if (!mOutputQueue.empty())
	{
		bufferedOut = mOutputQueue.front();
		mOutputQueue.pop();
	}
	else
		bufferedOut = new OutputList;

	uint8_t *start = (uint8_t *)buff->GetNative();
	uint8_t *end = start + buff->GetSize();
	const static uint8_t start_seq[] = { 0, 0, 1 };
	start = std::search(start, end, start_seq, start_seq + 3);

	if (mTmpFile)
		fwrite(start, 1, buff->GetSize(), mTmpFile);

	//FIXME
	uint64_t dts = 0;// outputTimestamp;
	uint64_t out_pts = buff->GetPts() / MS_TO_100NS;

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
		//OSDebugOut(TEXT("nal type: %d ref_idc: %d\n"), nal.i_type, nal.i_ref_idc);
	}
	size_t nalNum = nalOut.Num();

	//XXX mTSqueue is not thread-safe
	/*assert(!mTSqueue.empty());

	if (!mOffsetTS)
		mOffsetTS = mTSqueue.back();

	uint64_t ts = mTSqueue.front();
	mTSqueue.pop();*/

	uint32_t diff = 0;
	//buff->GetProperty(L"OUTPUTTS", &diff);

	//dts = (ts - mOffsetTS);
	int32_t timeOffset = 0;// int(out_pts - dts);// int(out_pts - dts);
	/*if (frameType == AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_IDR)
		timeOffset = 0;
	if (frameType == AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_P)
		timeOffset = 16;
	if (frameType == AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_B)
		timeOffset = -17;*/
	/*timeOffset += frameShift;

	if (nalNum && timeOffset > 0)
	{
		frameShift -= timeOffset;
		timeOffset = 0;
	}*/

	/*OSDebugOut(TEXT("%s ts: %lld pts: %lld dts: %lld toff: %d pts-dts: %d\n"),
		frameTypes[frameType], mOffsetTS, out_pts, dts, timeOffset, int(out_pts - dts));*/

	timeOffset = htonl(timeOffset);
	BYTE *timeOffsetAddr = ((BYTE*)&timeOffset) + 1;

	PacketType bestType = PacketType_VideoDisposable;
	bool bFoundFrame = false;
	bool bSPS = false;
	UINT64 pos = 5;

	for (unsigned int i = 0; i < nalNum; i++)
	{
		x264_nal_t &nal = nalOut[i];
		//TODO Check if B-frames really want SPS/PPS from IDR or if there's any diff at all.
		if (!mHdrPacket && nal.i_type == NAL_SLICE_IDR)
		{
			mHdrSize = buff->GetSize();
			mHdrPacket = new uint8_t[mHdrSize];
			memcpy(mHdrPacket, buff->GetNative(), mHdrSize);
		}

		if (nal.i_type == NAL_SLICE_IDR || nal.i_type == NAL_SLICE)
		{
			BYTE *skip = nal.p_payload;
			while (*(skip++) != 0x1);
			int skipBytes = (int)(skip - nal.p_payload);

			if (!bFoundFrame)
			{
				// Reusing buffer
				if (bufferedOut->pBuffer.Num() >= 5)
				{
					BYTE *ptr = bufferedOut->pBuffer.Array();
					ptr[0] = (nal.i_type == NAL_SLICE_IDR) ? 0x17 : 0x27;
					ptr[1] = 1;
					memcpy(ptr + 2, timeOffsetAddr, 3);
				}
				else
				{
					bufferedOut->pBuffer.Add((nal.i_type == NAL_SLICE_IDR) ? 0x17 : 0x27);
					bufferedOut->pBuffer.Add(1);
					bufferedOut->pBuffer.AppendArray(timeOffsetAddr, 3);
				}

				bFoundFrame = true;
			}
			break;
		}
	}

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
					packetOut.Seek(pos, 0);
					packetOut.OutputDword(htonl(sei_size + 2));
					packetOut.Serialize(sei_start - 1, sei_size + 1);
					packetOut.OutputByte(0x80);
					pos = packetOut.GetPos();
				}
				else
				{
					BufferOutputSerializer packetOut(bufferedOut->pBuffer);
					packetOut.Seek(pos, 0);
					packetOut.OutputDword(htonl(sei_size + 2));
					packetOut.Serialize(sei_start - 1, sei_size + 1);
					packetOut.OutputByte(0x80);
					pos = packetOut.GetPos();
				}
				sei_start += sei_size;

				if (*sei_start == 0x80 && std::find_if_not(sei_start + 1, end, [](uint8_t val) { return val == 0; }) == end) //find rbsp_trailing_bits
					break;
			}
		}
		else if (nal.i_type == NAL_AUD || (!mDiscardFiller && nal.i_type == NAL_FILLER))
		{
			BYTE *skip = nal.p_payload;
			while (*(skip++) != 0x1);
			int skipBytes = (int)(skip - nal.p_payload);

			int newPayloadSize = (nal.i_payload - skipBytes);

			BufferOutputSerializer packetOut(bufferedOut->pBuffer);
			packetOut.Seek(pos, 0);
			packetOut.OutputDword(htonl(newPayloadSize));
			packetOut.Serialize(nal.p_payload + skipBytes, newPayloadSize);
			pos = packetOut.GetPos();
		}
		else if (nal.i_type == NAL_SLICE_IDR || nal.i_type == NAL_SLICE)
		{
			BYTE *skip = nal.p_payload;
			while (*(skip++) != 0x1);
			int skipBytes = (int)(skip - nal.p_payload);

			/*if (!bFoundFrame)
			{
				bufferedOut->pBuffer.Insert(0, (nal.i_type == NAL_SLICE_IDR) ? 0x17 : 0x27);
				bufferedOut->pBuffer.Insert(1, 1);
				bufferedOut->pBuffer.InsertArray(2, timeOffsetAddr, 3);

				bFoundFrame = true;
			}*/

			int newPayloadSize = (nal.i_payload - skipBytes);
			BufferOutputSerializer packetOut(bufferedOut->pBuffer);
			packetOut.Seek(pos, 0);

			packetOut.OutputDword(htonl(newPayloadSize));
			packetOut.Serialize(nal.p_payload + skipBytes, newPayloadSize);
			pos = packetOut.GetPos();

			// P is _HIGH, I is _HIGHEST
			switch (nal.i_ref_idc)
			{
			case NAL_PRIORITY_DISPOSABLE:   bestType = MAX(bestType, PacketType_VideoDisposable);  break;
			case NAL_PRIORITY_LOW:          bestType = MAX(bestType, PacketType_VideoLow);         break;
			case NAL_PRIORITY_HIGH:         bestType = MAX(bestType, PacketType_VideoHigh);        break;
			case NAL_PRIORITY_HIGHEST:      bestType = MAX(bestType, PacketType_VideoHighest);     break;
			}
		}
		else if(nal.i_type == NAL_SPS)
		{
			BYTE *skip = nal.p_payload;
			while (*(skip++) != 0x1);
			int skipBytes = (int)(skip - nal.p_payload);
			/*
			if (bufferedOut->pBuffer.Num() >= 5)
			{
				BYTE *ptr = bufferedOut->pBuffer.Array();
				ptr[0] = 0x57;
				ptr[1] = 1;
				memcpy(ptr + 2, timeOffsetAddr, 3);
			}
			else
			{
				bufferedOut->pBuffer.Add(0x57);
				bufferedOut->pBuffer.Add(1);
				bufferedOut->pBuffer.AppendArray(timeOffsetAddr, 3);
			}*/

			BufferOutputSerializer packetOut(bufferedOut->pBuffer);
			packetOut.Seek(pos, 0);
			int newPayloadSize = (nal.i_payload - skipBytes);

			packetOut.OutputDword(htonl(newPayloadSize));
			packetOut.Serialize(nal.p_payload + skipBytes, newPayloadSize);

			x264_nal_t &pps = nalOut[i+1]; //the PPS always comes after the SPS
			skip = pps.p_payload;
			while (*(skip++) != 0x1);
			skipBytes = (int)(skip - pps.p_payload);
			newPayloadSize = (pps.i_payload - skipBytes);

			packetOut.OutputDword(htonl(newPayloadSize));
			packetOut.Serialize(pps.p_payload + skipBytes, newPayloadSize);
			pos = packetOut.GetPos();

			/*DataPacket packet;
			packet.lpPacket = bufferedOut->pBuffer.Array();
			packet.size = (UINT)pos;

			decltype(bufferedOut) tmp = bufferedOut;

			packetTypes << PacketType::PacketType_VideoHighest;
			packets << packet;

			if (!mOutputQueue.empty())
			{
				bufferedOut = mOutputQueue.front();
				mOutputQueue.pop();
			}
			else
				bufferedOut = new OutputList;
			pos = 5;
			mOutputQueue.push(tmp);*/
		}
	}

	//DataPacket packet;
	//packet.lpPacket = bufferedOut->pBuffer.Array();
	//packet.size = (UINT)pos; // bufferedOut->pBuffer.Num();
	bufferedOut->pBuffer.SetSize(pos); //FIXME extra size var or might as well use new pBuffer every time then
	bufferedOut->type = bestType;
	bufferedOut->timestamp = out_pts;//buff->GetPts() / MS_TO_100NS;

	mOutputProcessedQueue.push(bufferedOut);
	nalOut.Clear();

	profileOut
}

//TODO RequestBuffers
void VCEEncoder::RequestBuffers(LPVOID buffers)
{
	if (!buffers || !mAlive)
		return;

	mfxFrameData *buff = (mfxFrameData*)buffers;
	int idx = (int)buff->MemId - 1;
	bool ret = false;
	amf::AMF_MEMORY_TYPE mem_type = amf::AMF_MEMORY_UNKNOWN;
//#ifdef _DEBUG
	//OSDebugOut(TEXT("RequestBuffers: %d\n"), (unsigned int)buff->MemId - 1);
//#endif

	if (idx >= 0)
		mem_type = mInputBuffers[idx].mem_type;

	// Encoder is too slow, stall OBS some
	static UINT msgID = 0;
	static UINT msgCount = 0;

	if (msgID)
	{
		msgCount++;
		if (msgCount > 100)
		{
			OBSRemoveStreamInfo(msgID);
			msgID = 0;
			msgCount = 0;
		}
	}

	while (mSubmitThread)
	{
		//Scope the lock
		{
			OSMutexLocker lock(mSubmitMutex);
			if (mSubmitQueue.size() < MAX_INPUT_BUFFERS)
				break;
		}

		if (!msgID)
		{
			//But statusbar usually fails to update
			msgID = OBSAddStreamInfo(TEXT("VCE is too slow."), StreamInfoPriority::StreamInfoPriority_Critical);
			msgCount = 0;
			OSDebugOut(TEXT("VCE is too slow."));
		}
		OSSleep(5);
	}

	//TODO CL kernels are only usable with interop currently
	//if (GetFeatures() & VideoEncoder_D3D11Interop)
	if (mCanInterop && mCmdQueue)
	{
		if (mem_type == amf::AMF_MEMORY_UNKNOWN ||
			mem_type == amf::AMF_MEMORY_OPENCL)
			if (RequestBuffersCL(buffers))
				return;

		if (mem_type == amf::AMF_MEMORY_OPENCL && !mInputBuffers[idx].inUse)
			ClearInputBuffer(idx);
	}

	//if (mIsWin8OrGreater)
	if (mEngine == 2)
	{
		if ((mem_type == amf::AMF_MEMORY_UNKNOWN ||
			mem_type == amf::AMF_MEMORY_DX11))
		{
			if (RequestBuffersDX11(buffers))
				return;
		}

		if (mem_type == amf::AMF_MEMORY_DX11 && !mInputBuffers[idx].inUse)
			ClearInputBuffer(idx);
	}

	//FIXME IDirect3DSurface9: memory access fail when copying UV bytes, doesn't like odd positions?
	if (false && mEngine == 1)
	{
		if ((mem_type == amf::AMF_MEMORY_UNKNOWN ||
			mem_type == amf::AMF_MEMORY_DX9))
		{
			if (RequestBuffersDX9(buffers))
				return;
		}

		if (mem_type == amf::AMF_MEMORY_DX9 && !mInputBuffers[idx].inUse)
			ClearInputBuffer(idx);
	}

	if (mem_type == amf::AMF_MEMORY_UNKNOWN ||
		mem_type == amf::AMF_MEMORY_HOST)
	{
		if (RequestBuffersHost(buffers))
			return;
	}

	//Failed
	mInputBuffers[idx].mem_type = amf::AMF_MEMORY_UNKNOWN;
	assert(false && "Out of unused buffers!");

}

bool VCEEncoder::RequestBuffersHost(LPVOID buffers)
{
	mfxFrameData *buff = (mfxFrameData*)buffers;

	if (buff->MemId &&
		mInputBuffers[(unsigned int)buff->MemId - 1].pBuffer
		&& !mInputBuffers[(unsigned int)buff->MemId - 1].locked)
		return true;

	//*buff = { { 0 } }; //maybe
	//memset(buff, 0, sizeof(*buff));
	//buff->Y = nullptr;
	for (int i = 0; i < MAX_INPUT_BUFFERS; i++)
	{
		InputBuffer& inBuf = mInputBuffers[i];
		if (inBuf.locked || inBuf.inUse
			//|| inBuf.pBuffer
			)
			continue;

		if (!inBuf.pBuffer)
		{
			/*if (mContext->AllocBuffer(amf::AMF_MEMORY_HOST, mInBuffSize, &inBuf.pAMFBuffer) != AMF_OK)
			{
				VCELog(TEXT("AMF failed to allocate host buffer."));
				return false;
			}*/
			inBuf.pBuffer = new uint8_t[mInBuffSize];
			inBuf.size = mInBuffSize;
		}

		inBuf.mem_type = amf::AMF_MEMORY_HOST;
		buff->Pitch = mWidth;
		buff->Y = (mfxU8*)inBuf.pBuffer;
		buff->UV = buff->Y + (mHeight * buff->Pitch);
		buff->MemId = mfxMemId(i + 1);

#if _DEBUG
		OSDebugOut(TEXT("Giving buffer id %d\n"), i + 1);
#endif
		_InterlockedCompareExchange(&(inBuf.inUse), 1, 0);
		_InterlockedCompareExchange(&(inBuf.locked), 0, 1);
		return true;
	}

	VCELog(TEXT("Max number of input buffers reached."));
	OSDebugOut(TEXT("Max number of input buffers reached.\n"));
	return false;
}

bool VCEEncoder::RequestBuffersDX11(LPVOID buffers)
{
	mfxFrameData *buff = (mfxFrameData*)buffers;

	if (buff->MemId && 
		mInputBuffers[(unsigned int)buff->MemId - 1].isMapped
		&& !mInputBuffers[(unsigned int)buff->MemId - 1].locked)
		return true;

	for (int i = 0; i < MAX_INPUT_BUFFERS; i++)
	{
		InputBuffer& inBuf = mInputBuffers[i];
		if (inBuf.locked || inBuf.isMapped || inBuf.inUse)
			continue;

		HRESULT hres = S_OK;
		D3D11_TEXTURE2D_DESC desc;
		memset(&desc, 0, sizeof(D3D11_TEXTURE2D_DESC));

		// WIN7: Crap http://msdn.microsoft.com/en-us/library/windows/desktop/ff471325%28v=vs.85%29.aspx
		desc.Format = DXGI_FORMAT_NV12;
		desc.Width = mWidth;
		desc.Height = mHeight;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		//Force fail
		//desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		//desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		//desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.SampleDesc.Count = 1;
		//desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_STAGING;
		// 'Green' frames get thrown in for some reason
		//desc.Usage = D3D11_USAGE_DYNAMIC;

		ID3D11DeviceContext *d3dcontext = nullptr;

		mDX11Device.GetDevice()->GetImmediateContext(&d3dcontext);
		if (!d3dcontext)
		{
			VCELog(TEXT("Failed to get immediate D3D11 context."));
			return false;
		}

		if (!inBuf.pTex)
		{
			hres = mDX11Device.GetDevice()->CreateTexture2D(&desc, 0, &inBuf.pTex);
			HRETURNIFFAILED(hres, "Failed to create D3D11 texture.");
		}

		D3D11_MAPPED_SUBRESOURCE map;
		hres = d3dcontext->Map(inBuf.pTex, 0, D3D11_MAP::D3D11_MAP_WRITE/*_DISCARD*/, 0, &map);
		if (hres == E_OUTOFMEMORY)
			CrashError(TEXT("Failed to map D3D11 texture: Out of memory."));
		HRETURNIFFAILED(hres, "Failed to map D3D11 texture.");

		inBuf.mem_type = amf::AMF_MEMORY_DX11;
		inBuf.pBuffer = (uint8_t*)1;
		inBuf.isMapped = true;
		buff->Pitch = map.RowPitch;
		buff->Y = (mfxU8*)map.pData;
		buff->UV = buff->Y + (mHeight * buff->Pitch);
		buff->MemId = mfxMemId(i + 1);
		_InterlockedCompareExchange(&(inBuf.inUse), 1, 0);
		_InterlockedCompareExchange(&(inBuf.locked), 0, 1);

		hres = d3dcontext->Release();
		if(FAILED(hres))
			VCELog(TEXT("Failed to release D3D11 immediate context."));
		return true;
	}

	VCELog(TEXT("Max number of input buffers reached."));
	OSDebugOut(TEXT("Max number of input buffers reached.\n"));
	return false;
}

bool VCEEncoder::RequestBuffersDX9(LPVOID buffers)
{
	mfxFrameData *buff = (mfxFrameData*)buffers;
	HRESULT hr = S_OK;

	/*if (buff->MemId &&
		mInputBuffers[(unsigned int)buff->MemId - 1].pBuffer
		&& !mInputBuffers[(unsigned int)buff->MemId - 1].locked)
		return true;

	buff->Y = nullptr;*/
	for (int i = 0; i < MAX_INPUT_BUFFERS; i++)
	{
		InputBuffer& inBuf = mInputBuffers[i];
		if (inBuf.locked || inBuf.pBuffer)
			continue;

		// WTF surface is AMF using. mfxFrameData get overwritten (aka WTF is UV supposed to go?)
		// Looking at Wine source, DX9 creates surface from 2 textures: D3DFMT_L8, D3DFMT_A8L8?
		if (!inBuf.pSurface9)
		{
			//hr = mDX9Device.GetDevice()->CreateOffscreenPlainSurface(
			//	mWidth, mHeight, (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2'),
			//	//Has to be D3DPOOL_SYSTEMMEM for UpdateSurface()
			//	D3DPOOL::D3DPOOL_SYSTEMMEM,
			//	&inBuf.pSurface9, nullptr);
			hr = E_INVALIDARG;
			//hr = mDxvaService->CreateSurface(mWidth, mHeight, 0,
			//	(D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2'),
			//	D3DPOOL::D3DPOOL_SYSTEMMEM,
			//	//D3DPOOL::D3DPOOL_DEFAULT,
			//	0, DXVA2_VideoDecoderRenderTarget, &inBuf.pSurface9, nullptr);

			if (FAILED(hr))
			{
				VCELog(TEXT("Failed to create D3D9 surface: %08X"), hr);
				return false;
			}
		}

		D3DLOCKED_RECT rect;
		inBuf.pSurface9->UnlockRect();
		hr = inBuf.pSurface9->LockRect(&rect, nullptr, D3DLOCK_DISCARD /* 0 */);
		if (FAILED(hr))
		{
			VCELog(TEXT("Failed to lock D3D9 surface: %08X"), hr);
			return false;
		}

		inBuf.pBuffer = (uint8_t*)1;
		inBuf.mem_type = amf::AMF_MEMORY_DX9;
		buff->Pitch = rect.Pitch;
		buff->Y = (mfxU8*)rect.pBits;
		buff->UV = (mfxU8*)buff->Y;// +(mHeight * rect.Pitch);

		//Use host memory instead and copy to AMF surface before encoding
		/*
		if (!inBuf.pBuffer)
			inBuf.pBuffer = new uint8_t[mInBuffSize];
		buff->UV = (mfxU8*)buff->Y + (mHeight * mWidth);
		buff->Pitch = mWidth;
		buff->Y = (mfxU8*)inBuf.pBuffer;
		buff->UV = (mfxU8*)buff->Y + (mHeight * mWidth);*/
		buff->MemId = mfxMemId(i + 1);

#if _DEBUG
		OSDebugOut(TEXT("Giving DX9 buffer id %d\n"), i + 1);
#endif
		inBuf.inUse = 1;
		_InterlockedCompareExchange(&(inBuf.locked), 0, 1);
		return true;
	}

	VCELog(TEXT("Max number of input buffers reached."));
	OSDebugOut(TEXT("Max number of input buffers reached.\n"));
	return false;
}

bool VCEEncoder::RequestBuffersCL(LPVOID buffers)
{
	mfxFrameData *buff = (mfxFrameData*)buffers;

	if (buff->MemId && mInputBuffers[(unsigned int)buff->MemId - 1].isMapped
		&& !mInputBuffers[(unsigned int)buff->MemId - 1].locked
		)
		return true;

	//buff->Y = nullptr;
	for (int i = 0; i < MAX_INPUT_BUFFERS; ++i)
	{
		InputBuffer &inBuf = mInputBuffers[i];
		if (/*inBuf.isMapped ||*/
			inBuf.locked ||
			inBuf.inUse)// ||
			//!inBuf.yuv_surfaces[0]) //Out of memory?
			continue;

		cl_int status = CL_SUCCESS;
		cl_image_format imgf;
		imgf.image_channel_order = CL_R;
		imgf.image_channel_data_type = CL_UNSIGNED_INT8;

		//Maybe use clCreateImage
		inBuf.yuv_surfaces[0] = clCreateImage2D(mCLContext,
			//TODO tweak these
			CL_MEM_WRITE_ONLY, //CL_MEM_WRITE_ONLY | CL_MEM_USE_PERSISTENT_MEM_AMD,
			&imgf, mAlignedSurfaceWidth, mAlignedSurfaceHeight, 0, nullptr, &status);
		if (status != CL_SUCCESS)
		{
			VCELog(TEXT("Failed to create Y CL image"));
			return false;
		}

		imgf.image_channel_order = CL_RG;
		imgf.image_channel_data_type = CL_UNSIGNED_INT8;

		inBuf.uv_width = mAlignedSurfaceWidth / 2;
		inBuf.uv_height = (mAlignedSurfaceHeight + 1) / 2;

		mInputBuffers[i].yuv_surfaces[1] = clCreateImage2D(mCLContext,
			CL_MEM_WRITE_ONLY /*| CL_MEM_USE_PERSISTENT_MEM_AMD*/,
			&imgf, inBuf.uv_width, inBuf.uv_height,
			0, nullptr, &status);
		if (status != CL_SUCCESS)
		{
			VCELog(TEXT("Failed to create UV CL image"));
			return false;
		}

		//OSDebugOut(TEXT("Pitches: %d %d\n"), inBuf.yuv_row_pitches[0], inBuf.yuv_row_pitches[1]);

		inBuf.mem_type = amf::AMF_MEMORY_OPENCL;
		buff->Pitch = (mfxU16)inBuf.yuv_row_pitches[0];

		buff->Y = (mfxU8*)inBuf.yuv_surfaces[0];
		buff->UV = (mfxU8*)inBuf.yuv_surfaces[1];
		inBuf.isMapped = true;
		buff->MemId = mfxMemId(i + 1);
		_InterlockedCompareExchange(&(inBuf.inUse), 1, 0);
		_InterlockedCompareExchange(&(inBuf.locked), 0, 1);

#ifdef _DEBUG
		OSDebugOut(TEXT("Send buffer %d\n"), i + 1);
#endif
		return true;
	}

	VCELog(TEXT("Max number of input buffers reached. Meet your doom!"));
	OSDebugOut(TEXT("Max number of input buffers reached. Meet your doom!\n"));
	return false;
}

void VCEEncoder::GetHeaders(DataPacket &packet)
{
	OSMutexLocker locker(mFrameMutex);

	x264_nal_t nalSPS = { 0 }, nalPPS = { 0 };
	const static uint8_t start_seq[] = { 0, 0, 1 };

	// Might be incorrect SPS/PPS? http://devgurus.amd.com/thread/169647
#if 0
	amf::AMFBufferPtr buffer;
	amf::AMFVariant var;
	if (mEncoder->GetProperty(AMF_VIDEO_ENCODER_EXTRADATA, &var) != AMF_OK)
	{
		VCELog(TEXT("Failed to get header property."));
	}

	if (var.type != amf::AMF_VARIANT_INTERFACE ||
		var.pInterface->QueryInterface(amf::AMFBuffer::IID(), (void**)&buffer) != AMF_OK)
	{
		VCELog(TEXT("Failed to get header buffer."));
		//UINT id = OBSAddStreamInfo(TEXT("Failed to get header buffer."), StreamInfoPriority::StreamInfoPriority_Critical);
		//PostMessage(OBSGetMainWindow(), OBS_REQUESTSTOP, 1, 0);
	}
#endif

	if (!mHdrPacket)
	{
		List<DataPacket> packets;
		List<PacketType> types;
		DWORD pts = 0;
		mfxFrameData tmpBuf = { 0 };
		RequestBuffers(&tmpBuf);
		InputBuffer &inBuf = mInputBuffers[(int)tmpBuf.MemId - 1];
		inBuf.frameData = &tmpBuf;
		amf::AMFDataPtr data;
		AMF_RESULT res = AMF_REPEAT;

		do
		{
			SubmitBuffer(&inBuf);
			OSSleep(15);
			CopyOutput(packets, types, pts);
		} while (!mClosing && !mHdrPacket);

		// Clear output for re-init
		do
		{
			amf::AMFDataPtr data;
			res = mEncoder->QueryOutput(&data);
		} while (res == AMF_OK);

		do
		{
			packets.Clear();
			types.Clear();
			CopyOutput(packets, types, pts);
		} while (packets.Num());

		if (!mClosing) // User clicking start/stop too fast
		{
			res = mEncoder->ReInit(mWidth, mHeight);
			LOGIFFAILED(res, TEXT("Failed to reinit encoder."));
			ClearInputBuffer((int)tmpBuf.MemId - 1);
			mReqKeyframe = true;
		}
	}

	uint8_t *start = mHdrPacket;
	uint8_t *end = start + mHdrSize;

	if (start)
	{
		start = std::search(start, end, start_seq, start_seq + 3);

		while (start != end)
		{
			decltype(start) next = std::search(start + 1, end, start_seq, start_seq + 3);

			x264_nal_t nal = { 0 };

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

	//While OVE has 4 byte start codes, MFT/AMF seems to give 3 byte ones
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
	if (nalPPS.p_payload)
		headerOut.Serialize(nalPPS.p_payload + 3, nalPPS.i_payload - 3);

	packet.size = headerPacket.Num();
	packet.lpPacket = headerPacket.Array();
}

//TODO GetSEI
void VCEEncoder::GetSEI(DataPacket &packet)
{
	packet.size = 0;
	packet.lpPacket = (LPBYTE)1;
}

//TODO RequestKeyframe
void VCEEncoder::RequestKeyframe()
{
	OSMutexLocker locker(mFrameMutex);
	mReqKeyframe = true;
#ifdef _DEBUG
	OSDebugOut(TEXT("Keyframe requested\n"));
#endif
}

int VCEEncoder::GetBitRate() const
{
	amf::AMFVariant var;
	if (mEncoder->GetProperty(AMF_VIDEO_ENCODER_TARGET_BITRATE, &var) == AMF_OK)
	{
		return (int)(var.int64Value / 1000);
	}
	return 0;
}

//TODO SetBitRate
bool VCEEncoder::SetBitRate(DWORD maxBitrate, DWORD bufferSize)
{
	bool res = true;
	if (maxBitrate != -1)
	{
		if (mEncoder->SetProperty(AMF_VIDEO_ENCODER_TARGET_BITRATE, (amf_int64)maxBitrate * 1000) == AMF_OK)
			mMaxBitrate = maxBitrate;
		else
			res = false;
	}

	if (bufferSize != -1)
	{
		if (mEncoder->SetProperty(AMF_VIDEO_ENCODER_VBV_BUFFER_SIZE, (amf_int64)bufferSize * 1000) == AMF_OK)
			mBufferSize = bufferSize;
		else
			res = false;
	}

	return res;
}