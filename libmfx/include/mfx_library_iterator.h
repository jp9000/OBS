/* ****************************************************************************** *\

Copyright (C) 2012-2013 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfx_library_iterator.h

\* ****************************************************************************** */

#if !defined(__MFX_LIBRARY_ITERATOR_H)
#define __MFX_LIBRARY_ITERATOR_H


#include <mfxvideo.h>
#include "mfx_win_reg_key.h"
#include "mfx_dispatcher.h"

namespace MFX
{

// declare desired storage ID
enum
{
    MFX_UNKNOWN_KEY             = -1,
    MFX_CURRENT_USER_KEY        = 0,
    MFX_LOCAL_MACHINE_KEY       = 1,

    MFX_KEY_TYPES
};

class MFXLibraryIterator
{
public:
    // Default constructor
    MFXLibraryIterator(void);
    // Destructor
    ~MFXLibraryIterator(void);

    // Initialize the iterator
    mfxStatus Init(eMfxImplType implType, mfxIMPL implInterface, const mfxU32 adapterNum, int storageID);

    // Get the next library path
    mfxStatus SelectDLLVersion(wchar_t *pPath, size_t pathSize, eMfxImplType *pImplType, mfxVersion minVersion);

    // Return interface type on which Intel adapter was found (if any): D3D9 or D3D11
    mfxIMPL GetImplementationType(); 

protected:

    // Release the iterator
    void Release(void);

    WinRegKey m_baseRegKey;                                     // (WinRegKey) main registry key    

    eMfxImplType m_implType;                                    // Required library implementation 
    mfxIMPL m_implInterface;                                    // Required interface (D3D9, D3D11)

    mfxU32 m_vendorID;                                          // (mfxU32) property of used graphic card
    mfxU32 m_deviceID;                                          // (mfxU32) property of used graphic card

    mfxU32 m_lastLibIndex;                                      // (mfxU32) index of previously returned library
    mfxU32 m_lastLibMerit;                                      // (mfxU32) merit of previously returned library

private:
    // unimplemented by intent to make this class non-copyable
    MFXLibraryIterator(const MFXLibraryIterator &);
    void operator=(const MFXLibraryIterator &);
};

} // namespace MFX

#endif // __MFX_LIBRARY_ITERATOR_H
