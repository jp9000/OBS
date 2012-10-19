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


#include "GraphicsCaptureHook.h"


HINSTANCE hinstMain = NULL;
HWND hwndSender = NULL, hwndReceiver = NULL;
HANDLE textureMutexes[2] = {NULL, NULL};
bool bCapturing = true;
bool bTargetAquired = false;


LRESULT WINAPI SenderWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case SENDER_RESTARTCAPTURE:
            bCapturing = true;
            break;

        case SENDER_ENDCAPTURE:
            hwndReceiver = NULL;
            bCapturing = false;
            break;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

DWORD WINAPI CaptureThread(LPVOID lpNone)
{
    bool bSuccess = false;

    WNDCLASS wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.hInstance = hinstMain;
    wc.lpszClassName = SENDER_WINDOWCLASS;
    wc.lpfnWndProc = (WNDPROC)SenderWindowProc;

    if(RegisterClass(&wc))
    {
        hwndSender = CreateWindow(SENDER_WINDOWCLASS, NULL, 0, 0, 0, 0, 0, NULL, 0, hinstMain, 0);
        if(hwndSender)
        {
            textureMutexes[0] = OpenMutex(MUTEX_ALL_ACCESS, FALSE, TEXTURE_MUTEX1);
            if(textureMutexes[0])
            {
                textureMutexes[1] = OpenMutex(MUTEX_ALL_ACCESS, FALSE, TEXTURE_MUTEX2);
                if(textureMutexes[1])
                {
                    if(!InitD3D9Capture())
                        OutputDebugString(TEXT("Could not initialize D3D9 Capture"));

                    if(!InitGLCapture())
                        OutputDebugString(TEXT("Could not initialize OpenGL Capture"));

                    if(!InitDDrawCapture())
                        OutputDebugString(TEXT("Could not initialize DirectDraw Capture"));

                    MSG msg;
                    while(GetMessage(&msg, NULL, 0, 0))
                    {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                    }

                    CloseHandle(textureMutexes[1]);
                    textureMutexes[1] = NULL;
                }

                CloseHandle(textureMutexes[0]);
                textureMutexes[0] = NULL;
            }

            DestroyWindow(hwndSender);
        }
    }

    return 0;
}


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpBlah)
{
    if(dwReason == DLL_PROCESS_ATTACH)
    {
        HANDLE hThread = NULL;
        hinstMain = hinstDLL;

        if(!(hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CaptureThread, NULL, 0, 0)))
            return FALSE;

        CloseHandle(hThread);
    }
    else if(dwReason == DLL_PROCESS_DETACH)
    {
        if(hwndSender)
            DestroyWindow(hwndSender);

        for(UINT i=0; i<2; i++)
        {
            if(textureMutexes[i])
                CloseHandle(textureMutexes[i]);
        }
    }

    return TRUE;
}
