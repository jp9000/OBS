/********************************************************************************
 Copyright (C) 2013 Lucas Murray <lmurray@undefinedfire.com>

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

#include "NoiseGate.h"
#include "resource.h"

//============================================================================
// Helpers

inline float rmsToDb(float rms)
{
    float db = 20.0f * log10(rms);
    if(!_finite(db))
        return VOL_MIN;
    return db;
}

inline float dbToRms(float db)
{
    return pow(10.0f, db / 20.0f);
}

//============================================================================
// NoiseGateFilter class

NoiseGateFilter::NoiseGateFilter(NoiseGate *parent)
    : parent(parent)
    , attenuation(0.0f)
    , level(0.0f)
    , heldTime(0.0f)
    , isOpen(false)
{
}

NoiseGateFilter::~NoiseGateFilter()
{
}

AudioSegment *NoiseGateFilter::Process(AudioSegment *segment)
{
    // We process even if the filter is disabled so that it's state is updated for when it gets enabled again
    ApplyNoiseGate(segment->audioData.Array(), segment->audioData.Num());
    return segment;
}

void NoiseGateFilter::ApplyNoiseGate(float *buffer, int totalFloats)
{
    // We assume stereo input
    if(totalFloats % 2)
        return; // Odd number of samples

    // OBS is currently hard-coded to 44.1ksps
    const float SAMPLE_RATE_F = 44100.0f;
    const float dtPerSample = 1.0f / SAMPLE_RATE_F;

    // Convert configuration times into per-sample amounts
    const float attackRate = 1.0f / (parent->attackTime * SAMPLE_RATE_F);
    const float releaseRate = 1.0f / (parent->releaseTime * SAMPLE_RATE_F);

    // Determine level decay rate. We don't want human voice (75-300Hz) to cross the close
    // threshold if the previous peak crosses the open threshold. 75Hz at 44.1ksps is ~590
    // samples between peaks.
    const float thresholdDiff = parent->openThreshold - parent->closeThreshold;
    const float minDecayPeriod = (1.0f / 75.0f) * SAMPLE_RATE_F; // ~590 samples
    const float decayRate = thresholdDiff / minDecayPeriod;

    // We can't use SSE as the processing of each sample depends on the processed
    // result of the previous sample.
    for(int i = 0; i < totalFloats; i += 2)
    {
        // Get current input level
        float curLvl = abs(buffer[i] + buffer[i+1]) * 0.5f;

        // Test thresholds
        if(curLvl > parent->openThreshold && !isOpen)
            isOpen = true;
        if(level < parent->closeThreshold && isOpen)
        {
            heldTime = 0.0f;
            isOpen = false;
        }

        // Decay level slowly so human voice (75-300Hz) doesn't cross the close threshold
        // (Essentially a peak detector with very fast decay)
        level = max(level, curLvl) - decayRate;

        // Apply gate state to attenuation
        if(isOpen)
            attenuation = min(1.0f, attenuation + attackRate);
        else
        {
            heldTime += dtPerSample;
            if(heldTime > parent->holdTime)
                attenuation = max(0.0f, attenuation - releaseRate);
        }

        // Multiple input by gate multiplier (0.0f if fully closed, 1.0f if fully open)
        if(parent->isEnabled) {
            buffer[i] *= attenuation;
            buffer[i+1] *= attenuation;
        }
    }
}

//============================================================================
// NoiseGateConfigWindow class

NoiseGateConfigWindow::NoiseGateConfigWindow(NoiseGate *parent, HWND parentHwnd)
    : parent(parent)
    , parentHwnd(parentHwnd)
    , hwnd(NULL)
{
}

NoiseGateConfigWindow::~NoiseGateConfigWindow()
{
}

INT_PTR CALLBACK NoiseGateConfigWindow::DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Get the pointer to our class instance
    NoiseGateConfigWindow *window = (NoiseGateConfigWindow *)GetWindowLongPtr(hwnd, DWLP_USER);

    switch(message)
    {
    default:
        // Unhandled
        break;
    case WM_INITDIALOG:
        SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)lParam);
        window = (NoiseGateConfigWindow *)lParam;
        window->hwnd = hwnd;
        window->MsgInitDialog();
        return TRUE;
    case WM_COMMAND:
        if(window != NULL)
            return window->MsgClicked(LOWORD(wParam), (HWND)lParam);
        break;
    case WM_VSCROLL:
    case WM_HSCROLL:
        if(window != NULL)
            return window->MsgScroll(message == WM_VSCROLL, wParam, lParam);
        break;
    case WM_TIMER:
        if(window != NULL)
            return window->MsgTimer((int)wParam);
        break;
    }

    return FALSE;
}

/**
 * Display the dialog and begin its message loop. Returns \t{true} if the
 * user clicked the "OK" button.
 */
bool NoiseGateConfigWindow::Process()
{
    INT_PTR res = DialogBoxParam(parent->hinstDLL, MAKEINTRESOURCE(IDD_CONFIGURENOISEGATE), parentHwnd, (DLGPROC)DialogProc, (LPARAM)this);
    if(res == IDOK)
        return true;
    return false;
}

/**
 * Sets the caption of the open and close threshold trackbars ("-24 dB")
 */
void NoiseGateConfigWindow::SetTrackbarCaption(int controlId, int db)
{
    SetWindowText(GetDlgItem(hwnd, controlId), FormattedString(TEXT("%d dB"), db));
}

/**
 * Updates the current audio volume control.
 */
void NoiseGateConfigWindow::RepaintVolume()
{
    float rms, max, peak;

    rms = max = peak = -96.0f;
    if(OBSGetStreaming())
        OBSGetCurMicVolumeStats(&rms, &max, &peak);
    //SetWindowText(GetDlgItem(hwnd, IDC_OPENTHRES_DB), FormattedString(TEXT("%.3f"), rms));
    //SetWindowText(GetDlgItem(hwnd, IDC_CLOSETHRES_DB), FormattedString(TEXT("%d"), (int)((rms + 96.0f) * 4.0f)));
    SendMessage(GetDlgItem(hwnd, IDC_CURVOL), PBM_SETPOS, (int)((rms + 96.0f) * 4.0f), 0);
}

void NoiseGateConfigWindow::MsgInitDialog()
{
    float val;
    HWND ctrlHwnd;

    LocalizeWindow(hwnd);

    // Volume preview
    // FIXME: Don't use a progress bar as the default Windows style smoothly interpolates between
    //        values making if difficult to see the real sound level.
    ctrlHwnd = GetDlgItem(hwnd, IDC_CURVOL);
    SendMessage(ctrlHwnd, PBM_SETRANGE32, 0, CURVOL_RESOLUTION); // Bottom = 0, top = CURVOL_RESOLUTION
    RepaintVolume(); // Repaint immediately
    SetTimer(hwnd, REPAINT_TIMER_ID, 16, NULL); // Repaint every 16ms (~60fps)

    // Close threshold trackbar (Control uses positive values)
    val = rmsToDb(parent->closeThreshold);
    SetTrackbarCaption(IDC_CLOSETHRES_DB, (int)val);
    ctrlHwnd = GetDlgItem(hwnd, IDC_CLOSETHRES_SLIDER);
    SendMessage(ctrlHwnd, TBM_SETRANGEMIN, FALSE, 0);
    SendMessage(ctrlHwnd, TBM_SETRANGEMAX, FALSE, 96);
    SendMessage(ctrlHwnd, TBM_SETPOS, TRUE, -(int)val);

    // Open threshold trackbar (Control uses positive values)
    val = rmsToDb(parent->openThreshold);
    SetTrackbarCaption(IDC_OPENTHRES_DB, (int)val);
    ctrlHwnd = GetDlgItem(hwnd, IDC_OPENTHRES_SLIDER);
    SendMessage(ctrlHwnd, TBM_SETRANGEMIN, FALSE, 0);
    SendMessage(ctrlHwnd, TBM_SETRANGEMAX, FALSE, 96);
    SendMessage(ctrlHwnd, TBM_SETPOS, TRUE, -(int)val);

    // Attack, hold and release times
    SetWindowText(GetDlgItem(hwnd, IDC_ATTACKTIME_EDIT), IntString((int)(parent->attackTime * 1000.0f)));
    SetWindowText(GetDlgItem(hwnd, IDC_HOLDTIME_EDIT), IntString((int)(parent->holdTime * 1000.0f)));
    SetWindowText(GetDlgItem(hwnd, IDC_RELEASETIME_EDIT), IntString((int)(parent->releaseTime * 1000.0f)));

    // Enable/disable stream button
    ctrlHwnd = GetDlgItem(hwnd, IDC_PREVIEWON);
    if(OBSGetStreaming())
    {
        SetWindowText(ctrlHwnd, Str("Plugins.NoiseGate.DisablePreview"));

        // If not in preview mode then prevent the user from stopping the stream from this window
        if(OBSGetPreviewOnly())
            EnableWindow(ctrlHwnd, TRUE);
        else
            EnableWindow(ctrlHwnd, FALSE);
    }
    else
    {
        SetWindowText(ctrlHwnd, Str("Plugins.NoiseGate.EnablePreview"));
        EnableWindow(ctrlHwnd, TRUE);
    }
}

INT_PTR NoiseGateConfigWindow::MsgClicked(int controlId, HWND controlHwnd)
{
    switch(controlId)
    {
    default:
        // Unknown button
        break;
    case IDOK:
    case IDCANCEL:
        EndDialog(hwnd, controlId); // Return IDOK (1) or IDCANCEL (2)
        return TRUE;
    case IDC_PREVIEWON:
        // Toggle stream preview
        if(OBSGetStreaming())
        {
            OBSStartStopPreview();
            parent->isEnabled = true;
            SetWindowText(controlHwnd, Str("Plugins.NoiseGate.EnablePreview"));
        }
        else
        {
            OBSStartStopPreview();
            parent->isEnabled = false;
            SetWindowText(controlHwnd, Str("Plugins.NoiseGate.DisablePreview"));
        }
        return TRUE;
    }

    return FALSE;
}


INT_PTR NoiseGateConfigWindow::MsgScroll(bool vertical, WPARAM wParam, LPARAM lParam)
{
    if(lParam == NULL)
        return FALSE;
    HWND barHwnd = (HWND)lParam;

    // Determine the current value of the trackbar
    int pos;
    switch(LOWORD(wParam))
    {
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK: // The user dragged the slider
        pos = -HIWORD(wParam);
        break;
    default: // Everything else
        pos = -(int)SendMessage(barHwnd, TBM_GETPOS, 0, 0);
        break;
    }

    // Figure out which trackbar it was
    if(barHwnd == GetDlgItem(hwnd, IDC_CLOSETHRES_SLIDER))
    {
        // User modified the close threshold trackbar
        SetTrackbarCaption(IDC_CLOSETHRES_DB, pos);
        return TRUE;
    }
    else if(barHwnd == GetDlgItem(hwnd, IDC_OPENTHRES_SLIDER))
    {
        // User modified the open threshold trackbar
        SetTrackbarCaption(IDC_OPENTHRES_DB, pos);
        return TRUE;
    }

    return FALSE;
}

INT_PTR NoiseGateConfigWindow::MsgTimer(int timerId)
{
    if(timerId == REPAINT_TIMER_ID)
    {
        RepaintVolume();
        return TRUE;
    }
    return FALSE;
}

//============================================================================
// NoiseGate class

// Statics
HINSTANCE NoiseGate::hinstDLL = NULL;
NoiseGate *NoiseGate::instance = NULL;

NoiseGate::NoiseGate()
    : micSource(NULL)
    , filter(NULL)
    , isEnabled(true)
    , openThreshold(0.05f) // FIXME: Configurable
    , closeThreshold(0.005f)
    , attackTime(0.1f)
    , holdTime(0.2f)
    , releaseTime(0.2f)
{
}

NoiseGate::~NoiseGate()
{
    // Delete our filter if it exists
    StreamStopped();
}

void NoiseGate::StreamStarted()
{
    micSource = OBSGetMicAudioSource();
    if(micSource == NULL)
        return; // No microphone
    filter = new NoiseGateFilter(this);
    micSource->AddAudioFilter(filter);
}

void NoiseGate::StreamStopped()
{
    if(filter) {
        micSource->RemoveAudioFilter(filter);
        delete filter;
        filter = NULL;
    }
    micSource = NULL;
}

void NoiseGate::ShowConfigDialog(HWND parentHwnd)
{
    NoiseGateConfigWindow dialog(this, parentHwnd);
    if(OBSGetStreaming())
        isEnabled = false; // Disable the filter if we are currently streaming
    dialog.Process();
    isEnabled = true; // Force enable
}

//============================================================================
// Plugin entry points

bool LoadPlugin()
{
    if(NoiseGate::instance != NULL)
        return false;
    NoiseGate::instance = new NoiseGate();
    return true;
}

void UnloadPlugin()
{
    if(NoiseGate::instance == NULL)
        return;
    delete NoiseGate::instance;
    NoiseGate::instance = NULL;
}

void ConfigPlugin(HWND parentHwnd)
{
    if(NoiseGate::instance == NULL)
        return;
    NoiseGate::instance->ShowConfigDialog(parentHwnd);
}

void OnStartStream()
{
    if(NoiseGate::instance == NULL)
        return;
    NoiseGate::instance->StreamStarted();
}

void OnStopStream()
{
    if(NoiseGate::instance == NULL)
        return;
    NoiseGate::instance->StreamStopped();
}

CTSTR GetPluginName()
{
    return Str("Plugins.NoiseGate.PluginName");
}

CTSTR GetPluginDescription()
{
    return Str("Plugins.NoiseGate.PluginDescription");
}

BOOL CALLBACK DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if(fdwReason == DLL_PROCESS_ATTACH)
        NoiseGate::hinstDLL = hinstDLL;
    return TRUE;
}
