//=============================================================================
//! @file        IVideoCaptureFilter.h
//! @bc -----------------------------------------------------------------------
//! @ec @brief   Interface declaration for Elgato Video Capture Filter
//! @author      F.M.Birth, T.Schnitzler
//! @date        01-Oct-12 FMB - Creation
//! @date        08-Apr-13 TS  - Added <i>IVideoCaptureFilter2</i>
//! @date        14-Nov-13 TS  - Added <i>IVideoCaptureFilter3</i>
//! @date        21-Jul-14 TS  - Added <i>IVideoCaptureFilter4</i>
//! @date        28-Aug-14 FDj - MIT license added
//! @date        04-Sep-14 FMB - Added <i>interfaceVersion</i> to VIDEO_CAPTURE_FILTER_SETTINGS_EX
//! @date        29-Jan-15 FMB - Added MPEG-TS Pin to filter, added interface IElgatoVideoCaptureFilter6
//!
//! @note        The DirectShow filter works with
//!              - Elgato Game Capture HD
//!              - Elgato Game Capture HD60
//! @bc -----------------------------------------------------------------------
//! @ec @par     Copyright
//! @n           (c) 2012-15, Elgato Systems. All Rights Reserved.
//! @n
//! @n The MIT License (MIT)
//! @n
//! @n Permission is hereby granted, free of charge, to any person obtaining a copy
//! @n of this software and associated documentation files (the "Software"), to deal
//! @n in the Software without restriction, including without limitation the rights
//! @n to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//! @n copies of the Software, and to permit persons to whom the Software is
//! @n furnished to do so, subject to the following conditions:
//! @n
//! @n The above copyright notice and this permission notice shall be included in all
//! @n copies or substantial portions of the Software.
//! @n
//! @n THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//! @n IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//! @n FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//! @n AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//! @n LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//! @n OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//! @n SOFTWARE.
//! @n
//=============================================================================

#pragma once

/*=============================================================================
// FILTER INTERFACE
=============================================================================*/
#define VIDEO_CAPTURE_FILTER_NAME            "Elgato Game Capture HD"
#define VIDEO_CAPTURE_FILTER_NAME_L         L"Elgato Game Capture HD"

#define ELGATO_VCF_VIDEO_PID	100	//!< video PID in MPEG-TS stream
#define ELGATO_VCF_AUDIO_PID	101	//!< audio PID in MPEG-TS stream

//! Interface version:
//! - 1st digit: interface version (e.g. 5 for IElgatoVideoCaptureFilter5)
//! - 2nd digit: revision (changed e.g. when reserved fields in structures changed their meaning)
#define VIDEO_CAPTURE_FILTER_INTERFACE_VERSION	60

// {39F50F4C-99E1-464a-B6F9-D605B4FB5918}
DEFINE_GUID(CLSID_ElgatoVideoCaptureFilter,
0x39f50f4c, 0x99e1, 0x464a, 0xb6, 0xf9, 0xd6, 0x5, 0xb4, 0xfb, 0x59, 0x18);

// {39F50F4C-99E1-464a-B6F9-D605B4FB5919}
DEFINE_GUID(CLSID_ElgatoVideoCaptureFilterProperties,
0x39f50f4c, 0x99e1, 0x464a, 0xb6, 0xf9, 0xd6, 0x5, 0xb4, 0xfb, 0x59, 0x19);

// {39F50F4C-99E1-464a-B6F9-D605B4FB5920}
DEFINE_GUID(IID_IElgatoVideoCaptureFilter,
0x39f50f4c, 0x99e1, 0x464a, 0xb6, 0xf9, 0xd6, 0x5, 0xb4, 0xfb, 0x59, 0x20);

// {585B2914-252E-49bd-B730-7B4C40F4D4E5}
DEFINE_GUID(IID_IElgatoVideoCaptureFilter2,
0x585b2914, 0x252e, 0x49bd, 0xb7, 0x30, 0x7b, 0x4c, 0x40, 0xf4, 0xd4, 0xe5);

// {CC415EB7-B1C7-428c-9E5E-D9747DB4BE76}
DEFINE_GUID(IID_IElgatoVideoCaptureFilter3,
0xcc415eb7, 0xb1c7, 0x428c, 0x9e, 0x5e, 0xd9, 0x74, 0x7d, 0xb4, 0xbe, 0x76);

// {197992FF-ED65-47CB-8032-D287AB40B33F}
DEFINE_GUID(IID_IElgatoVideoCaptureFilter4,
0x197992ff, 0xed65, 0x47cb, 0x80, 0x32, 0xd2, 0x87, 0xab, 0x40, 0xb3, 0x3f);

// {7E6E9E9E-4062-4364-99B1-15C2F662B502}
DEFINE_GUID(IID_IElgatoVideoCaptureFilter5,
0x7e6e9e9e, 0x4062, 0x4364, 0x99, 0xb1, 0x15, 0xc2, 0xf6, 0x62, 0xb5, 0x2);

// {39F50F4C-99E1-464a-B6F9-D605B4FB5925}
DEFINE_GUID(IID_IElgatoVideoCaptureFilter6,
0x39f50f4c, 0x99e1, 0x464a, 0xb6, 0xf9, 0xd6, 0x05, 0xb4, 0xfb, 0x59, 0x25);


/*=============================================================================
// IElgatoVideoCaptureFilter
=============================================================================*/

//! Interface
DECLARE_INTERFACE_(IElgatoVideoCaptureFilter, IUnknown)
{
};

/*=============================================================================
// IElgatoVideoCaptureFilter2
=============================================================================*/

//! Video Capture device type
typedef enum VIDEO_CAPTURE_FILTER_DEVICE_TYPE
{
    VIDEO_CAPTURE_FILTER_DEVICE_TYPE_INVALID                = 0,            //!< Invalid
    VIDEO_CAPTURE_FILTER_DEVICE_TYPE_GAME_CAPTURE_HD        = 2,            //!< Game Capture HD   (VID: 0x0fd9 PID: 0x0044, 0x004e, 0x0051)
    VIDEO_CAPTURE_FILTER_DEVICE_TYPE_GAME_CAPTURE_HD60      = 8,            //!< Game Capture HD60 (VID: 0x0fd9 PID: 0x005c)
    NUM_VIDEO_CAPTURE_FILTER_DEVICE_TYPE
};

//! Input device
typedef enum VIDEO_CAPTURE_FILTER_INPUT_DEVICE
{
    VIDEO_CAPTURE_FILTER_INPUT_DEVICE_INVALID               =   0,          //!< Invalid
    VIDEO_CAPTURE_FILTER_INPUT_DEVICE_XBOX360               =   1,          //!< Microsoft Xbox 360
    VIDEO_CAPTURE_FILTER_INPUT_DEVICE_PLAYSTATION3          =   2,          //!< Sony PlayStation 3
    VIDEO_CAPTURE_FILTER_INPUT_DEVICE_IPAD                  =   3,          //!< Apple iPad
    VIDEO_CAPTURE_FILTER_INPUT_DEVICE_IPOD_IPHONE           =   4,          //!< Apple iPod or iPhone
    VIDEO_CAPTURE_FILTER_INPUT_DEVICE_WII                   =   5,          //!< Nintendo Wii
    VIDEO_CAPTURE_FILTER_INPUT_DEVICE_OTHER                 =   6,          //!< Other
    VIDEO_CAPTURE_FILTER_INPUT_DEVICE_WII_U                 =   7,          //!< Nintendo Wii U
    VIDEO_CAPTURE_FILTER_INPUT_DEVICE_XBOX_ONE              =   8,          //!< Microsoft Xbox One
    VIDEO_CAPTURE_FILTER_INPUT_DEVICE_PLAYSTATION4          =   9,          //!< Sony PlayStation 4
};

//! Video inputs
typedef enum VIDEO_CAPTURE_FILTER_VIDEO_INPUT
{
    VIDEO_CAPTURE_FILTER_VIDEO_INPUT_INVALID                =   0,          //!< Invalid
    VIDEO_CAPTURE_FILTER_VIDEO_INPUT_COMPOSITE              =   1,          //!< Composite
    VIDEO_CAPTURE_FILTER_VIDEO_INPUT_SVIDEO                 =   2,          //!< S-Video
    VIDEO_CAPTURE_FILTER_VIDEO_INPUT_COMPONENT              =   3,          //!< Component
    VIDEO_CAPTURE_FILTER_VIDEO_INPUT_HDMI                   =   4,          //!< HDMI
};

//! Video encoder profile
typedef enum VIDEO_CAPTURE_FILTER_VID_ENC_PROFILE
{
    VIDEO_CAPTURE_FILTER_VID_ENC_PROFILE_INVALID            = 0x00000000,   //!< Invalid
    VIDEO_CAPTURE_FILTER_VID_ENC_PROFILE_240                = 0x00000001,   //!< 320x240
    VIDEO_CAPTURE_FILTER_VID_ENC_PROFILE_360                = 0x00000002,   //!< 480x360
    VIDEO_CAPTURE_FILTER_VID_ENC_PROFILE_480                = 0x00000004,   //!< 640x480
    VIDEO_CAPTURE_FILTER_VID_ENC_PROFILE_720                = 0x00000008,   //!< 1280x720
    VIDEO_CAPTURE_FILTER_VID_ENC_PROFILE_1080               = 0x00000010,   //!< 1920x1080
};

//! Color range
typedef enum VIDEO_CAPTURE_FILTER_COLOR_RANGE
{
    VIDEO_CAPTURE_FILTER_COLOR_RANGE_INVALID                =   0,          //!< Invalid
    VIDEO_CAPTURE_FILTER_COLOR_RANGE_FULL                   =   1,          //!< 0-255
    VIDEO_CAPTURE_FILTER_COLOR_RANGE_LIMITED                =   2,          //!< 16-235
    VIDEO_CAPTURE_FILTER_COLOR_RANGE_SHOOT                  =   3,          //!<
};

//! Settings
typedef struct _VIDEO_CAPTURE_FILTER_SETTINGS
{
    TCHAR                                deviceName[256];                   //!< Device name (get only)
    VIDEO_CAPTURE_FILTER_INPUT_DEVICE    inputDevice;                       //!< Input device (e.g. Xbox360)
    VIDEO_CAPTURE_FILTER_VIDEO_INPUT     videoInput;                        //!< Video input (e.g. HDMI)
    VIDEO_CAPTURE_FILTER_VID_ENC_PROFILE profile;                           //!< Video encoder profile (maximum resolution)
    BOOL                                 useAnalogAudioInput;               //!< for HDMI with analog audio input
    VIDEO_CAPTURE_FILTER_COLOR_RANGE     hdmiColorRange;                    //!< HDMI color range
    int                                  brightness;                        //!< Brightness (0-10000)
    int                                  contrast;                          //!< Contrast   (0-10000)
    int                                  saturation;                        //!< Saturation (0-10000)
    int                                  hue;                               //!< Hue        (0-10000)
    int                                  analogAudioGain;                   //!< Analog audio gain  (-60 - 12 dB)
    int                                  digitalAudioGain;                  //!< Digital audio gain (-60 - 12 dB)
    BOOL                                 preserveInputFormat;               //!< Input Format will be preserved (e.g. do not convert interlaced to progressive)
    BOOL                                 stretchStandardDefinitionInput;    //!< Stretch SD input to 16:9
}VIDEO_CAPTURE_FILTER_SETTINGS, *PVIDEO_CAPTURE_FILTER_SETTINGS;
typedef const VIDEO_CAPTURE_FILTER_SETTINGS* PCVIDEO_CAPTURE_FILTER_SETTINGS;

//! Interface
DECLARE_INTERFACE_(IElgatoVideoCaptureFilter2, IElgatoVideoCaptureFilter)
{
    // Get current settings
    STDMETHOD(GetSettings)(THIS_ PVIDEO_CAPTURE_FILTER_SETTINGS pSettings) PURE;

    // Set settings
    STDMETHOD(SetSettings)(THIS_ PCVIDEO_CAPTURE_FILTER_SETTINGS pcSettings) PURE;
};

/*=============================================================================
// IElgatoVideoCaptureFilter3
=============================================================================*/

//! Interface
DECLARE_INTERFACE_(IElgatoVideoCaptureFilter3, IElgatoVideoCaptureFilter2)
{
    //! Get A/V delay in milli-seconds (approximate delay between input signal and DirectShow
    //! filter output)
    STDMETHOD(GetDelayMs)(THIS_ int* pnDelayMs) PURE;
};

/*=============================================================================
// IElgatoVideoCaptureFilter4
=============================================================================*/

//! Messages
typedef enum VIDEO_CAPTURE_FILTER_NOTIFICATION
{
	//! Description: Delay of the device has changed. Call GetDelayMs() to get the new delay.
    VIDEO_CAPTURE_FILTER_NOTIFICATION_DEVICE_DELAY_CHANGED              = 110,      //!< Data: none

	//! Description: Output format has changed. Update your signal path accordingly.
    VIDEO_CAPTURE_FILTER_NOTIFICATION_CAPTURE_OUTPUT_FORMAT_CHANGED     = 305,      //!< Data: none
};

//! Custom event that can be received by IMediaEvent::GetEvent. If SetNotificationCallback() was not set this method is used to send notifications.
//! lEventCode = VIDEO_CAPTURE_FILTER_EVENT
//! lParam1    = VIDEO_CAPTURE_FILTER_NOTIFICATION
//! lParam2    = reserved for future use (e.g. notifications with additional data)
#define VIDEO_CAPTURE_FILTER_EVENT		EC_USER + 0x0FD9

//! Message callback
typedef void (CALLBACK* PFN_VIDEO_CAPTURE_FILTER_NOTIFICATION_CALLBACK)(VIDEO_CAPTURE_FILTER_NOTIFICATION nMessage, void* pData, int nSize, void* pContext);

//! Interface
DECLARE_INTERFACE_(IElgatoVideoCaptureFilter4, IElgatoVideoCaptureFilter3)
{
    //! Check device is present
    STDMETHOD(GetDevicePresent)(THIS_ BOOL* pfDevicePresent) PURE;

    //! Get current device type
    STDMETHOD(GetDeviceType)(THIS_ VIDEO_CAPTURE_FILTER_DEVICE_TYPE* pnDeviceType) PURE;

    //! Set callback to receive notifications
    STDMETHOD(SetNotificationCallback)(THIS_ PFN_VIDEO_CAPTURE_FILTER_NOTIFICATION_CALLBACK pCallback, void* pContext) PURE;
};

/*=============================================================================
// IElgatoVideoCaptureFilter5
=============================================================================*/

//! Extended Settings
typedef struct _VIDEO_CAPTURE_FILTER_SETTINGS_EX
{
	VIDEO_CAPTURE_FILTER_SETTINGS		Settings;
	BOOL								enableFullFrameRate;				//!< Enable full frame rate (50/60 fps)

	BYTE								reserved[20 * 1024 - sizeof(DWORD)];

	DWORD								interfaceVersion;					//!< Clients need to set this value to VIDEO_CAPTURE_FILTER_INTERFACE_VERSION
}VIDEO_CAPTURE_FILTER_SETTINGS_EX, *PVIDEO_CAPTURE_FILTER_SETTINGS_EX;
typedef const VIDEO_CAPTURE_FILTER_SETTINGS_EX* PCVIDEO_CAPTURE_FILTER_SETTINGS_EX;

//! Interface IElgatoVideoCaptureFilter5
DECLARE_INTERFACE_(IElgatoVideoCaptureFilter5, IElgatoVideoCaptureFilter4)
{
	//! Get current settings
	STDMETHOD(GetSettingsEx)(THIS_ PVIDEO_CAPTURE_FILTER_SETTINGS_EX pSettings) PURE;

	//! Set settings
	STDMETHOD(SetSettingsEx)(THIS_ PCVIDEO_CAPTURE_FILTER_SETTINGS_EX pcSettings) PURE;
};

//! Interface IElgatoVideoCaptureFilter6
DECLARE_INTERFACE_(IElgatoVideoCaptureFilter6, IElgatoVideoCaptureFilter5)
{
	//! Get A/V delay in milli-seconds
	//! @param pnDelayMs latency of stream in milliseconds
	//! @param forCompressedData true -> get delay for MPEG-TS pin, false -> get delay for audio/video pins
	STDMETHOD(GetDelayMs)(THIS_ __out int* pnDelayMs, __in bool forCompressedData) PURE;
};
