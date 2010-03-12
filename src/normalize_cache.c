/* This file is part of Pazpar2.
   Copyright (C) 2006-2010 Index Data

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

#include <string.h>

#include <yaz/yaz-util.h>
#include <yaz/mutex.h>
#include <yaz/nmem.h>

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "normalize_cache.h"

#include "pazpar2_config.h"

struct cached_item {
    char *spec;
    struct cached_item *next;
    normalize_record_t nt;
};

struct normalize_cache_s {
    struct cached_item *items;
    NMEM nmem;
    YAZ_MUTEX mutex;
};

normalize_cache_t normalize_cache_create(void)
{
    NMEM nmem = nmem_create();
    normalize_cache_t nc = nmem_malloc(nmem, sizeof(*nc));
    nc->nmem = nmem;
    nc->items = 0;
    nc->mutex = 0;
    yaz_mutex_create(&nc->mutex);
    yaz_mutex_set_name(nc->mutex, "normalize_cache");
    return nc;
}

normalize_record_t normalize_cache_get(normalize_cache_t nc,
                                       struct conf_service *service,
                                       const char *spec)
{
    normalize_record_t nt;
    struct cached_item *ci;

    yaz_mutex_enter(nc->mutex);
    for (ci = nc->items; ci; ci = ci->next)
        if (!strcmp(spec, ci->spec))
            break;
    if (ci)
        nt = ci->nt;
    else
    {
        nt = normalize_record_create(service, spec);
        if (nt)
        {
            ci = nmem_malloc(nc->nmem, sizeof(*ci));
            ci->next = nc->items;
            nc->items = ci;
            ci->nt = nt;
            ci->spec = nmem_strdup(nc->nmem, spec);
        }
    }
    yaz_mutex_leave(nc->mutex);
    return nt;
}

void normalize_cache_destroy(normalize_cache_t nc)
{
    if (nc)
    {
        struct cached_item *ci = nc->items;
        for (; ci; ci = ci->next)
            normalize_record_destroy(ci->nt);
        yaz_mutex_destroy(&nc->mutex);
        nmem_destroy(nc->nmem);
    }
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

