/*******************************************************************************
 * Copyright (c) 2008-2013 The Khronos Group Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 ******************************************************************************/

/* $Revision: 14835 $ on $Date: 2011-05-26 11:32:00 -0700 (Thu, 26 May 2011) $ */

/* cl_ext.h contains OpenCL extensions which don't have external */
/* (OpenGL, D3D) dependencies.                                   */

#ifndef __CL_EXT_H
#define __CL_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __APPLE__
        #include <OpenCL/cl.h>
    #include <AvailabilityMacros.h>
#else
        #include <CL/cl.h>
#endif

/* cl_khr_fp16 extension - no extension #define since it has no functions  */
#define CL_DEVICE_HALF_FP_CONFIG                    0x1033

/* Memory object destruction
 *
 * Apple extension for use to manage externally allocated buffers used with cl_mem objects with CL_MEM_USE_HOST_PTR
 *
 * Registers a user callback function that will be called when the memory object is deleted and its resources 
 * freed. Each call to clSetMemObjectCallbackFn registers the specified user callback function on a callback 
 * stack associated with memobj. The registered user callback functions are called in the reverse order in 
 * which they were registered. The user callback functions are called and then the memory object is deleted 
 * and its resources freed. This provides a mechanism for the application (and libraries) using memobj to be 
 * notified when the memory referenced by host_ptr, specified when the memory object is created and used as 
 * the storage bits for the memory object, can be reused or freed.
 *
 * The application may not call CL api's with the cl_mem object passed to the pfn_notify.
 *
 * Please check for the "cl_APPLE_SetMemObjectDestructor" extension using clGetDeviceInfo(CL_DEVICE_EXTENSIONS)
 * before using.
 */
#define cl_APPLE_SetMemObjectDestructor 1
cl_int  CL_API_ENTRY clSetMemObjectDestructorAPPLE(  cl_mem /* memobj */, 
                                        void (* /*pfn_notify*/)( cl_mem /* memobj */, void* /*user_data*/), 
                                        void * /*user_data */ )             CL_EXT_SUFFIX__VERSION_1_0;  


/* Context Logging Functions
 *
 * The next three convenience functions are intended to be used as the pfn_notify parameter to clCreateContext().
 * Please check for the "cl_APPLE_ContextLoggingFunctions" extension using clGetDeviceInfo(CL_DEVICE_EXTENSIONS)
 * before using.
 *
 * clLogMessagesToSystemLog fowards on all log messages to the Apple System Logger 
 */
#define cl_APPLE_ContextLoggingFunctions 1
extern void CL_API_ENTRY clLogMessagesToSystemLogAPPLE(  const char * /* errstr */, 
                                            const void * /* private_info */, 
                                            size_t       /* cb */, 
                                            void *       /* user_data */ )  CL_EXT_SUFFIX__VERSION_1_0;

/* clLogMessagesToStdout sends all log messages to the file descriptor stdout */
extern void CL_API_ENTRY clLogMessagesToStdoutAPPLE(   const char * /* errstr */, 
                                          const void * /* private_info */, 
                                          size_t       /* cb */, 
                                          void *       /* user_data */ )    CL_EXT_SUFFIX__VERSION_1_0;

/* clLogMessagesToStderr sends all log messages to the file descriptor stderr */
extern void CL_API_ENTRY clLogMessagesToStderrAPPLE(   const char * /* errstr */, 
                                          const void * /* private_info */, 
                                          size_t       /* cb */, 
                                          void *       /* user_data */ )    CL_EXT_SUFFIX__VERSION_1_0;


/************************ 
* cl_khr_icd extension *                                                  
************************/
#define cl_khr_icd 1

/* cl_platform_info                                                        */
#define CL_PLATFORM_ICD_SUFFIX_KHR                  0x0920

/* Additional Error Codes                                                  */
#define CL_PLATFORM_NOT_FOUND_KHR                   -1001

extern CL_API_ENTRY cl_int CL_API_CALL
clIcdGetPlatformIDsKHR(cl_uint          /* num_entries */,
                       cl_platform_id * /* platforms */,
                       cl_uint *        /* num_platforms */);

typedef CL_API_ENTRY cl_int (CL_API_CALL *clIcdGetPlatformIDsKHR_fn)(
    cl_uint          /* num_entries */,
    cl_platform_id * /* platforms */,
    cl_uint *        /* num_platforms */);


/* Extension: cl_khr_image2D_buffer
 *
 * This extension allows a 2D image to be created from a cl_mem buffer without a copy.
 * The type associated with a 2D image created from a buffer in an OpenCL program is image2d_t.
 * Both the sampler and sampler-less read_image built-in functions are supported for 2D images
 * and 2D images created from a buffer.  Similarly, the write_image built-ins are also supported
 * for 2D images created from a buffer.
 *
 * When the 2D image from buffer is created, the client must specify the width,
 * height, image format (i.e. channel order and channel data type) and optionally the row pitch
 *
 * The pitch specified must be a multiple of CL_DEVICE_IMAGE_PITCH_ALIGNMENT pixels.
 * The base address of the buffer must be aligned to CL_DEVICE_IMAGE_BASE_ADDRESS_ALIGNMENT pixels.
 */
    
/*************************************
 * cl_khr_initalize_memory extension *
 *************************************/
    
#define CL_CONTEXT_MEMORY_INITIALIZE_KHR            0x200E
    
    
/**************************************
 * cl_khr_terminate_context extension *
 **************************************/
    
#define CL_DEVICE_TERMINATE_CAPABILITY_KHR          0x200F
#define CL_CONTEXT_TERMINATE_KHR                    0x2010

#define cl_khr_terminate_context 1
extern CL_API_ENTRY cl_int CL_API_CALL clTerminateContextKHR(cl_context /* context */) CL_EXT_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int (CL_API_CALL *clTerminateContextKHR_fn)(cl_context /* context */) CL_EXT_SUFFIX__VERSION_1_2;
    

/*
 * Extension: cl_khr_spir
 *
 * This extension adds support to create an OpenCL program object from a 
 * Standard Portable Intermediate Representation (SPIR) instance
 */

// KHR SPIR extension (Section 9.15.2 in the extension SPEC)
// TODO: The values have been approved by khronos and waiting to be updated in official header file.
#define CL_DEVICE_SPIR_VERSIONS                     0x40E0
#define CL_PROGRAM_BINARY_TYPE_INTERMEDIATE         0x40E1

/******************************************
* cl_nv_device_attribute_query extension *
******************************************/
/* cl_nv_device_attribute_query extension - no extension #define since it has no functions */
#define CL_DEVICE_COMPUTE_CAPABILITY_MAJOR_NV       0x4000
#define CL_DEVICE_COMPUTE_CAPABILITY_MINOR_NV       0x4001
#define CL_DEVICE_REGISTERS_PER_BLOCK_NV            0x4002
#define CL_DEVICE_WARP_SIZE_NV                      0x4003
#define CL_DEVICE_GPU_OVERLAP_NV                    0x4004
#define CL_DEVICE_KERNEL_EXEC_TIMEOUT_NV            0x4005
#define CL_DEVICE_INTEGRATED_MEMORY_NV              0x4006

/*********************************
* cl_amd_device_memory_flags *
*********************************/
#define cl_amd_device_memory_flags 1
#define CL_MEM_USE_PERSISTENT_MEM_AMD       (1 << 6)        // Alloc from GPU's CPU visible heap

/* cl_device_info */
#define CL_DEVICE_MAX_ATOMIC_COUNTERS_EXT           0x4032

/*********************************
* cl_amd_device_attribute_query *
*********************************/
#define CL_DEVICE_PROFILING_TIMER_OFFSET_AMD        0x4036
#define CL_DEVICE_TOPOLOGY_AMD                      0x4037
#define CL_DEVICE_BOARD_NAME_AMD                    0x4038
#define CL_DEVICE_GLOBAL_FREE_MEMORY_AMD            0x4039
#define CL_DEVICE_SIMD_PER_COMPUTE_UNIT_AMD         0x4040
#define CL_DEVICE_SIMD_WIDTH_AMD                    0x4041
#define CL_DEVICE_SIMD_INSTRUCTION_WIDTH_AMD        0x4042
#define CL_DEVICE_WAVEFRONT_WIDTH_AMD               0x4043
#define CL_DEVICE_GLOBAL_MEM_CHANNELS_AMD           0x4044
#define CL_DEVICE_GLOBAL_MEM_CHANNEL_BANKS_AMD      0x4045
#define CL_DEVICE_GLOBAL_MEM_CHANNEL_BANK_WIDTH_AMD 0x4046
#define CL_DEVICE_LOCAL_MEM_SIZE_PER_COMPUTE_UNIT_AMD   0x4047
#define CL_DEVICE_LOCAL_MEM_BANKS_AMD               0x4048
#define CL_DEVICE_THREAD_TRACE_SUPPORTED_AMD        0x4049
#define CL_DEVICE_GFXIP_MAJOR_AMD                   0x404A
#define CL_DEVICE_GFXIP_MINOR_AMD                   0x404B

typedef union
{
    struct { cl_uint type; cl_uint data[5]; } raw;
    struct { cl_uint type; cl_char unused[17]; cl_char bus; cl_char device; cl_char function; } pcie;
} cl_device_topology_amd;

#define CL_DEVICE_TOPOLOGY_TYPE_PCIE_AMD            1

/*************
* cl_amd_hsa *
**************/
#define CL_HSA_ENABLED_AMD                          (1ull << 62)
#define CL_HSA_DISABLED_AMD                         (1ull << 63)


/**************************
* cl_amd_offline_devices *
**************************/
#define CL_CONTEXT_OFFLINE_DEVICES_AMD              0x403F

#ifdef CL_VERSION_1_1
   /***********************************
    * cl_ext_device_fission extension *
    ***********************************/
    #define cl_ext_device_fission   1

    extern CL_API_ENTRY cl_int CL_API_CALL
    clReleaseDeviceEXT( cl_device_id /*device*/ ) CL_EXT_SUFFIX__VERSION_1_1; 

    typedef CL_API_ENTRY cl_int 
    (CL_API_CALL *clReleaseDeviceEXT_fn)( cl_device_id /*device*/ ) CL_EXT_SUFFIX__VERSION_1_1;

    extern CL_API_ENTRY cl_int CL_API_CALL
    clRetainDeviceEXT( cl_device_id /*device*/ ) CL_EXT_SUFFIX__VERSION_1_1; 

    typedef CL_API_ENTRY cl_int 
    (CL_API_CALL *clRetainDeviceEXT_fn)( cl_device_id /*device*/ ) CL_EXT_SUFFIX__VERSION_1_1;

    typedef cl_ulong  cl_device_partition_property_ext;
    extern CL_API_ENTRY cl_int CL_API_CALL
    clCreateSubDevicesEXT(  cl_device_id /*in_device*/,
                            const cl_device_partition_property_ext * /* properties */,
                            cl_uint /*num_entries*/,
                            cl_device_id * /*out_devices*/,
                            cl_uint * /*num_devices*/ ) CL_EXT_SUFFIX__VERSION_1_1;

    typedef CL_API_ENTRY cl_int 
    ( CL_API_CALL * clCreateSubDevicesEXT_fn)(  cl_device_id /*in_device*/,
                                                const cl_device_partition_property_ext * /* properties */,
                                                cl_uint /*num_entries*/,
                                                cl_device_id * /*out_devices*/,
                                                cl_uint * /*num_devices*/ ) CL_EXT_SUFFIX__VERSION_1_1;

    /* cl_device_partition_property_ext */
    #define CL_DEVICE_PARTITION_EQUALLY_EXT             0x4050
    #define CL_DEVICE_PARTITION_BY_COUNTS_EXT           0x4051
    #define CL_DEVICE_PARTITION_BY_NAMES_EXT            0x4052
    #define CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN_EXT  0x4053

    /* clDeviceGetInfo selectors */
    #define CL_DEVICE_PARENT_DEVICE_EXT                 0x4054
    #define CL_DEVICE_PARTITION_TYPES_EXT               0x4055
    #define CL_DEVICE_AFFINITY_DOMAINS_EXT              0x4056
    #define CL_DEVICE_REFERENCE_COUNT_EXT               0x4057
    #define CL_DEVICE_PARTITION_STYLE_EXT               0x4058

    /* clGetImageInfo enum */
    #define CL_IMAGE_BYTE_PITCH_AMD                     0x4059

    /* error codes */
    #define CL_DEVICE_PARTITION_FAILED_EXT              -1057
    #define CL_INVALID_PARTITION_COUNT_EXT              -1058
    #define CL_INVALID_PARTITION_NAME_EXT               -1059

    /* CL_AFFINITY_DOMAINs */
    #define CL_AFFINITY_DOMAIN_L1_CACHE_EXT             0x1
    #define CL_AFFINITY_DOMAIN_L2_CACHE_EXT             0x2
    #define CL_AFFINITY_DOMAIN_L3_CACHE_EXT             0x3
    #define CL_AFFINITY_DOMAIN_L4_CACHE_EXT             0x4
    #define CL_AFFINITY_DOMAIN_NUMA_EXT                 0x10
    #define CL_AFFINITY_DOMAIN_NEXT_FISSIONABLE_EXT     0x100
    /* cl_device_partition_property_ext list terminators */
    #define CL_PROPERTIES_LIST_END_EXT                  ((cl_device_partition_property_ext) 0)
    #define CL_PARTITION_BY_COUNTS_LIST_END_EXT         ((cl_device_partition_property_ext) 0)
    #define CL_PARTITION_BY_NAMES_LIST_END_EXT          ((cl_device_partition_property_ext) 0 - 1)

/*********************************
* cl_qcom_ext_host_ptr extension
*********************************/

#define CL_MEM_EXT_HOST_PTR_QCOM                  (1 << 29)

#define CL_DEVICE_EXT_MEM_PADDING_IN_BYTES_QCOM   0x40A0      
#define CL_DEVICE_PAGE_SIZE_QCOM                  0x40A1
#define CL_IMAGE_ROW_ALIGNMENT_QCOM               0x40A2
#define CL_IMAGE_SLICE_ALIGNMENT_QCOM             0x40A3
#define CL_MEM_HOST_UNCACHED_QCOM                 0x40A4
#define CL_MEM_HOST_WRITEBACK_QCOM                0x40A5
#define CL_MEM_HOST_WRITETHROUGH_QCOM             0x40A6
#define CL_MEM_HOST_WRITE_COMBINING_QCOM          0x40A7

typedef cl_uint                                   cl_image_pitch_info_qcom;

extern CL_API_ENTRY cl_int CL_API_CALL
clGetDeviceImageInfoQCOM(cl_device_id             device,
                         size_t                   image_width,
                         size_t                   image_height,
                         const cl_image_format   *image_format,
                         cl_image_pitch_info_qcom param_name,
                         size_t                   param_value_size,
                         void                    *param_value,
                         size_t                  *param_value_size_ret);

typedef struct _cl_mem_ext_host_ptr
{
    // Type of external memory allocation.
    // Legal values will be defined in layered extensions.
    cl_uint  allocation_type;
            
    // Host cache policy for this external memory allocation.
    cl_uint  host_cache_policy;

} cl_mem_ext_host_ptr;

/*********************************
* cl_qcom_ion_host_ptr extension
*********************************/

#define CL_MEM_ION_HOST_PTR_QCOM                  0x40A8

typedef struct _cl_mem_ion_host_ptr
{
    // Type of external memory allocation.
    // Must be CL_MEM_ION_HOST_PTR_QCOM for ION allocations.
    cl_mem_ext_host_ptr  ext_host_ptr;

    // ION file descriptor
    int                  ion_filedesc;
            
    // Host pointer to the ION allocated memory
    void*                ion_hostptr;

} cl_mem_ion_host_ptr;

#endif /* CL_VERSION_1_1 */

#ifdef CL_VERSION_1_2
    /********************************
    * cl_amd_bus_addressable_memory *
    ********************************/

    /* cl_mem flag - bitfield */
    #define CL_MEM_BUS_ADDRESSABLE_AMD               (1<<30)
    #define CL_MEM_EXTERNAL_PHYSICAL_AMD             (1<<31)

    #define CL_COMMAND_WAIT_SIGNAL_AMD                0x4080
    #define CL_COMMAND_WRITE_SIGNAL_AMD               0x4081
    #define CL_COMMAND_MAKE_BUFFERS_RESIDENT_AMD      0x4082

    typedef struct _cl_bus_address_amd
    {
        cl_ulong surface_bus_address;
        cl_ulong marker_bus_address;
    } cl_bus_address_amd;

    typedef CL_API_ENTRY cl_int
    (CL_API_CALL * clEnqueueWaitSignalAMD_fn)( cl_command_queue /*command_queue*/,
                                               cl_mem /*mem_object*/,
                                               cl_uint /*value*/,
                                               cl_uint /*num_events*/,
                                               const cl_event * /*event_wait_list*/,
                                               cl_event * /*event*/) CL_EXT_SUFFIX__VERSION_1_2;

    typedef CL_API_ENTRY cl_int
    (CL_API_CALL * clEnqueueWriteSignalAMD_fn)( cl_command_queue /*command_queue*/,
                                                cl_mem /*mem_object*/,
                                                cl_uint /*value*/,
                                                cl_ulong /*offset*/,
                                                cl_uint /*num_events*/,
                                                const cl_event * /*event_list*/,
                                                cl_event * /*event*/) CL_EXT_SUFFIX__VERSION_1_2;

    typedef CL_API_ENTRY cl_int
    (CL_API_CALL * clEnqueueMakeBuffersResidentAMD_fn)( cl_command_queue /*command_queue*/,
                                                 cl_uint /*num_mem_objs*/,
                                                 cl_mem * /*mem_objects*/,
                                                 cl_bool /*blocking_make_resident*/,
                                                 cl_bus_address_amd * /*bus_addresses*/,
                                                 cl_uint /*num_events*/,
                                                 const cl_event * /*event_list*/,
                                                 cl_event * /*event*/) CL_EXT_SUFFIX__VERSION_1_2;

    /*******************************************
     * Shared Virtual Memory (SVM) extension
     * The declarations match the order, naming and values of the original 2.0
     * standard, except for the fact that we added the _AMD suffix to each
     * symbol
     *******************************************/
    typedef cl_bitfield                      cl_device_svm_capabilities_amd;
    typedef cl_bitfield                      cl_svm_mem_flags_amd;
    typedef cl_uint                          cl_kernel_exec_info_amd;

    /* cl_device_info */
    #define CL_DEVICE_SVM_CAPABILITIES_AMD                     0x1053
    #define CL_DEVICE_PREFERRED_PLATFORM_ATOMIC_ALIGNMENT_AMD  0x1054

    /* cl_device_svm_capabilities_amd */
    #define CL_DEVICE_SVM_COARSE_GRAIN_BUFFER_AMD             (1 << 0)
    #define CL_DEVICE_SVM_FINE_GRAIN_BUFFER_AMD               (1 << 1)
    #define CL_DEVICE_SVM_FINE_GRAIN_SYSTEM_AMD               (1 << 2)
    #define CL_DEVICE_SVM_ATOMICS_AMD                         (1 << 3)

    /* cl_svm_mem_flags_amd */
    #define CL_MEM_SVM_FINE_GRAIN_BUFFER_AMD                  (1 << 10)
    #define CL_MEM_SVM_ATOMICS_AMD                            (1 << 11)

    /* cl_mem_info */
    #define CL_MEM_USES_SVM_POINTER_AMD                       0x1109

    /* cl_kernel_exec_info_amd */
    #define CL_KERNEL_EXEC_INFO_SVM_PTRS_AMD                  0x11B6
    #define CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM_AMD     0x11B7

    /* cl_command_type */
    #define CL_COMMAND_SVM_FREE_AMD                           0x1209
    #define CL_COMMAND_SVM_MEMCPY_AMD                         0x120A
    #define CL_COMMAND_SVM_MEMFILL_AMD                        0x120B
    #define CL_COMMAND_SVM_MAP_AMD                            0x120C
    #define CL_COMMAND_SVM_UNMAP_AMD                          0x120D

    typedef CL_API_ENTRY void*
    (CL_API_CALL * clSVMAllocAMD_fn)(
        cl_context            /* context */,
        cl_svm_mem_flags_amd  /* flags */,
        size_t                /* size */,
        unsigned int          /* alignment */
    ) CL_EXT_SUFFIX__VERSION_1_2;

    typedef CL_API_ENTRY void
    (CL_API_CALL * clSVMFreeAMD_fn)(
        cl_context  /* context */,
        void*       /* svm_pointer */
    ) CL_EXT_SUFFIX__VERSION_1_2;

    typedef CL_API_ENTRY cl_int
    (CL_API_CALL * clEnqueueSVMFreeAMD_fn)(
        cl_command_queue /* command_queue */,
        cl_uint          /* num_svm_pointers */,
        void**           /* svm_pointers */,
        void (CL_CALLBACK *)( /*pfn_free_func*/
            cl_command_queue /* queue */,
            cl_uint          /* num_svm_pointers */,
            void**           /* svm_pointers */,
            void*            /* user_data */),
        void*             /* user_data */,
        cl_uint           /* num_events_in_wait_list */,
        const cl_event*   /* event_wait_list */,
        cl_event*         /* event */
    ) CL_EXT_SUFFIX__VERSION_1_2;

    typedef CL_API_ENTRY cl_int
    (CL_API_CALL * clEnqueueSVMMemcpyAMD_fn)(
        cl_command_queue /* command_queue */,
        cl_bool          /* blocking_copy */,
        void*            /* dst_ptr */,
        const void*      /* src_ptr */,
        size_t           /* size */,
        cl_uint          /* num_events_in_wait_list */,
        const cl_event*  /* event_wait_list */,
        cl_event*        /* event */
    ) CL_EXT_SUFFIX__VERSION_1_2;

    typedef CL_API_ENTRY cl_int
    (CL_API_CALL * clEnqueueSVMMemFillAMD_fn)(
        cl_command_queue /* command_queue */,
        void*            /* svm_ptr */,
        const void*      /* pattern */,
        size_t           /* pattern_size */,
        size_t           /* size */,
        cl_uint          /* num_events_in_wait_list */,
        const cl_event*  /* event_wait_list */,
        cl_event*        /* event */
    ) CL_EXT_SUFFIX__VERSION_1_2;

    typedef CL_API_ENTRY cl_int
    (CL_API_CALL * clEnqueueSVMMapAMD_fn)(
        cl_command_queue /* command_queue */,
        cl_bool          /* blocking_map */,
        cl_map_flags     /* map_flags */,
        void*            /* svm_ptr */,
        size_t           /* size */,
        cl_uint          /* num_events_in_wait_list */,
        const cl_event*  /* event_wait_list */,
        cl_event*        /* event */
    ) CL_EXT_SUFFIX__VERSION_1_2;

    typedef CL_API_ENTRY cl_int
    (CL_API_CALL * clEnqueueSVMUnmapAMD_fn)(
        cl_command_queue /* command_queue */,
        void*            /* svm_ptr */,
        cl_uint          /* num_events_in_wait_list */,
        const cl_event*  /* event_wait_list */,
        cl_event*        /* event */
    ) CL_EXT_SUFFIX__VERSION_1_2;

    typedef CL_API_ENTRY cl_int
    (CL_API_CALL * clSetKernelArgSVMPointerAMD_fn)(
        cl_kernel     /* kernel */,
        cl_uint       /* arg_index */,
        const void *  /* arg_value */
    ) CL_EXT_SUFFIX__VERSION_1_2;

    typedef CL_API_ENTRY cl_int
    (CL_API_CALL * clSetKernelExecInfoAMD_fn)(
         cl_kernel                /* kernel */,
         cl_kernel_exec_info_amd  /* param_name */,
         size_t                   /* param_value_size */,
         const void *             /* param_value */
    ) CL_EXT_SUFFIX__VERSION_1_2;

#endif /* CL_VERSION_1_2 */

#ifdef __cplusplus
}
#endif


#endif /* __CL_EXT_H */
