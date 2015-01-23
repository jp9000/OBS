/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/


#include "Main.h"


#define DXGI_ERROR_ACCESS_LOST      _HRESULT_TYPEDEF_(0x887A0026L)
#define DXGI_ERROR_WAIT_TIMEOUT     _HRESULT_TYPEDEF_(0x887A0027L)



bool D3D10OutputDuplicator::Init(UINT output)
{
    HRESULT hRes;

    bool bSuccess = false;

    IDXGIDevice *device;
    if(SUCCEEDED(hRes = GetD3D()->QueryInterface(__uuidof(IDXGIDevice), (void**)&device)))
    {
        IDXGIAdapter *adapter;
        if(SUCCEEDED(hRes = device->GetAdapter(&adapter)))
        {
            IDXGIOutput *outputInterface;
            if(SUCCEEDED(hRes = adapter->EnumOutputs(output, &outputInterface)))
            {
                IDXGIOutput1 *output1;

                if(SUCCEEDED(hRes = outputInterface->QueryInterface(__uuidof(IDXGIOutput1), (void**)&output1)))
                {
                    if(SUCCEEDED(hRes = output1->DuplicateOutput(GetD3D(), &duplicator)))
                        bSuccess = true;
                    /*else
                        AppWarning(TEXT("D3D10OutputDuplicator::Init: output1->DuplicateOutput failed, result = %u"), (UINT)hRes);*/

                    output1->Release();
                }
                /*else
                    AppWarning(TEXT("D3D10OutputDuplicator::Init: outputInterface->QueryInterface failed, result = %u"), (UINT)hRes);*/

                outputInterface->Release();
            }
            /*else
                AppWarning(TEXT("D3D10OutputDuplicator::Init: adapter->EnumOutputs failed, result = %u"), (UINT)hRes);*/

            adapter->Release();
        }
        /*else
            AppWarning(TEXT("D3D10OutputDuplicator::Init: device->GetAdapter failed, result = %u"), (UINT)hRes);*/

        device->Release();
    }
    /*else
        AppWarning(TEXT("D3D10OutputDuplicator::Init: GetD3D()->QueryInterface failed, result = %u"), (UINT)hRes);*/

    return bSuccess;
}

D3D10OutputDuplicator::~D3D10OutputDuplicator()
{
    SafeRelease(duplicator);
    delete copyTex;
}

DuplicatorInfo D3D10OutputDuplicator::AcquireNextFrame(UINT timeout)
{
    if(!duplicator)
    {
        AppWarning(TEXT("D3D10OutputDuplicator::AcquireNextFrame: Well, apparently there's no duplicator."));
        return DuplicatorInfo_Error;
    }

    //------------------------------------------

    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    IDXGIResource *tempResource = NULL;

    HRESULT hRes = duplicator->AcquireNextFrame(timeout, &frameInfo, &tempResource);
    if(hRes == DXGI_ERROR_ACCESS_LOST)
        return DuplicatorInfo_Lost;
    else if(hRes == DXGI_ERROR_WAIT_TIMEOUT)
        return DuplicatorInfo_Timeout;
    else if(FAILED(hRes))
        return DuplicatorInfo_Error;

    //------------------------------------------

    ID3D10Texture2D *texVal;
    if(FAILED(hRes = tempResource->QueryInterface(__uuidof(ID3D10Texture2D), (void**)&texVal)))
    {
        SafeRelease(tempResource);
        AppWarning(TEXT("D3D10OutputDuplicator::AcquireNextFrame: could not query interface, result = 0x%08lX"), hRes);
        return DuplicatorInfo_Error;
    }

    tempResource->Release();

    //------------------------------------------

    D3D10_TEXTURE2D_DESC texDesc;
    texVal->GetDesc(&texDesc);

    if(!copyTex || copyTex->Width() != texDesc.Width || copyTex->Height() != texDesc.Height)
    {
        delete copyTex;
        copyTex = CreateTexture(texDesc.Width, texDesc.Height, ConvertGIBackBufferFormat(texDesc.Format), NULL, FALSE, TRUE);
    }

    //------------------------------------------

    if(copyTex)
    {
        D3D10Texture *d3dCopyTex = (D3D10Texture*)copyTex;
        GetD3D()->CopyResource(d3dCopyTex->texture, texVal);
    }

    SafeRelease(texVal);
    duplicator->ReleaseFrame();

    return DuplicatorInfo_Acquired;
}

Texture* D3D10OutputDuplicator::GetCopyTexture()
{
    return copyTex;
}

Texture* D3D10OutputDuplicator::GetCursorTex(POINT* pos)
{
    if(pos)
        mcpy(pos, &cursorPos, sizeof(POINT));

    if(bCursorVis)
        return cursorTex;

    return NULL;
}
