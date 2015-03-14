/*******************************************************************************
 Copyright ©2014 Advanced Micro Devices, Inc. All rights reserved.

 Redistribution and use in source and binary forms, with or without 
 modification, are permitted provided that the following conditions are met:

 1   Redistributions of source code must retain the above copyright notice, 
 this list of conditions and the following disclaimer.
 2   Redistributions in binary form must reproduce the above copyright notice, 
 this list of conditions and the following disclaimer in the 
 documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
 THE POSSIBILITY OF SUCH DAMAGE.
 *******************************************************************************/

/**
 * @file <MftUtil.cpp>
 *
 * @brief MFT helper functions implementation.
 */

#ifndef _AMF_MFT_HPP_
#define _AMF_MFT_HPP_

#include <initguid.h>
#include <mfapi.h>
#include "Typedef.h"

/**
 *   @brief CLSID for AMD HW MFT decoder. Currently it supports only Mpeg4 part 2 video with frame size 1280x720 or higher.
 */
DEFINE_GUID(CLSID_AMD_D3D11HW_DecoderMFT, 0X17796AEB, 0X0F66, 0X4663, 0XB8, 0XFB, 0X99, 0XCB, 0XEE, 0X02, 0X24, 0XCE);

/**
 *   @brief CLSID for AMD HW MFT encoder. It supports only H264 (Base, Main) and SVC temporal encoding (UCConstrainedHigh).
 */
DEFINE_GUID(CLSID_AMD_H264_HW_EncoderMFT, 0XADC9BC80, 0X0F41, 0X46C6, 0XAB, 0X75, 0XD6, 0X93, 0XD7, 0X93, 0X59, 0X7D);

/**
 *   @brief CLSID for MS MFT encoder.
 */
DEFINE_GUID(CLSID_MS_H264_EncoderMFT, 0x6CA50344, 0x051A, 0x4DED, 0x97, 0x79, 0xA4, 0x33, 0x05, 0x16, 0x5E, 0x35);

DEFINE_GUID(CLSID_MS_CC_MFT, 0x98230571, 0x0087, 0x4204, 0xB0, 0x20, 0x32, 0x82, 0x53, 0x8E, 0x57, 0xD3);

/**
 *   @brief XVID media subtype GUID.
 */
DEFINE_MEDIATYPE_GUID(MFVideoFormat_XVID, FCC('XVID'));

                typedef struct _AMF_MFT_VIDEOENCODERPARAMS
                {
                    DWORD compressionStandard; // Compression standard (VEP_COMPRESSIONSTANDARD_XXX)
                    DWORD gopSize; // Max number of frames in a GOP (0=auto)
                    DWORD numBFrames; // Max number of consecutive B-frames
                    DWORD maxBitrate; // Peak (maximum) bitrate of encoded video (only used for VBR)
                    DWORD meanBitrate; // Bitrate of encoded video (bits per second)
                    DWORD bufSize; // VBR buffer size
                    DWORD rateControlMethod; // Rate control Method
                    DWORD lowLatencyMode; // low latency mode (uses only 1 reference frame )
                    DWORD qualityVsSpeed; // 0 - low quality faster encoding 100 - Higher quality, slower encoding
                    DWORD commonQuality; // This parameter is used only when is encRateControlMethod  set to eAVEncCommonRateControlMode_Quality. in this mode encoder selects the bit rate to match the quality settings
                    DWORD enableCabac; // 0 disable CABAC 1 Enable
                    DWORD idrPeriod; // IDR interval 
                } AMF_MFT_VIDEOENCODERPARAMS;

                typedef struct AMF_MFT_VQPARAMS
                {
                    uint32 streadyVideoEnable;
                    uint32 streadyVideoZoom;
                    uint32 streadyVideoStrength;
                    uint32 streadyVideoDelay;

                    uint32 deinterlacing;
                    uint32 deinterlacingPullDownDetection;

                    uint32 enableEdgeEnhacement;
                    uint32 edgeEnhacementStrength;

                    uint32 enableDenoise;
                    uint32 denoiseStrength;

                    uint32 enableMosquitoNoise;
                    uint32 mosquitoNoiseStrength;

                    uint32 enableDeblocking;
                    uint32 deblockingStrength;

                    uint32 enableDynamicContrast;

                    uint32 enableColorVibrance;
                    uint32 colorVibranceStrength;

                    uint32 enableSkintoneCorrectoin;
                    uint32 skintoneCorrectoinStrength;

                    uint32 enableBrighterWhites;

                    uint32 enableGamaCorrection;
                    uint32 gamaCorrectionStrength;

                    uint32 enableDynamicRange;

                    uint32 effectBrightness;
                    uint32 effectContrast;
                    uint32 effectSaturation;
                    uint32 effectTint;

                    uint32 effectDynamic;
                    uint32 effectChanged;

                    uint32 enableFalseContourReduction;
                    uint32 falseContorReductionStrength;

                    uint32 enableSuperResDt;
                    uint32 superResDtStrength;

                    uint32 enableSuperResMctnr;
                    uint32 superResMctnrStrength;
                    
                    uint32 demoMode;
                } AMF_MFT_VQPARAMS;

				typedef struct _AMF_MFT_COMMONPARAMS
                {
					DWORD useInterop; //UseInterop
					DWORD useSWCodec; //UseSWCodec
                } AMF_MFT_COMMONPARAMS;


#endif //_AMF_MFT_HPP_
