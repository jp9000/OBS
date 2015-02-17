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

#undef profileIn
#define profileIn(x) {

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
#ifdef _WIN64
    VCELog(TEXT("If ObsVCE crashes while trying to use OpenCL functions, try 32bit version."));
#endif
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
            || numDevices == 0)
        return false;

    VCELog(TEXT("Seems to have support for AMD VCE."));
    return true;
}

//Keeping it like x264/qsv/nvenc (for now atleast)
extern "C" __declspec(dllexport) VideoEncoder* __cdecl CreateVCEEncoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR, ID3D10Device *d3d10)
{
    if (!initOVE())
        return 0;

    VCEEncoder *enc = new VCEEncoder(fps, width, height, quality, preset, bUse444, colorDesc, maxBitRate, bufferSize, bUseCFR, d3d10);

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
    if (mIsReady)
        return true;

    bool status = false;
    if (!initOVE() || !mOutPtr)
        return false;

    VCELog(TEXT("Build date ") TEXT(__DATE__) TEXT(" ") TEXT(__TIME__));
#define APPCFG(var,cfg) \
    var = static_cast<decltype(var)>(AppConfig->GetInt(TEXT("VCE Settings"), TEXT(cfg), var))
    bool bPadCBR = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("PadCBR"), 0) != 0;
    bool userCfg = AppConfig->GetInt(TEXT("VCE Settings"), TEXT("UseCustom"), 0) != 0;
    int numH = ((mHeight + 15) / 16);

    prepareConfigMap(mConfigTable, false);

    // Choose quality settings
    int size = mWidth * mHeight * mFps;
    if (size <= 1280 * 720 * 60)
        quickSet(mConfigTable, 2);
    else if (size <= 1920 * 1080 * 30)
        quickSet(mConfigTable, 1);
    else
        quickSet(mConfigTable, 0);

    memset(&mConfigCtrl, 0, sizeof (OvConfigCtrl));
    encodeSetParam(&mConfigCtrl, &mConfigTable);

    mConfigCtrl.rateControl.encRateControlFrameRateNumerator = mFps;
    mConfigCtrl.rateControl.encRateControlFrameRateDenominator = 1;
    mConfigCtrl.width = mWidth;
    mConfigCtrl.height = mHeight;

    String x264prof = AppConfig->GetString(TEXT("Video Encoding"), TEXT("X264Profile"));
    int profile = 77;
    if (x264prof.Compare(TEXT("high")))
        profile = 100;
    //Not in advanced setting but for completeness sake
    else if (x264prof.Compare(TEXT("base")))
        profile = 66;

    mConfigCtrl.profileLevel.profile = profile; // 66 - Baseline, 77 - Main, 100 - High

    //Probably forces 1 ref frame only
    mConfigCtrl.priority = OVE_ENCODE_TASK_PRIORITY_LEVEL2;

    mRCM = RCM_CBR;
    if (!userCfg)
    {
        if (!mUseCBR)
            mRCM = RCM_FQP;
    }
    else
    {
        int iInt = 0;
        APPCFG(iInt, "RCM");
        switch (iInt)
        {
        case 1:
        case 2:
            mRCM = RCM_VBR;
            VCELog(TEXT("Using VBR"));
            mUseCBR = false;
            break;
        case 3:
            mRCM = RCM_FQP;
            VCELog(TEXT("Using Fixed QP"));
            mUseCBR = false;
            break;
        default:
            mRCM = RCM_CBR;
            VCELog(TEXT("Using CBR"));
        }
    }

    if (mRCM == RCM_CBR)
    {
        int bitRateWindow = 50;
        int gopSize = mFps * (mKeyint == 0 ? 2 : mKeyint);
        gopSize -= gopSize % 6;
        mManKeyInt = gopSize;
        OSDebugOut(TEXT("gopSize: %d\n"), gopSize);

        mConfigCtrl.profileLevel.level = 50; // 40 - Level 4.0, 51 - Level 5.1
        
        mConfigCtrl.rateControl.encRateControlMethod = RCM_CBR; // 0 - None, 3 - CBR, 4 - VBR
        mConfigCtrl.rateControl.encRateControlTargetBitRate = mMaxBitrate * 1000; // (1000 - bitRateWindow);
        mConfigCtrl.rateControl.encRateControlPeakBitRate = mMaxBitrate * (1000 + bitRateWindow);
        mConfigCtrl.rateControl.encGOPSize = gopSize;
        mConfigCtrl.rateControl.encQP_I = 0;
        mConfigCtrl.rateControl.encQP_P = 0;
        mConfigCtrl.rateControl.encQP_B = 0;
        mConfigCtrl.rateControl.encRCOptions = bPadCBR ? 3 : 0;
        mConfigCtrl.pictControl.encInsertSEIMsg = 7;
        mConfigCtrl.pictControl.encInsertVUIParam = 1;

		//TODO SPS/PPS are not copied
        mConfigCtrl.pictControl.encHeaderInsertionSpacing = mManKeyInt;
		mConfigCtrl.pictControl.encIDRPeriod = 0;// New drivers add IDR every 30 frames, engage manual override
        mConfigCtrl.pictControl.encNumMBsPerSlice = 0;

        /*mConfigCtrl.meControl.encSearchRangeX = 16;
        mConfigCtrl.meControl.encSearchRangeY = 16;
        mConfigCtrl.meControl.encDisableSubMode = 120;
        mConfigCtrl.meControl.encEnImeOverwDisSubm = 1;
        mConfigCtrl.meControl.encImeOverwDisSubmNo = 1;
        mConfigCtrl.meControl.enableAMD = 1;*/
        mConfigCtrl.meControl.encIME2SearchRangeX = 4;
        mConfigCtrl.meControl.encIME2SearchRangeY = 4;
    }
    else
    {
        mConfigCtrl.rateControl.encRateControlTargetBitRate = mMaxBitrate * 1000;
        // Driver resets to bitrate?
        mConfigCtrl.rateControl.encRateControlPeakBitRate = mMaxBitrate * 1000;
        // Driver seems to override this
        mConfigCtrl.rateControl.encVBVBufferSize = mBufferSize * 1000;

        //TODO IDR nice for seeking in local files. Intra-refresh better for streaming?
		mManKeyInt = (mKeyint == 0 ? 2 : mKeyint) * mFps;
		mConfigCtrl.pictControl.encIDRPeriod = 0;
        mConfigCtrl.pictControl.encForceIntraRefresh = 0;
        //TODO Usually 0 to let VCE manage it.
        mConfigCtrl.rateControl.encGOPSize = mKeyint * mFps;
        mConfigCtrl.priority = OVE_ENCODE_TASK_PRIORITY_LEVEL1;
        if (bPadCBR) //TODO Serves same purpose? Drops too much still
            mConfigCtrl.rateControl.encRCOptions = 0x3;

        int QP = 23;
        QP = 40 - (mQuality * 5) / 2; // 40 .. 15
        //QP = 22 + (10 - mQuality); //Matching x264 CRF
        VCELog(TEXT("Quality: %d => QP %d"), mQuality, QP);

        mConfigCtrl.rateControl.encQP_I = mConfigCtrl.rateControl.encQP_P = QP;
        mConfigCtrl.rateControl.encQP_B = QP;

        mConfigCtrl.rateControl.encRateControlMethod = (unsigned int)mRCM;

        int numMBs = ((mWidth + 15) / 16) * numH;
		mConfigCtrl.pictControl.encNumMBsPerSlice = 0;// numMBs;
    }

    VCELog(TEXT("Rate control method: %d"), mConfigCtrl.rateControl.encRateControlMethod);
    VCELog(TEXT("Frame rate: %d"), mConfigCtrl.rateControl.encRateControlFrameRateNumerator);
    //WTF, why half the pixels
    mConfigCtrl.pictControl.encCropBottomOffset = (numH * 16 - mHeight) >> 1;

    //Apply user's custom setting
    if (userCfg)
    {
        int iOpposite = !mConfigCtrl.meControl.disableFavorPMVPoint;
        APPCFG(iOpposite, "FavorPMV");
        mConfigCtrl.meControl.disableFavorPMVPoint = !iOpposite;
        APPCFG(mConfigCtrl.meControl.enableAMD, "AMD");
        APPCFG(mConfigCtrl.meControl.disableSATD, "DisSATD");
        APPCFG(mConfigCtrl.meControl.encDisableSubMode, "RDODisSub");
        APPCFG(mConfigCtrl.meControl.encEnImeOverwDisSubm, "IMEOverwrite");
        APPCFG(mConfigCtrl.meControl.encImeOverwDisSubmNo, "IMEDisSubmNo");
        APPCFG(mConfigCtrl.meControl.encIME2SearchRangeX, "IME2SearchX");
        APPCFG(mConfigCtrl.meControl.encIME2SearchRangeY, "IME2SearchY");
        APPCFG(mConfigCtrl.meControl.encSearchRangeX, "SearchX");
        APPCFG(mConfigCtrl.meControl.encSearchRangeY, "SearchY");
        APPCFG(mConfigCtrl.meControl.forceZeroPointCenter, "ForceZPC");
        APPCFG(mConfigCtrl.meControl.imeDecimationSearch, "IMEDecimation");
        APPCFG(mConfigCtrl.meControl.lsmVert, "LSMVert");
        APPCFG(mConfigCtrl.meControl.motionEstHalfPixel, "MEHalf");
        APPCFG(mConfigCtrl.meControl.motionEstQuarterPixel, "MEQuarter");
        APPCFG(mConfigCtrl.rateControl.encQP_I, "QPI");
        APPCFG(mConfigCtrl.rateControl.encQP_P, "QPP");
        APPCFG(mConfigCtrl.rateControl.encQP_B, "QPB");

        APPCFG(mConfigCtrl.rdoControl.encDisableTbePredIFrame, "IPred");
        APPCFG(mConfigCtrl.rdoControl.encDisableTbePredPFrame, "PPred");
        APPCFG(mConfigCtrl.rdoControl.encForce16x16skip, "Force16x16Skip");
        APPCFG(mConfigCtrl.rdoControl.encSkipCostAdj, "SkipCostAdj"); //Not in cfg dialog

        APPCFG(mConfigCtrl.pictControl.cabacEnable, "CABAC");
		//APPCFG(mConfigCtrl.pictControl.encIDRPeriod, "IDRPeriod");
		APPCFG(mManKeyInt, "IDRPeriod");
        APPCFG(mConfigCtrl.pictControl.encIPicPeriod, "IPicPeriod");
        APPCFG(mConfigCtrl.pictControl.useConstrainedIntraPred, "ConstIntraPred");

        APPCFG(mConfigCtrl.rateControl.encGOPSize, "GOPSize");
    }

    delete[] mDeviceHandle.deviceInfo;
    mDeviceHandle.deviceInfo = NULL;

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

    uint32_t devIdx = (uint32_t)AppConfig->GetInt(TEXT("VCE Settings"), TEXT("DevIndex"), 0);
    int devTopoId = (int)AppConfig->GetHex(TEXT("VCE Settings"), TEXT("DevTopoId"), -1);
    bool devNameLogged = false;

    if (devIdx >= mDeviceHandle.numDevices)
        devIdx = 0;
    uint32_t device = mDeviceHandle.deviceInfo[devIdx].device_id;
    for (uint32_t i = 0; i < mDeviceHandle.numDevices; i++)
    {
        cl_device_id clDeviceID = 
            reinterpret_cast<cl_device_id>(mDeviceHandle.deviceInfo[i].device_id);

#ifdef _WIN64
        intptr_t ptr = intptr_t((intptr_t*)&clDeviceID);
        clDeviceID = (cl_device_id)((intptr_t)clDeviceID | (ptr & 0xFFFFFFFF00000000));
#endif

        int id = GetTopologyId(clDeviceID);
        if (id > -1)
        {
            VCELog(TEXT("Device %d Topology : PCI[B#%d, D#%d, F#%d] : TopoID: 0x%06X"),
                i, (id >> 16) & 0xFF, (id >> 8) & 0xFF, (id & 0xFF), id);

            if (devTopoId == id)
            {
                device = mDeviceHandle.deviceInfo[i].device_id;
                // print device name
                size_t valueSize;
                f_clGetDeviceInfo(clDeviceID, CL_DEVICE_NAME, 0, NULL, &valueSize);
                char* value = new char[valueSize];
                f_clGetDeviceInfo(clDeviceID, CL_DEVICE_NAME, valueSize, value, NULL);
                //TODO non-unicode string formatting
                VCELog(TEXT("Topology id match, using device %d (%S)"), i, value);
                delete[] value;
                devNameLogged = true;
            }
        }
    }

    if (!devNameLogged)
    {
        cl_device_id clDeviceID = reinterpret_cast<cl_device_id>(device);

#ifdef _WIN64
        // May ${DEITY} have mercy on us all.
        intptr_t ptr = intptr_t((intptr_t*)&clDeviceID);
        clDeviceID = (cl_device_id)((intptr_t)clDeviceID | (ptr & 0xFFFFFFFF00000000));
#endif

        // print device name
        size_t valueSize;
        f_clGetDeviceInfo(clDeviceID, CL_DEVICE_NAME, 0, NULL, &valueSize);
        char* value = (char*)malloc(valueSize);
        f_clGetDeviceInfo(clDeviceID, CL_DEVICE_NAME, valueSize, value, NULL);
        //TODO non-unicode string formatting
        VCELog(TEXT("Using device %d (%S)"), devIdx, value);
        free(value);
    }

    if (!encodeCreate(device))
    {
        //Cleanup
        encodeClose();
        return false;
    }

    mIsReady = true;
    if (mCanInterop)
        mUse444 = false;
    else
        Warmup();
    return true;
}

VCEEncoder::VCEEncoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR, ID3D10Device *d3d10)
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
    , mOveContext(NULL)
    , mOutput(NULL)
    , mProgram(NULL)
    , mLastTS(0)
    , mOutPtr(NULL)
    , mStatsOutSize(0)
    , mStartTime(0)
    , mKeyNum(0)
    , mCurrBucketSize(0)
    , mMaxBucketSize(0)
    , mDesiredPacketSize(0)
    , mCurrFrame(0)
    , mD3D10Device(d3d10)
    , mCanInterop(false)
{
    frameMutex = OSCreateMutex();

    mKernel[0] = NULL;
    mKernel[1] = NULL;

    mUseCBR = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("UseCBR"), 1) != 0;
    mKeyint = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("KeyframeInterval"), 0);
    if(AppConfig->GetInt(TEXT("VCE Settings"), TEXT("NoInterop"), 0) != 0)
        mD3D10Device = nullptr;

    headerPacket.SetSize(128);
    memset(&mDeviceHandle, 0, sizeof(mDeviceHandle));
    memset(&mEncodeHandle, 0, sizeof(mEncodeHandle));

    mAlignedSurfaceWidth = ((width + (256 - 1)) & ~(256 - 1));
    mAlignedSurfaceHeight = (true) ? (height + 31) & ~31 :
        (height + 15) & ~15;

    // NV12 is 3/2
    if (!mUse444)
        mInputBufSize = mAlignedSurfaceHeight * mAlignedSurfaceWidth * 3 / 2;
    else
    {
        //VCELog(TEXT("Using YUV444"));
        mInputBufSize = mHeight * mAlignedSurfaceWidth * 4;
        mOutputBufSize = mAlignedSurfaceHeight * mAlignedSurfaceWidth * 3 / 2;
    }

    mOutPtrSize = 1 * 1024 * 1024;
    mOutPtr = malloc(mOutPtrSize);

    bool bUsePad = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("PadCBR"), 0) != 0;
    //bool bAddFiller = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("AddFiller"), 1) != 0;
    if (bUsePad /* && bAddFiller*/)
    {
        mMaxBucketSize = mMaxBitrate * (1000 / 8);
        mDesiredPacketSize = mMaxBucketSize / mFps;
    }

    if (mD3D10Device) mD3D10Device->AddRef();
}

VCEEncoder::~VCEEncoder()
{
    encodeClose();

    cl_int err;
    if ((cl_context)mOveContext)
    {
        err = f_clReleaseContext((cl_context)mOveContext);
        if (err != CL_SUCCESS)
        {
            VCELog(TEXT("Error releasing cl context"));
        }
    }
    mOveContext = NULL;

    delete[] mDeviceHandle.deviceInfo;
    mDeviceHandle.deviceInfo = NULL;

    free(mHdrPacket);
    mHdrPacket = NULL;

    free(mOutPtr);
    mOutPtr = NULL;

    encoderRefDec();
    OSCloseMutex(frameMutex);

    if (mD3D10Device)
        mD3D10Device->Release();
}

void VCEEncoder::Warmup()
{
//#define WARMUP_ENCODE2
    mfxFrameSurface1 tmp;
#ifdef WARMUP_ENCODE2
    List<DataPacket> vidPackets;
    List<PacketType> packetTypes;
    DWORD out_pts;
#endif

#ifdef WARMUP_ENCODE2
    /// Try aligning to header insertion interval
    /// so SPS/PPS/IDR NALs are together
    int frames = mManKeyInt / MAX_INPUT_SURFACE;
#else
    int frames = 2;
#endif

    for (int j = 0; j < frames; j++)
    {
        for (int i = 0; i < MAX_INPUT_SURFACE; i++)
        {
            ZeroMemory(&tmp, sizeof(mfxFrameSurface1));
            RequestBuffers(&(tmp.Data));

#ifdef WARMUP_ENCODE2
            /// Hmm, still first Encode call with 1080p
            /// frame is still over 20ms.
            Encode(&tmp, vidPackets, packetTypes, 0, out_pts);
#else
            /// Only do a conversion
            if (mUse444)
                ConvertYUV444(i);
#endif
        }

        // Clean up like nothing happened
        for (int i = 0; i < MAX_INPUT_SURFACE; i++)
        {
            unmapBuffer(mEncodeHandle, i);
        }
    }

#ifdef WARMUP_ENCODE2
    for (int i = 0; i < mManKeyInt % MAX_INPUT_SURFACE; i++)
    {
        ZeroMemory(&tmp, sizeof(mfxFrameSurface1));
        RequestBuffers(&(tmp.Data));
        Encode(&tmp, vidPackets, packetTypes, 0, out_pts);
        unmapBuffer(mEncodeHandle, i);
    }
#endif

    mReqKeyframe = true;
    mFirstFrame = true;
}

void VCEEncoder::ConvertD3DTex(ID3D10Texture2D *d3dtex, void *data, void **state)
{
    assert(data);
    assert(state);
    mfxFrameData *mfx = static_cast<mfxFrameData*>(data);
    int idx = (int)(mfx->MemId);
    assert(idx);
    OPSurface &inBuf = mEncodeHandle.inputSurfaces[idx - 1];

    cl_mem clD3D = static_cast<cl_mem>(*state);
    if (!clD3D)
    {
        clD3D = CreateTexture2D(d3dtex);
        *state = clD3D;
        assert(clD3D);
        mCLMemObjs.push_back(clD3D);
    }

    AcquireD3DObj(clD3D);
    RunKernels(clD3D, (cl_mem)inBuf.surface, mWidth, mHeight);
    ReleaseD3DObj(clD3D);
    f_clFinish(mEncodeHandle.clCmdQueue[0]);
}

bool VCEEncoder::ConvertYUV444(int idx)
{
    profileIn("YUV444 conversion")
    return RunKernels((cl_mem)mEncodeHandle.inputSurfaces[idx].surface,
                    mOutput, mWidth, mHeight);
    profileOut
}

bool VCEEncoder::Encode(LPVOID picIn, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD timestamp, DWORD &out_pts)
{
    if (!mIsReady)
        return false;
    QWORD start = GetQPCTime100NS();
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

    if (!picIn)
        return true;

    profileIn("VCE encode")
    mfxFrameSurface1 *inputSurface = (mfxFrameSurface1*)picIn;
    mfxFrameData &data = inputSurface->Data;
    assert(data.MemId);

    unsigned int idx = (unsigned int)data.MemId - 1;

#ifdef _DEBUG
    if (mStartTime == 0)
        mStartTime = GetQPCTimeMS();
    //OSDebugOut(TEXT("Encoding buffer: %d ts:%d %lld\n"), idx, timestamp, data.TimeStamp);
#endif
    mLastTS = (uint64_t)data.TimeStamp;
    out_pts = timestamp;

    //mEncodeHandle.inputSurfaces[idx].locked = true;
    _InterlockedCompareExchange(&(mEncodeHandle.inputSurfaces[idx].locked), 1, 0);

    encodeTaskInputBufferList[0].bufferType = OVE_BUFFER_TYPE_PICTURE;
    encodeTaskInputBufferList[0].buffer.pPicture = (OVE_SURFACE_HANDLE)
        mUse444 ? mOutput : mEncodeHandle.inputSurfaces[idx].surface;

    //profileIn("Unmap buffer")
    if(!mCanInterop) unmapBuffer(mEncodeHandle, idx);
    //profileOut

    memset(&pictureParameter, 0, sizeof(OVE_ENCODE_PARAMETERS_H264));
    pictureParameter.size = sizeof(OVE_ENCODE_PARAMETERS_H264);
    pictureParameter.flags.value = 0;
    pictureParameter.flags.flags.reserved = 0;
    pictureParameter.insertSPS = (OVE_BOOL)mFirstFrame;
    pictureParameter.pictureStructure = OVE_PICTURE_STRUCTURE_H264_FRAME;
    pictureParameter.forceRefreshMap = (OVE_BOOL)true;
    //pictureParameter.forceIMBPeriod = 0;
    //pictureParameter.forcePicType = OVE_PICTURE_TYPE_H264_NONE;

    if (mReqKeyframe || mFirstFrame)
    {
        mReqKeyframe = false;
        pictureParameter.forcePicType = OVE_PICTURE_TYPE_H264_IDR;
    }

    if (/*!mUseCBR &&*/ mKeyNum >= mManKeyInt)
    {
        pictureParameter.forcePicType = OVE_PICTURE_TYPE_H264_IDR;
        mKeyNum = 0;
    }
    mKeyNum++;

    if (mUse444)
    {
        ConvertYUV444(idx);
    }

    //profileIn("Queue encode task")
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
        VCELog(TEXT("OVEncodeTask returned error %d"), res);
        ret = false;
        goto fail;
    }
    //profileOut

    profileIn("Wait for task(s)")
    //wait for encoder
    err = f_clWaitForEvents(1, (cl_event *)&(eventRunVideoProgram));
    if (err != CL_SUCCESS)
    {
        VCELog(TEXT("clWaitForEvents returned error 0x%08x"), err);
        ret = false;
        goto fail;
    }
    //profileOut

    //profileIn("Query task(s) completion")
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
            VCELog(TEXT("OVEncodeQueryTaskDescription returned error %d"), err);
            ret = false;
            goto fail;
        }

    } while (pTaskDescriptionList->status == OVE_TASK_STATUS_NONE);
    profileOut

    //profileIn("Remap buffer")
    if (!mCanInterop) mapBuffer(mEncodeHandle, idx, mInputBufSize);
    //profileOut

    //mEncodeHandle.inputSurfaces[idx].locked = false;
    _InterlockedCompareExchange(&(mEncodeHandle.inputSurfaces[idx].locked), 0, 1);

    profileIn("Get bitstream")
    //Copy data out from VCE
    for (uint32_t i = 0; i<numTaskDescriptionsReturned; i++)
    {
        if ((pTaskDescriptionList[i].status == OVE_TASK_STATUS_COMPLETE)
            && pTaskDescriptionList[i].size_of_bitstream_data > 0)
        {
            ProcessOutput(&pTaskDescriptionList[i], packets, packetTypes, data.TimeStamp);
           if (!mHdrPacket)
            {
                mHdrSize = pTaskDescriptionList[i].size_of_bitstream_data;
                mHdrPacket = (char*)malloc(mHdrSize);
                memcpy(mHdrPacket, pTaskDescriptionList[i].bitstream_data, mHdrSize);
            }
#ifdef _DEBUG
            if (mStatsOutSize < pTaskDescriptionList[i].size_of_bitstream_data)
            {
                mStatsOutSize = pTaskDescriptionList[i].size_of_bitstream_data;
                OSDebugOut(TEXT("New max bitstream size: %d\n"), mStatsOutSize);
            }
#endif
            res = OVEncodeReleaseTask(mEncodeHandle.session, pTaskDescriptionList[i].taskID);
        }
    }
    profileOut

fail:
    if (eventRunVideoProgram)
        f_clReleaseEvent((cl_event)eventRunVideoProgram);

    mFirstFrame = false;
    profileOut

#ifdef _DEBUG
    static int logCount = 0;
    static QWORD logAvg = 0;
    static QWORD logMax = 0;
    logCount++;
    start = GetQPCTime100NS() - start;
    if (start > logMax)
        logMax = start;
    logAvg += start;
    if (logCount >= 5)
    {
        OSDebugOut(TEXT("In Encode for avg %lld, max %lld\n"), logAvg/logCount, logMax);
        logMax = 0;
        logAvg = 0;
        logCount = 0;
    }
#endif

    return ret;
}

void VCEEncoder::ProcessOutput(OVE_OUTPUT_DESCRIPTION *surf, List<DataPacket> &packets, List<PacketType> &packetTypes, uint64_t timestamp)
{
    PacketType bestType = PacketType_VideoDisposable;
    bool bFoundFrame = false;
    encodeData.Clear();

    //Just in case
    if (!mOutPtr || surf->size_of_bitstream_data > mOutPtrSize)
    {
        VCELog(TEXT("Bitstream size larger than %d, reallocating."), mOutPtrSize);
        mOutPtrSize = surf->size_of_bitstream_data;
        mOutPtr = realloc(mOutPtr, mOutPtrSize);
        if (!mOutPtr)
        {
            VCELog(TEXT("Could not realloc output buffer: %d bytes"), mOutPtrSize);
            mOutPtrSize = 0;
            mReqKeyframe = true;
            return;
        }
    }

    memcpy(mOutPtr, surf->bitstream_data, surf->size_of_bitstream_data);

    uint8_t *start = (uint8_t*)mOutPtr;
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
        //OSDebugOut(TEXT("%d NAL type: %d\n"), nalOut.Num(), nal.i_type);

        if (!bFoundFrame && (nal.i_type == NAL_SLICE_IDR || nal.i_type == NAL_SLICE))
        {
            BYTE *skip = nal.p_payload;
            while (*(skip++) != 0x1);
            int skipBytes = (int)(skip - nal.p_payload);
            encodeData.Insert(0, (nal.i_type == NAL_SLICE_IDR) ? 0x17 : 0x27);
            encodeData.Insert(1, 1);
            bFoundFrame = true;
        }
    }
    size_t nalNum = nalOut.Num();

    //uint64_t dts = timestamp;
    //uint64_t out_pts = timestamp;

    //FIXME VCE outputs I/P frames thus pts and dts can be the same? timeOffset needed with B-frames?
    int32_t timeOffset = 0;// int(out_pts - dts);
    timeOffset += frameShift;

    if (nalNum && timeOffset < 0)
    {
        frameShift -= timeOffset;
        timeOffset = 0;
    }

    //timeOffset = htonl(timeOffset);
    BYTE *timeOffsetAddr = ((BYTE*)&timeOffset) + 1;

    if (bFoundFrame)
        encodeData.InsertArray(2, timeOffsetAddr, 3);

    profileIn("Parse bitstream")

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

    //Add filler
    if (mUseCBR && mDesiredPacketSize > 0)
    {
        if (mCurrFrame == 0)
        {
            //Random, trying to get OBS bitrate meter stay at mMaxBitrate
            if (mCurrBucketSize < -mMaxBucketSize)
                mCurrBucketSize = 0;
            mCurrBucketSize = mMaxBucketSize -
                (mCurrBucketSize < 0 ? -mCurrBucketSize : mCurrBucketSize);
        }

        if (mDesiredPacketSize > encodeData.Num() + 5
            && mCurrBucketSize > 0)
        {

            uint32_t fillerSize = mDesiredPacketSize - encodeData.Num() - 5;
            BufferOutputSerializer packetOut(encodeData);

            packetOut.OutputDword(htonl(fillerSize));
            packetOut.OutputByte(NAL_FILLER);//Disposable i_ref_idc

            uint32_t currSize = encodeData.Num();
            encodeData.SetSize(currSize + fillerSize);
            BYTE *ptr = encodeData.Array() + currSize;
            memset(ptr, 0xFF, fillerSize);
            *(ptr + fillerSize) = 0x80;

            mCurrBucketSize -= mDesiredPacketSize;
        }
        else
        {
            mCurrBucketSize -= encodeData.Num();
        }

        mCurrFrame++;
        if (mCurrFrame >= mFps)
            mCurrFrame = 0;
    }

    DataPacket packet;
    packet.lpPacket = encodeData.Array();
    packet.size = encodeData.Num();

    packetTypes << bestType;
    packets << packet;
    profileOut
}

void VCEEncoder::RequestBuffers(LPVOID buffers)
{
    cl_event inMapEvt = 0;
    cl_int   status = CL_SUCCESS;

    if (!buffers || !mIsReady)
        return;

    mfxFrameData *buff = (mfxFrameData*)buffers;

#ifdef _DEBUG
    //OSDebugOut(TEXT("RequestBuffers: %d\n"), (unsigned int)buff->MemId - 1);
#endif

    //TODO Threaded NV12 conversion asks same buffer (thread count) times?
    if (buff->MemId && mEncodeHandle.inputSurfaces[(unsigned int)buff->MemId - 1].isMapped
        && !mEncodeHandle.inputSurfaces[(unsigned int)buff->MemId - 1].locked
        )
        return;

    //Trying atomic inputSurface.locked instead
    //OSMutexLocker locker(frameMutex);

    for (int i = 0; i < MAX_INPUT_SURFACE; ++i)
    {
        OPSurface &inBuf = mEncodeHandle.inputSurfaces[i];
        if (inBuf.isMapped || inBuf.locked ||
            !inBuf.surface) //Out of memory?
            continue;

        if (!mCanInterop)
        {
            mapBuffer(mEncodeHandle, i, mInputBufSize);

            if (!inBuf.mapPtr)
                continue;

            buff->Pitch = mUse444 ? mAlignedSurfaceWidth * 4 : mAlignedSurfaceWidth;
            buff->Y = (mfxU8*)inBuf.mapPtr;
            buff->UV = buff->Y + (mHeight * buff->Pitch);
        }
        else
        {
            buff->Pitch = mAlignedSurfaceWidth;
            buff->Y = (mfxU8*)inBuf.surface;
            buff->UV = nullptr;
            inBuf.isMapped = true;
        }

        buff->MemId = mfxMemId(i + 1);
        //inBuf.locked = 0; //false
        _InterlockedCompareExchange(&(inBuf.locked), 0, 1);

#ifdef _DEBUG
        OSDebugOut(TEXT("Send buffer %d\n"), i + 1);
#endif

        return;
    }
}

//TODO GetBitRate
int  VCEEncoder::GetBitRate() const
{
    return mMaxBitrate;
}

//TODO DynamicBitrateSupported
bool VCEEncoder::DynamicBitrateSupported() const
{
    return false;
}

//TODO SetBitRate
bool VCEEncoder::SetBitRate(DWORD maxBitrate, DWORD bufferSize)
{
    OSMutexLocker locker(frameMutex);

    if (maxBitrate > 0)
    {
        mMaxBitrate = maxBitrate;
        mConfigCtrl.rateControl.encRateControlTargetBitRate = maxBitrate * 1000;
        //TODO set encVBVBufferSize if zero?
        //if (bufferSize == 0)
        //    mConfigCtrl.rateControl.encVBVBufferSize = maxBitrate * 1000 / 2;
    }

    if (bufferSize > 0)
    {
        mBufferSize = bufferSize;
        mConfigCtrl.rateControl.encVBVBufferSize = bufferSize * 1000;
    }

    //TODO Doesn't look like it takes effect
    setEncodeConfig(mEncodeHandle.session, &mConfigCtrl);

    return true;
}

void VCEEncoder::GetHeaders(DataPacket &packet)
{
    if (!mHdrPacket)
    {
        VCELog(TEXT("No header packet yet. Generating..."));
        // Seems like an only way to get SPS/PPS from OVE
        List<DataPacket> vidPackets;
        List<PacketType> packetTypes;
        mfxFrameSurface1 tmp;
        DWORD out_pts;
        mFirstFrame = true;

        ZeroMemory(&tmp, sizeof(mfxFrameSurface1));
        RequestBuffers(&(tmp.Data));
        Encode(&tmp, vidPackets, packetTypes, 0, out_pts);
        // Clean up like nothing happened
        unmapBuffer(mEncodeHandle, (int)tmp.Data.MemId - 1);
        mReqKeyframe = true;
        mFirstFrame = true;
    }

    uint32_t outSize = mHdrSize;
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
    OSDebugOut(TEXT("Keyframe requested\n"));
#endif
}
