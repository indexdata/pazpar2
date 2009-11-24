/* This file is part of Pazpar2.
   Copyright (C) 2006-2009 Index Data

Pazpar2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

Pazpar2 is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/

/** \file normalize7bit.c
    \brief char and string normalization for 7bit ascii only
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "normalize7bit.h"


/** \brief removes leading whitespace.. Removes suffix cahrs in rm_chars */
char * normalize7bit_generic(char * str, const char * rm_chars)
{
    char *p, *pe;
    for (p = str; *p && isspace(*(unsigned char *)p); p++)
        ;
    for (pe = p + strlen(p) - 1;
         pe > p && strchr(rm_chars, *pe); pe--)
        *pe = '\0';
    return p;
}

char *normalize7bit_mergekey(char *buf)
{
    char *p = buf, *pout = buf;
    while (*p)
    {
        while (*p && !isalnum(*(unsigned char *)p))
            p++;
        while (isalnum(*(unsigned char *)p))
            *(pout++) = tolower(*(unsigned char *)(p++));
        if (*p)
            *(pout++) = ' ';
        while (*p && !isalnum(*(unsigned char *)p))
            p++;
    }
    if (buf != pout)
        do {
            *(pout--) = '\0';
        }
        while (pout > buf && *pout == ' ');
    
    return buf;
}

// Extract what appears to be years from buf, storing highest and
// lowest values.
// longdate==1, look for YYYYMMDD, longdate=0 look only for YYYY
int extract7bit_dates(const char *buf, int *first, int *last, int longdate)
{
    *first = -1;
    *last = -1;
    while (*buf)
    {
        const char *e;
        int len;

        while (*buf && !isdigit(*(unsigned char *)buf))
            buf++;
        len = 0;
        for (e = buf; *e && isdigit(*(unsigned char *)e); e++)
            len++;
        if ((len == 4 && !longdate) || (longdate && len >= 4 && len <= 8))
        {
            int value = atoi(buf);
            if (longdate && len == 4)
                value *= 10000; //  should really suffix 0101?
            if (*first < 0 || value < *first)
                *first = value;
            if (*last < 0 || value > *last)
                *last = value;
        }
        buf = e;
    }
    return *first;
}



/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

