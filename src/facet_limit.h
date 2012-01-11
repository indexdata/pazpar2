/* This file is part of Pazpar2.
   Copyright (C) 2006-2012 Index Data

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

#ifndef FACET_LIMIT_H
#define FACET_LIMIT_H

#include <yaz/yaz-util.h>

typedef struct facet_limits *facet_limits_t;

facet_limits_t facet_limits_create(const char *param);

int facet_limits_num(facet_limits_t fl);

const char *facet_limits_get(facet_limits_t fl, int idx, const char **value);

void facet_limits_destroy(facet_limits_t fl);

facet_limits_t facet_limits_dup(facet_limits_t fl);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

