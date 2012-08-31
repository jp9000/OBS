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


//the following hacks are used to get x264 static link working.
#include <sys/stat.h>
#include <sys/timeb.h>
#include <process.h>

#undef _ftime
#undef _fstat

extern "C"
{
    int _fstat(int fd, struct stat *sb)
    {
        return fstat(fd, sb);
    }

    void _ftime(struct timeb *timeptr)
    {
        ftime(timeptr);
    }

    void blablablaHack1() //forces linkage to the pthreads thing
    {
        _endthreadex(0);
    }

    int __imp__fstat64(int fd, struct _stat64 *sb)
    {
        return _fstat64(fd, sb);
    }

    int snprintf(
        char *buffer,
        size_t count,
        const char *format, ... 
        )
    {
        va_list arglist;
        va_start(arglist, format);

        return _vsnprintf(buffer, count, format, arglist);
    }

    int fseeko64(FILE *stream, __int64 offset, int origin)
    {
        return _fseeki64(stream, offset, origin);
    }

    double __strtod(const char *nptr, char **endptr)
    {
        return strtod(nptr, endptr);
    }

    size_t ftello(FILE *stream)
    {
#ifdef _WIN64
        return _ftelli64(stream);
#else
        return ftell(stream);
#endif
    }

    double round(double val)
    {
        if(!_isnan(val) || !_finite(val))
            return val;

        if(val > 0.0f)
            return floor(val+0.5);
        else
            return floor(val-0.5);
    }

    float log2f(float val)
    {
        return logf(val) / logf(2.0f);
    }

}
