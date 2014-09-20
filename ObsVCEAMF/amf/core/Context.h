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
 * @file  Context.h
 * @brief AMFContext interface declaration
 ***************************************************************************************************
 */

#ifndef __AMFContext_h__
#define __AMFContext_h__
#pragma once

#include "Buffer.h"
#include "Surface.h"

namespace amf
{
    //----------------------------------------------------------------------------------------------
    // AMFContext interface
    //----------------------------------------------------------------------------------------------
    class AMFContext : virtual public AMFPropertyStorage
    {
    public:
        AMF_DECLARE_IID(0xa76a13f0, 0xd80e, 0x4fcc, 0xb5, 0x8, 0x65, 0xd0, 0xb5, 0x2e, 0xd9, 0xee)

        virtual ~AMFContext() {}

        // all contexts can be NULLs
        virtual AMF_RESULT AMF_STD_CALL Terminate() = 0;
        /**
         *******************************************************************************
         *   InitDX9
         *
         *   @brief
         *       Initialize DX9 device in the context
         *
         *       It must be called to have ability to use DX9 functionality in context,
         *       otherwise DX9 unavailable.
         *       If pass NULL then DX9 initialized by AMF
         *       If pass not NULL valid value supporting DX9 DeviceEx interface then it will be used for subsequent DX9 operations.
         *       AddRef is called for passed value, on device destruction Release is called.
         *       If pass valid pointer to DX9 Device not supporting DeviceEx interface then new DeviceEx device is creating inside context
         *       on the same adapter as passed Device
         *       This was done for optimization to have interop ability on surfaces allocated in media sdk.
         *
         *******************************************************************************
         */
        virtual AMF_RESULT AMF_STD_CALL InitDX9(void* pDX9Device) = 0;
        /**
         *******************************************************************************
         *   GetDX9Device
         *
         *   @brief
         *      If DX9 version required specified then always returns internal value.
         *
         *      If DX9_EX specified then return device if EX device is inside, otherwise returns NULL        
         *
         *******************************************************************************
         */
        virtual void* AMF_STD_CALL GetDX9Device(AMF_DX_VERSION dxVersionRequired = AMF_DX9) = 0;
        virtual AMF_RESULT AMF_STD_CALL LockDX9() = 0;
        virtual AMF_RESULT AMF_STD_CALL UnlockDX9() = 0;

        class AMFDX9Locker
        {
        public:
            AMFDX9Locker() : m_Context(NULL)
            {}
            AMFDX9Locker(AMFContext* resources) : m_Context(NULL)
            {
                Lock(resources);
            }
            ~AMFDX9Locker()
            {
                if(m_Context != NULL)
                {
                    m_Context->UnlockDX9();
                }
            }
            void Lock(AMFContext* resources)
            {
                if(m_Context != NULL)
                {
                    m_Context->UnlockDX9();
                }
                m_Context = resources;
                if(m_Context != NULL)
                {
                    m_Context->LockDX9();
                }
            }
        protected:
            AMFContext* m_Context;

        private:
            AMFDX9Locker(const AMFDX9Locker&);
            AMFDX9Locker& operator=(const AMFDX9Locker&);
        };
        /**
         *******************************************************************************
         *   InitDX11
         *
         *   @brief
         *       Initialize DX11 device in the context
         *
         *       It must be called to have ability to use DX11 functionality in context,
         *       otherwise DX11 unavailable.
         *       If pass NULL then DX11 initialized by AMF
         *       If pass not NULL value then it will be used for subsequent DX11 operations.
         *       AddRef is called for passed value, on device destruction Release is called
         *******************************************************************************
         */
        virtual AMF_RESULT AMF_STD_CALL InitDX11(void* pDX11Device, AMF_DX_VERSION dxVersionRequired = AMF_DX11_0) = 0;
        virtual void* AMF_STD_CALL GetDX11Device(AMF_DX_VERSION dxVersionRequired = AMF_DX11_0) = 0;
        virtual AMF_RESULT AMF_STD_CALL LockDX11() = 0;
        virtual AMF_RESULT AMF_STD_CALL UnlockDX11() = 0;

        class AMFDX11Locker
        {
        public:
            AMFDX11Locker() : m_Context(NULL)
            {}
            AMFDX11Locker(AMFContext* resources) : m_Context(NULL)
            {
                Lock(resources);
            }
            ~AMFDX11Locker()
            {
                if(m_Context != NULL)
                {
                    m_Context->UnlockDX11();
                }
            }
            void Lock(AMFContext* resources)
            {
                if(m_Context != NULL)
                {
                    m_Context->UnlockDX11();
                }
                m_Context = resources;
                if(m_Context != NULL)
                {
                    m_Context->LockDX11();
                }
            }
        protected:
            AMFContext* m_Context;

        private:
            AMFDX11Locker(const AMFDX11Locker&);
            AMFDX11Locker& operator=(const AMFDX11Locker&);
        };

        /**
         *******************************************************************************
         *   InitOpenCL
         *
         *   @brief
         *       Initialize OpenCL device in the context
         *
         *       If pass NULL then DX11 initialized by AMF
         *       If pass not NULL value then it will be used for subsequent OpenCL operations.
         *
         *******************************************************************************
         */

        virtual AMF_RESULT AMF_STD_CALL InitOpenCL(void* pCommandQueue = NULL) = 0;
        virtual void* AMF_STD_CALL GetOpenCLContext() = 0;
        virtual void* AMF_STD_CALL GetOpenCLCommandQueue() = 0;
        virtual void* AMF_STD_CALL GetOpenCLDeviceID() = 0;
        virtual AMF_RESULT AMF_STD_CALL LockOpenCL() = 0;
        virtual AMF_RESULT AMF_STD_CALL UnlockOpenCL() = 0;

        class AMFOpenCLLocker
        {
        public:
            AMFOpenCLLocker() : m_Context(NULL)
            {}
            AMFOpenCLLocker(AMFContext* resources) : m_Context(NULL)
            {
                Lock(resources);
            }
            ~AMFOpenCLLocker()
            {
                if(m_Context != NULL)
                {
                    m_Context->UnlockOpenCL();
                }
            }
            void Lock(AMFContext* resources)
            {
                if(m_Context != NULL)
                {
                    m_Context->UnlockOpenCL();
                }
                m_Context = resources;
                if(m_Context != NULL)
                {
                    m_Context->LockOpenCL();
                }
            }
        protected:
            AMFContext* m_Context;
        private:
            AMFOpenCLLocker(const AMFOpenCLLocker&);
            AMFOpenCLLocker& operator=(const AMFOpenCLLocker&);
        };

        /**
         *******************************************************************************
         *   InitOpenGL
         *
         *   @brief
         *       Initialize OpenGL device in the context
         *
         *       It must be called to have ability to use OpenGL functionality in context,
         *       otherwise OpenGL unavailable.
         *       If pass NULL, NULL then OpenGL initialized by AMF
         *       That created resources are freed in context termination.
         *       If pass not NULL values then it will be used for subsequent OpenGL operations.
         *       OpenGL resources are not reference countable, so no action in OpenGL is done
         *       with passed context and drawable.
         *       When the context destructed - attached context/drawable are not freed.
         *   @note
         *       Arguments should be either both NULLs or no one
         *******************************************************************************
         */
        virtual AMF_RESULT AMF_STD_CALL InitOpenGL(amf_handle hOpenGLContext, amf_handle hWindow, amf_handle hDC) = 0;
        virtual amf_handle AMF_STD_CALL GetOpenGLContext() = 0;
        virtual amf_handle AMF_STD_CALL GetOpenGLDrawable() = 0;
        virtual AMF_RESULT AMF_STD_CALL LockOpenGL() = 0;
        virtual AMF_RESULT AMF_STD_CALL UnlockOpenGL() = 0;

        class AMFOpenGLLocker
        {
        public:
            AMFOpenGLLocker(AMFContext* pContext) : m_pContext(pContext),
                m_GLLocked(false)
            {
                if(m_pContext != NULL)
                {
                    if(m_pContext->LockOpenGL() == AMF_OK)
                    {
                        m_GLLocked = true;
                    }
                }
            }
            ~AMFOpenGLLocker()
            {
                if(m_GLLocked)
                {
                    m_pContext->UnlockOpenGL();
                }
            }
        private:
            AMFContext* m_pContext;
            amf_bool m_GLLocked; ///< AMFOpenGLLocker can be called when OpenGL is not initialized yet
                                 ///< in this case don't call UnlockOpenGL
            AMFOpenGLLocker(const AMFOpenGLLocker&);
            AMFOpenGLLocker& operator=(const AMFOpenGLLocker&);
        };

        /**
         *******************************************************************************
         *   InitXV
         *
         *   @brief
         *       Initialize XV device in the context
         *
         *       It must be called to have ability to use XV functionality in context,
         *       otherwise XV unavailable.
         *       If pass NULL, NULL then XV initialized by AMF
         *       That created resources are freed in context termination.
         *       If pass not NULL values then it will be used for subsequent XV operations.
         *       XV resources are not reference countable, so no action in XV is done
         *       with passed context and drawable.
         *       When the context destructed - attached context/drawable are not freed.
         *   @note
         *       Arguments should be either both NULLs or no one
         *******************************************************************************
         */
        virtual AMF_RESULT AMF_STD_CALL InitXV(void* pXVDevice) = 0;
        virtual void* AMF_STD_CALL GetXVDevice() = 0;
        virtual AMF_RESULT AMF_STD_CALL LockXV() = 0;
        virtual AMF_RESULT AMF_STD_CALL UnlockXV() = 0;

        class AMFXVLocker
        {
        public:
            AMFXVLocker() : m_pContext(NULL)
            {}
            AMFXVLocker(AMFContext* pContext) : m_pContext(NULL)
            {
                Lock(pContext);
            }
            ~AMFXVLocker()
            {
                if(m_pContext != NULL)
                {
                    m_pContext->UnlockXV();
                }
            }
            void Lock(AMFContext* pContext)
            {
                if((pContext != NULL) && (pContext->GetXVDevice() != NULL))
                {
                    m_pContext = pContext;
                    m_pContext->LockXV();
                }
            }
        protected:
            AMFContext* m_pContext;
        private:
            AMFXVLocker(const AMFXVLocker&);
            AMFXVLocker& operator=(const AMFXVLocker&);
        };
        /**
         *******************************************************************************
         *   InitGralloc
         *
         *   @brief
         *       Initialize Gralloc device in the context
         *
         *       It must be called to have ability to use Gralloc functionality in context,
         *       otherwise Gralloc unavailable.
         *       If pass NULL, NULL then Gralloc initialized by AMF
         *       That created resources are freed in context termination.
         *       If pass not NULL values then it will be used for subsequent Gralloc operations.
         *       Gralloc resources are not reference countable, so no action in Gralloc is done
         *       with passed context and drawable.
         *       When the context destructed - attached context/drawable are not freed.
         *   @note
         *       Arguments should be either both NULLs or no one
         *******************************************************************************
         */
        virtual AMF_RESULT AMF_STD_CALL InitGralloc(void* pGrallocDevice) = 0;
        virtual void* AMF_STD_CALL GetGrallocDevice() = 0;
        virtual AMF_RESULT AMF_STD_CALL LockGralloc() = 0;
        virtual AMF_RESULT AMF_STD_CALL UnlockGralloc() = 0;

        class AMFGrallocLocker
        {
        public:
            AMFGrallocLocker() : m_pContext(NULL)
            {}
            AMFGrallocLocker(AMFContext* pContext) : m_pContext(NULL)
            {
                Lock(pContext);
            }
            ~AMFGrallocLocker()
            {
                if(m_pContext != NULL)
                {
                    m_pContext->UnlockGralloc();
                }
            }
            void Lock(AMFContext* pContext)
            {
                if((pContext != NULL) && (pContext->GetGrallocDevice() != NULL))
                {
                    m_pContext = pContext;
                    m_pContext->LockGralloc();
                }
            }
        protected:
            AMFContext* m_pContext;
        private:
            AMFGrallocLocker(const AMFGrallocLocker&);
            AMFGrallocLocker& operator=(const AMFGrallocLocker&);
        };

        /**
         *******************************************************************************
         *   AllocBuffer
         *
         *   @brief
         *       Creates AllocBuffer and allocate memory 
         *
         *   @note
         *
         *******************************************************************************
         */
        virtual AMF_RESULT AMF_STD_CALL AllocBuffer(AMF_MEMORY_TYPE type, amf_size size, AMFBuffer** ppBuffer) = 0;
        /**
         *******************************************************************************
         *   CreateBufferFromHostNative
         *
         *   @brief
         *       Creates AMFBuffer on passed system memory contiguous block
         *
         *       Created AMFSurface doesn't delete the memory.
         *       There are 2 scenarios of correct memory release.
         *       1) Make sure that the surface is freed, then release memory
         *       2) AddObserver to the surface, in its Destroy method release memory
         *       3) Specify pObserver which is equivalent of AddObserver later
         *
         *******************************************************************************
         */
        virtual AMF_RESULT AMF_STD_CALL CreateBufferFromHostNative(void* pHostBuffer, amf_size size, AMFBuffer** ppBuffer, AMFBufferObserver* pObserver) = 0;
        /**
         *******************************************************************************
         *   AllocSurface
         *
         *   @brief
         *       Creates AMFSurface and allocate memory 
         *
         *   @note
         *
         *******************************************************************************
         */
        virtual AMF_RESULT AMF_STD_CALL AllocSurface(AMF_MEMORY_TYPE type, AMF_SURFACE_FORMAT format, amf_int32 width, amf_int32 height, AMFSurface** ppSurface) = 0;
        /**
         *******************************************************************************
         *   CreateSurfaceFromHostNative
         *
         *   @brief
         *       Creates AMFSurface on passed system memory contiguous block
         *
         *       Created AMFSurface doesn't delete the memory.
         *       There are 2 scenarios of correct memory release.
         *       1) Make sure that the surface is freed, then release memory
         *       2) AddObserver to the surface, in its Destroy method release memory
         *       3) Specify pObserver which is equivalent of AddObserver later
         *
         *   @note
         *       This version with one width, height, hPitch values is applicable only
         *       to formats having one plane
         *******************************************************************************
         */
        virtual AMF_RESULT AMF_STD_CALL CreateSurfaceFromHostNative(AMF_SURFACE_FORMAT format,
            amf_int32 width, amf_int32 height, amf_int32 hPitch, amf_int32 vPitch, void* pData, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver) = 0;
        /**
         *******************************************************************************
         *   CreateSurfaceFromDX9Native
         *
         *   @brief
         *       Creates AMFSurface on passed DX9Surface*.
         *
         *       It calls AddRef on passed surface,
         *       when created AMFSurface is freed - it calls Release on attached DX9
         *       surface. Before Release it calls AMFSurfaceObserver::Destroy method,
         *       it allows custom resource management.
         *       pObserver could be specified to get notification when attached resources could be released
         *******************************************************************************
         */
        virtual AMF_RESULT AMF_STD_CALL CreateSurfaceFromDX9Native(void* pDX9Surface, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver) = 0;
        /**
         *******************************************************************************
         *   CreateSurfaceFromDX11Native
         *
         *   @brief
         *       Creates AMFSurface on passed D3D11Texture*.
         *       Works exactly like CreateSurfaceFromDX9Native
         *******************************************************************************
         */
        virtual AMF_RESULT AMF_STD_CALL CreateSurfaceFromDX11Native(void* pDX11Surface, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver) = 0;
        /**
         *******************************************************************************
         *   CreateSurfaceFromOpenGLNative
         *
         *   @brief
         *       Creates AMFSurface on passed OpenGL texture handle.
         *          should be GL_RGBA
         *          you can specify AMF format AMF_SURFACE_BGRA or AMF_SURFACE_RGBA.
         *          default AMF_SURFACE_BGRA
         *       Created AMFSurface doesn't free attached OpenGL texture.
         *******************************************************************************
         */
        virtual AMF_RESULT AMF_STD_CALL CreateSurfaceFromOpenGLNative(AMF_SURFACE_FORMAT format, amf_handle hGLTextureID, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver) = 0;
       /**
         *******************************************************************************
         *   CreateSurfaceFromGrallocNative
         *
         *   @brief
         *       Creates AMFSurface on passed Gralloc handle.
         *
         *       Created AMFSurface doesn't free attached Gralloc texture.
         *******************************************************************************
         */
        virtual AMF_RESULT AMF_STD_CALL CreateSurfaceFromGrallocNative(amf_handle hGrallocSurface, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver) = 0;
        /**
         *******************************************************************************
         *   CreateSurfaceFromOpenCLNative
         *
         *   @brief
         *       Creates AMFSurface on passed OpenCL objects.
         *******************************************************************************
         */
        virtual AMF_RESULT AMF_STD_CALL CreateSurfaceFromOpenCLNative(AMF_SURFACE_FORMAT format, amf_int32 width, amf_int32 height, void** pClPlanes, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver) = 0;
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFContext> AMFContextPtr;
}

extern "C"
{
    // class AMFContext object
    AMF_CORE_LINK AMF_RESULT AMF_CDECL_CALL AMFCreateContext(amf::AMFContext** ppContext);
}



#endif //#ifndef __AMFContext_h__
