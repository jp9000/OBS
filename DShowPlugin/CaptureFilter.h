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


//have to give major credit to VLC here which was very helpful in learning how to capture stuff from directshow.
//the capture filter is pretty much the same but directed toward just video because we do our audio elsewhere


class CaptureFilter;
class DeviceSource;


class CapturePin : public IPin, public IMemInputPin
{
    friend class CaptureEnumMediaTypes;

    long refCount;

    GUID expectedMajorType;
    GUID expectedMediaType;
    DeviceSource *source;
    CaptureFilter *filter;
    AM_MEDIA_TYPE connectedMediaType;
    IPin *connectedPin;

    bool IsValidMediaType(const AM_MEDIA_TYPE *pmt) const;

public:
    CapturePin(CaptureFilter *filter, DeviceSource *source, const GUID &expectedMajorType, const GUID &expectedMediaType);
    virtual ~CapturePin();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IPin methods
    STDMETHODIMP Connect(IPin *pReceivePin, const AM_MEDIA_TYPE *pmt);
    STDMETHODIMP ReceiveConnection(IPin *connector, const AM_MEDIA_TYPE *pmt);
    STDMETHODIMP Disconnect();
    STDMETHODIMP ConnectedTo(IPin **pPin);
    STDMETHODIMP ConnectionMediaType(AM_MEDIA_TYPE *pmt);
    STDMETHODIMP QueryPinInfo(PIN_INFO *pInfo);
    STDMETHODIMP QueryDirection(PIN_DIRECTION *pPinDir);
    STDMETHODIMP QueryId(LPWSTR *lpId);
    STDMETHODIMP QueryAccept(const AM_MEDIA_TYPE *pmt);
    STDMETHODIMP EnumMediaTypes(IEnumMediaTypes **ppEnum);
    STDMETHODIMP QueryInternalConnections(IPin* *apPin, ULONG *nPin);
    STDMETHODIMP EndOfStream();

    STDMETHODIMP BeginFlush();
    STDMETHODIMP EndFlush();
    STDMETHODIMP NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);

    // IMemInputPin methods
    STDMETHODIMP GetAllocator(IMemAllocator **ppAllocator);
    STDMETHODIMP NotifyAllocator(IMemAllocator *pAllocator, BOOL bReadOnly);
    STDMETHODIMP GetAllocatorRequirements(ALLOCATOR_PROPERTIES *pProps);
    STDMETHODIMP Receive(IMediaSample *pSample);
    STDMETHODIMP ReceiveMultiple(IMediaSample **pSamples, long nSamples, long *nSamplesProcessed);
    STDMETHODIMP ReceiveCanBlock();
};

class CaptureFilter : public IBaseFilter
{
    friend class CapturePin;

    long refCount;

    FILTER_STATE state;
    IFilterGraph *graph;
    CapturePin *pin;
    IAMFilterMiscFlags *flags;

public:
    CaptureFilter(DeviceSource *source, const GUID &expectedMajorType, const GUID &expectedMediaType);
    virtual ~CaptureFilter();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IPersist method
    STDMETHODIMP GetClassID(CLSID *pClsID);

    // IMediaFilter methods
    STDMETHODIMP GetState(DWORD dwMSecs, FILTER_STATE *State);
    STDMETHODIMP SetSyncSource(IReferenceClock *pClock);
    STDMETHODIMP GetSyncSource(IReferenceClock **pClock);
    STDMETHODIMP Stop();
    STDMETHODIMP Pause();
    STDMETHODIMP Run(REFERENCE_TIME tStart);

    // IBaseFilter methods
    STDMETHODIMP EnumPins(IEnumPins ** ppEnum);
    STDMETHODIMP FindPin(LPCWSTR Id, IPin **ppPin);
    STDMETHODIMP QueryFilterInfo(FILTER_INFO *pInfo);
    STDMETHODIMP JoinFilterGraph(IFilterGraph *pGraph, LPCWSTR pName);
    STDMETHODIMP QueryVendorInfo(LPWSTR *pVendorInfo);

    inline CapturePin* GetCapturePin() const {return pin;}
};

class CaptureEnumPins : public IEnumPins
{
    long refCount;

    CaptureFilter *filter;
    UINT curPin;

public:
    CaptureEnumPins(CaptureFilter *filterIn, CaptureEnumPins *pEnum);
    virtual ~CaptureEnumPins();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IEnumPins
    STDMETHODIMP Next(ULONG cPins, IPin **ppPins, ULONG *pcFetched);
    STDMETHODIMP Skip(ULONG cPins);
    STDMETHODIMP Reset();
    STDMETHODIMP Clone(IEnumPins **ppEnum);
};

class CaptureEnumMediaTypes : public IEnumMediaTypes
{
    long refCount;

    CapturePin *pin;

public:
    CaptureEnumMediaTypes(CapturePin *pinIn, CaptureEnumMediaTypes *pEnum);
    virtual ~CaptureEnumMediaTypes();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IEnumMediaTypes
    STDMETHODIMP Next(ULONG cMediaTypes, AM_MEDIA_TYPE **ppMediaTypes, ULONG * pcFetched);
    STDMETHODIMP Skip(ULONG cMediaTypes);
    STDMETHODIMP Reset();
    STDMETHODIMP Clone(IEnumMediaTypes **ppEnum);
};
