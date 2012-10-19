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


#include "GraphicsCapture.h"

BOOL WINAPI InjectLibrary(HANDLE hProcess, CTSTR lpDLL)
{
    UPARAM procAddress;
    DWORD dwTemp,dwSize;
    LPVOID lpStr;
    BOOL bWorks,bRet=0;
    HANDLE hThread;
    SIZE_T writtenSize;

    if(!hProcess) return 0;

    dwSize = ssize((TCHAR*)lpDLL);

    lpStr = (LPVOID)VirtualAllocEx(hProcess, NULL, dwSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if(!lpStr) goto end;

    bWorks = WriteProcessMemory(hProcess, lpStr, (LPVOID)lpDLL, dwSize, &writtenSize);
    if(!bWorks) goto end;

#ifdef UNICODE
    procAddress = (UPARAM)GetProcAddress(GetModuleHandle(TEXT("KERNEL32")), "LoadLibraryW");
#else
    procAddress = (UPARAM)GetProcAddress(GetModuleHandle(TEXT("KERNEL32")), "LoadLibraryA");
#endif
    if(!procAddress) goto end;

    hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)procAddress, lpStr, 0, &dwTemp);
    if(!hThread) goto end;

    if(WaitForSingleObject(hThread, 200) == WAIT_OBJECT_0)
    {
        DWORD dw;
        GetExitCodeThread(hThread, &dw);
        bRet = dw != 0;

        SetLastError(0);
    }

end:
    DWORD lastError;
    if(!bRet)
        lastError = GetLastError();

    if(hThread)
        CloseHandle(hThread);
    if(lpStr)
        VirtualFreeEx(hProcess, lpStr, dwSize, MEM_RELEASE);

    if(!bRet)
        SetLastError(lastError);

    return bRet;
}

bool GraphicsCaptureSource::Init(XElement *data)
{
    this->data = data;
    return true;
}

GraphicsCaptureSource::~GraphicsCaptureSource()
{
    if(warningID)
    {
        API->RemoveStreamInfo(warningID);
        warningID = 0;
    }

    EndScene(); //should never actually need to be called, but doing it anyway just to be safe
}

LRESULT WINAPI GraphicsCaptureSource::ReceiverWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_CREATE:
            {
                CREATESTRUCT *cs = (CREATESTRUCT*)lParam;
                SetWindowLongPtr(hwnd, 0, (LONG_PTR)cs->lpCreateParams);
            }
            break;

        case RECEIVER_NEWCAPTURE:
            {
                GraphicsCaptureSource *source = (GraphicsCaptureSource*)GetWindowLongPtr(hwnd, 0);
                if(source)
                    source->NewCapture((LPVOID)lParam);
            }
            break;

        case RECEIVER_ENDCAPTURE:
            {
                GraphicsCaptureSource *source = (GraphicsCaptureSource*)GetWindowLongPtr(hwnd, 0);
                if(source)
                    source->EndCapture();
            }
            break;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

void GraphicsCaptureSource::NewCapture(LPVOID address)
{
    if(!hProcess)
        return;

    if(capture)
    {
        delete capture;
        capture = NULL;
    }

    CaptureInfo info;
    if(!ReadProcessMemory(hProcess, address, &info, sizeof(info), NULL))
    {
        AppWarning(TEXT("GraphicsCaptureSource::NewCapture: Could not read capture info from target process"));
        return;
    }

    bFlip = info.bFlip != 0;

    if(info.captureType == CAPTURETYPE_MEMORY)
        capture = new MemoryCapture;
    /*else if(info.captureType == CAPTURETYPE_SHAREDTEX)
        capture = new SharedTexCapture;*/
    else
    {
        AppWarning(TEXT("GraphicsCaptureSource::NewCapture: wtf, bad data from the target process"));
        return;
    }

    if(!capture->Init(hProcess, hwndTarget, info))
    {
        delete capture;
        capture = NULL;
    }
}

void GraphicsCaptureSource::EndCapture()
{
    delete capture;
    capture = NULL;
    bErrorAquiring = false;

    if(warningID)
    {
        API->RemoveStreamInfo(warningID);
        warningID = 0;
    }
}

bool GraphicsCaptureSource::FindSenderWindow()
{
    if(hwndSender)
        return true;

    while(hwndSender = FindWindowEx(NULL, hwndSender, SENDER_WINDOWCLASS, NULL))
    {
        DWORD procID = 0;

        GetWindowThreadProcessId(hwndSender, &procID);
        if(procID == targetProcessID)
            return true;
    }

    return false;
}

void GraphicsCaptureSource::BeginScene()
{
    if(bCapturing)
        return;

    strWindowClass = data->GetString(TEXT("windowClass"));
    if(strWindowClass.IsEmpty())
        return;

    hwndReceiver = CreateWindow(RECEIVER_WINDOWCLASS, NULL, 0, 0, 0, 0, 0, 0, 0, hinstMain, this);

    AttemptCapture();
}

void GraphicsCaptureSource::AttemptCapture()
{
    hwndTarget = FindWindow(strWindowClass, NULL);
    if(hwndTarget)
    {
        GetWindowThreadProcessId(hwndTarget, &targetProcessID);
        if(!targetProcessID)
        {
            AppWarning(TEXT("GraphicsCaptureSource::BeginScene: GetWindowThreadProcessId failed, GetLastError = %u"), GetLastError());
            bErrorAquiring = true;
            return;
        }
    }
    else
    {
        if(!warningID)
            warningID = API->AddStreamInfo(Str("Sources.SoftwareCaptureSource.WindowNotFound"), StreamInfoPriority_High);

        return;
    }

    if(warningID)
    {
        API->RemoveStreamInfo(warningID);
        warningID = 0;
    }

    //-------------------------------------------
    // see if we already hooked the process.  if not, inject DLL

    hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, targetProcessID);
    if(hProcess)
    {
        if(FindSenderWindow())
        {
            PostMessage(hwndSender, SENDER_RESTARTCAPTURE, 0, 0);
            bCapturing = true;
        }
        else
        {
            String strDLL;
            DWORD dwDirSize = GetCurrentDirectory(0, NULL);
            strDLL.SetLength(dwDirSize);
            GetCurrentDirectory(dwDirSize, strDLL);

            strDLL << TEXT("\\plugins\\GraphicsCapture\\GraphicsCaptureHook.dll");

            if(InjectLibrary(hProcess, strDLL))
                bCapturing = true;
            else
            {
                AppWarning(TEXT("GraphicsCaptureSource::BeginScene: Failed to inject library, GetLastError = %u"), GetLastError());

                CloseHandle(hProcess);
                hProcess = NULL;
                bErrorAquiring = true;
            }
        }
    }
    else
    {
        AppWarning(TEXT("GraphicsCaptureSource::BeginScene: OpenProcess failed, GetLastError = %u"), GetLastError());
        bErrorAquiring = true;
    }
}

void GraphicsCaptureSource::EndScene()
{
    if(!bCapturing)
        return;

    bCapturing = false;

    if(FindSenderWindow())
    {
        PostMessage(hwndSender, SENDER_ENDCAPTURE, 0, 0);
        hwndSender = NULL;
    }

    if(hwndReceiver)
    {
        DestroyWindow(hwndReceiver);
        hwndReceiver = NULL;
    }

    if(capture)
    {
        delete capture;
        capture = NULL;
    }

    if(hProcess)
    {
        CloseHandle(hProcess);
        hProcess = NULL;
    }
}

void GraphicsCaptureSource::Preprocess()
{
    if(!bCapturing && !bErrorAquiring)
        AttemptCapture();
}

void GraphicsCaptureSource::Render(const Vect2 &pos, const Vect2 &size)
{
    if(capture)
    {
        Texture *tex = capture->LockTexture();
        if(tex)
        {
            if(bFlip)
                DrawSprite(tex, 0xFFFFFFFF, pos.x, pos.y+size.y, pos.x+size.x, pos.y);
            else
                DrawSprite(tex, 0xFFFFFFFF, pos.x, pos.y, pos.x+size.x, pos.y+size.y);
            capture->UnlockTexture();
        }
    }
}

Vect2 GraphicsCaptureSource::GetSize() const
{
    return Vect2(float(cx), float(cy));
}

void GraphicsCaptureSource::UpdateSettings()
{
    EndScene();
    BeginScene();
}
