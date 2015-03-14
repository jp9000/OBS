///-------------------------------------------------------------------------
///  Trade secret of Advanced Micro Devices, Inc.
///  Copyright 2013, Advanced Micro Devices, Inc., (unpublished)
///
///  All rights reserved.  This notice is intended as a precaution against
///  inadvertent publication and does not imply publication or any waiver
///  of confidentiality.  The year included in the foregoing notice is the
///  year of creation of the work.
///-------------------------------------------------------------------------
///  @file   Typedefs.h
///  @brief  frameworks typedefs
///-------------------------------------------------------------------------
#ifndef __AMFTypes_h__
#define __AMFTypes_h__
#pragma once

#include "Core.h"


//-------------------------------------------------------------------------------------------------
// basic data types
//-------------------------------------------------------------------------------------------------

#if defined(_WIN32)
#include <windows.h>

    typedef     __int64             amf_int64;
    typedef     __int32             amf_int32;
    typedef     __int16             amf_int16;
    typedef     __int8              amf_int8;

    typedef     unsigned __int64    amf_uint64;
    typedef     unsigned __int32    amf_uint32;
    typedef     unsigned __int16    amf_uint16;
    typedef     unsigned __int8     amf_uint8;
    typedef     size_t              amf_size;

    #define AMF_STD_CALL            __stdcall
    #define AMF_CDECL_CALL          __cdecl
    #define AMF_FAST_CALL           __fastcall
    #define AMF_INLINE              inline
    #define AMF_FORCEINLINE         __forceinline

#else // !WIN32 - Linux and Mac

    #include <stdint.h>
    #include <inttypes.h>
    #include <time.h>
    #include <stdio.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/socket.h>

    typedef     int64_t             amf_int64;
    typedef     int32_t             amf_int32;
    typedef     int16_t             amf_int16;
    typedef     int8_t              amf_int8;

    typedef     uint64_t            amf_uint64;
    typedef     uint32_t            amf_uint32;
    typedef     uint16_t            amf_uint16;
    typedef     uint8_t             amf_uint8;
    typedef     size_t              amf_size;


    #define AMF_STD_CALL
    #define AMF_CDECL_CALL
    #define AMF_FAST_CALL
    #define AMF_INLINE              __inline__
    #define AMF_FORCEINLINE         __inline__

#endif // WIN32

typedef     void*               amf_handle;
typedef     double              amf_double;
typedef     float               amf_float;

typedef     void                amf_void;
typedef     bool                amf_bool;
typedef     long                amf_long; 
typedef     int                 amf_int; 
typedef     unsigned long       amf_ulong; 
typedef     unsigned int        amf_uint; 


struct amf_int4
{  
    amf_int v[4];
};

struct amf_float4
{ 
    amf_float v[4];
};

typedef     amf_int64           amf_pts;     // in 100 nanosecs
#define     AMF_SECOND          10000000L   // 1 second 

extern "C"
{
    // allocator
    AMF_CORE_LINK void*      AMF_STD_CALL amf_alloc(amf_size count);
    AMF_CORE_LINK void       AMF_STD_CALL amf_free(void*  ptr);
}

namespace amf
{
    //----------------------------------------------------------------------------------------------
    // AMFEnumDescriptionEntry
    //----------------------------------------------------------------------------------------------
    struct AMFEnumDescriptionEntry
    {
        amf_int         value;
        const char*     sName;
        const wchar_t*  wName;
    };
}

namespace amf
{
    enum AMF_ERROR
    {
        AMF_OK                   =0,
        AMF_FAIL                 =1,
        AMF_INVALID_PARAM        =2,
        AMF_NO_MEMORY            =3,
        AMF_NOT_ALLOCATED        =4,
        AMF_NO_INTERFACE         =5,
        AMF_NOT_IMPLEMENTED      =6,
        AMF_OPENCL_FAILED        =7,
        AMF_DIRECTX_FAILED       =8,
        AMF_PIPLINE_DESTROYED    =9,
        AMF_ALREADY_INITIALIZED  =10,
        AMF_NOT_INITIALIZED      =11,
        AMF_WRONG_STATE          =12,
        AMF_FILE_NOT_OPEN        =13,
        AMF_EOF                  =14,
        AMF_CODEC_NOT_FOUND      =15,
        AMF_REPEAT               =16, // not real error - repeat the call to primitive
        AMF_PARAM_NOT_FOUND      =17,
        AMF_NULL_POINTER            =18,
        AMF_DIRECT3D_CREATE_FAILED = 19,           //failed to create the direct3d object
        AMF_DIRECT3D_DEVICE_CREATE_FAILED = 20,    //failed to create the direct3d device
        AMF_AVE_SERVICE_CREATE_FAILED = 21,        //failed to create the AVE service
        AMF_ENCODER_CREATE_FAILED = 22,            //failed to create the encoder
        AMF_ENCODER_DESTROY_FAILED = 23,           //failed to destroy the encoder
        AMF_ENCODER_OPEN_FAILED = 24,              //failed to open the AMF encoder
        AMF_INVALID_RESOLUTION = 25,               //invalid resolution (width or height)
        AMF_FORMAT_NOT_SUPPORTED = 26,             //format not supported
        AMF_SURFACE_CREATE_FAILED = 27,            //failed to create the surface
        AMF_LOCK_RECT_FAILED = 28,                 //failed to lock the rect on the texture surface
        AMF_AVE_ENCODE_FAILED = 29,                //failed to execute AVE encode.
        AMF_AVE_RECLAIM_FAILED = 30,               //failed to call AVEReclaim
        AMF_GET_CONFIG_FAILED = 31,                //failed to get the config
        AMF_SET_CONFIG_FAILED = 32,                //failed to set the config
        AMF_DECODER_CREATE_FAILED = 33,            //failed to create the decoder
        AMF_KERNEL_CREATE_FAILED = 34,             //failed to create the kernels
        AMF_DECODE_FAILED = 35,                    //failed to decode
        AMF_XML_ELEMENT_NOT_FOUND = 36,            // required XML element is missing
        AMF_XML_ATTRIBUTE_NOT_FOUND = 37,          // required XML attribute is missing
        AMF_XML_CANNOT_SAVE_FILE = 38,             // save XML failed
        AMF_XML_PARSING_FAIL = 39,                 // XML parsing error
        AMF_FILE_READ_ERROR = 40,                  //cannot read from file
        AMF_NO_HWACCELERATION_SERVICE = 40,        // Hardware acceleration service isn't supported
        AMF_NO_DIRECTX_VIDEO_SERVICE = 41,         // DirectX video service isn't supported
        AMF_NOT_SUPPORTED = 42,                    //feature isn't supported on specific platform
        AMF_XSERVER_CONNECTION_FAILED = 43,        //failed to connect to X server
        AMF_XV_FAILED = 44,                        //failed to use Xv extension
        AMF_INPUT_FULL = 45,                       // primitive have buffers full and cannot accept current input - come back later
        AMF_XML_ATTRIBUTE_WRONG_VALUE = 46,        // XML attribute has wrong value
        AMF_FORMAT_ERROR = 47,                     // data read from file cannot be parsed
        AMF_ALSA_FAILED = 48,                      //failed to use ALSA
        AMF_GLX_FAILED = 49,                       //failed to use GLX
        
        AMF_DECODER_OPEN_FAILED = 50,              //failed to create the decoder
        AMF_STREAM_TYPE_NOT_SUPPORTED = 51,        //stream type not supported
        AMF_CODEC_NOT_SUPPORTED = 52,              //codec not supported

        AMF_DEM_ERROR = 300,
        AMF_DEM_PROPERTY_READONLY = AMF_DEM_ERROR + 1,
        AMF_DEM_REMOTE_DISPLAY_CREATE_FAILED = AMF_DEM_ERROR + 2,
        AMF_DEM_START_ENCODING_FAILED = AMF_DEM_ERROR + 3,
        AMF_DEM_QUERY_OUTPUT_FAILED = AMF_DEM_ERROR + 4,
        AMF_DEM_LAST_ERROR = AMF_DEM_ERROR + 50,    // spacing
    };
}

namespace amf
{
    //----------------------------------------------------------------------------------------------
    enum AMF_CODEC_ID
    {
        // video
        AMF_CODEC_UNKNOWN           =-1,
        AMF_CODEC_RAWVIDEO          = 0,
        AMF_CODEC_RAWVIDEO_STEREO   = 1,
        AMF_CODEC_MPEG2             = 2,
        AMF_CODEC_MJPEG             = 8,
        AMF_CODEC_MPEG4             = 13,
        AMF_CODEC_WMV3              = 73,
        AMF_CODEC_VC1               = 72,
        AMF_CODEC_H264              = 28,
        AMF_CODEC_MVC               = 101,//??
        AMF_CODEC_SVC               = 102,//??
        AMF_CODEC_H264VLDMVC        = 103,//??
        AMF_CODEC_PCM_U8            = 65541,
        AMF_CODEC_PCM_S16LE         = 65536,
        AMF_CODEC_PCM_S32LE         = 65544,
        AMF_CODEC_PCM_F32LE         = 65557,
        AMF_CODEC_PCM_F64LE         = 65541,
        AMF_CODEC_MP3               = 86017,
        AMF_CODEC_AAC               = 86018,
        AMF_CODEC_PCM_MULAW         = 65542,
    };

    //----------------------------------------------------------------------------------------------
}

#if AMF_BUILD_STL
#include <string>

namespace amf
{
    //-------------------------------------------------------------------------------------------------
    // STL allocator redefined - will allocate all memory in "C" runtime of Common.DLL
    //-------------------------------------------------------------------------------------------------
    template<class _Ty>
    class amf_allocator : public std::allocator<_Ty>
    {
    public:
        amf_allocator() : std::allocator<_Ty>()
        {
        }
        amf_allocator(const amf_allocator<_Ty> &rhs) : std::allocator<_Ty>(rhs)
        {
        }
        template<class _Other> amf_allocator(const amf_allocator<_Other>& rhs) : std::allocator<_Ty>(rhs)
        {
        }
        template<class _Other> struct rebind
        {    // convert an allocator<_Ty> to an allocator <_Other>
            typedef amf_allocator<_Other> other;
        };
        void deallocate(typename std::allocator<_Ty>::pointer _Ptr, typename std::allocator<_Ty>::size_type)
        {
            amf_free((void*)_Ptr);
        }
        typename std::allocator<_Ty>::pointer allocate(typename std::allocator<_Ty>::size_type _Count)
        { // allocate array of _Count el ements
            return static_cast<typename std::allocator<_Ty>::pointer>((amf_alloc(_Count*sizeof(_Ty))));
        }
    };


    //-------------------------------------------------------------------------------------------------
    // string classes
    //-------------------------------------------------------------------------------------------------
}
#endif

typedef std::basic_string<char, std::char_traits<char>, amf::amf_allocator<char> > amf_string;
typedef std::basic_string<wchar_t, std::char_traits<wchar_t>, amf::amf_allocator<wchar_t> > amf_wstring;


namespace amf
{

    typedef const wchar_t* const AMFInterfaceIDType;
    #define AMF_DECLARE_IID(__IID) \
    static amf::AMFInterfaceIDType IID() \
            { \
            static amf::AMFInterfaceIDType  uid = L#__IID; \
                return uid; \
            } \
    
    //returns 0 if equal
    #define AMF_COMPARE_IID(__IID1, __IID2) wcscmp(__IID1, __IID2)

    //------------------------------------------------------------------------
    // smart pointer template for all AMF interfaces
    //------------------------------------------------------------------------
    template<typename _Interf> 
    class AMFRefPtr 
    {
    private:
        _Interf* m_pInterf;

        void InternalAcquire()
        {
            if (m_pInterf != 0) {
                m_pInterf->Acquire();
            }
        }
        void InternalRelease()
        {
            if (m_pInterf != 0) {
                m_pInterf->Release();
            }
        }
    public:
        AMFRefPtr() : m_pInterf(NULL)
        {
        }
        AMFRefPtr(const AMFRefPtr<_Interf>& p) : m_pInterf(p.m_pInterf)
        {
            InternalAcquire();
        }
 
        template <typename _OtherInterf>
        AMFRefPtr(const AMFRefPtr<_OtherInterf>& cp)
        {
            void* pInterf = NULL;
            if(cp == 0 || cp->QueryInterface(_Interf::IID(), &pInterf) != AMF_OK)
            {
                pInterf = NULL;
            }
            m_pInterf = static_cast<_Interf*>(pInterf);
        }
    
        template <typename _OtherInterf>
        AMFRefPtr(_OtherInterf* cp)
        {
            void* pInterf = NULL;
            if(cp==NULL || cp->QueryInterface(_Interf::IID(), &pInterf) != AMF_OK)
            {
                pInterf = NULL;
            }
            m_pInterf = static_cast<_Interf*>(pInterf);
        }

        AMFRefPtr(int null) : m_pInterf(NULL)
        {
            if (null != NULL)
            {
                // TODO: invent macros VERIFY for this purpose
                throw;
            }
        }

        AMFRefPtr(long null) : m_pInterf(NULL)
        {
            if (null != NULL)
            {
                // TODO: invent macros VERIFY for this purpose
                throw;
            }
        }

        AMFRefPtr(_Interf* pInterface)  : m_pInterf(pInterface)
        { 
            InternalAcquire(); 
        }

        AMFRefPtr(_Interf* pInterface, bool acquire) : m_pInterf(pInterface)
        {
            if(acquire)
            {
                InternalAcquire();
            }
        }

        ~AMFRefPtr()
        { 
            InternalRelease(); 
        }


        AMFRefPtr& operator=(_Interf* pInterface)
        {
            if (m_pInterf != pInterface)
            {
                _Interf* pOldInterface = m_pInterf;
                m_pInterf = pInterface;
                InternalAcquire();
                if(pOldInterface != NULL)
                {
                    pOldInterface->Release();
                }    
            }
            return *this;
        }
        AMFRefPtr& operator=(const AMFRefPtr<_Interf> & cp)
        { 
            return operator=(cp.m_pInterf); 
        }

        template <typename _OtherInterf>
        AMFRefPtr& operator=(const AMFRefPtr<_OtherInterf>& cp)
        {
            const _OtherInterf* pOtherInterf = cp.GetInterfacePtr();
            if((cp != 0) && (static_cast<const void*>(m_pInterf) == static_cast<const void*>(pOtherInterf)))
            {
                return *this;
            }

            _Interf* pOldInterface = m_pInterf;
            if (pOldInterface != 0)
            {
                pOldInterface->Release();
            }

            void* pInterf = NULL;
            if(cp == 0 || cp->QueryInterface(_Interf::IID(), &pInterf) != AMF_OK)
            {
                pInterf = NULL;
            }
            m_pInterf = static_cast<_Interf*>(pInterf);

            return *this;
        }

        AMFRefPtr& operator=(int null_)
        {
            if(null_ != NULL)
            {
                // TODO: invent macros VERIFY for this purpose
                throw;
            }
            return operator=(static_cast<_Interf*>(NULL));
        }

        AMFRefPtr& operator=(long null_)
        {
            if (null_ != NULL)
            {
                // TODO: invent macros VERIFY for this purpose
                throw;
            }
            return operator=(static_cast<_Interf*>(NULL));
        }

        void Attach(_Interf* pInterface)
        {
            InternalRelease();
            m_pInterf = pInterface;
        }

        void Attach(_Interf* pInterface, bool acquire)
        {
            InternalRelease();
            m_pInterf = pInterface;

            if(acquire)
            {
                if(pInterface == NULL)
                {
                    // TODO: invent macros VERIFY for this purpose
                    throw;
                }
                else
                {
                    InternalAcquire();
                }
            }
        }

        _Interf* Detach()
        {
            _Interf* const pOld = m_pInterf;
            m_pInterf = NULL;
            return pOld;
        }

        operator _Interf*() const
        { 
            return m_pInterf; 
        }

        operator _Interf&() const 
        { 
            //TODO: 0 or NULL?
            if(m_pInterf == 0)
            {
                throw ;
            }
            return *m_pInterf; 
        }

        _Interf& operator*() const 
        {
            //TODO: 0 or NULL?
            if(m_pInterf == 0)
            {
                throw;
            }
            return *m_pInterf; 
        }

        // Returns the address of the interface pointer contained in this
        // class. This is required for initializing from C-style factory function to
        // avoid getting an incorrect ref count at the beginning.

        _Interf** operator&()
        {
            InternalRelease();
            //TODO: 0 or NULL?
            m_pInterf = 0;
            return &m_pInterf;
        }

        _Interf* operator->() const 
        { 
            //TODO: 0 or NULL?
            if (m_pInterf == 0)
            {
                // TODO: invent macros VERIFY for this purpose
                throw ;
            }
            return m_pInterf; 
        }

        operator bool() const
        {
            //TODO: 0 or NULL?
            return m_pInterf != 0; 
        }
        bool operator==(const AMFRefPtr<_Interf>& p) 
        {
            return (m_pInterf == p.m_pInterf);
        }
        bool operator==(_Interf* p) 
        {
            return (m_pInterf == p);
        }
        bool operator==(int null)
        {
            if (null != 0)
            {
                // TODO: invent macros VERIFY for this purpose
                throw;
            }
            return m_pInterf == NULL;
        }
        bool operator==(long null)
        {
            if(null != 0L)
            {
                // TODO: invent macros VERIFY for this purpose
                throw;
            }
            return (NULL == m_pInterf);
        }
        bool operator!=(const AMFRefPtr<_Interf>& p) 
        {
            return !(operator==(p));
        }
        bool operator!=(_Interf* p) 
        {
            return !(operator==(p));
        }
        bool operator!=(int null)
        {
            return !(operator==(null));
        }
        bool operator!=(long null)
        {
            return !(operator==(null));
        }
        _Interf*& GetInterfacePtr()
        { 
            return m_pInterf; 
        }
        const _Interf* GetInterfacePtr() const
        { 
            return m_pInterf; 
        }

    private:

    };

    //------------------------------------------------------------------------
    // base class for all AMF interfaces
    //------------------------------------------------------------------------
    class AMFInterface
    {
    public:
        virtual ~AMFInterface(){}

        AMF_DECLARE_IID("AMFUnknown")

        virtual amf_long    AMF_STD_CALL Acquire() = 0;
        virtual amf_long    AMF_STD_CALL Release() = 0;
        virtual AMF_ERROR   AMF_STD_CALL QueryInterface(AMFInterfaceIDType interfaceID, void** ppInterface) = 0; 
    };

    typedef AMFRefPtr<AMFInterface> AMFInterfacePtr; 

    class AMFClonable : virtual public AMFInterface
    {
    public:
        AMF_DECLARE_IID("AMFClonable")
        virtual AMF_ERROR   AMF_STD_CALL Clone(AMFClonable** ppNewObject) = 0;
    };
    typedef AMFRefPtr<AMFClonable> AMFClonablePtr; 

}// namespace amf


#endif //#ifndef __AMFTypes_h__
