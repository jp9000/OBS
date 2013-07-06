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


#include "DShowPlugin.h"

//now properly takes CPU cache into account - it's just so much faster than it was.
void PackPlanar(LPBYTE convertBuffer, LPBYTE lpPlanar, UINT renderCX, UINT renderCY, UINT pitch, UINT startY, UINT endY, UINT linePitch, UINT lineShift)
{
    LPBYTE output = convertBuffer;
    LPBYTE input = lpPlanar + lineShift;
    LPBYTE input2 = input+(renderCX*renderCY);
    LPBYTE input3 = input2+(renderCX*renderCY/4);

    UINT halfStartY = startY/2;
    UINT halfX = renderCX/2;
    UINT halfY = endY/2;

    for(UINT y=halfStartY; y<halfY; y++)
    {
        LPBYTE lpLum1 = input + y*2*linePitch;
        LPBYTE lpLum2 = lpLum1 + linePitch;
        LPBYTE lpChroma1 = input2 + y*(linePitch/2);
        LPBYTE lpChroma2 = input3 + y*(linePitch/2);
        LPDWORD output1 = (LPDWORD)(output + (y*2)*pitch);
        LPDWORD output2 = (LPDWORD)(((LPBYTE)output1)+pitch);

        for(UINT x=0; x<halfX; x++)
        {
            DWORD out = (*(lpChroma1++) << 8) | (*(lpChroma2++) << 16);

            *(output1++) = *(lpLum1++) | out;
            *(output1++) = *(lpLum1++) | out;

            *(output2++) = *(lpLum2++) | out;
            *(output2++) = *(lpLum2++) | out;
        }
    }
}

void DeviceSource::Convert422To444(LPBYTE convertBuffer, LPBYTE lp422, UINT pitch, bool bLeadingY)
{
    DWORD size = lineSize;
    DWORD dwDWSize = size>>2;

    if(bLeadingY)
    {
        for(UINT y=0; y<renderCY; y++)
        {
            LPDWORD output = (LPDWORD)(convertBuffer+(y*pitch));
            LPDWORD inputDW = (LPDWORD)(lp422+(y*linePitch)+lineShift);
            LPDWORD inputDWEnd = inputDW+dwDWSize;

            while(inputDW < inputDWEnd)
            {
                register DWORD dw = *inputDW;

                output[0] = dw;
                dw &= 0xFFFFFF00;
                dw |= BYTE(dw>>16);
                output[1] = dw;

                output += 2;
                inputDW++;
            }
        }
    }
    else
    {
        for(UINT y=0; y<renderCY; y++)
        {
            LPDWORD output = (LPDWORD)(convertBuffer+(y*pitch));
            LPDWORD inputDW = (LPDWORD)(lp422+(y*linePitch)+lineShift);
            LPDWORD inputDWEnd = inputDW+dwDWSize;

            while(inputDW < inputDWEnd)
            {
                register DWORD dw = *inputDW;

                output[0] = dw;
                dw &= 0xFFFF00FF;
                dw |= (dw>>16) & 0xFF00;
                output[1] = dw;

                output += 2;
                inputDW++;
            }
        }
    }
}
