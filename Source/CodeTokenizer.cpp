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


#include "Main.h"



#define NUMCLOSINGTOKENS 29

static const TCHAR *ClosingTokens[NUMCLOSINGTOKENS] = {TEXT("}"),  TEXT(";"),  TEXT(")"),  TEXT(","),  TEXT("]"),
                                                       TEXT("?"),  TEXT(":"),  TEXT("&&"), TEXT("||"), TEXT("<"),  
                                                       TEXT(">"),  TEXT("<="), TEXT(">="), TEXT("=="), TEXT("!="), 
                                                       TEXT("="),  TEXT("-="), TEXT("+="), TEXT("/="), TEXT("*="), 
                                                       TEXT("<<"), TEXT(">>"), TEXT("-"),  TEXT("+"),  TEXT("/"),  
                                                       TEXT("%"),  TEXT("*"),  TEXT("|"),  TEXT("&")};

static const int TokenPrecedence[NUMCLOSINGTOKENS] = {0,  1,  2,  2,  2,
                                                      3,  3,  4,  5,  6,  
                                                      6,  7,  7,  7,  7,
                                                      8,  9,  9,  10, 10,
                                                      11, 11, 12, 12, 13,
                                                      13, 13, 14, 14};

BOOL CodeTokenizer::GetNextToken(String &token, BOOL bPeek)
{
    TSTR lpStart = lpTemp;

    TSTR lpTokenStart = NULL;
    BOOL bAlphaNumeric = FALSE;

    while(*lpTemp)
    {
        if(mcmp(lpTemp, TEXT("//"), 2*sizeof(TCHAR)))
        {
            lpTemp = schr(lpTemp, '\n');

            if(!lpTemp)
                return FALSE;
        }
        else if(mcmp(lpTemp, TEXT("/*"), 2*sizeof(TCHAR)))
        {
            lpTemp = sstr(lpTemp+2, TEXT("*/"));

            if(!lpTemp)
                return FALSE;

            lpTemp += 2;
        }

        if((*lpTemp == '_') || iswalnum(*lpTemp))
        {
            if(lpTokenStart)
            {
                if(!bAlphaNumeric)
                    break;
            }
            else
            {
                lpTokenStart = lpTemp;
                bAlphaNumeric = TRUE;
            }
        }
        else
        {
            if(lpTokenStart)
            {
                if(bAlphaNumeric)
                    break;

                if(*lpTokenStart == '>' || *lpTokenStart == '<')
                {
                    if((*lpTemp != '=') && (*lpTemp != '>') && (*lpTemp != '<'))
                        break;
                }

                if( ((*lpTokenStart == '=') && (*lpTemp != '=')) ||
                    (*lpTokenStart == ';') ||
                    (*lpTemp == ' ')   ||
                    (*lpTemp == L'@') ||
                    (*lpTemp == '\'')  ||
                    (*lpTemp == '"')   ||
                    (*lpTemp == ';')   ||
                    (*lpTemp == '(')   ||
                    (*lpTemp == ')')   ||
                    (*lpTemp == '[')   ||
                    (*lpTemp == ']')   ||
                    (*lpTemp == '{')   ||
                    (*lpTemp == '}')   ||
                    (*lpTemp == '\r')  ||
                    (*lpTemp == '\t')  ||
                    (*lpTemp == '\n')  )
                {
                    break;
                }
            }
            else
            {
                if(*lpTemp == '"')
                {
                    lpTokenStart = lpTemp;

                    BOOL bFoundEnd = TRUE;
                    while(*++lpTemp != '"')
                    {
                        if(!*lpTemp)
                        {
                            bFoundEnd = FALSE;
                            break;
                        }
                    }

                    if(!bFoundEnd)
                        return FALSE;

                    ++lpTemp;
                    break;
                }
                if(*lpTemp == ';')
                {
                    lpTokenStart = lpTemp;
                    ++lpTemp;
                    break;
                }

                if(*lpTemp == '\'')
                {
                    lpTokenStart = lpTemp;

                    BOOL bFoundEnd = TRUE;
                    while(*++lpTemp != '\'')
                    {
                        if(!*lpTemp)
                        {
                            bFoundEnd = FALSE;
                            break;
                        }
                    }

                    if(!bFoundEnd)
                        return FALSE;

                    ++lpTemp;
                    break;
                }
                else if((*lpTemp == '(') ||
                        (*lpTemp == ')') ||
                        (*lpTemp == '[') ||
                        (*lpTemp == ']') ||
                        (*lpTemp == '{') ||
                        (*lpTemp == '}'))
                {
                    lpTokenStart = lpTemp++;
                    break;
                }

                if( (*lpTemp != ' ')   &&
                    (*lpTemp != L'@') &&
                    (*lpTemp != '\r')  &&
                    (*lpTemp != '\t')  &&
                    (*lpTemp != '\n')  )
                {
                    lpTokenStart = lpTemp;
                    bAlphaNumeric = FALSE;
                }
            }
        }

        ++lpTemp;
    }

    if(!lpTokenStart)
        return FALSE;

    TCHAR oldCH = *lpTemp;
    *lpTemp = 0;

    token = lpTokenStart;

    *lpTemp = oldCH;

    if(bAlphaNumeric && iswdigit(*lpTokenStart)) //handle floating points
    {
        if( (token.Length() > 2) && 
            (lpTokenStart[0] == '0') &&
            (lpTokenStart[1] == 'x')) //convert hex
        {
            unsigned int val = tstring_base_to_uint(lpTokenStart, NULL, 0);
            token = FormattedString(TEXT("%d"), val);
        }
        else
        {
            String nextToken;

            TSTR lpPos = lpTemp;
            if(!GetNextToken(nextToken)) return FALSE;
            if(nextToken[0] == '.')
            {
                lpPos = lpTemp;

                token << nextToken;
                if(!GetNextToken(nextToken)) return FALSE;
                if(iswdigit(nextToken[0]) || nextToken == TEXT("f"))
                    token << nextToken;
                else
                    lpTemp = lpPos;
            }
            else
                lpTemp = lpPos;

            if(token[token.Length()-1] == 'e')
            {
                if(*lpTemp == '-')
                {
                    TSTR lpPos = lpTemp++;

                    if(!GetNextToken(nextToken)) return FALSE;
                    if(!iswdigit(nextToken[0]))
                        lpTemp = lpPos;
                    else
                        token << TEXT("-") << nextToken;
                }
            }

            lpPos = lpTemp;
            if(!GetNextToken(nextToken)) return FALSE;
            if(nextToken[0] == '.')
            {
                lpPos = lpTemp;

                token << nextToken;
                if(!GetNextToken(nextToken)) return FALSE;
                if(iswdigit(nextToken[0]) || nextToken == TEXT("f"))
                    token << nextToken;
                else
                    lpTemp = lpPos;
            }
            else
                lpTemp = lpPos;
        }
    }

    if(bPeek)
        lpTemp = lpStart;

    return TRUE;
}

BOOL CodeTokenizer::GetNextTokenEval(String &token, BOOL *bFloatOccurance, int curPrecedence)
{
    TSTR lpLastSafePos = lpTemp;
    String curVal, curToken;
    BOOL bFoundNumber = FALSE;

    if(!GetNextToken(curToken)) return FALSE;

    if(curToken == TEXT("("))
    {
        TSTR lpPrevPos = lpTemp;
        int newPrecedence = GetTokenPrecedence(curToken);
        if(!GetNextTokenEval(curToken, bFloatOccurance, newPrecedence)) return FALSE;

        if(!ValidFloatString(curToken))
        {
            lpTemp = lpPrevPos;
            token = TEXT("(");
            return TRUE;
        }

        String nextToken;
        if(!GetNextToken(nextToken)) return FALSE;
        if(nextToken != TEXT(")"))
        {
            lpTemp = lpPrevPos;
            token = TEXT("(");
            return TRUE;
        }
    }

    if(curToken == TEXT("-") && iswdigit(*lpTemp))
    {
        String nextToken;
        if(!GetNextToken(nextToken)) return FALSE;
        curToken << nextToken;
    }

    if(ValidFloatString(curToken))
    {
        bFoundNumber = TRUE;
        curVal = curToken;

        if(!ValidIntString(curVal) && bFloatOccurance)
            *bFloatOccurance = TRUE;
    }
    else
    {
        if(bFoundNumber)
        {
            lpTemp = lpLastSafePos;
            token = curVal;
            return TRUE;
        }
        else
        {
            token = curToken;
            return TRUE;
        }
    }

    lpLastSafePos = lpTemp;

    String operatorToken;
    while(GetNextToken(operatorToken))
    {
        int newPrecedence = GetTokenPrecedence(operatorToken);
        if(newPrecedence <= curPrecedence)
        {
            lpTemp = lpLastSafePos;
            token = curVal;
            return TRUE;
        }

        String nextVal;
        if(!GetNextTokenEval(nextVal, bFloatOccurance, newPrecedence)) return FALSE;

        if(!ValidFloatString(nextVal))
        {
            lpTemp = lpLastSafePos;
            token = curVal;
            return TRUE;
        }

        if(operatorToken == TEXT("<<"))
        {
            int val1 = tstoi(curVal);
            int val2 = tstoi(nextVal);

            val1 <<= val2;
            curVal = IntString(val1);
        }
        else if(operatorToken == TEXT(">>"))
        {
            int val1 = tstoi(curVal);
            int val2 = tstoi(nextVal);

            val1 >>= val2;
            curVal = IntString(val1);
        }
        else if(operatorToken == TEXT("*"))
        {
            float val1 = (float)tstof(curVal);
            float val2 = (float)tstof(nextVal);

            val1 *= val2;
            curVal = FormattedString(TEXT("%g"), val1);
        }
        else if(operatorToken == TEXT("/"))
        {
            float val1 = (float)tstof(curVal);
            float val2 = (float)tstof(nextVal);

            val1 /= val2;
            curVal = FormattedString(TEXT("%g"), val1);
        }
        else if(operatorToken == TEXT("+"))
        {
            float val1 = (float)tstof(curVal);
            float val2 = (float)tstof(nextVal);

            val1 += val2;
            curVal = FormattedString(TEXT("%g"), val1);
        }
        else if(operatorToken == TEXT("-"))
        {
            float val1 = (float)tstof(curVal);
            float val2 = (float)tstof(nextVal);

            val1 -= val2;
            curVal = FormattedString(TEXT("%g"), val1);
        }
        else if(operatorToken == TEXT("|"))
        {
            int val1 = tstoi(curVal);
            int val2 = tstoi(nextVal);

            val1 |= val2;
            curVal = IntString(val1);
        }
        else if(operatorToken == TEXT("&"))
        {
            int val1 = tstoi(curVal);
            int val2 = tstoi(nextVal);

            val1 &= val2;
            curVal = IntString(val1);
        }
        else
        {
            lpTemp = lpLastSafePos;
            token = curVal;
            return TRUE;
        }

        lpLastSafePos = lpTemp;
    }

    return FALSE;
}

BOOL CodeTokenizer::IsClosingToken(const String &token, CTSTR lpTokenPriority)
{
    DWORD i;

    String strPriority = lpTokenPriority;
    int priorityPrecedence = 0, tokenPrecedence = 14;

    for(i=0; i<NUMCLOSINGTOKENS; i++)
    {
        if(token.Compare(ClosingTokens[i]))
            tokenPrecedence = TokenPrecedence[i];

        if(strPriority.Compare(ClosingTokens[i]))
            priorityPrecedence = TokenPrecedence[i];
    }

    return tokenPrecedence <= priorityPrecedence;
}

BOOL CodeTokenizer::PassBracers(TSTR lpCodePos)
{
    lpTemp = lpCodePos;

    String curToken;

    if(!GetNextToken(curToken))
        return FALSE;
    if(curToken[0] != '{')
        return FALSE;

    while(GetNextToken(curToken, TRUE))
    {
        if(curToken[0] == '}')
        {
            GetNextToken(curToken);
            return TRUE;
        }
        else if(curToken[0] == '{')
        {
            PassBracers(lpTemp);
            continue;
        }
        else if(curToken[0] == '"')
        {
            PassString(lpTemp);
            continue;
        }

        GetNextToken(curToken);
    };

    return FALSE;
}

BOOL CodeTokenizer::PassParenthesis(TSTR lpCodePos)
{
    lpTemp = lpCodePos;

    String curToken;

    if(!GetNextToken(curToken))
        return FALSE;
    if(curToken[0] != '(')
        return FALSE;

    while(GetNextToken(curToken, TRUE))
    {
        if(curToken[0] == ')')
        {
            GetNextToken(curToken);
            return TRUE;
        }
        else if(curToken[0] == '(')
        {
            PassParenthesis(lpTemp);
            continue;
        }
        else if(curToken[0] == '{')
        {
            PassBracers(lpTemp);
            continue;
        }
        else if(curToken[0] == '"')
        {
            PassString(lpTemp);
            continue;
        }
        GetNextToken(curToken);
    }

    return FALSE;
}

BOOL CodeTokenizer::PassString(TSTR lpCodePos)
{
    TSTR lpNextLeft = schr(lpCodePos, '"');
    TSTR lpNextRight = lpNextLeft;

    while(1)
    {
        lpNextRight = schr(lpNextRight+1, '"');
        if(!lpNextRight)
            return FALSE;

        if(lpNextRight[-1] != '\\')
            break;
    }

    lpTemp = lpNextRight+1;

    return TRUE;
}

BOOL CodeTokenizer::GotoToken(CTSTR lpTarget, BOOL bPassToken)
{
    String curToken;

    while(GetNextToken(curToken, TRUE))
    {
        if(curToken == lpTarget)
        {
            if(bPassToken)
                GetNextToken(curToken);
            return TRUE;
        }
        else if(curToken[0] == '{')
        {
            PassBracers(lpTemp);
            continue;
        }
        else if(curToken[0] == '(')
        {
            PassParenthesis(lpTemp);
            continue;
        }
        GetNextToken(curToken);
    }

    return FALSE;
}

BOOL CodeTokenizer::GotoClosingToken(CTSTR lpTokenPriority)
{
    String curToken;

    while(GetNextToken(curToken, TRUE))
    {
        if(IsClosingToken(curToken, lpTokenPriority))
            return TRUE;
        else if(curToken[0] == '{')
        {
            PassBracers(lpTemp);
            continue;
        }
        else if(curToken[0] == '(')
        {
            PassParenthesis(lpTemp);
            continue;
        }
        else if(curToken[0] == '"')
        {
            PassString(lpTemp-1);
            continue;
        }

        GetNextToken(curToken);
    }

    return FALSE;
}

int CodeTokenizer::GetTokenPrecedence(CTSTR lpToken)
{
    for(int i=0; i<NUMCLOSINGTOKENS; i++)
    {
        if(!scmp(ClosingTokens[i], lpToken))
            return TokenPrecedence[i];
    }

    return 0;
}
