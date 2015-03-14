/*******************************************************************************
* DemEncoder.h
*
* AMD Media Framework (AMF) Display Encode Mode (DEM)
*
* Copyright (c) 2013 Advanced Micro Devices, Inc. (unpublished)
*
* All rights reserved. This notice is intended as a precaution against
* inadvertent publication and does not imply publication or any waiver of
* confidentiality. The year included in the foregoing notice is the year of
* creation of the work.
*
*
*******************************************************************************/
#ifndef __AMFDEMEncoder_h__
#define __AMFDEMEncoder_h__

#include "PropertyStorageEx.h"

namespace amf
{
    // Read-only properties
    static const wchar_t*        DEM_GENERIC_SUPPORT = L"Generic Support";
    static const wchar_t*        DEM_WFD_SUPPORT = L"Wireless Display Support";
    static const wchar_t*        DEM_LOWLATENCY_SUPPORT = L"Low Latency Support";

    static const wchar_t*        DEM_WIDTH = L"Width";
    static const wchar_t*        DEM_HEIGHT = L"Height";
    static const wchar_t*        DEM_FPS_NUM = L"FPS Numerator";
    static const wchar_t*        DEM_FPS_DENOM = L"FPS Denominator";

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    // Static Properties: The following properties must be set prior to the CreateEncoder() call

    // Usage: The usage parameter tailors the encoding and rate control parameters for the intended use case
    static const wchar_t*        DEM_USAGE = L"Usage";
    enum DemUsage 
    { 
        DEM_USAGE_GENERIC           =0x01,// Generic usage provides the application with the most control over encoding/rate control parameters
        DEM_USAGE_WIRELESS_DISPLAY  =0x02,// Configures encoding parameters for typical Wireless Display use-case. Minimize latency while preserving quality
        DEM_USAGE_LOWLATENCY        =0x04 // Configures parameters for low latency, high-interactivity use-case such as remote gaming. 
    };

    static const amf_uint32 DEM_USAGE_DEFAULT = DEM_USAGE_WIRELESS_DISPLAY;

    // Profile - AVC profile
    static const wchar_t* DEM_PROFILE = L"Profile";
    enum DemProfileType 
    { 
        DEM_PROFILE_CONSTRAINED_BASELINE    =66, 
        DEM_PROFILE_MAIN                    =77, 
        DEM_PROFILE_HIGH                    =100
    };
    static const amf_uint32 DEM_PROFILE_DEFAULT = DEM_PROFILE_CONSTRAINED_BASELINE;

    // Output type
    static const wchar_t*        DEM_OUTPUT_TYPE = L"Output Type";
    enum DemOutputType 
    { 
        DEM_AV_TS =0, // Audio and video transport stream
        DEM_AV_ES,    // Audio and video elementary stream
        DEM_V_TS,     // Video transport stream
        DEM_V_ES,     // Video elementary stream
        DEM_A_ES      // Audio elementary stream (Note: this value isn't supported as input and used only as AMFDemBuffer::GetDataType() return value)
    };
    static const DemOutputType DEM_OUTPUT_TYPE_DEFAULT = DEM_AV_TS;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    // Dynamic Properties: The following properties can be adjusted during encoding. 
    //
    // Note: Make sure to call FlushConfiguration() after adjusting these to have them sent to the encoder


    // IDR period
    // This property is used to configure the interval between Intra Display Refresh pictures.
    // Value 0 means no IDR
    static const wchar_t*        DEM_IDR_PERIOD         = L"IDR Period";
    static const amf_uint32      DEM_IDR_PERIOD_MIN     = 0;
    static const amf_uint32      DEM_IDR_PERIOD_MAX     = 6000;
    static const amf_uint32      DEM_IDR_PERIOD_DEFAULT = 300;

    // Skipped PIC period
    // Value 0 means no skip, 1 means skip of evert second frame, 2 means skip every 2 of three frames etc.
    static const wchar_t*        DEM_SKIPPED_PIC_PERIOD         = L"Skipped PIC Period";
    static const amf_uint32      DEM_SKIPPED_PIC_PERIOD_MIN     = 0;
    static const amf_uint32      DEM_SKIPPED_PIC_PERIOD_MAX     = 7;
    static const amf_uint32      DEM_SKIPPED_PIC_PERIOD_DEFAULT = 0;

    // Rate control method
    static const wchar_t* DEM_RATE_CONTROL_METHOD = L"Rate Control Method";
    enum DemRateControlMethod 
    { 
        DEM_PEAK_CONSTRAINED_VBR =0, // Peak Constrained Variable Bitrate 
        DEM_LATENCY_CONSTRAINED_VBR, // Latency Constrained Variable Bitrate favours latency over quality
        DEM_CBR,                     // Constant Bitrate
        DEM_NO_RC                    // No Rate Control
    };
    static const DemRateControlMethod DEM_RATE_CONTROL_METHOD_DEFAULT = DEM_PEAK_CONSTRAINED_VBR;

    // Bitrate and peak bitrate. 
    // Max value depends on resolution and frame rate:
    //   - 10 MBits/sec for 720p and frame rate less than or equal to 30 FPS
    //   - 20 MBits/sec for 720p and frame rate less than or equal to 60 FPS
    //   - 20 MBits/sec for 1080p and frame rate less than or equal to 30 FPS
    //   - 50 MBits/sec for 1080p and frame rate less than or equal to 60 FPS
    static const wchar_t*        DEM_BITRATE      = L"Bitrate"; 
    static const wchar_t*        DEM_PEAK_BITRATE = L"Peak Bitrate";

    // Intra refresh macroblocks per slot.
    // The intra refresh slot mechanism allows an application to insert intra macroblocks without having to encode an entire IDR picture.
    static const wchar_t*        DEM_INTRA_REFRESH_MB_PER_SLOT = L"Intra Refresh MBs Number Per Slot";

    // Initial VBV buffer fullness in percents
    static const wchar_t*        DEM_INITIAL_VBV_BUFFER_FULLNESS         = L"Initial VBV Buffer Fullness";
    static const amf_uint32      DEM_INITIAL_VBV_BUFFER_FULLNESS_DEFAULT = 100;
    static const amf_uint32      DEM_INITIAL_VBV_BUFFER_FULLNESS_MIN     = 0;
    static const amf_uint32      DEM_INITIAL_VBV_BUFFER_FULLNESS_MAX     = 100;

    // VBV buffer size.
    // Specifies the buffer size of the leaky bucket model used with the CBR and Peak Constrained VBR rate control modes
    static const wchar_t*        DEM_VBV_BUFFER_SIZE = L"VBV Buffer Size";

    // Min and max QP.
    static const wchar_t*        DEM_MIN_QP         = L"Min QP";
    static const wchar_t*        DEM_MAX_QP         = L"Max QP";
    static const amf_uint32      DEM_MIN_QP_DEFAULT = 22;
    static const amf_uint32      DEM_MAX_QP_DEFAULT = 51;
    static const amf_uint32      DEM_QP_MIN         = 0;
    static const amf_uint32      DEM_QP_MAX         = 51;

    // I frame quantization only if rate control is disabled
    static const wchar_t*        DEM_QP_I         = L"QP I";
    static const amf_uint32      DEM_QP_I_DEFAULT = 22;
    static const amf_uint32      DEM_QP_I_MIN     = 0;
    static const amf_uint32      DEM_QP_I_MAX     = 51;

    // P frame quantization if rate control is disabled
    static const wchar_t*        DEM_QP_P         = L"QP P";
    static const amf_uint32      DEM_QP_P_DEFAULT = 22;
    static const amf_uint32      DEM_QP_P_MIN     = 0;
    static const amf_uint32      DEM_QP_P_MAX     = 51;

    // Number of feedback slots
    static const wchar_t*        DEM_FEEDBACKS         = L"Feedbacks";
    static const amf_uint32      DEM_FEEDBACKS_DEFAULT = 16;
    static const amf_uint32      DEM_FEEDBACKS_MIN     = 3;
    static const amf_uint32      DEM_FEEDBACKS_MAX     = 32;

    // Inloop Deblocking Filter Enable
    static const wchar_t*        DEM_INLOOP_DEBLOCKING         = L"De-blocking Filter";
    static const amf_bool        DEM_INLOOP_DEBLOCKING_DEFAULT = true;

    // FillerData for RC modes
    static const wchar_t*        DEM_FILLER_DATA         = L"Filler Data";
    static const amf_bool        DEM_FILLER_DATA_DEFAULT = false;

    // Insert AUD for Transport Stream mode
    static const wchar_t*        DEM_INSERT_AUD         = L"Insert AUD";
    static const amf_bool        DEM_INSERT_AUD_DEFAULT = true;

    // Half pel motion estimation
    static const wchar_t*        DEM_MOTION_EST_HALF_PIXEL = L"Half Pixel";
    static const amf_bool        DEM_MOTION_EST_HALF_PIXEL_DEFAULT = true;

    // Quarter pel motion estimation
    static const wchar_t*        DEM_MOTION_EST_QUARTER_PIXEL = L"Quarter Pixel";
    static const amf_bool        DEM_MOTION_EST_QUARTER_PIXEL_DEFAULT = true;

    // Timing Info Presence
    static const wchar_t*        DEM_TIMING_INFO_PRESENT = L"Timing Info";
    static const amf_bool        DEM_TIMING_INFO_PRESENT_DEFAULT = true;

    // Slices per frame
    static const wchar_t*        DEM_SLICES_PER_FRAME         = L"Slices Per Frame";
    static const amf_uint32      DEM_SLICES_PER_FRAME_DEFAULT = 1;

    // Defines maximum time in miliseconds while GetNextFrame will wait for next frame.
    static const wchar_t*        DEM_WAIT_FRAME_MAX         = L"Wait Frame Max";
    static const amf_uint32      DEM_WAIT_FRAME_MAX_DEFAULT = 500;
    static const amf_uint32      DEM_WAIT_FRAME_UNLIMITED   = (amf_uint32)~0;

    //----------------------------------------------------------------------------------------------
    // VCE-DEM buffer interface
    //----------------------------------------------------------------------------------------------
    class AMFDemBuffer: public virtual AMFInterface
    {
    public:
        AMF_DECLARE_IID("AMFDemBuffer")

        virtual AMF_ERROR        AMF_STD_CALL GetMemory(void **ppMem)=0;
        virtual DemOutputType    AMF_STD_CALL GetDataType()=0;
        virtual amf_size         AMF_STD_CALL GetMemorySize()=0;
        virtual amf_int64        AMF_STD_CALL GetTimeStamp()=0;
    };
    typedef AMFRefPtr<AMFDemBuffer> AMFDemBufferPtr; 

    //----------------------------------------------------------------------------------------------
    // VCE-DEM Decoder interface
    //----------------------------------------------------------------------------------------------
    class AMFEncoderVCEDEM: public virtual AMFPropertyStorageEx
    {
    public:
        AMF_DECLARE_IID("AMFEncoderVCEDEM")

        // Initialization and termination
        virtual AMF_ERROR        AMF_STD_CALL AcquireRemoteDisplay()=0;
        virtual AMF_ERROR        AMF_STD_CALL CreateEncoder()=0;
        virtual AMF_ERROR        AMF_STD_CALL StartEncoding()=0;
        virtual AMF_ERROR        AMF_STD_CALL StopEncoding()=0;
        virtual AMF_ERROR        AMF_STD_CALL DestroyEncoder()=0;
        virtual AMF_ERROR        AMF_STD_CALL ReleaseRemoteDisplay()=0;

        // Buffer retrieving
        virtual AMF_ERROR        AMF_STD_CALL GetNextFrame(AMFDemBuffer** buff)=0;
        
        // This method must to be called from another thread
        // to terminate waiting inside GetNextThread
        virtual AMF_ERROR        AMF_STD_CALL CancelGetNextFrame()=0;

        // This method flushes configuration to encoder
        virtual AMF_ERROR        AMF_STD_CALL FlushConfiguration()=0;
    };
    typedef AMFRefPtr<AMFEncoderVCEDEM> AMFEncoderVCEDEMPtr; 

    AMF_CORE_LINK void           AMF_STD_CALL AMFCreateEncoderVCEDEM(AMFEncoderVCEDEM** encoder);
}


#endif //#ifndef __AMFEncoderVCEDEM_h__
