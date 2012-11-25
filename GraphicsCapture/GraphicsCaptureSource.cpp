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
    LPVOID lpStr = NULL;
    BOOL bWorks,bRet=0;
    HANDLE hThread = NULL;
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
        VirtualFreeEx(hProcess, lpStr, 0, MEM_RELEASE);

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
                CaptureWindowData *data = (CaptureWindowData*)GetWindowLongPtr(hwnd, 0);
                if(data->source)
                    data->source->NewCapture((LPVOID)lParam);
            }
            return API->GetMaxFPS();

        case RECEIVER_ENDCAPTURE:
            {
                CaptureWindowData *data = (CaptureWindowData*)GetWindowLongPtr(hwnd, 0);
                if(data->source)
                {
                    API->EnterSceneMutex();
                    data->source->EndCapture();
                    API->LeaveSceneMutex();
                }
            }
            break;

        case WM_DESTROY:
            {
                CaptureWindowData *data = (CaptureWindowData*)GetWindowLongPtr(hwnd, 0);
                delete data;
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

    API->EnterSceneMutex();

    if(capture)
    {
        capture->Destroy();
        delete capture;
        capture = NULL;
    }

    CaptureInfo info;
    if(!ReadProcessMemory(hProcess, address, &info, sizeof(info), NULL))
    {
        API->LeaveSceneMutex();

        RUNONCE AppWarning(TEXT("GraphicsCaptureSource::NewCapture: Could not read capture info from target process"));
        return;
    }

    bFlip = info.bFlip != 0;

    hwndCapture = info.hwndCapture;

    if(info.captureType == CAPTURETYPE_MEMORY)
        capture = new MemoryCapture;
    else if(info.captureType == CAPTURETYPE_SHAREDTEX)
        capture = new SharedTexCapture;
    else
    {
        API->LeaveSceneMutex();

        RUNONCE AppWarning(TEXT("GraphicsCaptureSource::NewCapture: wtf, bad data from the target process"));
        return;
    }

    if(!capture->Init(hProcess, hwndTarget, info))
    {
        capture->Destroy();
        delete capture;
        capture = NULL;
    }

    API->LeaveSceneMutex();
}

void GraphicsCaptureSource::EndCapture()
{
    if(capture)
    {
        capture->Destroy();
        delete capture;
        capture = NULL;
    }

    bErrorAcquiring = false;
    bCapturing = false;
    captureCheckInterval = -1.0f;
    hwndCapture = NULL;

    if(warningID)
    {
        API->RemoveStreamInfo(warningID);
        warningID = 0;
    }
}

bool GraphicsCaptureSource::FindSenderWindow()
{
    if(hwndSender)
    {
        if(!IsWindow(hwndSender))
            hwndSender = NULL;
        else
            return true;
    }

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

    bStretch = data->GetInt(TEXT("stretchImage")) != 0;
    bCaptureMouse = data->GetInt(TEXT("captureMouse"), 1) != 0;

    if(bCaptureMouse && data->GetInt(TEXT("invertMouse")))
        invertShader = CreatePixelShaderFromFile(TEXT("shaders\\InvertTexture.pShader"));

    windowData = new CaptureWindowData(this);
    hwndReceiver = CreateWindow(RECEIVER_WINDOWCLASS, NULL, 0, 0, 0, 0, 0, 0, 0, hinstMain, windowData);

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
            bErrorAcquiring = true;
            return;
        }
    }
    else
    {
        if(!warningID)
            warningID = API->AddStreamInfo(Str("Sources.SoftwareCaptureSource.WindowNotFound"), StreamInfoPriority_High);

        bCapturing = false;

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
            captureWaitCount = 0;
        }
        else
        {
            String strDLL;
            DWORD dwDirSize = GetCurrentDirectory(0, NULL);
            strDLL.SetLength(dwDirSize);
            GetCurrentDirectory(dwDirSize, strDLL);

            strDLL << TEXT("\\plugins\\GraphicsCapture\\GraphicsCaptureHook.dll");

            if(InjectLibrary(hProcess, strDLL))
            {
                captureWaitCount = 0;
                bCapturing = true;
            }
            else
            {
                AppWarning(TEXT("GraphicsCaptureSource::BeginScene: Failed to inject library, GetLastError = %u"), GetLastError());

                CloseHandle(hProcess);
                hProcess = NULL;
                bErrorAcquiring = true;
            }
        }
    }
    else
    {
        AppWarning(TEXT("GraphicsCaptureSource::BeginScene: OpenProcess failed, GetLastError = %u"), GetLastError());
        bErrorAcquiring = true;
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

    if(windowData)
    {
        windowData->source = NULL;
        windowData = NULL;
    }

    if(hwndReceiver)
    {
        DestroyWindow(hwndReceiver);
        hwndReceiver = NULL;
    }

    if(capture)
    {
        capture->Destroy();
        delete capture;
        capture = NULL;
    }

    if(hProcess)
    {
        CloseHandle(hProcess);
        hProcess = NULL;
    }

    if(invertShader)
    {
        delete invertShader;
        invertShader = NULL;
    }

    if(cursorTexture)
    {
        delete cursorTexture;
        cursorTexture = NULL;
    }
}

void GraphicsCaptureSource::Tick(float fSeconds)
{
    if(bCapturing && !capture)
    {
        if(++captureWaitCount >= API->GetMaxFPS())
            bCapturing = false;
    }

    if(!bCapturing && !bErrorAcquiring)
    {
        captureCheckInterval += fSeconds;
        if(captureCheckInterval >= 3.0f)
        {
            AttemptCapture();
            captureCheckInterval = 0.0f;
        }
    }
    else
    {
        if(!FindSenderWindow())
            EndCapture();
    }
}

inline double round(double val)
{
    if(!_isnan(val) || !_finite(val))
        return val;

    if(val > 0.0f)
        return floor(val+0.5);
    else
        return floor(val-0.5);
}

LPBYTE GetCursorData(HICON hIcon, ICONINFO &ii, UINT &size)
{
    BITMAP bmp;
    HBITMAP hBmp = ii.hbmColor ? ii.hbmColor : ii.hbmMask;

    if(GetObject(hBmp, sizeof(bmp), &bmp) != 0)
    {
        BITMAPINFO bi;
        zero(&bi, sizeof(bi));

        size = bmp.bmWidth;

        void* lpBits;

        BITMAPINFOHEADER &bih = bi.bmiHeader;
        bih.biSize = sizeof(bih);
        bih.biBitCount = 32;
        bih.biWidth  = bmp.bmWidth;
        bih.biHeight = bmp.bmHeight;
        bih.biPlanes = 1;

        HDC hTempDC = CreateCompatibleDC(NULL);
        HBITMAP hBitmap = CreateDIBSection(hTempDC, &bi, DIB_RGB_COLORS, &lpBits, NULL, 0);
        HBITMAP hbmpOld = (HBITMAP)SelectObject(hTempDC, hBitmap);

        zero(lpBits, bmp.bmHeight*bmp.bmWidth*4);
        DrawIcon(hTempDC, 0, 0, hIcon);

        LPBYTE lpData = (LPBYTE)Allocate(bmp.bmHeight*bmp.bmWidth*4);
        mcpy(lpData, lpBits, bmp.bmHeight*bmp.bmWidth*4);

        SelectObject(hTempDC, hbmpOld);
        DeleteObject(hBitmap);
        DeleteDC(hTempDC);

        return lpData;
    }

    return NULL;
}

void GraphicsCaptureSource::Render(const Vect2 &pos, const Vect2 &size)
{
    if(capture)
    {
        //----------------------------------------------------------
        // capture mouse

        bMouseCaptured = false;
        if(bCaptureMouse)
        {
            CURSORINFO ci;
            zero(&ci, sizeof(ci));
            ci.cbSize = sizeof(ci);

            if(GetCursorInfo(&ci) && hwndCapture)
            {
                mcpy(&cursorPos, &ci.ptScreenPos, sizeof(cursorPos));

                ScreenToClient(hwndCapture, &cursorPos);

                if(ci.flags & CURSOR_SHOWING)
                {
                    if(ci.hCursor == hCurrentCursor)
                        bMouseCaptured = true;
                    else
                    {
                        HICON hIcon = CopyIcon(ci.hCursor);
                        hCurrentCursor = ci.hCursor;

                        delete cursorTexture;
                        cursorTexture = NULL;

                        if(hIcon)
                        {
                            ICONINFO ii;
                            if(GetIconInfo(hIcon, &ii))
                            {
                                xHotspot = int(ii.xHotspot);
                                yHotspot = int(ii.yHotspot);

                                UINT size;
                                LPBYTE lpData = GetCursorData(hIcon, ii, size);
                                if(lpData)
                                {
                                    cursorTexture = CreateTexture(size, size, GS_BGRA, lpData, FALSE);
                                    if(cursorTexture)
                                        bMouseCaptured = true;

                                    Free(lpData);
                                }

                                DeleteObject(ii.hbmColor);
                                DeleteObject(ii.hbmMask);
                            }

                            DestroyIcon(hIcon);
                        }
                    }
                }
            }
        }

        //----------------------------------------------------------
        // game texture

        Vect2 texPos = Vect2(0.0f, 0.0f);
        Vect2 texStretch = Vect2(1.0f, 1.0f);

        Texture *tex = capture->LockTexture();
        if(tex)
        {
            BlendFunction(GS_BLEND_ONE, GS_BLEND_ZERO);

            if(bStretch)
            {
                //Vect2 halfSize = size*0.5f;
                //Vect2 center = pos+halfSize;
                Vect2 halfSize = API->GetBaseSize()*0.5f;
                Vect2 center = halfSize;
                center.x = float(round(center.x));
                center.y = float(round(center.y));

                Vect2 texSize = Vect2(float(tex->Width()), float(tex->Height()));
                Vect2 outSize = API->GetBaseSize(), outPos = Vect2(0.0f, 0.0f);  //outSize = size,
                Vect2 lr = Vect2(0.0f, 0.0f);

                double sourceAspect = double(tex->Width())/double(tex->Height());
                double baseAspect = double(outSize.x)/double(outSize.y);

                if(!CloseDouble(baseAspect, sourceAspect))
                {
                    if(baseAspect < sourceAspect)
                        outSize.y = float(double(outSize.x) / sourceAspect);
                    else
                        outSize.x = float(double(outSize.y) * sourceAspect);

                    outPos = (API->GetBaseSize()-outSize)*0.5f; //(size-outSize)*0.5f;

                    outPos.x = (float)round(outPos.x);
                    outPos.y = (float)round(outPos.y);

                    outSize.x = (float)round(outSize.x);
                    outSize.y = (float)round(outSize.y);
                }

                lr += outSize;
                lr += outPos;
                //outPos += pos;

                texPos = outPos;
                texStretch = outSize/texSize;

                if(bFlip)
                    DrawSprite(tex, 0xFFFFFFFF, outPos.x, lr.y, lr.x, outPos.y);
                else
                    DrawSprite(tex, 0xFFFFFFFF, outPos.x, outPos.y, lr.x, lr.y);
            }
            else
            {
                //Vect2 halfSize = size*0.5f;
                //Vect2 center = pos+halfSize;
                Vect2 halfSize = API->GetBaseSize()*0.5f;
                Vect2 center = halfSize;
                center.x = float(round(center.x));
                center.y = float(round(center.y));

                Vect2 texHalfSize = Vect2(float(tex->Width()/2), float(tex->Height()/2));
                texPos = center-texHalfSize;

                if(bFlip)
                    DrawSprite(tex, 0xFFFFFFFF, center.x-texHalfSize.x, center.y+texHalfSize.y, center.x+texHalfSize.x, center.y-texHalfSize.y);
                else
                    DrawSprite(tex, 0xFFFFFFFF, center.x-texHalfSize.x, center.y-texHalfSize.y, center.x+texHalfSize.x, center.y+texHalfSize.y);
            }

            capture->UnlockTexture();

            BlendFunction(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);
        }

        //----------------------------------------------------------
        // draw mouse

        if(bMouseCaptured && cursorTexture)
        {
            float fCursorX = (texPos.x + texStretch.x * float(cursorPos.x-xHotspot));
            float fCursorY = (texPos.y + texStretch.y * float(cursorPos.y-xHotspot));
            float fCursorCX = texStretch.x * float(cursorTexture->Width());
            float fCursorCY = texStretch.y * float(cursorTexture->Height());

            Shader *lastShader;
            bool bInvertCursor = false;
            if(invertShader)
            {
                lastShader = GetCurrentPixelShader();
                if(bInvertCursor = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0)
                    LoadPixelShader(invertShader);
            }

            DrawSprite(cursorTexture, 0xFFFFFFFF, fCursorX, fCursorY+fCursorCY, fCursorX+fCursorCX, fCursorY);

            if(bInvertCursor)
                LoadPixelShader(lastShader);
        }
    }
}

Vect2 GraphicsCaptureSource::GetSize() const
{
    return API->GetBaseSize();
}

void GraphicsCaptureSource::UpdateSettings()
{
    EndScene();
    BeginScene();
}