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


void Convert444to420(LPBYTE input, int width, int pitch, int height, LPBYTE *output, bool bSSE2Available)
{
    traceIn(Convert444to420);

    LPBYTE lumPlane     = output[0];
    LPBYTE uPlane       = output[1];
    LPBYTE vPlane       = output[2];
    int  chrPitch       = width>>1;

    if(bSSE2Available)
    {
        __m128i lumMask = _mm_set1_epi32(0x0000FF00);
        __m128i uvMask = _mm_set1_epi16(0x00FF);

        for(int y=0; y<height; y+=2)
        {
            int yPos    = y*pitch;
            int chrYPos = ((y>>1)*chrPitch);
            int lumYPos = y*width;

            for(int x=0; x<width; x+=4)
            {
                LPBYTE lpImagePos = input+yPos+(x*4);
                int chrPos  = chrYPos + (x>>1);
                int lumPos0 = lumYPos + x;
                int lumPos1 = lumPos0+width;

                __m128i line1 = _mm_load_si128((__m128i*)lpImagePos);
                __m128i line2 = _mm_load_si128((__m128i*)(lpImagePos+pitch));

                //pack lum vals
                {
                    __m128i packVal = _mm_packs_epi32(_mm_srli_si128(_mm_and_si128(line1, lumMask), 1), _mm_srli_si128(_mm_and_si128(line2, lumMask), 1));
                    packVal = _mm_packus_epi16(packVal, packVal);

                    *(LPUINT)(lumPlane+lumPos0) = packVal.m128i_u32[0];
                    *(LPUINT)(lumPlane+lumPos1) = packVal.m128i_u32[1];
                }

                //do average, pack UV vals
                {
                    __m128i addVal = _mm_add_epi64(_mm_and_si128(line1, uvMask), _mm_and_si128(line2, uvMask));
                    __m128i avgVal = _mm_srai_epi16(_mm_add_epi64(addVal, _mm_shuffle_epi32(addVal, _MM_SHUFFLE(2, 3, 0, 1))), 2);
                    avgVal = _mm_shuffle_epi32(avgVal, _MM_SHUFFLE(3, 1, 2, 0));
                    avgVal = _mm_shufflelo_epi16(avgVal, _MM_SHUFFLE(3, 1, 2, 0));
                    avgVal = _mm_packus_epi16(avgVal, avgVal);

                    DWORD packedVals = avgVal.m128i_u32[0];

                    *(LPWORD)(uPlane+chrPos) = WORD(packedVals);
                    *(LPWORD)(vPlane+chrPos) = WORD(packedVals>>16);
                }
            }
        }
    }
    else
    {
#ifdef _WIN64
        for(int y=0; y<height; y+=2)
        {
            int yPos    = y*pitch;
            int chrYPos = ((y>>1)*chrPitch);
            int lumYPos = y*width;

            for(int x=0; x<width; x+=2)
            {
                LPBYTE lpImagePos = input+yPos+(x*4);
                int chrPos  = chrYPos + (x>>1);
                int lumPos0 = lumYPos + x;
                int lumPos1 = lumPos0+width;

                QWORD pixel_0x0 = *(QWORD*)(lpImagePos);
                QWORD pixel_1x0 = *(QWORD*)(lpImagePos+pitch);

                *(LPWORD)(lumPlane+lumPos0) = WORD(((pixel_0x0>>8)&0xFF) | ((pixel_0x0>>32)&0xFF00));
                *(LPWORD)(lumPlane+lumPos1) = WORD(((pixel_1x0>>8)&0xFF) | ((pixel_1x0>>32)&0xFF00));

                DWORD pixAvg = (( pixel_0x0      & 0xFF00FF) +
                                ((pixel_0x0>>32) & 0xFF00FF) +
                                ( pixel_1x0      & 0xFF00FF) +
                                ((pixel_1x0>>32) & 0xFF00FF) ) >> 2;

                uPlane[chrPos]   = BYTE(pixAvg);
                vPlane[chrPos]   = BYTE(pixAvg>>16);
            }
        }
#else
        for(int y=0; y<height; y+=2)
        {
            int yPos    = y*pitch;
            int chrYPos = ((y>>1)*chrPitch);
            int lumYPos = y*width;

            for(int x=0; x<width; x+=2)
            {
                LPBYTE lpImagePos = input+yPos+(x*4);
                int chrPos  = chrYPos + (x>>1);
                int lumPos0 = lumYPos + x;
                int lumPos1 = lumPos0+width;

                DWORD pixel_0x0 = *(DWORD*)(lpImagePos);
                DWORD pixel_0x1 = *(DWORD*)(lpImagePos+4);
                DWORD pixel_1x0 = *(DWORD*)(lpImagePos+pitch);
                DWORD pixel_1x1 = *(DWORD*)(lpImagePos+pitch+4);

                *(LPWORD)(lumPlane+lumPos0) = WORD(((pixel_0x0>>8)&0xFF) | ((pixel_0x1)&0xFF00));
                *(LPWORD)(lumPlane+lumPos1) = WORD(((pixel_1x0>>8)&0xFF) | ((pixel_1x1)&0xFF00));

                DWORD pixAvg = ((pixel_0x0 & 0xFF00FF) +
                                (pixel_0x1 & 0xFF00FF) +
                                (pixel_1x0 & 0xFF00FF) +
                                (pixel_1x1 & 0xFF00FF) ) >> 2;

                uPlane[chrPos]   = BYTE(pixAvg);
                vPlane[chrPos]   = BYTE(pixAvg>>16);
            }
        }
#endif
    }

    traceOut;
}

