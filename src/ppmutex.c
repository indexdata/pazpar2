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

/** \file
    \brief control MUTEX debugging
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <yaz/log.h>
#include "ppmutex.h"

static int ppmutex_level = 0;

void pazpar2_mutex_init(void)
{
    ppmutex_level = yaz_log_module_level("mutex");
}

void pazpar2_mutex_create_flag(YAZ_MUTEX *p, const char *name, int flags)
{
    assert(p);
    yaz_mutex_create_attr(p, flags);
    yaz_mutex_set_name(*p, ppmutex_level, name);
}

void pazpar2_mutex_create(YAZ_MUTEX *p, const char *name) {
    pazpar2_mutex_create_flag(p, name, 0);
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

