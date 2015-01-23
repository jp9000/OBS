/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/


#pragma once


class D3D10VertexShader;



inline GSColorFormat ConvertGIBackBufferFormat(DXGI_FORMAT format)
{
    switch(format)
    {
    case DXGI_FORMAT_R10G10B10A2_UNORM: return GS_R10G10B10A2;
    case DXGI_FORMAT_R8G8B8A8_UNORM:    return GS_RGBA;
    case DXGI_FORMAT_B8G8R8A8_UNORM:    return GS_BGRA;
    case DXGI_FORMAT_B8G8R8X8_UNORM:    return GS_BGR;
    case DXGI_FORMAT_B5G5R5A1_UNORM:    return GS_B5G5R5A1;
    case DXGI_FORMAT_B5G6R5_UNORM:      return GS_B5G6R5;
    }

    return GS_UNKNOWNFORMAT;
}


//=============================================================================

class D3D10VertexBuffer : public VertexBuffer
{
    friend class D3D10System;
    friend class OBS;

    ID3D10Buffer *vertexBuffer;
    ID3D10Buffer *normalBuffer;
    ID3D10Buffer *colorBuffer;
    ID3D10Buffer *tangentBuffer;
    List<ID3D10Buffer*> UVBuffers;

    UINT vertexSize;
    UINT normalSize;
    UINT colorSize;
    UINT tangentSize;
    List<UINT> UVSizes;

    BOOL bDynamic;
    UINT numVerts;
    VBData *data;

    static VertexBuffer* CreateVertexBuffer(VBData *vbData, BOOL bStatic);

    void MakeBufferList(D3D10VertexShader *vShader, List<ID3D10Buffer*> &bufferList, List<UINT> &strides) const;

public:
    ~D3D10VertexBuffer();

    virtual void FlushBuffers();
    virtual VBData* GetData();
};

//=============================================================================

class D3D10SamplerState : public SamplerState
{
    friend class D3D10System;
    friend class OBS;

    ID3D10SamplerState *state;

    static SamplerState* CreateSamplerState(SamplerInfo &info);

public:
    ~D3D10SamplerState();
};

//--------------------------------------------------

class D3D10Texture : public Texture
{
    friend class D3D10OutputDuplicator;
    friend class D3D10System;
    friend class OBS;

    ID3D10Texture2D          *texture;
    ID3D10ShaderResourceView *resource;
    ID3D10RenderTargetView   *renderTarget;

    UINT width, height;
    GSColorFormat format;
    IDXGISurface1 *surface;
    bool bGDICompatible;
    bool bDynamic;

    static Texture* CreateFromSharedHandle(unsigned int width, unsigned int height, HANDLE handle);
    static Texture* CreateTexture(unsigned int width, unsigned int height, GSColorFormat colorFormat, void *lpData, BOOL bGenMipMaps, BOOL bStatic);
    static Texture* CreateFromFile(CTSTR lpFile, BOOL bBuildMipMaps);
    static Texture* CreateRenderTarget(unsigned int width, unsigned int height, GSColorFormat colorFormat, BOOL bGenMipMaps);
    static Texture* CreateGDITexture(unsigned int width, unsigned int height);
    static Texture* CreateShared(unsigned int width, unsigned int height);

public:
    ~D3D10Texture();

    virtual DWORD Width() const;
    virtual DWORD Height() const;
    virtual BOOL HasAlpha() const;
    virtual void SetImage(void *lpData, GSImageFormat imageFormat, UINT pitch);
    virtual bool Map(BYTE *&lpData, UINT &pitch);
    virtual void Unmap();
    virtual GSColorFormat GetFormat() const;

    virtual bool GetDC(HDC &hDC);
    virtual void ReleaseDC();

    LPVOID GetD3DTexture() {return texture;}
    virtual HANDLE GetSharedHandle();
};

//=============================================================================

struct ShaderParam
{
    ShaderParameterType type;
    String name;

    UINT samplerID;
    UINT textureID;

    int arrayCount;

    List<BYTE> curValue;
    List<BYTE> defaultValue;
    BOOL bChanged;

    inline ~ShaderParam() {FreeData();}

    inline void FreeData()
    {
        name.Clear();
        curValue.Clear();
        defaultValue.Clear();
    }
};

struct ShaderSampler
{
    String name;
    SamplerState *sampler;

    inline ~ShaderSampler() {FreeData();}

    inline void FreeData()
    {
        name.Clear();
        delete sampler;
    }
};

//--------------------------------------------------

struct ShaderProcessor : CodeTokenizer
{
    BOOL ProcessShader(CTSTR input, CTSTR filename);
    BOOL AddState(SamplerInfo &info, String &stateName, String &stateVal);

    UINT nTextures;
    List<ShaderSampler> Samplers;
    List<ShaderParam>   Params;

    List<D3D10_INPUT_ELEMENT_DESC> generatedLayout;

    bool bHasNormals;
    bool bHasColors;
    bool bHasTangents;
    UINT numTextureCoords;

    inline ShaderProcessor()  {zero(this, sizeof(ShaderProcessor));}
    inline ~ShaderProcessor() {FreeData();}

    inline void FreeData()
    {
        UINT i;
        for(i=0; i<Samplers.Num(); i++)
            Samplers[i].FreeData();
        Samplers.Clear();
        for(i=0; i<Params.Num(); i++)
            Params[i].FreeData();
        Params.Clear();
    }

    inline UINT GetSamplerID(CTSTR lpSampler)
    {
        for(UINT i=0; i<Samplers.Num(); i++)
        {
            if(Samplers[i].name.Compare(lpSampler))
                return i;
        }

        return INVALID;
    }
};

//--------------------------------------------------

class D3D10Shader : public Shader
{
    friend class D3D10System;
    friend class OBS;

    List<ShaderParam>   Params;
    List<ShaderSampler> Samplers;

    ID3D10Buffer *constantBuffer;
    UINT constantSize;

protected:
    bool ProcessData(ShaderProcessor &processor, CTSTR lpFileName);

    void UpdateParams();

public:
    ~D3D10Shader();

    virtual int    NumParams() const;
    virtual HANDLE GetParameter(UINT parameter) const;
    virtual HANDLE GetParameterByName(CTSTR lpName) const;
    virtual void   GetParameterInfo(HANDLE hObject, ShaderParameterInfo &paramInfo) const;

    virtual void   LoadDefaults();

    virtual void   SetBool(HANDLE hObject, BOOL bValue);
    virtual void   SetFloat(HANDLE hObject, float fValue);
    virtual void   SetInt(HANDLE hObject, int iValue);
    virtual void   SetMatrix(HANDLE hObject, float *matrix);
    virtual void   SetVector(HANDLE hObject, const Vect &value);
    virtual void   SetVector2(HANDLE hObject, const Vect2 &value);
    virtual void   SetVector4(HANDLE hObject, const Vect4 &value);
    virtual void   SetTexture(HANDLE hObject, BaseTexture *texture);
    virtual void   SetValue(HANDLE hObject, const void *val, DWORD dwSize);
};

//--------------------------------------------------

class D3D10VertexShader : public D3D10Shader
{
    friend class D3D10System;
    friend class D3D10VertexBuffer;
    friend class OBS;

    ID3D10VertexShader *vertexShader;
    ID3D10InputLayout  *inputLayout;

    bool bHasNormals;
    bool bHasColors;
    bool bHasTangents;
    UINT nTextureCoords;

    inline UINT NumBuffersExpected() const
    {
        UINT count = 1;
        if(bHasNormals)  count++;
        if(bHasColors)   count++;
        if(bHasTangents) count++;
        count += nTextureCoords;

        return count;
    }

    static Shader* CreateVertexShaderFromBlob(std::vector<char> const &blob, CTSTR lpShader, CTSTR lpFileName);
    static Shader* CreateVertexShader(CTSTR lpShader, CTSTR lpFileName);
    static void CreateVertexShaderBlob(std::vector<char> &blob, CTSTR lpShader, CTSTR lpFileName);

public:
    ~D3D10VertexShader();

    virtual ShaderType GetType() const {return ShaderType_Vertex;}
};

//--------------------------------------------------

class D3D10PixelShader : public D3D10Shader
{
    friend class D3D10System;

    ID3D10PixelShader *pixelShader;

    static Shader* CreatePixelShaderFromBlob(ShaderBlob const &blob, CTSTR lpShader, CTSTR lpFileName);
    static Shader* CreatePixelShader(CTSTR lpShader, CTSTR lpFileName);
    static void CreatePixelShaderBlob(ShaderBlob &blob, CTSTR lpShader, CTSTR lpFileName);

public:
    ~D3D10PixelShader();

    virtual ShaderType GetType() const {return ShaderType_Pixel;}
};

//--------------------------------------------------

class D3D10OutputDuplicator : public OutputDuplicator
{
    IDXGIOutputDuplication *duplicator;
    Texture *copyTex;

    POINT cursorPos;
    Texture *cursorTex;
    BOOL bCursorVis;

public:
    bool Init(UINT output);
    virtual ~D3D10OutputDuplicator();

    virtual DuplicatorInfo AcquireNextFrame(UINT timeout);
    virtual Texture* GetCopyTexture();
    virtual Texture* GetCursorTex(POINT* pos);
};


//=============================================================================

struct SavedBlendState
{
    GSBlendType srcFactor, destFactor;
    ID3D10BlendState *blendState;
};

class D3D10System : public GraphicsSystem
{
    friend class OBS;
    friend class D3D10VertexShader;
    friend class D3D10PixelShader;

    IDXGIFactory1           *factory;
    ID3D10Device1           *d3d;
    IDXGISwapChain          *swap;
    ID3D10RenderTargetView  *swapRenderView;

    ID3D10DepthStencilState *depthState;
    ID3D10RasterizerState   *rasterizerState;
    ID3D10RasterizerState   *scissorState;

    bool bDisableCompatibilityMode;

    //---------------------------

    D3D10Texture            *curRenderTarget;
    D3D10Texture            *curTextures[8];
    D3D10SamplerState       *curSamplers[8];
    D3D10VertexBuffer       *curVertexBuffer;
    D3D10VertexShader       *curVertexShader;
    D3D10PixelShader        *curPixelShader;

    D3D10_PRIMITIVE_TOPOLOGY curTopology;

    List<SavedBlendState>   blends;
    ID3D10BlendState        *curBlendState;
    ID3D10BlendState        *disabledBlend;
    BOOL                    bBlendingEnabled;

    //---------------------------

    VertexBuffer            *spriteVertexBuffer, *boxVertexBuffer;

    //---------------------------

    float                   curProjMatrix[16];
    float                   curViewMatrix[16];
    float                   curViewProjMatrix[16];

    float                   curBlendFactor[4];

    float                   curCropping[4];

    virtual void ResetViewMatrix();

    virtual void ResizeView();
    virtual void UnloadAllData();

    virtual void CreateVertexShaderBlob(std::vector<char> &blob, CTSTR lpShader, CTSTR lpFileName) override;
    virtual void CreatePixelShaderBlob(std::vector<char> &blob, CTSTR lpShader, CTSTR lpFileName) override;

public:
    D3D10System();
    ~D3D10System();

    virtual LPVOID GetDevice();

    virtual void Init();


    ////////////////////////////
    //Texture Functions
    virtual Texture*        CreateTextureFromSharedHandle(unsigned int width, unsigned int height, HANDLE handle);
    virtual Texture*        CreateTexture(unsigned int width, unsigned int height, GSColorFormat colorFormat, void *lpData, BOOL bBuildMipMaps, BOOL bStatic);
    virtual Texture*        CreateTextureFromFile(CTSTR lpFile, BOOL bBuildMipMaps);
    virtual Texture*        CreateRenderTarget(unsigned int width, unsigned int height, GSColorFormat colorFormat, BOOL bGenMipMaps);
    virtual Texture*        CreateGDITexture(unsigned int width, unsigned int height);

    virtual bool            GetTextureFileInfo(CTSTR lpFile, TextureInfo &info);

    virtual SamplerState*   CreateSamplerState(SamplerInfo &info);

    virtual UINT            GetNumOutputs();
    virtual OutputDuplicator *CreateOutputDuplicator(UINT outputID);


    ////////////////////////////
    //Shader Functions
    virtual Shader*         CreateVertexShaderFromBlob(ShaderBlob const &blob, CTSTR lpShader, CTSTR lpFileName) override;
    virtual Shader*         CreatePixelShaderFromBlob(ShaderBlob const &blob, CTSTR lpShader, CTSTR lpFileName) override;
    virtual Shader*         CreateVertexShader(CTSTR lpShader, CTSTR lpFileName);
    virtual Shader*         CreatePixelShader(CTSTR lpShader, CTSTR lpFileName);


    ////////////////////////////
    //Vertex Buffer Functions
    virtual VertexBuffer* CreateVertexBuffer(VBData *vbData, BOOL bStatic=1);


    ////////////////////////////
    //Main Rendering Functions
    virtual void  LoadVertexBuffer(VertexBuffer* vb);
    virtual void  LoadTexture(Texture *texture, UINT idTexture=0);
    virtual void  LoadSamplerState(SamplerState *sampler, UINT idSampler=0);
    virtual void  LoadVertexShader(Shader *vShader);
    virtual void  LoadPixelShader(Shader *pShader);

    virtual Shader* GetCurrentPixelShader();
    virtual Shader* GetCurrentVertexShader();

    virtual void  SetRenderTarget(Texture *texture);
    virtual void  Draw(GSDrawMode drawMode, DWORD startVert=0, DWORD nVerts=0);


    ////////////////////////////
    //Drawing mode functions
    virtual void  EnableBlending(BOOL bEnable);
    virtual void  BlendFunction(GSBlendType srcFactor, GSBlendType destFactor, float fFactor);

    virtual void  ClearColorBuffer(DWORD color=0xFF000000);


    ////////////////////////////
    //Other Functions
    void  Ortho(float left, float right, float top, float bottom, float znear, float zfar);
    void  Frustum(float left, float right, float top, float bottom, float znear, float zfar);

    virtual void  SetViewport(float x, float y, float width, float height);

    virtual void  SetScissorRect(XRect *pRect=NULL);


    virtual void  DrawSpriteEx(Texture *texture, DWORD color, float x, float y, float x2, float y2, float u, float v, float u2, float v2);
    virtual void  DrawBox(const Vect2 &upperLeft, const Vect2 &size);
    virtual void  SetCropping(float left, float top, float right, float bottom);

    virtual void  CopyTexture(Texture *texDest, Texture *texSrc);
    virtual void  DrawSpriteExRotate(Texture *texture, DWORD color, float x, float y, float x2, float y2, float degrees, float u, float v, float u2, float v2, float texDegrees);

    // To prevent breaking the API, put this at the end instead of with the other Texture functions
    virtual Texture*        CreateSharedTexture(unsigned int width, unsigned int height);
};

inline ID3D10Device*        GetD3D()        {return static_cast<ID3D10Device*>(GS->GetDevice());}
