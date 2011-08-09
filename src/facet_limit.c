/* This file is part of Pazpar2.
   Copyright (C) 2006-2011 Index Data

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

/** \file facet_limit.c
    \brief Parse facet limit
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <assert.h>

#include <yaz/yaz-version.h>
#include <yaz/nmem.h>

#include "facet_limit.h"

struct facet_limits {
    NMEM nmem;
    int num;
    char **darray;
};

facet_limits_t facet_limits_create(const char *param)
{
    int i;
    NMEM nmem = nmem_create();
    facet_limits_t fl = nmem_malloc(nmem, sizeof(*fl));
    fl->nmem = nmem;
    fl->num = 0;
    fl->darray = 0;
    if (param)
        nmem_strsplit_escape(fl->nmem, ",", param, &fl->darray,
                             &fl->num, 1, '\\');
    /* replace = with \0 .. for each item */
    for (i = 0; i < fl->num; i++)
    {
        char *cp = strchr(fl->darray[i], '=');
        if (!cp)
        {
            facet_limits_destroy(fl);
            return 0;
        }
        *cp = '\0';
    }
    return fl;
}

int facet_limits_num(facet_limits_t fl)
{
    return fl->num;
}

const char *facet_limits_get(facet_limits_t fl, int idx, const char **value)
{
    if (idx >= fl->num || idx < 0)
        return 0;
    *value = fl->darray[idx] + strlen(fl->darray[idx]) + 1;
    return fl->darray[idx];
}

void facet_limits_destroy(facet_limits_t fl)
{
    if (fl)
        nmem_destroy(fl->nmem);
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

