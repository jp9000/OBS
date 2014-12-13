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

//extern "C" _declspec(dllexport) DWORD NvOptimusEnablement = 0x00000000;

void GetDisplayDevices(DeviceOutputs &deviceList)
{
    HRESULT err;

    deviceList.ClearData();

#ifdef USE_DXGI1_2
    REFIID iidVal = OSGetVersion() >= 8 ? __uuidof(IDXGIFactory2) : __uuidof(IDXGIFactory1);
#else
    REFIIF iidVal = __uuidof(IDXGIFactory1);
#endif

    IDXGIFactory1 *factory;
    if(SUCCEEDED(err = CreateDXGIFactory1(iidVal, (void**)&factory)))
    {
        UINT i=0;
        IDXGIAdapter1 *giAdapter;

        while(factory->EnumAdapters1(i++, &giAdapter) == S_OK)
        {
            //Log(TEXT("------------------------------------------"));

            DXGI_ADAPTER_DESC adapterDesc;
            if(SUCCEEDED(err = giAdapter->GetDesc(&adapterDesc)))
            {
                if (adapterDesc.DedicatedVideoMemory != 0) {
                    DeviceOutputData &deviceData = *deviceList.devices.CreateNew();
                    deviceData.strDevice = adapterDesc.Description;

                    UINT j=0;
                    IDXGIOutput *giOutput;
                    while(giAdapter->EnumOutputs(j++, &giOutput) == S_OK)
                    {
                        DXGI_OUTPUT_DESC outputDesc;
                        if(SUCCEEDED(giOutput->GetDesc(&outputDesc)))
                        {
                            if(outputDesc.AttachedToDesktop)
                            {
                                deviceData.monitorNameList << outputDesc.DeviceName;

                                MonitorInfo &monitorInfo = *deviceData.monitors.CreateNew();
                                monitorInfo.hMonitor = outputDesc.Monitor;
                                mcpy(&monitorInfo.rect, &outputDesc.DesktopCoordinates, sizeof(RECT));
                                switch (outputDesc.Rotation) {
                                case DXGI_MODE_ROTATION_ROTATE90:
                                    monitorInfo.rotationDegrees = 90.0f;
                                    break;
                                case DXGI_MODE_ROTATION_ROTATE180:
                                    monitorInfo.rotationDegrees = 180.0f;
                                    break;
                                case DXGI_MODE_ROTATION_ROTATE270:
                                    monitorInfo.rotationDegrees = 270.0f;
                                }
                            }
                        }

                        giOutput->Release();
                    }
                }
            }
            else
                AppWarning(TEXT("Could not query adapter %u"), i);

            giAdapter->Release();
        }

        factory->Release();
    }
}


void LogVideoCardStats()
{
    HRESULT err;

#ifdef USE_DXGI1_2
    REFIID iidVal = OSGetVersion() >= 8 ? __uuidof(IDXGIFactory2) : __uuidof(IDXGIFactory1);
#else
    REFIIF iidVal = __uuidof(IDXGIFactory1);
#endif

    IDXGIFactory1 *factory;
    if(SUCCEEDED(err = CreateDXGIFactory1(iidVal, (void**)&factory)))
    {
        UINT i=0;
        IDXGIAdapter1 *giAdapter;

        while(factory->EnumAdapters1(i++, &giAdapter) == S_OK)
        {
            DXGI_ADAPTER_DESC adapterDesc;
            if(SUCCEEDED(err = giAdapter->GetDesc(&adapterDesc)))
            {
                if (!(adapterDesc.VendorId == 0x1414 && adapterDesc.DeviceId == 0x8c)) { // Ignore Microsoft Basic Render Driver
                    Log(TEXT("------------------------------------------"));
                    Log(TEXT("Adapter %u"), i);
                    Log(TEXT("  Video Adapter: %s"), adapterDesc.Description);
                    Log(TEXT("  Video Adapter Dedicated Video Memory: %u"), adapterDesc.DedicatedVideoMemory);
                    Log(TEXT("  Video Adapter Shared System Memory: %u"), adapterDesc.SharedSystemMemory);

                    UINT j = 0;
                    IDXGIOutput *output;
                    while(SUCCEEDED(giAdapter->EnumOutputs(j++, &output)))
                    {
                        DXGI_OUTPUT_DESC desc;
                        if(SUCCEEDED(output->GetDesc(&desc)))
                            Log(TEXT("  Video Adapter Output %u: pos={%d, %d}, size={%d, %d}, attached=%s"), j,
                                desc.DesktopCoordinates.left, desc.DesktopCoordinates.top,
                                desc.DesktopCoordinates.right-desc.DesktopCoordinates.left, desc.DesktopCoordinates.bottom-desc.DesktopCoordinates.top,
                                desc.AttachedToDesktop ? L"true" : L"false");
                        output->Release();
                    }
                }
            }
            else
                AppWarning(TEXT("Could not query adapter %u"), i);

            giAdapter->Release();
        }

        factory->Release();
    }
}

static void HandleNvidiaOptimus(IDXGIFactory1 *factory, IDXGIAdapter1 *&adapter, UINT &adapterID)
{
    if (adapterID != 1)
        return;

    //NvOptimusEnablement = 0;
    DXGI_ADAPTER_DESC adapterDesc;
    if (SUCCEEDED(adapter->GetDesc(&adapterDesc)))
    {
        String name = adapterDesc.Description;
        name.KillSpaces();

        if (name.IsEmpty())
            return;

        if (sstri(adapterDesc.Description, L"NVIDIA") != NULL)
        {
            if (name[name.Length()-1] == 'M' || name[name.Length()-1] == 'm') {
                adapter->Release();

                adapterID = 0;
                //NvOptimusEnablement = 1;
                Log(L"Nvidia optimus detected, second adapter selected, ignoring useless second adapter, I guess.");
                if(FAILED(factory->EnumAdapters1(adapterID, &adapter)))
                    CrashError(TEXT("Could not get DXGI adapter"));
            }
        }
    }
}

const static D3D_FEATURE_LEVEL featureLevels[] =
{
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
	D3D_FEATURE_LEVEL_9_3,
};

D3D10System::D3D10System()
{
    HRESULT err;

#ifdef USE_DXGI1_2
    REFIID iidVal = OSGetVersion() >= 8 ? __uuidof(IDXGIFactory2) : __uuidof(IDXGIFactory1);
#else
    REFIID iidVal = __uuidof(IDXGIFactory1);
#endif

    UINT adapterID = GlobalConfig->GetInt(TEXT("Video"), TEXT("Adapter"), 0);

    if(FAILED(err = CreateDXGIFactory1(iidVal, (void**)&factory)))
        CrashError(TEXT("Could not create DXGI factory"));

    UINT numAdapters = 0, i = 0;
    IDXGIAdapter1 *giAdapter;

    while (factory->EnumAdapters1(i++, &giAdapter) == S_OK)
    {
        numAdapters++;
        giAdapter->Release();
    }

    if (adapterID >= numAdapters)
    {
        Log(TEXT("Invalid adapter id %d, only %d adapters on system. Resetting to 0."), adapterID, numAdapters);
        GlobalConfig->SetInt(TEXT("Video"), TEXT("Adapter"), 0);
        adapterID = 0;
    }

    IDXGIAdapter1 *adapter;
    if(FAILED(err = factory->EnumAdapters1(adapterID, &adapter)))
        CrashError(TEXT("Could not get DXGI adapter %d"), adapterID);

    HandleNvidiaOptimus(factory, adapter, adapterID);

    //------------------------------------------------------------------

    DXGI_SWAP_CHAIN_DESC swapDesc;
    zero(&swapDesc, sizeof(swapDesc));
    swapDesc.BufferCount = 2;
    swapDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapDesc.BufferDesc.Width  = App->renderFrameWidth;
    swapDesc.BufferDesc.Height = App->renderFrameHeight;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.OutputWindow = hwndRenderFrame;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.Windowed = TRUE;

    bDisableCompatibilityMode = 1;//AppConfig->GetInt(TEXT("Video"), TEXT("DisableD3DCompatibilityMode"), 1) != 0;

    UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    if(GlobalConfig->GetInt(TEXT("General"), TEXT("UseDebugD3D")))
        createFlags |= D3D11_CREATE_DEVICE_DEBUG;

    D3D_FEATURE_LEVEL level;

    String adapterName;
    DXGI_ADAPTER_DESC desc;
    if (adapter->GetDesc(&desc) == S_OK)
        adapterName = desc.Description;
    else
        adapterName = TEXT("<unknown>");

    adapterName.KillSpaces();

    Log(TEXT("Loading up D3D11 on %s (Adapter %u)..."), adapterName.Array(), adapterID+1);

    //D3D11_CREATE_DEVICE_DEBUG
    //D3D11_DRIVER_TYPE_REFERENCE, D3D11_DRIVER_TYPE_HARDWARE
    err = D3D11CreateDeviceAndSwapChain(adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, createFlags, featureLevels, sizeof(featureLevels) / sizeof(featureLevels[0]), D3D11_SDK_VERSION, &swapDesc, &swap, &d3d, &level, &context);
    if(FAILED(err))
    {
        Log (TEXT("D3D11CreateDeviceAndSwapChain: Failed on %s: 0x%08x"), adapterName.Array(), err);
        CrashError(TEXT("Could not initialize DirectX 11 on %s.  This error can happen for one of the following reasons:\r\n\r\n1.) Your GPU is not supported (DirectX 11 is required - note that many integrated laptop GPUs do not support DX11)\r\n2.) You're running Windows Vista without the \"Platform Update\"\r\n3.) Your video card drivers are out of date\r\n\r\nIf you are using a laptop with NVIDIA Optimus or AMD Switchable Graphics, make sure OBS is set to run on the high performance GPU in your driver settings."), adapterName.Array());
    }

    adapter->Release();

    //------------------------------------------------------------------

    D3D11_DEPTH_STENCIL_DESC depthDesc;
    zero(&depthDesc, sizeof(depthDesc));
    depthDesc.DepthEnable = FALSE;

    err = d3d->CreateDepthStencilState(&depthDesc, &depthState);
    if(FAILED(err))
        CrashError(TEXT("Unable to create depth state"));

    context->OMSetDepthStencilState(depthState, 0);

    //------------------------------------------------------------------

    D3D11_RASTERIZER_DESC rasterizerDesc;
    zero(&rasterizerDesc, sizeof(rasterizerDesc));
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_NONE;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthClipEnable = TRUE;

    err = d3d->CreateRasterizerState(&rasterizerDesc, &rasterizerState);
    if(FAILED(err))
        CrashError(TEXT("Unable to create rasterizer state"));

    context->RSSetState(rasterizerState);

    //------------------------------------------------------------------

    rasterizerDesc.ScissorEnable = TRUE;

    err = d3d->CreateRasterizerState(&rasterizerDesc, &scissorState);
    if(FAILED(err))
        CrashError(TEXT("Unable to create scissor state"));

    //------------------------------------------------------------------

    ID3D11Texture2D *backBuffer = NULL;
    err = swap->GetBuffer(0, IID_ID3D11Texture2D, (void**)&backBuffer);
    if(FAILED(err))
        CrashError(TEXT("Unable to get back buffer from swap chain"));

    err = d3d->CreateRenderTargetView(backBuffer, NULL, &swapRenderView);
    if(FAILED(err))
        CrashError(TEXT("Unable to get render view from back buffer"));

    backBuffer->Release();

    //------------------------------------------------------------------

    D3D11_BLEND_DESC disabledBlendDesc;
    zero(&disabledBlendDesc, sizeof(disabledBlendDesc));
    for(int i=0; i<8; i++)
    {
        disabledBlendDesc.RenderTarget[i].BlendEnable           = TRUE;
        disabledBlendDesc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        disabledBlendDesc.RenderTarget[i].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        disabledBlendDesc.RenderTarget[i].BlendOp               = D3D11_BLEND_OP_ADD;
        disabledBlendDesc.RenderTarget[i].SrcBlendAlpha         = D3D11_BLEND_ONE;
        disabledBlendDesc.RenderTarget[i].DestBlendAlpha        = D3D11_BLEND_ZERO;
        disabledBlendDesc.RenderTarget[i].SrcBlend              = D3D11_BLEND_ONE;
        disabledBlendDesc.RenderTarget[i].DestBlend             = D3D11_BLEND_ZERO;
    }

    err = d3d->CreateBlendState(&disabledBlendDesc, &disabledBlend);
    if(FAILED(err))
        CrashError(TEXT("Unable to create disabled blend state"));

    this->BlendFunction(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA, 1.0f);
    bBlendingEnabled = true;
}

D3D10System::~D3D10System()
{
    delete spriteVertexBuffer;
    delete boxVertexBuffer;

    for(UINT i=0; i<blends.Num(); i++)
        SafeRelease(blends[i].blendState);

    SafeRelease(scissorState);
    SafeRelease(rasterizerState);
    SafeRelease(depthState);
    SafeRelease(disabledBlend);
    SafeRelease(swapRenderView);
    SafeRelease(swap);
    SafeRelease(context);
    SafeRelease(d3d);
    SafeRelease(factory);
}

void D3D10System::UnloadAllData()
{
    LoadVertexShader(NULL);
    LoadPixelShader(NULL);
    LoadVertexBuffer(NULL);
    for(UINT i=0; i<8; i++)
    {
        LoadSamplerState(NULL, i);
        LoadTexture(NULL, i);
    }

    UINT zeroVal = 0;
    LPVOID nullBuff[8];
    float bla[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    zero(nullBuff, sizeof(nullBuff));

    context->VSSetConstantBuffers(0, 1, (ID3D11Buffer**)nullBuff);
    context->PSSetConstantBuffers(0, 1, (ID3D11Buffer**)nullBuff);
    context->OMSetDepthStencilState(NULL, 0);
    context->PSSetSamplers(0, 1, (ID3D11SamplerState**)nullBuff);
    context->OMSetBlendState(NULL, bla, 0xFFFFFFFF);
    context->OMSetRenderTargets(1, (ID3D11RenderTargetView**)nullBuff, NULL);
    context->IASetVertexBuffers(0, 8, (ID3D11Buffer**)nullBuff, &zeroVal, &zeroVal);
    context->PSSetShaderResources(0, 8, (ID3D11ShaderResourceView**)nullBuff);
    context->IASetInputLayout(NULL);
    context->PSSetShader(NULL, NULL, 0);
    context->VSSetShader(NULL, NULL, 0);
    context->RSSetState(NULL);
    context->RSSetScissorRects(0, NULL);
}

LPVOID D3D10System::GetDevice()
{
    return (LPVOID)d3d;
}

LPVOID D3D10System::GetContext()
{
    return (LPVOID)context;
}


void D3D10System::Init()
{
    VBData *data = new VBData;
    data->UVList.SetSize(1);

    data->VertList.SetSize(4);
    data->UVList[0].SetSize(4);

    spriteVertexBuffer = CreateVertexBuffer(data, FALSE);

    //------------------------------------------------------------------

    data = new VBData;
    data->VertList.SetSize(5);
    boxVertexBuffer = CreateVertexBuffer(data, FALSE);

    //------------------------------------------------------------------

    GraphicsSystem::Init();
}


////////////////////////////
//Texture Functions
Texture* D3D10System::CreateTextureFromSharedHandle(unsigned int width, unsigned int height, HANDLE handle)
{
    return D3D10Texture::CreateFromSharedHandle(width, height, handle);
}

Texture* D3D10System::CreateSharedTexture(unsigned int width, unsigned int height)
{
    return D3D10Texture::CreateShared(width, height);
}

Texture* D3D10System::CreateTexture(unsigned int width, unsigned int height, GSColorFormat colorFormat, void *lpData, BOOL bBuildMipMaps, BOOL bStatic)
{
    return D3D10Texture::CreateTexture(width, height, colorFormat, lpData, bBuildMipMaps, bStatic);
}

Texture* D3D10System::CreateTextureFromFile(CTSTR lpFile, BOOL bBuildMipMaps)
{
    return D3D10Texture::CreateFromFile(lpFile, bBuildMipMaps);
}

Texture* D3D10System::CreateRenderTarget(unsigned int width, unsigned int height, GSColorFormat colorFormat, BOOL bGenMipMaps)
{
    return D3D10Texture::CreateRenderTarget(width, height, colorFormat, bGenMipMaps);
}

Texture* D3D10System::CreateGDITexture(unsigned int width, unsigned int height)
{
    return D3D10Texture::CreateGDITexture(width, height);
}

bool D3D10System::GetTextureFileInfo(CTSTR lpFile, TextureInfo &info)
{
    D3DX11_IMAGE_INFO ii;
    if(SUCCEEDED(D3DX11GetImageInfoFromFile(lpFile, NULL, &ii, NULL)))
    {
        info.width = ii.Width;
        info.height = ii.Height;
        switch(ii.Format)
        {
            case DXGI_FORMAT_A8_UNORM:              info.type = GS_ALPHA;       break;
            case DXGI_FORMAT_R8_UNORM:              info.type = GS_GRAYSCALE;   break;
            case DXGI_FORMAT_B8G8R8X8_UNORM:        info.type = GS_BGR;         break;
            case DXGI_FORMAT_B8G8R8A8_UNORM:        info.type = GS_BGRA;        break;
            case DXGI_FORMAT_R8G8B8A8_UNORM:        info.type = GS_RGBA;        break;
            case DXGI_FORMAT_R16G16B16A16_FLOAT:    info.type = GS_RGBA16F;     break;
            case DXGI_FORMAT_R32G32B32A32_FLOAT:    info.type = GS_RGBA32F;     break;
            case DXGI_FORMAT_BC1_UNORM:             info.type = GS_DXT1;        break;
            case DXGI_FORMAT_BC2_UNORM:             info.type = GS_DXT3;        break;
            case DXGI_FORMAT_BC3_UNORM:             info.type = GS_DXT5;        break;
            default:
                info.type = GS_UNKNOWNFORMAT;
        }

        return true;
    }

    return false;
}

SamplerState* D3D10System::CreateSamplerState(SamplerInfo &info)
{
    return D3D10SamplerState::CreateSamplerState(info);
}


UINT D3D10System::GetNumOutputs()
{
    UINT count = 0;

    IDXGIDevice *device;
    if(SUCCEEDED(d3d->QueryInterface(__uuidof(IDXGIDevice), (void**)&device)))
    {
        IDXGIAdapter *adapter;
        if(SUCCEEDED(device->GetAdapter(&adapter)))
        {
            IDXGIOutput *outputInterface;

            while(SUCCEEDED(adapter->EnumOutputs(count, &outputInterface)))
            {
                count++;
                outputInterface->Release();
            }

            adapter->Release();
        }

        device->Release();
    }

    return count;
}

OutputDuplicator *D3D10System::CreateOutputDuplicator(UINT outputID)
{
    D3D10OutputDuplicator *duplicator = new D3D10OutputDuplicator;
    if(duplicator->Init(outputID))
        return duplicator;

    delete duplicator;
    return NULL;
}


////////////////////////////
//Shader Functions
Shader* D3D10System::CreateVertexShaderFromBlob(ShaderBlob const &blob, CTSTR lpShader, CTSTR lpFileName)
{
    return D3D10VertexShader::CreateVertexShaderFromBlob(blob, lpShader, lpFileName);
}

Shader* D3D10System::CreatePixelShaderFromBlob(ShaderBlob const &blob, CTSTR lpShader, CTSTR lpFileName)
{
    return D3D10PixelShader::CreatePixelShaderFromBlob(blob, lpShader, lpFileName);
}

Shader* D3D10System::CreateVertexShader(CTSTR lpShader, CTSTR lpFileName)
{
    return D3D10VertexShader::CreateVertexShader(lpShader, lpFileName);
}

Shader* D3D10System::CreatePixelShader(CTSTR lpShader, CTSTR lpFileName)
{
    return D3D10PixelShader::CreatePixelShader(lpShader, lpFileName);
}

void D3D10System::CreateVertexShaderBlob(ShaderBlob &blob, CTSTR lpShader, CTSTR lpFileName)
{
    D3D10VertexShader::CreateVertexShaderBlob(blob, lpShader, lpFileName);
}

void D3D10System::CreatePixelShaderBlob(ShaderBlob &blob, CTSTR lpShader, CTSTR lpFileName)
{
    D3D10PixelShader::CreatePixelShaderBlob(blob, lpShader, lpFileName);
}


////////////////////////////
//Vertex Buffer Functions
VertexBuffer* D3D10System::CreateVertexBuffer(VBData *vbData, BOOL bStatic)
{
    return D3D10VertexBuffer::CreateVertexBuffer(vbData, bStatic);
}


////////////////////////////
//Main Rendering Functions
void D3D10System::LoadVertexBuffer(VertexBuffer* vb)
{
    if(vb != curVertexBuffer)
    {
        D3D10VertexBuffer *d3dVB = static_cast<D3D10VertexBuffer*>(vb);
        if(curVertexShader)
        {
            List<ID3D11Buffer*> buffers;
            List<UINT> strides;
            List<UINT> offsets;

            if(d3dVB)
                d3dVB->MakeBufferList(curVertexShader, buffers, strides);
            else
            {
                UINT nBuffersToClear = curVertexShader->NumBuffersExpected();
                buffers.SetSize(nBuffersToClear);
                strides.SetSize(nBuffersToClear);
            }

            offsets.SetSize(buffers.Num());
            context->IASetVertexBuffers(0, buffers.Num(), buffers.Array(), strides.Array(), offsets.Array());
        }

        curVertexBuffer = d3dVB;
    }
}

void D3D10System::LoadTexture(Texture *texture, UINT idTexture)
{
    if(curTextures[idTexture] != texture)
    {
        D3D10Texture *d3dTex = static_cast<D3D10Texture*>(texture);
        if(d3dTex)
            context->PSSetShaderResources(idTexture, 1, &d3dTex->resource);
        else
        {
            LPVOID lpNull = NULL;
            context->PSSetShaderResources(idTexture, 1, (ID3D11ShaderResourceView**)&lpNull);
        }

        curTextures[idTexture] = d3dTex;
    }
}

void D3D10System::LoadSamplerState(SamplerState *sampler, UINT idSampler)
{
    if(curSamplers[idSampler] != sampler)
    {
        D3D10SamplerState *d3dSampler = static_cast<D3D10SamplerState*>(sampler);
        if(d3dSampler)
            context->PSSetSamplers(idSampler, 1, &d3dSampler->state);
        else
        {
            LPVOID lpNull = NULL;
            context->PSSetSamplers(idSampler, 1, (ID3D11SamplerState**)&lpNull);
        }
        curSamplers[idSampler] = d3dSampler;
    }
}

void D3D10System::LoadVertexShader(Shader *vShader)
{
    if(curVertexShader != vShader)
    {
        if(vShader)
        {
            D3D10VertexBuffer *lastVertexBuffer = curVertexBuffer;
            if(curVertexBuffer)
                LoadVertexBuffer(NULL);

            D3D10VertexShader *shader = static_cast<D3D10VertexShader*>(vShader);

            context->VSSetShader(shader->vertexShader, NULL, 0);
            context->IASetInputLayout(shader->inputLayout);
            context->VSSetConstantBuffers(0, 1, &shader->constantBuffer);

            if(lastVertexBuffer)
                LoadVertexBuffer(lastVertexBuffer);
        }
        else
        {
            LPVOID lpNULL = NULL;

            context->VSSetShader(NULL, NULL, 0);
            context->VSSetConstantBuffers(0, 1, (ID3D11Buffer**)&lpNULL);
        }

        curVertexShader = static_cast<D3D10VertexShader*>(vShader);
    }
}

void D3D10System::LoadPixelShader(Shader *pShader)
{
    if(curPixelShader != pShader)
    {
        if(pShader)
        {
            D3D10PixelShader *shader = static_cast<D3D10PixelShader*>(pShader);

            context->PSSetShader(shader->pixelShader, NULL, 0);
            context->PSSetConstantBuffers(0, 1, &shader->constantBuffer);

            for(UINT i=0; i<shader->Samplers.Num(); i++)
                LoadSamplerState(shader->Samplers[i].sampler, i);
        }
        else
        {
            LPVOID lpNULL = NULL;

            context->PSSetShader(NULL, NULL, 0);
            context->PSSetConstantBuffers(0, 1, (ID3D11Buffer**)&lpNULL);

            for(UINT i=0; i<8; i++)
                curSamplers[i] = NULL;

            ID3D11SamplerState *states[8];
            zero(states, sizeof(states));
            context->PSSetSamplers(0, 8, states);
        }

        curPixelShader = static_cast<D3D10PixelShader*>(pShader);
    }
}

Shader* D3D10System::GetCurrentPixelShader()
{
    return curPixelShader;
}

Shader* D3D10System::GetCurrentVertexShader()
{
    return curVertexShader;
}

void D3D10System::SetRenderTarget(Texture *texture)
{
    if(curRenderTarget != texture)
    {
        if(texture)
        {
            ID3D11RenderTargetView *view = static_cast<D3D10Texture*>(texture)->renderTarget;
            if(!view)
            {
                AppWarning(TEXT("tried to set a texture that wasn't a render target as a render target"));
                return;
            }

            context->OMSetRenderTargets(1, &view, NULL);
        }
        else
            context->OMSetRenderTargets(1, &swapRenderView, NULL);

        curRenderTarget = static_cast<D3D10Texture*>(texture);
    }
}

const D3D11_PRIMITIVE_TOPOLOGY topologies[] = {D3D11_PRIMITIVE_TOPOLOGY_POINTLIST, D3D11_PRIMITIVE_TOPOLOGY_LINELIST, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP};

void D3D10System::Draw(GSDrawMode drawMode, DWORD startVert, DWORD nVerts)
{
    if(!curVertexBuffer)
    {
        AppWarning(TEXT("Tried to call draw without setting a vertex buffer"));
        return;
    }

    if(!curVertexShader)
    {
        AppWarning(TEXT("Tried to call draw without setting a vertex shader"));
        return;
    }

    if(!curPixelShader)
    {
        AppWarning(TEXT("Tried to call draw without setting a pixel shader"));
        return;
    }

    curVertexShader->SetMatrix(curVertexShader->GetViewProj(), curViewProjMatrix);

    curVertexShader->UpdateParams();
    curPixelShader->UpdateParams();

    D3D11_PRIMITIVE_TOPOLOGY newTopology = topologies[(int)drawMode];
    if(newTopology != curTopology)
    {
        context->IASetPrimitiveTopology(newTopology);
        curTopology = newTopology;
    }

    if(nVerts == 0)
        nVerts = static_cast<D3D10VertexBuffer*>(curVertexBuffer)->numVerts;

    context->Draw(nVerts, startVert);
}


////////////////////////////
//Drawing mode functions

const D3D11_BLEND blendConvert[] = {D3D11_BLEND_ZERO, D3D11_BLEND_ONE, D3D11_BLEND_SRC_COLOR, D3D11_BLEND_INV_SRC_COLOR, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_DEST_COLOR, D3D11_BLEND_INV_DEST_COLOR, D3D11_BLEND_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_BLEND_FACTOR, D3D11_BLEND_INV_BLEND_FACTOR};

void  D3D10System::EnableBlending(BOOL bEnable)
{
    if(bBlendingEnabled != bEnable)
    {
        if(bBlendingEnabled = bEnable)
            context->OMSetBlendState(curBlendState, curBlendFactor, 0xFFFFFFFF);
        else
            context->OMSetBlendState(disabledBlend, curBlendFactor, 0xFFFFFFFF);
    }
}

void D3D10System::BlendFunction(GSBlendType srcFactor, GSBlendType destFactor, float fFactor)
{
    bool bUseFactor = (srcFactor >= GS_BLEND_FACTOR || destFactor >= GS_BLEND_FACTOR);

    if(bUseFactor)
        curBlendFactor[0] = curBlendFactor[1] = curBlendFactor[2] = curBlendFactor[3] = fFactor;

    for(UINT i=0; i<blends.Num(); i++)
    {
        SavedBlendState &blendInfo = blends[i];
        if(blendInfo.srcFactor == srcFactor && blendInfo.destFactor == destFactor)
        {
            if(bUseFactor || curBlendState != blendInfo.blendState)
            {
                context->OMSetBlendState(blendInfo.blendState, curBlendFactor, 0xFFFFFFFF);
                curBlendState = blendInfo.blendState;
            }
            return;
        }
    }

    //blend wasn't found, create a new one and save it for later
    D3D11_BLEND_DESC blendDesc;
    zero(&blendDesc, sizeof(blendDesc));
    for(int i=0; i<8; i++)
    {
        blendDesc.RenderTarget[i].BlendEnable               = TRUE;
        blendDesc.RenderTarget[i].RenderTargetWriteMask     = D3D11_COLOR_WRITE_ENABLE_ALL;
        blendDesc.RenderTarget[i].BlendOpAlpha              = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[i].BlendOp                   = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[i].SrcBlendAlpha             = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[i].DestBlendAlpha            = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[i].SrcBlend                  = blendConvert[srcFactor];
        blendDesc.RenderTarget[i].DestBlend                 = blendConvert[destFactor];
    }

    SavedBlendState *savedBlend = blends.CreateNew();
    savedBlend->destFactor      = destFactor;
    savedBlend->srcFactor       = srcFactor;

    if(FAILED(d3d->CreateBlendState(&blendDesc, &savedBlend->blendState)))
        CrashError(TEXT("Could not set blend state"));

    if(bBlendingEnabled)
        context->OMSetBlendState(savedBlend->blendState, curBlendFactor, 0xFFFFFFFF);

    curBlendState = savedBlend->blendState;
}

void D3D10System::ClearColorBuffer(DWORD color)
{
    Color4 floatColor;
    floatColor.MakeFromRGBA(color);

    D3D10Texture *d3dTex = static_cast<D3D10Texture*>(curRenderTarget);
    if(d3dTex)
        context->ClearRenderTargetView(d3dTex->renderTarget, floatColor.ptr);
    else
        context->ClearRenderTargetView(swapRenderView, floatColor.ptr);
}


////////////////////////////
//Other Functions
void D3D10System::Ortho(float left, float right, float top, float bottom, float znear, float zfar)
{
    Matrix4x4Ortho(curProjMatrix, left, right, top, bottom, znear, zfar);
    ResetViewMatrix();
}

void D3D10System::Frustum(float left, float right, float top, float bottom, float znear, float zfar)
{
    Matrix4x4Frustum(curProjMatrix, left, right, top, bottom, znear, zfar);
    ResetViewMatrix();
}


void D3D10System::SetViewport(float x, float y, float width, float height)
{
    D3D11_VIEWPORT vp;
    zero(&vp, sizeof(vp));
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = FLOAT(x);
    vp.TopLeftY = FLOAT(y);
    vp.Width    = FLOAT(width);
    vp.Height   = FLOAT(height);
    context->RSSetViewports(1, &vp);
}

void D3D10System::SetScissorRect(XRect *pRect)
{
    if(pRect)
    {
        context->RSSetState(scissorState);
        D3D11_RECT rc = {pRect->x, pRect->y, pRect->x+pRect->cx, pRect->y+pRect->cy};
        context->RSSetScissorRects(1, &rc);
    }
    else
    {
        context->RSSetState(rasterizerState);
        context->RSSetScissorRects(0, NULL);
    }
}

//(jim) hey, I changed this to x, y, x2, y2
void D3D10System::SetCropping(float left, float top, float right, float bottom)
{
    curCropping[0] = left;
    curCropping[1] = top;
    curCropping[2] = right;
    curCropping[3] = bottom;
}

void D3D10System::DrawSpriteEx(Texture *texture, DWORD color, float x, float y, float x2, float y2, float u, float v, float u2, float v2)
{
    DrawSpriteExRotate(texture, color, x, y, x2, y2, 0.0f, u, v, u2, v2, 0.0f);
}

void D3D10System::DrawSpriteExRotate(Texture *texture, DWORD color, float x, float y, float x2, float y2, float degrees, float u, float v, float u2, float v2, float texDegrees)
{
    if(!curPixelShader)
        return; 

    if(!texture)
    {
        AppWarning(TEXT("Trying to draw a sprite with a NULL texture"));
        return;
    }

    HANDLE hColor = curPixelShader->GetParameterByName(TEXT("outputColor"));

    if(hColor)
        curPixelShader->SetColor(hColor, color);

    //------------------------------
    // crop positional values

    Vect2 totalSize = Vect2(x2-x, y2-y);
    Vect2 invMult   = Vect2(totalSize.x < 0.0f ? -1.0f : 1.0f, totalSize.y < 0.0f ? -1.0f : 1.0f);
    totalSize.Abs();

    if(y2-y < 0) {
        float tempFloat = curCropping[1];
        curCropping[1] = curCropping[3];
        curCropping[3] = tempFloat;
    }

    if(x2-x < 0) {
        float tempFloat = curCropping[0];
        curCropping[0] = curCropping[2];
        curCropping[2] = tempFloat;
    }

    bool bFlipX = (x2 - x) < 0.0f;
    bool bFlipY = (y2 - y) < 0.0f;

    x  += curCropping[0] * invMult.x;
    y  += curCropping[1] * invMult.y;
    x2 -= curCropping[2] * invMult.x;
    y2 -= curCropping[3] * invMult.y;

    bool cropXUnder = bFlipX ? ((x - x2) < 0.0f) : ((x2 - x) < 0.0f);
    bool cropYUnder = bFlipY ? ((y - y2) < 0.0f) : ((y2 - y) < 0.0f);

    // cropped out completely (eg mouse cursor texture)
    if (cropXUnder || cropYUnder)
        return;

    //------------------------------
    // crop texture coordinate values

    float cropMult[4];
    cropMult[0] = curCropping[0]/totalSize.x;
    cropMult[1] = curCropping[1]/totalSize.y;
    cropMult[2] = curCropping[2]/totalSize.x;
    cropMult[3] = curCropping[3]/totalSize.y;

    Vect2 totalUVSize = Vect2(u2-u, v2-v);
    u  += cropMult[0] * totalUVSize.x;
    v  += cropMult[1] * totalUVSize.y;
    u2 -= cropMult[2] * totalUVSize.x;
    v2 -= cropMult[3] * totalUVSize.y;

    //------------------------------
    // draw

    VBData *data = spriteVertexBuffer->GetData();
    data->VertList[0].Set(x,  y,  0.0f);
    data->VertList[1].Set(x,  y2, 0.0f);
    data->VertList[2].Set(x2, y,  0.0f);
    data->VertList[3].Set(x2, y2, 0.0f);

    if (!CloseFloat(degrees, 0.0f)) {
        List<Vect> &coords = data->VertList;

        Vect2 center(x+totalSize.x/2, y+totalSize.y/2);

        Matrix rotMatrix;
        rotMatrix.SetIdentity();
        rotMatrix.Rotate(AxisAngle(0.0f, 0.0f, 1.0f, RAD(degrees)));

        for (int i = 0; i < 4; i++) {
            Vect val = coords[i]-Vect(center);
            val.TransformVector(rotMatrix);
            coords[i] = val;
            coords[i] += Vect(center);
        }
    }

    List<UVCoord> &coords = data->UVList[0];
    coords[0].Set(u,  v);
    coords[1].Set(u,  v2);
    coords[2].Set(u2, v);
    coords[3].Set(u2, v2);

    if (!CloseFloat(texDegrees, 0.0f)) {
        Matrix rotMatrix;
        rotMatrix.SetIdentity();
        rotMatrix.Rotate(AxisAngle(0.0f, 0.0f, 1.0f, -RAD(texDegrees)));

        Vect2 minVal = Vect2(0.0f, 0.0f);
        for (int i = 0; i < 4; i++) {
            Vect val = Vect(coords[i]);
            val.TransformVector(rotMatrix);
            coords[i] = val;
            minVal.ClampMax(coords[i]);
        }

        for (int i = 0; i < 4; i++)
            coords[i] -= minVal;
    }

    spriteVertexBuffer->FlushBuffers();

    LoadVertexBuffer(spriteVertexBuffer);
    LoadTexture(texture);

    Draw(GS_TRIANGLESTRIP);
}

void D3D10System::DrawBox(const Vect2 &upperLeft, const Vect2 &size)
{
    VBData *data = boxVertexBuffer->GetData();

    Vect2 bottomRight = upperLeft+size;

    data->VertList[0] = upperLeft;
    data->VertList[1].Set(bottomRight.x, upperLeft.y);
    data->VertList[2].Set(bottomRight.x, bottomRight.y);
    data->VertList[3].Set(upperLeft.x, bottomRight.y);
    data->VertList[4] = upperLeft;

    boxVertexBuffer->FlushBuffers();

    LoadVertexBuffer(boxVertexBuffer);

    Draw(GS_LINESTRIP);
}

void D3D10System::ResetViewMatrix()
{
    Matrix4x4Convert(curViewMatrix, MatrixStack[curMatrix].GetTranspose());
    Matrix4x4Multiply(curViewProjMatrix, curViewMatrix, curProjMatrix);
    Matrix4x4Transpose(curViewProjMatrix, curViewProjMatrix);
}

void D3D10System::ResizeView()
{
    LPVOID nullVal = NULL;
    context->OMSetRenderTargets(1, (ID3D11RenderTargetView**)&nullVal, NULL);

    SafeRelease(swapRenderView);

    swap->ResizeBuffers(2, 0, 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0);

    ID3D11Texture2D *backBuffer = NULL;
    HRESULT err = swap->GetBuffer(0, IID_ID3D11Texture2D, (void**)&backBuffer);
    if(FAILED(err))
        CrashError(TEXT("Unable to get back buffer from swap chain"));

    err = d3d->CreateRenderTargetView(backBuffer, NULL, &swapRenderView);
    if(FAILED(err))
        CrashError(TEXT("Unable to get render view from back buffer"));

    backBuffer->Release();
}

void D3D10System::CopyTexture(Texture *texDest, Texture *texSrc)
{
    D3D10Texture *d3d10Dest = static_cast<D3D10Texture*>(texDest);
    D3D10Texture *d3d10Src  = static_cast<D3D10Texture*>(texSrc);

    context->CopyResource(d3d10Dest->texture, d3d10Src->texture);
}
