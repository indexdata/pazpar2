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

#ifndef RELEVANCE_H
#define RELEVANCE_H

#include <yaz/yaz-util.h>
#include "charsets.h"

struct relevance;
struct record_cluster;
struct reclist;

struct relevance *relevance_create(pp2_charset_t pct,
                                   NMEM nmem, const char **terms);
void relevance_destroy(struct relevance **rp);
void relevance_newrec(struct relevance *r, struct record_cluster *cluster);
void relevance_countwords(struct relevance *r, struct record_cluster *cluster,
                          const char *words, int multiplier, const char *name);
void relevance_donerecord(struct relevance *r, struct record_cluster *cluster);

void relevance_prepare_read(struct relevance *rel, struct reclist *rec);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

