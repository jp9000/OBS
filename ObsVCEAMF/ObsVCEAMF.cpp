#include "stdafx.h"
#include <../libmfx/include/msdk/include/mfxstructures.h>

//#pragma comment(lib, "dxva2")
#ifndef AMF_CORE_STATIC
#if _WIN64
#pragma comment(lib, "amf-core-windesktop64")
#else
#pragma comment(lib, "amf-core-windesktop32")
#endif
#endif

#define USE_DX11 1
/// Use D3D10 interop with OpenCL
//#define NO_CL_INTEROP 1

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
	ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR, ID3D10Device *d3d10device)
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
	, mD3D10device(d3d10device)
	, mCanInterop(true)
{
	mFrameMutex = OSCreateMutex();
	mIsWin8OrGreater = IsWindows8OrGreater();
	mUseCBR = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseCBR"), 1) != 0;
	mKeyint = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("KeyframeInterval"), 0);

	if (AppConfig->GetInt(TEXT("VCE Settings"), TEXT("NoInterop"), 0) != 0)
	{
		mD3D10device.Release();
		//TODO nulling unnecessery?
		mD3D10device = nullptr;
		mCanInterop = false;
	}

	//OpenCL wants aligned surfaces
	mAlignedSurfaceWidth = ((mWidth + (256 - 1)) & ~(256 - 1));
	mAlignedSurfaceHeight = (mHeight + 31) & ~31;

	mInBuffSize = mAlignedSurfaceWidth * mAlignedSurfaceHeight * 3 / 2;

	memset(mInputBuffers, 0, sizeof(mInputBuffers));
}

VCEEncoder::~VCEEncoder()
{
	while (!mOutputQueue.empty())
	{
		OutputList *out = mOutputQueue.front();
		mOutputQueue.pop();
		delete out; //dtor should call Clear()
	}

	for (int i = 0; i < MAX_INPUT_BUFFERS; i++)
	{
		ClearInputBuffer(i);
	}

	/*clReleaseDevice(mClDevice);
	clReleaseCommandQueue(mCmdQueue);
	clReleaseContext(mCLContext);*/

	if (mEncoder) mEncoder->Terminate();
	////mContext->Terminate();

	for (auto mem : mCLMemObjs)
		clReleaseMemObject(mem);
	mCLMemObjs.clear();

	mTSqueue = {};
	mOCLDevice.Terminate();
	mDX11Device.Terminate();
	mDX9Device.Terminate();
	mD3D10device.Release();
	OSCloseMutex(mFrameMutex);
#ifdef AMF_CORE_STATIC
	UnbindAMF();
#endif
}

void VCEEncoder::ClearInputBuffer(int i)
{
	OSDebugOut(TEXT("Clearing buffer %d type %d\n"), i, mInputBuffers[i].mem_type);
	if (mInputBuffers[i].pBuffer)
	{
		//unmapBuffer(mCmdQueue, mInputBuffers[i]);
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
#ifdef NO_CL_INTEROP
		if (mInputBuffers[i].isMapped)
			unmapImage(mCmdQueue, mInputBuffers[i], k);
#endif
		if (mInputBuffers[i].yuv_surfaces[k])
		{
			clReleaseMemObject(mInputBuffers[i].yuv_surfaces[k]);
			mInputBuffers[i].yuv_surfaces[k] = nullptr;
		}
	}
	if (mInputBuffers[i].amf_surface)
		mInputBuffers[i].amf_surface = nullptr;
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

	// Select engine. DX11 looks be best for D3D10 -> OpenCL -> AMF interop.
	mEngine = (uint32_t)AppConfig->GetInt(TEXT("VCE Settings"), TEXT("AMFEngine"), 2);
	if (mEngine > 2) mEngine = 2;

	if (mEngine == 2) //TODO Maybe default to DX11 on Win8+
	{
		res = mDX11Device.Init(adapterId, false);
		RETURNIFFAILED(res, TEXT("D3D11 device init failed. %d\n"), res);
		res = mContext->InitDX11(mDX11Device.GetDevice(), amf::AMF_DX11_0);
		RETURNIFFAILED(res, TEXT("AMF context init with D3D11 failed. %d\n"), res);
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

	res = mOCLDevice.Init(mDX9Device.GetDevice(), mD3D10device, mDX11Device.GetDevice());
	RETURNIFFAILED(res, TEXT("OpenCL device init failed. %d"), res);

	mCmdQueue = mOCLDevice.GetCommandQueue();
	status = clGetCommandQueueInfo(mCmdQueue, CL_QUEUE_CONTEXT,
		sizeof(mCLContext), &mCLContext, nullptr);
	if (status != CL_SUCCESS)
		VCELog(TEXT("Failed to get CL context from command queue."));

	res = mContext->InitOpenCL(mOCLDevice.GetCommandQueue());
	RETURNIFFAILED(res, TEXT("AMF context init with OpenCL failed. %d\n"), res);

	//-----------------------
	if (mCanInterop && mCmdQueue)
	for (int i = 0; i < MAX_INPUT_BUFFERS; i++)
	{
		InputBuffer &inBuf = mInputBuffers[i];
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
		inBuf.uv_height = mAlignedSurfaceHeight % 2 ? (mAlignedSurfaceHeight + 1) / 2 :
			mAlignedSurfaceHeight / 2;
		OSDebugOut(TEXT("UV size: %dx%d\n"), inBuf.uv_width, inBuf.uv_height);

		mInputBuffers[i].yuv_surfaces[1] = clCreateImage2D(mCLContext,
			CL_MEM_WRITE_ONLY /*| CL_MEM_USE_PERSISTENT_MEM_AMD*/,
			&imgf, inBuf.uv_width, inBuf.uv_height,
			0, nullptr, &status);
		if (status != CL_SUCCESS)
		{
			VCELog(TEXT("Failed to create UV CL image"));
			return false;
		}
	}

	/*************************************************
	*
	* Create AMF encoder and set properties.
	*
	**************************************************/
	res = pAMFCreateComponent(mContext, AMFVideoEncoderVCE_AVC, &mEncoder);
	RETURNIFFAILED(res, TEXT("AMFCreateComponent(encoder) failed. %d"), res);

	// USAGE is a "preset" property so set before anything else
	amf_int64 usage = 0; // AMF_VIDEO_ENCODER_USAGE_ENUM::AMF_VIDEO_ENCODER_USAGE_LOW_LATENCY;//AMF_VIDEO_ENCODER_USAGE_TRANSCONDING; // typos
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

	//-----------------------
	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_PROFILE, (amf_int64)profile);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_PROFILE);
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

	if (mUseCBR)
	{
		res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_ENFORCE_HRD, true);
		LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_ENFORCE_HRD);
	}

	bool bUsePad = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("PadCBR"), 0) != 0;
	if (bUsePad && (mUseCBR || !mUseCFR))
	{
		mEncoder->SetProperty(AMF_VIDEO_ENCODER_FILLER_DATA_ENABLE, true);
		LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_FILLER_DATA_ENABLE);
	}

	int gopSize = mFps * (mKeyint == 0 ? 2 : mKeyint);
	//gopSize -= gopSize % 6;
	USERCFG(gopSize, "GOPSize");
	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_GOP_SIZE, gopSize);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_GOP_SIZE);

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
	AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_ENUM rcm = AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_PEAK_CONSTRAINED_VBR;
	if (mUseCBR)
	{
		rcm = AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_CBR;
		VCELog(TEXT("    CBR (%d)"), rcm);
	}
	else if (mUseCFR)
	{
		rcm = AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_CONSTRAINED_QP;
		VCELog(TEXT("    Constrained QP (%d)"), rcm);
	}
	else
	{
		VCELog(TEXT("    Peak constrained VBR (%d)"), rcm);
	}

	//-----------------------
	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD, rcm);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD);

	int window = 50;
	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_TARGET_BITRATE, mMaxBitrate * 1000);
	RETURNIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_TARGET_BITRATE);

	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_PEAK_BITRATE, mMaxBitrate * (1000 + window));
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_PEAK_BITRATE);

	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_VBV_BUFFER_SIZE, mBufferSize * 1000);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_VBV_BUFFER_SIZE);

	AMFRate fps = { mFps, 1 };
	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_FRAMERATE, fps);
	RETURNIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_FRAMERATE);

	int qp = 23;
	qp = 40 - (mQuality * 5) / 2; // 40 .. 15
	//qp = 22 + (10 - mQuality); //Matching x264 CRF
	VCELog(TEXT("Quality: %d => QP %d"), mQuality, qp);
	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_QP_I, qp);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_QP_I);
	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_QP_P, qp);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_QP_P);
	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_QP_B, qp);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_QP_B);

	iInt = 0;
	USERCFG(iInt, "FrameSkip");
	res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_RATE_CONTROL_SKIP_FRAME_ENABLE, !!iInt);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_RATE_CONTROL_SKIP_FRAME_ENABLE);

	//B frames are not supported by Encode() yet
	//because encoder may need more than one input frame
	//before it returns anything, causing A/V sync issues.
	mEncoder->SetProperty(AMF_VIDEO_ENCODER_B_REFERENCE_ENABLE, false);
	mEncoder->SetProperty(AMF_VIDEO_ENCODER_B_PIC_PATTERN, 0);

	//No point for now
	//res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_HEADER_INSERTION_SPACING, mIDRPeriod < 1001 ? mIDRPeriod : 0);
	//LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_HEADER_INSERTION_SPACING);

	mEncoder->GetProperty(AMF_VIDEO_ENCODER_INTRA_REFRESH_NUM_MBS_PER_SLOT, &mIntraMBs);
	//res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_INTRA_REFRESH_NUM_MBS_PER_SLOT, ((mHeight + 15) & ~15) / 16);
	//LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_INTRA_REFRESH_NUM_MBS_PER_SLOT);
	mEncoder->SetProperty(AMF_VIDEO_ENCODER_DE_BLOCKING_FILTER, true);

	iInt = 18;
	USERCFG(iInt, "MinQP");
	mEncoder->SetProperty(AMF_VIDEO_ENCODER_MIN_QP, iInt);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_MIN_QP);

	iInt = 51;
	USERCFG(iInt, "MaxQP");
	mEncoder->SetProperty(AMF_VIDEO_ENCODER_MAX_QP, iInt);
	LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_MAX_QP);

	//------------------------
	// Print few caps etc.
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
			str << encCaps->GetLevel(i) << " ";
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

	mReqKeyframe = true;
	mAlive = true;
	return true;
}

void VCEEncoder::ConvertD3D10(ID3D10Texture2D *d3dtex, void *data, void **state)
{
	assert(data);
	assert(state);
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

	mOCLDevice.AcquireD3D10(clD3D);
	mOCLDevice.RunKernels(clD3D, inBuf.yuv_surfaces[0], inBuf.yuv_surfaces[1], mWidth, mHeight);
	mOCLDevice.ReleaseD3D10(clD3D);
	clFinish(mOCLDevice.GetCommandQueue());

	amf::AMFSurfacePtr pSurf;
	mContext->CreateSurfaceFromOpenCLNative(amf::AMF_SURFACE_NV12, mWidth, mHeight, 
		(void**)&inBuf.yuv_surfaces, &pSurf, nullptr);
	mSurfaces.push(pSurf);
}

bool VCEEncoder::Stop()
{
	mEncoder->Drain();
	return true;
}

bool VCEEncoder::Encode(LPVOID picIn, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD outputTimestamp, DWORD &out_pts)
{
	AMF_RESULT res = AMF_OK, inRes = AMF_OK;
	amf::AMFSurfacePtr pSurface;
	amf::AMFDataPtr amfdata;

	OSMutexLocker locker(mFrameMutex);

	packets.Clear();
	packetTypes.Clear();

	profileIn("Encode")

	while (!mOutputReadyQueue.empty())
	{
		auto q = mOutputReadyQueue.front();
		mOutputReadyQueue.pop();
		mOutputQueue.push(q);
	}

	if (!picIn)
	{
		Stop();

		res = AMF_REPEAT;
		do
		{
			OSSleep(1);
			res = mEncoder->QueryOutput(&amfdata);
		} while (res == AMF_REPEAT);

		if (res == AMF_OK)
		{
			amf::AMFBufferPtr buf(amfdata);
			ProcessBitstream(buf, packets, packetTypes, outputTimestamp);
		}
		else
			mEOF = true;
		OSDebugOut(TEXT("Drained %d frame(s)\n"), packets.Num());
		return true;
	}

#ifdef _DEBUG
	QWORD start = GetQPCTime100NS();
#endif

	mfxFrameSurface1 *inputSurface = (mfxFrameSurface1*)picIn;
	mfxFrameData &data = inputSurface->Data;
	assert(data.MemId);

	unsigned int idx = (unsigned int)data.MemId - 1;
#ifdef _DEBUG
	//OSDebugOut(TEXT("Encoding buffer: %d\n"), idx + 1);
#endif

	InputBuffer &inBuf = mInputBuffers[idx];
	_InterlockedCompareExchange(&(inBuf.locked), 1, 0);
	//inBuf.timestamp = data.TimeStamp;

	//amf::AMFSurface * pSurface = inBuf.amf_surface;
#ifdef NO_CL_INTEROP
	if (inBuf.mem_type == amf::AMF_MEMORY_OPENCL)
	{
		profileIn("Unmap")
		//unmapBuffer(mCmdQueue, inBuf);
		unmapImage(mCmdQueue, inBuf, 0);
		unmapImage(mCmdQueue, inBuf, 1);
		profileOut
	}
#endif

#pragma region Allocate surface and submit
	//clFinish(mOCLDevice.GetCommandQueue());

	if (mSurfaces.empty() && !AllocateSurface(inBuf, data))
		return false;

	//TODO On win8, creating buffer from OpenCL is sub-millisecond, so no need.
	pSurface = mSurfaces.front();
	mSurfaces.pop();

	if (mReqKeyframe || (mLowLatencyKeyInt > 0 && mCurrFrame >= mLowLatencyKeyInt))
	{
		res = pSurface->SetProperty(AMF_VIDEO_ENCODER_FORCE_PICTURE_TYPE, AMF_VIDEO_ENCODER_PICTURE_TYPE_IDR);
		LOGIFFAILED(res, STR_FAILED_TO_SET_PROPERTY, AMF_VIDEO_ENCODER_FORCE_PICTURE_TYPE);
		mReqKeyframe = true;
		if (mCurrFrame >= mLowLatencyKeyInt)
			mCurrFrame = 0;
		//res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_INTRA_REFRESH_NUM_MBS_PER_SLOT, 0);
		OSDebugOut(TEXT("Keyframe requested %d\n"), res);
	}
	else
		res = pSurface->SetProperty(AMF_VIDEO_ENCODER_FORCE_PICTURE_TYPE, AMF_VIDEO_ENCODER_PICTURE_TYPE_NONE);

	mCurrFrame++;
	pSurface->SetFrameType(amf::AMF_FRAME_PROGRESSIVE);
	pSurface->SetDuration(1000 * MS_TO_100NS / mFps);
	pSurface->SetPts(data.TimeStamp * MS_TO_100NS);
	//mTSqueue.push(data.TimeStamp);

	profileIn("Query output")
	inRes = mEncoder->SubmitInput(pSurface);
	LOGIFFAILED(inRes, TEXT("Failed to sumbit data to encoder: %d %s"), inRes, amf::AMFGetResultText(inRes));
#pragma endregion

	if (mReqKeyframe)
	{
		mReqKeyframe = false;
		//res = mEncoder->SetProperty(AMF_VIDEO_ENCODER_INTRA_REFRESH_NUM_MBS_PER_SLOT, mIntraMBs);
	}

	static bool process = true; //simulate slow encode
	//if (process)
	{
		res = mEncoder->QueryOutput(&amfdata);

		//Really need to empty some queues
		if (res == AMF_REPEAT && inRes == AMF_INPUT_FULL)
		{
			OSDebugOut(TEXT("Input queue is full\n"));
			while (res == AMF_REPEAT)
			{
				//OSSleepMicrosecond(100);
				//OSSleep(1);// 1561 calls per frame my ass
				profileIn("Query output (single)")
				res = mEncoder->QueryOutput(&amfdata);
				profileOut
				if (amfdata) break;
			}
		}

		if (inRes == AMF_INPUT_FULL)
		{
			inRes = mEncoder->SubmitInput(pSurface);
			//TODO possible A/V sync issue then or just dropped frame?
			LOGIFFAILED(inRes, TEXT("Failed to resumbit data to encoder: %d (%s)"), inRes, amf::AMFGetResultText(inRes));
			LOGIFFAILED(inRes, TEXT("Possible A/V sync issues. Try lowering resolution, fps or encoding quality."));
		}

		if (res == AMF_OK)
		{
			amf::AMFBufferPtr buf(amfdata);
			//OSDebugOut(TEXT("Buffer size: %d\n"), buf->GetSize());
			out_pts = buf->GetPts() / MS_TO_100NS;
			ProcessBitstream(buf, packets, packetTypes, outputTimestamp);
		}
		else
			OSDebugOut(TEXT("***********   No output   ***********\n"));
		//OSDebugOut(TEXT("Ready %d %d\n"), mOutputReadyQueue.size(), packets.Num());
	}
	//process = !process;

	profileOut

#ifdef NO_CL_INTEROP
	if (inBuf.mem_type == amf::AMF_MEMORY_OPENCL)
	{
		profileIn("Map")
		mapImage(mCmdQueue, inBuf, 0, mWidth, mHeight);
		mapImage(mCmdQueue, inBuf, 1, inBuf.uv_width, inBuf.uv_height);
		profileOut
	}
#endif

	_InterlockedCompareExchange(&(inBuf.locked), 0, 1);

#ifdef _DEBUG
	start = GetQPCTime100NS() - start;
	OSDebugOut(TEXT("Process time: %fms\n"), float(start) / MS_TO_100NS);
#endif

	profileOut
	return true;
}

inline bool VCEEncoder::AllocateSurface(InputBuffer &inBuf, mfxFrameData &data)
{
	AMF_RESULT res = AMF_FAIL;
	amf::AMFSurfacePtr pSurface;
	amf::AMFPlanePtr plane;

	profileIn("Alloc surface")
	switch (inBuf.mem_type)
	{
		case amf::AMF_MEMORY_DX11:
		{
			res = mContext->AllocSurface(amf::AMF_MEMORY_DX11, amf::AMF_SURFACE_NV12, mWidth, mHeight, &pSurface);
			RETURNIFFAILED(res, TEXT("Failed to allocate surface: %s"), amf::AMFGetResultText(res));
#ifdef _DEBUG
			OSDebugOut(TEXT("AMF_MEMORY_DX11\n"));
#endif
			profileIn("CopyResource (dx11)")
			amf::AMFPlanePtr plane = pSurface->GetPlaneAt(0);
			//Not ref incremented
			ID3D11Texture2D *pTex = (ID3D11Texture2D *)plane->GetNative();
			ID3D11Device *pDevice;
			pTex->GetDevice(&pDevice);
			ID3D11DeviceContext *pD3Dcontext;
			pDevice->GetImmediateContext(&pD3Dcontext);
			pD3Dcontext->Unmap(inBuf.pTex, 0);
			pD3Dcontext->CopyResource(pTex, inBuf.pTex);

			D3D11_MAPPED_SUBRESOURCE map;
			HRESULT hres = pD3Dcontext->Map(inBuf.pTex, 0, D3D11_MAP::D3D11_MAP_WRITE/*_DISCARD*/, 0, &map);
			//TODO remap failure handling
			if (FAILED(hres))
				VCELog(TEXT("Failed to remap D3D11 texture."));

			data.Pitch = map.RowPitch;
			data.Y = (mfxU8*)map.pData;
			data.UV = data.Y + (mHeight * map.RowPitch);

			pD3Dcontext->Release();
			pDevice->Release();
			profileOut
		}
			break;

		case amf::AMF_MEMORY_DX9:

			res = mContext->AllocSurface(amf::AMF_MEMORY_DX9, amf::AMF_SURFACE_NV12, mWidth, mHeight, &pSurface);
			plane = pSurface->GetPlaneAt(0);

			// Copy preallocated surface to AMF. Note: AMF surface has different byte layout :S
			// FIXME Is this IDirect3DSurface9?
			IDirect3DSurface9 *dst;
			dst = static_cast<IDirect3DSurface9*>(plane->GetNative());

			D3DSURFACE_DESC desc;
			dst->GetDesc(&desc);

			//IDirect3DDevice9Ex *container;
			//dst->GetContainer(__uuidof(IDirect3DDevice9Ex), reinterpret_cast<void**>(&container));

			OSDebugOut(TEXT("Dst plane pitch: %dx%d\n"), plane->GetHPitch(), plane->GetVPitch());

			HRESULT hres;
			RECT rct;
			rct.top = 0; rct.left = 0; rct.right = mWidth; rct.bottom = mHeight;
			inBuf.pSurface9->UnlockRect();
			hres = mDX9Device.GetDevice()->UpdateSurface(inBuf.pSurface9, &rct, dst, nullptr);
			if (FAILED(hres))
			{
				VCELog(TEXT("UpdateSurface failed with %08X."), hres);
			}

			D3DLOCKED_RECT lock;
			hres = inBuf.pSurface9->LockRect(&lock, nullptr, 0 /*D3DLOCK_DISCARD*/);
			if (SUCCEEDED(hres))
			{
				data.Pitch = lock.Pitch;
				data.Y = (mfxU8*)lock.pBits;
				data.UV = data.Y;// +(mHeight * data.Pitch);
			}
			else
				VCELog(TEXT("LockRect failed with %08X."), hres);

			//D3DSURFACE_DESC desc;
			//D3DLOCKED_RECT rect;
			//IDirect3DSurface9 *surf;
			//HRESULT hr;
			//surf = static_cast<IDirect3DSurface9*>(plane->GetNative());
			//surf->GetDesc(&desc);
			//hr = surf->LockRect(&rect, nullptr, D3DLOCK_DISCARD /* 0 */);
			//if (FAILED(hr))
			//{
			//	VCELog(TEXT("Failed to lock D3D9 surface: %08X"), hr);
			//	return false;
			//}

			//uint8_t *dst, *src;
			//dst = (uint8_t *)rect.pBits;
			//src = inBuf.pBuffer;

			//for (int i = 0; i < plane->GetHeight(); i++)
			//{
			//	memcpy(dst, src, mWidth);
			//	dst += rect.Pitch;
			//	src += mWidth;
			//}

			//// TODO How come no 0xC0000005?
			//for (int i = 0; i < plane->GetHeight() / 2; i++)
			//{
			//	memcpy(dst, src, mWidth);
			//	dst += rect.Pitch;
			//	src += mWidth;
			//}

			//surf->UnlockRect();
			break;

		case amf::AMF_MEMORY_OPENCL:
			res = mContext->CreateSurfaceFromOpenCLNative(amf::AMF_SURFACE_NV12,
				mWidth, mHeight, (void**)&inBuf.yuv_surfaces, &pSurface, nullptr);
			break;

		case amf::AMF_MEMORY_HOST:

			//TODO doesn't work with NV12?
			res = mContext->CreateSurfaceFromHostNative(amf::AMF_SURFACE_NV12,
				mWidth, mHeight, mWidth, mHeight,
				inBuf.pBuffer, &pSurface, nullptr);
			break;
		default:
			break;
	}

	RETURNIFFAILED(res, TEXT("Failed to allocate surface: %s"), amf::AMFGetResultText(res));
	mSurfaces.push(pSurface);
	profileOut

	return true;
}

void VCEEncoder::ProcessBitstream(amf::AMFBufferPtr &buff, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD outputTimestamp)
{
	profileIn("ProcessBitstream")

	OutputList *bufferedOut = nullptr;

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
		//OSDebugOut(TEXT("nal type: %d\n"), nal.i_type);
	}
	size_t nalNum = nalOut.Num();

	//FIXME
	uint64_t dts = outputTimestamp;
	uint64_t out_pts = buff->GetPts() / MS_TO_100NS;
	//mTSqueue.pop();

	int32_t timeOffset = 0;// int(out_pts - dts);
	/*if (timeOffset == 0)
		frameShift = 0;
	else if (timeOffset < 0)
	{
		timeOffset = frameShift;
		frameShift -= int(out_pts - dts);
	}
	else
	{
		frameShift = 0;
	}*/
	timeOffset += frameShift;

	if (nalNum && timeOffset < 0)
	{
		frameShift -= timeOffset;
		timeOffset = 0;
	}

	/*OSDebugOut(TEXT("Frame dts: %lld pts: %lld ts: %d offset: %d shift: %d\n"), 
					dts, out_pts, outputTimestamp, timeOffset, frameShift);*/

	timeOffset = htonl(timeOffset);
	BYTE *timeOffsetAddr = ((BYTE*)&timeOffset) + 1;

	PacketType bestType = PacketType_VideoDisposable;
	bool bFoundFrame = false;
	bool bSPS = false;
	UINT64 pos = 5;

	for (unsigned int i = 0; i < nalNum; i++)
	{
		x264_nal_t &nal = nalOut[i];
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
		else if (nal.i_type == NAL_AUD || nal.i_type == NAL_FILLER)
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
		else if(false && nal.i_type == NAL_SPS)
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
		else
			continue;
	}

	DataPacket packet;
	packet.lpPacket = bufferedOut->pBuffer.Array();
	packet.size = (UINT)pos; // bufferedOut->pBuffer.Num();
	bufferedOut->timestamp = buff->GetPts() / MS_TO_100NS;

	//if (mOutputQueue.empty())
	//	mOutputQueue.push(bufferedOut);
	mOutputReadyQueue.push(bufferedOut);

	packetTypes << bestType;
	packets << packet;
	nalOut.Clear();

	profileOut
}

//TODO RequestBuffers
void VCEEncoder::RequestBuffers(LPVOID buffers)
{
	if (!buffers || !mAlive)
		return;

	OSMutexLocker locker(mFrameMutex);
	mfxFrameData *buff = (mfxFrameData*)buffers;
	int idx = (int)buff->MemId - 1;
	bool ret = false;
	amf::AMF_MEMORY_TYPE mem_type = amf::AMF_MEMORY_UNKNOWN;
//#ifdef _DEBUG
//	OSDebugOut(TEXT("RequestBuffers: %d\n"), (unsigned int)buff->MemId - 1);
//#endif

	if (idx >= 0)
		mem_type = mInputBuffers[idx].mem_type;

	//TODO CL kernels are only usable with interop currently
	//if (GetFeatures() & VideoEncoder_D3D10Interop)
	if (mCanInterop && mCmdQueue)
	{
		if (mem_type == amf::AMF_MEMORY_UNKNOWN ||
			mem_type == amf::AMF_MEMORY_OPENCL)
			if (RequestBuffersCL(buffers))
				return;

		if (mem_type == amf::AMF_MEMORY_OPENCL)
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

		if (mem_type == amf::AMF_MEMORY_DX11)
			ClearInputBuffer(idx);
	}

	//FIXME IDirect3DSurface9: memory access fail when copying UV bytes
	if (false && mEngine == 1)
	{
		if ((mem_type == amf::AMF_MEMORY_UNKNOWN ||
			mem_type == amf::AMF_MEMORY_DX9))
		{
			if (RequestBuffersDX9(buffers))
				return;
		}

		if (mem_type == amf::AMF_MEMORY_DX9)
			ClearInputBuffer(idx);
	}

	if (mem_type == amf::AMF_MEMORY_UNKNOWN ||
		mem_type == amf::AMF_MEMORY_HOST)
	{
		RequestBuffersHost(buffers);
	}
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
	buff->Y = nullptr;
	for (int i = 0; i < MAX_INPUT_BUFFERS; i++)
	{
		InputBuffer& inBuf = mInputBuffers[i];
		if (inBuf.locked || inBuf.pBuffer)
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
		inBuf.inUse = 1;
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
		mInputBuffers[(unsigned int)buff->MemId - 1].pBuffer
		/*&& mInputBuffers[(unsigned int)buff->MemId - 1].locked*/)
		return true;

	buff->Y = nullptr;
	for (int i = 0; i < MAX_INPUT_BUFFERS; i++)
	{
		InputBuffer& inBuf = mInputBuffers[i];
		if (inBuf.locked || inBuf.pBuffer)
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

		hres = mDX11Device.GetDevice()->CreateTexture2D(&desc, 0, &inBuf.pTex);
		HRETURNIFFAILED(hres, "Failed to create D3D11 texture.");
		D3D11_MAPPED_SUBRESOURCE map;
		hres = d3dcontext->Map(inBuf.pTex, 0, D3D11_MAP::D3D11_MAP_WRITE/*_DISCARD*/, 0, &map);
		HRETURNIFFAILED(hres, "Failed to map D3D11 texture.");

		hres = d3dcontext->Release();
		HRETURNIFFAILED(hres, "Failed to release D3D11 immediate context.");

		inBuf.mem_type = amf::AMF_MEMORY_DX11;
		inBuf.pBuffer = (uint8_t*)1;
		buff->Pitch = map.RowPitch;
		buff->Y = (mfxU8*)map.pData;
		buff->UV = buff->Y + (mHeight * buff->Pitch);

		buff->MemId = mfxMemId(i + 1);

		inBuf.inUse = 1;
		_InterlockedCompareExchange(&(inBuf.locked), 0, 1);
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

	if (buff->MemId &&
		mInputBuffers[(unsigned int)buff->MemId - 1].pBuffer
		&& !mInputBuffers[(unsigned int)buff->MemId - 1].locked)
		return true;

	buff->Y = nullptr;
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

	buff->Y = nullptr;
	for (int i = 0; i < MAX_INPUT_BUFFERS; ++i)
	{
		InputBuffer &inBuf = mInputBuffers[i];
		if (inBuf.isMapped ||
			inBuf.locked ||
			!inBuf.yuv_surfaces[0]) //Out of memory?
			continue;

#ifdef NO_CL_INTEROP
		mapImage(mCmdQueue, inBuf, 0, mWidth, mHeight);
		mapImage(mCmdQueue, inBuf, 1, inBuf.uv_width, inBuf.uv_height);
		if (!inBuf.yuv_host_ptr[0])
			continue;
#endif

		//OSDebugOut(TEXT("Pitches: %d %d\n"), inBuf.yuv_row_pitches[0], inBuf.yuv_row_pitches[1]);

		inBuf.mem_type = amf::AMF_MEMORY_OPENCL;
		buff->Pitch = (mfxU16)inBuf.yuv_row_pitches[0];
#ifdef NO_CL_INTEROP
		buff->Y = (mfxU8*)inBuf.yuv_host_ptr[0];
		buff->UV = (mfxU8*)inBuf.yuv_host_ptr[1];
#else
		buff->Y = (mfxU8*)inBuf.yuv_surfaces[0];
		buff->UV = (mfxU8*)inBuf.yuv_surfaces[1];
		inBuf.isMapped = true;
#endif
		buff->MemId = mfxMemId(i + 1);
		_InterlockedCompareExchange(&(inBuf.locked), 0, 1);

//#ifdef _DEBUG
		OSDebugOut(TEXT("Send buffer %d\n"), i + 1);
//#endif

		return true;
	}

	VCELog(TEXT("Max number of input buffers reached."));
	OSDebugOut(TEXT("Max number of input buffers reached.\n"));
	return false;
}

void VCEEncoder::GetHeaders(DataPacket &packet)
{
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

	x264_nal_t nalSPS = { 0 }, nalPPS = { 0 };
	uint8_t *start = (buffer ? (uint8_t *)buffer->GetNative() : 0);
	uint8_t *end = start + (buffer ? buffer->GetSize() : 0);
	const static uint8_t start_seq[] = { 0, 0, 1 };

	if (start)
	{
		start = std::search(start, end, start_seq, start_seq + 3);

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