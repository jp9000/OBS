/********************************************************************************
Copyright (C) 2015 Richard Stanway <r1ch@obsproject.com>

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

bool InstallUserService(TCHAR *path)
{
    XConfig serverData;
    if (serverData.Open(path))
    {
        XElement *services = serverData.GetRootElement();
        if (services)
        {
            auto numServices = services->NumElements();

            if (numServices != 1)
                return false;

            auto service = services->GetElementByID(0);
            if (!service)
                return false;

            CTSTR name = service->GetName();

            auto servers = service->GetElement(TEXT("servers"));
            if (!servers || !servers->NumDataItems())
                return false;

            String confirmMessage = FormattedString(Str("InstallServiceConfirm"), name);
            confirmMessage.FindReplace(TEXT("$1"), name);

            String uninstallMessage = FormattedString(Str("InstallServiceAlreadyInstalled"), name);
            uninstallMessage.FindReplace(TEXT("$1"), name);

            serverData.Close();

            CTSTR serviceFileName = srchr(path, '\\');
            if (serviceFileName)
                serviceFileName++;
            else
                return false;

            String baseFilename = serviceFileName;
            baseFilename.FindReplace(TEXT(".obs-service"), TEXT(""));

            String destination = FormattedString(TEXT("%s\\services\\%s.xconfig"), lpAppDataPath, baseFilename.Array());

            if (OSFileExists(destination.Array()))
            {
                if (OBSMessageBox(NULL, uninstallMessage.Array(), L"Open Broadcaster Software", MB_ICONQUESTION | MB_YESNO) == IDYES)
                    OSDeleteFile(destination.Array());
                return true;
            }

            if (OBSMessageBox(NULL, confirmMessage.Array(), L"Open Broadcaster Software", MB_ICONQUESTION | MB_YESNO) == IDYES)
            {
                if (!OSCopyFile(destination.Array(), path))
                {
                    OBSMessageBox(NULL, FormattedString(TEXT("Failed to copy service file: error %d"), GetLastError()), L"Open Broadcaster Software", MB_ICONERROR);
                    return false;
                }
                else
                {
                    OBSMessageBox(NULL, Str("InstallServiceInstalled"), L"Open Broadcaster Software", MB_ICONINFORMATION);
                    return true;
                }
            }
            else
                return false;
        }
        else
            return false;
    }
    return false;
}

void RegisterServiceFileHandler(void)
{
    // the Win32 API seems to be lacking a nice way to register file extensions
    // so we just try to register it every time.

    HKEY hk, newKey;

    if (RegOpenCurrentUser(KEY_WRITE, &hk) != ERROR_SUCCESS)
        return;

    if (RegCreateKeyEx(hk, L"Software\\Classes\\.obs-service", 0, NULL, 0, KEY_WRITE, NULL, &newKey, NULL) != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return;
    }

    RegSetValue(newKey, NULL, REG_SZ, L"OpenBroadcasterSoftware", 0);
    RegSetValue(newKey, L"Content Type", REG_SZ, L"application/x-obs-service", 0);
    RegCloseKey(newKey);

    if (RegCreateKeyEx(hk, L"Software\\Classes\\OpenBroadcasterSoftware\\Content Type", 0, NULL, 0, KEY_WRITE, NULL, &newKey, NULL) != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return;
    }

    RegSetValue(newKey, NULL, REG_SZ, L"application/x-obs-service", 0);
    RegCloseKey(newKey);

    if (RegCreateKeyEx(hk, L"Software\\Classes\\OpenBroadcasterSoftware\\shell", 0, NULL, 0, KEY_WRITE, NULL, &newKey, NULL) != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return;
    }

    RegSetValue(newKey, NULL, REG_SZ, L"install", 0);
    RegCloseKey(newKey);

    if (RegCreateKeyEx(hk, L"Software\\Classes\\OpenBroadcasterSoftware\\shell\\install", 0, NULL, 0, KEY_WRITE, NULL, &newKey, NULL) != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return;
    }

    RegSetValue(newKey, NULL, REG_SZ, L"Install Service", 0);
    RegCloseKey(newKey);

    if (RegCreateKeyEx(hk, L"Software\\Classes\\OpenBroadcasterSoftware\\shell\\install\\command", 0, NULL, 0, KEY_WRITE, NULL, &newKey, NULL) != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return;
    }

    String modulePath;
    modulePath.SetLength(MAX_PATH);

    if (GetModuleFileName(NULL, modulePath, modulePath.Length() - 1))
        RegSetValue(newKey, NULL, REG_SZ, FormattedString(TEXT("\"%s\" -installservice \"%%1\""), modulePath.Array()), 0);

    RegCloseKey(newKey);
    RegCloseKey(hk);
}
