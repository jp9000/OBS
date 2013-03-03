/********************************************************************************
 Copyright (C) 2012 Bill Hamilton
                    Hugh Bailey <obs.jim@gmail.com>

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

#define VOLUME_METER_CLASS TEXT("OBSVolumeMeter")

BASE_EXPORT void InitVolumeMeter(HINSTANCE hInst);
BASE_EXPORT float SetVolumeMeterValue(HWND hwnd, float fTopVal, float fTopMax, float fTopPeak, float fBotVal = FLT_MIN, float fBotMax = FLT_MIN, float fBotPeak = FLT_MIN);

#define VOL_MIN -96
#define VOL_MAX 0

#define VOLN_METERED  0x302