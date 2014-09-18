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


#include "DShowPlugin.h"


#define FILTER_NAME     L"Capture Filter"
#define VIDEO_PIN_NAME  L"Video Capture"
#define AUDIO_PIN_NAME  L"Audio Capture"


//========================================================================================================

CapturePin::CapturePin(CaptureFilter* filterIn, DeviceSource *sourceIn, const GUID &expectedMajorType, const GUID &expectedMediaType)
: filter(filterIn), source(sourceIn), refCount(1)
{
    connectedMediaType.majortype = expectedMajorType;
    connectedMediaType.subtype   = GUID_NULL;
    connectedMediaType.pbFormat  = NULL;
    connectedMediaType.cbFormat  = 0;
    connectedMediaType.pUnk      = NULL;
    this->expectedMediaType = expectedMediaType;
    this->expectedMajorType = expectedMajorType;
}

CapturePin::~CapturePin()
{
    FreeMediaType(connectedMediaType);
}

STDMETHODIMP CapturePin::QueryInterface(REFIID riid, void **ppv)
{
    if(riid == IID_IUnknown || riid == IID_IPin)
    {
        AddRef();
        *ppv = (IPin*)this;
        return NOERROR;
    }
    else if(riid == IID_IMemInputPin)
    {
        AddRef();
        *ppv = (IMemInputPin*)this;
        return NOERROR;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
}

STDMETHODIMP_(ULONG) CapturePin::AddRef()  {return ++refCount;}
STDMETHODIMP_(ULONG) CapturePin::Release() {if(!InterlockedDecrement(&refCount)) {delete this; return 0;} return refCount;}

// IPin methods
STDMETHODIMP CapturePin::Connect(IPin *pReceivePin, const AM_MEDIA_TYPE *pmt)
{
    if(filter->state == State_Running)
        return VFW_E_NOT_STOPPED;
    if(connectedPin)
        return VFW_E_ALREADY_CONNECTED;
    if(!pmt)
        return S_OK;
    if(pmt->majortype != GUID_NULL && pmt->majortype != expectedMajorType)
        return S_FALSE;
    if(pmt->majortype == expectedMajorType && !IsValidMediaType(pmt))
        return S_FALSE;

    return S_OK;
}

STDMETHODIMP CapturePin::ReceiveConnection(IPin *pConnector, const AM_MEDIA_TYPE *pmt)
{
    if(filter->state != State_Stopped)
        return VFW_E_NOT_STOPPED;
    if(!pConnector || !pmt)
        return E_POINTER;
    if(connectedPin)
        return VFW_E_ALREADY_CONNECTED;

    if(QueryAccept(pmt) != S_OK)
        return VFW_E_TYPE_NOT_ACCEPTED;

    connectedPin = pConnector;
    connectedPin->AddRef();

    FreeMediaType(connectedMediaType);
    return CopyMediaType(&connectedMediaType, pmt);
}

STDMETHODIMP CapturePin::Disconnect()
{
    if(!connectedPin)
        return S_FALSE;

    connectedPin->Release();
    connectedPin = NULL;
    return S_OK;
}


STDMETHODIMP CapturePin::ConnectedTo(IPin **pPin)
{
    if(!connectedPin)
        return VFW_E_NOT_CONNECTED;

    connectedPin->AddRef();
    *pPin = connectedPin;
    return S_OK;
}

STDMETHODIMP CapturePin::ConnectionMediaType(AM_MEDIA_TYPE *pmt)
{
    if(!connectedPin)
        return VFW_E_NOT_CONNECTED;

    return CopyMediaType(pmt, &connectedMediaType);
}

STDMETHODIMP CapturePin::QueryPinInfo(PIN_INFO *pInfo)
{
    pInfo->pFilter = filter;
    if(filter) filter->AddRef();

    if(expectedMajorType == MEDIATYPE_Video)
        mcpy(pInfo->achName, VIDEO_PIN_NAME, sizeof(VIDEO_PIN_NAME));
    else
        mcpy(pInfo->achName, AUDIO_PIN_NAME, sizeof(AUDIO_PIN_NAME));

    pInfo->dir = PINDIR_INPUT;

    return NOERROR;
}

STDMETHODIMP CapturePin::QueryDirection(PIN_DIRECTION *pPinDir)    {*pPinDir = PINDIR_INPUT; return NOERROR;}
STDMETHODIMP CapturePin::QueryId(LPWSTR *lpId)                     {*lpId = L"Capture Pin"; return S_OK;}
STDMETHODIMP CapturePin::QueryAccept(const AM_MEDIA_TYPE *pmt)
{
    if(pmt->majortype != expectedMajorType)
        return S_FALSE;
    if(!IsValidMediaType(pmt))
        return S_FALSE;

    if(connectedPin)
    {
        FreeMediaType(connectedMediaType);
        CopyMediaType(&connectedMediaType, pmt);
    }

    return S_OK;
}

STDMETHODIMP CapturePin::EnumMediaTypes(IEnumMediaTypes **ppEnum)
{
    *ppEnum = new CaptureEnumMediaTypes(this, NULL);
    if(!*ppEnum)
        return E_OUTOFMEMORY;

    return NOERROR;
}

STDMETHODIMP CapturePin::QueryInternalConnections(IPin **apPin, ULONG *nPin)   {return E_NOTIMPL;}
STDMETHODIMP CapturePin::EndOfStream()                                         {return S_OK;}
STDMETHODIMP CapturePin::BeginFlush()
{
    source->FlushSamples();
    return S_OK;
}

STDMETHODIMP CapturePin::EndFlush()
{
    return S_OK;
}

STDMETHODIMP CapturePin::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate) {return S_OK;}

// IMemInputPin methods
STDMETHODIMP CapturePin::GetAllocator(IMemAllocator **ppAllocator)                     {return VFW_E_NO_ALLOCATOR;}
STDMETHODIMP CapturePin::NotifyAllocator(IMemAllocator *pAllocator, BOOL bReadOnly)    {return S_OK;}
STDMETHODIMP CapturePin::GetAllocatorRequirements(ALLOCATOR_PROPERTIES *pProps)        {return E_NOTIMPL;}

STDMETHODIMP CapturePin::Receive(IMediaSample *pSample)
{
    if(pSample)
    {
        if(expectedMajorType == MEDIATYPE_Video)
            source->ReceiveMediaSample(pSample, false);
        else if(expectedMajorType == MEDIATYPE_Audio)
            source->ReceiveMediaSample(pSample, true);
    }
    return S_OK;
}

STDMETHODIMP CapturePin::ReceiveMultiple(IMediaSample **pSamples, long nSamples, long *nSamplesProcessed)
{
    for(long i=0; i<nSamples; i++)
        Receive(pSamples[i]);
    *nSamplesProcessed = nSamples;
    return S_OK;
}

STDMETHODIMP CapturePin::ReceiveCanBlock() {return S_OK;}

bool CapturePin::IsValidMediaType(const AM_MEDIA_TYPE *pmt) const
{
    if(pmt->pbFormat)
    {
        if(pmt->subtype != expectedMediaType || pmt->majortype != expectedMajorType)
            return false;

        if(expectedMajorType == MEDIATYPE_Video)
        {
            BITMAPINFOHEADER *bih = GetVideoBMIHeader(pmt);

            if( bih->biHeight == 0 || bih->biWidth == 0)
                return false;
        }
    }

    return true;
}


//========================================================================================================

class CaptureFlags : public IAMFilterMiscFlags {
    long refCount = 1;
public:
    inline CaptureFlags() {}
    STDMETHODIMP_(ULONG) AddRef() {return ++refCount;}
    STDMETHODIMP_(ULONG) Release() {if (!--refCount) {delete this; return 0;} return refCount;}
    STDMETHODIMP         QueryInterface(REFIID riid, void **ppv)
    {
        if (riid == IID_IUnknown)
        {
            AddRef();
            *ppv = (void*)this;
            return NOERROR;
        }

        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) GetMiscFlags() {return AM_FILTER_MISC_FLAGS_IS_RENDERER;}
};

CaptureFilter::CaptureFilter(DeviceSource *source, const GUID &expectedMajorType, const GUID &expectedMediaType)
: state(State_Stopped), refCount(1)
{
    pin = new CapturePin(this, source, expectedMajorType, expectedMediaType);
    flags = new CaptureFlags();
}

CaptureFilter::~CaptureFilter()
{
    pin->Release();
    flags->Release();
}

// IUnknown methods
STDMETHODIMP CaptureFilter::QueryInterface(REFIID riid, void **ppv)
{
    if(riid == IID_IUnknown)
    {
        AddRef();
        *ppv = (IUnknown*)this;
        return NOERROR;
    }
    else if(riid == IID_IPersist)
    {
        AddRef();
        *ppv = (IPersist*)this;
        return NOERROR;
    }
    else if(riid == IID_IMediaFilter)
    {
        AddRef();
        *ppv = (IMediaFilter*)this;
        return NOERROR;
    }
    else if(riid == IID_IBaseFilter)
    {
        AddRef();
        *ppv = (IBaseFilter*)this;
        return NOERROR;
    }
    else if (riid == IID_IAMFilterMiscFlags)
    {
        flags->AddRef();
        *ppv = flags;
        return NOERROR;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
}

STDMETHODIMP_(ULONG) CaptureFilter::AddRef()  {return ++refCount;}
STDMETHODIMP_(ULONG) CaptureFilter::Release() {if(!InterlockedDecrement(&refCount)) {delete this; return 0;} return refCount;}

// IPersist method
STDMETHODIMP CaptureFilter::GetClassID(CLSID *pClsID) {return E_NOTIMPL;}

// IMediaFilter methods
STDMETHODIMP CaptureFilter::GetState(DWORD dwMSecs, FILTER_STATE *State)   {*State = state; return S_OK;}
STDMETHODIMP CaptureFilter::SetSyncSource(IReferenceClock *pClock)         {return S_OK;}
STDMETHODIMP CaptureFilter::GetSyncSource(IReferenceClock **pClock)        {*pClock = NULL; return NOERROR;}
STDMETHODIMP CaptureFilter::Stop()                                         {pin->EndFlush(); state = State_Stopped; return S_OK;}
STDMETHODIMP CaptureFilter::Pause()                                        {state = State_Paused;  return S_OK;}
STDMETHODIMP CaptureFilter::Run(REFERENCE_TIME tStart)                     {state = State_Running; return S_OK;}

// IBaseFilter methods
STDMETHODIMP CaptureFilter::EnumPins(IEnumPins **ppEnum)
{
    *ppEnum = new CaptureEnumPins(this, NULL);
    return (*ppEnum == NULL) ? E_OUTOFMEMORY : NOERROR;
}

STDMETHODIMP CaptureFilter::FindPin(LPCWSTR Id, IPin **ppPin) {return E_NOTIMPL;}
STDMETHODIMP CaptureFilter::QueryFilterInfo(FILTER_INFO *pInfo)
{
    mcpy(pInfo->achName, FILTER_NAME, sizeof(FILTER_NAME));

    pInfo->pGraph = graph;
    if(graph) graph->AddRef();
    return NOERROR;
}

STDMETHODIMP CaptureFilter::JoinFilterGraph(IFilterGraph *pGraph, LPCWSTR pName)   {graph = pGraph; return NOERROR;}
STDMETHODIMP CaptureFilter::QueryVendorInfo(LPWSTR *pVendorInfo)                   {return E_NOTIMPL;}


//========================================================================================================

CaptureEnumPins::CaptureEnumPins(CaptureFilter *filterIn, CaptureEnumPins *pEnum)
: filter(filterIn), refCount(1)
{
    filter->AddRef();
    curPin = (pEnum != NULL) ? pEnum->curPin : 0;
}

CaptureEnumPins::~CaptureEnumPins()
{
    filter->Release();
}

// IUnknown
STDMETHODIMP CaptureEnumPins::QueryInterface(REFIID riid, void **ppv)
{
    if(riid == IID_IUnknown || riid == IID_IEnumPins)
    {
        AddRef();
        *ppv = (IEnumPins*)this;
        return NOERROR;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
}

STDMETHODIMP_(ULONG) CaptureEnumPins::AddRef()  {return ++refCount;}
STDMETHODIMP_(ULONG) CaptureEnumPins::Release() {if(!InterlockedDecrement(&refCount)) {delete this; return 0;} return refCount;}

// IEnumPins
STDMETHODIMP CaptureEnumPins::Next(ULONG cPins, IPin **ppPins, ULONG *pcFetched)
{
    UINT nFetched = 0;

    if(curPin == 0 && cPins > 0)
    {
        IPin *pPin = filter->GetCapturePin();
        *ppPins = pPin;
        pPin->AddRef();
        nFetched = 1;
        curPin++;
    }

    if(pcFetched) *pcFetched = nFetched;

    return (nFetched == cPins) ? S_OK : S_FALSE;
}

STDMETHODIMP CaptureEnumPins::Skip(ULONG cPins)     {return (++curPin > 1) ? S_FALSE : S_OK;}
STDMETHODIMP CaptureEnumPins::Reset()               {curPin = 0; return S_OK;}
STDMETHODIMP CaptureEnumPins::Clone(IEnumPins **ppEnum)
{
    *ppEnum = new CaptureEnumPins(filter, this);
    return (*ppEnum == NULL) ? E_OUTOFMEMORY : NOERROR;
}


//========================================================================================================

CaptureEnumMediaTypes::CaptureEnumMediaTypes(CapturePin *pinIn, CaptureEnumMediaTypes *pEnum)
: pin(pinIn), refCount(1)
{
    pin->AddRef();
}

CaptureEnumMediaTypes::~CaptureEnumMediaTypes()
{
    pin->Release();
}

STDMETHODIMP CaptureEnumMediaTypes::QueryInterface(REFIID riid, void **ppv)
{
    if(riid == IID_IUnknown || riid == IID_IEnumMediaTypes)
    {
        AddRef();
        *ppv = (IEnumMediaTypes*)this;
        return NOERROR;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
}

STDMETHODIMP_(ULONG) CaptureEnumMediaTypes::AddRef()  {return ++refCount;}
STDMETHODIMP_(ULONG) CaptureEnumMediaTypes::Release() {if(!InterlockedDecrement(&refCount)) {delete this; return 0;} return refCount;}

// IEnumMediaTypes
STDMETHODIMP CaptureEnumMediaTypes::Next(ULONG cMediaTypes, AM_MEDIA_TYPE **ppMediaTypes, ULONG *pcFetched) {return S_FALSE;}
STDMETHODIMP CaptureEnumMediaTypes::Skip(ULONG cMediaTypes)                                                 {return S_FALSE;}
STDMETHODIMP CaptureEnumMediaTypes::Reset()                                                                 {return S_OK;}
STDMETHODIMP CaptureEnumMediaTypes::Clone(IEnumMediaTypes **ppEnum)
{
    *ppEnum = new CaptureEnumMediaTypes(pin, this);
    return (*ppEnum == NULL) ? E_OUTOFMEMORY : NOERROR;
}
