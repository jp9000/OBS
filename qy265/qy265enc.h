///////////////////////////////////////////////////
//
//         Kingsoft H265 Codec Library 
//
//  Copyright(c) Kingsoft cloud Inc.
//              http://www.ksyun.com/
//
///////////////////////////////////////////////////
/************************************************************************************
* encInf.h: interface of encoder for user
*
* \date     2013-09-28: first version
*    
************************************************************************************/
#ifndef   _QY265_ENCODER_INTERFACE_H_
#define   _QY265_ENCODER_INTERFACE_H_

#include "qy265def.h"
// ****************************************
// base configuration 
// ****************************************
//app type
typedef enum QY265Tune_tag{
    QY265TUNE_DEFAULT = 0,
    QY265TUNE_SELFSHOW = 1,
    QY265TUNE_GAME = 2,
    QY265TUNE_MOVIE = 3,
    QY265TUNE_SCREEN = 4
}QY265Tune;

typedef enum QY265Preset_tag{
    QY265PRESET_ULTRAFAST = 0,
    QY265PRESET_SUPERFAST = 1,
    QY265PRESET_VERYFAST = 2,
    QY265PRESET_FAST = 3,
    QY265PRESET_MEDIUM = 4,
    QY265PRESET_SLOW = 5,
    QY265PRESET_SLOWER = 6,
    QY265PRESET_VERYSLOW = 7,
    QY265PRESET_PLACEBO = 8,
}QY265Preset;

typedef enum QY265Latency_tag{
    QY265LATENCY_ZERO = 0,
    QY265LATENCY_LOWDELAY = 1,
    QY265LATENCY_LIVESTREMING = 2,
    QY265LATENCY_OFFLINE = 3,
}QY265Latency;

//base configuration
typedef struct QY265EncConfig{
    void* pAuth;        //QYAuth, invalid if don't need aksk auth
    QY265Tune tune;    //
    QY265Preset preset;
    QY265Latency latency;
    int profileId;         //currently, support 1 and 3 separately for main and main still profile
    int bHeaderBeforeKeyframe; //whether output vps,sps,pps before key frame, default 1. dis/enable 0/1
    int picWidth;          // input frame width
    int picHeight;         // input frame height
    double frameRate;      // input frame rate
    int bframes;           // num of bi-pred frames, -1: using default
    int temporalLayer;     // works with QY265LATENCY_ZERO, separate P frames into temporal layers, 0 or 1

    int rc;                // rc type 0 disable,1 cbr,2 abr,3 crf, default 2
    int bitrateInkbps;     // target bit rate in kbps, valid when rctype is cbr abd vbr
    int vbv_buffer_size;   // buf size of vbv
    int vbv_max_rate;      // max rate of vbv
    int qp;                // valid when rctype is disable, default 26
    int crf;               // valid when rctype is crf,default 24
    int iIntraPeriod;      // I-Frame period, -1 = only first
    int qpmin;              //minimal qp, valid when rc != 0, 0~51
    int qpmax;              //maximal qp, valid when rc != 0, 1~51, qpmax = 0 means 51
    int enFrameSkip;        //1: enable frame skip for ratecontrol, default 0
    //* Execute Properties 
    int enWavefront;       //enable wave front parallel
    int enFrameParallel;   //enable frame parallel
    int threads;           // number of threads used in encoding ( for wavefront, frame parallel, or enable both )
    //* vui_parameters
    //vui_parameters_present_flag equal to 1 specifies that the vui_parameters() syntax in struct vui should set by usr
    int vui_parameters_present_flag;
    struct{
        /* video_signal_type_present_flag.  If this is set then
         * video_format, video_full_range_flag and colour_description_present_flag
         * will be added to the VUI. The default is false */
        int video_signal_type_present_flag;
        /* Video format of the source video.  0 = component, 1 = PAL, 2 = NTSC,
         * 3 = SECAM, 4 = MAC, 5 = unspecified video format is the default */
        int video_format;
        /* video_full_range_flag indicates the black level and range of the luma
         * and chroma signals as derived from E'Y, E'PB, and E'PR or E'R, E'G,
         * and E'B real-valued component signals. The default is false */
        int video_full_range_flag;
        /* colour_description_present_flag in the VUI. If this is set then
         * color_primaries, transfer_characteristics and matrix_coeffs are to be
         * added to the VUI. The default is false */
        int colour_description_present_flag;
        /* colour_primaries holds the chromacity coordinates of the source
         * primaries. The default is 2 */
        int colour_primaries;
        /* transfer_characteristics indicates the opto-electronic transfer
         * characteristic of the source picture. The default is 2 */
        int transfer_characteristics;
        /* matrix_coeffs used to derive the luma and chroma signals from
         * the red, blue and green primaries. The default is 2 */
        int matrix_coeffs;
    }vui;
    //* tool list
    int logLevel;          //log level (-1: dbg; 0: info; 1:warn; 2:err; 3:fatal)
    int lookahead;         // rc lookahead settings
    int calcPsnr;          //0:not calc psnr; 1: print total psnr; 2: print each frame
    int shortLoadingForPlayer;  //reduce b frames after I frame, for shorting the loading time of VOD for some players
    //ZEL_2PASS:parameters for 2pass
    int  iPass; //Multi pass rate control,0,disable 2pass encode method; 1: first pass; 2: second pass;
    char statFileName[256]; //log file produced from first pass, seet by user
    double      fRateTolerance;//default 2.0f,0.5 is suitable to reduce the largest bitrate, and 0.1 is to make the bitrate stable
    int  rdoq;//1:enabling rdoq
    int  me;//0: DIA, 1: HEX, 2: UMH, 3:EPZS,
    int  part;//enabling 2nxn, nx2n pu
    int  do64;//1:enabling 64x64 cu
    int  tu;//max tu depth, 0~2
    int  smooth;//1: enabling strong intra smoothing
    int  transskip;//1: enabling transform skip
    int  subme;// 0 : disable 1 : fast, 2 : square full
    int  satd;//1:enabling hardmad sad
    int  searchrange;//search range
    int  ref;// reference number 
    int  sao;//sao enabling, 0: disable; 1:faster; 2: faster; 3: usual; 4:complex
    int  longTermRef;//0:disabling longterm reference 1:enable;
    int  iAqMode;// adaptive quantization 0~3, 0: disable
    double fAqStrength;//strength of adaptive quantizaiton, 0~3.0, default 1.0
    int  rasl; // enable RASL NAL for CRA,default 1, if not enable RASL, then CRA is act like IDR
}QY265EncConfig;

// ****************************************
// callback functions
// ****************************************
//the encoder works in asynchronous mode (for supports of B frames)
//once calling on EncodeFrame not corresponds to one Frame's bitstream output
//thus, use callback function on Frame Encoded
//also, buffer of srcYUV should be reserved for encoder, until it's done
// CALLBACK method to feed the encoded bit stream

// input frame data and info
typedef struct QY265YUV{
    int iWidth;                 // input frame width
    int iHeight;                // input frame height
    unsigned char* pData[3];    // input frame Y U V
    int iStride[3];             // stride for Y U V
}QY265YUV;

// input frame data and info
typedef struct QY265Picture{
    int iSliceType; // specified by output pictures
    int poc;        // ignored on input
    long long pts;
    long long dts;
    QY265YUV* yuv;
}QY265Picture;


typedef struct QY265Nal
{
    int naltype;
    int tid;
    int iSize;
    long long pts;
    unsigned char* pPayload;
}QY265Nal;


#if defined(__cplusplus)
extern "C" {
#endif//__cplusplus
/**
* create encoder
* @param pCfg : base config of encoder
* @param errorCode: error code
* @return encoder handle
*/
_h_dll_export void* QY265EncoderOpen(QY265EncConfig* pCfg, int *errorCode);
// destroy encoder 
_h_dll_export void QY265EncoderClose(void* pEncoder);
// reconfig encoder
_h_dll_export void QY265EncoderReconfig(void* pEncoder,QY265EncConfig* pCfg);
// return the VPS, SPS and PPS that will be used for the whole stream.
_h_dll_export int QY265EncoderEncodeHeaders(void* pEncoder,QY265Nal** pNals,int* iNalCount);

/**
* Encode one frame add logo or not
*
* @param pEncoder   handle of encoder
* @param pNals      pointer array of output NAL units
* @param iNalCount  output NAL unit count
* @param pInPic     input frame
* @param pOutPic    output frame
* @param bForceLogo add logo on the input frame ( when auth failed)
* @return if succeed, return 0; if failed, return the error code
*/
_h_dll_export int QY265EncoderEncodeFrame(void* pEncoder, QY265Nal** pNals, int* iNalCount, QY265Picture* pInpic, QY265Picture* pOutpic, int bForceLogo);

// Request encoder to encode a Key Frame
_h_dll_export void QY265EncoderKeyFrameRequest(void* pEncoder);
// current buffered frames 
_h_dll_export int QY265EncoderDelayedFrames(void* pEncoder);

static const char* const  qy265_preset_names[] = { "ultrafast", "superfast", "veryfast", "fast", "medium", "slow", "slower", "veryslow", "placebo", 0 };
static const char* const  qy265_tunes_names[] = { "default", "selfshow", "game", "movie", "screen", 0 };
static const char* const  qy265_latency_names[] = { "zerolatency", "lowdelay", "livestreaming", "offline", 0 };
// get default config values by preset, tune and latency. enum format
_h_dll_export int QY265ConfigDefault(QY265EncConfig* pConfig, QY265Preset preset, QY265Tune tune, QY265Latency latency);

// get default config values by preset, tune and latency. string format
_h_dll_export int QY265ConfigDefaultPreset(QY265EncConfig* pConfig, char* preset, char* tune, char* latency);

#define QY265_PARAM_BAD_NAME  (-1)
#define QY265_PARAM_BAD_VALUE (-2)
_h_dll_export int QY265ConfigParse(QY265EncConfig *p, const char *name, const char *value);
#if defined(__cplusplus)
}
#endif//__cplusplus

#endif
