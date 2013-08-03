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


typedef HANDLE (WINAPI *CRTPROC)(HANDLE, LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
typedef BOOL   (WINAPI *WPMPROC)(HANDLE, LPVOID, LPCVOID, SIZE_T, SIZE_T*);
typedef LPVOID (WINAPI *VAEPROC)(HANDLE, LPVOID, SIZE_T, DWORD, DWORD);
typedef BOOL   (WINAPI *VFEPROC)(HANDLE, LPVOID, SIZE_T, DWORD);
typedef HANDLE (WINAPI *OPPROC) (DWORD, BOOL, DWORD);


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

    //--------------------------------------------------------

    int obfSize = 12;

    char pWPMStr[19], pCRTStr[19], pVAEStr[15], pVFEStr[14], pLLStr[13];
    mcpy(pWPMStr, "RvnrdPqmni|}Dmfegm", 19); //WriteProcessMemory with each character obfuscated
    mcpy(pCRTStr, "FvbgueQg`c{k]`yotp", 19); //CreateRemoteThread with each character obfuscated
    mcpy(pVAEStr, "WiqvpekGeddiHt", 15);     //VirtualAllocEx with each character obfuscated
    mcpy(pVFEStr, "Wiqvpek@{mnOu", 14);      //VirtualFreeEx with each character obfuscated
    mcpy(pLLStr,  "MobfImethzr", 12);        //LoadLibrary with each character obfuscated

#ifdef UNICODE
    pLLStr[11] = 'W';
#else
    pLLStr[11] = 'A';
#endif
    pLLStr[12] = 0;

    obfSize += 6;
    for (int i=0; i<obfSize; i++) pWPMStr[i] ^= i^5;
    for (int i=0; i<obfSize; i++) pCRTStr[i] ^= i^5;

    obfSize -= 4;
    for (int i=0; i<obfSize; i++) pVAEStr[i] ^= i^1;

    obfSize -= 1;
    for (int i=0; i<obfSize; i++) pVFEStr[i] ^= i^1;

    obfSize -= 2;
    for (int i=0; i<obfSize; i++) pLLStr[i]  ^= i^1;

    HMODULE hK32 = GetModuleHandle(TEXT("KERNEL32"));
    WPMPROC pWriteProcessMemory = (WPMPROC)GetProcAddress(hK32, pWPMStr);
    CRTPROC pCreateRemoteThread = (CRTPROC)GetProcAddress(hK32, pCRTStr);
    VAEPROC pVirtualAllocEx     = (VAEPROC)GetProcAddress(hK32, pVAEStr);
    VFEPROC pVirtualFreeEx      = (VFEPROC)GetProcAddress(hK32, pVFEStr);

    //--------------------------------------------------------

    lpStr = (LPVOID)(*pVirtualAllocEx)(hProcess, NULL, dwSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if(!lpStr) goto end;

    bWorks = (*pWriteProcessMemory)(hProcess, lpStr, (LPVOID)lpDLL, dwSize, &writtenSize);
    if(!bWorks) goto end;

    procAddress = (UPARAM)GetProcAddress(hK32, pLLStr);
    if(!procAddress) goto end;

    hThread = (*pCreateRemoteThread)(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)procAddress, lpStr, 0, &dwTemp);
    if(!hThread) goto end;

    if(WaitForSingleObject(hThread, 2000) == WAIT_OBJECT_0)
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
        (*pVirtualFreeEx)(hProcess, lpStr, 0, MEM_RELEASE);

    if(!bRet)
        SetLastError(lastError);

    return bRet;
}

bool GraphicsCaptureSource::Init(XElement *data)
{
    this->data = data;
    capture = NULL;

    Log(TEXT("Using graphics capture"));
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

static bool GetCaptureInfo(CaptureInfo &ci, DWORD processID)
{
    HANDLE hFileMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, String() << INFO_MEMORY << UINT(processID));
    if(hFileMap == NULL)
    {
        AppWarning(TEXT("GetCaptureInfo: Could not open file mapping"));
        return false;
    }

    CaptureInfo *infoIn;
    infoIn = (CaptureInfo*)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(CaptureInfo));
    if(!infoIn)
    {
        AppWarning(TEXT("GetCaptureInfo: Could not map view of file"));
        return false;
    }

    mcpy(&ci, infoIn, sizeof(CaptureInfo));

    if(infoIn)
        UnmapViewOfFile(infoIn);

    if(hFileMap)
        CloseHandle(hFileMap);

    return true;
}

void GraphicsCaptureSource::NewCapture()
{
    if(capture)
    {
        Log(TEXT("GraphicsCaptureSource::NewCapture:  eliminating old capture"));
        capture->Destroy();
        delete capture;
        capture = NULL;
    }

    if(!hSignalRestart)
    {
        hSignalRestart = GetEvent(String() << RESTART_CAPTURE_EVENT << UINT(targetProcessID));
        if(!hSignalRestart)
        {
            RUNONCE AppWarning(TEXT("GraphicsCaptureSource::NewCapture:  Could not create restart event"));
            return;
        }
    }

    hSignalEnd = GetEvent(String() << END_CAPTURE_EVENT << UINT(targetProcessID));
    if(!hSignalEnd)
    {
        RUNONCE AppWarning(TEXT("GraphicsCaptureSource::NewCapture:  Could not create end event"));
        return;
    }

    hSignalReady = GetEvent(String() << CAPTURE_READY_EVENT << UINT(targetProcessID));
    if(!hSignalReady)
    {
        RUNONCE AppWarning(TEXT("GraphicsCaptureSource::NewCapture:  Could not create ready event"));
        return;
    }

    hSignalExit = GetEvent(String() << APP_EXIT_EVENT << UINT(targetProcessID));
    if(!hSignalExit)
    {
        RUNONCE AppWarning(TEXT("GraphicsCaptureSource::NewCapture:  Could not create exit event"));
        return;
    }

    CaptureInfo info;
    if(!GetCaptureInfo(info, targetProcessID))
        return;

    bFlip = info.bFlip != 0;

    hwndCapture = (HWND)info.hwndCapture;

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

    if(!capture->Init(info))
    {
        capture->Destroy();
        delete capture;
        capture = NULL;
    }
}

void GraphicsCaptureSource::EndCapture()
{
    if(capture)
    {
        capture->Destroy();
        delete capture;
        capture = NULL;
    }

    if(hOBSIsAlive)
        CloseHandle(hOBSIsAlive);
    if(hSignalRestart)
        CloseHandle(hSignalRestart);
    if(hSignalEnd)
        CloseHandle(hSignalEnd);
    if(hSignalReady)
        CloseHandle(hSignalReady);
    if(hSignalExit)
        CloseHandle(hSignalExit);

    hSignalRestart = hSignalEnd = hSignalReady = hSignalExit = hOBSIsAlive = NULL;

    bErrorAcquiring = false;

    bCapturing = false;
    captureCheckInterval = -1.0f;
    hwndCapture = NULL;
    targetProcessID = 0;
    foregroundPID = 0;
    foregroundCheckCount = 0;

    if(warningID)
    {
        API->RemoveStreamInfo(warningID);
        warningID = 0;
    }
}

void GraphicsCaptureSource::Preprocess()
{
}

void GraphicsCaptureSource::BeginScene()
{
    if(bCapturing)
        return;

    strWindowClass = data->GetString(TEXT("windowClass"));
    if(strWindowClass.IsEmpty())
        return;

    bStretch = data->GetInt(TEXT("stretchImage")) != 0;
    bIgnoreAspect = data->GetInt(TEXT("ignoreAspect")) != 0;
    bCaptureMouse = data->GetInt(TEXT("captureMouse"), 1) != 0;

    bUseHotkey = data->GetInt(TEXT("useHotkey"), 0) != 0;
    hotkey = data->GetInt(TEXT("hotkey"), VK_F12);

    if (bUseHotkey)
        hotkeyID = OBSCreateHotkey(hotkey, (OBSHOTKEYPROC)GraphicsCaptureSource::CaptureHotkey, (UPARAM)this);

    gamma = data->GetInt(TEXT("gamma"), 100);

    if(bCaptureMouse && data->GetInt(TEXT("invertMouse")))
        invertShader = CreatePixelShaderFromFile(TEXT("shaders\\InvertTexture.pShader"));

    drawShader = CreatePixelShaderFromFile(TEXT("shaders\\DrawTexture_ColorAdjust.pShader"));

    AttemptCapture();
}

void GraphicsCaptureSource::AttemptCapture()
{
    //Log(TEXT("attempting to capture.."));

    if (!bUseHotkey)
        hwndTarget = FindWindow(strWindowClass, NULL);
    else
    {
        hwndTarget = hwndNextTarget;
        hwndNextTarget = NULL;
    }

    if (hwndTarget)
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
        if (!bUseHotkey && !warningID)
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

    char pOPStr[12];
    mcpy(pOPStr, "NpflUvhel{x", 12); //OpenProcess obfuscated
    for (int i=0; i<11; i++) pOPStr[i] ^= i^1;

    OPPROC pOpenProcess = (OPPROC)GetProcAddress(GetModuleHandle(TEXT("KERNEL32")), pOPStr);

    HANDLE hProcess = (*pOpenProcess)(PROCESS_ALL_ACCESS, FALSE, targetProcessID);
    if(hProcess)
    {
        //-------------------------------------------
        // load keepalive event

        hOBSIsAlive = CreateEvent(NULL, FALSE, FALSE, String() << OBS_KEEPALIVE_EVENT << UINT(targetProcessID));

        //-------------------------------------------

        hwndCapture = hwndTarget;

        hSignalRestart = OpenEvent(EVENT_ALL_ACCESS, FALSE, String() << RESTART_CAPTURE_EVENT << UINT(targetProcessID));
        if(hSignalRestart)
        {
            SetEvent(hSignalRestart);
            bCapturing = true;
            captureWaitCount = 0;
        }
        else
        {
            BOOL bSameBit = TRUE;

            if(Is64BitWindows())
            {
                BOOL bCurrentProcessWow64, bTargetProcessWow64;
                IsWow64Process(GetCurrentProcess(), &bCurrentProcessWow64);
                IsWow64Process(hProcess, &bTargetProcessWow64);

                bSameBit = (bCurrentProcessWow64 == bTargetProcessWow64);
            }

            if(bSameBit)
            {
                String strDLL;
                DWORD dwDirSize = GetCurrentDirectory(0, NULL);
                strDLL.SetLength(dwDirSize);
                GetCurrentDirectory(dwDirSize, strDLL);

                strDLL << TEXT("\\plugins\\GraphicsCapture\\GraphicsCaptureHook");

                BOOL b32bit = TRUE;
                if(Is64BitWindows())
                    IsWow64Process(hProcess, &b32bit);

                if(!b32bit)
                    strDLL << TEXT("64");

                strDLL << TEXT(".dll");

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
            else
            {
                String strDLLPath;
                DWORD dwDirSize = GetCurrentDirectory(0, NULL);
                strDLLPath.SetLength(dwDirSize);
                GetCurrentDirectory(dwDirSize, strDLLPath);

                strDLLPath << TEXT("\\plugins\\GraphicsCapture");

                BOOL b32bit = TRUE;
                if(Is64BitWindows())
                    IsWow64Process(hProcess, &b32bit);

                String strHelper = strDLLPath;
                strHelper << ((b32bit) ? TEXT("\\injectHelper.exe") : TEXT("\\injectHelper64.exe"));

                String strCommandLine;
                strCommandLine << TEXT("\"") << strHelper << TEXT("\" ") << UIntString(targetProcessID);

                //---------------------------------------

                PROCESS_INFORMATION pi;
                STARTUPINFO si;

                zero(&pi, sizeof(pi));
                zero(&si, sizeof(si));
                si.cb = sizeof(si);

                if(CreateProcess(strHelper, strCommandLine, NULL, NULL, FALSE, 0, NULL, strDLLPath, &si, &pi))
                {
                    int exitCode = 0;

                    WaitForSingleObject(pi.hProcess, INFINITE);
                    GetExitCodeProcess(pi.hProcess, (DWORD*)&exitCode);
                    CloseHandle(pi.hThread);
                    CloseHandle(pi.hProcess);

                    if(exitCode == 0)
                    {
                        captureWaitCount = 0;
                        bCapturing = true;
                    }
                    else
                    {
                        AppWarning(TEXT("GraphicsCaptureSource::BeginScene: Failed to inject library, error code = %d"), exitCode);
                        bErrorAcquiring = true;
                    }
                }
                else
                {
                    AppWarning(TEXT("GraphicsCaptureSource::BeginScene: Could not create inject helper, GetLastError = %u"), GetLastError());
                    bErrorAcquiring = true;
                }
            }
        }

        CloseHandle(hProcess);
        hProcess = NULL;

        if (!bCapturing)
        {
            CloseHandle(hOBSIsAlive);
            hOBSIsAlive = NULL;
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
    if(capture)
    {
        capture->Destroy();
        delete capture;
        capture = NULL;
    }

    delete invertShader;
    invertShader = NULL;

    delete drawShader;
    drawShader = NULL;

    delete cursorTexture;
    cursorTexture = NULL;

    if (hotkeyID)
    {
        OBSDeleteHotkey(hotkeyID);
        hotkeyID = 0;
    }

    if(!bCapturing)
        return;

    bCapturing = false;

    SetEvent(hSignalEnd);
    EndCapture();
}

void GraphicsCaptureSource::Tick(float fSeconds)
{
    if(hSignalExit && WaitForSingleObject(hSignalExit, 0) == WAIT_OBJECT_0) {
        Log(TEXT("Exit signal received, terminating capture"));
        EndCapture();
    }

    if(bCapturing && !hSignalReady && targetProcessID)
        hSignalReady = GetEvent(String() << CAPTURE_READY_EVENT << UINT(targetProcessID));

    if (hSignalReady) {
        DWORD val = WaitForSingleObject(hSignalReady, 0);
        if (val == WAIT_OBJECT_0)
            NewCapture();
        /*else if (val != WAIT_TIMEOUT)
            Log(TEXT("what the heck?  val is 0x%08lX"), val);*/
    }

    /*    static int floong = 0;

        if (floong++ == 30) {
            Log(TEXT("valid, bCapturing = %s"), bCapturing ? TEXT("true") : TEXT("false"));
            floong = 0;
        }
    } else {
        static int floong = 0;

        if (floong++ == 30) {
            Log(TEXT("not valid, bCapturing = %s"), bCapturing ? TEXT("true") : TEXT("false"));
            floong = 0;
        }
    }*/

    if(bCapturing && !capture)
    {
        if(++captureWaitCount >= API->GetMaxFPS())
        {
            bCapturing = false;
        }
    }

    if(!bCapturing && !bErrorAcquiring)
    {
        captureCheckInterval += fSeconds;
        if ((!bUseHotkey && captureCheckInterval >= 3.0f) ||
            (bUseHotkey && hwndNextTarget != NULL))
        {
            AttemptCapture();
            captureCheckInterval = 0.0f;
        }
    }
    else
    {
        if(!IsWindow(hwndCapture)) {
            Log(TEXT("Capture window 0x%08lX invalid or changing, terminating capture"), DWORD(hwndCapture));
            EndCapture();
        } else if (bUseHotkey && hwndNextTarget && hwndNextTarget != hwndTarget) {
            Log(TEXT("Capture hotkey triggered for new window, terminating capture"));
            EndCapture();
        } else {
            hwndNextTarget = NULL;
        }
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

void RoundVect2(Vect2 &v)
{
    v.x = float(round(v.x));
    v.y = float(round(v.y));
}

void GraphicsCaptureSource::Render(const Vect2 &pos, const Vect2 &size)
{
    if(capture)
    {
        Shader *lastShader = GetCurrentPixelShader();

        float fGamma = float(-(gamma-100) + 100) * 0.01f;

        LoadPixelShader(drawShader);
        HANDLE hGamma = drawShader->GetParameterByName(TEXT("gamma"));
        if(hGamma)
            drawShader->SetFloat(hGamma, fGamma);

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

                                UINT width, height;
                                LPBYTE lpData = GetCursorData(hIcon, ii, width, height);
                                if(lpData && width && height)
                                {
                                    cursorTexture = CreateTexture(width, height, GS_BGRA, lpData, FALSE);
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

        Texture *tex = capture->LockTexture();

        Vect2 texPos = Vect2(0.0f, 0.0f);
        Vect2 texStretch = Vect2(1.0f, 1.0f);

        if(tex)
        {
            Vect2 texSize = Vect2(float(tex->Width()), float(tex->Height()));
            Vect2 totalSize = API->GetBaseSize();

            Vect2 center = totalSize*0.5f;

            BlendFunction(GS_BLEND_ONE, GS_BLEND_ZERO);

            if(bStretch)
            {
                if(bIgnoreAspect)
                    texStretch *= totalSize;
                else
                {
                    float multiplyVal = (texSize.y * (totalSize.x / texSize.x) > totalSize.y) ? totalSize.y / texSize.y : multiplyVal = totalSize.x / texSize.x;
                        
                    texStretch *= texSize*multiplyVal;
                    texPos = center-(texStretch*0.5f);
                }
            }
            else
            {
                texStretch *= texSize;
                texPos = center-(texStretch*0.5f);
            }

            Vect2 sizeAdjust = size/totalSize;
            texPos *= sizeAdjust;
            texPos += pos;
            texStretch *= sizeAdjust;

            RoundVect2(texPos);
            RoundVect2(texSize);

            if(bFlip)
                DrawSprite(tex, 0xFFFFFFFF, texPos.x, texPos.y+texStretch.y, texPos.x+texStretch.x, texPos.y);
            else
                DrawSprite(tex, 0xFFFFFFFF, texPos.x, texPos.y, texPos.x+texStretch.x, texPos.y+texStretch.y);

            capture->UnlockTexture();

            BlendFunction(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);

            //----------------------------------------------------------
            // draw mouse

            if (!foregroundCheckCount)
            {
                //only check for foreground window every 10 frames since this involves two syscalls
                GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPID);
                foregroundCheckCount = 10;
            }

            if(bMouseCaptured && cursorTexture && foregroundPID == targetProcessID)
            {
                Vect2 newCursorPos  = Vect2(float(cursorPos.x-xHotspot), float(cursorPos.y-xHotspot));
                Vect2 newCursorSize = Vect2(float(cursorTexture->Width()), float(cursorTexture->Height()));

                newCursorPos  /= texSize;
                newCursorSize /= texSize;

                newCursorPos *= texStretch;
                newCursorPos += texPos;

                newCursorSize *= texStretch;

                bool bInvertCursor = false;
                if(invertShader)
                {
                    if(bInvertCursor = ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 || (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0))
                        LoadPixelShader(invertShader);
                }

                DrawSprite(cursorTexture, 0xFFFFFFFF, newCursorPos.x, newCursorPos.y, newCursorPos.x+newCursorSize.x, newCursorPos.y+newCursorSize.y);
            }

            foregroundCheckCount--;
        }

        if(lastShader)
            LoadPixelShader(lastShader);
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

void GraphicsCaptureSource::SetInt(CTSTR lpName, int iVal)
{
    if(scmpi(lpName, TEXT("gamma")) == 0)
    {
        gamma = iVal;
        if(gamma < 50)        gamma = 50;
        else if(gamma > 175)  gamma = 175;
    }
}

void STDCALL GraphicsCaptureSource::CaptureHotkey(DWORD hotkey, GraphicsCaptureSource *capture, bool bDown)
{
    if (bDown)
        capture->hwndNextTarget = GetForegroundWindow();
}
