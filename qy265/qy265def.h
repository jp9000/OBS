#ifndef _QY265_DEF_H_
#define  _QY265_DEF_H_

// ****************************************
// error type
// ****************************************
enum
{
    QY_OK = (0x00000000),          // Success codes
    QY_FAIL = (0x80000001),        //  Unspecified error
    QY_OUTOFMEMORY = (0x80000002), //  Ran out of memory
    QY_POINTER = (0x80000003),     //  Invalid pointer
    QY_NOTSUPPORTED = (0x80000004),//  NOT support feature encoutnered
    QY_AUTH_INVALID = (0x80000005), //  authentication invalid
    QY_SEARCHING_ACCESS_POINT = (0x00000001), // in process of searching first access point
    QY_REF_PIC_NOT_FOUND = (0x00000007), // reference picture not found, can be ignored
#if defined(EMSCRIPTEN)||defined(_TEST_FOR_EMSCRIPTEN)
    QY_NEED_MORE_DATA = (0x00000008), //need push more data
#endif
    QY_BITSTREAM_ERROR = (0x00000009), // detecting bitstream error, can be ignored
};

enum NAL_UNIT_TYPE{
    NAL_UNIT_TYPE_TRAIL_N = 0,
    NAL_UNIT_TYPE_TRAIL_R = 1,

    NAL_UNIT_TYPE_TSA_N = 2,
    NAL_UNIT_TYPE_TSA_R = 3,

    NAL_UNIT_TYPE_STSA_N = 4,
    NAL_UNIT_TYPE_STSA_R = 5,

    NAL_UNIT_TYPE_RADL_N = 6,
    NAL_UNIT_TYPE_RADL_R = 7,

    NAL_UNIT_TYPE_RASL_N = 8,
    NAL_UNIT_TYPE_RASL_R = 9,

    //reserved
    NAL_UNIT_TYPE_RSV_VCL_N10 = 10,
    NAL_UNIT_TYPE_RSV_VCL_N12 = 12,
    NAL_UNIT_TYPE_RSV_VCL_N14 = 13,
    NAL_UNIT_TYPE_RSV_VCL_R11 = 11,
    NAL_UNIT_TYPE_RSV_VCL_R13 = 13,
    NAL_UNIT_TYPE_RSV_VCL_R15 = 15,

    NAL_UNIT_TYPE_BLA_W_LP = 16,
    NAL_UNIT_TYPE_BLA_W_RADL = 17,
    NAL_UNIT_TYPE_BLA_N_LP = 18,

    NAL_UNIT_TYPE_IDR_W_RADL = 19,
    NAL_UNIT_TYPE_IDR_N_LP = 20,

    NAL_UNIT_TYPE_CRA_NUT = 21,

    NAL_UNIT_TYPE_RSV_IRAP_VCL22 = 22,
    NAL_UNIT_TYPE_RSV_IRAP_VCL23 = 23,

    NAL_UNIT_TYPE_RSV_VCL24 = 24,
    NAL_UNIT_TYPE_RSV_VCL25 = 25,
    NAL_UNIT_TYPE_RSV_VCL26 = 26,
    NAL_UNIT_TYPE_RSV_VCL27 = 27,
    NAL_UNIT_TYPE_RSV_VCL28 = 28,
    NAL_UNIT_TYPE_RSV_VCL29 = 29,
    NAL_UNIT_TYPE_RSV_VCL30 = 30,
    NAL_UNIT_TYPE_RSV_VCL31 = 31,

    NAL_UNIT_TYPE_VPS_NUT = 32,
    NAL_UNIT_TYPE_SPS_NUT = 33,
    NAL_UNIT_TYPE_PPS_NUT = 34,
    NAL_UNIT_TYPE_AUD_NUT = 35,
    NAL_UNIT_TYPE_EOS_NUT = 36,
    NAL_UNIT_TYPE_EOB_NUT = 37,
    NAL_UNIT_TYPE_FD_NUT = 38,

    NAL_UNIT_TYPE_PREFIX_SEI_NUT = 39,
    NAL_UNIT_TYPE_SUFFIX_SEI_NUT = 40,

    NAL_UNIT_TYPE_RSV_NVCL41 = 41,
    NAL_UNIT_TYPE_RSV_NVCL42 = 42,
    NAL_UNIT_TYPE_RSV_NVCL43 = 43,
    NAL_UNIT_TYPE_RSV_NVCL44 = 44,
    NAL_UNIT_TYPE_RSV_NVCL45 = 45,
    NAL_UNIT_TYPE_RSV_NVCL46 = 46,
    NAL_UNIT_TYPE_RSV_NVCL47 = 47,

    NAL_UNIT_TYPE_UNSPEC48 = 48,
    NAL_UNIT_TYPE_UNSPEC49 = 49,
    NAL_UNIT_TYPE_UNSPEC50 = 50,
    NAL_UNIT_TYPE_UNSPEC51 = 51,
    NAL_UNIT_TYPE_UNSPEC52 = 52,
    NAL_UNIT_TYPE_UNSPEC53 = 53,
    NAL_UNIT_TYPE_UNSPEC54 = 54,
    NAL_UNIT_TYPE_UNSPEC55 = 55,
    NAL_UNIT_TYPE_UNSPEC56 = 56,
    NAL_UNIT_TYPE_UNSPEC57 = 57,
    NAL_UNIT_TYPE_UNSPEC58 = 58,
    NAL_UNIT_TYPE_UNSPEC59 = 59,
    NAL_UNIT_TYPE_UNSPEC60 = 60,
    NAL_UNIT_TYPE_UNSPEC61 = 61,
    NAL_UNIT_TYPE_UNSPEC62 = 62,
    NAL_UNIT_TYPE_UNSPEC63 = 63,
};

// ****************************************
// VUI
// ****************************************
typedef struct vui_parameters{
        // --- sample aspect ratio (SAR) ---
    unsigned char     aspect_ratio_info_present_flag;
    unsigned short sar_width;  // sar_width and sar_height are zero if unspecified
    unsigned short sar_height;

    // --- overscan ---
    unsigned char     overscan_info_present_flag;
    unsigned char     overscan_appropriate_flag;

    // --- video signal type ---
    unsigned char   video_signal_type_present_flag;
    unsigned char   video_format;
    unsigned char   video_full_range_flag;
    unsigned char   colour_description_present_flag;
    unsigned char   colour_primaries;
    unsigned char   transfer_characteristics;
    unsigned char   matrix_coeffs;

    // --- chroma / interlaced ---
    unsigned char     chroma_loc_info_present_flag;
    unsigned char  chroma_sample_loc_type_top_field;
    unsigned char  chroma_sample_loc_type_bottom_field;
    unsigned char     neutral_chroma_indication_flag;
    unsigned char     field_seq_flag;
    unsigned char     frame_field_info_present_flag;

    // --- default display window ---
    unsigned char     default_display_window_flag;
    unsigned int def_disp_win_left_offset;
    unsigned int def_disp_win_right_offset;
    unsigned int def_disp_win_top_offset;
    unsigned int def_disp_win_bottom_offset;

    // --- timing ---
    unsigned char     vui_timing_info_present_flag;
    unsigned int vui_num_units_in_tick;
    unsigned int vui_time_scale;

    unsigned char     vui_poc_proportional_to_timing_flag;
    unsigned int vui_num_ticks_poc_diff_one;

    // --- hrd parameters ---
    unsigned char     vui_hrd_parameters_present_flag;
    //hrd_parameters vui_hrd_parameters;

    // --- bitstream restriction ---
    unsigned char bitstream_restriction_flag;
    unsigned char tiles_fixed_structure_flag;
    unsigned char motion_vectors_over_pic_boundaries_flag;
    unsigned char restricted_ref_pic_lists_flag;
    unsigned short min_spatial_segmentation_idc;
    unsigned char  max_bytes_per_pic_denom;
    unsigned char  max_bits_per_min_cu_denom;
    unsigned char  log2_max_mv_length_horizontal;
    unsigned char  log2_max_mv_length_vertical;
}vui_parameters;

#if defined(SWIG) || defined(__AVM2__)
#define _h_dll_export
#else

#ifdef WIN32
#define _h_dll_export   __declspec(dllexport)
#else // for GCC
#define _h_dll_export __attribute__ ((visibility("default")))
#endif

#endif  //SWIG

typedef void  (*QYLogPrintf)(const char* msg);
typedef void  (*QYAuthWarning)();

#if defined(__cplusplus)
extern "C" {
#endif//__cplusplus

// log output callback func pointer 
// if  pFuncCB == NULL, use the default printf
_h_dll_export void QY265SetLogPrintf ( QYLogPrintf pFuncCB);

// auth trouble warning callback func pointer
_h_dll_export void QY265SetAuthWarning ( QYAuthWarning pFuncCB);

#if defined(__cplusplus)
}
#endif//__cplusplus

//libqy265 version number string
_h_dll_export extern const char strLibQy265Version[];

#endif
