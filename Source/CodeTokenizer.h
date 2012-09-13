/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/


#pragma once


//this is a stripped down version of one of my compiler classes in my game engine

struct CodeTokenizer
{
    String dupString;
    TSTR lpCode, lpTemp;

    inline CodeTokenizer() {lpCode = lpTemp = NULL;}

    inline void SetCodeStart(CTSTR lpCodeIn)
    {
        dupString = lpCodeIn;
        lpTemp = lpCode = dupString;
    }

    BOOL GetNextToken(String &token, BOOL bPeek=FALSE);
    BOOL GetNextTokenEval(String &token, BOOL *bFloatOccurance=NULL, int curPrecedence=0);
    BOOL IsClosingToken(const String &token, CTSTR lpTokenPriority);

    BOOL PassBracers(TSTR lpCodePos);
    BOOL PassParenthesis(TSTR lpCodePos);
    BOOL PassString(TSTR lpCodePos);

    BOOL GotoToken(CTSTR lpTarget, BOOL bPassToken=FALSE);
    BOOL GotoClosingToken(CTSTR lpTokenPriority);

    static int GetTokenPrecedence(CTSTR lpToken);
};
