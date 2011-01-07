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

/** \file incref.c
    \brief MUTEX protect ref counts
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "incref.h"

void pazpar2_incref(int *ref, YAZ_MUTEX mutex)
{
    yaz_mutex_enter(mutex);
    (*ref)++;
    yaz_mutex_leave(mutex);
}


int pazpar2_decref(int *ref, YAZ_MUTEX mutex)
{
    int value ;
    yaz_mutex_enter(mutex);
    value = --(*ref);
    yaz_mutex_leave(mutex);
    return value;
}
/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

