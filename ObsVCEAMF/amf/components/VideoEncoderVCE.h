/*
 ***************************************************************************************************
 *
 * Copyright (c) 2014 Advanced Micro Devices, Inc. (unpublished)
 *
 *  All rights reserved.  This notice is intended as a precaution against inadvertent publication and
 *  does not imply publication or any waiver of confidentiality.  The year included in the foregoing
 *  notice is the year of creation of the work.
 *
 ***************************************************************************************************
 */
/**
 ***************************************************************************************************
 * @file  VideoEncoderVCE.h
 * @brief AMFVideoEncoderHW_AVC interface declaration
 ***************************************************************************************************
 */
#ifndef __AMFVideoEncoderHW_AVC_h__
#define __AMFVideoEncoderHW_AVC_h__
#pragma once

#include "Component.h"

#define AMFVideoEncoderVCE_AVC L"AMFVideoEncoderVCE_AVC"
#define AMFVideoEncoderVCE_SVC L"AMFVideoEncoderVCE_SVC"

enum AMF_VIDEO_ENCODER_USAGE_ENUM
{
    AMF_VIDEO_ENCODER_USAGE_TRANSCONDING = 0,
    AMF_VIDEO_ENCODER_USAGE_ULTRA_LOW_LATENCY,
    AMF_VIDEO_ENCODER_USAGE_LOW_LATENCY,
    AMF_VIDEO_ENCODER_USAGE_WEBCAM
};

enum AMF_VIDEO_ENCODER_PROFILE_ENUM
{
    AMF_VIDEO_ENCODER_PROFILE_BASELINE = 66,
    AMF_VIDEO_ENCODER_PROFILE_MAIN = 77,
    AMF_VIDEO_ENCODER_PROFILE_HIGH = 100
};

enum AMF_VIDEO_ENCODER_SCANTYPE_ENUM
{
    AMF_VIDEO_ENCODER_SCANTYPE_PROGRESSIVE = 0,
    AMF_VIDEO_ENCODER_SCANTYPE_INTERLACED
};

enum AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_ENUM
{
    AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_CONSTRAINED_QP = 0,
    AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_CBR,
    AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_PEAK_CONSTRAINED_VBR,
    AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_LATENCY_CONSTRAINED_VBR
};

enum AMF_VIDEO_ENCODER_QUALITY_PRESET_ENUM
{
    AMF_VIDEO_ENCODER_QUALITY_PRESET_BALANCED = 0,
    AMF_VIDEO_ENCODER_QUALITY_PRESET_SPEED,
    AMF_VIDEO_ENCODER_QUALITY_PRESET_QUALITY
};

enum AMF_VIDEO_ENCODER_PICTURE_STRUCTURE_ENUM
{
    AMF_VIDEO_ENCODER_PICTURE_STRUCTURE_NONE = 0,
    AMF_VIDEO_ENCODER_PICTURE_STRUCTURE_FRAME,
    AMF_VIDEO_ENCODER_PICTURE_STRUCTURE_TOP_FIELD,
    AMF_VIDEO_ENCODER_PICTURE_STRUCTURE_BOTTOM_FIELD
};

enum AMF_VIDEO_ENCODER_PICTURE_TYPE_ENUM
{
    AMF_VIDEO_ENCODER_PICTURE_TYPE_NONE = 0,
    AMF_VIDEO_ENCODER_PICTURE_TYPE_SKIP,
    AMF_VIDEO_ENCODER_PICTURE_TYPE_IDR,
    AMF_VIDEO_ENCODER_PICTURE_TYPE_I,
    AMF_VIDEO_ENCODER_PICTURE_TYPE_P,
    AMF_VIDEO_ENCODER_PICTURE_TYPE_B
};

enum AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_ENUM
{
    AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_IDR,
    AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_I,
    AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_P,
    AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_B
};

// Static properties - can be set befor Init()

#define AMF_VIDEO_ENCODER_FRAMESIZE                             L"FrameSize"                // AMFSize; default = 0,0; Frame size
#define AMF_VIDEO_ENCODER_FRAMERATE                             L"FrameRate"                // AMFRate; default = depends on usage; Frame Rate 

#define AMF_VIDEO_ENCODER_EXTRADATA                             L"ExtraData"                // AMFInterface* - > AMFBuffer*; SPS/PPS buffer - read-only
#define AMF_VIDEO_ENCODER_USAGE                                 L"Usage"                    // amf_int64(AMF_VIDEO_ENCODER_USAGE_ENUM); default = N/A; Encoder usage type. fully configures parameter set. 
#define AMF_VIDEO_ENCODER_PROFILE                               L"Profile"                  // amf_int64(AMF_VIDEO_ENCODER_PROFILE_ENUM) ; default = AMF_VIDEO_ENCODER_PROFILE_MAIN;  H264 profile
#define AMF_VIDEO_ENCODER_PROFILE_LEVEL                         L"ProfileLevel"             // amf_int64; default = 42; H264 profile level
#define AMF_VIDEO_ENCODER_MAX_LTR_FRAMES                        L"MaxOfLTRFrames"           // amf_int64; default = 0; Max number of LTR frames
#define AMF_VIDEO_ENCODER_SCANTYPE                              L"ScanType"                 // amf_int64(AMF_VIDEO_ENCODER_SCANTYPE_ENUM); default = AMF_VIDEO_ENCODER_SCANTYPE_PROGRESSIVE; indicates input stream type
    // Quality preset property
#define AMF_VIDEO_ENCODER_QUALITY_PRESET                        L"QualityPreset"            // amf_int64(AMF_VIDEO_ENCODER_QUALITY_PRESET_ENUM); default = depends on USAGE; Quality Preset 


// dynamic properties - can be set at any time

    // Rate control properties
#define AMF_VIDEO_ENCODER_B_PIC_DELTA_QP                        L"BPicturesDeltaQP"         // amf_int64; default = depends on USAGE; B-picture Delta
#define AMF_VIDEO_ENCODER_REF_B_PIC_DELTA_QP                    L"ReferenceBPicturesDeltaQP" //  amf_int64; default = depends on USAGE; Reference B-picture Delta

#define AMF_VIDEO_ENCODER_ENFORCE_HRD                           L"EnforceHRD"               // bool; default = depends on USAGE; Enforce HRD
#define AMF_VIDEO_ENCODER_FILLER_DATA_ENABLE                    L"FillerDataEnable"         // bool; default = false; Filler Data Enable


#define AMF_VIDEO_ENCODER_GOP_SIZE                              L"GOPSize"                  // amf_int64; default = 60; GOP Size, in frames
#define AMF_VIDEO_ENCODER_VBV_BUFFER_SIZE                       L"VBVBufferSize"            // amf_int64; default = depends on USAGE; VBV Buffer Size in bits
#define AMF_VIDEO_ENCODER_INITIAL_VBV_BUFFER_FULLNESS           L"InitialVBVBufferFullness" //amf_int64; default =  64; Initial VBV Buffer Fullness 0=0% 64=100%

#define AMF_VIDEO_ENCODER_MAX_AU_SIZE                           L"MaxAUSize"                // amf_int64; default = 60; Max AU Size in bits

#define AMF_VIDEO_ENCODER_MIN_QP                                L"MinQP"                    // amf_int64; default = depends on USAGE; Min QP; range = 0-51
#define AMF_VIDEO_ENCODER_MAX_QP                                L"MaxQP"                    // amf_int64; default = depends on USAGE; Max QP; range = 0-51
#define AMF_VIDEO_ENCODER_QP_I                                  L"QPI"                      // amf_int64; default = 22; I-frame QP; range = 0-51
#define AMF_VIDEO_ENCODER_QP_P                                  L"QPP"                      // amf_int64; default = 22; P-frame QP; range = 0-51
#define AMF_VIDEO_ENCODER_QP_B                                  L"QPB"                      // amf_int64; default = 22; B-frame QP; range = 0-51
#define AMF_VIDEO_ENCODER_TARGET_BITRATE                        L"TargetBitrate"            // amf_int64; default = depends on USAGE; Target bit rate in bits
#define AMF_VIDEO_ENCODER_PEAK_BITRATE                          L"PeakBitrate"              // amf_int64; default = depends on USAGE; Peak bit rate in bits
#define AMF_VIDEO_ENCODER_RATE_CONTROL_SKIP_FRAME_ENABLE        L"RateControlSkipFrameEnable"   // bool; default =  depends on USAGE; Rate Control Based Frame Skip 
#define AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD                   L"RateControlMethod"        // amf_int64(AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_ENUM); default = depends on USAGE; Rate Control Method 

    // Picture control properties - 
#define AMF_VIDEO_ENCODER_HEADER_INSERTION_SPACING              L"HeaderInsertionSpacing"   // amf_int64; default = 0; Header Insertion Spacing; range 0-1000
#define AMF_VIDEO_ENCODER_B_PIC_PATTERN                         L"BPicturesPattern"         // amf_int64; default = 3; B-picture Pattern (number of B-Frames)
#define AMF_VIDEO_ENCODER_DE_BLOCKING_FILTER                    L"DeBlockingFilter"         // bool; default = depends on USAGE; De-blocking Filter
#define AMF_VIDEO_ENCODER_B_REFERENCE_ENABLE                    L"BReferenceEnable"         // bool; default = true; Enable Refrence to B-frames
#define AMF_VIDEO_ENCODER_IDR_PERIOD                            L"IDRPeriod"                // amf_int64; default = depends on USAGE; IDR Period in frames
#define AMF_VIDEO_ENCODER_INTRA_REFRESH_NUM_MBS_PER_SLOT        L"IntraRefreshMBsNumberPerSlot" // amf_int64; default = depends on USAGE; Intra Refresh MBs Number Per Slot in Macroblocks
#define AMF_VIDEO_ENCODER_SLICES_PER_FRAME                      L"SlicesPerFrame"           // amf_int64; default = 1; Number of slices Per Frame 

    // Motion estimation
#define AMF_VIDEO_ENCODER_MOTION_HALF_PIXEL                     L"HalfPixel"                // bool; default= true; Half Pixel 
#define AMF_VIDEO_ENCODER_MOTION_QUARTERPIXEL                   L"QuarterPixel"             // bool; default= true; Quarter Pixel

    // SVC
#define AMF_VIDEO_ENCODER_NUM_TEMPORAL_ENHANCMENT_LAYERS        L"NumOfTemporalEnhancmentLayers" // amf_int64; default = 0; range = 0, min(2, caps->GetMaxNumOfTemporalLayers()) number of temporal enhancment Layers (SVC)

// Per-submission properties - can be set on input surface interface

#define AMF_VIDEO_ENCODER_END_OF_SEQUENCE                       L"EndOfSequence"            // bool; default = false; generate end of sequence
#define AMF_VIDEO_ENCODER_END_OF_STREAM                         L"EndOfStream"              // bool; default = false; generate end of stream
#define AMF_VIDEO_ENCODER_FORCE_PICTURE_TYPE                    L"ForcePictureType"         // amf_int64(AMF_VIDEO_ENCODER_PICTURE_TYPE_ENUM); default = AMF_VIDEO_ENCODER_PICTURE_TYPE_NONE; generate particular picture type
#define AMF_VIDEO_ENCODER_INSERT_AUD                            L"InsertAUD"                // bool; default = false; insert AUD
#define AMF_VIDEO_ENCODER_INSERT_SPS                            L"InsertSPS"                // bool; default = false; insert SPS
#define AMF_VIDEO_ENCODER_INSERT_PPS                            L"InsertPPS"                // bool; default = false; insert PPS
#define AMF_VIDEO_ENCODER_PICTURE_STRUCTURE                     L"PictureStructure"         // amf_int64(AMF_VIDEO_ENCODER_PICTURE_STRUCTURE_ENUM); default = AMF_VIDEO_ENCODER_PICTURE_STRUCTURE_FRAME; indicate picture type
#define AMF_VIDEO_ENCODER_MARK_CURRENT_WITH_LTR_INDEX           L"MarkCurrentWithLTRIndex"  // //amf_int64; default = N/A; Mark current frame with LTR index
#define AMF_VIDEO_ENCODER_FORCE_LTR_REFERENCE_BITFIELD          L"ForceLTRReferenceBitfield"// amf_int64; default = 0; force LTR bit-field 

// properties set by encoder on output buffer interface
#define AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE                      L"OutputDataType"           // amf_int64(AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_ENUM); default = N/A
#define AMF_VIDEO_ENCODER_OUTPUT_MARKED_LTR_INDEX               L"MarkedLTRIndex"           //amf_int64; default = -1; Marked LTR index
#define AMF_VIDEO_ENCODER_OUTPUT_REFERENCED_LTR_INDEX_BITFIELD  L"ReferencedLTRIndexBitfield" // amf_int64; default = 0; referenced LTR bit-field 


#endif //#ifndef __AMFVideoEncoderHW_AVC_h__
