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
    if(parent->isEnabled)
    {
        ApplyNoiseGate(segment->audioData.Array(), segment->audioData.Num());
    }
    else
    {
        // Reset state
        attenuation = 0.0f;
        level = 0.0f;
        heldTime = 0.0f;
        isOpen = false;
    }
    return segment;
}

void NoiseGateFilter::ApplyNoiseGate(float *buffer, int totalFloats)
{
    // We assume stereo input
    if(totalFloats % 2)
        return; // Odd number of samples

    const float SAMPLE_RATE_F = float(OBSGetSampleRateHz());
    const float dtPerSample = 1.0f / SAMPLE_RATE_F;

    // Convert configuration times into per-sample amounts
    const float attackRate = 1.0f / (parent->attackTime * SAMPLE_RATE_F);
    const float releaseRate = 1.0f / (parent->releaseTime * SAMPLE_RATE_F);

    // Determine level decay rate. We don't want human voice (75-300Hz) to cross the close
    // threshold if the previous peak crosses the open threshold.
    const float thresholdDiff = parent->openThreshold - parent->closeThreshold;
    const float minDecayPeriod = (1.0f / 75.0f) * SAMPLE_RATE_F;
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

        // Test if disabled from the config window here so that the above state calculations
        // are still processed when playing around with the configuration
        if(!parent->isDisabledFromConfig) {
            // Multiple input by gate multiplier (0.0f if fully closed, 1.0f if fully open)
            buffer[i] *= attenuation;
            buffer[i+1] *= attenuation;
        }
    }
}

//============================================================================
// NoiseGateSettings class

NoiseGateSettings::NoiseGateSettings(NoiseGate *parent)
    : SettingsPane()
    , parent(parent)
{
}

NoiseGateSettings::~NoiseGateSettings()
{
}

CTSTR NoiseGateSettings::GetCategory() const
{
    static CTSTR name = Str("Plugins.NoiseGate.PluginName");
    return name;
}

HWND NoiseGateSettings::CreatePane(HWND parentHwnd)
{
    hwnd = OBSCreateDialog(NoiseGate::hinstDLL, MAKEINTRESOURCE(IDD_CONFIGURENOISEGATE), parentHwnd, (DLGPROC)DialogProc, (LPARAM)this);
    return hwnd;
}

void NoiseGateSettings::DestroyPane()
{
    DestroyWindow(hwnd);
    hwnd = NULL;
}

void NoiseGateSettings::ApplySettings()
{
    String str;
    int val;
    HWND ctrlHwnd;

    // Gate enabled
    if(SendMessage(GetDlgItem(hwnd, IDC_ENABLEGATE), BM_GETCHECK, 0, 0) == BST_CHECKED)
        parent->isEnabled = true;
    else
        parent->isEnabled = false;

    // Attack time
    ctrlHwnd = GetDlgItem(hwnd, IDC_ATTACKTIME_EDIT);
    str.SetLength(GetWindowTextLength(ctrlHwnd));
    GetWindowText(ctrlHwnd, str, str.Length() + 1);
    parent->attackTime = (float)str.ToInt() * 0.001f;

    // Hold time
    ctrlHwnd = GetDlgItem(hwnd, IDC_HOLDTIME_EDIT);
    str.SetLength(GetWindowTextLength(ctrlHwnd));
    GetWindowText(ctrlHwnd, str, str.Length() + 1);
    parent->holdTime = (float)str.ToInt() * 0.001f;

    // Release time
    ctrlHwnd = GetDlgItem(hwnd, IDC_RELEASETIME_EDIT);
    str.SetLength(GetWindowTextLength(ctrlHwnd));
    GetWindowText(ctrlHwnd, str, str.Length() + 1);
    parent->releaseTime = (float)str.ToInt() * 0.001f;

    // Close threshold
    val = (int)SendMessage(GetDlgItem(hwnd, IDC_CLOSETHRES_SLIDER), TBM_GETPOS, 0, 0);
    parent->closeThreshold = dbToRms((float)(-val));

    // Open threshold
    val = (int)SendMessage(GetDlgItem(hwnd, IDC_OPENTHRES_SLIDER), TBM_GETPOS, 0, 0);
    parent->openThreshold = dbToRms((float)(-val));

    // Save to file
    parent->SaveSettings();
}

void NoiseGateSettings::CancelSettings()
{
}

bool NoiseGateSettings::HasDefaults() const
{
    return true;
}

void NoiseGateSettings::SetDefaults()
{
    if(OBSMessageBox(hwnd, Str("Plugins.NoiseGate.ConfirmReset"), Str("Plugins.NoiseGate.ResetToDefaults"), MB_YESNO) == IDYES)
    {
        parent->LoadDefaults();
        RefreshConfig();
        parent->SaveSettings();
        SetChangedSettings(false);
    }
}

/**
 * Sets the caption of the open and close threshold trackbars ("-24 dB")
 */
void NoiseGateSettings::SetTrackbarCaption(int controlId, int db)
{
    SetWindowText(GetDlgItem(hwnd, controlId), FormattedString(TEXT("%d dB"), db));
}

/**
 * Updates the current audio volume control.
 */
void NoiseGateSettings::RepaintVolume()
{
    float rms, max, peak;

    rms = max = peak = -96.0f;
    if(OBSGetStreaming())
        OBSGetCurMicVolumeStats(&rms, &max, &peak);
    //SetWindowText(GetDlgItem(hwnd, IDC_OPENTHRES_DB), FormattedString(TEXT("%.3f"), max));
    //SetWindowText(GetDlgItem(hwnd, IDC_CLOSETHRES_DB), FormattedString(TEXT("%d"), (int)((max + 96.0f) * 4.0f)));
    SendMessage(GetDlgItem(hwnd, IDC_CURVOL), PBM_SETPOS, (int)((max + 96.0f) * 4.0f), 0);
}

/**
 * Reload the configuration settings from the main object. Useful for resetting to defaults.
 */
void NoiseGateSettings::RefreshConfig()
{
    float val;
    HWND ctrlHwnd;

    // Gate enabled
    SendMessage(GetDlgItem(hwnd, IDC_ENABLEGATE), BM_SETCHECK, parent->isEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

    // Attack, hold and release times
    SetWindowText(GetDlgItem(hwnd, IDC_ATTACKTIME_EDIT), IntString((int)(parent->attackTime * 1000.0f)));
    SetWindowText(GetDlgItem(hwnd, IDC_HOLDTIME_EDIT), IntString((int)(parent->holdTime * 1000.0f)));
    SetWindowText(GetDlgItem(hwnd, IDC_RELEASETIME_EDIT), IntString((int)(parent->releaseTime * 1000.0f)));

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
}

INT_PTR NoiseGateSettings::ProcMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
    case WM_INITDIALOG:
        MsgInitDialog();
        return TRUE;
    case WM_DESTROY:
        MsgDestroy();
        return TRUE;
    case WM_COMMAND:
        return MsgClicked(LOWORD(wParam), HIWORD(wParam), (HWND)lParam);
    case WM_VSCROLL:
    case WM_HSCROLL:
        return MsgScroll(message == WM_VSCROLL, wParam, lParam);
    case WM_TIMER:
        return MsgTimer((int)wParam);
    }
    return FALSE;
}

LRESULT CALLBACK NoiseGateSettings::PBSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    switch(uMsg)
    {
    case WM_ERASEBKGND:
        return TRUE;

    case WM_PAINT:
        {
            RECT clRect, eclRect;
            PAINTSTRUCT ps;
            PBRANGE pbRange;
            int position;
            bool orientation;
            float perc;

            HBRUSH hGreen  = CreateSolidBrush(0x32CD32);

            orientation = (GetWindowLongPtr(hWnd, GWL_STYLE) & PBS_VERTICAL) == PBS_VERTICAL;  // false = horizontal, true = vertical

            position = (int)SendMessage(hWnd, PBM_GETPOS, 0, 0);

            SendMessage(hWnd, PBM_GETRANGE, TRUE, (LPARAM)&pbRange);

            GetClientRect(hWnd, &clRect);
            memcpy(&eclRect, &clRect, sizeof(RECT));

            HDC hDC = BeginPaint(hWnd, &ps);
            
            perc = float(position - pbRange.iLow) / float(pbRange.iHigh - pbRange.iLow);

            if(orientation)
            {
                clRect.top = clRect.bottom - int(float(clRect.bottom - clRect.top) * perc);
                eclRect.bottom = clRect.top;
            }
            else
            {
                clRect.right = clRect.left + int(float(clRect.right - clRect.left) * perc);
                eclRect.left = clRect.right;
            }

            FillRect(hDC, &eclRect, (HBRUSH)COLOR_WINDOW);
            FillRect(hDC, &clRect, hGreen);

            DeleteObject(hGreen);

            EndPaint(hWnd, &ps);

            return TRUE;
        }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void NoiseGateSettings::MsgInitDialog()
{
    HWND ctrlHwnd;

    // Disable the filter if we are currently streaming
    if(OBSGetStreaming())
        parent->isDisabledFromConfig = true;

    LocalizeWindow(hwnd);

    // Load settings from parent object
    RefreshConfig();

    // Volume preview
    // Custom drawn progress bar, beware, static member
    ctrlHwnd = GetDlgItem(hwnd, IDC_CURVOL);
    SetWindowSubclass(ctrlHwnd, PBSubclassProc, 0, 0);

    SendMessage(ctrlHwnd, PBM_SETRANGE32, 0, CURVOL_RESOLUTION); // Bottom = 0, top = CURVOL_RESOLUTION
    RepaintVolume(); // Repaint immediately
    SetTimer(hwnd, REPAINT_TIMER_ID, 16, NULL); // Repaint every 16ms (~60fps)

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

    SetChangedSettings(false);
}

void NoiseGateSettings::MsgDestroy()
{
    // Make sure the filter is enabled again
    parent->isDisabledFromConfig = false;
}

INT_PTR NoiseGateSettings::MsgClicked(int controlId, int code, HWND controlHwnd)
{
    switch(controlId)
    {
    default:
        // Unknown button
        break;
    case IDC_ATTACKTIME_EDIT:
    case IDC_HOLDTIME_EDIT:
    case IDC_RELEASETIME_EDIT:
        if(code == EN_CHANGE) // Modified the text box
            SetChangedSettings(true);
        break;
    case IDC_ENABLEGATE:
        if(code == BN_CLICKED) // Changed the check box
            SetChangedSettings(true);
        break;
    case IDC_PREVIEWON:
        // Toggle stream preview
        if(OBSGetStreaming())
        {
            OBSStartStopPreview();
            parent->isDisabledFromConfig = false;
            SetWindowText(controlHwnd, Str("Plugins.NoiseGate.EnablePreview"));
        }
        else
        {
            OBSStartStopPreview();
            parent->isDisabledFromConfig = true;
            SetWindowText(controlHwnd, Str("Plugins.NoiseGate.DisablePreview"));
        }
        return TRUE;
    }

    return FALSE;
}

INT_PTR NoiseGateSettings::MsgScroll(bool vertical, WPARAM wParam, LPARAM lParam)
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
        SetChangedSettings(true);
        return TRUE;
    }
    else if(barHwnd == GetDlgItem(hwnd, IDC_OPENTHRES_SLIDER))
    {
        // User modified the open threshold trackbar
        SetTrackbarCaption(IDC_OPENTHRES_DB, pos);
        SetChangedSettings(true);
        return TRUE;
    }

    return FALSE;
}

INT_PTR NoiseGateSettings::MsgTimer(int timerId)
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
    , settings(NULL)
    , config()
    , isDisabledFromConfig(false)
    //, isEnabled() // Initialized in LoadDefaults()
    //, openThreshold()
    //, closeThreshold()
    //, attackTime()
    //, holdTime()
    //, releaseTime()
{
    LoadDefaults();
    config.Open(OBSGetPluginDataPath() + CONFIG_FILENAME, true);
    LoadSettings();

    // Create settings pane
    settings = new NoiseGateSettings(this);
    OBSAddSettingsPane(settings);
}

NoiseGate::~NoiseGate()
{
    // Delete our filter if it exists
    StreamStopped();

    // Delete settings pane
    OBSRemoveSettingsPane(settings);
    delete settings;

    // Close config file cleanly
    config.Close();
}

void NoiseGate::LoadDefaults()
{
    isEnabled = false;
    openThreshold = dbToRms(-26.0f);
    closeThreshold = dbToRms(-32.0f);
    attackTime = 0.025f;
    holdTime = 0.2f;
    releaseTime = 0.15f;
}

void NoiseGate::LoadSettings()
{
    isEnabled = config.GetInt(TEXT("General"), TEXT("IsEnabled"), isEnabled ? 1 : 0) ? true : false;
    openThreshold = dbToRms((float)config.GetInt(TEXT("General"), TEXT("OpenThreshold"), (int)rmsToDb(openThreshold)));
    closeThreshold = dbToRms((float)config.GetInt(TEXT("General"), TEXT("CloseThreshold"), (int)rmsToDb(closeThreshold)));
    attackTime = config.GetFloat(TEXT("General"), TEXT("AttackTime"), attackTime);
    holdTime = config.GetFloat(TEXT("General"), TEXT("HoldTime"), holdTime);
    releaseTime = config.GetFloat(TEXT("General"), TEXT("ReleaseTime"), releaseTime);
}

void NoiseGate::SaveSettings()
{
    config.SetInt(TEXT("General"), TEXT("IsEnabled"), isEnabled ? 1 : 0);
    config.SetInt(TEXT("General"), TEXT("OpenThreshold"), (int)rmsToDb(openThreshold));
    config.SetInt(TEXT("General"), TEXT("CloseThreshold"), (int)rmsToDb(closeThreshold));
    config.SetFloat(TEXT("General"), TEXT("AttackTime"), attackTime);
    config.SetFloat(TEXT("General"), TEXT("HoldTime"), holdTime);
    config.SetFloat(TEXT("General"), TEXT("ReleaseTime"), releaseTime);
}

void NoiseGate::StreamStarted()
{
    if (!isEnabled)
        return;

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
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
#if defined _M_X64 && _MSC_VER == 1800
        //workaround AVX2 bug in VS2013, http://connect.microsoft.com/VisualStudio/feedback/details/811093
        _set_FMA3_enable(0);
#endif
        NoiseGate::hinstDLL = hinstDLL;
    }
    return TRUE;
}
