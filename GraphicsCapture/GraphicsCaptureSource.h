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


class GraphicsCaptureSource : public ImageSource
{
    GraphicsCaptureMethod *capture;
    UINT cx, cy;

    XElement *data;

    bool bUseHotkey;
    DWORD hotkey, hotkeyID;
    String strWindowClass;
    String strExecutable;

    bool bUseDWMCapture;

    wchar_t lastProcessName[MAX_PATH];

    bool useSafeHook;
    HANDLE injectHelperProcess;

    HWND hwndTarget, hwndCapture, hwndNextTarget;
    HANDLE hTargetProcess;
    bool bCapturing, bErrorAcquiring, bFlip, bStretch, bAlphaBlend, bIgnoreAspect, bCaptureMouse;
    UINT captureWaitCount;
    DWORD targetProcessID;
    DWORD targetThreadID;
    UINT warningID;

    int gamma;

    POINT cursorPos;
    int xHotspot, yHotspot;
    bool bMouseCaptured, bMouseDown;
    HCURSOR hCurrentCursor;
    Shader *invertShader, *drawShader;
    Texture *cursorTexture;

    HANDLE hSignalRestart, hSignalEnd;
    HANDLE hSignalReady, hSignalExit;

    HANDLE hOBSIsAlive;

    float captureCheckInterval;

    DWORD foregroundPID;
    DWORD foregroundCheckCount;

    void NewCapture();
    void EndCapture();

    void AttemptCapture();
    BOOL CheckFileIntegrity(LPCTSTR path);

    static void STDCALL CaptureHotkey(DWORD hotkey, GraphicsCaptureSource *capture, bool bDown);

public:
    bool Init(XElement *data);
    ~GraphicsCaptureSource();

    void BeginScene();
    void EndScene();

    void Preprocess();
    void Tick(float fSeconds);
    void Render(const Vect2 &pos, const Vect2 &size);
    Vect2 GetSize() const;

    void UpdateSettings();

    void SetInt(CTSTR lpName, int iVal);
};
