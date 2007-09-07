/* $Id: normalize7bit.c,v 1.4 2007-09-07 10:46:33 adam Exp $
   Copyright (c) 2006-2007, Index Data.

   This file is part of Pazpar2.

   Pazpar2 is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2, or (at your option) any later
   version.

   Pazpar2 is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with Pazpar2; see the file LICENSE.  If not, write to the
   Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.
*/

/** \file normalize7bit.c
    \brief char and string normalization for 7bit ascii only
*/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if HAVE_CONFIG_H
#include "cconfig.h"
#endif

#include "normalize7bit.h"


/** \brief removes leading whitespace.. Removes suffix cahrs in rm_chars */
char * normalize7bit_generic(char * str, const char * rm_chars)
{
    char *p, *pe;
    for (p = str; *p && isspace(*p); p++)
        ;
    for (pe = p + strlen(p) - 1;
         pe > p && strchr(rm_chars, *pe); pe--)
        *pe = '\0';
    return p;
}



char * normalize7bit_mergekey(char *buf, int skiparticle)
{
    char *p = buf, *pout = buf;

    if (skiparticle)
    {
        char firstword[64];
        char articles[] = "the den der die des an a "; // must end in space

        while (*p && !isalnum(*p))
            p++;
        pout = firstword;
        while (*p && *p != ' ' && pout - firstword < 62)
            *(pout++) = tolower(*(p++));
        *(pout++) = ' ';
        *(pout++) = '\0';
        if (!strstr(articles, firstword))
            p = buf;
        pout = buf;
    }

    while (*p)
    {
        while (*p && !isalnum(*p))
            p++;
        while (isalnum(*p))
            *(pout++) = tolower(*(p++));
        if (*p)
            *(pout++) = ' ';
        while (*p && !isalnum(*p))
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
int extract7bit_years(const char *buf, int *first, int *last)
{
    *first = -1;
    *last = -1;
    while (*buf)
    {
        const char *e;
        int len;

        while (*buf && !isdigit(*buf))
            buf++;
        len = 0;
        for (e = buf; *e && isdigit(*e); e++)
            len++;
        if (len == 4)
        {
            int value = atoi(buf);
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
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
