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


//hello, you've come into the file I hate the most.



extern WNDPROC listboxProc;

void STDCALL SceneHotkey(DWORD hotkey, UPARAM param, bool bDown);

enum
{
    ID_LISTBOX_REMOVE=1,
    ID_LISTBOX_MOVEUP,
    ID_LISTBOX_MOVEDOWN,
    ID_LISTBOX_MOVETOTOP,
    ID_LISTBOX_MOVETOBOTTOM,
    ID_LISTBOX_CENTER,
    ID_LISTBOX_FITTOSCREEN,
    ID_LISTBOX_RESETSIZE,
    ID_LISTBOX_RENAME,
    ID_LISTBOX_HOTKEY,
    ID_LISTBOX_CONFIG,

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

                        if(App->sceneElement)
                        {
                            XElement *sources = App->sceneElement->GetElement(TEXT("sources"));
                            if(!sources)
                                sources = App->sceneElement->CreateElement(TEXT("sources"));

                            if(sources->GetElement(str) != NULL)
                            {
                                String strExists = Str("NameExists");
                                strExists.FindReplace(TEXT("$1"), str);
                                MessageBox(hwnd, strExists, NULL, 0);
                                break;
                            }
                        }

                        String &strOut = *(String*)GetWindowLongPtr(hwnd, DWLP_USER);
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

                        XElement *scenes = App->scenesConfig.GetElement(TEXT("scenes"));
                        if(scenes->GetElement(str) != NULL)
                        {
                            String strExists = Str("NameExists");
                            strExists.FindReplace(TEXT("$1"), str);
                            MessageBox(hwnd, strExists, NULL, 0);
                            break;
                        }

                        String &strOut = *(String*)GetWindowLongPtr(hwnd, DWLP_USER);
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
    if(message == WM_RBUTTONDOWN)
    {
        CallWindowProc(listboxProc, hwnd, WM_LBUTTONDOWN, wParam, lParam);

        UINT id = (UINT)GetWindowLongPtr(hwnd, GWL_ID);

        HMENU hMenu = CreatePopupMenu();

        int numItems = (int)SendMessage(hwnd, LB_GETCOUNT, 0, 0);
        bool bSelected = true;

        if(id == ID_SCENES)
        {
            SendMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_SCENES, LBN_SELCHANGE), (LPARAM)GetDlgItem(hwndMain, ID_SCENES));

            for(UINT i=0; i<App->sceneClasses.Num(); i++)
            {
                String strAdd = Str("Listbox.Add");
                strAdd.FindReplace(TEXT("$1"), App->sceneClasses[i].strName);
                AppendMenu(hMenu, MF_STRING, ID_LISTBOX_ADD+i, strAdd.Array());
            }
        }
        else if(id == ID_SOURCES)
        {
            if(!App->sceneElement)
                return 0;

            SendMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_SOURCES, LBN_SELCHANGE), (LPARAM)GetDlgItem(hwndMain, ID_SCENES));

            for(UINT i=0; i<App->imageSourceClasses.Num(); i++)
            {
                String strAdd = Str("Listbox.Add");
                strAdd.FindReplace(TEXT("$1"), App->imageSourceClasses[i].strName);

                if(App->imageSourceClasses[i].strClass == TEXT("GlobalSource"))
                {
                    List<CTSTR> sourceNames;
                    App->GetGlobalSourceNames(sourceNames);

                    if(!sourceNames.Num())
                        continue;

                    HMENU hmenuGlobals = CreatePopupMenu();

                    for(UINT j=0; j<sourceNames.Num(); j++)
                        AppendMenu(hmenuGlobals, MF_STRING, ID_LISTBOX_GLOBALSOURCE+j, sourceNames[j]);

                    AppendMenu(hMenu, MF_STRING|MF_POPUP, (UINT_PTR)hmenuGlobals, strAdd.Array());
                }
                else
                    AppendMenu(hMenu, MF_STRING, ID_LISTBOX_ADD+i, strAdd.Array());
            }

            bSelected = SendMessage(hwnd, LB_GETSELCOUNT, 0, 0) != 0;
        }

        if(numItems && bSelected)
        {
            String strRemove       = Str("Remove");
            String strRename       = Str("Rename");
            String strMoveUp       = Str("MoveUp");
            String strMoveDown     = Str("MoveDown");
            String strMoveTop      = Str("MoveToTop");
            String strMoveToBottom = Str("MoveToBottom");
            String strCenter       = Str("Listbox.Center");
            String strFitToScreen  = Str("Listbox.FitToScreen");
            String strResize       = Str("Listbox.ResetSize");

            if(id == ID_SOURCES)
            {
                strRemove       << TEXT("\tDel");
                strMoveUp       << TEXT("\tCtrl-Up");
                strMoveDown     << TEXT("\tCtrl-Down");
                strMoveTop      << TEXT("\tCtrl-Home");
                strMoveToBottom << TEXT("\tCtrl-End");
                strCenter       << TEXT("\tCtrl-C");
                strFitToScreen  << TEXT("\tCtrl-F");
                strResize       << TEXT("\tCtrl-R");
            }

            AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
            AppendMenu(hMenu, MF_STRING, ID_LISTBOX_REMOVE,         strRemove);
            AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
            AppendMenu(hMenu, MF_STRING, ID_LISTBOX_MOVEUP,         strMoveUp);
            AppendMenu(hMenu, MF_STRING, ID_LISTBOX_MOVEDOWN,       strMoveDown);
            AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
            AppendMenu(hMenu, MF_STRING, ID_LISTBOX_MOVETOTOP,      strMoveTop);
            AppendMenu(hMenu, MF_STRING, ID_LISTBOX_MOVETOBOTTOM,   strMoveToBottom);

            UINT numSelected = (UINT)SendMessage(hwnd, LB_GETSELCOUNT, 0, 0);
            if(id == ID_SCENES || numSelected == 1)
            {
                AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
                AppendMenu(hMenu, MF_STRING, ID_LISTBOX_RENAME,         strRename);
            }

            if(id == ID_SCENES && numSelected)
            {
                AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
                AppendMenu(hMenu, MF_STRING, ID_LISTBOX_HOTKEY, Str("Listbox.SetHotkey"));
            }

            if(id == ID_SOURCES)
            {
                AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
                AppendMenu(hMenu, MF_STRING, ID_LISTBOX_CENTER,         strCenter);
                AppendMenu(hMenu, MF_STRING, ID_LISTBOX_FITTOSCREEN,    strFitToScreen);
                AppendMenu(hMenu, MF_STRING, ID_LISTBOX_RESETSIZE,      strResize);
            }
        }

        POINT p;
        GetCursorPos(&p);

        int curSel = (int)SendMessage(hwnd, LB_GETCURSEL, 0, 0);

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
                if(curClassInfo->configProc)
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
                        }
                    }
                    break;

                case ID_LISTBOX_REMOVE:
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

                        bDelete = true;
                    }
                    break;

                case ID_LISTBOX_RENAME:
                    {
                        String strName;
                        if(DialogBoxParam(hinstMain, MAKEINTRESOURCE(IDD_ENTERNAME), hwndMain, OBS::EnterSceneNameDialogProc, (LPARAM)&strName) == IDOK)
                        {
                            SendMessage(hwnd, LB_DELETESTRING, curSel, 0);
                            SendMessage(hwnd, LB_INSERTSTRING, curSel, (LPARAM)strName.Array());
                            SendMessage(hwnd, LB_SETCURSEL, curSel, 0);

                            item->SetName(strName);
                        }
                        break;
                    }

                case ID_LISTBOX_CONFIG:
                    if(curClassInfo->configProc(item, false))
                    {
                        if(App->bRunning)
                            App->scene->UpdateSettings();
                    }
                    break;

                case ID_LISTBOX_HOTKEY:
                    {
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

                SendMessage(GetDlgItem(hwndMain, ID_SOURCES), LB_RESETCONTENT, 0, 0);

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

            UINT numSelected = (UINT)SendMessage(hwnd, LB_GETSELCOUNT, 0, 0);

            XElement *selectedElement = NULL;

            ClassInfo *curClassInfo = NULL;
            if(numSelected == 1)
            {
                UINT selectedID;
                SendMessage(hwnd, LB_GETSELITEMS, 1, (LPARAM)&selectedID);

                XElement *sourcesElement = App->sceneElement->GetElement(TEXT("sources"));
                selectedElement = sourcesElement->GetElementByID(selectedID);

                curClassInfo = App->GetImageSourceClass(selectedElement->GetString(TEXT("class")));
                if(curClassInfo && curClassInfo->configProc)
                {
                    AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
                    AppendMenu(hMenu, MF_STRING, ID_LISTBOX_CONFIG, Str("Listbox.Config"));
                }
            }

            int ret = (int)TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, p.x, p.y, hwndMain, NULL);
            switch(ret)
            {
                default:
                    if(ret >= ID_LISTBOX_ADD)
                    {
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
                            XElement *sources = curSceneElement->GetElement(TEXT("sources"));
                            if(!sources)
                                sources = curSceneElement->CreateElement(TEXT("sources"));
                            XElement *newSourceElement = sources->CreateElement(strName);

                            if(ret >= ID_LISTBOX_GLOBALSOURCE)
                            {
                                newSourceElement->SetString(TEXT("class"), TEXT("GlobalSource"));

                                List<CTSTR> sourceNames;
                                App->GetGlobalSourceNames(sourceNames);

                                CTSTR lpName = sourceNames[ret-ID_LISTBOX_GLOBALSOURCE];

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
                                        break;
                                    }
                                }
                            }

                            if(App->sceneElement == curSceneElement)
                            {
                                if(App->bRunning)
                                {
                                    App->EnterSceneMutex();
                                    App->scene->AddImageSource(newSourceElement);
                                    App->LeaveSceneMutex();
                                }

                                UINT newID = (UINT)SendMessage(hwnd, LB_ADDSTRING, 0, (LPARAM)strName.Array());
                                PostMessage(hwnd, LB_SETCURSEL, newID, 0);
                                PostMessage(hwndMain, WM_COMMAND, MAKEWPARAM(ID_SOURCES, LBN_SELCHANGE), (LPARAM)hwnd);
                            }
                        }
                    }
                    break;

                case ID_LISTBOX_REMOVE:
                    App->DeleteItems();
                    break;

                case ID_LISTBOX_RENAME:
                    {
                        String strName;
                        if(DialogBoxParam(hinstMain, MAKEINTRESOURCE(IDD_ENTERNAME), hwndMain, OBS::EnterSourceNameDialogProc, (LPARAM)&strName) == IDOK)
                        {
                            SendMessage(hwnd, LB_DELETESTRING, curSel, 0);
                            SendMessage(hwnd, LB_INSERTSTRING, curSel, (LPARAM)strName.Array());
                            SendMessage(hwnd, LB_SETSEL, TRUE, curSel);

                            selectedElement->SetName(strName);
                        }
                        break;
                    }

                case ID_LISTBOX_CONFIG:
                    if(curClassInfo->configProc(selectedElement, false))
                    {
                        if(App->bRunning)
                        {
                            App->EnterSceneMutex();

                            if(selectedSceneItems[0]->GetSource())
                                selectedSceneItems[0]->GetSource()->UpdateSettings();
                            selectedSceneItems[0]->Update();

                            App->LeaveSceneMutex();
                        }
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
                    App->CenterItems();
                    break;

                case ID_LISTBOX_FITTOSCREEN:
                    App->FitItemsToScreen();
                    break;

                case ID_LISTBOX_RESETSIZE:
                    App->ResetItemSizes();
                    break;
            }
        }

        DestroyMenu(hMenu);

        return 0;
    }

    return CallWindowProc(listboxProc, hwnd, message, wParam, lParam);
}

//----------------------------

void OBS::DeleteItems()
{
    HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);
    UINT numSelected = (UINT)SendMessage(hwndSources, LB_GETSELCOUNT, 0, 0);
    int numItems = (int)SendMessage(hwndSources, LB_GETCOUNT, 0, 0);

    List<SceneItem*> selectedSceneItems;
    if(App->scene)
        App->scene->GetSelectedItems(selectedSceneItems);

    List<UINT> selectedIDs;
    selectedIDs.SetSize(numSelected);
    SendMessage(hwndSources, LB_GETSELITEMS, numSelected, (LPARAM)selectedIDs.Array());

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
                    SceneItem *item = selectedSceneItems[i];
                    App->scene->RemoveImageSource(item);
                }
            }
            else
            {
                for(UINT i=0; i<selectedElements.Num(); i++)
                    sourcesElement->RemoveElement(selectedElements[i]);
            }

            for(UINT i=0; i<selectedIDs.Num(); i++)
                SendMessage(hwndSources, LB_DELETESTRING, selectedIDs[i], 0);

            if(selectedSceneItems.Num())
                App->LeaveSceneMutex();
        }
    }
}

void OBS::MoveSourcesUp()
{
    HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);
    UINT numSelected = (UINT)SendMessage(hwndSources, LB_GETSELCOUNT, 0, 0);
    int numItems = (int)SendMessage(hwndSources, LB_GETCOUNT, 0, 0);

    List<SceneItem*> selectedSceneItems;
    if(App->scene)
        App->scene->GetSelectedItems(selectedSceneItems);

    List<UINT> selectedIDs;
    selectedIDs.SetSize(numSelected);
    SendMessage(hwndSources, LB_GETSELITEMS, numSelected, (LPARAM)selectedIDs.Array());

    XElement *sourcesElement = NULL;
    List<XElement*> selectedElements;
    if(numItems)
    {
        sourcesElement = App->sceneElement->GetElement(TEXT("sources"));
        for(UINT i=0; i<selectedIDs.Num(); i++)
            selectedElements << sourcesElement->GetElementByID(selectedIDs[i]);
    }

    for(UINT i=0; i<selectedIDs.Num(); i++)
    {
        if( (i == 0 && selectedIDs[i] > 0) ||
            (i != 0 && selectedIDs[i-1] != selectedIDs[i]-1) )
        {
            if(App->scene)
                selectedSceneItems[i]->MoveUp();
            else
                selectedElements[i]->MoveUp();

            String strName = GetLBText(hwndSources, selectedIDs[i]);
            SendMessage(hwndSources, LB_DELETESTRING, selectedIDs[i], 0);
            SendMessage(hwndSources, LB_INSERTSTRING, --selectedIDs[i], (LPARAM)strName.Array());
            SendMessage(hwndSources, LB_SETSEL, TRUE, selectedIDs[i]);
        }
    }
}

void OBS::MoveSourcesDown()
{
    HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);
    UINT numSelected = (UINT)SendMessage(hwndSources, LB_GETSELCOUNT, 0, 0);
    int numItems = (int)SendMessage(hwndSources, LB_GETCOUNT, 0, 0);

    List<SceneItem*> selectedSceneItems;
    if(App->scene)
        App->scene->GetSelectedItems(selectedSceneItems);

    List<UINT> selectedIDs;
    selectedIDs.SetSize(numSelected);
    SendMessage(hwndSources, LB_GETSELITEMS, numSelected, (LPARAM)selectedIDs.Array());

    XElement *sourcesElement = NULL;
    List<XElement*> selectedElements;
    if(numItems)
    {
        sourcesElement = App->sceneElement->GetElement(TEXT("sources"));
        for(UINT i=0; i<selectedIDs.Num(); i++)
            selectedElements << sourcesElement->GetElementByID(selectedIDs[i]);
    }

    UINT lastItem = (UINT)SendMessage(hwndSources, LB_GETCOUNT, 0, 0)-1;
    UINT lastSelectedID = numSelected-1;

    for(int i=(int)lastSelectedID; i>=0; i--)
    {
        if( (i == lastSelectedID && selectedIDs[i] < lastItem) ||
            (i != lastSelectedID && selectedIDs[i+1] != selectedIDs[i]+1) )
        {
            if(App->scene)
                selectedSceneItems[i]->MoveDown();
            else
                selectedElements[i]->MoveDown();

            String strName = GetLBText(hwndSources, selectedIDs[i]);
            SendMessage(hwndSources, LB_DELETESTRING, selectedIDs[i], 0);
            SendMessage(hwndSources, LB_INSERTSTRING, ++selectedIDs[i], (LPARAM)strName.Array());
            SendMessage(hwndSources, LB_SETSEL, TRUE, selectedIDs[i]);
        }
    }
}

void OBS::MoveSourcesToTop()
{
    HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);
    UINT numSelected = (UINT)SendMessage(hwndSources, LB_GETSELCOUNT, 0, 0);
    int numItems = (int)SendMessage(hwndSources, LB_GETCOUNT, 0, 0);

    List<SceneItem*> selectedSceneItems;
    if(App->scene)
        App->scene->GetSelectedItems(selectedSceneItems);

    List<UINT> selectedIDs;
    selectedIDs.SetSize(numSelected);
    SendMessage(hwndSources, LB_GETSELITEMS, numSelected, (LPARAM)selectedIDs.Array());

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

    for(UINT i=0; i<selectedIDs.Num(); i++)
    {
        if(selectedIDs[i] != i)
        {
            String strName = GetLBText(hwndSources, selectedIDs[i]);
            SendMessage(hwndSources, LB_DELETESTRING, selectedIDs[i], 0);
            SendMessage(hwndSources, LB_INSERTSTRING, i, (LPARAM)strName.Array());
            SendMessage(hwndSources, LB_SETSEL, TRUE, i);
        }
    }
}

void OBS::MoveSourcesToBottom()
{
    HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);
    UINT numSelected = (UINT)SendMessage(hwndSources, LB_GETSELCOUNT, 0, 0);
    int numItems = (int)SendMessage(hwndSources, LB_GETCOUNT, 0, 0);

    List<SceneItem*> selectedSceneItems;
    if(App->scene)
        App->scene->GetSelectedItems(selectedSceneItems);

    List<UINT> selectedIDs;
    selectedIDs.SetSize(numSelected);
    SendMessage(hwndSources, LB_GETSELITEMS, numSelected, (LPARAM)selectedIDs.Array());

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

    UINT curID = (UINT)SendMessage(hwndSources, LB_GETCOUNT, 0, 0)-1;

    for(int i=int(selectedIDs.Num()-1); i>=0; i--)
    {
        if(selectedIDs[i] != curID)
        {
            String strName = GetLBText(hwndSources, selectedIDs[i]);
            SendMessage(hwndSources, LB_DELETESTRING, selectedIDs[i], 0);
            SendMessage(hwndSources, LB_INSERTSTRING, curID, (LPARAM)strName.Array());
            SendMessage(hwndSources, LB_SETSEL, TRUE, curID);
        }

        curID--;
    }
}

void OBS::CenterItems()
{
    if(App->bRunning)
    {
        List<SceneItem*> selectedItems;
        App->scene->GetSelectedItems(selectedItems);

        Vect2 baseSize = App->GetBaseSize();

        for(UINT i=0; i<selectedItems.Num(); i++)
        {
            SceneItem *item = selectedItems[i];
            item->pos = (baseSize*0.5f)-(item->size*0.5f);

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

                Vect2 pos = Vect2(0.0f, 0.0f);
                Vect2 size = baseSize;

                double sourceAspect = double(itemSize.x)/double(itemSize.y);
                if(!CloseDouble(baseAspect, sourceAspect))
                {
                    if(baseAspect < sourceAspect)
                        size.y = float(double(size.x) / sourceAspect);
                    else
                        size.x = float(double(size.y) * sourceAspect);

                    pos = (baseSize-size)*0.5f;

                    pos.x = (float)round(pos.x);
                    pos.y = (float)round(pos.y);

                    size.x = (float)round(size.x);
                    size.y = (float)round(size.y);
                }

                item->pos  = pos;
                item->size = size;

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

//----------------------------

INT_PTR CALLBACK OBS::EnterGlobalSourceNameDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)lParam);
            LocalizeWindow(hwnd);

            //SetFocus(GetDlgItem(hwnd, IDC_NAME));
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

                        XElement *globals = App->scenesConfig.GetElement(TEXT("global sources"));
                        if(globals)
                        {
                            if(globals->GetElement(str) != NULL)
                            {
                                String strExists = Str("NameExists");
                                strExists.FindReplace(TEXT("$1"), str);
                                MessageBox(hwnd, strExists, NULL, 0);
                                break;
                            }
                        }

                        String &strOut = *(String*)GetWindowLongPtr(hwnd, DWLP_USER);
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
                                String strAdd = Str("Listbox.Add");
                                strAdd.FindReplace(TEXT("$1"), App->imageSourceClasses[i].strName);
                                AppendMenu(hMenu, MF_STRING, i+1, strAdd.Array());
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
                            HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);

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
                                            UINT listID = (UINT)SendMessage(hwndSources, LB_FINDSTRINGEXACT, -1, (LPARAM)item->GetName());
                                            if(listID != LB_ERR)
                                                SendMessage(hwndSources, LB_DELETESTRING, listID, 0);

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
                                                    sources->RemoveElement(source);
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

                        String strName;
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

                case ID_FILE_EXIT:
                case ID_EXIT:
                    PostQuitMessage(0);
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
                            if(App->micVol < EPSILON)
                                App->micVol = 0.0f;

                            if(HIWORD(wParam) == VOLN_FINALVALUE)
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
                            if(App->desktopVol < EPSILON)
                                App->desktopVol = 0.0f;

                            if(HIWORD(wParam) == VOLN_FINALVALUE)
                                AppConfig->SetFloat(TEXT("Audio"), TEXT("DesktopVolume"), App->desktopVol);
                        }
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

                case ID_SOURCES:
                    if(HIWORD(wParam) == LBN_SELCHANGE)
                        App->SelectSources();
                    break;

                case ID_TESTSTREAM:
                    App->bTestStream = true;
                    App->ToggleCapturing();
                    break;

                case ID_STARTSTOP:
                    App->ToggleCapturing();
                    break;

                case IDA_SOURCE_DELETE:
                    App->DeleteItems();
                    break;

                case IDA_SOURCE_CENTER:
                    App->CenterItems();
                    break;

                case IDA_SOURCE_FITTOSCREEN:
                    App->FitItemsToScreen();
                    break;

                case IDA_SOURCE_RESETSIZE:
                    App->ResetItemSizes();
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
            }
            break;

        case WM_EXITSIZEMOVE:
            if(App->bSizeChanging)
            {
                RECT client;
                GetClientRect(hwnd, &client);

                App->ResizeWindow(true);
                App->bSizeChanging = false;

                ShowWindow(GetDlgItem(hwndMain, ID_SCENES), SW_SHOW);
                ShowWindow(GetDlgItem(hwndMain, ID_SOURCES), SW_SHOW);
                ShowWindow(GetDlgItem(hwndMain, ID_MICVOLUME), SW_SHOW);
                ShowWindow(GetDlgItem(hwndMain, ID_DESKTOPVOLUME), SW_SHOW);
                ShowWindow(GetDlgItem(hwndMain, ID_SETTINGS), SW_SHOW);
                ShowWindow(GetDlgItem(hwndMain, ID_STARTSTOP), SW_SHOW);
                ShowWindow(GetDlgItem(hwndMain, ID_SCENEEDITOR), SW_SHOW);
                ShowWindow(GetDlgItem(hwndMain, ID_EXIT), SW_SHOW);
                ShowWindow(GetDlgItem(hwndMain, ID_TESTSTREAM), SW_SHOW);
                ShowWindow(GetDlgItem(hwndMain, ID_GLOBALSOURCES), SW_SHOW);
            }
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

                int maxCX = GetSystemMetrics(SM_CXFULLSCREEN);
                int maxCY = GetSystemMetrics(SM_CYFULLSCREEN);

                if(newWidth > maxCX)
                    newWidth = maxCX;
                if(newHeight > maxCY)
                    newHeight = maxCY;

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

                if(App->clientWidth != client.right || App->clientHeight != client.bottom)
                {
                    App->clientWidth  = client.right;
                    App->clientHeight = client.bottom;
                    if(wParam != SIZE_MINIMIZED && wParam != SIZE_RESTORED && wParam != SIZE_MAXIMIZED)
                        App->bSizeChanging = true;
                    App->ResizeWindow(false);
                }
                break;
            }

        case OBS_REQUESTSTOP:
            App->Stop();

            if(wParam == 0)
            {
                if(!App->bAutoReconnect)
                    MessageBox(hwnd, Str("Connection.Disconnected"), NULL, 0);
                else
                {
                    App->bReconnecting = false;
                    DialogBox(hinstMain, MAKEINTRESOURCE(IDD_RECONNECTING), hwnd, OBS::ReconnectDialogProc);
                }
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

        case OBS_UPDATESTATUSBAR:
            App->SetStatusBarData();
            break;

        case WM_CLOSE:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

ItemModifyType GetItemModifyType(const Vect2 &mousePos, const Vect2 &itemPos, const Vect2 &itemSize)
{
    Vect2 lowerRight = itemPos+itemSize;
    if( mousePos.x < itemPos.x    ||
        mousePos.y < itemPos.y    ||
        mousePos.x > lowerRight.x ||
        mousePos.y > lowerRight.y )
    {
        return ItemModifyType_None;
    }

    if(mousePos.CloseTo(itemPos, 10.0f))
        return ItemModifyType_ScaleTopLeft;
    else if(mousePos.CloseTo(lowerRight, 10.0f))
        return ItemModifyType_ScaleBottomRight;
    else if(mousePos.CloseTo(Vect2(lowerRight.x, itemPos.y), 10.0f))
        return ItemModifyType_ScaleTopRight;
    else if(mousePos.CloseTo(Vect2(itemPos.x, lowerRight.y), 10.0f))
        return ItemModifyType_ScaleBottomLeft;

    if(CloseFloat(mousePos.x, itemPos.x, 4.0f))
        return ItemModifyType_ScaleLeft;
    else if(CloseFloat(mousePos.x, lowerRight.x, 4.0f))
        return ItemModifyType_ScaleRight;
    else if(CloseFloat(mousePos.y, itemPos.y, 4.0f))
        return ItemModifyType_ScaleTop;
    else if(CloseFloat(mousePos.y, lowerRight.y, 4.0f))
        return ItemModifyType_ScaleBottom;

    return ItemModifyType_Move;
}

enum
{
    ID_TOGGLERENDERVIEW=1,
};

LRESULT CALLBACK OBS::RenderFrameProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if(message == WM_LBUTTONDOWN)
    {
        POINTS pos;
        pos.x = (short)LOWORD(lParam);
        pos.y = (short)HIWORD(lParam);

        if(App->bEditMode && App->scene)
        {
            Vect2 mousePos = Vect2(float(pos.x), float(pos.y));
            Vect2 framePos = mousePos*(App->GetBaseSize()/App->GetRenderFrameSize());

            bool bControlDown = HIBYTE(GetKeyState(VK_LCONTROL)) != 0 || HIBYTE(GetKeyState(VK_RCONTROL)) != 0;

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
                        SendMessage(GetDlgItem(hwndMain, ID_SOURCES), LB_SELITEMRANGEEX, App->scene->NumSceneItems(), 0); 
                        App->scene->DeselectAll();
                    }

                    topItem->Select(true);
                    SendMessage(GetDlgItem(hwndMain, ID_SOURCES), LB_SETSEL, TRUE, topItem->GetID());

                    if(App->modifyType == ItemModifyType_None)
                        App->modifyType = ItemModifyType_Move;
                }
                else if(!bControlDown) //clicked on empty space without control
                {
                    SendMessage(GetDlgItem(hwndMain, ID_SOURCES), LB_SELITEMRANGEEX, App->scene->NumSceneItems(), 0); 
                    App->scene->DeselectAll();
                }
            }
            else
            {
                SceneItem *topItem = items.Last();
                App->bItemWasSelected = topItem->bSelected;
            }

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

            SceneItem *&scaleItem = App->scaleItem; //just reduces a bit of typing

            if(!App->bMouseDown)
            {
                List<SceneItem*> items;
                App->scene->GetSelectedItems(items);

                Vect2 scaleValI = (App->GetRenderFrameSize()/App->GetBaseSize());

                bool bInside = false;

                App->scaleItem = NULL;
                App->modifyType = ItemModifyType_None;

                for(int i=int(items.Num()-1); i>=0; i--)
                {
                    SceneItem *item = items[i];

                    Vect2 adjPos  = item->GetPos()  * scaleValI;
                    Vect2 adjSize = item->GetSize() * scaleValI;

                    ItemModifyType curType = GetItemModifyType(mousePos, adjPos, adjSize);
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
                Vect2 scaleVal = (baseRenderSize/App->GetRenderFrameSize());
                Vect2 framePos = mousePos*scaleVal;
                Vect2 scaleValI = 1.0f/scaleVal;

                if(!App->bMouseMoved && mousePos.Dist(App->startMousePos) > 2.0f)
                {
                    SetCapture(hwnd);
                    App->bMouseMoved = true;
                }

                if(App->bMouseMoved)
                {
                    bool bControlDown = HIBYTE(GetKeyState(VK_LCONTROL)) != 0 || HIBYTE(GetKeyState(VK_RCONTROL)) != 0;
                    bool bKeepAspect = HIBYTE(GetKeyState(VK_LSHIFT)) == 0 && HIBYTE(GetKeyState(VK_RSHIFT)) == 0;

                    List<SceneItem*> items;
                    App->scene->GetSelectedItems(items);

                    Vect2 totalAdjust = (mousePos-App->startMousePos)*scaleVal;
                    Vect2 minSize = scaleVal*21.0f;
                    Vect2 snapSize = scaleVal*10.0f;

                    Vect2 baseScale;
                    float baseScaleAspect;

                    if(scaleItem)
                    {
                        if(scaleItem->GetSource())
                        {
                            baseScale = scaleItem->GetSource()->GetSize();
                            baseScaleAspect = baseScale.x/baseScale.y;
                        }
                        else
                            bKeepAspect = false; //if the source is missing (due to invalid setting or missing plugin), don't allow aspect lock
                    }

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
                                        Vect2 bottomRight = item->pos+item->size;

                                        bool bVerticalSnap = true;
                                        if(CloseFloat(item->pos.x, 0.0f, snapSize.x))
                                            item->pos.x = 0.0f;
                                        else if(CloseFloat(bottomRight.x, baseRenderSize.x, snapSize.x))
                                            item->pos.x = baseRenderSize.x-item->size.x;
                                        else
                                            bVerticalSnap = false;

                                        bool bHorizontalSnap = true;
                                        if(CloseFloat(item->pos.y, 0.0f, snapSize.y))
                                            item->pos.y = 0.0f;
                                        else if(CloseFloat(bottomRight.y, baseRenderSize.y, snapSize.y))
                                            item->pos.y = baseRenderSize.y-item->size.y;
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

                        case ItemModifyType_ScaleBottom:
                            scaleItem->size.y = scaleItem->startSize.y+totalAdjust.y;
                            if(scaleItem->size.y < minSize.y)
                                scaleItem->size.y = minSize.y;

                            if(!bControlDown)
                            {
                                float bottom = scaleItem->pos.y+scaleItem->size.y;

                                if(CloseFloat(bottom, baseRenderSize.y, snapSize.y))
                                {
                                    bottom = baseRenderSize.y;
                                    scaleItem->size.y = bottom-scaleItem->pos.y;
                                }
                            }

                            if(bKeepAspect)
                                scaleItem->size.x = scaleItem->size.y*baseScaleAspect;
                            else
                                scaleItem->size.x = scaleItem->startSize.x;
                            break;

                        case ItemModifyType_ScaleTop:
                            scaleItem->size.y = scaleItem->startSize.y-totalAdjust.y;
                            if(scaleItem->size.y < minSize.y)
                                scaleItem->size.y = minSize.y;

                            if(!bControlDown)
                            {
                                float top = scaleItem->startPos.y+(scaleItem->startSize.y-scaleItem->size.y);
                                if(CloseFloat(top, 0.0f, snapSize.y))
                                    scaleItem->size.y = scaleItem->startPos.y+scaleItem->startSize.y;
                            }

                            if(bKeepAspect)
                                scaleItem->size.x = scaleItem->size.y*baseScaleAspect;
                            else
                                scaleItem->size.x = scaleItem->startSize.x;

                            totalAdjust.y = scaleItem->startSize.y-scaleItem->size.y;
                            scaleItem->pos.y = scaleItem->startPos.y+totalAdjust.y;
                            break;

                        case ItemModifyType_ScaleRight:
                            scaleItem->size.x = scaleItem->startSize.x+totalAdjust.x;
                            if(scaleItem->size.x < minSize.x)
                                scaleItem->size.x = minSize.x;

                            if(!bControlDown)
                            {
                                float right = scaleItem->pos.x+scaleItem->size.x;

                                if(CloseFloat(right, baseRenderSize.x, snapSize.x))
                                {
                                    right = baseRenderSize.x;
                                    scaleItem->size.x = right-scaleItem->pos.x;
                                }
                            }

                            if(bKeepAspect)
                                scaleItem->size.y = scaleItem->size.x/baseScaleAspect;
                            else
                                scaleItem->size.y = scaleItem->startSize.y;
                            break;

                        case ItemModifyType_ScaleLeft:
                            scaleItem->size.x = scaleItem->startSize.x-totalAdjust.x;
                            if(scaleItem->size.x < minSize.x)
                                scaleItem->size.x = minSize.x;

                            if(!bControlDown)
                            {
                                float left = scaleItem->startPos.x+(scaleItem->startSize.x-scaleItem->size.x);

                                if(CloseFloat(left, 0.0f, snapSize.x))
                                    scaleItem->size.x = scaleItem->startPos.x+scaleItem->startSize.x;
                            }

                            if(bKeepAspect)
                                scaleItem->size.y = scaleItem->size.x/baseScaleAspect;
                            else
                                scaleItem->size.y = scaleItem->startSize.y;

                            totalAdjust.x = scaleItem->startSize.x-scaleItem->size.x;
                            scaleItem->pos.x = scaleItem->startPos.x+totalAdjust.x;
                            break;

                        case ItemModifyType_ScaleBottomRight:
                            scaleItem->size = scaleItem->startSize+totalAdjust;
                            scaleItem->size.ClampMin(minSize);

                            if(!bControlDown)
                            {
                                Vect2 lowerRight = scaleItem->pos+scaleItem->size;

                                if(CloseFloat(lowerRight.x, baseRenderSize.x, snapSize.x))
                                {
                                    lowerRight.x = baseRenderSize.x;
                                    scaleItem->size.x = lowerRight.x-scaleItem->pos.x;
                                }
                                if(CloseFloat(lowerRight.y, baseRenderSize.y, snapSize.y))
                                {
                                    lowerRight.y = baseRenderSize.y;
                                    scaleItem->size.y = lowerRight.y-scaleItem->pos.y;
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
                            break;

                        case ItemModifyType_ScaleTopLeft:
                            scaleItem->size = scaleItem->startSize-totalAdjust;
                            scaleItem->size.ClampMin(minSize);

                            if(!bControlDown)
                            {
                                Vect2 topLeft = scaleItem->startPos+(scaleItem->startSize-scaleItem->size);

                                if(CloseFloat(topLeft.x, 0.0f, snapSize.x))
                                    scaleItem->size.x = scaleItem->startPos.x+scaleItem->startSize.x;
                                if(CloseFloat(topLeft.y, 0.0f, snapSize.y))
                                    scaleItem->size.y = scaleItem->startPos.y+scaleItem->startSize.y;
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
                            break;

                        case ItemModifyType_ScaleBottomLeft:
                            scaleItem->size.x = scaleItem->startSize.x-totalAdjust.x;
                            scaleItem->size.y = scaleItem->startSize.y+totalAdjust.y;
                            scaleItem->size.ClampMin(minSize);

                            if(!bControlDown)
                            {
                                float left = scaleItem->startPos.x+(scaleItem->startSize.x-scaleItem->size.x);
                                float bottom = scaleItem->pos.y+scaleItem->size.y;

                                if(CloseFloat(left, 0.0f, snapSize.x))
                                    scaleItem->size.x = scaleItem->startPos.x+scaleItem->startSize.x;

                                if(CloseFloat(bottom, baseRenderSize.y, snapSize.y))
                                {
                                    bottom = baseRenderSize.y;
                                    scaleItem->size.y = bottom-scaleItem->pos.y;
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
                            break;

                        case ItemModifyType_ScaleTopRight:
                            scaleItem->size.x = scaleItem->startSize.x+totalAdjust.x;
                            scaleItem->size.y = scaleItem->startSize.y-totalAdjust.y;
                            scaleItem->size.ClampMin(minSize);

                            if(!bControlDown)
                            {
                                float right = scaleItem->pos.x+scaleItem->size.x;
                                float top = scaleItem->startPos.y+(scaleItem->startSize.y-scaleItem->size.y);

                                if(CloseFloat(right, baseRenderSize.x, snapSize.x))
                                {
                                    right = baseRenderSize.x;
                                    scaleItem->size.x = right-scaleItem->pos.x;
                                }

                                if(CloseFloat(top, 0.0f, snapSize.y))
                                    scaleItem->size.y = scaleItem->startPos.y+scaleItem->startSize.y;
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

                case ItemModifyType_ScaleLeft:
                case ItemModifyType_ScaleRight:
                    SetCursor(LoadCursor(NULL, IDC_SIZEWE));
                    return 0;

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
    else if(message == WM_LBUTTONUP)
    {
        if(App->bEditMode && App->scene)
        {
            POINTS pos;
            pos.x = (short)LOWORD(lParam);
            pos.y = (short)HIWORD(lParam);

            if(App->bMouseDown)
            {
                Vect2 mousePos = Vect2(float(pos.x), float(pos.y));

                bool bControlDown = HIBYTE(GetKeyState(VK_CONTROL)) != 0;

                List<SceneItem*> items;

                if(!App->bMouseMoved)
                {
                    Vect2 framePos = mousePos*(App->GetBaseSize()/App->GetRenderFrameSize());

                    App->scene->GetItemsOnPoint(framePos, items);

                    if(bControlDown && App->bItemWasSelected)
                    {
                        HWND hwndSources = GetDlgItem(hwndMain, ID_SOURCES);

                        SceneItem *lastItem = items.Last();
                        lastItem->Select(false);
                        SendMessage(hwndSources, LB_SETSEL, FALSE, lastItem->GetID());
                    }
                    else
                    {
                        if(items.Num())
                        {
                            SceneItem *topItem = items.Last();

                            if(!bControlDown)
                            {
                                SendMessage(GetDlgItem(hwndMain, ID_SOURCES), LB_SELITEMRANGEEX, App->scene->NumSceneItems(), 0); 
                                App->scene->DeselectAll();
                            }

                            topItem->Select(true);
                            SendMessage(GetDlgItem(hwndMain, ID_SOURCES), LB_SETSEL, TRUE, topItem->GetID());

                            if(App->modifyType == ItemModifyType_None)
                                App->modifyType = ItemModifyType_Move;
                        }
                        else if(!bControlDown) //clicked on empty space without control
                        {
                            SendMessage(GetDlgItem(hwndMain, ID_SOURCES), LB_SELITEMRANGEEX, App->scene->NumSceneItems(), 0); 
                            App->scene->DeselectAll();
                        }
                    }
                }
                else
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
                    }

                    App->modifyType = ItemModifyType_None;
                }

                App->bMouseDown = false;
            }
        }
    }
    else if(message == WM_RBUTTONDOWN)
    {
        HMENU hPopup = CreatePopupMenu();
        AppendMenu(hPopup, MF_STRING | (App->bRenderViewEnabled ? MF_CHECKED : 0), ID_TOGGLERENDERVIEW, Str("RenderView.EnableView"));

        POINT p;
        GetCursorPos(&p);

        int ret = (int)TrackPopupMenuEx(hPopup, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, p.x, p.y, hwndMain, NULL);
        switch(ret)
        {
            case ID_TOGGLERENDERVIEW:
                App->bRenderViewEnabled = !App->bRenderViewEnabled;
                break;
        }

        DestroyMenu(hPopup);
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
