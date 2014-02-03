/********************************************************************************
 Copyright (C) 2001-2012 Hugh Bailey <obs.jim@gmail.com>

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


#include "XT.h"

//forgive the mess


#if defined(WIN32)
    #define WCHARMASK 0xFFFF
    #define WCHARBITS 16
#else
    #define UTF32 1
    #define WCHARMASK 0xFFFFFFFF
    #define WCHARBITS 32
#endif

#ifdef UNICODE
    #define TCHARMASK WCHARMASK
    #define TCHARBITS WCHARBITS
#else
    #define TCHARMASK 0xFF
    #define TCHARBITS 8
#endif

#ifdef C64
    #define MEMBOUNDRY 7
#else
    #define MEMBOUNDRY 3
#endif


String::String()
{
    curLength = 0;
    lpString = NULL;
}

String::String(LPCSTR str)
{
#ifdef UNICODE
    if(!str)
    {
        curLength = 0;
        lpString = NULL;
        return;
    }

    size_t utf8Len = strlen(str);
    curLength = (UINT)utf8_to_wchar_len(str, utf8Len, 0);

    if(curLength)
    {
        lpString = (TSTR)Allocate((curLength+1)*sizeof(wchar_t));
        utf8_to_wchar(str, utf8Len+1, lpString, curLength+1, 0);
    }
    else
        lpString = NULL;
#else
    if(!str)
    {
        curLength = 0;
        lpString = NULL;
        return;
    }

    curLength = slen(str);

    if(curLength)
    {
        lpString = (TSTR)Allocate(curLength+1);
        scpy(lpString, str);
    }
    else
        lpString = NULL;
#endif
}

String::String(CWSTR str)
{
#ifdef UNICODE
    if(!str)
    {
        curLength = 0;
        lpString = NULL;
        return;
    }

    curLength = slen(str);

    if(curLength)
    {
        lpString = (TSTR)Allocate((curLength+1)*sizeof(TCHAR));
        scpy(lpString, str);
    }
    else
        lpString = NULL;
#else
    if(!str)
    {
        curLength = 0;
        lpString = NULL;
        return;
    }

    size_t wideLen = wcslen(str);
    curLength = (UINT)wchar_to_utf8_len(str, wideLen, 0);

    if(curLength)
    {
        lpString = (TSTR)Allocate(curLength+1);
        wchar_to_utf8(str, wideLen+1, lpString, curLength+1, 0);
    }
    else
        lpString = NULL;
#endif
}

String::String(const String &str)
{
    curLength = str.curLength;
    if(curLength)
    {
        lpString = (TSTR)Allocate((curLength+1)*sizeof(TCHAR));
        scpy(lpString, str.lpString);
    }
    else
        lpString = NULL;
}


String::~String()
{
    if(lpString)
        Free(lpString);
}


String& String::operator=(CTSTR str)
{
    if(!str)
        curLength = 0;
    else
        curLength = slen(str);

    if(curLength)
    {
        lpString = (TSTR)ReAllocate(lpString, (curLength+1)*sizeof(TCHAR));
        scpy(lpString, str);
    }
    else
    {
        if(lpString)
            Free(lpString);
        lpString = NULL;
    }

    return *this;
}

String& String::operator+=(CTSTR str)
{
    UINT strLength;
    
    if(!str)
        strLength = 0;
    else
        strLength = slen(str);

    if(!strLength)
        return *this;

    curLength += strLength;

    if(curLength)
    {
        if(lpString)
        {
            lpString = (TSTR)ReAllocate(lpString, (curLength+1)*sizeof(TCHAR));
            scat(lpString, str);
        }
        else
        {
            lpString = (TSTR)Allocate((curLength+1)*sizeof(TCHAR));
            scpy(lpString, str);
        }
    }
    else
    {
        if(lpString)
            Free(lpString);
        lpString = NULL;
    }

    return *this;
}

String  String::operator+(CTSTR str) const
{
    return (String(*this) += str);
}


String& String::operator=(TCHAR ch)
{
    curLength = 1;
    lpString = (TSTR)ReAllocate(lpString, 2);
    *lpString = ch;
    lpString[1] = 0;

    return *this;
}

String& String::operator+=(TCHAR ch)
{
    ++curLength;
    lpString = (TSTR)ReAllocate(lpString, (curLength+1)*sizeof(TCHAR));
    lpString[curLength-1] = ch;
    lpString[curLength]   = 0;

    return *this;
}

String  String::operator+(TCHAR ch) const
{
    return String(*this) += ch;
}


String& String::operator=(int number)
{
    TCHAR strNum[16];
    itots_s(number, strNum, 15, 10);

    return *this = strNum;
}

String& String::operator+=(int number)
{
    TCHAR strNum[16];
    itots_s(number, strNum, 15, 10);

    return *this += strNum;
}

String  String::operator+(int number) const
{
    TCHAR strNum[16];
    itots_s(number, strNum, 15, 10);

    return *this + strNum;
}


String& String::operator=(unsigned int unumber)
{
    TCHAR strNum[16];
    uitots_s(unumber, strNum, 15, 10);

    return *this = strNum;
}

String& String::operator+=(unsigned int unumber)
{
    TCHAR strNum[16];
    uitots_s(unumber, strNum, 15, 10);

    return *this += strNum;
}

String  String::operator+(unsigned int unumber) const
{
    TCHAR strNum[16];
    uitots_s(unumber, strNum, 15, 10);

    return *this + strNum;
}

String& String::operator=(const String &str)
{
    curLength = str.curLength;

    if(curLength)
    {
        lpString = (TSTR)ReAllocate(lpString, (curLength+1)*sizeof(TCHAR));
        scpy(lpString, str.lpString);
    }
    else
    {
        if(lpString)
            Free(lpString);
        lpString = NULL;
    }

    return *this;
}

String& String::operator+=(const String &str)
{
    if(!str.curLength)
        return *this;

    curLength += str.curLength;

    if(curLength)
    {
        if(lpString)
        {
            lpString = (TSTR)ReAllocate(lpString, (curLength+1)*sizeof(TCHAR));
            scat(lpString, str.lpString);
        }
        else
        {
            lpString = (TSTR)Allocate((curLength+1)*sizeof(TCHAR));
            scpy(lpString, str.lpString);
        }
    }
    else
    {
        if(lpString)
            Free(lpString);
        lpString = NULL;
    }

    return *this;
}

String  String::operator+(const String &str) const
{
    return (String(*this) += str);
}


String& String::operator<<(CTSTR str)
{
    return (*this += str);
}

String& String::operator<<(TCHAR ch)
{
    return (*this += ch);
}

String& String::operator<<(int number)
{
    return (*this += number);
}

String& String::operator<<(unsigned int unumber)
{
    return (*this += unumber);
}

String& String::operator<<(const String &str)
{
    return (*this += str);
}


/*TCHAR& String::operator[](unsigned int index)
{
    assert(index < curLength);
    //if(index >= curLength)
    //    return 0;

    return lpString[index];
}*/


BOOL String::Compare(CTSTR str) const
{
    if(lpString)
    {
        if(!str)
            return (lpString[0] == 0);

        return scmp(lpString, str) == 0;
    }
    else
    {
        return !str || (str[0] == 0);
    }
}

BOOL String::CompareI(CTSTR str) const
{
    if(lpString)
    {
        if(!str)
            return (lpString[0] == 0);

        return scmpi(lpString, str) == 0;
    }
    else
    {
        return !str || (str[0] == 0);
    }
}


LPSTR String::CreateUTF8String()
{
    return (!lpString) ? NULL : tstr_createUTF8(lpString);
}


String& String::FindReplace(CTSTR strFind, CTSTR strReplace)
{
    if(!lpString)
        return *this;

    if(!strReplace) strReplace = TEXT("");

    int findLen = slen(strFind), replaceLen = slen(strReplace);
    TSTR lpTemp = lpString;

    if(replaceLen < findLen)
    {
        int nOccurences = 0;

        while(lpTemp = sstr(lpTemp, strFind))
        {
            TSTR lpEndSegment = lpTemp+findLen;
            UINT endLen = slen(lpEndSegment);

            if(endLen)
            {
                mcpy(lpTemp+replaceLen, lpEndSegment, (endLen+1)*sizeof(TCHAR));
                mcpy(lpTemp, strReplace, replaceLen*sizeof(TCHAR));
            }
            else
                scpy(lpTemp, strReplace);

            lpTemp += replaceLen;
            ++nOccurences;
        }

        if(nOccurences)
            curLength += (replaceLen-findLen)*nOccurences;
    }
    else if(replaceLen == findLen)
    {
        while(lpTemp = sstr(lpTemp, strFind))
        {
            mcpy(lpTemp, strReplace, replaceLen*sizeof(TCHAR));
            lpTemp += replaceLen;
        }
    }
    else
    {
        int nOccurences = 0;

        while(lpTemp = sstr(lpTemp, strFind))
        {
            lpTemp += findLen;
            ++nOccurences;
        }

        if(nOccurences)
        {
            curLength += (replaceLen-findLen)*nOccurences;

            lpTemp = lpString = (TSTR)ReAllocate(lpString, (curLength+1)*sizeof(TCHAR));

            while(lpTemp = sstr(lpTemp, strFind))
            {
                TSTR lpEndSegment = lpTemp+findLen;
                UINT endLen = slen(lpEndSegment);

                if(endLen)
                {
                    mcpyrev(lpTemp+replaceLen, lpEndSegment, (endLen+1)*sizeof(TCHAR));
                    mcpy(lpTemp, strReplace, replaceLen*sizeof(TCHAR));
                }
                else
                    scpy(lpTemp, strReplace);

                lpTemp += replaceLen;
            }
        }
    }

    return *this;
}

void String::InsertString(UINT dwPos, CTSTR str)
{
    assert(str);
    if(!str) return;

    assert(dwPos <= curLength);

    if(dwPos == curLength)
    {
        AppendString(str);
        return;
    }

    UINT strLength = slen(str);

    if(strLength)
    {
        lpString = (TSTR)ReAllocate(lpString, (curLength+strLength+1)*sizeof(TCHAR));

        TSTR lpPos = lpString+dwPos;
        mcpyrev(lpPos+strLength, lpPos, ((curLength+1)-dwPos)*sizeof(TCHAR));
        mcpy(lpPos, str, strLength*sizeof(TCHAR));

        curLength += strLength;
    }
}

String& String::AppendString(CTSTR str, UINT count)
{
    UINT strLength;
    
    if(!str)
        strLength = 0;
    else if(count)
        strLength = MIN(slen(str), count);
    else
        strLength = slen(str);

    if(!strLength)
        return *this;

    UINT oldLen = curLength;

    curLength += strLength;

    if(curLength)
    {
        lpString = (TSTR)ReAllocate(lpString, (curLength+1)*sizeof(TCHAR));
        scpy_n(lpString+oldLen, str, strLength);
    }
    else
    {
        if(lpString)
            Free(lpString);
        lpString = NULL;
    }

    return *this;
}

UINT String::GetLinePos(UINT dwLine)
{
    assert(lpString);
    if(!lpString)
        return 0;

    if(!dwLine)
        return 0;

    TSTR lpTemp = lpString;

    for(UINT i=0; i<dwLine; i++)
    {
        TSTR lpNewLine = schr(lpTemp, '\n');
        if(!lpNewLine)
            break;

        lpTemp = lpNewLine+1;
    }

    return UINT(lpTemp-lpString);
}

String& String::Clear()
{
    if(lpString)
        Free(lpString);
    lpString = NULL;
    curLength = 0;

    return *this;
}


String& String::GroupDigits()
{
    UINT dwGroups = (curLength-1)/3;
    UINT oldLen = curLength;

    for(UINT i=1; i<=dwGroups; i++)
    {
        this->InsertChar(oldLen-(3*i), ',');
    }
    return *this;
}


int String::NumTokens(TCHAR token) const
{
    if(IsEmpty())
        return 0;

    TSTR lpTemp = lpString;
    UINT count = 0;

    while(lpTemp = schr(lpTemp, token))
    {
        ++lpTemp;
        ++count;
    }

    return count+1;
}

String String::GetToken(int id, TCHAR token) const
{
    TSTR lpTemp = lpString;
    UINT curTokenID = 0;

    do
    {
        if(curTokenID == id)
        {
            TSTR lpNextSeperator = schr(lpTemp, token);

            if(lpNextSeperator)
                *lpNextSeperator = 0;

            String newString(lpTemp);

            if(lpNextSeperator)
                *lpNextSeperator = token;

            return newString;
        }

        ++curTokenID;
    } while((lpTemp = schr(lpTemp, token)+1) != (TSTR)sizeof(TCHAR));

    AppWarning(TEXT("Bad String token, token %d, seperator '%c', string \"%s\""), id, token, lpString);
    return String();
}

void String::GetTokenList(StringList &strList, TCHAR token, BOOL bIncludeEmpty) const
{
    TSTR lpTemp = lpString;

    do
    {
        TSTR lpNextSeperator = schr(lpTemp, token);

        if(lpNextSeperator)
            *lpNextSeperator = 0;

        if(*lpTemp || bIncludeEmpty)
        {
            String &newString = *strList.CreateNew();
            newString = lpTemp;
        }

        if(lpNextSeperator)
            *lpNextSeperator = token;

    } while((lpTemp = schr(lpTemp, token)+1) != (TSTR)sizeof(TCHAR));
}

CTSTR String::GetTokenOffset(int token, TCHAR seperator) const
{
    TSTR lpTemp = lpString;
    UINT curToken = 0;

    do
    {
        if(curToken == token)
            return lpTemp;

        ++curToken;
    } while((lpTemp = schr(lpTemp, seperator)+1) != (TSTR)sizeof(TCHAR));

    return NULL;
}

String  String::Left(UINT iOffset)
{
    if(iOffset > curLength)
    {
        AppWarning(TEXT("Bad call to String::Left.  iOffset is bigger than the current length."));
        return String();
    }

    return String(*this).SetLength(iOffset);
}

String  String::Mid(UINT iStart, UINT iEnd)
{
    if (!IsValid())
        return String();

    if( (iStart >= curLength) ||
        (iEnd > curLength || iEnd <= iStart)   )
    {
        AppWarning(TEXT("Bad call to String::Mid.  iStart or iEnd is bigger than the current length (string: %s)."), lpString);
        return String();
    }

    String newString = lpString+iStart;
    return newString.SetLength(iEnd-iStart);
}

String  String::Right(UINT iOffset)
{
    if(iOffset > curLength)
    {
        AppWarning(TEXT("Bad call to String::Right.  iOffset is bigger than the current length."));
        return String();
    }

    return String((lpString+curLength)-iOffset);
}


String& String::SetLength(UINT length)
{
    UINT oldLength = curLength;

    curLength = length;
    if(curLength)
    {
        lpString = (TSTR)ReAllocate(lpString, (curLength+1)*sizeof(TCHAR));

        if(oldLength < curLength)
            zero(&lpString[oldLength], ((curLength+1)-oldLength)*sizeof(TCHAR));
        else
            lpString[length] = 0;
    }
    else
    {
        if(lpString)
            Free(lpString);
        lpString = NULL;
    }

    return *this;
}


void String::RemoveRange(unsigned int from, unsigned int to)
{
    assert(from < to);
    assert(to <= curLength);
    if(from >= to || to > curLength) return;

    unsigned int delLength = to-from;
    if(delLength == curLength)
    {
        Clear();
        return;
    }

    unsigned int remainderLength = (curLength+1)-to;
    curLength -= delLength;
    mcpy(lpString+from, lpString+to, remainderLength*sizeof(TCHAR));
    lpString = (TSTR)ReAllocate(lpString, (curLength+1)*sizeof(TCHAR));
}


String& String::AppendChar(TCHAR chr)
{
    return (*this) += chr;
}

String& String::InsertChar(UINT pos, TCHAR chr)
{
    if(pos > curLength)
    {
        AppWarning(TEXT("String::InsertChar - bad index specified"));
        return *this;
    }
    
    ++curLength;
    lpString = (TSTR)ReAllocate(lpString, (curLength+1)*sizeof(TCHAR));
    if(curLength > 1)
        mcpyrev(lpString+pos+1, lpString+pos, (curLength-pos)*sizeof(TCHAR));

    lpString[pos] = chr;

    return *this;
}

String& String::RemoveChar(UINT pos)
{
    if(pos >= curLength)
    {
        AppWarning(TEXT("String::RemoveChar - bad index specified"));
        return *this;
    }
    
    
    if(curLength == 1)
        Clear();
    else
    {
        if(pos < curLength)
            mcpy(lpString+pos, lpString+pos+1, (curLength-pos)*sizeof(TCHAR));
        lpString = (TSTR)ReAllocate(lpString, (curLength)*sizeof(TCHAR));
        --curLength;
    }

    return *this;
}

String String::RepresentationToString(String &stringToken)
{
    if(stringToken.IsEmpty() || stringToken[0] != '"')
        return stringToken;

    String stringOut = stringToken.Mid(1, stringToken.Length()-1);

    stringOut.FindReplace(TEXT("\\\\"), TEXT("\\"));
    stringOut.FindReplace(TEXT("\\\""), TEXT("\""));
    stringOut.FindReplace(TEXT("\\t"),  TEXT("\t"));
    stringOut.FindReplace(TEXT("\\r"),  TEXT(""));
    stringOut.FindReplace(TEXT("\\n"),  TEXT("\r\n"));

    return stringOut;
}

String String::StringToRepresentation(String &string)
{
    if(string.IsEmpty())
        return String(TEXT("\"\""));

    String stringOut = string;
    stringOut.FindReplace(TEXT("\\"), TEXT("\\\\"));
    stringOut.FindReplace(TEXT("\""), TEXT("\\\""));
    stringOut.FindReplace(TEXT("\t"), TEXT("\\t"));
    stringOut.FindReplace(TEXT("\r"), TEXT("\\r"));
    stringOut.FindReplace(TEXT("\n"), TEXT("\\n"));
    stringOut.InsertString(0, TEXT("\""));
    stringOut << TEXT("\"");

    return stringOut;
}

String FormattedStringva(CTSTR lpFormat, va_list arglist)
{
    int iSize = vtscprintf(lpFormat, arglist);
    if(!iSize)
        return String();

    String newString;
    newString.SetLength(iSize);

    int retVal = vtsprintf_s(newString, iSize+1, lpFormat, arglist);
    if(retVal == -1)
        return newString.Clear();
    else
        return newString.SetLength(retVal);
}

String FormattedString(CTSTR lpFormat, ...)
{
    va_list args;
    va_start(args, lpFormat);

    int iSize = vtscprintf(lpFormat, args);
    if(!iSize)
        return String();

    String newString;
    newString.SetLength(iSize);

    int retVal = vtsprintf_s(newString, iSize+1, lpFormat, args);
    if(retVal == -1)
        return newString.Clear();
    else
        return newString.SetLength(retVal);
}

int STDCALL GetStringLine(const TCHAR *lpStart, const TCHAR *lpOffset)
{
    assert(lpStart && lpOffset);

    CTSTR lpTemp = lpStart;
    int iLine = 1;

    while(lpTemp = schr(lpTemp, '\n'))
    {
        if(lpTemp >= lpOffset)
            break;

        ++iLine;
        ++lpTemp;
    }

    return iLine;
}

String STDCALL GetStringSection(const TCHAR *lpStart, const TCHAR *lpOffset)
{
    assert(lpStart && lpOffset && lpOffset > lpStart);

    if(lpStart >= lpOffset)
        return String();

    String newStr;
    newStr.SetLength((UINT)(lpOffset-lpStart));
    scpy_n(newStr, lpStart, (UINT)(lpOffset-lpStart));

    return newStr;
}

WORD StringCRC16(CTSTR lpData)
{
    WORD crc = 0;
    int len = slen(lpData);

    for(int i=0; i<len; i++)
    {                
        TCHAR val = lpData[i];

        for(int j=0; j<(sizeof(TCHAR)*8); j++)
        {
            if((crc ^ val) & 1)
                crc = (crc >> 1) ^ 0xA003;
            else
                crc >>= 1;
            val >>= 1;
        }
    }

    return crc;
}

WORD StringCRC16I(CTSTR lpData)
{
    WORD crc = 0;
    int len = slen(lpData);

    for(int i=0; i<len; i++)
    {                
        TCHAR val = lpData[i];

        if((val >= 'A') && (val <= 'Z'))
            val += 0x20;

        for(int j=0; j<(sizeof(TCHAR)*8); j++)
        {
            if((crc ^ val) & 1)
                crc = (crc >> 1) ^ 0xA003;
            else
                crc >>= 1;
            val >>= 1;
        }
    }

    return crc;
}


Serializer& operator<<(Serializer &s, String &str)
{
    if(s.IsLoading())
    {
        //FIXME: ???
        UINT utf8Length;

        s << utf8Length;
        if(utf8Length)
        {
            LPSTR lpUTF8 = (LPSTR)Allocate(utf8Length+1);
            s.Serialize(lpUTF8, utf8Length+1);
            str = String(lpUTF8);
            Free(lpUTF8);
        }
        else
            str.Clear();
    }
    else
    {
        LPSTR lpUTF8 = str.CreateUTF8String();
        UINT utf8Length = lpUTF8 ? (UINT)strlen(lpUTF8) : 0;
        s << utf8Length;
        if(utf8Length)
        {
            s.Serialize(lpUTF8, utf8Length+1);
            Free(lpUTF8);
        }
    }

    return s;
}

BOOL STDCALL ValidFloatString(CTSTR lpStr)
{
    if(!lpStr || !*lpStr)
        return FALSE;

    BOOL bFoundDecimal = FALSE;
    BOOL bFoundExp = FALSE;

    if((*lpStr == '-') || (*lpStr == '+'))
        ++lpStr;

    BOOL bFoundNumber = FALSE;

    do
    {
        if(*lpStr == '.')
        {
            if(bFoundDecimal || bFoundExp || !bFoundNumber)
                return FALSE;

            bFoundDecimal = TRUE;

            continue;
        }
        else if(*lpStr == 'e')
        {
            if(bFoundExp || !bFoundNumber)
                return FALSE;

            bFoundExp = TRUE;
            bFoundNumber = FALSE;

            continue;
        }
        else if((*lpStr == '-') || (*lpStr == '+'))
        {
            if(!bFoundExp)
                return FALSE;
            if(bFoundNumber)
                return FALSE;

            continue;
        }
        else if(*lpStr == 'f')
        {
            if(*++lpStr != 0)
                return FALSE;

            break;
        }
        else if((*lpStr > '9') || (*lpStr < '0'))
            return FALSE;

        bFoundNumber = TRUE;

    }while(*++lpStr);

    return bFoundNumber;
}

BOOL STDCALL ValidIntString(CTSTR lpStr)
{
    if(!lpStr || !*lpStr)
        return FALSE;

    if((*lpStr == '-') || (*lpStr == '+'))
        ++lpStr;

    BOOL bFoundNumber = FALSE;

    do
    {
        if((*lpStr > '9') || (*lpStr < '0'))
            return FALSE;

        bFoundNumber = TRUE;
    }while(*++lpStr);

    return bFoundNumber;
}

BOOL DefinitelyFloatString(CTSTR lpStr)
{
    if(!lpStr || !*lpStr)
        return FALSE;

    BOOL bFoundDecimal = FALSE;
    BOOL bFoundExp = FALSE;
    BOOL bFoundEndThingy = FALSE;

    if((*lpStr == '-') || (*lpStr == '+'))
        ++lpStr;

    BOOL bFoundNumber = FALSE;

    do
    {
        if(*lpStr == '.')
        {
            if(bFoundDecimal || bFoundExp || !bFoundNumber)
                return FALSE;

            bFoundDecimal = TRUE;

            continue;
        }
        else if(*lpStr == 'e')
        {
            if(bFoundExp || !bFoundNumber)
                return FALSE;

            bFoundExp = TRUE;
            bFoundNumber = FALSE;

            continue;
        }
        else if((*lpStr == '-') || (*lpStr == '+'))
        {
            if(!bFoundExp)
                return FALSE;
            if(bFoundNumber)
                return FALSE;

            continue;
        }
        else if(*lpStr == 'f')
        {
            if(*++lpStr != 0)
                return FALSE;

            bFoundEndThingy = TRUE;
            break;
        }
        else if((*lpStr > '9') || (*lpStr < '0'))
            return FALSE;

        bFoundNumber = TRUE;

    }while(*++lpStr);

    return bFoundNumber && (bFoundDecimal || bFoundExp || bFoundEndThingy);
}


String UIntString(unsigned int ui, int radix)
{
    TCHAR lpString[14];
    uitots_s(ui, lpString, 13, radix);
    return String(lpString);
}

String IntString(int i, int radix)
{
    TCHAR lpString[14];
    itots_s(i, lpString, 13, radix);
    return String(lpString);
}

String UInt64String(UINT64 i, int radix)
{
    TCHAR lpString[50];
    ui64tots_s(i, lpString, 49, radix);
    return String(lpString);
}

String Int64String(INT64 i, int radix)
{
    TCHAR lpString[50];
    i64tots_s(i, lpString, 49, radix);
    return String(lpString);
}

String FloatString(double f)
{
    return FormattedString(TEXT("%g"), f);
}



UINT STDCALL slen(const TCHAR *strSrc)
{
    assert(strSrc);

#if !defined(UNICODE) || !defined(UTF32) || !defined(C64)
    if((reinterpret_cast<UPARAM>(strSrc) & MEMBOUNDRY) == 0)
    {
        const UPARAM *src = (UPARAM*)strSrc;
        while(true)
        {
#ifdef UNICODE
 #ifdef UTF32
            if(!(*src & 0xFFFFFFFFULL))                 return (unsigned int)((reinterpret_cast<const TCHAR*>(src))-strSrc);
            else if(!(*src & 0xFFFFFFFF00000000ULL))    return (unsigned int)((reinterpret_cast<const TCHAR*>(src)+1)-strSrc);
 #else //!UTF32
            if(!(*src & 0xFFFF))                        return (unsigned int)((reinterpret_cast<const TCHAR*>(src))-strSrc);
            else if(!(*src & 0xFFFF0000))               return (unsigned int)((reinterpret_cast<const TCHAR*>(src)+1)-strSrc);
  #ifdef C64
            else if(!(*src & 0xFFFF00000000ULL))        return (unsigned int)((reinterpret_cast<const TCHAR*>(src)+2)-strSrc);
            else if(!(*src & 0xFFFF000000000000ULL))    return (unsigned int)((reinterpret_cast<const TCHAR*>(src)+3)-strSrc);
  #endif //C64
 #endif //UTF32
#else //!UNICODE
            if(!(*src & 0xFF))                          return (unsigned int)((reinterpret_cast<const TCHAR*>(src))-strSrc);
            else if(!(*src & 0xFF00))                   return (unsigned int)((reinterpret_cast<const TCHAR*>(src)+1)-strSrc);
            else if(!(*src & 0xFF0000))                 return (unsigned int)((reinterpret_cast<const TCHAR*>(src)+2)-strSrc);
            else if(!(*src & 0xFF000000))               return (unsigned int)((reinterpret_cast<const TCHAR*>(src)+3)-strSrc);
 #ifdef C64
            else if(!(*src & 0xFF00000000ULL))          return (unsigned int)((reinterpret_cast<const TCHAR*>(src)+4)-strSrc);
            else if(!(*src & 0xFF0000000000ULL))        return (unsigned int)((reinterpret_cast<const TCHAR*>(src)+5)-strSrc);
            else if(!(*src & 0xFF000000000000ULL))      return (unsigned int)((reinterpret_cast<const TCHAR*>(src)+6)-strSrc);
            else if(!(*src & 0xFF00000000000000ULL))    return (unsigned int)((reinterpret_cast<const TCHAR*>(src)+7)-strSrc);
 #endif //C64
#endif //UNICODE
            ++src;
        }
    }
    else
#endif
    {
        unsigned int size=0;
        while(*strSrc++)
            ++size;
        return size;
    }
}

UINT STDCALL ssize(const TCHAR *strSrc)
{
    assert(strSrc);

    if(!strSrc)
        return 0;

#if !defined(UNICODE) || !defined(UTF32) || !defined(C64)
    if((reinterpret_cast<UPARAM>(strSrc) & MEMBOUNDRY) == 0)
    {
        const UPARAM *src = (UPARAM*)strSrc;
        while(true)
        {
#ifdef UNICODE
 #ifdef UTF32
            if(!(*src & 0xFFFFFFFFULL))                 return (unsigned int)(((reinterpret_cast<const TCHAR*>(src)+1)-strSrc)*sizeof(TCHAR));
            else if(!(*src & 0xFFFFFFFF00000000ULL))    return (unsigned int)(((reinterpret_cast<const TCHAR*>(src)+2)-strSrc)*sizeof(TCHAR));
 #else //!UTF32
            if(!(*src & 0xFFFF))                        return (unsigned int)(((reinterpret_cast<const TCHAR*>(src)+1)-strSrc)*sizeof(TCHAR));
            else if(!(*src & 0xFFFF0000))               return (unsigned int)(((reinterpret_cast<const TCHAR*>(src)+2)-strSrc)*sizeof(TCHAR));
  #ifdef C64
            else if(!(*src & 0xFFFF00000000ULL))        return (unsigned int)(((reinterpret_cast<const TCHAR*>(src)+3)-strSrc)*sizeof(TCHAR));
            else if(!(*src & 0xFFFF000000000000ULL))    return (unsigned int)(((reinterpret_cast<const TCHAR*>(src+1))-strSrc)*sizeof(TCHAR));
  #endif //C64
 #endif //UTF32
#else //!UNICODE
            if(!(*src & 0xFF))                          return (unsigned int)(((reinterpret_cast<const TCHAR*>(src)+1)-strSrc)*sizeof(TCHAR));
            else if(!(*src & 0xFF00))                   return (unsigned int)(((reinterpret_cast<const TCHAR*>(src)+2)-strSrc)*sizeof(TCHAR));
            else if(!(*src & 0xFF0000))                 return (unsigned int)(((reinterpret_cast<const TCHAR*>(src)+3)-strSrc)*sizeof(TCHAR));
            else if(!(*src & 0xFF000000))               return (unsigned int)(((reinterpret_cast<const TCHAR*>(src)+4)-strSrc)*sizeof(TCHAR));
 #ifdef C64
            else if(!(*src & 0xFF00000000ULL))          return (unsigned int)(((reinterpret_cast<const TCHAR*>(src)+5)-strSrc)*sizeof(TCHAR));
            else if(!(*src & 0xFF0000000000ULL))        return (unsigned int)(((reinterpret_cast<const TCHAR*>(src)+6)-strSrc)*sizeof(TCHAR));
            else if(!(*src & 0xFF000000000000ULL))      return (unsigned int)(((reinterpret_cast<const TCHAR*>(src)+7)-strSrc)*sizeof(TCHAR));
            else if(!(*src & 0xFF00000000000000ULL))    return (unsigned int)(((reinterpret_cast<const TCHAR*>(src+1))-strSrc)*sizeof(TCHAR));
 #endif //C64
#endif //UNICODE
            ++src;
        }
    }
    else
#endif
    {
        unsigned int size=1;
        while(*strSrc++)
            ++size;
        return size*sizeof(TCHAR);
    }
}

void STDCALL scpy_n(TCHAR *strDest, const TCHAR *strSrc, unsigned int n)
{
    assert(strDest);
    assert(strSrc);

    if(!n) return;

    while(*strSrc && n--)
        *(strDest++) = *(strSrc++);
    *strDest = 0;
}

void STDCALL scpy(TCHAR *strDest, const TCHAR *strSrc)
{
    assert(strDest);
    assert(strSrc);

#if !defined(UNICODE) || !defined(UTF32) || !defined(C64)
    if((reinterpret_cast<UPARAM>(strSrc) & MEMBOUNDRY) == 0)
    {
        const UPARAM *src = (UPARAM*)strSrc;
        UPARAM *dest = (UPARAM*)strDest;
        while(true)
        {
#ifdef UNICODE
 #ifdef UTF32
            if(!(*src & 0xFFFFFFFFULL))
            {
                *(reinterpret_cast<TCHAR*>(dest)) = 0;
                return;
            }
            else if(!(*src & 0xFFFFFFFF00000000ULL))
            {
                *(reinterpret_cast<TCHAR*>(dest))   = *(reinterpret_cast<const TCHAR*>(src));
                *(reinterpret_cast<TCHAR*>(dest)+1) = 0;
                return;
            }
 #else //!UTF32
            if(!(*src & 0xFFFF))
            {
                *(reinterpret_cast<TCHAR*>(dest)) = *(reinterpret_cast<const TCHAR*>(src));
                return;
            }
            else if(!(*src & 0xFFFF0000))
            {
                *(reinterpret_cast<DWORD*>(dest))   = *(reinterpret_cast<const DWORD*>(src));
                return;
            }
  #ifdef C64
            else if(!(*src & 0xFFFF00000000ULL))
            {
                *(reinterpret_cast<DWORD*>(dest))   = *(reinterpret_cast<const DWORD*>(src));
                *(reinterpret_cast<TCHAR*>(dest)+2) = 0;
                return;
            }
            else if(!(*src & 0xFFFF000000000000ULL))
            {
                *dest = *src;
                return;
            }
  #endif //C64
 #endif //UTF32
#else  //!UNICODE
            if(!(*src & 0xFF))
            {
                *(reinterpret_cast<TCHAR*>(dest))   = 0;
                return;
            }
            else if(!(*src & 0xFF00))
            {
                *(reinterpret_cast<WORD*>(dest))    = *(reinterpret_cast<const WORD*>(src));
                return;
            }
            else if(!(*src & 0xFF0000))
            {
                *(reinterpret_cast<WORD*>(dest))    = *(reinterpret_cast<const WORD*>(src));
                *(reinterpret_cast<TCHAR*>(dest)+2) = 0;
                return;
            }
            else if(!(*src & 0xFF000000))
            {
                *(reinterpret_cast<DWORD*>(dest))   = *(reinterpret_cast<const DWORD*>(src));
                return;
            }
 #ifdef C64
            else if(!(*src & 0xFF00000000ULL))
            {
                *(reinterpret_cast<DWORD*>(dest))   = *(reinterpret_cast<const DWORD*>(src));
                *(reinterpret_cast<TCHAR*>(dest)+4) = 0;
                return;
            }
            else if(!(*src & 0xFF0000000000ULL))
            {
                *(reinterpret_cast<DWORD*>(dest))   = *(reinterpret_cast<const DWORD*>(src));
                *(reinterpret_cast<WORD*>(dest)+2)  = *(reinterpret_cast<const WORD*>(src)+2);
                return;
            }
            else if(!(*src & 0xFF000000000000ULL))
            {
                *(reinterpret_cast<DWORD*>(dest))   = *(reinterpret_cast<const DWORD*>(src));
                *(reinterpret_cast<WORD*>(dest)+2)  = *(reinterpret_cast<const WORD*>(src)+2);
                *(reinterpret_cast<TCHAR*>(dest)+6) = 0;
                return;
            }
            else if(!(*src & 0xFF00000000000000ULL))
            {
                *dest = *src;
                return;
            }
 #endif //C64
#endif //UNICODE
            *(dest++) = *(src++);
        }
    }
    else
#endif
    {
        do {
            *strDest = *strSrc;
            ++strDest;
        }while(*strSrc++);
    }
}

void STDCALL scat(TCHAR *strDest, const TCHAR *strSrc)
{
    assert(strDest);
    assert(strSrc);

    strDest += slen(strDest);
    scpy(strDest, strSrc);
}

void STDCALL scat_n(TCHAR *strDest, const TCHAR *strSrc, unsigned int n)
{
    assert(strDest);
    assert(strSrc);

    if(!n) return;

    strDest += slen(strDest);

    mcpy(strDest, strSrc, n*sizeof(TCHAR));
    strDest[n] = 0;
}

void STDCALL slwr(TCHAR *strDest)
{
    assert(strDest);

    while(*strDest)
    {
        if((*strDest >= 'A') && (*strDest <= 'Z'))
            *strDest += 0x20;
        ++strDest;
    }
}

void STDCALL supr(TCHAR *strDest)
{
    assert(strDest);

    while(*strDest)
    {
        if((*strDest >= 'a') && (*strDest <= 'z'))
            *strDest -= 0x20;
        ++strDest;
    }
}

TCHAR* STDCALL sdup(const TCHAR *str)
{
    assert(str);
    if(!str) return NULL;

    int dataLen = ssize(str);

    TCHAR* lpNewString = (TCHAR*)Allocate(dataLen);
    mcpy(lpNewString, str, dataLen);

    return lpNewString;
}

int STDCALL scmp(const TCHAR *str1, const TCHAR *str2)
{
    if(!str1 || !str2)
        return -1;

    do {
        if(*str1 < *str2)
            return -1;
        else if(*str1 > *str2)
            return 1;
    }while(*str1++ && *str2++);

    return 0;
}

int STDCALL scmpi(const TCHAR *str1, const TCHAR *str2)
{
    TCHAR val1, val2;

    if(!str1 || !str2)
        return -1;

    do {
        val1 = *str1, val2 = *str2;

        if((val1 >= 'A') && (val1 <= 'Z'))
            val1 += 0x20;
        if((val2 >= 'A') && (val2 <= 'Z'))
            val2 += 0x20;

        if(val1 < val2)
            return -1;
        else if(val1 > val2)
            return 1;
    }while(*str1++ && *str2++);

    return 0;
}


int STDCALL scmp_n(const TCHAR *str1, const TCHAR *str2, unsigned int num)
{
    assert(str1);
    assert(str2);

    if(!num || !str1 || !str2)
        return -1;

    do {
        if(*str1 < *str2)
            return -1;
        else if(*str1 > *str2)
            return 1;
    }while(*str1++ && *str2++ && --num);

    return 0;
}

int STDCALL scmpi_n(const TCHAR *str1, const TCHAR *str2, unsigned int num)
{
    assert(str1);
    assert(str2);

    TCHAR val1, val2;

    if(!num || !str1 || !str2)
        return -1;

    do {
        val1 = *str1, val2 = *str2;

        if((val1 >= 'A') && (val1 <= 'Z'))
            val1 += 0x20;
        if((val2 >= 'A') && (val2 <= 'Z'))
            val2 += 0x20;

        if(val1 < val2)
            return -1;
        else if(val1 > val2)
            return 1;
    }while(*str1++ && *str2++ && --num);

    return 0;
}

TCHAR *STDCALL srchr(const TCHAR *strSrc, const TCHAR chr)
{
    assert(strSrc);

    const TCHAR *lpTemp = strSrc+slen(strSrc);

    while(lpTemp-- != strSrc)
    {
        if(*lpTemp == chr)
            return (TCHAR*)lpTemp;
    }

    return NULL;
}

TCHAR *STDCALL schr(const TCHAR *strSrc, const TCHAR chr)
{
    if(!strSrc) return NULL;

    while(*strSrc != chr)
    {
        if(*strSrc++ == 0)
            return NULL;
    }

    return (TCHAR*)strSrc;
}

TCHAR *STDCALL sstr(const TCHAR *strSrc, const TCHAR *strSearch)
{
    if(!strSearch || !strSrc) return NULL;

    int lenSearch = slen(strSearch);
    int len = slen(strSrc)-lenSearch;

    while(len-- >= 0)
    {
        if(mcmp(strSrc, strSearch, lenSearch*sizeof(TCHAR)))
            return (TCHAR*)strSrc;

        ++strSrc;
    }

    return NULL;
}

TCHAR *STDCALL srchri(const TCHAR *strSrc, TCHAR chr)
{
    assert(strSrc);

    const TCHAR *lpTemp = strSrc+slen(strSrc);

    if((chr >= 'A') && (chr <= 'Z'))
        chr += 0x20;

    while(lpTemp-- != strSrc)
    {
        TCHAR test = *lpTemp;
        if((test >= 'A') && (test <= 'Z'))
            test += 0x20;

        if(test == chr)
            return (TCHAR*)lpTemp;
    }

    return NULL;
}

TCHAR *STDCALL schri(const TCHAR *strSrc, TCHAR chr)
{
    if(!strSrc) return NULL;

    if((chr >= 'A') && (chr <= 'Z'))
        chr += 0x20;

    TCHAR test = *strSrc;
    while(test != chr)
    {
        if(*strSrc++ == 0)
            return NULL;

        test = *strSrc;
        if((test >= 'A') && (test <= 'Z'))
            test += 0x20;
    }

    return (TCHAR*)strSrc;
}

TCHAR *STDCALL sstri(const TCHAR *strSrc, const TCHAR *strSearch)
{
    if(!strSearch || !strSrc) return NULL;

    int lenSearch = slen(strSearch);
    int len = slen(strSrc)-lenSearch;

    while(len-- >= 0)
    {
        if(scmpi_n(strSrc, strSearch, lenSearch) == 0)
            return (TCHAR*)strSrc;

        ++strSrc;
    }

    return NULL;
}

TCHAR *STDCALL sfix(TCHAR *str)
{
    if(!str || !*str)
        return str;

    TCHAR *lpTemp = str;

    //get rid of leading spaces
    while(*lpTemp == ' ' || *lpTemp == '\t' || *lpTemp == L'Å@') ++lpTemp;
    UINT len = slen(lpTemp);
    if(lpTemp != str)
        mcpy(str, lpTemp, (len+1)*sizeof(TCHAR));

    if(len)
    {
        //get rid of ending spaces
        lpTemp = str+(len-1);
        while(*lpTemp == ' ' || *lpTemp == '\t' || *lpTemp == L'Å@') *(lpTemp--) = 0;
    }

    return str;
}




BOOL UsingUnicode()
{
#ifdef UNICODE
    return 1;
#else
    return 0;
#endif
}

int tstr_to_utf8(const TCHAR *strSrc, char *strDest, size_t destLen)
{
#ifdef UNICODE
    return (int)wchar_to_utf8(strSrc, slen(strSrc)+1, strDest, destLen, 0);
#else
    scpy(strDest, strSrc);
    return (int)destLen;
#endif
}

int tstr_to_wide(const TCHAR *strSrc, wchar_t *strDest, size_t destLen)
{
#ifdef UNICODE
    scpy(strDest, strSrc);
    return (int)destLen;
#else
    return (int)utf8_to_wchar(strSrc, slen(strSrc)+1, strDest, destLen, 0);
#endif
}

int utf8_to_tchar(const char *strSrc, TCHAR *strDest, size_t destLen)
{
#ifdef UNICODE
    return (int)utf8_to_wchar(strSrc, strlen(strSrc)+1, strDest, destLen, 0);
#else
    mcpy(strDest, strSrc, destLen+1);
    return (int)destLen;
#endif
}

int tstr_to_utf8_datalen(const TCHAR *strSrc)
{
#ifdef UNICODE
    return (int)wchar_to_utf8_len(strSrc, slen(strSrc)+1, 0);
#else
    return (int)(slen(strSrc)+1);
#endif
}

int tstr_to_wide_datalen(const TCHAR *strSrc)
{
#ifdef UNICODE
    return (int)slen(strSrc)+sizeof(wchar_t);
#else
    return (int)utf8_to_wchar_len(strSrc, slen(strSrc)+1, 0);
#endif
}

int utf8_to_tchar_datalen(const char *strSrc)
{
#ifdef UNICODE
    return ((int)utf8_to_wchar_len(strSrc, strlen(strSrc)+1, 0)+1)*sizeof(wchar_t);
#else
    return (int)(slen(strSrc)+1);
#endif
}


LPSTR tstr_createUTF8(const TCHAR *strSrc)
{
#ifdef UNICODE
    size_t stSize = tstr_to_utf8_datalen(strSrc);
    LPSTR lpString = (LPSTR)Allocate((UINT)stSize);
    tstr_to_utf8(strSrc, lpString, stSize);
    return lpString;
#else
    return sdup(strSrc);
#endif
}

WSTR tstr_createWide(const TCHAR *strSrc)
{
#ifdef UNICODE
    return sdup(strSrc);
#else
    size_t stSize = tstr_to_wide_datalen(strSrc);
    WSTR lpString = (WSTR)Allocate(stSize*sizeof(wchar_t));
    tstr_to_wide(strSrc, lpString, stSize);
    return lpString;
#endif
}

BASE_EXPORT TSTR utf8_createTstr(const char *strSrc)
{
#ifdef UNICODE
    int iSize = utf8_to_tchar_datalen(strSrc);
    TSTR lpString = (TSTR)Allocate(iSize);
    utf8_to_tchar(strSrc, lpString, iSize/sizeof(wchar_t));
    return lpString;
#else
    int iSize = slen(strSrc)+1;
    LPSTR lpString = (LPSTR)Allocate(iSize);
    mcpy(lpString, strSrc, iSize);
    return lpString;
#endif
}



int vtscprintf(const TCHAR *format, va_list args)
{
#ifdef UNICODE
    return _vscwprintf(format, args);
#else
    return _vscprintf(format, args);
#endif
}

int vtsprintf_s(TCHAR *pDest, size_t bufLen, const TCHAR *format, va_list args)
{
#ifdef UNICODE
    return vswprintf_s(pDest, bufLen, format, args);
#else
    return vsprintf_s(pDest, bufLen, format, args);
#endif
}

int tsprintf_s(TCHAR *pDest, size_t bufLen, const TCHAR *format, ...)
{
    va_list args;
    va_start(args, format);
    return vtsprintf_s(pDest, bufLen, format, args);
}

int itots_s(int val, TCHAR *buffer, size_t bufLen, int radix)
{
#ifdef UNICODE
    return _itow_s(val, buffer, bufLen, radix);
#else
    return _itoa_s(val, buffer, bufLen, radix);
#endif
}

int uitots_s(unsigned int val, TCHAR *buffer, size_t bufLen, int radix)
{
#ifdef UNICODE
    return _ultow_s((unsigned long)val, buffer, bufLen, radix);
#else
    return _ultoa_s((unsigned long)val, buffer, bufLen, radix);
#endif
}

int i64tots_s(INT64 val, TCHAR *buffer, size_t bufLen, int radix)
{
#ifdef UNICODE
    return _i64tow_s(val, buffer, bufLen, radix);
#else
    return _i64toa_s(val, buffer, bufLen, radix);
#endif
}

int ui64tots_s(UINT64 val, TCHAR *buffer, size_t bufLen, int radix)
{
#ifdef UNICODE
    return _ui64tow_s(val, buffer, bufLen, radix);
#else
    return _ui64toa_s((unsigned long)val, buffer, bufLen, radix);
#endif
}

int tstring_base_to_int(const TCHAR *nptr, TCHAR **endptr, int base)
{
#ifdef UNICODE
    return (int)wcstol(nptr, endptr, base);
#else
    return (int)strtol(nptr, endptr, base);
#endif
}

unsigned int tstring_base_to_uint(const TCHAR *nptr, TCHAR **endptr, int base)
{
#ifdef UNICODE
    return (unsigned int)wcstoul(nptr, endptr, base);
#else
    return (unsigned int)strtoul(nptr, endptr, base);
#endif
}

INT64 tstring_base_to_int64(const TCHAR *nptr, TCHAR **endptr, int base)
{
#ifdef UNICODE
    return (INT64)_wcstoi64(nptr, endptr, base);
#else
    return (INT64)_strtoi64(nptr, endptr, base);
#endif
}

UINT64 tstring_base_to_uint64(const TCHAR *nptr, TCHAR **endptr, int base)
{
#ifdef UNICODE
    return (UINT64)_wcstoui64(nptr, endptr, base);
#else
    return (UINT64)_strtoui64(nptr, endptr, base);
#endif
}


double tstof(TCHAR *lpFloat)
{
#ifdef UNICODE
    return _wtof(lpFloat);
#else
    return atof(lpFloat);
#endif
}

int tstoi(TCHAR *lpInt)
{
#ifdef UNICODE
    return _wtoi(lpInt);
#else
    return atoi(lpInt);
#endif
}

unsigned int tstoui(TCHAR *lpInt)
{
    return (unsigned int)tstring_base_to_uint(lpInt, NULL, 10);
}

INT64 tstoi64(TCHAR *lpInt)
{
#ifdef UNICODE
    return _wtoi64(lpInt);
#else
    return _atoi64(lpInt);
#endif
}

UINT64 tstoui64(TCHAR *lpInt)
{
    return (UINT64)tstring_base_to_uint64(lpInt, NULL, 10);
}
