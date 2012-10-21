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

    String strWindowClass;
    HWND hwndTarget, hwndSender, hwndReceiver;
    bool bCapturing, bErrorAcquiring, bFlip, bStretch;
    DWORD targetProcessID;
    HANDLE hProcess;
    UINT warningID;

    void NewCapture(LPVOID address);
    void EndCapture();

    bool FindSenderWindow();

    void AttemptCapture();

public:
    bool Init(XElement *data);
    ~GraphicsCaptureSource();

    void BeginScene();
    void EndScene();

    void Preprocess();
    void Render(const Vect2 &pos, const Vect2 &size);
    Vect2 GetSize() const;

    void UpdateSettings();

    static LRESULT WINAPI ReceiverWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
};
