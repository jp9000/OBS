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

#ifdef _WIN64

void DeviceSource::PackPlanar(LPBYTE convertBuffer, LPBYTE lpPlanar)
{
    UINT halfWidth  = renderCX/2;
    UINT halfHeight = renderCY/2;

    LPBYTE output = convertBuffer;
    LPBYTE chromaPlanes[2];
    chromaPlanes[0] = lpPlanar+(renderCX*renderCY);
    chromaPlanes[1] = chromaPlanes[0]+(halfWidth*halfHeight);

    //----------------------------------------------------------
    // lum val
    DWORD size = renderCX*renderCY;
    DWORD dwQWSize = size>>3;
    QWORD *inputQW = (QWORD*)lpPlanar;
    QWORD *inputQWEnd = inputQW+dwQWSize;

    while(inputQW < inputQWEnd)
    {
        register QWORD qw = *inputQW;

        *output    = BYTE(qw);
        output[4]  = BYTE(qw>>=8);
        output[8]  = BYTE(qw>>=8);
        output[12] = BYTE(qw>>=8);
        output[16] = BYTE(qw>>=8);
        output[20] = BYTE(qw>>=8);
        output[24] = BYTE(qw>>=8);
        output[28] = BYTE(qw>>=8);

        output += 32;
        inputQW++;
    }

    LPBYTE input = (LPBYTE)inputQW;
    size &= 7;
    while(size--)
    {
        *output = *input;

        output += 4;
        input++;
    }

    //----------------------------------------------------------
    // chroma 1

    for(UINT i=0; i<2; i++)
    {
        output = convertBuffer+i+1;
        dwQWSize = halfWidth>>3;

        for(UINT y=0; y<renderCY; y++)
        {
            size = halfWidth;
            inputQW = (QWORD*)(chromaPlanes[i]+(halfWidth*(y>>1)));
            inputQWEnd = inputQW+dwQWSize;

            nop();

            while(inputQW < inputQWEnd)
            {
                register QWORD qw = *inputQW;

                *output    = BYTE(qw);
                output[4]  = BYTE(qw);
                output[8]  = BYTE(qw>>=8);
                output[12] = BYTE(qw);
                output[16] = BYTE(qw>>=8);
                output[20] = BYTE(qw);
                output[24] = BYTE(qw>>=8);
                output[28] = BYTE(qw);
                output[32] = BYTE(qw>>=8);
                output[36] = BYTE(qw);
                output[40] = BYTE(qw>>=8);
                output[44] = BYTE(qw);
                output[48] = BYTE(qw>>=8);
                output[52] = BYTE(qw);
                output[56] = BYTE(qw>>=8);
                output[60] = BYTE(qw);

                output += 64;
                inputQW++;
            }

            input = (LPBYTE)inputQW;
            size &= 7;
            while(size--)
            {
                register BYTE byte = *input;
                *output   = byte;
                output[4] = byte;

                output += 8;
                input++;
            }
        }
    }
}

#else

void DeviceSource::PackPlanar(LPBYTE convertBuffer, LPBYTE lpPlanar)
{
    UINT halfWidth  = renderCX/2;
    UINT halfHeight = renderCY/2;

    LPBYTE output = convertBuffer;
    LPBYTE chromaPlanes[2];
    chromaPlanes[0] = lpPlanar+(renderCX*renderCY);
    chromaPlanes[1] = chromaPlanes[0]+(halfWidth*halfHeight);

    //----------------------------------------------------------
    // lum val
    DWORD size = renderCX*renderCY;
    DWORD dwDWSize = size>>2;
    LPDWORD inputDW = (LPDWORD)lpPlanar;
    LPDWORD inputDWEnd = inputDW+dwDWSize;

    while(inputDW < inputDWEnd)
    {
        register DWORD dw = *inputDW;

        *output    = BYTE(dw);
        output[4]  = BYTE(dw>>=8);
        output[8]  = BYTE(dw>>=8);
        output[12] = BYTE(dw>>=8);

        output += 16;
        inputDW++;
    }

    LPBYTE input = (LPBYTE)inputDW;
    size &= 3;
    while(size--)
    {
        *output = *input;

        output += 4;
        input++;
    }

    //----------------------------------------------------------
    // chroma 1

    for(UINT i=0; i<2; i++)
    {
        output = convertBuffer+i+1;
        dwDWSize = halfWidth>>2;

        for(UINT y=0; y<renderCY; y++)
        {
            size = halfWidth;
            inputDW = (LPDWORD)(chromaPlanes[i]+(halfWidth*(y>>1)));
            inputDWEnd = inputDW+dwDWSize;

            nop();

            while(inputDW < inputDWEnd)
            {
                register DWORD dw = *inputDW;

                *output    = BYTE(dw);
                output[4]  = BYTE(dw);
                output[8]  = BYTE(dw>>=8);
                output[12] = BYTE(dw);
                output[16] = BYTE(dw>>=8);
                output[20] = BYTE(dw);
                output[24] = BYTE(dw>>=8);
                output[28] = BYTE(dw);

                output += 32;
                inputDW++;
            }

            input = (LPBYTE)inputDW;
            size &= 3;
            while(size--)
            {
                register BYTE byte = *input;
                *output   = byte;
                output[4] = byte;

                output += 8;
                input++;
            }
        }
    }
}

#endif

/*void DeviceSource::PackPlanar(LPBYTE lpPlanar)
{
    LPBYTE output = convertBuffer;
    LPBYTE input = lpPlanar;

    //lum val
    for(UINT y=0; y<renderCY; y++)
    {
        for(UINT x=0; x<renderCX; x++)
        {
            *output = *input;

            output += 4;
            input++;
        }
    }

    //chroma 1
    output = convertBuffer+1;
    LPBYTE plane1 = lpPlanar+(renderCX*renderCY);
    UINT halfWidth  = renderCX/2;
    UINT halfHeight = renderCY/2;

    for(UINT y=0; y<renderCY; y++)
    {
        input = plane1+(halfWidth*(y/2));

        for(UINT x=0; x<halfWidth; x++)
        {
            *output = *input;
            output += 4;
            *output = *input;
            output += 4;

            input++;
        }
    }

    //chroma 1
    output = convertBuffer+2;
    LPBYTE plane2 = plane1+(halfWidth*halfHeight);

    for(UINT y=0; y<renderCY; y++)
    {
        input = plane2+(halfWidth*(y/2));

        for(UINT x=0; x<halfWidth; x++)
        {
            *output = *input;
            output += 4;
            *output = *input;
            output += 4;

            input++;
        }
    }
}*/

void DeviceSource::Convert422To444(LPBYTE convertBuffer, LPBYTE lp422, bool bLeadingY)
{
    LPDWORD output = (LPDWORD)convertBuffer;

    //---------------------------------

    UINT halfWidth = renderCX>>1;
    DWORD size = renderCY*halfWidth*4;
    DWORD dwDWSize = size>>2;
    LPDWORD inputDW = (LPDWORD)lp422;
    LPDWORD inputDWEnd = inputDW+dwDWSize;

    if(bLeadingY)
    {
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
    else
    {
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
