#define _WIN32_WINDOWS 0x0410
#define _WIN32_WINNT   0x0403
#include <windows.h>
#include "XT.h"

size_t utf8_to_wchar(const char *in, size_t insize, wchar_t *out, size_t outsize, int flags)
{
	return (size_t)MultiByteToWideChar(CP_UTF8, 0, in, (int)insize, out, (int)outsize);
}

size_t wchar_to_utf8(const wchar_t *in, size_t insize, char *out, size_t outsize, int flags)
{
	return (size_t)WideCharToMultiByte(CP_UTF8, 0, in, (int)insize, out, (int)outsize, NULL, NULL);
}

size_t utf8_to_wchar_len(const char *in, size_t insize, int flags)
{
	return (size_t)MultiByteToWideChar(CP_UTF8, 0, in, (int)insize, NULL, 0);
}

size_t wchar_to_utf8_len(const wchar_t *in, size_t insize, int flags)
{
	return (size_t)WideCharToMultiByte(CP_UTF8, 0, in, (int)insize, NULL, 0, NULL, NULL);
}
