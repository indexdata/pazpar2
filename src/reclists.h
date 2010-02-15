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

#ifndef RECLISTS_H
#define RECLISTS_H

#include "pazpar2_config.h"
#include "record.h"

struct reclist;

// This is a recipe for sorting. First node in list has highest priority
struct reclist_sortparms
{
    int offset;
    enum conf_sortkey_type type;
    int increasing;
    struct reclist_sortparms *next;
};

struct reclist *reclist_create(NMEM);
void reclist_destroy(struct reclist *l);
struct record_cluster *reclist_insert(struct reclist *tl,
                                      struct conf_service *service,
                                      struct record  *record,
                                      const char *merge_key, int *total);
void reclist_sort(struct reclist *l, struct reclist_sortparms *parms);
struct record_cluster *reclist_read_record(struct reclist *l);
void reclist_enter(struct reclist *l);
void reclist_leave(struct reclist *l);
struct reclist_sortparms *reclist_parse_sortparms(NMEM nmem, const char *parms,
    struct conf_service *service);

int reclist_get_num_records(struct reclist *l);
struct record_cluster *reclist_get_cluster(struct reclist *l, int i);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

