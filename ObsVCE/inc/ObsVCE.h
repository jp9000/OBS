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

#pragma once
#include "Main.h"
using std::map;
using std::string;

//TODO Some optimal count
#define MAX_INPUT_SURFACE      3

#define VCELog(...) Log(__VA_ARGS__)
#define AppConfig (*VCEAppConfig)

#include "OVEncodeDyn.h"

#define OBSVCE_API __declspec(dllexport) __cdecl

#define htons(n) (((((uint16_t)(n) & 0xFF)) << 8) | (((uint16_t)(n) & 0xFF00) >> 8))
#define htonl(n) (((((uint32_t)(n) & 0xFF)) << 24) | \
    ((((uint32_t)(n)& 0xFF00)) << 8) | \
    ((((uint32_t)(n)& 0xFF0000)) >> 8) | \
    ((((uint32_t)(n)& 0xFF000000)) >> 24))

enum RateControlMode
{
    RCM_FQP = 0, //fixed qp
    RCM_CBR = 3,
    RCM_VBR = 4,
    //RCM_SOMETHING = 5 //VCE 2.0 accepts it, seems like vbr
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

typedef struct _OVConfigCtrl
{
    uint32_t                        height;
    uint32_t                        width;
    OVE_ENCODE_MODE                 encodeMode;

    OVE_PROFILE_LEVEL               profileLevel; /**< Profile Level                       */

    OVE_PICTURE_FORMAT              pictFormat;   /**< Profile format                      */
    OVE_ENCODE_TASK_PRIORITY        priority;     /**< priority settings                   */

    OVE_CONFIG_PICTURE_CONTROL      pictControl;  /**< Picture control                     */
    OVE_CONFIG_RATE_CONTROL         rateControl;  /**< Rate contorl config                 */
    OVE_CONFIG_MOTION_ESTIMATION    meControl;    /**< Motion Estimation settings          */
    OVE_CONFIG_RDO                  rdoControl;   /**< Rate distorsion optimization control*/
} OvConfigCtrl;//, far * pConfig;

typedef struct OVDeviceHandle
{
    ovencode_device_info *deviceInfo; /**< Pointer to device info        */
    uint32_t              numDevices; /**< Number of devices available   */
    cl_platform_id        platform;   /**< Platform                      */
}OVDeviceHandle;

typedef struct OPSurface
{
    OPMemHandle surface;
    bool isMapped;
    LONG locked;
    void *mapPtr;
} OPSurface;

typedef struct OVEncodeHandle
{
    ove_session          session;       /**< Pointer to encoder session   */
    //OPMemHandle          inputSurfaces[MAX_INPUT_SURFACE]; /**< input buffer  */
    OPSurface            inputSurfaces[MAX_INPUT_SURFACE]; /**< input buffer  */
    cl_command_queue     clCmdQueue[2];    /**< command queue  */
}OVEncodeHandle;

void mapBuffer(OVEncodeHandle &encodeHandle, int i, uint32_t size);
void unmapBuffer(OVEncodeHandle &encodeHandle, int surf);

//config.cpp
void quickSet(map<string, int32_t> &configTable, int qs);
void prepareConfigMap(map<string, int32_t> &configTable, bool quickset);
void encodeSetParam(OvConfigCtrl *pConfig, map<string, int32_t>* pConfigTable);
bool setEncodeConfig(ove_session session, OvConfigCtrl *pConfig);

//device.cpp
void waitForEvent(cl_event inMapEvt);
bool gpuCheck(cl_platform_id platform, cl_device_type* dType);
bool getPlatform(cl_platform_id &platform);
bool getDevice(OVDeviceHandle *deviceHandle);
bool getDeviceInfo(ovencode_device_info **deviceInfo,
    uint32_t *numDevices);
bool getDeviceCap(OPContextHandle oveContext, uint32_t oveDeviceID,
    OVE_ENCODE_CAPS *encodeCaps);

class VCEEncoder : public VideoEncoder
{
protected:
    bool Encode(LPVOID picIn, List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD timestamp, DWORD &out_pts);

    void RequestBuffers(LPVOID buffers);

public:
    VCEEncoder(int fps, int width, int height, int quality, CTSTR preset, bool bUse444, ColorDescription &colorDesc, int maxBitRate, int bufferSize, bool bUseCFR);
    ~VCEEncoder();

    int  GetBitRate() const;
    bool DynamicBitrateSupported() const;
    bool SetBitRate(DWORD maxBitrate, DWORD bufferSize);

    void GetHeaders(DataPacket &packet);
    void GetSEI(DataPacket &packet) { packet.size = 0; packet.lpPacket = (LPBYTE)1; }

    void RequestKeyframe();

    String GetInfoString() const { return String(); };

    bool isQSV() { return true; }

    int GetBufferedFrames() { if (HasBufferedFrames()) return -1; return 0; }
    bool HasBufferedFrames() { return false; }

    bool init();
private:

	// Warm up OpenCL buffers etc.
    void Warmup();
	bool YUV444Convert(int bufferidx);
    bool encodeCreate(uint32_t deviceId);
    bool encodeOpen(uint32_t deviceId);
    bool encodeClose();
    void ProcessOutput(OVE_OUTPUT_DESCRIPTION *surf, List<DataPacket> &packets, List<PacketType> &packetTypes, uint64_t timestamp);

    HANDLE frameMutex;

    OVDeviceHandle      mDeviceHandle;
    OPContextHandle     mOveContext;
    OVEncodeHandle      mEncodeHandle;

    OvConfigCtrl            mConfigCtrl;
    map<string, int32_t>    mConfigTable;
    List<BYTE> encodeData, headerPacket, seiData;

    void       *mOutPtr;
    uint32_t   mOutPtrSize;
    int32_t    mKeyNum; //Frames since last keyframe
    int32_t    mManKeyInt; //Manual IDR interval
    uint64_t   mLastTS;
    QWORD      mStartTime; //Workaround for lack of pts from OVE
    cl_mem     mOutput;
    cl_kernel  mKernel[2];
    cl_program mProgram;
    bool       mIsReady; // init() was called successfully
    int        frameShift;
    void       *inputBuffer;
    char       *mHdrPacket;
    uint32_t   mHdrSize;
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
    int32_t    mAlignedSurfaceWidth;
    int32_t    mAlignedSurfaceHeight; //Not used much
    uint32_t   mInputBufSize;
    uint32_t   mOutputBufSize;
    uint32_t   mStatsOutSize;
    //
    CTSTR    mPreset;
    bool     mUse444; //Max VCE 2.0 can do is 422 probably
    ColorDescription mColorDesc;
    bool     mFirstFrame;

    int32_t mCurrBucketSize;
    int32_t mMaxBucketSize;
    uint32_t mDesiredPacketSize;
    int32_t mCurrFrame;
};