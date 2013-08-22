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


#define COLOR_CONTROL_CLASS TEXT("OBSColorControl")

BASE_EXPORT void InitColorControl(HINSTANCE hInstance);

BASE_EXPORT void CCSetCustomColors(DWORD *colors);
BASE_EXPORT void CCGetCustomColors(DWORD *colors);

BASE_EXPORT DWORD CCGetColor(HWND hwnd);
BASE_EXPORT void  CCSetColor(HWND hwnd, DWORD color);
BASE_EXPORT void  CCSetColor(HWND hwnd, const Color3 &color);


#define CCN_CHANGED     0
