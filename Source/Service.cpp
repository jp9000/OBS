/********************************************************************************
 Copyright (C) 2014 Ruwen Hahn <palana@stunned.de>

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

#include <functional>

using namespace std;

static inline void LoadBuiltinServices(vector<ServiceIdentifier> &services_)
{
    XConfig serverData;
    if (serverData.Open(FormattedString(L"%s\\services.xconfig", API->GetAppPath())))
    {
        XElement *services = serverData.GetElement(TEXT("services"));
        if (services)
        {
            auto numServices = services->NumElements();

            for (decltype(numServices) i = 0; i < numServices; i++)
            {
                auto service = services->GetElementByID(i);
                if (!service)
                    continue;

                services_.emplace_back(service->GetInt(L"id"), String());
            }
        }
    }
}

static inline void LoadUserServices(vector<ServiceIdentifier> &services)
{
    OSFindData findData;
    String servicesDir = FormattedString(L"%s/services/", API->GetAppDataPath());
    if (HANDLE find = OSFindFirstFile(FormattedString(L"%s/*.xconfig", servicesDir.Array()), findData))
    {
        do
        {
            if (findData.bDirectory)
                continue;

            XConfig service(servicesDir + findData.fileName);
            if (!service.IsOpen())
                continue;

            services.emplace_back(0, findData.fileName);
        } while (OSFindNextFile(find, findData));

        OSFindClose(find);
    }
}

vector<ServiceIdentifier> LoadServiceIdentifiers()
{
    vector<ServiceIdentifier> services;

    LoadBuiltinServices(services);
    LoadUserServices(services);

    return services;
}

static inline bool EnumerateBuiltinServices(function<bool(ServiceIdentifier, XElement*)> &func)
{

    XConfig serverData;
    if (serverData.Open(FormattedString(L"%s\\services.xconfig", API->GetAppPath())))
    {
        XElement *services = serverData.GetElement(TEXT("services"));
        if (services)
        {
            auto numServices = services->NumElements();

            for (decltype(numServices) i = 0; i < numServices; i++)
            {
                auto service = services->GetElementByID(i);
                if (!service)
                    continue;

                if (!func({ service->GetInt(L"id"), String() }, service))
                    return false;
            }
        }
    }
    return true;
}

static inline void EnumerateUserServices(function<bool(ServiceIdentifier, XElement*)> &func)
{
    OSFindData findData;
    String servicesDir = FormattedString(L"%s/services/", API->GetAppDataPath());
    if (HANDLE find = OSFindFirstFile(FormattedString(L"%s/*.xconfig", servicesDir.Array()), findData))
    {
        do
        {
            if (findData.bDirectory)
                continue;

            XConfig service(servicesDir + findData.fileName);
            if (!service.IsOpen())
                continue;

            func({ 0, findData.fileName }, service.GetElementByID(0));
        } while (OSFindNextFile(find, findData));

        OSFindClose(find);
    }
}

void EnumerateServices(function<bool(ServiceIdentifier, XElement*)> func)
{
    if (!EnumerateBuiltinServices(func))
        return;

    EnumerateUserServices(func);
}

std::pair<std::unique_ptr<XConfig>, XElement*> LoadService(const ServiceIdentifier& sid, String *failReason)
{
    auto fail = [&](CTSTR reason)
    {
        if (failReason)
            *failReason = reason;
        return make_pair(std::unique_ptr<XConfig>(), nullptr);
    };

    auto serverData = make_unique<XConfig>();

    if (sid.file.IsEmpty())
    {
        if (!serverData->Open(FormattedString(L"%s\\services.xconfig", API->GetAppPath())))
            return fail(L"Could not open services.xconfig");

        XElement *services = serverData->GetElement(TEXT("services"));
        if (!services)
            return fail(L"Could not find any services in services.xconfig");

        XElement *service = NULL;
        auto numServices = services->NumElements();
        for (decltype(numServices) i = 0; i < numServices; i++)
        {
            XElement *curService = services->GetElementByID(i);
            if (!curService)
                continue;

            if (curService->GetInt(L"id") == sid.id)
                return { move(serverData), curService };
        }
    }
    else
    {
        if (serverData->Open(FormattedString(L"%s/services/%s", API->GetAppDataPath(), sid.file.Array())))
            return { move(serverData), serverData->GetElementByID(sid.id) };
        else
            return fail(FormattedString(L"Could not open service file '%s'", sid.file.Array()));
    }

    return make_pair(std::unique_ptr<XConfig>(), nullptr);
}

std::pair<std::unique_ptr<XConfig>, XElement*>  LoadService(String *failReason)
{
    return LoadService(GetCurrentService(), failReason);
}

ServiceIdentifier GetCurrentService()
{
    int serviceID = AppConfig->GetInt(L"Publish", L"Service", 0);
    String serviceFile = AppConfig->GetString(L"Publish", L"ServiceFile");
    return { serviceID, serviceFile };
}