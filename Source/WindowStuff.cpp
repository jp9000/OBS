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


#include "Main.h"
#include <shellapi.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <MMSystem.h>


//hello, you've come into the file I hate the most.

#define FREEZE_WND(hwnd)   SendMessage(hwnd, WM_SETREDRAW, (WPARAM)FALSE, (LPARAM) 0);
#define THAW_WND(hwnd)     {SendMessage(hwnd, WM_SETREDRAW, (WPARAM)TRUE, (LPARAM) 0); RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);}

extern WNDPROC listboxProc;
extern WNDPROC listviewProc;

void STDCALL SceneHotkey(DWORD hotkey, UPARAM param, bool bDown);

enum
{
    ID_LISTBOX_REMOVE = 1,
    ID_LISTBOX_MOVEUP,
    ID_LISTBOX_MOVEDOWN,
    ID_LISTBOX_MOVETOTOP,
    ID_LISTBOX_MOVETOBOTTOM,
    ID_LISTBOX_CENTER,
    ID_LISTBOX_CENTERHOR,
    ID_LISTBOX_CENTERVER,
    ID_LISTBOX_MOVELEFT,
    ID_LISTBOX_MOVETOP,
    ID_LISTBOX_MOVERIGHT,
    ID_LISTBOX_MOVEBOTTOM,
    ID_LISTBOX_FITTOSCREEN,
    ID_LISTBOX_RESETSIZE,
    ID_LISTBOX_RESETCROP,
    ID_LISTBOX_RENAME,
    ID_LISTBOX_COPY,
    ID_LISTBOX_HOTKEY,
    ID_LISTBOX_CONFIG,

    // Render frame related.
    ID_TOGGLERENDERVIEW,
    ID_TOGGLEPANEL,
    ID_TOGGLEFULLSCREEN,
    ID_PREVIEWSCALETOFITMODE,
    ID_PREVIEW1TO1MODE,

    ID_LISTBOX_ADD,

    ID_LISTBOX_GLOBALSOURCE=5000,
};

INT_PTR CALLBACK OBS::EnterSourceNameDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)lParam);
                LocalizeWindow(hwnd);

                String &strOut = *(String*)GetWindowLongPtr(hwnd, DWLP_USER);
                SetWindowText(GetDlgItem(hwnd, IDC_NAME), strOut);

                //SetFocus(GetDlgItem(hwnd, IDC_NAME));
                return TRUE;
            }

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDOK:
                    {
                        String str;
                        str.SetLength((UINT)SendMessage(GetDlgItem(hwnd, IDC_NAME), WM_GETTEXTLENGTH, 0, 0));
                        if(!str.Length())
                        {
                            MessageBox(hwnd, Str("EnterName"), NULL, 0);
                            break;
                        }

                        SendMessage(GetDlgItem(hwnd, IDC_NAME), WM_GETTEXT, str.Length()+1, (LPARAM)str.Array());

                        String &strOut = *(String*)GetWindowLongPtr(hwnd, DWLP_USER);

                        if(App->sceneElement)
                        {
                            XElement *sources = App->sceneElement->GetElement(TEXT("sources"));
                            if(!sources)
                                sources = App->sceneElement->CreateElement(TEXT("sources"));

                            XElement *foundSource = sources->GetElement(str);
                            if(foundSource != NULL && strOut != foundSource->GetName())
                            {
                                String strExists = Str("NameExists");
                                strExists.FindReplace(TEXT("$1"), str);
                                MessageBox(hwnd, strExists, NULL, 0);
                                break;
                            }
                        }

                        strOut = str;
                    }

                case IDCANCEL:
                    EndDialog(hwnd, LOWORD(wParam));
                    break;
            }
    }

    return 0;
}

INT_PTR CALLBACK OBS::SceneHotkeyDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)lParam);
                LocalizeWindow(hwnd);

                SceneHotkeyInfo *hotkeyInfo = (SceneHotkeyInfo*)lParam;
                SendMessage(GetDlgItem(hwnd, IDC_HOTKEY), HKM_SETHOTKEY, hotkeyInfo->hotkey, 0);

                return TRUE;                    
            }

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_CLEAR:
                    if(HIWORD(wParam) == BN_CLICKED)
                        SendMessage(GetDlgItem(hwnd, IDC_HOTKEY), HKM_SETHOTKEY, 0, 0);
                    break;

                case IDOK:
                    {
                        SceneHotkeyInfo *hotkeyInfo = (SceneHotkeyInfo*)GetWindowLongPtr(hwnd, DWLP_USER);

                        DWORD hotkey = (DWORD)SendMessage(GetDlgItem(hwnd, IDC_HOTKEY), HKM_GETHOTKEY, 0, 0);

                        if(hotkey == hotkeyInfo->hotkey)
                        {
                            EndDialog(hwnd, IDCANCEL);
                            break;
                        }

                        if(hotkey)
                        {
                            XElement *scenes = API->GetSceneListElement();
                            UINT numScenes = scenes->NumElements();
                            for(UINT i=0; i<numScenes; i++)
                            {
                                XElement *sceneElement = scenes->GetElementByID(i);
                                if(sceneElement->GetInt(TEXT("hotkey")) == hotkey)
                                {
                                    MessageBox(hwnd, Str("Scene.Hotkey.AlreadyInUse"), NULL, 0);
                                    return 0;
                                }
                            }
                        }

                        hotkeyInfo->hotkey = hotkey;
                    }

                case IDCANCEL:
                    EndDialog(hwnd, LOWORD(wParam));
                    break;
            }
    }

    return 0;
}

INT_PTR CALLBACK OBS::EnterSceneNameDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)lParam);
                LocalizeWindow(hwnd);

                String &strOut = *(String*)GetWindowLongPtr(hwnd, DWLP_USER);
                SetWindowText(GetDlgItem(hwnd, IDC_NAME), strOut);

                return TRUE;
            }

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDOK:
                    {
                        String str;
                        str.SetLength((UINT)SendMessage(GetDlgItem(hwnd, IDC_NAME), WM_GETTEXTLENGTH, 0, 0));
                        if(!str.Length())
                        {
                            MessageBox(hwnd, Str("EnterName"), NULL, 0);
                            break;
                        }

                        SendMessage(GetDlgItem(hwnd, IDC_NAME), WM_GETTEXT, str.Length()+1, (LPARAM)str.Array());

                        String &strOut = *(String*)GetWindowLongPtr(hwnd, DWLP_USER);

                        XElement *scenes = App->scenesConfig.GetElement(TEXT("scenes"));
                        XElement *foundScene = scenes->GetElement(str);
                        if(foundScene != NULL && strOut != foundScene->GetName())
                        {
                            String strExists = Str("NameExists");
                            strExists.FindReplace(TEXT("$1"), str);
                            MessageBox(hwnd, strExists, NULL, 0);
                            break;
                        }

                        strOut = str;
                    }

                case IDCANCEL:
                    EndDialog(hwnd, LOWORD(wParam));
                    break;
            }
    }
    return 0;
}

void OBS::GetNewSceneName(String &strScene)
{
    XElement *scenes = App->scenesConfig.GetElement(TEXT("scenes"));
    if(scenes)
    {
        String strTestName = strScene;

        UINT num = 1;
        while(scenes->GetElement(strTestName) != NULL)
            strTestName.Clear() << strScene << FormattedString(TEXT(" %u"), ++num);

        strScene = strTestName;
    }
}

void OBS::GetNewSourceName(String &strSource)
{
    XElement *sceneElement = API->GetSceneElement();
    if(sceneElement)
    {
        XElement *sources = sceneElement->GetElement(TEXT("sources"));
        if(!sources)
            sources = sceneElement->CreateElement(TEXT("sources"));

        String strTestName = strSource;

        UINT num = 1;
        while(sources->GetElement(strTestName) != NULL)
            strTestName.Clear() << strSource << FormattedString(TEXT(" %u"), ++num);

        strSource = strTestName;
    }
}

LRESULT CALLBACK OBS::ListboxHook(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UINT id = (UINT)GetWindowLongPtr(hwnd, GWL_ID);

    if(message == WM_RBUTTONDOWN)
    {
        int numItems = 0;
        if(id == ID_SCENES)
        {
            CallWindowProc(listboxProc, hwnd, WM_LBUTTONDOWN, wParam, lParam);
            numItems = (int)SendMessage(hwnd, LB_GETCOUNT, 0, 0);
        }
        else
        {
            LVHITTESTINFO htInfo;
            int index;

            // Default behaviour of left/right click is to check/uncheck, we do not want to toggle items when right clicking above checkbox.

            numItems = ListView_GetItemCount(hwnd);

            GetCursorPos(&htInfo.pt);
            ScreenToClient(hwnd, &htInfo.pt);

            index = ListView_HitTestEx(hwnd, &htInfo);

            if(index != -1)
            {
                // Focus our control
                if(GetFocus() != hwnd)
                    SetFocus(hwnd);
                
                // Clear all selected items state and select/focus the item we've right-clicked if it wasn't previously selected.
                if(!(ListView_GetItemState(hwnd, index, LVIS_SELECTED) & LVIS_SELECTED))
                {
                    ListView_SetItemState(hwnd , -1 , 0, LVIS_SELECTED | LVIS_FOCUSED);
                
                    ListView_SetItemState(hwnd, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

                    ListView_SetSelectionMark(hwnd, index);
                }
            }
            else
            {
                ListView_SetItemState(hwnd , -1 , 0, LVIS_SELECTED | LVIS_FOCUSED)
                CallWindowProc(listviewProc, hwnd, WM_RBUTTONDOWN, wParam, lParam);
            }
        }

        HMENU hMenu = CreatePopupMenu();

        bool bSelected = true;

        if(id == ID_SCENES)
        {
            SendMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_SCENES, LBN_SELCHANGE), (LPARAM)GetDlgItem(hwndMain, ID_SCENES));

            for(UINT i=0; i<App->sceneClasses.Num(); i++)
            {
                if (App->sceneClasses[i].bDeprecated)
                    continue;

                String strAdd = Str("Listbox.Add");
                strAdd.FindReplace(TEXT("$1"), App->sceneClasses[i].strName);
                AppendMenu(hMenu, MF_STRING, ID_LISTBOX_ADD+i, strAdd.Array());
            }
        }
        else if(id == ID_SOURCES)
        {
            if(!App->sceneElement)
                return 0;

            bSelected = ListView_GetSelectedCount(hwnd) != 0;
        }

        App->AppendModifyListbox(hwnd, hMenu, id, numItems, bSelected);

        POINT p;
        GetCursorPos(&p);

        int curSel = (id== ID_SOURCES)?(ListView_GetNextItem(hwnd, -1, LVNI_SELECTED)):((int)SendMessage(hwnd, LB_GETCURSEL, 0, 0));

        XElement *curSceneElement = App->sceneElement;

        if(id == ID_SCENES)
        {
            XElement *item = App->sceneElement;
            if(!item && numItems)
                return 0;

            ClassInfo *curClassInfo = NULL;
            if(numItems)
            {
                curClassInfo = App->GetSceneClass(item->GetString(TEXT("class")));
                if(!curClassInfo)
                {
                    curSceneElement->AddString(TEXT("class"), TEXT("Scene"));
                    curClassInfo = App->GetSceneClass(item->GetString(TEXT("class")));
                }

                if(curClassInfo && curClassInfo->configProc)
                {
                    AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
                    AppendMenu(hMenu, MF_STRING, ID_LISTBOX_CONFIG, Str("Listbox.Config"));
                }
            }

            bool bDelete = false;

            int ret = (int)TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, p.x, p.y, hwndMain, NULL);
            switch(ret)
            {
                default:
                    if(ret >= ID_LISTBOX_ADD)
                    {
                        App->EnableSceneSwitching(false);

                        String strName = Str("Scene");
                        GetNewSceneName(strName);

                        if(DialogBoxParam(hinstMain, MAKEINTRESOURCE(IDD_ENTERNAME), hwndMain, OBS::EnterSceneNameDialogProc, (LPARAM)&strName) == IDOK)
                        {
                            UINT classID = ret-ID_LISTBOX_ADD;
                            ClassInfo &ci = App->sceneClasses[classID];

                            XElement *scenes = App->scenesConfig.GetElement(TEXT("scenes"));
                            XElement *newSceneElement = scenes->CreateElement(strName);

                            newSceneElement->SetString(TEXT("class"), ci.strClass);
                            if(ci.configProc)
                            {
                                if(!ci.configProc(newSceneElement, true))
                                {
                                    scenes->RemoveElement(newSceneElement);
                                    break;
                                }
                            }
                            
                            UINT newID = (UINT)SendMessage(hwnd, LB_ADDSTRING, 0, (LPARAM)strName.Array());
                            PostMessage(hwnd, LB_SETCURSEL, newID, 0);
                            PostMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_SCENES, LBN_SELCHANGE), (LPARAM)hwnd);
                            App->ReportScenesChanged();
                        }

                        App->EnableSceneSwitching(true);
                    }
                    break;

                case ID_LISTBOX_REMOVE:
                    App->EnableSceneSwitching(false);

                    if(MessageBox(hwndMain, Str("DeleteConfirm"), Str("DeleteConfirm.Title"), MB_YESNO) == IDYES)
                    {
                        DWORD hotkey = item->GetInt(TEXT("hotkey"));
                        if(hotkey)
                            App->RemoveSceneHotkey(hotkey);

                        SendMessage(hwnd, LB_DELETESTRING, curSel, 0);
                        if(--numItems)
                        {
                            if(curSel == numItems)
                                curSel--;
                        }
                        else
                            curSel = LB_ERR;
                        
                        App->ReportScenesChanged();
                        bDelete = true;
                    }

                    App->EnableSceneSwitching(true);
                    break;

                case ID_LISTBOX_RENAME:
                    {
                        App->EnableSceneSwitching(false);

                        String strName = item->GetName();
                        if(DialogBoxParam(hinstMain, MAKEINTRESOURCE(IDD_ENTERNAME), hwndMain, OBS::EnterSceneNameDialogProc, (LPARAM)&strName) == IDOK)
                        {
                            SendMessage(hwnd, LB_DELETESTRING, curSel, 0);
                            SendMessage(hwnd, LB_INSERTSTRING, curSel, (LPARAM)strName.Array());
                            SendMessage(hwnd, LB_SETCURSEL, curSel, 0);

                            item->SetName(strName);
                            App->ReportScenesChanged();
                        }

                        App->EnableSceneSwitching(true);
                        break;
                    }
                case ID_LISTBOX_COPY:
                    {
                        App->EnableSceneSwitching(false);
                        
                        String strName = Str("Scene");
                        GetNewSceneName(strName);

                        if(DialogBoxParam(hinstMain, MAKEINTRESOURCE(IDD_ENTERNAME), hwndMain, OBS::EnterSceneNameDialogProc, (LPARAM)&strName) == IDOK)
                        {
                            UINT classID = 0;   // ID_LISTBOX_ADD - ID_LISTBOX_ADD
                            ClassInfo &ci = App->sceneClasses[classID];

                            XElement *scenes = App->scenesConfig.GetElement(TEXT("scenes"));
                            XElement *newSceneElement = scenes->CopyElement(item, strName);

                            newSceneElement->SetString(TEXT("class"), ci.strClass);
                            if(ci.configProc)
                            {
                                if(!ci.configProc(newSceneElement, true))
                                {
                                    scenes->RemoveElement(newSceneElement);
                                    break;
                                }
                            }

                            UINT newID = (UINT)SendMessage(hwnd, LB_ADDSTRING, 0, (LPARAM)strName.Array());
                            PostMessage(hwnd, LB_SETCURSEL, newID, 0);
                            PostMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_SCENES, LBN_SELCHANGE), (LPARAM)hwnd);
                            App->ReportScenesChanged();
                        }

                        App->EnableSceneSwitching(true);

                        break;
                    }
                case ID_LISTBOX_CONFIG:
                    App->EnableSceneSwitching(false);

                    if(curClassInfo->configProc(item, false))
                    {
                        if(App->bRunning)
                            App->scene->UpdateSettings();
                    }

                    App->EnableSceneSwitching(true);
                    break;

                case ID_LISTBOX_HOTKEY:
                    {
                        App->EnableSceneSwitching(false);

                        DWORD prevHotkey = item->GetInt(TEXT("hotkey"));

                        SceneHotkeyInfo hotkeyInfo;
                        hotkeyInfo.hotkey = prevHotkey;
                        hotkeyInfo.scene = item;

                        if(DialogBoxParam(hinstMain, MAKEINTRESOURCE(IDD_SCENEHOTKEY), hwndMain, OBS::SceneHotkeyDialogProc, (LPARAM)&hotkeyInfo) == IDOK)
                        {
                            if(hotkeyInfo.hotkey)
                                hotkeyInfo.hotkeyID = API->CreateHotkey(hotkeyInfo.hotkey, SceneHotkey, 0);

                            item->SetInt(TEXT("hotkey"), hotkeyInfo.hotkey);

                            if(prevHotkey)
                                App->RemoveSceneHotkey(prevHotkey);

                            if(hotkeyInfo.hotkeyID)
                                App->sceneHotkeys << hotkeyInfo;
                        }

                        App->EnableSceneSwitching(true);
                        break;
                    }

                case ID_LISTBOX_MOVEUP:
                    if(curSel > 0)
                    {
                        SendMessage(hwnd, LB_DELETESTRING, curSel, 0);
                        SendMessage(hwnd, LB_INSERTSTRING, curSel-1, (LPARAM)item->GetName());
                        SendMessage(hwnd, LB_SETCURSEL, curSel-1, 0);

                        curSel--;
                        item->MoveUp();
                        App->ReportScenesChanged();
                    }
                    break;

                case ID_LISTBOX_MOVEDOWN:
                    if(curSel != (numItems-1))
                    {
                        SendMessage(hwnd, LB_DELETESTRING, curSel, 0);
                        SendMessage(hwnd, LB_INSERTSTRING, curSel+1, (LPARAM)item->GetName());
                        SendMessage(hwnd, LB_SETCURSEL, curSel+1, 0);

                        curSel++;
                        item->MoveDown();
                        App->ReportScenesChanged();
                    }
                    break;

                case ID_LISTBOX_MOVETOTOP:
                    if(curSel != 0)
                    {
                        SendMessage(hwnd, LB_DELETESTRING, curSel, 0);
                        SendMessage(hwnd, LB_INSERTSTRING, 0, (LPARAM)item->GetName());
                        SendMessage(hwnd, LB_SETCURSEL, 0, 0);

                        curSel = 0;
                        item->MoveToTop();
                        App->ReportScenesChanged();
                    }
                    break;

                case ID_LISTBOX_MOVETOBOTTOM:
                    if(curSel != numItems-1)
                    {
                        SendMessage(hwnd, LB_DELETESTRING, curSel, 0);
                        SendMessage(hwnd, LB_INSERTSTRING, numItems-1, (LPARAM)item->GetName());
                        SendMessage(hwnd, LB_SETCURSEL, numItems-1, 0);

                        curSel = numItems-1;
                        item->MoveToBottom();
                        App->ReportScenesChanged();
                    }
                    break;
            }

            if(curSel != LB_ERR)
            {
                SendMessage(hwnd, LB_SETCURSEL, curSel, 0);
                SendMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_SCENES, LBN_SELCHANGE), (LPARAM)GetDlgItem(hwndMain, ID_SCENES));

                if(bDelete)
                    item->GetParent()->RemoveElement(item);
            }
            else if(bDelete)
            {
                if(App->bRunning)
                {
                    OSEnterMutex(App->hSceneMutex);
                    delete App->scene;
                    App->scene = NULL;
                    OSLeaveMutex(App->hSceneMutex);
                }

                App->bChangingSources = true;
                ListView_DeleteAllItems(GetDlgItem(hwndMain, ID_SOURCES));
                App->bChangingSources = false;
                
                item->GetParent()->RemoveElement(item);
                App->sceneElement = NULL;
            }
        }
        else if(id == ID_SOURCES)
        {
            if(!App->sceneElement && numItems)
                return 0;

            List<SceneItem*> selectedSceneItems;
            if(App->scene)
                App->scene->GetSelectedItems(selectedSceneItems);

            if(selectedSceneItems.Num() < 1)
                nop();

            int ret = (int)TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, p.x, p.y, hwndMain, NULL);
            App->TrackModifyListbox(hwnd, ret);
        }

        DestroyMenu(hMenu);

        return 0;
    }

    if(id == ID_SOURCES)
    {
        return CallWindowProc(listviewProc, hwnd, message, wParam, lParam);
    }
    else
    {
        return CallWindowProc(listboxProc, hwnd, message, wParam, lParam);
    }
}

void OBS::TrackModifyListbox(HWND hwnd, int ret)
{
    HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);
    UINT numSelected = (ListView_GetSelectedCount(hwndSources));
    XElement *selectedElement = NULL;
    ClassInfo *curClassInfo = NULL;
    if(numSelected == 1)
    {
        UINT selectedID = ListView_GetNextItem(hwndSources, -1, LVNI_SELECTED);
        XElement *sourcesElement = App->sceneElement->GetElement(TEXT("sources"));
        selectedElement = sourcesElement->GetElementByID(selectedID);
        curClassInfo = App->GetImageSourceClass(selectedElement->GetString(TEXT("class")));
    }

    switch(ret)
    {
        // General render frame stuff above here
        case ID_TOGGLERENDERVIEW:
            App->bRenderViewEnabled = !App->bRenderViewEnabled;
            App->bForceRenderViewErase = !App->bRenderViewEnabled;
            App->UpdateRenderViewMessage();
            break;
        case ID_TOGGLEPANEL:
            if (App->bFullscreenMode)
                App->bPanelVisibleFullscreen = !App->bPanelVisibleFullscreen;
            else
                App->bPanelVisibleWindowed = !App->bPanelVisibleWindowed;
            App->bPanelVisible = App->bFullscreenMode ? App->bPanelVisibleFullscreen : App->bPanelVisibleWindowed;
            App->bPanelVisibleProcessed = false;
            App->ResizeWindow(true);
            break;
        case ID_TOGGLEFULLSCREEN:
            App->SetFullscreenMode(!App->bFullscreenMode);
            break;
        case ID_PREVIEWSCALETOFITMODE:
            App->renderFrameIn1To1Mode = false;
            App->ResizeRenderFrame(true);
            break;
        case ID_PREVIEW1TO1MODE:
            App->renderFrameIn1To1Mode = true;
            App->ResizeRenderFrame(true);
            break;

        // Sources below here
        default:
            if(ret >= ID_LISTBOX_ADD)
            {
                App->EnableSceneSwitching(false);

                ClassInfo *ci;
                if(ret >= ID_LISTBOX_GLOBALSOURCE)
                    ci = App->GetImageSourceClass(TEXT("GlobalSource"));
                else
                {
                    UINT classID = ret-ID_LISTBOX_ADD;
                    ci = App->imageSourceClasses+classID;
                }

                String strName;
                if(ret >= ID_LISTBOX_GLOBALSOURCE)
                {
                    List<CTSTR> sourceNames;
                    App->GetGlobalSourceNames(sourceNames);
                    strName = sourceNames[ret-ID_LISTBOX_GLOBALSOURCE];
                }
                else
                    strName = ci->strName;

                GetNewSourceName(strName);

                if(DialogBoxParam(hinstMain, MAKEINTRESOURCE(IDD_ENTERNAME), hwndMain, OBS::EnterSourceNameDialogProc, (LPARAM)&strName) == IDOK)
                {
                    XElement *curSceneElement = App->sceneElement;
                    XElement *sources = curSceneElement->GetElement(TEXT("sources"));
                    if(!sources)
                        sources = curSceneElement->CreateElement(TEXT("sources"));
                    XElement *newSourceElement = sources->InsertElement(0, strName);
                    newSourceElement->SetInt(TEXT("render"), 1);

                    if(ret >= ID_LISTBOX_GLOBALSOURCE)
                    {
                        newSourceElement->SetString(TEXT("class"), TEXT("GlobalSource"));
                                
                        List<CTSTR> sourceNames;
                        App->GetGlobalSourceNames(sourceNames);

                        CTSTR lpName = sourceNames[ret-ID_LISTBOX_GLOBALSOURCE];
                        if(App->bRunning) App->GetGlobalSource(lpName)->GlobalSourceEnterScene();

                        XElement *data = newSourceElement->CreateElement(TEXT("data"));
                        data->SetString(TEXT("name"), lpName);

                        XElement *globalElement = App->GetGlobalSourceElement(lpName);
                        if(globalElement)
                        {
                            newSourceElement->SetInt(TEXT("cx"), globalElement->GetInt(TEXT("cx"), 100));
                            newSourceElement->SetInt(TEXT("cy"), globalElement->GetInt(TEXT("cy"), 100));
                        }
                    }
                    else
                    {
                        newSourceElement->SetString(TEXT("class"), ci->strClass);
                        if(ci->configProc)
                        {
                            if(!ci->configProc(newSourceElement, true))
                            {
                                sources->RemoveElement(newSourceElement);
                                App->EnableSceneSwitching(true);
                                break;
                            }
                        }
                    }

                    if(App->sceneElement == curSceneElement)
                    {
                        if(App->bRunning)
                        {
                            App->EnterSceneMutex();
                            App->scene->InsertImageSource(0, newSourceElement);
                            App->LeaveSceneMutex();
                        }

                        UINT numSources = sources->NumElements();

                        // clear selection/focus for all items before adding the new item
                        ListView_SetItemState(hwndSources , -1 , 0, LVIS_SELECTED | LVIS_FOCUSED);
                        ListView_SetItemCount(hwndSources, numSources);
                                
                        App->bChangingSources = true;
                        App->InsertSourceItem(0, (LPWSTR)strName.Array(), true);
                        App->bChangingSources = false;

                        SetFocus(hwndSources);
                                
                        // make sure the added item is visible/selected/focused and selection mark moved to it.
                        ListView_EnsureVisible(hwndSources, 0, false);
                        ListView_SetItemState(hwndSources, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                        ListView_SetSelectionMark(hwndSources, 0);

                        App->ReportSourcesAddedOrRemoved();
                    }
                }

                App->EnableSceneSwitching(true);
            }
            break;

        case ID_LISTBOX_REMOVE:
            App->DeleteItems();
            break;

        case ID_LISTBOX_RENAME:
            {
                App->EnableSceneSwitching(false);

                String strName = selectedElement->GetName();
                TSTR oldStrName = sdup(strName.Array());
                if(DialogBoxParam(hinstMain, MAKEINTRESOURCE(IDD_ENTERNAME), hwndMain, OBS::EnterSourceNameDialogProc, (LPARAM)&strName) == IDOK)
                {
                    int curSel = (int)SendMessage(hwndSources, LB_GETCURSEL, 0, 0);
                    ListView_SetItemText(hwndSources, curSel, 0, strName.Array());
                    selectedElement->SetName(strName);
                            
                    App->ReportSourceChanged(oldStrName, selectedElement);
                    Free((void*)oldStrName);
                            
                    ListView_SetColumnWidth(hwndSources, 0, LVSCW_AUTOSIZE_USEHEADER);
                    ListView_SetColumnWidth(hwndSources, 1, LVSCW_AUTOSIZE_USEHEADER);
                }

                App->EnableSceneSwitching(true);
                break;
            }

        case ID_LISTBOX_CONFIG:
            {
                Vect2 multiple;
                ImageSource *source;

                App->EnableSceneSwitching(false);

                List<SceneItem*> selectedSceneItems;
                if(App->scene)
                    App->scene->GetSelectedItems(selectedSceneItems);

                if(App->bRunning)
                {
                    source = selectedSceneItems[0]->GetSource();
                    if(source)
                    {
                        Vect2 curSize = Vect2(selectedElement->GetFloat(TEXT("cx"), 32.0f), selectedElement->GetFloat(TEXT("cy"), 32.0f));
                        Vect2 baseSize = source->GetSize();

                        multiple = curSize/baseSize;
                    }
                }
                if(curClassInfo->configProc && curClassInfo->configProc(selectedElement, false))
                {
                    if(App->bRunning)
                    {
                        App->EnterSceneMutex();

                        if(source)
                        {
                            Vect2 newSize = Vect2(selectedElement->GetFloat(TEXT("cx"), 32.0f), selectedElement->GetFloat(TEXT("cy"), 32.0f));
                            newSize *= multiple;

                            selectedElement->SetFloat(TEXT("cx"), newSize.x);
                            selectedElement->SetFloat(TEXT("cy"), newSize.y);

                            selectedSceneItems[0]->GetSource()->UpdateSettings();
                        }

                        selectedSceneItems[0]->Update();

                        App->LeaveSceneMutex();
                    }
                }

                App->EnableSceneSwitching(true);
            }
            break;

        case ID_LISTBOX_MOVEUP:
            App->MoveSourcesUp();
            break;

        case ID_LISTBOX_MOVEDOWN:
            App->MoveSourcesDown();
            break;

        case ID_LISTBOX_MOVETOTOP:
            App->MoveSourcesToTop();
            break;

        case ID_LISTBOX_MOVETOBOTTOM:
            App->MoveSourcesToBottom();
            break;

        case ID_LISTBOX_CENTER:
            App->CenterItems(true, true);
            break;

        case ID_LISTBOX_CENTERHOR:
            App->CenterItems(true, false);
            break;

        case ID_LISTBOX_CENTERVER:
            App->CenterItems(false, true);
            break;

        case ID_LISTBOX_MOVELEFT:
            App->MoveItemsToEdge(-1, 0);
            break;

        case ID_LISTBOX_MOVETOP:
            App->MoveItemsToEdge(0, -1);
            break;

        case ID_LISTBOX_MOVERIGHT:
            App->MoveItemsToEdge(1, 0);
            break;

        case ID_LISTBOX_MOVEBOTTOM:
            App->MoveItemsToEdge(0, 1);
            break;

        case ID_LISTBOX_FITTOSCREEN:
            App->FitItemsToScreen();
            break;

        case ID_LISTBOX_RESETSIZE:
            App->ResetItemSizes();
            break;

        case ID_LISTBOX_RESETCROP:
            App->ResetItemCrops();
            break;
    }
}

void OBS::AppendModifyListbox(HWND hwnd, HMENU hMenu, int id, int numItems, bool bSelected)
{
    if(GetMenuItemCount(hMenu) > 0 && id == ID_SOURCES)
        AppendMenu(hMenu, MF_SEPARATOR, 0, 0);

        if(id == ID_SOURCES)
        {
            HMENU hmenuAdd = CreatePopupMenu();

            for(UINT i=0; i<App->imageSourceClasses.Num(); i++)
            {
                if (App->imageSourceClasses[i].bDeprecated)
                    continue;

                String strAdd = App->imageSourceClasses[i].strName;

                if(App->imageSourceClasses[i].strClass == TEXT("GlobalSource"))
                {
                    List<CTSTR> sourceNames;
                    App->GetGlobalSourceNames(sourceNames);

                    if(!sourceNames.Num())
                        continue;

                    HMENU hmenuGlobals = CreatePopupMenu();

                    for(UINT j=0; j<sourceNames.Num(); j++)
                        AppendMenu(hmenuGlobals, MF_STRING, ID_LISTBOX_GLOBALSOURCE+j, sourceNames[j]);

                    AppendMenu(hmenuAdd, MF_STRING|MF_POPUP, (UINT_PTR)hmenuGlobals, strAdd.Array());
                }
                else
                    AppendMenu(hmenuAdd, MF_STRING, ID_LISTBOX_ADD+i, strAdd.Array());
            }

            AppendMenu(hMenu, MF_STRING|MF_POPUP, (UINT_PTR)hmenuAdd, Str("Add"));
        }

    if(numItems && bSelected)
    {
        String strRemove               = Str("Remove");
        String strRename               = Str("Rename");
        String strCopy                 = Str("Copy");
        String strMoveUp               = Str("MoveUp");
        String strMoveDown             = Str("MoveDown");
        String strMoveTop              = Str("MoveToTop");
        String strMoveToBottom         = Str("MoveToBottom");
        String strPositionSize         = Str("Listbox.Positioning");
        String strCenter               = Str("Listbox.Center");
        String strCenterHor            = Str("Listbox.CenterHorizontally");
        String strCenterVer            = Str("Listbox.CenterVertically");
        String strMoveLeftOfCanvas     = Str("Listbox.MoveLeft");
        String strMoveTopOfCanvas      = Str("Listbox.MoveTop");
        String strMoveRightOfCanvas    = Str("Listbox.MoveRight");
        String strMoveBottomOfCanvas   = Str("Listbox.MoveBottom");
        String strFitToScreen          = Str("Listbox.FitToScreen");
        String strResize               = Str("Listbox.ResetSize");
        String strResetCrop            = Str("Listbox.ResetCrop");
        String strConfig               = Str("Listbox.Config");

        if(id == ID_SOURCES)
        {
            strRemove       << TEXT("\tDel");
            strMoveUp       << TEXT("\tCtrl-Up");
            strMoveDown     << TEXT("\tCtrl-Down");
            strMoveTop      << TEXT("\tCtrl-Home");
            strMoveToBottom << TEXT("\tCtrl-End");
            strCenter       << TEXT("\tCtrl-C");
            strCenterVer    << TEXT("\tCtrl-Shift-C");
            strCenterHor    << TEXT("\tCtrl-Alt-C");
            strFitToScreen  << TEXT("\tCtrl-F");
            strResize       << TEXT("\tCtrl-R");
            strMoveLeftOfCanvas << TEXT("\tCtrl-Alt-Left");
            strMoveTopOfCanvas << TEXT("\tCtrl-Alt-Up");
            strMoveRightOfCanvas << TEXT("\tCtrl-Alt-Right");
            strMoveBottomOfCanvas << TEXT("\tCtrl-Alt-Down");
            strResetCrop    << TEXT("\tCtrl-Alt-R");
        }

        AppendMenu(hMenu, MF_SEPARATOR, 0, 0);

        HMENU hMenuOrder = CreatePopupMenu();

        AppendMenu(hMenuOrder, MF_STRING, ID_LISTBOX_MOVEUP,         strMoveUp);
        AppendMenu(hMenuOrder, MF_STRING, ID_LISTBOX_MOVEDOWN,       strMoveDown);
        AppendMenu(hMenuOrder, MF_SEPARATOR, 0, 0);
        AppendMenu(hMenuOrder, MF_STRING, ID_LISTBOX_MOVETOTOP,      strMoveTop);
        AppendMenu(hMenuOrder, MF_STRING, ID_LISTBOX_MOVETOBOTTOM,   strMoveToBottom);

        AppendMenu(hMenu, MF_STRING|MF_POPUP, (UINT_PTR)hMenuOrder, Str("Order"));


        if (id == ID_SOURCES)
        {
            HMENU hmenuPositioning = CreatePopupMenu();
            AppendMenu(hmenuPositioning, MF_STRING, ID_LISTBOX_FITTOSCREEN,    strFitToScreen);
            AppendMenu(hmenuPositioning, MF_STRING, ID_LISTBOX_RESETSIZE,      strResize);
            AppendMenu(hmenuPositioning, MF_STRING, ID_LISTBOX_RESETCROP,      strResetCrop);
            AppendMenu(hmenuPositioning, MF_SEPARATOR, 0, 0);
            AppendMenu(hmenuPositioning, MF_STRING, ID_LISTBOX_CENTER,         strCenter);
            AppendMenu(hmenuPositioning, MF_STRING, ID_LISTBOX_CENTERHOR,         strCenterHor);
            AppendMenu(hmenuPositioning, MF_STRING, ID_LISTBOX_CENTERVER,         strCenterVer);
            AppendMenu(hmenuPositioning, MF_SEPARATOR, 0, 0);
            AppendMenu(hmenuPositioning, MF_STRING, ID_LISTBOX_MOVELEFT,         strMoveLeftOfCanvas);
            AppendMenu(hmenuPositioning, MF_STRING, ID_LISTBOX_MOVETOP,         strMoveTopOfCanvas);
            AppendMenu(hmenuPositioning, MF_STRING, ID_LISTBOX_MOVERIGHT,         strMoveRightOfCanvas);
            AppendMenu(hmenuPositioning, MF_STRING, ID_LISTBOX_MOVEBOTTOM,         strMoveBottomOfCanvas);

            AppendMenu(hMenu, MF_STRING|MF_POPUP, (UINT_PTR)hmenuPositioning, strPositionSize.Array());
        }

        /////AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
        AppendMenu(hMenu, MF_STRING, ID_LISTBOX_REMOVE,         strRemove);


        UINT numSelected = (id==ID_SOURCES)?(ListView_GetSelectedCount(hwnd)):((UINT)SendMessage(hwnd, LB_GETSELCOUNT, 0, 0));
        if(id == ID_SCENES || numSelected == 1)
        {
            /////AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
            AppendMenu(hMenu, MF_STRING, ID_LISTBOX_RENAME,         strRename);
        }

        if(id == ID_SCENES && numSelected)
        {
            AppendMenu(hMenu, MF_STRING, ID_LISTBOX_COPY,           strCopy);
            AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
            AppendMenu(hMenu, MF_STRING, ID_LISTBOX_HOTKEY, Str("Listbox.SetHotkey"));
        }

        if(id == ID_SOURCES)
        {
            UINT numSelected = (ListView_GetSelectedCount(hwnd));
            XElement *selectedElement = NULL;
            ClassInfo *curClassInfo = NULL;
            if(numSelected == 1)
            {
                UINT selectedID = ListView_GetNextItem(hwnd, -1, LVNI_SELECTED);
                
                XElement *sourcesElement = App->sceneElement->GetElement(TEXT("sources"));
                selectedElement = sourcesElement->GetElementByID(selectedID);

                curClassInfo = App->GetImageSourceClass(selectedElement->GetString(TEXT("class")));
                if(curClassInfo && curClassInfo->configProc)
                {
                    AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
                    AppendMenu(hMenu, MF_STRING, ID_LISTBOX_CONFIG, Str("Listbox.Config"));
                }
            }
        }
    }
}

//----------------------------

void OBS::DeleteItems()
{
    HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);
    int numItems = ListView_GetItemCount(hwndSources);

    List<SceneItem*> selectedSceneItems;
    if(App->scene)
        App->scene->GetSelectedItems(selectedSceneItems);

    List<UINT> selectedIDs;
       
    //get selected items
    int iPos = ListView_GetNextItem(hwndSources, -1, LVNI_SELECTED);
    while (iPos != -1)
    {
        selectedIDs.Add((UINT) iPos);
        iPos = ListView_GetNextItem(hwndSources, iPos, LVNI_SELECTED);
    }

    XElement *sourcesElement = NULL;
    List<XElement*> selectedElements;
    if(numItems)
    {
        sourcesElement = App->sceneElement->GetElement(TEXT("sources"));
        for(UINT i=0; i<selectedIDs.Num(); i++)
            selectedElements << sourcesElement->GetElementByID(selectedIDs[i]);
    }

    if(selectedIDs.Num())
    {
        if(MessageBox(hwndMain, Str("DeleteConfirm"), Str("DeleteConfirm.Title"), MB_YESNO) == IDYES)
        {
            if(selectedSceneItems.Num())
            {
                App->EnterSceneMutex();

                for(UINT i=0; i<selectedSceneItems.Num(); i++)
                {
                    bool globalSourcesRemain = false;

                    SceneItem *item = selectedSceneItems[i];
                    XElement *source = selectedSceneItems[i]->GetElement();
                    String className = source->GetString(TEXT("class"));

                    if(className == "GlobalSource") {
                        String globalSourceName = source->GetElement(TEXT("data"))->GetString(TEXT("name"));

                        for(UINT j=0; j<scene->sceneItems.Num(); j++) {
                            XElement *compareSource = scene->sceneItems[j]->GetElement()->GetElement(TEXT("data"));
                            String globalSourceCompareName = compareSource->GetString(TEXT("name"));
                            SceneItem *compareItem = scene->sceneItems[j];
                            if(globalSourceName == globalSourceCompareName && item != compareItem) {
                                globalSourcesRemain = true;
                            }
                        }

                        if(!globalSourcesRemain) {
                            App->GetGlobalSource(globalSourceName)->GlobalSourceLeaveScene();
                        }
                    }
                    App->scene->RemoveImageSource(item);
                }
            }
            else
            {
                for(UINT i=0; i<selectedElements.Num(); i++)
                    sourcesElement->RemoveElement(selectedElements[i]);
            }

            while(selectedIDs.Num())
            {
                UINT id = selectedIDs[0];
                selectedIDs.Remove(0);

                for(UINT i=0; i<selectedIDs.Num(); i++)
                {
                    if(selectedIDs[i] > id)
                        selectedIDs[i]--;
                }
                App->bChangingSources = true;
                ListView_DeleteItem(hwndSources, id);
                ListView_SetColumnWidth(hwndSources, 0, LVSCW_AUTOSIZE_USEHEADER);
                ListView_SetColumnWidth(hwndSources, 1, LVSCW_AUTOSIZE_USEHEADER);
                App->bChangingSources = false;
            }

            if(selectedSceneItems.Num())
                App->LeaveSceneMutex();
            ReportSourcesAddedOrRemoved();
        }
    }
}

void OBS::SetSourceOrder(StringList &sourceNames)
{
    HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);
    int numItems = ListView_GetItemCount(hwndSources);

    if(numItems == 0)
        return;

    XElement* sourcesElement = App->sceneElement->GetElement(TEXT("sources"));
    
    FREEZE_WND(hwndSources);

    for(UINT i=0; i<sourceNames.Num(); i++)
    {
        
        /* find id of source by name */
        int id = -1;
        for(UINT j = 0; j < sourcesElement->NumElements(); j++)
        {
            XElement* source = sourcesElement->GetElementByID(j);
            if(scmp(source->GetName(), sourceNames[i].Array()) == 0)
            {
                id = j;
                break;
            }
        }

        if(id >= 0)
        {
            if(App->scene)
            {
                App->scene->GetSceneItem(id)->MoveToBottom();
            }
            else
            {
                sourcesElement->GetElementByID(id)->MoveToBottom();
            }

            String strName = GetLVText(hwndSources, id);
            bool checkState = ListView_GetCheckState(hwndSources, id) > 0;
            
            bChangingSources = true;
            ListView_DeleteItem(hwndSources, id);
            InsertSourceItem(numItems-1, (LPWSTR)strName.Array(), checkState);
            bChangingSources = false;    
        }
    }

    THAW_WND(hwndSources);

    ReportSourceOrderChanged();
}


void OBS::MoveSourcesUp()
{
    HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);
    int numItems = ListView_GetItemCount(hwndSources);
    UINT focusedItem = -1, selectionMark;

    List<SceneItem*> selectedSceneItems;
    if(App->scene)
        App->scene->GetSelectedItems(selectedSceneItems);

    List<UINT> selectedIDs;

    selectionMark = ListView_GetSelectionMark(hwndSources);

    //get selected items
    int iPos = ListView_GetNextItem(hwndSources, -1, LVNI_SELECTED);
    while (iPos != -1)
    {
        selectedIDs.Add((UINT) iPos);
        if(ListView_GetItemState(hwndSources, iPos, LVIS_FOCUSED) & LVIS_FOCUSED)
            focusedItem = iPos;
        iPos = ListView_GetNextItem(hwndSources, iPos, LVNI_SELECTED);
    }

    XElement *sourcesElement = NULL;
    List<XElement*> selectedElements;
    if(numItems)
    {
        sourcesElement = App->sceneElement->GetElement(TEXT("sources"));
        for(UINT i=0; i<selectedIDs.Num(); i++)
            selectedElements << sourcesElement->GetElementByID(selectedIDs[i]);
    }

    UINT stateFlags;
    FREEZE_WND(hwndSources);
    for(UINT i=0; i<selectedIDs.Num(); i++)
    {
        if( (i == 0 && selectedIDs[i] > 0) ||
            (i != 0 && selectedIDs[i-1] != selectedIDs[i]-1) )
        {
            if(App->scene)
                selectedSceneItems[i]->MoveUp();
            else
                selectedElements[i]->MoveUp();

            String strName = GetLVText(hwndSources, selectedIDs[i]);
            bool checkState = ListView_GetCheckState(hwndSources, selectedIDs[i]) > 0;
            
            bChangingSources = true;
            ListView_DeleteItem(hwndSources, selectedIDs[i]);
            InsertSourceItem(--selectedIDs[i], (LPWSTR)strName.Array(), checkState);

            if(focusedItem == selectedIDs[i]+1)
                stateFlags = LVIS_SELECTED | LVIS_FOCUSED;
            else
                stateFlags = LVIS_SELECTED;
            if(selectionMark == selectedIDs[i]+1)
                ListView_SetSelectionMark(hwndSources, selectedIDs[i]);

            ListView_SetItemState(hwndSources, selectedIDs[i], stateFlags, stateFlags);
            bChangingSources = false;
            
        }
    }
    THAW_WND(hwndSources);

    ReportSourceOrderChanged();
}

void OBS::MoveSourcesDown()
{
   HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);
    UINT numSelected = ListView_GetSelectedCount(hwndSources);
    int numItems = ListView_GetItemCount(hwndSources);
    int focusedItem = -1, selectionMark;

    List<SceneItem*> selectedSceneItems;
    if(App->scene)
        App->scene->GetSelectedItems(selectedSceneItems);

    List<UINT> selectedIDs;

    selectionMark = ListView_GetSelectionMark(hwndSources);

    //get selected items
    int iPos = ListView_GetNextItem(hwndSources, -1, LVNI_SELECTED);
    while (iPos != -1)
    {
        selectedIDs.Add((UINT) iPos);
        if(ListView_GetItemState(hwndSources, iPos, LVIS_FOCUSED) &  LVIS_FOCUSED)
            focusedItem = iPos;
        iPos = ListView_GetNextItem(hwndSources, iPos, LVNI_SELECTED);
    }

    XElement *sourcesElement = NULL;
    List<XElement*> selectedElements;
    if(numItems)
    {
        sourcesElement = App->sceneElement->GetElement(TEXT("sources"));
        for(UINT i=0; i<selectedIDs.Num(); i++)
            selectedElements << sourcesElement->GetElementByID(selectedIDs[i]);
    }

    UINT lastItem = (UINT)ListView_GetItemCount(hwndSources)-1;
    UINT lastSelectedID = numSelected-1;
    UINT stateFlags;

    FREEZE_WND(hwndSources);
    for(int i=(int)lastSelectedID; i>=0; i--)
    {
        if( (i == lastSelectedID && selectedIDs[i] < lastItem) ||
            (i != lastSelectedID && selectedIDs[i+1] != selectedIDs[i]+1) )
        {
            if(App->scene)
                selectedSceneItems[i]->MoveDown();
            else
                selectedElements[i]->MoveDown();

            String strName = GetLVText(hwndSources, selectedIDs[i]);
            bool checkState = ListView_GetCheckState(hwndSources, selectedIDs[i]) > 0;
            
            bChangingSources = true;
            ListView_DeleteItem(hwndSources, selectedIDs[i]);
            InsertSourceItem(++selectedIDs[i], (LPWSTR)strName.Array(), checkState);

            if(focusedItem == selectedIDs[i]-1)
                stateFlags = LVIS_SELECTED | LVIS_FOCUSED;
            else
                stateFlags = LVIS_SELECTED;
            if(selectionMark == selectedIDs[i]-1)
                ListView_SetSelectionMark(hwndSources, selectedIDs[i]);

            ListView_SetItemState(hwndSources, selectedIDs[i], stateFlags, stateFlags);
            bChangingSources = false;
        }
    }
    THAW_WND(hwndSources);

    ReportSourceOrderChanged();
}

void OBS::MoveSourcesToTop()
{
    HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);
    int numItems = ListView_GetItemCount(hwndSources);
    UINT focusedItem = -1, selectionMark;

    List<SceneItem*> selectedSceneItems;
    if(App->scene)
        App->scene->GetSelectedItems(selectedSceneItems);

    List<UINT> selectedIDs;

    selectionMark = ListView_GetSelectionMark(hwndSources);

    //get selected items
    int iPos = ListView_GetNextItem(hwndSources, -1, LVNI_SELECTED);
    while (iPos != -1)
    {
        selectedIDs.Add((UINT) iPos);
        if(ListView_GetItemState(hwndSources, iPos, LVIS_FOCUSED) &  LVIS_FOCUSED)
            focusedItem = iPos;
        iPos = ListView_GetNextItem(hwndSources, iPos, LVNI_SELECTED);
    }

    XElement *sourcesElement = NULL;
    List<XElement*> selectedElements;
    if(numItems)
    {
        sourcesElement = App->sceneElement->GetElement(TEXT("sources"));
        for(UINT i=0; i<selectedIDs.Num(); i++)
            selectedElements << sourcesElement->GetElementByID(selectedIDs[i]);
    }

    if(App->scene)
    {
        for(int i=(int)selectedSceneItems.Num()-1; i>=0; i--)
            selectedSceneItems[i]->MoveToTop();
    }
    else
    {
        for(int i=(int)selectedElements.Num()-1; i>=0; i--)
            selectedElements[i]->MoveToTop();
    }

    UINT stateFlags;

    FREEZE_WND(hwndSources);
    for(UINT i=0; i<selectedIDs.Num(); i++)
    {
        if(selectedIDs[i] != i)
        {
            String strName = GetLVText(hwndSources, selectedIDs[i]);
            bool checkState = ListView_GetCheckState(hwndSources, selectedIDs[i]) > 0;
            
            bChangingSources = true;
            ListView_DeleteItem(hwndSources, selectedIDs[i]);
            InsertSourceItem(i, (LPWSTR)strName.Array(), checkState);
            
            if(focusedItem == selectedIDs[i])
                stateFlags = LVIS_SELECTED | LVIS_FOCUSED;
            else
                stateFlags = LVIS_SELECTED;
            if(selectionMark == selectedIDs[i])
                ListView_SetSelectionMark(hwndSources, i);

            ListView_SetItemState(hwndSources, i, stateFlags, stateFlags);
            bChangingSources = false;
        }
    }
    THAW_WND(hwndSources);

    ReportSourceOrderChanged();
}

void OBS::MoveSourcesToBottom()
{
    HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);
    int numItems = ListView_GetItemCount(hwndSources);
    UINT focusedItem = -1, selectionMark;

    List<SceneItem*> selectedSceneItems;
    if(App->scene)
        App->scene->GetSelectedItems(selectedSceneItems);

    List<UINT> selectedIDs;

    selectionMark = ListView_GetSelectionMark(hwndSources);

    //get selected items
    int iPos = ListView_GetNextItem(hwndSources, -1, LVNI_SELECTED);
    while (iPos != -1)
    {
        selectedIDs.Add((UINT) iPos);
        if(ListView_GetItemState(hwndSources, iPos, LVIS_FOCUSED) &  LVIS_FOCUSED)
            focusedItem = iPos;
        iPos = ListView_GetNextItem(hwndSources, iPos, LVNI_SELECTED);
    }

    XElement *sourcesElement = NULL;
    List<XElement*> selectedElements;
    if(numItems)
    {
        sourcesElement = App->sceneElement->GetElement(TEXT("sources"));
        for(UINT i=0; i<selectedIDs.Num(); i++)
            selectedElements << sourcesElement->GetElementByID(selectedIDs[i]);
    }

    if(App->scene)
    {
        for(UINT i=0; i<selectedSceneItems.Num(); i++)
            selectedSceneItems[i]->MoveToBottom();
    }
    else
    {
        for(UINT i=0; i<selectedElements.Num(); i++)
            selectedElements[i]->MoveToBottom();
    }

    UINT curID = ListView_GetItemCount(hwndSources)-1;
    UINT stateFlags;

    FREEZE_WND(hwndSources);
    for(int i=int(selectedIDs.Num()-1); i>=0; i--)
    {
        if(selectedIDs[i] != curID)
        {
            String strName = GetLVText(hwndSources, selectedIDs[i]);
            bool checkState = ListView_GetCheckState(hwndSources, selectedIDs[i]) > 0;
            
            bChangingSources = true;
            ListView_DeleteItem(hwndSources, selectedIDs[i]);
            InsertSourceItem(curID, (LPWSTR)strName.Array(), checkState);

            if(focusedItem == selectedIDs[i])
                stateFlags = LVIS_SELECTED | LVIS_FOCUSED;
            else
                stateFlags = LVIS_SELECTED;
            if(selectionMark == selectedIDs[i])
                ListView_SetSelectionMark(hwndSources, curID);

            ListView_SetItemState(hwndSources, curID, stateFlags, stateFlags);
            bChangingSources = false;
        }

        curID--;
    }
    THAW_WND(hwndSources);

    ReportSourceOrderChanged();
}

void OBS::CenterItems(bool horizontal, bool vertical)
{
    if(App->bRunning)
    {
        List<SceneItem*> selectedItems;
        App->scene->GetSelectedItems(selectedItems);

        Vect2 baseSize = App->GetBaseSize();

        for(UINT i=0; i<selectedItems.Num(); i++)
        {
            SceneItem *item = selectedItems[i];

            if (horizontal)
                item->pos.x = (baseSize.x*0.5f)-((item->size.x + item->GetCrop().x - item->GetCrop().w)*0.5f);
            if (vertical)
                item->pos.y = (baseSize.y*0.5f)-((item->size.y + item->GetCrop().y - item->GetCrop().z)*0.5f);

            XElement *itemElement = item->GetElement();
            if (horizontal)
                itemElement->SetInt(TEXT("x"), int(item->pos.x));
            if (vertical)
                itemElement->SetInt(TEXT("y"), int(item->pos.y));
        }
    }
}

void OBS::MoveItemsToEdge(int horizontal, int vertical)
{
    if(App->bRunning)
    {
        List<SceneItem*> selectedItems;
        App->scene->GetSelectedItems(selectedItems);

        Vect2 baseSize = App->GetBaseSize();

        for(UINT i=0; i<selectedItems.Num(); i++)
        {
            SceneItem *item = selectedItems[i];

            if (horizontal == 1)
                item->pos.x = baseSize.x - item->size.x + item->GetCrop().w;
            if (horizontal == -1)
                item->pos.x = -item->GetCrop().x;

            if (vertical == 1)
                item->pos.y =  baseSize.y - item->size.y + item->GetCrop().z;
            if (vertical == -1)
                item->pos.y = -item->GetCrop().y;

            XElement *itemElement = item->GetElement();
            if (horizontal)
                itemElement->SetInt(TEXT("x"), int(item->pos.x));
            if (vertical)
                itemElement->SetInt(TEXT("y"), int(item->pos.y));
        }
    }
}

void OBS::MoveItemsByPixels(int dx, int dy)
{
    if(App->bRunning)
    {
        List<SceneItem*> selectedItems;
        App->scene->GetSelectedItems(selectedItems);

        Vect2 baseSize = App->GetBaseSize();
        Vect2 renderSize = App->GetRenderFrameSize();

        for(UINT i=0; i<selectedItems.Num(); i++)
        {
            SceneItem *item = selectedItems[i];
            item->pos.x += dx;
            item->pos.y += dy;
            if(App->bMouseMoved)
            {
                App->startMousePos.x -= dx * renderSize.x / baseSize.x;
                App->startMousePos.y -= dy * renderSize.y / baseSize.y;
            }

            XElement *itemElement = item->GetElement();
            itemElement->SetInt(TEXT("x"), int(item->pos.x));
            itemElement->SetInt(TEXT("y"), int(item->pos.y));
        }
    }
}

extern "C" double round(double val);

void OBS::FitItemsToScreen()
{
    if(App->bRunning)
    {
        List<SceneItem*> selectedItems;
        App->scene->GetSelectedItems(selectedItems);

        Vect2 baseSize = App->GetBaseSize();
        double baseAspect = double(baseSize.x)/double(baseSize.y);

        for(UINT i=0; i<selectedItems.Num(); i++)
        {
            SceneItem *item = selectedItems[i];
            item->pos = (baseSize*0.5f)-(item->size*0.5f);

            if(item->source)
            {
                Vect2 itemSize = item->source->GetSize();
                itemSize.x -= (item->crop.x + item->crop.w);
                itemSize.y -= (item->crop.y + item->crop.z);

                Vect2 size = baseSize;
                double sourceAspect = double(itemSize.x)/double(itemSize.y);
                if(!CloseDouble(baseAspect, sourceAspect))
                {
                    if(baseAspect < sourceAspect)
                        size.y = float(double(size.x) / sourceAspect);
                    else
                        size.x = float(double(size.y) * sourceAspect);
            
                    size.x = (float)round(size.x);
                    size.y = (float)round(size.y);
                }

                Vect2 scale = itemSize / size;
                size.x += (item->crop.x + item->crop.w) / scale.x;
                size.y += (item->crop.y + item->crop.z) / scale.y;
                item->size = size;

                Vect2 pos;
                pos.x = (baseSize.x*0.5f)-((item->size.x + item->GetCrop().x - item->GetCrop().w)*0.5f);
                pos.y = (baseSize.y*0.5f)-((item->size.y + item->GetCrop().y - item->GetCrop().z)*0.5f);
                pos.x = (float)round(pos.x);
                pos.y = (float)round(pos.y);
                item->pos  = pos;

                XElement *itemElement = item->GetElement();
                itemElement->SetInt(TEXT("x"),  int(pos.x));
                itemElement->SetInt(TEXT("y"),  int(pos.y));
                itemElement->SetInt(TEXT("cx"), int(size.x));
                itemElement->SetInt(TEXT("cy"), int(size.y));
            }
        }
    }
}

void OBS::ResetItemSizes()
{
    if(App->bRunning)
    {
        List<SceneItem*> selectedItems;
        App->scene->GetSelectedItems(selectedItems);

        for(UINT i=0; i<selectedItems.Num(); i++)
        {
            SceneItem *item = selectedItems[i];
            if(item->source)
            {
                item->size = item->source->GetSize();

                XElement *itemElement = item->GetElement();
                itemElement->SetInt(TEXT("cx"), int(item->size.x));
                itemElement->SetInt(TEXT("cy"), int(item->size.y));
            }
        }
    }
}

void OBS::ResetItemCrops()
{
    if(App->bRunning)
    {
        List<SceneItem*> selectedItems;
        App->scene->GetSelectedItems(selectedItems);

        for(UINT i=0; i<selectedItems.Num(); i++)
        {
            SceneItem *item = selectedItems[i];
            if(item->source)
            {
                item->crop = Vect4(0, 0, 0, 0);

                XElement *itemElement = item->GetElement();
                itemElement->SetFloat(TEXT("crop.left"), item->crop.x);
                itemElement->SetFloat(TEXT("crop.top"), item->crop.y);
                itemElement->SetFloat(TEXT("crop.right"), item->crop.w);
                itemElement->SetFloat(TEXT("crop.bottom"), item->crop.z);
            }
        }
    }
}

//----------------------------

INT_PTR CALLBACK OBS::EnterGlobalSourceNameDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)lParam);
                LocalizeWindow(hwnd);

                String &strOut = *(String*)lParam;
                SetWindowText(GetDlgItem(hwnd, IDC_NAME), strOut);
            }
            return TRUE;

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDOK:
                    {
                        String str;
                        str.SetLength((UINT)SendMessage(GetDlgItem(hwnd, IDC_NAME), WM_GETTEXTLENGTH, 0, 0));
                        if(!str.Length())
                        {
                            MessageBox(hwnd, Str("EnterName"), NULL, 0);
                            break;
                        }

                        SendMessage(GetDlgItem(hwnd, IDC_NAME), WM_GETTEXT, str.Length()+1, (LPARAM)str.Array());

                        String &strOut = *(String*)GetWindowLongPtr(hwnd, DWLP_USER);

                        XElement *globals = App->scenesConfig.GetElement(TEXT("global sources"));
                        if(globals)
                        {
                            XElement *foundSource = globals->GetElement(str);
                            if(foundSource != NULL && strOut != foundSource->GetName())
                            {
                                String strExists = Str("NameExists");
                                strExists.FindReplace(TEXT("$1"), str);
                                MessageBox(hwnd, strExists, NULL, 0);
                                break;
                            }
                        }

                        strOut = str;
                    }

                case IDCANCEL:
                    EndDialog(hwnd, LOWORD(wParam));
                    break;
            }
    }
    return 0;
}

INT_PTR CALLBACK OBS::GlobalSourcesProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                LocalizeWindow(hwnd);

                HWND hwndSources = GetDlgItem(hwnd, IDC_SOURCES);
                XElement *globals = App->scenesConfig.GetElement(TEXT("global sources"));
                if(globals)
                {
                    UINT numGlobals = globals->NumElements();

                    for(UINT i=0; i<numGlobals; i++)
                    {
                        XElement *globalSource = globals->GetElementByID(i);
                        SendMessage(hwndSources, LB_ADDSTRING, 0, (LPARAM)globalSource->GetName());
                    }
                }

                return TRUE;
            }

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_ADD:
                    {
                        HMENU hMenu = CreatePopupMenu();

                        for(UINT i=0; i<App->imageSourceClasses.Num(); i++)
                        {
                            if(App->imageSourceClasses[i].strClass != TEXT("GlobalSource"))
                            {
                                if (!App->imageSourceClasses[i].bDeprecated)
                                {
                                    String strAdd = Str("Listbox.Add");
                                    strAdd.FindReplace(TEXT("$1"), App->imageSourceClasses[i].strName);
                                    AppendMenu(hMenu, MF_STRING, i+1, strAdd.Array());
                                }
                            }
                        }

                        POINT p;
                        GetCursorPos(&p);

                        int classID = (int)TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, p.x, p.y, hwndMain, NULL);
                        if(!classID)
                            break;

                        String strName;
                        if(DialogBoxParam(hinstMain, MAKEINTRESOURCE(IDD_ENTERNAME), hwnd, OBS::EnterGlobalSourceNameDialogProc, (LPARAM)&strName) == IDOK)
                        {
                            ClassInfo &ci = App->imageSourceClasses[classID-1];

                            XElement *globals = App->scenesConfig.GetElement(TEXT("global sources"));
                            if(!globals)
                                globals = App->scenesConfig.CreateElement(TEXT("global sources"));

                            XElement *newSourceElement = globals->CreateElement(strName);
                            newSourceElement->SetString(TEXT("class"), ci.strClass);

                            if(ci.configProc)
                            {
                                if(!ci.configProc(newSourceElement, true))
                                {
                                    globals->RemoveElement(newSourceElement);
                                    break;
                                }
                            }

                            SendMessage(GetDlgItem(hwnd, IDC_SOURCES), LB_ADDSTRING, 0, (LPARAM)strName.Array());
                        }

                        break;
                    }

                case IDC_REMOVE:
                    {
                        HWND hwndSources = GetDlgItem(hwnd, IDC_SOURCES);
                        HWND hwndSceneSources = GetDlgItem(hwndMain, ID_SOURCES);

                        UINT id = (UINT)SendMessage(GetDlgItem(hwnd, IDC_SOURCES), LB_GETCURSEL, 0, 0);
                        if(id == LB_ERR)
                            break;

                        XElement *globals = App->scenesConfig.GetElement(TEXT("global sources"));
                        if(!globals)
                            break;

                        if(MessageBox(hwnd, Str("GlobalSources.DeleteConfirm"), Str("DeleteConfirm.Title"), MB_YESNO) == IDNO)
                            break;

                        App->EnterSceneMutex();

                        XElement *element = globals->GetElementByID(id);

                        if(App->bRunning)
                        {
                            for(int i=int(App->scene->sceneItems.Num()-1); i>=0; i--)
                            {
                                SceneItem *item = App->scene->sceneItems[i];
                                if(item->element && scmpi(item->element->GetString(TEXT("class")), TEXT("GlobalSource")) == 0)
                                {
                                    XElement *data = item->element->GetElement(TEXT("data"));
                                    if(data)
                                    {
                                        if(scmpi(data->GetString(TEXT("name")), element->GetName()) == 0)
                                        {
                                            LVFINDINFO findInfo;
                                            findInfo.flags = LVFI_STRING;
                                            findInfo.psz = (LPCWSTR) element->GetName();

                                            int listID = ListView_FindItem(hwndSceneSources, -1, &findInfo);
                                            if(listID != -1)
                                            {
                                                App->bChangingSources = true;
                                                ListView_DeleteItem(hwndSceneSources, listID);
                                                App->bChangingSources = false;
                                            }
                                            App->scene->RemoveImageSource(item);
                                        }
                                    }
                                }
                            }
                        }

                        XElement *scenes = App->scenesConfig.GetElement(TEXT("scenes"));
                        if(scenes)
                        {
                            UINT numScenes = scenes->NumElements();

                            for(UINT i=0; i<numScenes; i++)
                            {
                                XElement *scene = scenes->GetElementByID(i);
                                XElement *sources = scene->GetElement(TEXT("sources"));
                                if(sources)
                                {
                                    UINT numSources = sources->NumElements();

                                    for(int j=int(numSources-1); j>=0; j--)
                                    {
                                        XElement *source = sources->GetElementByID(j);

                                        if(scmpi(source->GetString(TEXT("class")), TEXT("GlobalSource")) == 0)
                                        {
                                            XElement *data = source->GetElement(TEXT("data"));
                                            if(data)
                                            {
                                                CTSTR lpName = data->GetString(TEXT("name"));
                                                if(scmpi(lpName, element->GetName()) == 0)
                                                {
                                                    LVFINDINFO findInfo;
                                                    findInfo.flags = LVFI_STRING;
                                                    findInfo.psz = (LPCWSTR) source->GetName();

                                                    int listID = ListView_FindItem(hwndSceneSources, -1, &findInfo);
                                                    if(listID != -1)
                                                    {
                                                        App->bChangingSources = true;
                                                        ListView_DeleteItem(hwndSceneSources, listID);
                                                        App->bChangingSources = false;
                                                    }
                                                    sources->RemoveElement(source);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        SendMessage(hwndSources, LB_DELETESTRING, id, 0);

                        if(App->bRunning)
                        {
                            for(UINT i=0; i<App->globalSources.Num(); i++)
                            {
                                GlobalSourceInfo &info = App->globalSources[i];
                                if(info.strName.CompareI(element->GetName()) && info.source)
                                {
                                    info.FreeData();
                                    App->globalSources.Remove(i);
                                    break;
                                }
                            }
                        }

                        globals->RemoveElement(element);

                        App->LeaveSceneMutex();
                        break;
                    }

                case IDC_RENAME:
                    {
                        HWND hwndSources = GetDlgItem(hwnd, IDC_SOURCES);
                        UINT id = (UINT)SendMessage(hwndSources, LB_GETCURSEL, 0, 0);
                        if(id == LB_ERR)
                            break;

                        XElement *globals = App->scenesConfig.GetElement(TEXT("global sources"));
                        if(!globals)
                            break;

                        XElement *element = globals->GetElementByID(id);

                        String strName = element->GetName();
                        if(DialogBoxParam(hinstMain, MAKEINTRESOURCE(IDD_ENTERNAME), hwndMain, OBS::EnterGlobalSourceNameDialogProc, (LPARAM)&strName) == IDOK)
                        {
                            SendMessage(hwndSources, LB_DELETESTRING, id, 0);
                            SendMessage(hwndSources, LB_INSERTSTRING, id, (LPARAM)strName.Array());
                            SendMessage(hwndSources, LB_SETCURSEL, id, 0);

                            XElement *scenes = App->scenesConfig.GetElement(TEXT("scenes"));
                            if(scenes)
                            {
                                UINT numScenes = scenes->NumElements();

                                for(UINT i=0; i<numScenes; i++)
                                {
                                    XElement *sceneElement = scenes->GetElementByID(i);
                                    XElement *sources = sceneElement->GetElement(TEXT("sources"));
                                    if(sources)
                                    {
                                        UINT numSources = sources->NumElements();

                                        for(UINT j=0; j<numSources; j++)
                                        {
                                            XElement *source = sources->GetElementByID(j);

                                            if(scmpi(source->GetString(TEXT("class")), TEXT("GlobalSource")) == 0)
                                            {
                                                XElement *data = source->GetElement(TEXT("data"));
                                                if(data)
                                                {
                                                    CTSTR lpName = data->GetString(TEXT("name"));
                                                    if(scmpi(lpName, element->GetName()) == 0)
                                                        data->SetString(TEXT("name"), strName);
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            element->SetName(strName);
                        }

                        break;
                    }

                case IDC_CONFIG:
                    {
                        UINT id = (UINT)SendMessage(GetDlgItem(hwnd, IDC_SOURCES), LB_GETCURSEL, 0, 0);
                        if(id == LB_ERR)
                            break;

                        XElement *globals = App->scenesConfig.GetElement(TEXT("global sources"));
                        if(!globals)
                            break;

                        XElement *element = globals->GetElementByID(id);
                        CTSTR lpClass = element->GetString(TEXT("class"));

                        ClassInfo *imageSourceClass = App->GetImageSourceClass(lpClass);
                        if(imageSourceClass && imageSourceClass->configProc && imageSourceClass->configProc(element, false))
                        {
                            App->EnterSceneMutex();

                            if(App->bRunning)
                            {
                                for(UINT i=0; i<App->scene->sceneItems.Num(); i++)
                                {
                                    SceneItem *item = App->scene->sceneItems[i];
                                    if(item->element && scmpi(item->element->GetString(TEXT("class")), TEXT("GlobalSource")) == 0)
                                    {
                                        XElement *data = item->element->GetElement(TEXT("data"));
                                        if(data)
                                        {
                                            if(scmpi(data->GetString(TEXT("name")), element->GetName()) == 0)
                                            {
                                                if(App->bRunning)
                                                {
                                                    for(UINT i=0; i<App->globalSources.Num(); i++)
                                                    {
                                                        GlobalSourceInfo &info = App->globalSources[i];
                                                        if(info.strName.CompareI(element->GetName()) && info.source)
                                                            info.source->UpdateSettings();
                                                    }
                                                }

                                                item->Update();
                                            }
                                        }
                                    }
                                }
                            }

                            App->LeaveSceneMutex();
                        }

                        break;
                    }

                /*case IDC_MOVEUP:
                case IDC_MOVEDOWN:
                case IDC_MOVETOTOP:
                case IDC_MOVETOBOTTOM:
                    {
                        HWND hwndSources = GetDlgItem(hwnd, IDC_SOURCES);

                        UINT id = (UINT)SendMessage(hwndSources, LB_GETCURSEL, 0, 0);
                        if(id == LB_ERR)
                            break;

                        XElement *globals = App->scenesConfig.GetElement(TEXT("global sources"));
                        if(!globals)
                            break;

                        XElement *element = globals->GetElementByID(id);
                        UINT numElements = globals->NumElements();

                        if(LOWORD(wParam) == IDC_MOVEUP)
                        {
                            if(id != 0)
                            {
                                SendMessage(hwndSources, LB_DELETESTRING, id--, 0);
                                SendMessage(hwndSources, LB_INSERTSTRING, id, (LPARAM)element->GetName());
                                PostMessage(hwndSources, LB_SETCURSEL, id, 0);

                                element->MoveUp();
                            }
                        }
                        else if(LOWORD(wParam) == IDC_MOVEDOWN)
                        {
                            if(id != (numElements-1))
                            {
                                SendMessage(hwndSources, LB_DELETESTRING, id++, 0);
                                SendMessage(hwndSources, LB_INSERTSTRING, id, (LPARAM)element->GetName());
                                PostMessage(hwndSources, LB_SETCURSEL, id, 0);

                                element->MoveDown();
                            }
                        }
                        else if(LOWORD(wParam) == IDC_MOVETOTOP)
                        {
                            if(id != 0)
                            {
                                SendMessage(hwndSources, LB_DELETESTRING, id, 0);
                                SendMessage(hwndSources, LB_INSERTSTRING, 0, (LPARAM)element->GetName());
                                PostMessage(hwndSources, LB_SETCURSEL, 0, 0);

                                element->MoveToTop();
                            }
                        }
                        else
                        {
                            if(id != (numElements-1))
                            {
                                SendMessage(hwndSources, LB_DELETESTRING, id, 0);
                                SendMessage(hwndSources, LB_INSERTSTRING, (numElements-1), (LPARAM)element->GetName());
                                PostMessage(hwndSources, LB_SETCURSEL, (numElements-1), 0);

                                element->MoveToBottom();
                            }
                        }
                        break;
                    }*/

                case IDOK:
                    EndDialog(hwnd, IDOK);
            }
            break;

        case WM_CLOSE:
            EndDialog(hwnd, IDOK);
    }

    return FALSE;
}

//----------------------------

struct ReconnectInfo
{
    UINT_PTR timerID;
    UINT secondsLeft;
};

INT_PTR CALLBACK OBS::ReconnectDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                LocalizeWindow(hwnd);

                ReconnectInfo *ri = new ReconnectInfo;
                ri->secondsLeft = App->reconnectTimeout;
                ri->timerID = 1;

                if(!SetTimer(hwnd, 1, 1000, NULL))
                {
                    App->bReconnecting = false;
                    EndDialog(hwnd, IDCANCEL);
                    delete ri;
                }

                String strText;
                if(App->bReconnecting)
                    strText << Str("Reconnecting.Retrying") << UIntString(ri->secondsLeft);
                else
                    strText << Str("Reconnecting") << UIntString(ri->secondsLeft);

                SetWindowText(GetDlgItem(hwnd, IDC_RECONNECTING), strText);

                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)ri);
                return TRUE;
            }

        case WM_TIMER:
            {
                ReconnectInfo *ri = (ReconnectInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                if(wParam != 1)
                    break;

                if(!--ri->secondsLeft)
                {
                    SendMessage(hwndMain, OBS_RECONNECT, 0, 0);
                    EndDialog(hwnd, IDOK);
                }
                else
                {
                    String strText;
                    if(App->bReconnecting)
                        strText << Str("Reconnecting.Retrying") << UIntString(ri->secondsLeft);
                    else
                        strText << Str("Reconnecting") << UIntString(ri->secondsLeft);

                    SetWindowText(GetDlgItem(hwnd, IDC_RECONNECTING), strText);
                }
                break;
            }

        case WM_COMMAND:
            if(LOWORD(wParam) == IDCANCEL)
            {
                App->bReconnecting = false;
                EndDialog(hwnd, IDCANCEL);
            }
            break;

        case WM_CLOSE:
            App->bReconnecting = false;
            EndDialog(hwnd, IDCANCEL);
            break;

        case WM_DESTROY:
            {
                ReconnectInfo *ri = (ReconnectInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                KillTimer(hwnd, ri->timerID);
                delete ri;
            }
    }

    return FALSE;
}

//----------------------------

void OBS::AddProfilesToMenu(HMENU menu)
{
    StringList profileList;
    GetProfiles(profileList);
    for(UINT i=0; i<profileList.Num(); i++)
    {
        String &strProfile = profileList[i];

        UINT flags = MF_STRING;
        if(strProfile.CompareI(GetCurrentProfile()))
            flags |= MF_CHECKED;

        AppendMenu(menu, flags, ID_SWITCHPROFILE+i, strProfile.Array());
    }
}

//----------------------------

void OBS::ResetProfileMenu()
{
    HMENU hmenuMain = GetMenu(hwndMain);
    HMENU hmenuProfiles = GetSubMenu(hmenuMain, 2);
    while(DeleteMenu(hmenuProfiles, 0, MF_BYPOSITION));
    AddProfilesToMenu(hmenuProfiles);
}

//----------------------------

String OBS::GetApplicationName()
{
    String name;
    name << OBS_VERSION_STRING << TEXT(" -profile: ") << App->GetCurrentProfile();
    return name;
}

//----------------------------

void OBS::ResetApplicationName()
{
    SetWindowText(hwndMain, GetApplicationName());
}

//----------------------------

LRESULT CALLBACK OBS::OBSProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case ID_SETTINGS_SETTINGS:
                case ID_SETTINGS:
                    DialogBox(hinstMain, MAKEINTRESOURCE(IDD_SETTINGS), hwnd, (DLGPROC)OBS::SettingsDialogProc);
                    break;

                case ID_GLOBALSOURCES:
                    DialogBox(hinstMain, MAKEINTRESOURCE(IDD_GLOBAL_SOURCES), hwnd, (DLGPROC)OBS::GlobalSourcesProc);
                    break;

                case ID_FILE_SAVE2:
                    App->scenesConfig.Save();
                    break;

                case ID_FILE_EXIT:
                case ID_EXIT:
                    PostQuitMessage(0);
                    break;

                case ID_ALWAYSONTOP:
                    {
                        HMENU hmenuBar = GetMenu(hwnd); 
                        App->bAlwaysOnTop = !App->bAlwaysOnTop;
                        CheckMenuItem(hmenuBar, ID_ALWAYSONTOP, (App->bAlwaysOnTop)?MF_CHECKED:MF_UNCHECKED);
                        
                        SetWindowPos(hwndMain, (App->bAlwaysOnTop)?HWND_TOPMOST:HWND_NOTOPMOST, 0, 0, 0, 0,
                            SWP_NOSIZE | SWP_NOMOVE);
                    }
                    break;

                case ID_FULLSCREENMODE:
                    App->SetFullscreenMode(!App->bFullscreenMode);
                    break;

                case ID_HELP_VISITWEBSITE:
                    ShellExecute(NULL, TEXT("open"), TEXT("http://www.obsproject.com"), 0, 0, SW_SHOWNORMAL);
                    break;

                case ID_HELP_CONTENTS:
                    {
                        String strHelpPath;
                        UINT dirSize = GetCurrentDirectory(0, 0);
                        strHelpPath.SetLength(dirSize);
                        GetCurrentDirectory(dirSize, strHelpPath.Array());

                        strHelpPath << TEXT("\\OBSHelp.chm");

                        ShellExecute(NULL, TEXT("open"), strHelpPath, 0, 0, SW_SHOWNORMAL);
                    }
                    break;

                case ID_DASHBOARD:
                    ShellExecute(NULL, TEXT("open"), App->strDashboard, 0, 0, SW_SHOWNORMAL);
                    break;

                case ID_SETTINGS_OPENCONFIGFOLDER:
                    {
                        String strAppPath = API->GetAppDataPath();
                        ShellExecute(NULL, TEXT("open"), strAppPath, 0, 0, SW_SHOWNORMAL);
                    }
                    break;

                case ID_SETTINGS_OPENLOGFOLDER:
                    {
                        String strAppPath = API->GetAppDataPath();
                        strAppPath << TEXT("\\logs");
                        ShellExecute(NULL, TEXT("open"), strAppPath, 0, 0, SW_SHOWNORMAL);
                    }
                    break;

                case ID_PLUGINS:
                    DialogBox(hinstMain, MAKEINTRESOURCE(IDD_PLUGINS), hwnd, (DLGPROC)OBS::PluginsDialogProc);
                    break;

                case ID_MICVOLUME:
                    if(HIWORD(wParam) == VOLN_ADJUSTING || HIWORD(wParam) == VOLN_FINALVALUE)
                    {
                        if(IsWindowEnabled((HWND)lParam))
                        {
                            App->micVol = GetVolumeControlValue((HWND)lParam);

                            bool finalValue = HIWORD(wParam) == VOLN_FINALVALUE;

                            App->ReportMicVolumeChange(App->micVol, App->micVol < VOLN_MUTELEVEL, finalValue);

                            if(App->micVol < EPSILON)
                                App->micVol = 0.0f;

                            if(finalValue)
                                AppConfig->SetFloat(TEXT("Audio"), TEXT("MicVolume"), App->micVol);
                        }
                    }
                    break;

                case ID_DESKTOPVOLUME:
                    if(HIWORD(wParam) == VOLN_ADJUSTING || HIWORD(wParam) == VOLN_FINALVALUE)
                    {
                        if(IsWindowEnabled((HWND)lParam))
                        {
                            App->desktopVol = GetVolumeControlValue((HWND)lParam);
                            
                            bool finalValue = HIWORD(wParam) == VOLN_FINALVALUE;

                            App->ReportDesktopVolumeChange(App->desktopVol, App->desktopVol < VOLN_MUTELEVEL, finalValue);

                            if(App->desktopVol < EPSILON)
                                App->desktopVol = 0.0f;

                            if(finalValue)
                                AppConfig->SetFloat(TEXT("Audio"), TEXT("DesktopVolume"), App->desktopVol);

                            
                        }
                    }
                    break;
                case ID_DESKTOPVOLUMEMETER:
                case ID_MICVOLUMEMETER:
                    if(HIWORD(wParam) == VOLN_METERED)
                    {
                        App->UpdateAudioMeters();
                    }
                    break;
                case ID_SCENEEDITOR:
                    if(HIWORD(wParam) == BN_CLICKED)
                        App->bEditMode = !App->bEditMode;
                    break;

                case ID_SCENES:
                    if(HIWORD(wParam) == LBN_SELCHANGE)
                    {
                        HWND hwndScenes = (HWND)lParam;
                        UINT id = (UINT)SendMessage(hwndScenes, LB_GETCURSEL, 0, 0);
                        if(id != LB_ERR)
                        {
                            String strName;
                            strName.SetLength((UINT)SendMessage(hwndScenes, LB_GETTEXTLEN, id, 0));
                            SendMessage(hwndScenes, LB_GETTEXT, id, (LPARAM)strName.Array());
                            App->SetScene(strName);
                        }
                    }
                    break;

                case ID_TESTSTREAM:
                    App->bTestStream = true;
                    App->ToggleCapturing();
                    break;

                case ID_STARTSTOP:
                    EnableWindow(GetDlgItem(hwnd, ID_STARTSTOP), false);
                    App->ToggleCapturing();
                    EnableWindow(GetDlgItem(hwnd, ID_STARTSTOP), true);
                    break;

                case ID_MINIMIZERESTORE:
                    {
                        bool bMinimizeToNotificationArea = AppConfig->GetInt(TEXT("General"), TEXT("MinimizeToNotificationArea"), 0) != 0;
                        if (bMinimizeToNotificationArea)
                        {
                            if (IsWindowVisible(hwnd))
                            {
                                ShowWindow(hwnd, SW_MINIMIZE);
                                ShowWindow(hwnd, SW_HIDE);
                            }
                            else
                            {
                                ShowWindow(hwnd, SW_SHOW);
                                ShowWindow(hwnd, SW_RESTORE);
                            }
                        }
                        else
                        {
                            if (IsIconic(hwnd))
                                ShowWindow(hwnd, SW_RESTORE);
                            else
                                ShowWindow(hwnd, SW_MINIMIZE);
                        }
                    }
                    break;

                case IDA_SOURCE_DELETE:
                    App->DeleteItems();
                    break;

                case IDA_SOURCE_CENTER:
                    App->CenterItems(true, true);
                    break;

                case IDA_SOURCE_CENTER_VER:
                    App->CenterItems(false, true);
                    break;

                case IDA_SOURCE_CENTER_HOR:
                    App->CenterItems(true, false);
                    break;

                case IDA_SOURCE_LEFT_CANVAS:
                    App->MoveItemsToEdge(-1, 0);
                    break;

                case IDA_SOURCE_TOP_CANVAS:
                    App->MoveItemsToEdge(0, -1);
                    break;

                case IDA_SOURCE_RIGHT_CANVAS:
                    App->MoveItemsToEdge(1, 0);
                    break;

                case IDA_SOURCE_BOTTOM_CANVAS:
                    App->MoveItemsToEdge(0, 1);
                    break;

                case IDA_SOURCE_FITTOSCREEN:
                    App->FitItemsToScreen();
                    break;

                case IDA_SOURCE_RESETSIZE:
                    App->ResetItemSizes();
                    break;

                case IDA_SOURCE_RESETCROP:
                    App->ResetItemCrops();
                    break;

                case IDA_SOURCE_MOVEUP:
                    App->MoveSourcesUp();
                    break;

                case IDA_SOURCE_MOVEDOWN:
                    App->MoveSourcesDown();
                    break;

                case IDA_SOURCE_MOVETOTOP:
                    App->MoveSourcesToTop();
                    break;

                case IDA_SOURCE_MOVETOBOTTOM:
                    App->MoveSourcesToBottom();
                    break;

                case IDCANCEL:
                    // This actually means the user pressed the ESC key
                    if(App->bFullscreenMode)
                        App->SetFullscreenMode(false);
                    break;

                default:
                    {
                        UINT id = LOWORD(wParam);
                        if (id >= ID_SWITCHPROFILE && 
                            id <= ID_SWITCHPROFILE+500)
                        {
                            MENUITEMINFO mii;
                            zero(&mii, sizeof(mii));
                            mii.cbSize = sizeof(mii);
                            mii.fMask = MIIM_STRING;

                            HMENU hmenuMain = GetMenu(hwndMain);
                            HMENU hmenuProfiles = GetSubMenu(hmenuMain, 2);
                            GetMenuItemInfo(hmenuProfiles, id, FALSE, &mii);

                            String strProfile;
                            strProfile.SetLength(mii.cch++);
                            mii.dwTypeData = strProfile.Array();

                            GetMenuItemInfo(hmenuProfiles, id, FALSE, &mii);

                            if(!strProfile.CompareI(GetCurrentProfile()))
                            {
                                String strProfilePath;
                                strProfilePath << lpAppDataPath << TEXT("\\profiles\\") << strProfile << TEXT(".ini");

                                if(!AppConfig->Open(strProfilePath))
                                {
                                    MessageBox(hwnd, TEXT("Error - unable to open ini file"), NULL, 0);
                                    break;
                                }

                                GlobalConfig->SetString(TEXT("General"), TEXT("Profile"), strProfile);
                                App->ReloadIniSettings();
                                ResetProfileMenu();
                                ResetApplicationName();
                            }
                        }
                    }
            }
            break;

        case WM_NOTIFY:
            {
                NMHDR nmh = *(LPNMHDR)lParam;

                switch(wParam)
                {
                    case ID_SOURCES:
                        if(nmh.code == NM_CUSTOMDRAW)
                        {
                            LPNMLVCUSTOMDRAW  lplvcd = (LPNMLVCUSTOMDRAW)lParam;
                            switch( lplvcd->nmcd.dwDrawStage)
                            {
                                case CDDS_ITEMPREPAINT:

                                    int state, bkMode;
                                    BOOL checkState;
                                    RECT iconRect, textRect, itemRect;
                                    COLORREF oldTextColor;

                                    // It seems there's a limitation to ListView's displayed max text length http://support.microsoft.com/default.aspx?scid=KB;EN-US;321104
                                    // Read the comment below.
                                    String itemText;

                                    HDC hdc = lplvcd->nmcd.hdc;
                                    int itemId = (int)lplvcd->nmcd.dwItemSpec;

                                    XElement *sources, *sourcesElement;

                                    ListView_GetItemRect(nmh.hwndFrom,itemId, &itemRect, LVIR_BOUNDS);
                                    ListView_GetItemRect(nmh.hwndFrom,itemId, &textRect, LVIR_LABEL);

                                    iconRect.right = textRect.left - 1;
                                    iconRect.left = iconRect.right - (textRect.bottom - textRect.top);
                                    iconRect.top  = textRect.top + 2;
                                    iconRect.bottom = textRect.bottom - 2;

                                    state = ListView_GetItemState(nmh.hwndFrom, itemId, LVIS_SELECTED);
                                    checkState = ListView_GetCheckState(nmh.hwndFrom, itemId);

                                    oldTextColor = GetTextColor(hdc);

                                    if(state&LVIS_SELECTED)
                                    {
                                        FillRect(hdc, &itemRect, (HBRUSH)(COLOR_HIGHLIGHT + 1));
                                        SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
                                    }
                                    else
                                    {
                                        FillRect(hdc, &itemRect, (HBRUSH)(COLOR_WINDOW + 1));
                                        SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
                                    }
                                   

                                    HTHEME hTheme = OpenThemeData(hwnd, TEXT("BUTTON"));
                                    
                                    if(hTheme)
                                    {
                                        if(checkState)
                                            DrawThemeBackground(hTheme, hdc, BP_CHECKBOX, (state&LVIS_SELECTED)?CBS_CHECKEDPRESSED:CBS_CHECKEDNORMAL, &iconRect, NULL);
                                        else
                                            DrawThemeBackground(hTheme, hdc, BP_CHECKBOX, CBS_UNCHECKEDNORMAL, &iconRect, NULL);
                                        CloseThemeData(hTheme);
                                    }
                                    else
                                    {
                                        iconRect.right = iconRect.left + iconRect.bottom - iconRect.top;
                                        if(checkState)
                                            DrawFrameControl(hdc,&iconRect, DFC_BUTTON, DFCS_BUTTONCHECK | DFCS_CHECKED | DFCS_FLAT);
                                        else
                                            DrawFrameControl(hdc,&iconRect, DFC_BUTTON, DFCS_BUTTONCHECK | DFCS_FLAT);
                                    }

                                    textRect.left += 2;

                                    // Not happy about this at all , wanted it to be generic (as a  simple ListView_GetItemText should suffice if we knew max text length), 
                                    // but sending LVM_GETITEMTEXT msg to get the text length seems confusing (for me) by MSDN doc.
                                    // We can use ListView_GetItemText with a MAX_PATH sized buffer instead of the following stuff (see the MSDN link above).

                                    sources = App->sceneElement->GetElement(TEXT("sources"));

                                    if(sources)
                                    {
                                        sourcesElement = sources->GetElementByID(itemId);
                                        if(sourcesElement)
                                        {
                                            itemText = sourcesElement->GetName();
                                            if(itemText.IsValid())
                                            {
                                                bkMode = SetBkMode(hdc, TRANSPARENT);
                                                DrawText(hdc, itemText, slen(itemText), &textRect, DT_LEFT | DT_END_ELLIPSIS | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX );
                                                SetBkMode(hdc, bkMode);
                                            }
                                        }
                                    }

                                    SetTextColor(hdc, oldTextColor);

                                    return CDRF_SKIPDEFAULT;
                            }

                            return CDRF_NOTIFYPOSTPAINT | CDRF_NOTIFYITEMDRAW;
                        }

                        if(nmh.code == LVN_ITEMCHANGED && !App->bChangingSources)
                        {
                            NMLISTVIEW pnmv = *(LPNMLISTVIEW)lParam;
                            if((pnmv.uOldState & LVIS_SELECTED) != (pnmv.uNewState & LVIS_SELECTED))
                            {
                                /*item selected*/
                                App->SelectSources();
                            }
                            if((pnmv.uOldState & LVIS_STATEIMAGEMASK) != (pnmv.uNewState & LVIS_STATEIMAGEMASK))
                            {
                                //checks changed
                                App->CheckSources();
                            }
                        }
                        else if(nmh.code == NM_DBLCLK)
                        {
                            NMITEMACTIVATE itemActivate = *(LPNMITEMACTIVATE)lParam;
                            UINT selectedID = itemActivate.iItem;

                            /* check to see if the double click is on top of the checkbox
                               if so, forget about it */
                            LVHITTESTINFO hitInfo;
                            hitInfo.pt = itemActivate.ptAction;
                            ListView_HitTestEx(nmh.hwndFrom, &hitInfo);
                            if(hitInfo.flags & LVHT_ONITEMSTATEICON)
                                break;
                            
                            if(!App->sceneElement)
                                break;

                            XElement *sourcesElement = App->sceneElement->GetElement(TEXT("sources"));
                            if(!sourcesElement)
                                break;

                            XElement *selectedElement = sourcesElement->GetElementByID(selectedID);
                            if(!selectedElement)
                                break;

                            ClassInfo *curClassInfo = App->GetImageSourceClass(selectedElement->GetString(TEXT("class")));
                            if(curClassInfo && curClassInfo->configProc)
                            {
                                Vect2 multiple;
                                ImageSource *source;

                                App->EnableSceneSwitching(false);

                                if(App->bRunning && App->scene)
                                {
                                    SceneItem* selectedItem = App->scene->GetSceneItem(selectedID);
                                    source = selectedItem->GetSource();
                                    if(source)
                                    {
                                        Vect2 curSize = Vect2(selectedElement->GetFloat(TEXT("cx"), 32.0f), selectedElement->GetFloat(TEXT("cy"), 32.0f));
                                        Vect2 baseSize = source->GetSize();

                                        multiple = curSize/baseSize;
                                    }
                                }

                                if(curClassInfo->configProc(selectedElement, false))
                                {
                                    if(App->bRunning)
                                    {
                                        App->EnterSceneMutex();

                                        if(App->scene)
                                        {
                                            Vect2 newSize = Vect2(selectedElement->GetFloat(TEXT("cx"), 32.0f), selectedElement->GetFloat(TEXT("cy"), 32.0f));
                                            newSize *= multiple;

                                            selectedElement->SetFloat(TEXT("cx"), newSize.x);
                                            selectedElement->SetFloat(TEXT("cy"), newSize.y);

                                            SceneItem* selectedItem = App->scene->GetSceneItem(selectedID);
                                            if(selectedItem->GetSource())
                                                selectedItem->GetSource()->UpdateSettings();
                                            selectedItem->Update();
                                        }

                                        App->LeaveSceneMutex();
                                    }
                                }

                                App->EnableSceneSwitching(true);
                            }
                        }
                        break;
                }
            }
            break;

        case WM_ENTERSIZEMOVE:
            App->bDragResize = true;
            break;

        case WM_EXITSIZEMOVE:
            if(App->bSizeChanging)
            {
                RECT client;
                GetClientRect(hwnd, &client);
                App->ResizeWindow(true);
                App->bSizeChanging = false;
            }
            App->bDragResize = false;
            break;

        case WM_DRAWITEM:
            if(wParam == ID_STATUS)
            {
                DRAWITEMSTRUCT &dis = *(DRAWITEMSTRUCT*)lParam; //don't dis me bro
                App->DrawStatusBar(dis);
            }
            break;

        case WM_SIZING:
            {
                RECT &screenSize = *(RECT*)lParam;

                int newWidth  = MAX(screenSize.right  - screenSize.left, minClientWidth+App->borderXSize);
                int newHeight = MAX(screenSize.bottom - screenSize.top , minClientHeight+App->borderYSize);

                /*int maxCX = GetSystemMetrics(SM_CXFULLSCREEN);
                int maxCY = GetSystemMetrics(SM_CYFULLSCREEN);

                if(newWidth > maxCX)
                    newWidth = maxCX;
                if(newHeight > maxCY)
                    newHeight = maxCY;*/

                if(wParam == WMSZ_LEFT || wParam == WMSZ_BOTTOMLEFT || wParam == WMSZ_TOPLEFT)
                    screenSize.left = screenSize.right - newWidth;
                else
                    screenSize.right = screenSize.left + newWidth;

                if(wParam == WMSZ_BOTTOM || wParam == WMSZ_BOTTOMLEFT || wParam == WMSZ_BOTTOMRIGHT)
                    screenSize.bottom = screenSize.top + newHeight;
                else
                    screenSize.top = screenSize.bottom - newHeight;

                return TRUE;
            }

        case WM_SIZE:
            {
                RECT client;
                GetClientRect(hwnd, &client);

                if(wParam != SIZE_MINIMIZED && (App->clientWidth != client.right || App->clientHeight != client.bottom))
                {
                    App->clientWidth  = client.right;
                    App->clientHeight = client.bottom;
                    App->bSizeChanging = true;

                    if(wParam == SIZE_MAXIMIZED)
                        App->ResizeWindow(true);
                    else
                        App->ResizeWindow(!App->bDragResize);

                    if(!App->bDragResize)
                        App->bSizeChanging = false;
                }
                else if (wParam == SIZE_MINIMIZED && AppConfig->GetInt(TEXT("General"), TEXT("MinimizeToNotificationArea"), 0) != 0)
                {
                    ShowWindow(hwnd, SW_HIDE);
                }
                break;
            }

        case OBS_REQUESTSTOP:
            if (!App->IsRunning())
                break;

            App->Stop();

            if((App->bFirstConnect && App->totalStreamTime < 10000) || !App->bAutoReconnect)
            {
                if(App->streamReport.IsValid())
                {
                    MessageBox(hwndMain, App->streamReport.Array(), Str("StreamReport"), MB_ICONEXCLAMATION);
                    App->streamReport.Clear();
                }
                else
                    MessageBox(hwnd, Str("Connection.Disconnected"), NULL, MB_ICONEXCLAMATION);
            }
            else if(wParam == 0)
            {
                //FIXME: show some kind of non-modal notice to the user explaining why they got disconnected
                //status bar would be nice, but that only renders when we're streaming.
                PlaySound((LPCTSTR)SND_ALIAS_SYSTEMASTERISK, NULL, SND_ALIAS_ID | SND_ASYNC);
                App->bReconnecting = false;
                DialogBox(hinstMain, MAKEINTRESOURCE(IDD_RECONNECTING), hwnd, OBS::ReconnectDialogProc);
            }

            break;

        case OBS_CALLHOTKEY:
            App->CallHotkey((DWORD)lParam, wParam != 0);
            break;

        case OBS_RECONNECT:
            App->bReconnecting = true;
            App->Start();
            break;

        case OBS_SETSCENE:
            {
                TSTR lpScene = (TSTR)lParam;
                App->SetScene(lpScene);
                Free(lpScene);
                break;
            }
        case OBS_SETSOURCEORDER:
            {
                StringList *order = (StringList*)lParam;
                App->SetSourceOrder(*order);
                delete(order);
                break;
            }
        case OBS_SETSOURCERENDER:
            {
                bool render = lParam > 0;
                CTSTR sourceName = (CTSTR) wParam;
                App->SetSourceRender(sourceName, render);
                Free((void *)sourceName);
                break;
            }
        case OBS_UPDATESTATUSBAR:
            App->SetStatusBarData();
            break;

        case OBS_NOTIFICATIONAREA:
            // the point is to only perform the show/hide (minimize) or the menu creation if no modal dialogs are opened
            // if a modal dialog is topmost, then simply focus it
            switch (lParam)
            {
                case WM_LBUTTONUP:
                    if (IsWindowEnabled(hwnd))
                        PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(ID_MINIMIZERESTORE, 0), 0);
                    else
                        SetForegroundWindow(GetWindow(hwnd, GW_ENABLEDPOPUP));
                    break;
                case WM_RBUTTONUP:
                    if (IsWindowEnabled(hwnd))
                    {
                        HMENU hMenu = CreatePopupMenu();
                        if (AppConfig->GetInt(TEXT("General"), TEXT("MinimizeToNotificationArea"), 0) != 0)
                        {
                            AppendMenu(hMenu, MF_STRING, ID_MINIMIZERESTORE, IsWindowVisible(hwnd) ? Str("Settings.General.HideObs") : Str("Settings.General.ShowObs"));
                            AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
                        }
                        AddProfilesToMenu(hMenu);
                        AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
                        if (App->bRunning)
                        {
                            if (App->bTestStream)
                                AppendMenu(hMenu, MF_STRING, ID_TESTSTREAM, Str("MainWindow.StopTest"));
                            else
                                AppendMenu(hMenu, MF_STRING, ID_STARTSTOP, Str("MainWindow.StopStream"));
                        }
                        else
                            AppendMenu(hMenu, MF_STRING, ID_STARTSTOP, Str("MainWindow.StartStream"));
                        if (!IsIconic(hwnd))
                            AppendMenu(hMenu, MF_STRING | (App->bFullscreenMode ? MF_CHECKED : 0), ID_FULLSCREENMODE, Str("MainMenu.Settings.FullscreenMode"));
                        AppendMenu(hMenu, MF_STRING | (App->bAlwaysOnTop ? MF_CHECKED : 0), ID_ALWAYSONTOP, Str("MainMenu.Settings.AlwaysOnTop"));
                        AppendMenu(hMenu, MF_STRING, ID_FILE_EXIT, Str("MainWindow.Exit"));
                    
                        SetForegroundWindow(hwnd);
                        POINT p;
                        GetCursorPos(&p);
                        TrackPopupMenu(hMenu, TPM_LEFTALIGN, p.x, p.y, 0, hwnd, NULL);
                        DestroyMenu(hMenu);
                    }
                    else
                        SetForegroundWindow(GetWindow(hwnd, GW_ENABLEDPOPUP));
                    break;
            }
            break;

        case WM_CLOSE:
            PostQuitMessage(0);
            break;

        default:
            if (App && message == App->wmExplorerRestarted)
            {
                App->UpdateNotificationAreaIcon();
            }
            return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

ItemModifyType GetItemModifyType(const Vect2 &mousePos, const Vect2 &itemPos, const Vect2 &itemSize, const Vect4 &crop, const Vect2 &scaleVal)
{
    Vect2 lowerRight = itemPos+itemSize;
    if( mousePos.x < itemPos.x    ||
        mousePos.y < itemPos.y    ||
        mousePos.x > lowerRight.x ||
        mousePos.y > lowerRight.y )
    {
        return ItemModifyType_None;
    }

    float epsilon = 10.0f;

    Vect2 croppedItemPos = itemPos + Vect2(crop.x / scaleVal.x, crop.y / scaleVal.y);
    Vect2 croppedLowerRight = lowerRight - Vect2(crop.w / scaleVal.x, crop.z / scaleVal.y);

    

    // Corner sizing
    if(mousePos.CloseTo(croppedItemPos, epsilon))
        return ItemModifyType_ScaleTopLeft;
    else if(mousePos.CloseTo(croppedLowerRight, epsilon))
        return ItemModifyType_ScaleBottomRight;
    else if(mousePos.CloseTo(Vect2(croppedLowerRight.x, croppedItemPos.y), epsilon))
        return ItemModifyType_ScaleTopRight;
    else if(mousePos.CloseTo(Vect2(croppedItemPos.x, croppedLowerRight.y), epsilon))
        return ItemModifyType_ScaleBottomLeft;

    epsilon = 4.0f;

    // Edge sizing
    if(CloseFloat(mousePos.x, croppedItemPos.x, epsilon))
        return ItemModifyType_ScaleLeft;
    else if(CloseFloat(mousePos.x, croppedLowerRight.x, epsilon))
        return ItemModifyType_ScaleRight;
    else if(CloseFloat(mousePos.y, croppedItemPos.y, epsilon))
        return ItemModifyType_ScaleTop;
    else if(CloseFloat(mousePos.y, croppedLowerRight.y, epsilon))
        return ItemModifyType_ScaleBottom;


    return ItemModifyType_Move;
}

/**
 * Maps a point in window coordinates to frame coordinates.
 */
Vect2 OBS::MapWindowToFramePos(Vect2 mousePos)
{
    if(App->renderFrameIn1To1Mode)
        return (mousePos - App->GetRenderFrameOffset()) * (App->GetBaseSize() / App->GetOutputSize());
    return (mousePos - App->GetRenderFrameOffset()) * (App->GetBaseSize() / App->GetRenderFrameSize());
}

/**
 * Maps a point in frame coordinates to window coordinates.
 */
Vect2 OBS::MapFrameToWindowPos(Vect2 framePos)
{
    if(App->renderFrameIn1To1Mode)
        return framePos * (App->GetOutputSize() / App->GetBaseSize()) + App->GetRenderFrameOffset();
    return framePos * (App->GetRenderFrameSize() / App->GetBaseSize()) + App->GetRenderFrameOffset();
}

/**
 * Maps a size in window coordinates to frame coordinates.
 */
Vect2 OBS::MapWindowToFrameSize(Vect2 windowSize)
{
    if(App->renderFrameIn1To1Mode)
        return windowSize * (App->GetBaseSize() / App->GetOutputSize());
    return windowSize * (App->GetBaseSize() / App->GetRenderFrameSize());
}

/**
 * Maps a size in frame coordinates to window coordinates.
 */
Vect2 OBS::MapFrameToWindowSize(Vect2 frameSize)
{
    if(App->renderFrameIn1To1Mode)
        return frameSize * (App->GetOutputSize() / App->GetBaseSize());
    return frameSize * (App->GetRenderFrameSize() / App->GetBaseSize());
}

/**
 * Returns the scale of the window relative to the actual frame size. E.g.
 * if the window is twice the size of the frame this will return "0.5".
 */
Vect2 OBS::GetWindowToFrameScale()
{
    return MapWindowToFrameSize(Vect2(1.0f, 1.0f));
}

/**
 * Returns the scale of the frame relative to the window size. E.g.
 * if the window is twice the size of the frame this will return "2.0".
 */
Vect2 OBS::GetFrameToWindowScale()
{
    return MapFrameToWindowSize(Vect2(1.0f, 1.0f));
}

bool OBS::EnsureCropValid(SceneItem *&scaleItem, Vect2 &minSize, Vect2 &snapSize, bool bControlDown, int cropEdges, bool cropSymmetric) 
{
    Vect2 scale = (scaleItem->GetSource() ? scaleItem->GetSource()->GetSize() : scaleItem->GetSize()) / scaleItem->GetSize();

    // When keep aspect is on, cropping can only be half size - 2 * minsize
    if (cropSymmetric)
    {
        if (cropEdges & (edgeLeft | edgeRight))
        {
            if (scaleItem->GetCrop().x > (scaleItem->size.x / 2 ) - 2 * minSize.x)
            {
                scaleItem->crop.x = ((scaleItem->size.x / 2 ) - 2 * minSize.x) * scale.x;
                scaleItem->crop.w = ((scaleItem->size.x / 2 ) - 2 * minSize.x) * scale.x;
            }
            scaleItem->crop.x = (scaleItem->crop.x < 0.0f) ? 0.0f : scaleItem->crop.x;
            scaleItem->crop.w = (scaleItem->crop.w < 0.0f) ? 0.0f : scaleItem->crop.w;
        }
        if (cropEdges & (edgeTop | edgeBottom))
        {
            if (scaleItem->GetCrop().y > (scaleItem->size.y / 2 ) - 2 * minSize.y)
            {
                scaleItem->crop.y = ((scaleItem->size.y / 2 ) - 2 * minSize.y) * scale.y;
                scaleItem->crop.z = ((scaleItem->size.y / 2 ) - 2 * minSize.y) * scale.y;
            }
            scaleItem->crop.y = (scaleItem->crop.y < 0.0f) ? 0.0f : scaleItem->crop.y;
            scaleItem->crop.z = (scaleItem->crop.z < 0.0f) ? 0.0f : scaleItem->crop.z;
        }
    }
    else 
    {
        // left
        if (scaleItem->GetCrop().x > (scaleItem->size.x - scaleItem->GetCrop().w - 32) - minSize.x && cropEdges & edgeLeft)
        {
            scaleItem->crop.x = ((scaleItem->size.x - scaleItem->GetCrop().w - 32) - minSize.x) * scale.x;
        }
        scaleItem->crop.x = (scaleItem->crop.x < 0.0f) ? 0.0f : scaleItem->crop.x;

        // top
        if (scaleItem->GetCrop().y > (scaleItem->size.y - scaleItem->GetCrop().z - 32) - minSize.y && cropEdges & edgeTop)
        {
            scaleItem->crop.y = ((scaleItem->size.y - scaleItem->GetCrop().z - 32) - minSize.y) * scale.y;
        }
        scaleItem->crop.y = (scaleItem->crop.y < 0.0f) ? 0.0f : scaleItem->crop.y;

        // right
        if (scaleItem->GetCrop().w > (scaleItem->size.x - scaleItem->GetCrop().x - 32) - minSize.x && cropEdges & edgeRight)
        {
            scaleItem->crop.w = ((scaleItem->size.x - scaleItem->GetCrop().x - 32) - minSize.x) * scale.x;
        }
        scaleItem->crop.w = (scaleItem->crop.w < 0.0f) ? 0.0f : scaleItem->crop.w;

        // bottom
        if (scaleItem->GetCrop().z > (scaleItem->size.y - scaleItem->GetCrop().y - 32) - minSize.y && cropEdges & edgeBottom)
        {
            scaleItem->crop.z = ((scaleItem->size.y - scaleItem->GetCrop().y - 32) - minSize.y) * scale.y;
        }
        scaleItem->crop.z = (scaleItem->crop.z < 0.0f) ? 0.0f : scaleItem->crop.z;
    }
    if (!bControlDown) 
    {
        // left
        if(CloseFloat(scaleItem->GetCrop().x, 0.0f, snapSize.x))
        {
            scaleItem->crop.x = 0.0f;
        }
        // top
        if(CloseFloat(scaleItem->GetCrop().y, 0.0f, snapSize.y))
        {
            scaleItem->crop.y = 0.0f;
        }
        // right
        if(CloseFloat(scaleItem->GetCrop().w, 0.0f, snapSize.x))
        {
            scaleItem->crop.w = 0.0f;
        }
        // bottom
        if(CloseFloat(scaleItem->GetCrop().z, 0.0f, snapSize.y))
        {
            scaleItem->crop.z = 0.0f;
        }
    }

    return true;
}

LRESULT CALLBACK OBS::RenderFrameProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);

    if(message == WM_ERASEBKGND)
    {
        if(App->bRenderViewEnabled && App->bRunning)
            return 1L;
        else
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    else if(message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN)
    {
        POINTS pos;
        pos.x = (short)LOWORD(lParam);
        pos.y = (short)HIWORD(lParam);

        if((App->bEditMode || message == WM_RBUTTONDOWN) && App->scene)
        {
            Vect2 mousePos = Vect2(float(pos.x), float(pos.y));
            Vect2 framePos = MapWindowToFramePos(mousePos);

            bool bControlDown = HIBYTE(GetKeyState(VK_LCONTROL)) != 0 || HIBYTE(GetKeyState(VK_RCONTROL)) != 0;

            SetFocus(hwnd);

            List<SceneItem*> items;
            App->scene->GetSelectedItems(items);
            if(!items.Num())
            {
                App->scene->GetItemsOnPoint(framePos, items);

                if(items.Num())
                {
                    SceneItem *topItem = items.Last();
                    App->bItemWasSelected = topItem->bSelected;

                    if(!bControlDown)
                    {
                        /* clears all selections */
                        App->bChangingSources = true;
                        ListView_SetItemState(hwndSources, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
                        App->bChangingSources = false;

                        App->scene->DeselectAll();
                    }

                    topItem->Select(true);
                    App->bChangingSources = true;
                    
                    ListView_SetItemState(hwndSources, topItem->GetID(), LVIS_SELECTED, LVIS_SELECTED);
                    App->bChangingSources = false;

                    if (App->bEditMode)
                        if(App->modifyType == ItemModifyType_None)
                            App->modifyType = ItemModifyType_Move;
                }
                else if(!bControlDown) //clicked on empty space without control
                {
                    /* clears all selections */
                    App->bChangingSources = true;
                    ListView_SetItemState(hwndSources, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
                    App->bChangingSources = false;

                    App->scene->DeselectAll();
                }
            }
            else
            {
                
                App->scene->GetItemsOnPoint(framePos, items);
                if(items.Num())
                {
                    SceneItem *topItem = items.Last();
                    App->bItemWasSelected = topItem->bSelected;
                }
            }

            if (message == WM_LBUTTONDOWN)
            {
                App->bMouseDown = true;
                App->lastMousePos = App->startMousePos = mousePos;

                App->scene->GetSelectedItems(items);
                for(UINT i=0; i<items.Num(); i++)
                {
                    items[i]->startPos  = items[i]->pos;
                    items[i]->startSize = items[i]->size;
                }
            }
            else
                App->bRMouseDown = true;
        }
        else
        {
            //SendMessage(hwndMain, WM_NCLBUTTONDOWN, HTCAPTION, (LPARAM)&pos);
            return 0;
        }
    }
    else if(message == WM_MOUSEMOVE)
    {
        if(App->bEditMode && App->scene)
        {
            POINTS pos;
            pos.x = (short)LOWORD(lParam);
            pos.y = (short)HIWORD(lParam);
            Vect2 mousePos = Vect2(float(pos.x), float(pos.y));
            Vect2 scaleVal = GetWindowToFrameScale();

            SceneItem *&scaleItem = App->scaleItem; //just reduces a bit of typing

            if(!App->bMouseDown)
            {
                List<SceneItem*> items;
                App->scene->GetSelectedItems(items);

                App->scaleItem = NULL;
                App->modifyType = ItemModifyType_None;

                for(int i=int(items.Num()-1); i>=0; i--)
                {
                    SceneItem *item = items[i];

                    // Get item in window coordinates
                    Vect2 adjPos  = MapFrameToWindowPos(item->GetPos());
                    Vect2 adjSize = MapFrameToWindowSize(item->GetSize());
                    Vect2 adjSizeBase = MapFrameToWindowSize(item->GetSource() ? item->GetSource()->GetSize() : item->GetSize());

                    ItemModifyType curType = GetItemModifyType(mousePos, adjPos, adjSize, item->GetCrop(), scaleVal);
                    if(curType > ItemModifyType_Move)
                    {
                        App->modifyType = curType;
                        scaleItem = item;
                        break;
                    }
                    else if(curType == ItemModifyType_Move)
                        App->modifyType = ItemModifyType_Move;
                }
            }
            else
            {
                Vect2 baseRenderSize = App->GetBaseSize();
                Vect2 framePos = MapWindowToFramePos(mousePos);

                if(!App->bMouseMoved && mousePos.Dist(App->startMousePos) > 2.0f)
                {
                    SetCapture(hwnd);
                    App->bMouseMoved = true;
                }

                if(App->bMouseMoved)
                {
                    bool bControlDown = HIBYTE(GetKeyState(VK_LCONTROL)) != 0 || HIBYTE(GetKeyState(VK_RCONTROL)) != 0;
                    bool bKeepAspect = HIBYTE(GetKeyState(VK_LSHIFT)) == 0 && HIBYTE(GetKeyState(VK_RSHIFT)) == 0;
                    bool isCropping = HIBYTE(GetKeyState(VK_MENU)) != 0;

                    if (isCropping) {
                        switch(App->modifyType) {
                            case ItemModifyType_ScaleTop:
                                App->modifyType = ItemModifyType_CropTop;
                                break;
                            case ItemModifyType_ScaleLeft:
                                App->modifyType = ItemModifyType_CropLeft;
                                break;
                            case ItemModifyType_ScaleRight:
                                App->modifyType = ItemModifyType_CropRight;
                                break;
                            case ItemModifyType_ScaleBottom:
                                App->modifyType = ItemModifyType_CropBottom;
                                break;
                            case ItemModifyType_ScaleTopLeft:
                                App->modifyType = ItemModifyType_CropTopLeft;
                                break;
                            case ItemModifyType_ScaleTopRight:
                                App->modifyType = ItemModifyType_CropTopRight;
                                break;
                            case ItemModifyType_ScaleBottomRight:
                                App->modifyType = ItemModifyType_CropBottomRight;
                                break;
                            case ItemModifyType_ScaleBottomLeft:
                                App->modifyType = ItemModifyType_CropBottomLeft;
                                break;
                        }
                    }

                    List<SceneItem*> items;
                    App->scene->GetSelectedItems(items);

                    Vect2 totalAdjust = (mousePos-App->startMousePos)*scaleVal;
                    Vect2 frameStartMousePos = App->MapWindowToFramePos(App->startMousePos);
                    Vect2 minSize = scaleVal*21.0f;
                    Vect2 snapSize = scaleVal*10.0f;

                    Vect2 baseScale;
                    float baseScaleAspect;
                    Vect2 cropFactor;

                    if(scaleItem)
                    {
                        if(scaleItem->GetSource())
                        {
                            baseScale = scaleItem->GetSource()->GetSize();
                            baseScaleAspect = baseScale.x/baseScale.y;
                        }
                        else
                        {
                            bKeepAspect = false; //if the source is missing (due to invalid setting or missing plugin), don't allow aspect lock
                            baseScale = scaleItem->size;
                        }
                        cropFactor = baseScale / scaleItem->GetSize();
                    }
                     

                    int edgeAll = edgeLeft | edgeRight | edgeBottom | edgeTop;
                    //more clusterf*** action, straight to your doorstep.  does moving, scaling, snapping, as well as keeping aspect ratio with shift down
                    switch(App->modifyType)
                    {
                        case ItemModifyType_Move:
                            {
                                bool bFoundSnap = false;

                                for(UINT i=0; i<items.Num(); i++)
                                {
                                    SceneItem *item = items[i];
                                    item->pos = item->startPos+totalAdjust;

                                    if(!bControlDown)
                                    {
                                        Vect2 pos = item->pos;
                                        Vect2 bottomRight = pos+item->size;
                                        pos += item->GetCropTL();
                                        bottomRight += item->GetCropBR();

                                        bool bVerticalSnap = true;
                                        if(CloseFloat(pos.x, 0.0f, snapSize.x))
                                            item->pos.x = -item->GetCrop().x;
                                        else if(CloseFloat(bottomRight.x, baseRenderSize.x, snapSize.x))
                                            item->pos.x = baseRenderSize.x-item->size.x+item->GetCrop().w;
                                        else
                                            bVerticalSnap = false;

                                        bool bHorizontalSnap = true;
                                        if(CloseFloat(pos.y, 0.0f, snapSize.y))
                                            item->pos.y = -item->GetCrop().y;
                                        else if(CloseFloat(bottomRight.y, baseRenderSize.y, snapSize.y))
                                            item->pos.y = baseRenderSize.y-item->size.y+item->GetCrop().z;
                                        else
                                            bHorizontalSnap = true;

                                        if(bVerticalSnap || bHorizontalSnap)
                                        {
                                            bFoundSnap = true;
                                            totalAdjust = item->pos-item->startPos;
                                        }
                                    }
                                }

                                if(bFoundSnap)
                                {
                                    for(UINT i=0; i<items.Num(); i++)
                                    {
                                        SceneItem *item = items[i];
                                        item->pos = item->startPos+totalAdjust;
                                    }
                                }

                                break;
                            }

                        case ItemModifyType_CropTop: 
                            scaleItem->crop.y = ((frameStartMousePos.y - scaleItem->pos.y) + totalAdjust.y) * cropFactor.y;
                            if (!bKeepAspect)
                                scaleItem->crop.z = ((frameStartMousePos.y - scaleItem->pos.y) + totalAdjust.y) * cropFactor.y;
                            EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeTop | (!bKeepAspect ? edgeBottom : 0), !bKeepAspect);
                            break;

                        case ItemModifyType_CropBottom: 
                            scaleItem->crop.z = ((scaleItem->pos.y + scaleItem->size.y - frameStartMousePos.y) - totalAdjust.y) * cropFactor.y;
                            if (!bKeepAspect)
                                scaleItem->crop.y = ((scaleItem->pos.y + scaleItem->size.y - frameStartMousePos.y) - totalAdjust.y) * cropFactor.y;
                            EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeBottom | (!bKeepAspect ? edgeTop : 0), !bKeepAspect);
                            break;

                        case ItemModifyType_CropLeft: 
                            scaleItem->crop.x = ((frameStartMousePos.x - scaleItem->pos.x) + totalAdjust.x) * cropFactor.x;
                            if (!bKeepAspect)
                                scaleItem->crop.w = ((frameStartMousePos.x - scaleItem->pos.x) + totalAdjust.x) * cropFactor.x;
                            EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeLeft | (!bKeepAspect ? edgeRight : 0), !bKeepAspect);
                            break;

                        case ItemModifyType_CropRight: 
                            scaleItem->crop.w = ((scaleItem->pos.x + scaleItem->size.x - frameStartMousePos.x) - totalAdjust.x) * cropFactor.x;
                            if (!bKeepAspect)
                                scaleItem->crop.x = ((scaleItem->pos.x + scaleItem->size.x - frameStartMousePos.x) - totalAdjust.x) * cropFactor.x;
                            EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeRight | (!bKeepAspect ? edgeLeft : 0), !bKeepAspect);
                            break;
                            
                        case ItemModifyType_CropBottomLeft:
                            if (bKeepAspect)
                            {
                                scaleItem->crop.z = ((scaleItem->pos.y + scaleItem->size.y - frameStartMousePos.y) - totalAdjust.y) * cropFactor.y;
                                scaleItem->crop.x = ((frameStartMousePos.x - scaleItem->pos.x) + totalAdjust.x) * cropFactor.x;
                            }
                            else
                            {
                                float amount = MIN(((scaleItem->pos.y + scaleItem->size.y - frameStartMousePos.y) - totalAdjust.y), ((frameStartMousePos.x - scaleItem->pos.x) + totalAdjust.x));
                                scaleItem->crop.w = amount * cropFactor.x;
                                scaleItem->crop.x = amount * cropFactor.x;
                                scaleItem->crop.y = amount * cropFactor.y;
                                scaleItem->crop.z = amount * cropFactor.y;
                            }
                            EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeLeft | edgeBottom | (!bKeepAspect ? edgeAll : 0), !bKeepAspect);
                            break;

                        case ItemModifyType_CropBottomRight:
                            if (bKeepAspect)
                            {
                                scaleItem->crop.z = ((scaleItem->pos.y + scaleItem->size.y - frameStartMousePos.y) - totalAdjust.y) * cropFactor.y;
                                scaleItem->crop.w = ((scaleItem->pos.x + scaleItem->size.x - frameStartMousePos.x) - totalAdjust.x) * cropFactor.x;
                            }
                            else
                            {
                                float amount = MIN(((scaleItem->pos.y + scaleItem->size.y - frameStartMousePos.y) - totalAdjust.y), ((scaleItem->pos.x + scaleItem->size.x - frameStartMousePos.x) - totalAdjust.x));
                                scaleItem->crop.w = amount * cropFactor.x;
                                scaleItem->crop.x = amount * cropFactor.x;
                                scaleItem->crop.y = amount * cropFactor.y;
                                scaleItem->crop.z = amount * cropFactor.y;
                            }
                            EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeRight | edgeBottom | (!bKeepAspect ? edgeAll : 0), !bKeepAspect);
                            break;

                        case ItemModifyType_CropTopLeft:
                            if (bKeepAspect)
                            {
                                scaleItem->crop.y = ((frameStartMousePos.y - scaleItem->pos.y) + totalAdjust.y) * cropFactor.y;
                                scaleItem->crop.x = ((frameStartMousePos.x - scaleItem->pos.x) + totalAdjust.x) * cropFactor.x;
                            }
                            else
                            {
                                float amount = MIN(((frameStartMousePos.y - scaleItem->pos.y) + totalAdjust.y), ((frameStartMousePos.x - scaleItem->pos.x) + totalAdjust.x));
                                scaleItem->crop.w = amount * cropFactor.x;
                                scaleItem->crop.x = amount * cropFactor.x;
                                scaleItem->crop.y = amount * cropFactor.y;
                                scaleItem->crop.z = amount * cropFactor.y;
                            }
                            EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeLeft | edgeTop | (!bKeepAspect ? edgeAll : 0), !bKeepAspect);
                            break;

                        case ItemModifyType_CropTopRight:
                            if (bKeepAspect)
                            {
                                scaleItem->crop.y = ((frameStartMousePos.y - scaleItem->pos.y) + totalAdjust.y) * cropFactor.y;
                                scaleItem->crop.w = ((scaleItem->pos.x + scaleItem->size.x - frameStartMousePos.x) - totalAdjust.x) * cropFactor.x;
                            }
                            else
                            {
                                float amount = MIN(((frameStartMousePos.y - scaleItem->pos.y) + totalAdjust.y), ((scaleItem->pos.x + scaleItem->size.x - frameStartMousePos.x) - totalAdjust.x));
                                scaleItem->crop.w = amount * cropFactor.x;
                                scaleItem->crop.x = amount * cropFactor.x;
                                scaleItem->crop.y = amount * cropFactor.y;
                                scaleItem->crop.z = amount * cropFactor.y;
                            }
                            EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeRight | edgeTop | (!bKeepAspect ? edgeAll : 0), !bKeepAspect);
                            break;

                        case ItemModifyType_ScaleBottom:
                            {
                                Vect2 pos = scaleItem->pos + scaleItem->GetCropTL();

                                scaleItem->size.y = scaleItem->startSize.y+totalAdjust.y;
                                if(scaleItem->size.y < minSize.y)
                                    scaleItem->size.y = minSize.y;

                                if(!bControlDown)
                                {
                                    float bottom = scaleItem->pos.y+scaleItem->size.y-scaleItem->GetCrop().z;

                                    if(CloseFloat(bottom, baseRenderSize.y, snapSize.y))
                                    {
                                        bottom = baseRenderSize.y;
                                        scaleItem->size.y = bottom-scaleItem->pos.y+scaleItem->GetCrop().z;
                                    }
                                }

                                if(bKeepAspect)
                                    scaleItem->size.x = scaleItem->size.y*baseScaleAspect;
                                else
                                    scaleItem->size.x = scaleItem->startSize.x;

                                EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeAll, false);

                                scaleItem->pos = pos - scaleItem->GetCropTL();
                            }
                            break;

                        case ItemModifyType_ScaleTop:
                            {
                                Vect2 pos = scaleItem->pos + scaleItem->size + scaleItem->GetCropBR();

                                scaleItem->size.y = scaleItem->startSize.y-totalAdjust.y;
                                if(scaleItem->size.y < minSize.y)
                                    scaleItem->size.y = minSize.y;

                                if(!bControlDown)
                                {
                                    float top = scaleItem->startPos.y+(scaleItem->startSize.y-scaleItem->size.y)+scaleItem->GetCrop().x;
                                    if(CloseFloat(top, 0.0f, snapSize.y))
                                        scaleItem->size.y = scaleItem->startPos.y+scaleItem->startSize.y+scaleItem->GetCrop().x;
                                }

                                if(bKeepAspect)
                                    scaleItem->size.x = scaleItem->size.y*baseScaleAspect;
                                else
                                    scaleItem->size.x = scaleItem->startSize.x;

                                totalAdjust.y = scaleItem->startSize.y-scaleItem->size.y;
                                scaleItem->pos.y = scaleItem->startPos.y+totalAdjust.y;

                                EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeAll, false);

                                scaleItem->pos = pos - scaleItem->GetCropBR() - scaleItem->size;
                            }
                            break;

                        case ItemModifyType_ScaleRight:
                            {
                                Vect2 pos = scaleItem->pos + scaleItem->GetCropTL();

                                scaleItem->size.x = scaleItem->startSize.x+totalAdjust.x;
                                if(scaleItem->size.x < minSize.x)
                                    scaleItem->size.x = minSize.x;

                                if(!bControlDown)
                                {
                                    float right = scaleItem->pos.x + scaleItem->size.x-scaleItem->GetCrop().y;

                                    if(CloseFloat(right, baseRenderSize.x, snapSize.x))
                                    {
                                        right = baseRenderSize.x;
                                        scaleItem->size.x = right-scaleItem->pos.x+scaleItem->GetCrop().y;
                                    }
                                }

                                if(bKeepAspect)
                                    scaleItem->size.y = scaleItem->size.x/baseScaleAspect;
                                else
                                    scaleItem->size.y = scaleItem->startSize.y;

                                EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeAll, false);

                                scaleItem->pos = pos - scaleItem->GetCropTL();
                            }
                            break;

                        case ItemModifyType_ScaleLeft:
                            {
                                Vect2 pos = scaleItem->pos + scaleItem->size + scaleItem->GetCropBR();

                                scaleItem->size.x = scaleItem->startSize.x-totalAdjust.x;
                                if(scaleItem->size.x < minSize.x)
                                    scaleItem->size.x = minSize.x;

                                if(!bControlDown)
                                {
                                    float left = scaleItem->startPos.x+(scaleItem->startSize.x-scaleItem->size.x)+scaleItem->GetCrop().w;

                                    if(CloseFloat(left, 0.0f, snapSize.x))
                                        scaleItem->size.x = scaleItem->startPos.x+scaleItem->startSize.x+scaleItem->GetCrop().w;
                                }

                                if(bKeepAspect)
                                    scaleItem->size.y = scaleItem->size.x/baseScaleAspect;
                                else
                                    scaleItem->size.y = scaleItem->startSize.y;

                                totalAdjust.x = scaleItem->startSize.x-scaleItem->size.x;
                                scaleItem->pos.x = scaleItem->startPos.x+totalAdjust.x;

                                EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeAll, false);

                                scaleItem->pos = pos - scaleItem->GetCropBR() - scaleItem->size;
                            }
                            break;

                        case ItemModifyType_ScaleBottomRight:
                            {
                                Vect2 pos = scaleItem->pos + scaleItem->GetCropTL();

                                scaleItem->size = scaleItem->startSize+totalAdjust;
                                scaleItem->size.ClampMin(minSize);

                                if(!bControlDown)
                                {
                                    Vect2 cropPart = Vect2(scaleItem->GetCrop().y, scaleItem->GetCrop().z);
                                    Vect2 lowerRight = scaleItem->pos+scaleItem->size-cropPart;
                                

                                    if(CloseFloat(lowerRight.x, baseRenderSize.x, snapSize.x))
                                    {
                                        lowerRight.x = baseRenderSize.x;
                                        scaleItem->size.x = lowerRight.x-scaleItem->pos.x+cropPart.x;
                                    }
                                    if(CloseFloat(lowerRight.y, baseRenderSize.y, snapSize.y))
                                    {
                                        lowerRight.y = baseRenderSize.y;
                                        scaleItem->size.y = lowerRight.y-scaleItem->pos.y+cropPart.y;
                                    }
                                }

                                if(bKeepAspect)
                                {
                                    float scaleAspect = scaleItem->size.x/scaleItem->size.y;
                                    if(scaleAspect < baseScaleAspect)
                                        scaleItem->size.x = scaleItem->size.y*baseScaleAspect;
                                    else if(scaleAspect > baseScaleAspect)
                                        scaleItem->size.y = scaleItem->size.x/baseScaleAspect;
                                }

                                EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeAll, false);

                                scaleItem->pos = pos - scaleItem->GetCropTL();
                            }
                            break;

                        case ItemModifyType_ScaleTopLeft:
                            {
                                Vect2 pos = scaleItem->pos + scaleItem->size + scaleItem->GetCropBR();

                                scaleItem->size = scaleItem->startSize-totalAdjust;
                                scaleItem->size.ClampMin(minSize);

                                if(!bControlDown)
                                {
                                    Vect2 cropPart = Vect2(scaleItem->crop.w, scaleItem->crop.x);
                                    Vect2 topLeft = scaleItem->startPos+(scaleItem->startSize-scaleItem->size)+cropPart;

                                    if(CloseFloat(topLeft.x, 0.0f, snapSize.x))
                                        scaleItem->size.x = scaleItem->startPos.x+scaleItem->startSize.x+cropPart.x;
                                    if(CloseFloat(topLeft.y, 0.0f, snapSize.y))
                                        scaleItem->size.y = scaleItem->startPos.y+scaleItem->startSize.y+cropPart.y;
                                }

                                if(bKeepAspect)
                                {
                                    float scaleAspect = scaleItem->size.x/scaleItem->size.y;
                                    if(scaleAspect < baseScaleAspect)
                                        scaleItem->size.x = scaleItem->size.y*baseScaleAspect;
                                    else if(scaleAspect > baseScaleAspect)
                                        scaleItem->size.y = scaleItem->size.x/baseScaleAspect;
                                }

                                totalAdjust = scaleItem->startSize-scaleItem->size;
                                scaleItem->pos = scaleItem->startPos+totalAdjust;

                                EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeAll, false);

                                scaleItem->pos = pos - scaleItem->GetCropBR() - scaleItem->size;
                            }
                            break;

                        case ItemModifyType_ScaleBottomLeft:
                            {
                                Vect2 pos = scaleItem->pos + Vect2(scaleItem->size.x, 0) + scaleItem->GetCropTR();

                                scaleItem->size.x = scaleItem->startSize.x-totalAdjust.x;
                                scaleItem->size.y = scaleItem->startSize.y+totalAdjust.y;
                                scaleItem->size.ClampMin(minSize);

                                if(!bControlDown)
                                {
                                    Vect2 cropPart = Vect2(scaleItem->GetCrop().w, scaleItem->GetCrop().z);
                                    float left = scaleItem->startPos.x+(scaleItem->startSize.x-scaleItem->size.x)+cropPart.x;
                                    float bottom = scaleItem->pos.y+scaleItem->size.y-cropPart.y;

                                    if(CloseFloat(left, 0.0f, snapSize.x))
                                        scaleItem->size.x = scaleItem->startPos.x+scaleItem->startSize.x+cropPart.x;

                                    if(CloseFloat(bottom, baseRenderSize.y, snapSize.y))
                                    {
                                        bottom = baseRenderSize.y;
                                        scaleItem->size.y = bottom-scaleItem->pos.y+cropPart.y;
                                    }
                                }

                                if(bKeepAspect)
                                {
                                    float scaleAspect = scaleItem->size.x/scaleItem->size.y;
                                    if(scaleAspect < baseScaleAspect)
                                        scaleItem->size.x = scaleItem->size.y*baseScaleAspect;
                                    else if(scaleAspect > baseScaleAspect)
                                        scaleItem->size.y = scaleItem->size.x/baseScaleAspect;
                                }

                                totalAdjust.x = scaleItem->startSize.x-scaleItem->size.x;
                                scaleItem->pos.x = scaleItem->startPos.x+totalAdjust.x;

                                EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeAll, false);

                                scaleItem->pos = pos - scaleItem->GetCropTR() - Vect2(scaleItem->size.x, 0);
                            }
                            break;

                        case ItemModifyType_ScaleTopRight:
                            {
                                Vect2 pos = scaleItem->pos + Vect2(0, scaleItem->size.y) + scaleItem->GetCropBL();

                                scaleItem->size.x = scaleItem->startSize.x+totalAdjust.x;
                                scaleItem->size.y = scaleItem->startSize.y-totalAdjust.y;
                                scaleItem->size.ClampMin(minSize);

                                if(!bControlDown)
                                {
                                    Vect2 cropPart = Vect2(scaleItem->GetCrop().y, scaleItem->GetCrop().x);
                                    float right = scaleItem->pos.x+scaleItem->size.x-cropPart.x;
                                    float top = scaleItem->startPos.y+(scaleItem->startSize.y-scaleItem->size.y)+cropPart.y;

                                    if(CloseFloat(right, baseRenderSize.x, snapSize.x))
                                    {
                                        right = baseRenderSize.x;
                                        scaleItem->size.x = right-scaleItem->pos.x+cropPart.x;
                                    }

                                    if(CloseFloat(top, 0.0f, snapSize.y))
                                        scaleItem->size.y = scaleItem->startPos.y+scaleItem->startSize.y+cropPart.y;
                                }

                                if(bKeepAspect)
                                {
                                    float scaleAspect = scaleItem->size.x/scaleItem->size.y;
                                    if(scaleAspect < baseScaleAspect)
                                        scaleItem->size.x = scaleItem->size.y*baseScaleAspect;
                                    else if(scaleAspect > baseScaleAspect)
                                        scaleItem->size.y = scaleItem->size.x/baseScaleAspect;
                                }

                                totalAdjust.y = scaleItem->startSize.y-scaleItem->size.y;
                                scaleItem->pos.y = scaleItem->startPos.y+totalAdjust.y;

                                EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeAll, false);

                                scaleItem->pos = pos - scaleItem->GetCropBL() - Vect2(0, scaleItem->size.y);
                            }
                            break;

                    }

                    App->lastMousePos = mousePos;
                }
            }

            switch(App->modifyType)
            {
                case ItemModifyType_ScaleBottomLeft:
                case ItemModifyType_ScaleTopRight:
                    SetCursor(LoadCursor(NULL, IDC_SIZENESW));
                    return 0;

                case ItemModifyType_ScaleBottomRight:
                case ItemModifyType_ScaleTopLeft:
                    SetCursor(LoadCursor(NULL, IDC_SIZENWSE));
                    return 0;

                case ItemModifyType_CropLeft:
                case ItemModifyType_CropRight:
                case ItemModifyType_ScaleLeft:
                case ItemModifyType_ScaleRight:
                    SetCursor(LoadCursor(NULL, IDC_SIZEWE));
                    return 0;

                case ItemModifyType_CropTop:
                case ItemModifyType_CropBottom:
                case ItemModifyType_ScaleTop:
                case ItemModifyType_ScaleBottom:
                    SetCursor(LoadCursor(NULL, IDC_SIZENS));
                    return 0;

                default:
                    SetCursor(LoadCursor(NULL, IDC_ARROW));
                    return 0;
            }
        }
    }
    else if(message == WM_LBUTTONUP || message == WM_RBUTTONUP)
    {
        if((App->bEditMode || message == WM_RBUTTONUP) && App->scene)
        {
            POINTS pos;
            pos.x = (short)LOWORD(lParam);
            pos.y = (short)HIWORD(lParam);

            if(App->bMouseDown || App->bRMouseDown)
            {
                Vect2 mousePos = Vect2(float(pos.x), float(pos.y));

                bool bControlDown = HIBYTE(GetKeyState(VK_CONTROL)) != 0;

                List<SceneItem*> items;

                if(!App->bMouseMoved)
                {
                    Vect2 framePos = MapWindowToFramePos(mousePos);
                    App->scene->GetItemsOnPoint(framePos, items);

                    if(bControlDown && App->bItemWasSelected)
                    {
                        SceneItem *lastItem = items.Last();
                        lastItem->Select(false);
                        ListView_SetItemState(hwndSources, lastItem->GetID(), 0, LVIS_SELECTED);
                    }
                    else
                    {
                        if(items.Num())
                        {
                            SceneItem *topItem = items.Last();

                            if(!bControlDown)
                            {
                                App->bChangingSources = true;
                                ListView_SetItemState(hwndSources, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
                                App->bChangingSources = false;

                                App->scene->DeselectAll();
                            }

                            topItem->Select(true);

                            App->bChangingSources = true;
                            SetFocus(hwnd);
                            ListView_SetItemState(hwndSources, topItem->GetID(), LVIS_SELECTED, LVIS_SELECTED);
                            App->bChangingSources = false;

                            if(App->modifyType == ItemModifyType_None)
                                App->modifyType = ItemModifyType_Move;
                        }
                        else if(!bControlDown) //clicked on empty space without control
                        {
                            App->bChangingSources = true;
                            ListView_SetItemState(hwndSources, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
                            App->bChangingSources = false;
                            App->scene->DeselectAll();
                        }
                    }
                }
                else if(message == WM_LBUTTONUP)
                {
                    App->scene->GetSelectedItems(items);

                    ReleaseCapture();
                    App->bMouseMoved = false;

                    for(UINT i=0; i<items.Num(); i++)
                    {
                        SceneItem *item = items[i];
                        (item->pos  += 0.5f).Floor();
                        (item->size += 0.5f).Floor();

                        XElement *itemElement = item->GetElement();
                        itemElement->SetInt(TEXT("x"),  int(item->pos.x));
                        itemElement->SetInt(TEXT("y"),  int(item->pos.y));
                        itemElement->SetInt(TEXT("cx"), int(item->size.x));
                        itemElement->SetInt(TEXT("cy"), int(item->size.y));
                        itemElement->SetFloat(TEXT("crop.left"), item->crop.x);
                        itemElement->SetFloat(TEXT("crop.top"), item->crop.y);
                        itemElement->SetFloat(TEXT("crop.right"), item->crop.w);
                        itemElement->SetFloat(TEXT("crop.bottom"), item->crop.z);
                    }

                    App->modifyType = ItemModifyType_None;
                }

                App->bMouseDown = false;
                App->bRMouseDown = false;
            }
        }
        if(message == WM_RBUTTONUP)
        {
            HMENU hPopup = CreatePopupMenu();

            AppendMenu(hPopup, MF_STRING | (App->bFullscreenMode ? MF_CHECKED : 0), ID_TOGGLEFULLSCREEN, Str("MainMenu.Settings.FullscreenMode"));

            HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);
            int numItems = ListView_GetItemCount(hwndSources);
            bool bSelected = ListView_GetSelectedCount(hwndSources) != 0;
            HMENU hMenuPreview;

            if (App->IsRunning() && bSelected)
                hMenuPreview = CreatePopupMenu();
            else
            {
                hMenuPreview = hPopup;
                AppendMenu(hMenuPreview, MF_SEPARATOR, 0, 0);
            }

            AppendMenu(hMenuPreview, MF_STRING | (!App->renderFrameIn1To1Mode ? MF_CHECKED : 0), ID_PREVIEWSCALETOFITMODE, Str("RenderView.ViewModeScaleToFit"));
            AppendMenu(hMenuPreview, MF_STRING | (App->renderFrameIn1To1Mode ? MF_CHECKED : 0), ID_PREVIEW1TO1MODE, Str("RenderView.ViewMode1To1"));
            AppendMenu(hMenuPreview, MF_SEPARATOR, 0, 0);
            AppendMenu(hMenuPreview, MF_STRING | (App->bRenderViewEnabled ? MF_CHECKED : 0), ID_TOGGLERENDERVIEW, Str("RenderView.EnableView"));
            AppendMenu(hMenuPreview, MF_STRING | (App->bPanelVisible ? MF_CHECKED : 0), ID_TOGGLEPANEL, Str("RenderView.DisplayPanel"));

            if (App->IsRunning())
            {
                if (bSelected)
                    AppendMenu(hPopup, MF_STRING|MF_POPUP, (UINT_PTR)hMenuPreview, Str("Preview"));
                App->AppendModifyListbox(hwndSources, hPopup, ID_SOURCES, numItems, bSelected);
            }

            POINT p;
            GetCursorPos(&p);

            int ret = (int)TrackPopupMenuEx(hPopup, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, p.x, p.y, hwndMain, NULL);
            App->TrackModifyListbox(hwnd, ret);

            DestroyMenu(hPopup);
        }
    }
    else if(message == WM_GETDLGCODE && App->bEditMode)
    {
        return DLGC_WANTARROWS; 
    }
    else if(message == WM_KEYDOWN && App->bRunning && App->bEditMode)
    {
        int dx, dy ;
        dx = dy = 0;
        switch(wParam)
        {
            case VK_UP:
                dy = -1;
                break;
            case VK_DOWN:
                dy = 1;
                break;
            case VK_RIGHT:
                dx = 1;
                break;
            case VK_LEFT:
                dx = -1;
                break;
            default:
                return DefWindowProc(hwnd, message, wParam, lParam);
        }
        App->MoveItemsByPixels(dx, dy);
        return 0;
    }
    else if(message == WM_KEYUP && App->bEditMode)
    {
        return 0;
    }
            

    return DefWindowProc(hwnd, message, wParam, lParam);
}

typedef CTSTR (*GETPLUGINNAMEPROC)();
typedef CTSTR (*GETPLUGINDESCRIPTIONPROC)();
typedef void (*CONFIGUREPLUGINPROC)(HWND);



INT_PTR CALLBACK OBS::PluginsDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                LocalizeWindow(hwnd);

                HWND hwndPlugins = GetDlgItem(hwnd, IDC_PLUGINS);

                for(UINT i=0; i<App->plugins.Num(); i++)
                {
                    PluginInfo &pluginInfo = App->plugins[i];
                    GETPLUGINNAMEPROC getName = (GETPLUGINNAMEPROC)GetProcAddress(pluginInfo.hModule, "GetPluginName");

                    CTSTR lpName;
                    if(getName)
                        lpName = getName();
                    else
                        lpName = pluginInfo.strFile;

                    UINT id = (UINT)SendMessage(hwndPlugins, LB_ADDSTRING, 0, (LPARAM)lpName);
                    SendMessage(hwndPlugins, LB_SETITEMDATA, id, (LPARAM)i);
                }

                return TRUE;
            }

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_PLUGINS:
                    if(HIWORD(wParam) == LBN_SELCHANGE)
                    {
                        UINT id = (UINT)SendMessage((HWND)lParam, LB_GETCURSEL, 0, 0);
                        if(id == LB_ERR)
                            break;

                        UINT pluginID = (UINT)SendMessage((HWND)lParam, LB_GETITEMDATA, id, 0);
                        PluginInfo &pluginInfo = App->plugins[pluginID];

                        //-------------------------------------

                        GETPLUGINDESCRIPTIONPROC getDescription = (GETPLUGINDESCRIPTIONPROC)GetProcAddress(pluginInfo.hModule, "GetPluginDescription");

                        CTSTR lpDescription = NULL;
                        if(getDescription)
                            lpDescription = getDescription();

                        String strText;
                        strText << Str("Plugins.Filename")    << TEXT(" ") << pluginInfo.strFile;
                        
                        if(lpDescription)
                            strText << TEXT("\r\n\r\n") << Str("Plugins.Description") << TEXT("\r\n") << lpDescription;

                        SetWindowText(GetDlgItem(hwnd, IDC_DESCRIPTION), strText.Array());

                        //-------------------------------------

                        CONFIGUREPLUGINPROC configPlugin = (CONFIGUREPLUGINPROC)GetProcAddress(pluginInfo.hModule, "ConfigPlugin");
                        EnableWindow(GetDlgItem(hwnd, IDC_CONFIG), configPlugin != NULL ? TRUE : FALSE);
                    }
                    break;

                case IDC_CONFIG:
                    if(HIWORD(wParam) == BN_CLICKED)
                    {
                        HWND hwndPlugins = GetDlgItem(hwnd, IDC_PLUGINS);

                        UINT id = (UINT)SendMessage(hwndPlugins, LB_GETCURSEL, 0, 0);
                        if(id == LB_ERR)
                            break;

                        UINT pluginID = (UINT)SendMessage(hwndPlugins, LB_GETITEMDATA, id, 0);
                        PluginInfo &pluginInfo = App->plugins[pluginID];

                        //-------------------------------------

                        CONFIGUREPLUGINPROC configPlugin = (CONFIGUREPLUGINPROC)GetProcAddress(pluginInfo.hModule, "ConfigPlugin");
                        configPlugin(hwnd);
                    }
                    break;

                case IDOK:
                    EndDialog(hwnd, IDOK);
            }
            break;

        case WM_CLOSE:
            EndDialog(hwnd, IDOK);
    }

    return FALSE;
}

int SetSliderText(HWND hwndParent, int controlSlider, int controlText)
{
    HWND hwndSlider = GetDlgItem(hwndParent, controlSlider);
    HWND hwndText   = GetDlgItem(hwndParent, controlText);

    int sliderVal = (int)SendMessage(hwndSlider, TBM_GETPOS, 0, 0);
    float floatVal = float(sliderVal)*0.01f;

    SetWindowText(hwndText, FormattedString(TEXT("%.02f"), floatVal));

    return sliderVal;
}

