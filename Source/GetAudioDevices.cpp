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


#include "Main.h"

#include <Mmdeviceapi.h>
#include <Audioclient.h>
#include <propsys.h>
#include <Functiondiscoverykeys_devpkey.h>


void GetAudioDevices(AudioDeviceList &deviceList, AudioDeviceType deviceType, bool bConnectedOnly, bool canDisable)
{
    const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
    const IID IID_IMMDeviceEnumerator    = __uuidof(IMMDeviceEnumerator);
    IMMDeviceEnumerator *mmEnumerator;
    HRESULT err;

    err = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&mmEnumerator);
    if(FAILED(err))
    {
        AppWarning(TEXT("GetAudioDevices: Could not create IMMDeviceEnumerator: 0x%08lx"), err);
        return;
    }

    //-------------------------------------------------------

    AudioDeviceInfo *info;

    if (canDisable) {
        info = deviceList.devices.CreateNew();
        info->strID = TEXT("Disable");
        info->strName = Str("Disable");
    }

    info = deviceList.devices.CreateNew();
    info->strID = TEXT("Default");
    info->strName = Str("Default");

    //-------------------------------------------------------

    IMMDeviceCollection *collection;

    EDataFlow audioDeviceType;
    switch(deviceType)
    {
    case ADT_RECORDING:
        audioDeviceType = eCapture;
        break;
    case ADT_PLAYBACK:
        audioDeviceType = eRender;
        break;
    default:
        audioDeviceType = eAll;
        break;
    }

    DWORD flags = DEVICE_STATE_ACTIVE;
    if (!bConnectedOnly)
        flags |= DEVICE_STATE_UNPLUGGED;

    err = mmEnumerator->EnumAudioEndpoints(audioDeviceType, flags, &collection);
    if(FAILED(err))
    {
        AppWarning(TEXT("GetAudioDevices: Could not enumerate audio endpoints"));
        SafeRelease(mmEnumerator);
        return;
    }

    UINT count;
    if(SUCCEEDED(collection->GetCount(&count)))
    {
        for(UINT i=0; i<count; i++)
        {
            IMMDevice *device;
            if(SUCCEEDED(collection->Item(i, &device)))
            {
                CWSTR wstrID;
                if(SUCCEEDED(device->GetId((LPWSTR*)&wstrID)))
                {
                    IPropertyStore *store;
                    if(SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store)))
                    {
                        PROPVARIANT varName;

                        PropVariantInit(&varName);
                        if(SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &varName)))
                        {
                            CWSTR wstrName = varName.pwszVal;

                            AudioDeviceInfo *info = deviceList.devices.CreateNew();
                            info->strID = wstrID;
                            info->strName = wstrName;
                        }
                    }

                    CoTaskMemFree((LPVOID)wstrID);
                }

                SafeRelease(device);
            }
        }
    }

    //-------------------------------------------------------

    SafeRelease(collection);
    SafeRelease(mmEnumerator);
}

bool GetDefaultDevice(String &strVal, EDataFlow df )
{
    const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
    const IID IID_IMMDeviceEnumerator    = __uuidof(IMMDeviceEnumerator);
    IMMDeviceEnumerator *mmEnumerator;
    HRESULT err;

    err = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&mmEnumerator);
    if(FAILED(err))
        return false;

    //-------------------------------------------------------

    IMMDevice *defDevice;
    if(FAILED(mmEnumerator->GetDefaultAudioEndpoint(df, eCommunications, &defDevice)))
    {
        SafeRelease(mmEnumerator);
        return false;
    }

    CWSTR wstrDefaultID;
    if(FAILED(defDevice->GetId((LPWSTR*)&wstrDefaultID)))
    {
        SafeRelease(defDevice);
        SafeRelease(mmEnumerator);
        return false;
    }

    strVal = wstrDefaultID;

    CoTaskMemFree((LPVOID)wstrDefaultID);
    SafeRelease(defDevice);
    SafeRelease(mmEnumerator);

    return true;
}

bool GetDefaultMicID(String &strVal) {
    return GetDefaultDevice(strVal, eCapture);
}

bool GetDefaultSpeakerID(String &strVal) {
    return GetDefaultDevice(strVal, eRender);
}
