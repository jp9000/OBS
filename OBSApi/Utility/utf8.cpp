/*
 * Copyright (c) 2007 Alexey Vatchenko <av@bsdua.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <wchar.h>
#include "XT.h"

#define _NXT    0x80
#define _SEQ2    0xc0
#define _SEQ3    0xe0
#define _SEQ4    0xf0
#define _SEQ5    0xf8
#define _SEQ6    0xfc

#define _BOM    0xfeff




static int __wchar_forbitten(wchar_t sym);
static int __utf8_forbitten(unsigned char octet);

static int
__wchar_forbitten(wchar_t sym)
{

    // Surrogate pairs
    if (sym >= 0xd800 && sym <= 0xdfff)
        return (-1);

    return (0);
}

static int
__utf8_forbitten(unsigned char octet)
{

    switch (octet) {
    case 0xc0:
    case 0xc1:
    case 0xf5:
    case 0xff:
        return (-1);
    }

    return (0);
}

size_t
utf8_to_wchar(const char *in, size_t insize, wchar_t *out, size_t outsize,
    int flags)
{
    unsigned char *p, *lim;
    wchar_t *wlim, high;
    size_t n, total, i, n_bits;

    if (in == NULL || insize == 0 || (outsize == 0 && out != NULL))
        return (0);

    total = 0;
    p = (unsigned char *)in;
    lim = p + insize;
    wlim = out + outsize;

    for (; p < lim; p += n) {
        if (__utf8_forbitten(*p) != 0 &&
            (flags & UTF8_IGNORE_ERROR) == 0)
            return (0);

        //
        // Get number of bytes for one wide character.
        //
        n = 1;    // default: 1 byte. Used when skipping bytes.
        if ((*p & 0x80) == 0)
            high = (wchar_t)*p;
        else if ((*p & 0xe0) == _SEQ2) {
            n = 2;
            high = (wchar_t)(*p & 0x1f);
        } else if ((*p & 0xf0) == _SEQ3) {
            n = 3;
            high = (wchar_t)(*p & 0x0f);
        } else if ((*p & 0xf8) == _SEQ4) {
            n = 4;
            high = (wchar_t)(*p & 0x07);
        } else if ((*p & 0xfc) == _SEQ5) {
            n = 5;
            high = (wchar_t)(*p & 0x03);
        } else if ((*p & 0xfe) == _SEQ6) {
            n = 6;
            high = (wchar_t)(*p & 0x01);
        } else {
            if ((flags & UTF8_IGNORE_ERROR) == 0)
                return (0);
            continue;
        }

        // does the sequence header tell us truth about length?
        if (lim - p <= (unsigned char)n - 1) {
            if ((flags & UTF8_IGNORE_ERROR) == 0)
                return (0);
            n = 1;
            continue;    // skip
        }

        //
        // Validate sequence.
        // All symbols must have higher bits set to 10xxxxxx
        //
        if (n > 1) {
            for (i = 1; i < n; i++) {
                if ((p[i] & 0xc0) != _NXT)
                    break;
            }
            if (i != n) {
                if ((flags & UTF8_IGNORE_ERROR) == 0)
                    return (0);
                n = 1;
                continue;    // skip
            }
        }

        total++;

        if (out == NULL)
            continue;

        if (out >= wlim)
            return (0);        // no space left

        *out = 0;
        n_bits = 0;
        for (i = 1; i < n; i++) {
            *out |= (wchar_t)(p[n - i] & 0x3f) << n_bits;
            n_bits += 6;        // 6 low bits in every byte
        }
        *out |= high << n_bits;

        if (__wchar_forbitten(*out) != 0) {
            if ((flags & UTF8_IGNORE_ERROR) == 0)
                return (0);    // forbitten character
            else {
                total--;
                out--;
            }
        } else if (*out == _BOM && (flags & UTF8_DONT_SKIP_BOM) == 0) {
            total--;
            out--;
        }

        out++;
    }

    return (total);
}

size_t
wchar_to_utf8(const wchar_t *in, size_t insize, char *out, size_t outsize,
    int flags)
{
    wchar_t *w, *wlim;
    unsigned char *p, *lim, *oc;
    unsigned int ch;
    size_t total, n;

    if (in == NULL || insize == 0 || (outsize == 0 && out != NULL))
        return (0);

    w = (wchar_t *)in;
    wlim = w + insize;
    p = (unsigned char *)out;
    lim = p + outsize;
    total = 0;
    for (; w < wlim; w++) {
        if (__wchar_forbitten(*w) != 0) {
            if ((flags & UTF8_IGNORE_ERROR) == 0)
                return (0);
            else
                continue;
        }

        if (*w == _BOM && (flags & UTF8_DONT_SKIP_BOM) == 0)
            continue;

        if (*w < 0) {
            if ((flags & UTF8_IGNORE_ERROR) == 0)
                return (0);
            continue;
        } else if (*w <= 0x0000007f)
            n = 1;
        else if (*w <= 0x000007ff)
            n = 2;
        else if (*w <= 0x0000ffff)
            n = 3;
        else if (*w <= 0x001fffff)
            n = 4;
        else if (*w <= 0x03ffffff)
            n = 5;
        else // if (*w <= 0x7fffffff)
            n = 6;

        total += n;

        if (out == NULL)
            continue;

        if (lim - p <= (unsigned char)n - 1)
            return (0);        // no space left

        // make it work under different endians //jim - or how about, *no*.
        ch = (*w);
        oc = (unsigned char *)&ch;
        switch (n) {
        case 1:
            *p = oc[0];
            break;

        case 2:
            p[1] = _NXT | oc[0] & 0x3f;
            p[0] = _SEQ2 | (oc[0] >> 6) | ((oc[1] & 0x07) << 2);
            break;

        case 3:
            p[2] = _NXT | oc[0] & 0x3f;
            p[1] = _NXT | (oc[0] >> 6) | ((oc[1] & 0x0f) << 2);
            p[0] = _SEQ3 | ((oc[1] & 0xf0) >> 4);
            break;

        case 4:
            p[3] = _NXT | oc[0] & 0x3f;
            p[2] = _NXT | (oc[0] >> 6) | ((oc[1] & 0x0f) << 2);
            p[1] = _NXT | ((oc[1] & 0xf0) >> 4) |
                ((oc[2] & 0x03) << 4);
            p[0] = _SEQ4 | ((oc[2] & 0x1f) >> 2);
            break;

        case 5:
            p[4] = _NXT | oc[0] & 0x3f;
            p[3] = _NXT | (oc[0] >> 6) | ((oc[1] & 0x0f) << 2);
            p[2] = _NXT | ((oc[1] & 0xf0) >> 4) |
                ((oc[2] & 0x03) << 4);
            p[1] = _NXT | (oc[2] >> 2);
            p[0] = _SEQ5 | oc[3] & 0x03;
            break;

        case 6:
            p[5] = _NXT | oc[0] & 0x3f;
            p[4] = _NXT | (oc[0] >> 6) | ((oc[1] & 0x0f) << 2);
            p[3] = _NXT | (oc[1] >> 4) | ((oc[2] & 0x03) << 4);
            p[2] = _NXT | (oc[2] >> 2);
            p[1] = _NXT | oc[3] & 0x3f;
            p[0] = _SEQ6 | ((oc[3] & 0x40) >> 6);
            break;
        }

        //
        // NOTE: do not check here for forbitten UTF-8 characters.
        // They cannot appear here because we do proper convertion.
        //

        p += n;
    }

    return (total);
}

//jim - the dude really should have written functions like this rather than leaving the people to guess at how long their new string will need to be.
//alexey...   ALEXEY BABY!  I love you anyway.  YOU ARE LIKE BROTHER TO ME

size_t utf8_to_wchar_len(const char *in, size_t insize, int flags)
{
    unsigned char *p, *lim;
    wchar_t high, val;
    size_t n, total, i, n_bits;

    if (in == NULL || insize == 0)
        return (0);

    total = 0;
    p = (unsigned char *)in;
    lim = p + insize;

    for (; p < lim; p += n) {
        if (__utf8_forbitten(*p) != 0 &&
            (flags & UTF8_IGNORE_ERROR) == 0)
            return (0);

        //
        // Get number of bytes for one wide character.
        //
        n = 1;    // default: 1 byte. Used when skipping bytes.
        if ((*p & 0x80) == 0)
            high = (wchar_t)*p;
        else if ((*p & 0xe0) == _SEQ2) {
            n = 2;
            high = (wchar_t)(*p & 0x1f);
        } else if ((*p & 0xf0) == _SEQ3) {
            n = 3;
            high = (wchar_t)(*p & 0x0f);
        } else if ((*p & 0xf8) == _SEQ4) {
            n = 4;
            high = (wchar_t)(*p & 0x07);
        } else if ((*p & 0xfc) == _SEQ5) {
            n = 5;
            high = (wchar_t)(*p & 0x03);
        } else if ((*p & 0xfe) == _SEQ6) {
            n = 6;
            high = (wchar_t)(*p & 0x01);
        } else {
            if ((flags & UTF8_IGNORE_ERROR) == 0)
                return (0);
            continue;
        }

        // does the sequence header tell us truth about length?
        if (lim - p <= (unsigned char)n - 1) {
            if ((flags & UTF8_IGNORE_ERROR) == 0)
                return (0);
            n = 1;
            continue;    // skip
        }

        //
        // Validate sequence.
        // All symbols must have higher bits set to 10xxxxxx
        //
        if (n > 1) {
            for (i = 1; i < n; i++) {
                if ((p[i] & 0xc0) != _NXT)
                    break;
            }
            if (i != n) {
                if ((flags & UTF8_IGNORE_ERROR) == 0)
                    return (0);
                n = 1;
                continue;    // skip
            }
        }

        total++;

        val = 0;
        n_bits = 0;
        for (i = 1; i < n; i++) {
            val |= (wchar_t)(p[n - i] & 0x3f) << n_bits;
            n_bits += 6;        // 6 low bits in every byte
        }
        val |= high << n_bits;

        if (__wchar_forbitten(val) != 0) {
            if ((flags & UTF8_IGNORE_ERROR) == 0)
                return (0);    // forbitten character
            else {
                total--;
            }
        } else if (val == _BOM && (flags & UTF8_DONT_SKIP_BOM) == 0) {
            total--;
        }
    }

    return (total);
}

size_t wchar_to_utf8_len(const wchar_t *in, size_t insize, int flags)
{
    wchar_t *w, *wlim;
    size_t total, n;

    if (in == NULL || insize == 0)
        return (0);

    w = (wchar_t *)in;
    wlim = w + insize;
    total = 0;
    for (; w < wlim; w++) {
        if (__wchar_forbitten(*w) != 0) {
            if ((flags & UTF8_IGNORE_ERROR) == 0)
                return (0);
            else
                continue;
        }

        if (*w == _BOM && (flags & UTF8_DONT_SKIP_BOM) == 0)
            continue;

        if (*w < 0) {
            if ((flags & UTF8_IGNORE_ERROR) == 0)
                return (0);
            continue;
        } else if (*w <= 0x0000007f)
            n = 1;
        else if (*w <= 0x000007ff)
            n = 2;
        else if (*w <= 0x0000ffff)
            n = 3;
        else if (*w <= 0x001fffff)
            n = 4;
        else if (*w <= 0x03ffffff)
            n = 5;
        else // if (*w <= 0x7fffffff)
            n = 6;

        total += n;
    }

    return (total);
}
