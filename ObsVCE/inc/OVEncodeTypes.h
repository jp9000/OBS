/* ============================================================

    From SDK 2.8. Media SDK 1.1 probably has more.
    Copyright (c) 2011 Advanced Micro Devices, Inc.

============================================================ */

#ifndef __OVENCODETYPES_H__
#define __OVENCODETYPES_H__

#define OVE_MAX_NUM_PICTURE_FORMATS_H264		10
#define OVE_MAX_NUM_PROFILE_LEVELS_H264     20

#define OVresult            bool
#define ove_handle          void *
#define OVE_SURFACE_HANDLE  void *
#define ove_session         void *
#define OVE_BOOL            bool
#define OPContextHandle     void *
#define OPMemHandle         void *
#define OPEventHandle       void *
#define ove_event           void *
// End added


// OpenEncode  Ported 
// OpenVideo Encode Profile 
typedef enum
{
    OVE_H264_BASELINE_41 = 1,// H.264 bitstream acceleration baseline profile up to level 4.1
    OVE_H264_MAIN_41,     // H.264 bitstream acceleration main profile up to level 4.1
    OVE_H264_HIGH_41,     // H.264 bitstream acceleration high profile up to level 4.1
    /* OpenEncode  keep h264 profiles
    OVD_VC1_SIMPLE,			 // VC-1 bitstream acceleration simple profile
    OVD_VC1_MAIN,			 // VC-1 bitstream acceleration main profile
    OVD_VC1_ADVANCED,		 // VC-1 bitstream acceleration advanced profile
    OVD_MPEG2_VLD,			 // VC-1 bitstream acceleration advanced profile
    */
    OVE_H264_BASELINE_51,// H.264 bitstream acceleration baseline profile up to level 5.1
    OVE_H264_MAIN_51,// H.264 bitstream acceleration main profile up to level 5.1
    OVE_H264_HIGH_51,// H.264 bitstream acceleration high profile up to level 5.1
    OVE_H264_STEREO_HIGH,	 // H.264 bitstream acceleration stereo high profile
} ovencode_profile;


typedef struct _ovencode_device_info{
    unsigned int    device_id;
    unsigned int     encode_cap_list_size;
} ovencode_device_info;


// OpenVideo Encode Format
// to do: confirm that these are correct formats.

typedef enum
{
    OVE_NV12_INTERLEAVED_AMD = 1, // NV12 Linear Interleaved
    OVE_YV12_INTERLEAVED_AMD
} ovencode_format;

// OpenVideo Profile 

typedef struct {
    ovencode_profile profile;       // codec information about the encode capability
    ovencode_format input_format;   // encode input format supported in this device
} ovencode_cap;


typedef enum _OVE_MODE {
    OVE_MODE_NONE	  =0,
    OVE_AVC_FULL	  =1,
    OVE_AVC_ENTROPY   =2,
}OVE_ENCODE_MODE;

typedef enum _OVE_ENCODE_TASK_PRIORITY
{
    OVE_ENCODE_TASK_PRIORITY_NONE   = 0,
    OVE_ENCODE_TASK_PRIORITY_LEVEL1 = 1, 	// Always in normal queue
    OVE_ENCODE_TASK_PRIORITY_LEVEL2 = 2   // possibly in low-latency queue
}  OVE_ENCODE_TASK_PRIORITY;

typedef struct _OVE_PROFILE_LEVEL
{
    unsigned int   profile;		//based on  H.264 standard 
    unsigned int   level;
}  OVE_PROFILE_LEVEL;

typedef enum _OVE_PICTURE_FORMAT
{
    OVE_PICTURE_FORMAT_NONE   = 0,
    OVE_PICTURE_FORMAT_NV12   = 1
}  OVE_PICTURE_FORMAT;

typedef struct _OVE_ENCODE_CAPS_H264
{
    unsigned int                max_picture_size_in_MB;     // Max picture size in MBs
    unsigned int                min_picture_size_in_MB;     // Min picture size in MBs
    unsigned int                num_picture_formats;        // number of supported picture formats
    OVE_PICTURE_FORMAT          supported_picture_formats[OVE_MAX_NUM_PICTURE_FORMATS_H264]; 
    unsigned int                num_Profile_level;          // number of supported profiles/levels returne;
    OVE_PROFILE_LEVEL           supported_profile_level[OVE_MAX_NUM_PROFILE_LEVELS_H264];
    unsigned int                max_bit_rate;               // Max bit rate
    unsigned int                min_bit_rate;               // min bit rate
    OVE_ENCODE_TASK_PRIORITY    supported_task_priority;    // supported max level of job priority
}OVE_ENCODE_CAPS_H264;

typedef struct _OVE_ENCODE_CAPS_ENTROPY
{
    OVE_ENCODE_TASK_PRIORITY    supported_task_priority;// supported max level of job priority
    unsigned int                max_task_queue_depth; // Max job queue depth
}OVE_ENCODE_CAPS_ENTROPY;

typedef struct _OVE_ENCODE_CAPS
{
    OVE_ENCODE_MODE 		        EncodeModes;
    //ovencode_profile_uvd        profile;
      unsigned int			          encode_cap_size;
      union
    {
            OVE_ENCODE_CAPS_H264		*encode_cap_full;
            OVE_ENCODE_CAPS_ENTROPY	*encode_cap_entropy;
            void						        *encode_cap;
    }  caps;

} OVE_ENCODE_CAPS;


// Picture and slice control
typedef struct _OVE_CONFIG_PICTURE_CONTROL
{
    unsigned int                size;                       // structure size
    unsigned int                useConstrainedIntraPred;    // binary var - force the use of constrained intra prediction when set to 1

    //CABAC options
    unsigned int                cabacEnable;                // Enable CABAC entropy coding
    unsigned int                cabacIDC;                   // cabac_init_id = 0; cabac_init_id = 1; cabac_init_id = 2;
    unsigned int                loopFilterDisable;          //  binary var - disable loop filter when 1
                                                            // enable loop filter when 0 (0 and 1 are the only two supported cases)
    int                         encLFBetaOffset;            // -- move with loop control flag,
                                                            //	Loop filter control, slice_beta_offset (N.B. only used if deblocking filter
                                                            //   is not disabled, and there is no div2 as defined in the h264 bitstream syntax)
    int                         encLFAlphaC0Offset;         // Loop filter control, slice_alpha_c0_offset (N.B. only used if deblocking filter
                                                            // is not disabled, and there is no div2 as defined in the h264 bitstream syntax)
    unsigned int                encIDRPeriod;
    unsigned int                encIPicPeriod;              // spacing for I pictures, in case driver doesnt force/select a picture type,
                                                            //    this will be used for inference
    //unsigned int                encBPicPattern;             //    pattern for B pictures (when supported), in case driver doesnt force/select
                                                            //    a picture type, this will be used for inference. i.e. 1 = IBPBPBP..., 2 = IBBPBBPBBP...,
                                                            //    3 = IBBBPBBBPBBBP...
    int                         encHeaderInsertionSpacing;  // spacing for inserting SPS/PPS. Example usage cases are:
                                                            //        0 for inserting at the beginning of the stream only,
                                                            //        1 for every picture, "GOP size" to align it with GOP boundaries etc.
                                                            //       For compliance reasons, these headers might be inserted when
                                                            //	SPS/PPS parameters change from the config packages.
    unsigned int                encCropLeftOffset;
    unsigned int                encCropRightOffset;
    unsigned int                encCropTopOffset;
    unsigned int                encCropBottomOffset;
    //unsigned int                encFrameCroppingFlag;

    unsigned int                encNumMBsPerSlice;          // replaces encSliceArgument - Slice control - number of MBs per slice
    unsigned int                encNumSlicesPerFrame;       // Slice control - number of slices in this frame, pre-calculated to avoid DIV operation in firmware
    unsigned int                encForceIntraRefresh;	    // 1 serves to load intra refresh bitmap from address
                                                            //  force_intra_refresh_bitmap_mc_addr when equal to
                                                            //     1, 3 also loads dirty clean bitmap on top of the intra refresh
    unsigned int                encForceIMBPeriod;          // --- package with intra referesh -Intra MB spacing. if encForceIntraRefresh = 2,
                                                            //  shifts intra refreshed MBs by frame number
    unsigned int                encInsertVUIParam;          // insert VUI params in SPS
    unsigned int                encInsertSEIMsg;            // insert SEI messages (bit 0 for buffering period; bit 1 for picture timing; bit 2 for pan scan)
    //unsigned int                encSliceMode;             // Slice mode (values 1 and 2 - fixed MBs/size)
    //unsigned int                encSliceArgument;	        // Slice Control - Argument for slice mode –
                                                            //  either number of bytes per slice or number of MB's per slice
    //unsigned char               picInterlace;

} OVE_CONFIG_PICTURE_CONTROL;



//OVEncodeGetRateControlConfigure
// Rate Control
typedef struct _OVE_CONFIG_RATE_CONTROL
{
    unsigned int                size;                       // structure size
    unsigned int                encRateControlMethod;       // rate control method to be used
    unsigned int                encRateControlTargetBitRate;// target bit rate
    unsigned int                encRateControlPeakBitRate;  // peak bit rate
    unsigned int                encRateControlFrameRateNumerator;    // target frame rate
    unsigned int                encGOPSize;                 // RC GOP size
    unsigned int                encRCOptions;               // packed bitfield definition for extending options here,
                                                            //  bit 0: RC will not generate skipped frames in order to meet
                                                            //  GOP target, bits 1-30: up for grabs by the RC alg designer
    unsigned int                encQP_I;                    // I frame quantization only if rate control is disabled
    unsigned int                encQP_P;                    // P frame quantization if rate control is disabled
    unsigned int                encQP_B;                    // B frame quantization if rate control is disabled
    unsigned int                encVBVBufferSize;           // VBV buffer size - this is CPB Size, and the default is 
                                                            // per Table A-1 of the spec
    //unsigned int                encResetVBVLevel;           // Resets VBV level or not. This is a mechanism for a
                                                            //  “manual” reset, which is generally associated with
                                                            //  a buffering_period SEI msg
    //unsigned int                encVBVLevelValue;           // This is the initial buffer fullness. It defaults to
                                                            //  75% occupancy. Its value is between [0-64] (default 48)
    unsigned int                encRateControlFrameRateDenominator;    // target frame rate
} OVE_CONFIG_RATE_CONTROL;



//OVEncodeGetMotionEstimationConfig
// mode estimation control options
typedef struct _OVE_CONFIG_MOTION_ESTIMATION
{
    unsigned int                size;                 // structure size
    unsigned int                imeDecimationSearch;	// decimation search is on
    unsigned int                motionEstHalfPixel;   // enable half pel motion estimation
    unsigned int                motionEstQuarterPixel;// enable quarter pel motion estimation
    unsigned int                disableFavorPMVPoint; // disable favorization of PMV point
    unsigned int                forceZeroPointCenter; // force [0,0] point as search window center in IME
    unsigned int                lsmVert;              //  Luma Search window in MBs, set to either VCE_ENC_SEARCH_WIND_5x3 or VCE_ENC_SEARCH_WIND_9x5 or VCE_ENC_SEARCH_WIND_13x7
//    unsigned int                lsmVert5;             // encFlagsOptions + pack 3 bits -- Luma Search window in MBs 9x5
//    unsigned int                lsmVert3;             // Luma Search window in MBs 5x3
//    unsigned int                lsmVert1;             // Luma Search window in MBs 13x7
    unsigned int                encSearchRangeX;      // forward prediction - Manual limiting of horizontal motion vector range
                                                      // (for performance) in pel resolution
    unsigned int                encSearchRangeY;      // forward prediction - Manual limiting of vertical motion vector range
                                                      // (for performance)
    unsigned int                encSearch1RangeX;     // for 2nd ref - curr IME_SEARCH_SIZE doesn't have
                                                      //   SIZE__SEARCH1_X bitfield
    unsigned int                encSearch1RangeY;     // for 2nd ref
    unsigned int                disable16x16Frame1;   // second reference (B frame) limitation
    unsigned int                disableSATD;          // Disable SATD cost calculation (SAD only)
    unsigned int                enableAMD;            // FME advanced mode decision
    unsigned int                encDisableSubMode;    // --- FME
    unsigned int                encIMESkipX;          // sub sample search window horz --- UENC_IME_OPTIONS.SKIP_POINT_X
    unsigned int                encIMESkipY;          // sub sample search window vert --- UENC_IME_OPTIONS.SKIP_POINT_Y
    unsigned int                encEnImeOverwDisSubm; // Enable overwriting of fme_disable_submode in IME with enabled mode number 
                                                                  //  equal to ime_overw_dis_subm_no (only 8x8 and above could be enabled)
    unsigned int                encImeOverwDisSubmNo; // Numbers of mode IME will pick if en_ime_overw_dis_subm equal to 1.
    unsigned int                encIME2SearchRangeX;  // IME Additional Search Window Size: horizontal 1-4 
                                                      //    (+- this value left and right from center)
    unsigned int                encIME2SearchRangeY;  // IME Additional Search Window Size: vertical not-limited 
                                                      //   (+- this value up and down from center)
} OVE_CONFIG_MOTION_ESTIMATION;                       // structure aligned to 88 bytes



//OVEncodeGetRDOConfig
//RDO options

typedef struct _OVE_CONFIG_RDO
{
    unsigned int                size;                       // structure size
    unsigned int                encDisableTbePredIFrame;    // Disable Prediction Modes For I-Frames
    unsigned int                encDisableTbePredPFrame;    // same as above for P frames
    unsigned int                useFmeInterpolY;            // zero_residues_luma
    unsigned int                useFmeInterpolUV;           // zero_residues_chroma
    unsigned int                enc16x16CostAdj;            // --- UENC_FME_MD.M16x16_COST_ADJ
    unsigned int                encSkipCostAdj;             // --- UENC_FME_MD.MSkip_COST_ADJ
    unsigned char               encForce16x16skip;
} OVE_CONFIG_RDO;


//OVEncodeSendConfig

typedef enum _OVE_CONFIG_TYPE
{
    OVE_CONFIG_TYPE_NONE                    = 0,
    OVE_CONFIG_TYPE_PICTURE_CONTROL 		= 1,
    OVE_CONFIG_TYPE_RATE_CONTROL		    = 2,
    OVE_CONFIG_TYPE_MOTION_ESTIMATION       = 3,
    OVE_CONFIG_TYPE_RDO                     = 4
} OVE_CONFIG_TYPE;

typedef struct _OVE_CONFIG
{
    OVE_CONFIG_TYPE                             	configType;
    union
    {
        OVE_CONFIG_PICTURE_CONTROL*             	pPictureControl;
        OVE_CONFIG_RATE_CONTROL*                	pRateControl;
        OVE_CONFIG_MOTION_ESTIMATION*           	pMotionEstimation;
        OVE_CONFIG_RDO*                         	pRDO;
    }    config;
} OVE_CONFIG;


//OVEncodeTask

typedef enum _OVE_PICTURE_STRUCTURE_H264
{
    OVE_PICTURE_STRUCTURE_H264_NONE 		    = 0,
    OVE_PICTURE_STRUCTURE_H264_FRAME 		    = 1, 
    OVE_PICTURE_STRUCTURE_H264_TOP_FIELD	 	= 2,
    OVE_PICTURE_STRUCTURE_H264_BOTTOM_FIELD     = 3
} OVE_PICTURE_STRUCTURE_H264;


// Used to force picture type
typedef enum _OVE_PICTURE_TYPE_H264
{
    OVE_PICTURE_TYPE_H264_NONE                  = 0,
    OVE_PICTURE_TYPE_H264_SKIP                  = 1,
    OVE_PICTURE_TYPE_H264_IDR                   = 2,
    OVE_PICTURE_TYPE_H264_I                     = 3,
    OVE_PICTURE_TYPE_H264_P                     = 4
} OVE_PICTURE_TYPE_H264;


typedef union _OVE_PARAMETERS_FLAGS
{
    struct
    {
        // enable/disable features
        unsigned int                            reserved    : 32;               // reserved fields must be set to zero
    }                                           flags;
    unsigned int                                value;
}OVE_PARAMETERS_FLAGS;


typedef struct _OVE_ENCODE_PARAMETERS_H264
{
    unsigned int                        size;                           // structure size
    OVE_PARAMETERS_FLAGS flags;                                         // enable/disable any supported features

    OVE_BOOL                            insertSPS;    
    OVE_PICTURE_STRUCTURE_H264          pictureStructure;
    OVE_BOOL                            forceRefreshMap;
    unsigned int                        forceIMBPeriod;
    OVE_PICTURE_TYPE_H264               forcePicType;
} OVE_ENCODE_PARAMETERS_H264;

typedef enum _OVE_BUFFER_TYPE
{
    OVE_BUFFER_TYPE_NONE                   = 0,
    OVE_BUFFER_TYPE_PICTURE                = 2,
    OVE_BUFFER_TYPE_INTRA_REFRESH_BITMAP   = 3,
    OVE_BUFFER_TYPE_DIRTY_CLEAN_BITMAP     = 4,
    OVE_BUFFER_TYPE_SLICE_HEADER           = 5,
    OVE_BUFFER_TYPE_SLICE                  = 6
} OVE_BUFFER_TYPE;

typedef struct _OVE_BUFFER
{
    OVE_BUFFER_TYPE                     bufferType;
    union
    {
        OVE_SURFACE_HANDLE              pPicture;
        OVE_SURFACE_HANDLE              pIntraRefreshBitmap;
        OVE_SURFACE_HANDLE              pDirtyCleanBitmap;
        OVE_SURFACE_HANDLE              pSliceHeader;
        OVE_SURFACE_HANDLE              pSlice;
    }   buffer;
} OVE_INPUT_DESCRIPTION;


//OVEncodeQueryTaskDescription

typedef enum _OVE_TASK_STATUS
{
    OVE_TASK_STATUS_NONE                = 0,
    OVE_TASK_STATUS_COMPLETE           	= 1,	 // encoding task has finished successfully.
    OVE_TASK_STATUS_FAILED              = 2         	 // encoding task has finished but failed. 
} OVE_TASK_STATUS;

typedef struct _OVE_OUTPUT_DESCRIPTION
{
    unsigned int      size;					      // structure size
    unsigned int      taskID;					    // task ID
    OVE_TASK_STATUS   status;				      // Task status. May be duplicated if current task has multiple output blocks.
    unsigned int      size_of_bitstream_data;		// data size of the output block 
    void             *bitstream_data;		  // read pointer the top portion of the generated bitstream data for the current task
} OVE_OUTPUT_DESCRIPTION;

#endif // #ifndef __OVENCODETYPES_H__