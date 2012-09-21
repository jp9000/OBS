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





////////////////
//globals (oh deary me.  if there were a convention convention, I most certainly wouldn't be invited.)
SafeList<DWORD>         TickList;

Alloc                  *MainAllocator   = NULL;
BOOL                    bBaseLoaded     = FALSE;
BOOL                    bDebugBreak     = TRUE;
TCHAR                   lpLogFileName[260] = TEXT("XT.log");
XFile                   LogFile;
StringList              TraceFuncList;

BOOL bLogStarted = FALSE;

void STDCALL CriticalExit();
void STDCALL OpenLogFile();
void STDCALL CloseLogFile();

void STDCALL OSInit();
void STDCALL OSExit();

BOOL STDCALL InitXT(CTSTR logFile, CTSTR allocatorName)
{
    if(!bBaseLoaded)
    {
        if(logFile)
            scpy(lpLogFileName, logFile);

        OSInit();

        ResetXTAllocator(allocatorName);
        bBaseLoaded = 1;
    }

    return TRUE;
}


void STDCALL InitXTLog(CTSTR logFile)
{
    if(logFile)
        scpy(lpLogFileName, logFile);
}

void STDCALL ResetXTAllocator(CTSTR lpAllocator)
{
    delete locale;
    delete MainAllocator;

    if(scmpi(lpAllocator, TEXT("DebugAlloc")) == 0)
        MainAllocator = new DebugAlloc;
    else if (scmpi(lpAllocator, TEXT("DefaultAlloc")) == 0)
        MainAllocator = new DefaultAlloc;
    else if (scmpi(lpAllocator, TEXT("SeriousMemoryDebuggingAlloc")) == 0)
        MainAllocator = new SeriousMemoryDebuggingAlloc;
    else
        MainAllocator = new FastAlloc;

    locale = new LocaleStringLookup;
}

void STDCALL TerminateXT()
{
    if(bBaseLoaded)
    {
        DumpProfileData();
        FreeProfileData();

        delete locale;
        locale = NULL;

        if(MainAllocator)
        {
            Alloc *curAlloc = MainAllocator;
            MainAllocator = new DefaultAlloc;

            delete curAlloc;

            delete MainAllocator;
            MainAllocator = NULL;
        }

        bBaseLoaded = 0;

        if(LogFile.IsOpen())
            LogFile.Close();

        OSExit();
    }
}

void STDCALL CriticalExit()
{
    if(bBaseLoaded)
    {
        if(MainAllocator)
        {
            MainAllocator->ErrorTermination();
            free(MainAllocator);
        }

        bBaseLoaded = 0;

        if(LogFile.IsOpen())
            LogFile.Close();

        OSCriticalExit();
    }
}

#define MAX_STACK_TRACE 1000

void STDCALL TraceCrash(const TCHAR *trackName)
{
    TraceFuncList.Insert(0, trackName);

    if(TraceFuncList.Num() > MAX_STACK_TRACE)
        TraceFuncList.SetSize(MAX_STACK_TRACE);
}

void STDCALL TraceCrashEnd()
{
    String strStackTrace = TEXT("\r\nException Fault - Stack Trace:");

    for(unsigned int i=0; i<TraceFuncList.Num(); i++)
    {
        if(i) strStackTrace << TEXT(" -> ");
        if(!(i%10)) strStackTrace << TEXT("\r\n    ");
        strStackTrace << TraceFuncList[i];
    }

    if(TraceFuncList.Num() == MAX_STACK_TRACE)
        strStackTrace << TEXT(" -> ...");

    String strOut = FormattedString(TEXT("%s\r\n"), strStackTrace.Array());

    OpenLogFile();
    LogFile.WriteAsUTF8(strOut, strOut.Length());
    LogFile.WriteAsUTF8(TEXT("\r\n"));
    CloseLogFile();

    OSMessageBox(TEXT("Error: Exception fault - More info in the log file.\r\n\r\nMake sure you're using the latest verison, otherwise send your log to obs.jim@gmail.com"));

    TraceFuncList.Clear();
    CriticalExit();
}


void STDCALL OpenLogFile()
{
    if(LogFile.IsOpen())
        return;

    if(!bLogStarted)
    {
        LogFile.Open(lpLogFileName, XFILE_WRITE, XFILE_CREATEALWAYS);
        LogFile.Write("\xEF\xBB\xBF", 3);
        bLogStarted = TRUE;
    }
    else
    {
        //LogFile.Open(lpLogFileName, XFILE_WRITE, XFILE_OPENEXISTING);
        //LogFile.SetPos(0, XFILE_END);
    }
}

void STDCALL CloseLogFile()
{
    //LogFile.Close();
}



void __cdecl Logva(const TCHAR *format, va_list argptr)
{
    if(!format) return;

    String strOut = FormattedStringva(format, argptr);

    OpenLogFile();
    LogFile.WriteAsUTF8(strOut, strOut.Length());
    LogFile.WriteAsUTF8(TEXT("\r\n"));
    CloseLogFile();
}

void __cdecl Log(const TCHAR *format, ...)
{
    if(!format) return;

    va_list arglist;

    va_start(arglist, format);

    Logva(format, arglist);
}


void __cdecl AppWarning(const TCHAR *format, ...)
{
    if(!format) return;

    va_list arglist;

    va_start(arglist, format);

    if(bLogStarted)
    {
        OpenLogFile();
        LogFile.WriteStr(TEXT("Warning -- "));

        String strOut = FormattedStringva(format, arglist);
        LogFile.WriteAsUTF8(strOut, strOut.Length());
        LogFile.WriteAsUTF8(TEXT("\r\n"));
        CloseLogFile();
    }

    OSDebugOut(TEXT("Warning -- "));
    OSDebugOutva(format, arglist);
    OSDebugOut(TEXT("\r\n"));

    //------------------------------------------------------
    // NOTE:
    // If you're seeting this, you can safely continue running, but I recommend fixing whatever's causing this warning.
    //
    // The debug output window contains the warning that has occured.
    //------------------------------------------------------

#if defined(_DEBUG) && defined(_WIN32)
    if(bDebugBreak && OSDebuggerPresent())
    {
        ProgramBreak();
    }
#endif
}


//note to self:  do try not to rely on this too much.
void __cdecl CrashError(const TCHAR *format, ...)
{
    if(!format) return;

    va_list arglist;

    va_start(arglist, format);

    String strOut = FormattedStringva(format, arglist);

    OpenLogFile();
    LogFile.WriteStr(TEXT("\r\nError: "));
    LogFile.WriteAsUTF8(strOut);
    LogFile.WriteStr(TEXT("\r\n"));
    CloseLogFile();

    OSMessageBoxva(format, arglist);

#if defined(_DEBUG) && defined(_WIN32)
    if(bDebugBreak && OSDebuggerPresent())
        ProgramBreak();
#endif

    CriticalExit();
}

