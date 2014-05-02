/* ============================================================

    New sdk seems to not include openvideo headers and libs
    so use GetProc...yaddayadda.
    Based on OVEncode.h from APP SDK 2.8.
    Copyright (c) 2011 Advanced Micro Devices, Inc.

============================================================ */

#ifndef __OVENCODEDYN_H__
#define __OVENCODEDYN_H__

#ifndef OPENVIDEOAPI
#define OPENVIDEOAPI __stdcall
#endif // OPENVIDEOAPI

#include "OVEncodeTypes.h"

typedef OVresult(OPENVIDEOAPI *t_OVEncodeGetDeviceInfo) (
    unsigned int            *num_device,
    ovencode_device_info    *device_info);
extern t_OVEncodeGetDeviceInfo OVEncodeGetDeviceInfo;

/*
    * This function is used by application to query the encoder capability that includes
    * codec information and format that the device can support.
    */
typedef OVresult (OPENVIDEOAPI *t_OVEncodeGetDeviceCap) (
    OPContextHandle             platform_context,
    unsigned int                device_id,
    unsigned int                encode_cap_list_size,
    unsigned int                *num_encode_cap,
    OVE_ENCODE_CAPS             *encode_cap_list);
extern t_OVEncodeGetDeviceCap OVEncodeGetDeviceCap;

/*
    * This function is used by the application to create the encode handle from the 
    * platform memory handle. The encode handle can be used in the OVEncodePicture 
    * function as the output encode buffer. The application can create multiple 
    * output buffers to queue up the decode job. 
    */
typedef ove_handle (OPENVIDEOAPI *t_OVCreateOVEHandleFromOPHandle) (
    OPMemHandle                 platform_memhandle);
extern t_OVCreateOVEHandleFromOPHandle OVCreateOVEHandleFromOPHandle;

/* 
    * This function is used by the application to release the encode handle. 
    * After release, the handle is invalid and should not be used for encode picture. 
    */
typedef OVresult (OPENVIDEOAPI *t_OVReleaseOVEHandle)(ove_handle encode_handle);
extern t_OVReleaseOVEHandle OVReleaseOVEHandle;

/* 
    * This function is used by the application to acquire the memory objects that 
    * have been created from OpenCL. These objects need to be acquired before they 
    * can be used by the decode function. 
    */

typedef OVresult (OPENVIDEOAPI *t_OVEncodeAcquireObject) (
    ove_session                 session,
    unsigned int                num_handle,
    ove_handle                 *encode_handle,
    unsigned int                num_event_in_wait_list,
    OPEventHandle              *event_wait_list,
    OPEventHandle              *event);
extern t_OVEncodeAcquireObject OVEncodeAcquireObject;

/*
    * This function is used by the application to release the memory objects that
    * have been created from OpenCL. The objects need to be released before they
    * can be used by OpenCL.
    */
typedef OVresult (OPENVIDEOAPI *t_OVEncodeReleaseObject) (
    ove_session                  session,
    unsigned int                 num_handle,
    ove_handle                  *encode_handle,
    unsigned int                 num_event_in_wait_list,
    OPEventHandle               *event_wait_list,
    OPEventHandle               *event);
extern t_OVEncodeReleaseObject OVEncodeReleaseObject;

typedef ove_event (OPENVIDEOAPI *t_OVCreateOVEEventFromOPEventHandle) (
    OPEventHandle               platform_eventhandle);
extern t_OVCreateOVEEventFromOPEventHandle OVCreateOVEEventFromOPEventHandle;

/* 
    * This function is used by the application to release the encode event handle. 
    * After release, the event handle is invalid and should not be used. 
    */
typedef OVresult (OPENVIDEOAPI *t_OVEncodeReleaseOVEEventHandle) (
    ove_event                   ove_ev);
extern t_OVEncodeReleaseOVEEventHandle OVEncodeReleaseOVEEventHandle;


/*
    * This function is used by the application to create the encode session for
    * each encoding stream. After the session creation, the encoder is ready to
    * accept the encode picture job from the application. For multiple streams
    * encoding, the application can create multiple sessions within the same
    * platform context and the application is responsible to manage the input and
    * output buffers for each corresponding session.
    */
typedef ove_session (OPENVIDEOAPI *t_OVEncodeCreateSession) (
    OPContextHandle             platform_context,
    unsigned int                device_id,
    OVE_ENCODE_MODE             encode_mode,
    OVE_PROFILE_LEVEL           encode_profile,
    OVE_PICTURE_FORMAT	        encode_format,
    unsigned int                encode_width,
    unsigned int                encode_height,
    OVE_ENCODE_TASK_PRIORITY    encode_task_priority);
extern t_OVEncodeCreateSession OVEncodeCreateSession;

/*
    * This function is used by the application to destroy the encode session. Destroying a
    * session will release all associated hardware resources.  No further decoding work
    * can be performed with the session after it is destroyed.
    */
typedef OVresult (OPENVIDEOAPI *t_OVEncodeDestroySession) (
    ove_session                 session);
extern t_OVEncodeDestroySession OVEncodeDestroySession;

// Retrieve one configuration data structure
typedef OVresult (OPENVIDEOAPI *t_OVEncodeGetPictureControlConfig) (
    ove_session                 session,
    OVE_CONFIG_PICTURE_CONTROL *pPictureControlConfig);
extern t_OVEncodeGetPictureControlConfig OVEncodeGetPictureControlConfig;

// Get current rate control configuration
typedef OVresult (OPENVIDEOAPI *t_OVEncodeGetRateControlConfig) (
    ove_session                 session,
    OVE_CONFIG_RATE_CONTROL	   *pRateControlConfig);
extern t_OVEncodeGetRateControlConfig OVEncodeGetRateControlConfig;

// Get current motion estimation configuration
typedef OVresult (OPENVIDEOAPI *t_OVEncodeGetMotionEstimationConfig) (
    ove_session                 session,
    OVE_CONFIG_MOTION_ESTIMATION *pMotionEstimationConfig);
extern t_OVEncodeGetMotionEstimationConfig OVEncodeGetMotionEstimationConfig;

// Get current RDO configuration
typedef OVresult (OPENVIDEOAPI *t_OVEncodeGetRDOControlConfig) (
    ove_session             session,
    OVE_CONFIG_RDO          *pRDOConfig);
extern t_OVEncodeGetRDOControlConfig OVEncodeGetRDOControlConfig;

typedef OVresult (OPENVIDEOAPI *t_OVEncodeSendConfig) (
    ove_session             session,
    unsigned int            num_of_config_buffers,
    OVE_CONFIG              *pConfigBuffers);
extern t_OVEncodeSendConfig OVEncodeSendConfig;

// Fully encode a single picture
typedef OVresult (OPENVIDEOAPI *t_OVEncodeTask) (
    ove_session             session,
    unsigned int            number_of_encode_task_input_buffers,
    OVE_INPUT_DESCRIPTION   *encode_task_input_buffers_list,
    void                    *picture_parameter,
    unsigned int            *pTaskID,
    unsigned int            num_event_in_wait_list,
    ove_event               *event_wait_list,
    ove_event               *event);
extern t_OVEncodeTask OVEncodeTask;

// Query outputs
typedef OVresult (OPENVIDEOAPI *t_OVEncodeQueryTaskDescription) (
    ove_session             session,
    unsigned int            num_of_task_description_request,
    unsigned int            *num_of_task_description_return,
    OVE_OUTPUT_DESCRIPTION  *task_description_list);
extern t_OVEncodeQueryTaskDescription OVEncodeQueryTaskDescription;

// Reclaim the resource of the output ring up to the specified task.
typedef OVresult (OPENVIDEOAPI *t_OVEncodeReleaseTask) (
    ove_session             session,
    unsigned int            taskID);
extern t_OVEncodeReleaseTask OVEncodeReleaseTask;


void deinitOVE();
bool initOVE();

#endif // __OVENCODEDYN_H__