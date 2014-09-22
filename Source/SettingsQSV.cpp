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

#include "Settings.h"

#include "mfxstructures.h"

#include <algorithm>

#include "../QSVHelper/Utilities.h"

bool QSVMethodAvailable(decltype(mfxInfoMFX::RateControlMethod) method);

namespace
{
    using ratecontrol_t = decltype(mfxInfoMFX::RateControlMethod);

    struct
    {
        int id;
        ratecontrol_t method;
    } id_method_map[] = {
            { IDC_QSVCBR,   MFX_RATECONTROL_CBR },
            { IDC_QSVVCM,   MFX_RATECONTROL_VCM },
            { IDC_QSVVBR,   MFX_RATECONTROL_VBR },
            { IDC_QSVAVBR,  MFX_RATECONTROL_AVBR },
            { IDC_QSVLA,    MFX_RATECONTROL_LA },
            { IDC_QSVCQP,   MFX_RATECONTROL_CQP },
            { IDC_QSVICQ,   MFX_RATECONTROL_ICQ },
            { IDC_QSVLAICQ, MFX_RATECONTROL_LA_ICQ },
    };

    int parameter_control_ids[] = {
        IDC_TARGETKBPS,
        IDC_USEGLOBALBITRATE,
        IDC_MAXKBPS,
        IDC_BUFFERSIZE,
        IDC_USEGLOBALBUFFERSIZE,
        IDC_ACCURACY_EDIT,
        IDC_ACCURACY,
        IDC_CONVERGENCE_EDIT,
        IDC_CONVERGENCE,
        IDC_QPI_EDIT,
        IDC_QPI,
        IDC_QPP_EDIT,
        IDC_QPP,
        IDC_QPB_EDIT,
        IDC_QPB,
        IDC_LADEPTH_EDIT,
        IDC_LADEPTH,
        IDC_ICQQUALITY_EDIT,
        IDC_ICQQUALITY,
    };

    struct
    {
        ratecontrol_t method;
        std::initializer_list<int> enabled_ids;
    } method_enabled_ids[] = {
            { MFX_RATECONTROL_CBR,    { IDC_TARGETKBPS, IDC_USEGLOBALBITRATE, IDC_BUFFERSIZE, IDC_USEGLOBALBUFFERSIZE } },
            { MFX_RATECONTROL_VCM,    { IDC_TARGETKBPS, IDC_USEGLOBALBITRATE, IDC_MAXKBPS, IDC_BUFFERSIZE, IDC_USEGLOBALBUFFERSIZE } }, //TODO: find settings that are actually used
            { MFX_RATECONTROL_VBR,    { IDC_TARGETKBPS, IDC_USEGLOBALBITRATE, IDC_MAXKBPS, IDC_BUFFERSIZE, IDC_USEGLOBALBUFFERSIZE } },
            { MFX_RATECONTROL_AVBR,   { IDC_TARGETKBPS, IDC_USEGLOBALBITRATE, IDC_ACCURACY, IDC_CONVERGENCE, IDC_ACCURACY_EDIT, IDC_CONVERGENCE_EDIT } },
            { MFX_RATECONTROL_LA,     { IDC_TARGETKBPS, IDC_USEGLOBALBITRATE, IDC_LADEPTH, IDC_LADEPTH_EDIT } },
            { MFX_RATECONTROL_CQP,    { IDC_QPI_EDIT, IDC_QPI, IDC_QPP_EDIT, IDC_QPP, IDC_QPB_EDIT, IDC_QPB } },
            { MFX_RATECONTROL_ICQ,    { IDC_ICQQUALITY, IDC_ICQQUALITY_EDIT } },
            { MFX_RATECONTROL_LA_ICQ, { IDC_LADEPTH, IDC_LADEPTH_EDIT, IDC_ICQQUALITY, IDC_ICQQUALITY_EDIT } }
    };
}

//============================================================================
// SettingsQSV class

SettingsQSV::SettingsQSV()
    : SettingsPane()
{
}

SettingsQSV::~SettingsQSV()
{
}

CTSTR SettingsQSV::GetCategory() const
{
    static CTSTR name = Str("Settings.QSV");
    return name;
}

HWND SettingsQSV::CreatePane(HWND parentHwnd)
{
    hwnd = OBSCreateDialog(hinstMain, MAKEINTRESOURCE(IDD_SETTINGS_QSV), parentHwnd, (DLGPROC)DialogProc, (LPARAM)this);
    return hwnd;
}

void SettingsQSV::DestroyPane()
{
    DestroyWindow(hwnd);
    hwnd = nullptr;
}

static ratecontrol_t get_method(HWND hwnd)
{
    for (auto mapping : id_method_map)
        if (SendMessage(GetDlgItem(hwnd, mapping.id), BM_GETCHECK, 0, 0) == BST_CHECKED)
            return mapping.method;

    return MFX_RATECONTROL_VBR;
}

void SettingsQSV::ApplySettings()
{
    auto checked = [&](int id) { return SendMessage(GetDlgItem(hwnd, id), BM_GETCHECK, 0, 0) == BST_CHECKED; };

    AppConfig->SetInt(L"QSV (Advanced)", L"UseCustomParams", checked(IDC_USECUSTOMPARAMS));

    ratecontrol_t method = get_method(hwnd);
    AppConfig->SetInt(L"QSV (Advanced)", L"RateControlMethod", method);

    AppConfig->SetInt(L"QSV (Advanced)", L"TargetKbps", GetEditText(GetDlgItem(hwnd, IDC_TARGETKBPS)).ToInt());
    AppConfig->SetInt(L"QSV (Advanced)", L"UseGlobalBitrate", checked(IDC_USEGLOBALBITRATE));
    AppConfig->SetInt(L"QSV (Advanced)", L"MaxKbps", GetEditText(GetDlgItem(hwnd, IDC_MAXKBPS)).ToInt());
    AppConfig->SetInt(L"QSV (Advanced)", L"BufferSizeInKB", GetEditText(GetDlgItem(hwnd, IDC_BUFFERSIZE)).ToInt());
    AppConfig->SetInt(L"QSV (Advanced)", L"UseGlobalBufferSize", checked(IDC_USEGLOBALBUFFERSIZE));

    auto save_range = [&](int id, CTSTR name)
    {
        AppConfig->SetInt(L"QSV (Advanced)", name, (UINT)SendMessage(GetDlgItem(hwnd, id), UDM_GETPOS32, 0, 0));
    };

    save_range(IDC_ACCURACY, L"Accuracy");

    save_range(IDC_CONVERGENCE, L"Convergence");

    save_range(IDC_LADEPTH, L"LADepth");

    save_range(IDC_QPI, L"QPI");
    save_range(IDC_QPP, L"QPP");
    save_range(IDC_QPB, L"QPB");

    save_range(IDC_ICQQUALITY, L"ICQQuality");

    RateControlMethodChanged();
}

void SettingsQSV::CancelSettings()
{
}

bool SettingsQSV::HasDefaults() const
{
    return false;
}

void SettingsQSV::RateControlMethodChanged()
{
    using namespace std;

    auto checked = [&](int id) { return SendMessage(GetDlgItem(hwnd, id), BM_GETCHECK, 0, 0) == BST_CHECKED; };
    auto enable  = [&](int id, bool enable) { EnableWindow(GetDlgItem(hwnd, id), enable); };

    bool enabled = checked(IDC_USECUSTOMPARAMS) && AppConfig->GetString(L"Video Encoding", L"Encoder") == L"QSV";

    //--------------------------------------------

    ratecontrol_t config_method = AppConfig->GetInt(L"QSV (Advanced)", L"RateControlMethod", MFX_RATECONTROL_CBR);
    if (!valid_method(config_method))
        config_method = MFX_RATECONTROL_CBR;

    ratecontrol_t method = get_method(hwnd);

    for (auto mapping : id_method_map)
        EnableWindow(GetDlgItem(hwnd, mapping.id), enabled && (config_method == mapping.method || QSVMethodAvailable(mapping.method)));

    //--------------------------------------------

    auto mapping = find_if(begin(method_enabled_ids), end(method_enabled_ids), [method](const decltype(method_enabled_ids[0]) &item) { return item.method == method; });
    if (mapping == end(method_enabled_ids))
        return;

    for (auto id : parameter_control_ids)
        EnableWindow(GetDlgItem(hwnd, id), enabled && find(begin(mapping->enabled_ids), end(mapping->enabled_ids), id) != end(mapping->enabled_ids));

    enable(IDC_TARGETKBPS, IsWindowEnabled(GetDlgItem(hwnd, IDC_USEGLOBALBITRATE)) && !checked(IDC_USEGLOBALBITRATE));
    enable(IDC_BUFFERSIZE, IsWindowEnabled(GetDlgItem(hwnd, IDC_USEGLOBALBUFFERSIZE)) && !checked(IDC_USEGLOBALBUFFERSIZE));
}

INT_PTR SettingsQSV::ProcMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    static bool updating_accuracy = false;

    switch (message)
    {
    case WM_INITDIALOG:
    {
        LocalizeWindow(hwnd);

        //--------------------------------------------

        auto check = [&](int item, bool checked)
        {
            SendMessage(GetDlgItem(hwnd, item), BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
        };

        //--------------------------------------------

        bool enabled = !!AppConfig->GetInt(L"QSV (Advanced)", L"UseCustomParams");
        check(IDC_USECUSTOMPARAMS, enabled);

        bool qsv_selected = !!(AppConfig->GetString(L"Video Encoding", L"Encoder") == L"QSV");
        EnableWindow(GetDlgItem(hwnd, IDC_USECUSTOMPARAMS), qsv_selected);
        enabled = enabled && qsv_selected;

        //--------------------------------------------

        ratecontrol_t method = AppConfig->GetInt(L"QSV (Advanced)", L"RateControlMethod", MFX_RATECONTROL_CBR);
        if (!valid_method(method))
            method = MFX_RATECONTROL_CBR;

        for (auto mapping : id_method_map)
            check(mapping.id, method == mapping.method);

        //--------------------------------------------

        bool use_global_bitrate = !!AppConfig->GetInt(L"QSV (Advanced)", L"UseGlobalBitrate", true);
        check(IDC_USEGLOBALBITRATE, use_global_bitrate);

        //--------------------------------------------

        bool use_global_buffer = !!AppConfig->GetInt(L"QSV (Advanced)", L"UseGlobalBufferSize", true);
        check(IDC_USEGLOBALBUFFERSIZE, use_global_buffer);

        //--------------------------------------------

        LoadSettingEditInt(GetDlgItem(hwnd, IDC_TARGETKBPS), L"QSV (Advanced)", L"TargetKbps", 1000);
        LoadSettingEditInt(GetDlgItem(hwnd, IDC_MAXKBPS), L"QSV (Advanced)", L"MaxKbps", 0);
        LoadSettingEditInt(GetDlgItem(hwnd, IDC_BUFFERSIZE), L"QSV (Advanced)", L"BufferSizeInKB", 0);

        //--------------------------------------------

        auto load_range = [&](int id, CTSTR name, int def, int min, int max)
        {
            HWND dlg = GetDlgItem(hwnd, id);
            int val = clamp(AppConfig->GetInt(L"QSV (Advanced)", name, def), min, max);
            SendMessage(dlg, UDM_SETRANGE32, min, max);
            SendMessage(dlg, UDM_SETPOS32, 0, val);
            return val;
        };

        int accuracy = load_range(IDC_ACCURACY, L"Accuracy", 1000, 0, 1000);

        load_range(IDC_CONVERGENCE, L"Convergence", 0, 0, 1000);

        load_range(IDC_LADEPTH, L"LADepth", 40, 10, 100);

        load_range(IDC_QPI, L"QPI", 23, 1, 51);
        load_range(IDC_QPP, L"QPP", 23, 1, 51);
        load_range(IDC_QPB, L"QPB", 23, 1, 51);

        load_range(IDC_ICQQUALITY, L"ICQQuality", 23, 1, 51);

        //--------------------------------------------

        SetWindowText(GetDlgItem(hwnd, IDC_ACCURACY_EDIT), FloatString(accuracy / 10.).Array());

        //--------------------------------------------

        RateControlMethodChanged();

        //--------------------------------------------

        //need this as some of the dialog item sets above trigger the notifications
        SetChangedSettings(false);
        ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_HIDE);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_USECUSTOMPARAMS:
        case IDC_QSVCBR:
        case IDC_QSVVCM:
        case IDC_QSVVBR:
        case IDC_QSVAVBR:
        case IDC_QSVLA:
        case IDC_QSVCQP:
        case IDC_QSVICQ:
        case IDC_QSVLAICQ:
        case IDC_USEGLOBALBITRATE:
        case IDC_USEGLOBALBUFFERSIZE:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                SetChangedSettings(true);
                if (App->GetVideoEncoder())
                    ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
                RateControlMethodChanged();
            }
            break;

        case IDC_ACCURACY_EDIT:
            if (HIWORD(wParam) == EN_CHANGE && !updating_accuracy)
            {
                updating_accuracy = true;
                int prev = int(GetEditText(GetDlgItem(hwnd, IDC_ACCURACY_EDIT)).ToFloat() * 10);
                int val = clamp(int(GetEditText(GetDlgItem(hwnd, IDC_ACCURACY_EDIT)).ToFloat() * 10), 0, 1000);
                SendMessage(GetDlgItem(hwnd, IDC_ACCURACY), UDM_SETPOS32, 0, val);
                if (val != prev)
                   SetWindowText(GetDlgItem(hwnd, IDC_ACCURACY_EDIT), FloatString(val / 10.).Array());
                updating_accuracy = false;
            }
        case IDC_TARGETKBPS:
        case IDC_MAXKBPS:
        case IDC_CUSTOMBUFFER:
        case IDC_CONVERGENCE_EDIT:
        case IDC_LADEPTH_EDIT:
        case IDC_QPI_EDIT:
        case IDC_QPP_EDIT:
        case IDC_QPB_EDIT:
        case IDC_ICQQUALITY_EDIT:
            if (HIWORD(wParam) == EN_CHANGE)
            {
                SetChangedSettings(true);
                if (App->GetVideoEncoder())
                    ShowWindow(GetDlgItem(hwnd, IDC_INFO), SW_SHOW);
            }
            break;
        }
        break;

    case WM_NOTIFY:
        auto foo = (LPNMHDR)lParam;
        switch (foo->code)
        {
        case UDN_DELTAPOS:
            {
                auto lpnmud = (LPNMUPDOWN)lParam;
                if (lpnmud->hdr.hwndFrom != GetDlgItem(hwnd, IDC_ACCURACY))
                    break;

                int newpos = lpnmud->iPos + lpnmud->iDelta;
                if (newpos < 0 || newpos > 1000)
                    return TRUE;

                updating_accuracy = true;
                SetWindowText(GetDlgItem(hwnd, IDC_ACCURACY_EDIT), FloatString(newpos/10.).Array());
                updating_accuracy = false;
                break;
            }
        }

    }
    return FALSE;
}
