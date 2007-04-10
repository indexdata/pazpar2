/* $Id: reclists.h,v 1.5 2007-04-10 08:48:56 adam Exp $
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

#ifndef RECLISTS_H
#define RECLISTS_H

#include "config.h"

struct reclist
{
    struct reclist_bucket **hashtable;
    int hashtable_size;
    int hashmask;

    struct record_cluster **flatlist;
    int flatlist_size;
    int num_records;
    int pointer;

    NMEM nmem;
};

// This is a recipe for sorting. First node in list has highest priority
struct reclist_sortparms
{
    int offset;
    enum conf_sortkey_type type;
    int increasing;
    struct reclist_sortparms *next;
};

struct reclist *reclist_create(NMEM, int numrecs);
struct record_cluster *reclist_insert(struct reclist *tl, struct record  *record,
		char *merge_key, int *total);
void reclist_sort(struct reclist *l, struct reclist_sortparms *parms);
struct record_cluster *reclist_read_record(struct reclist *l);
void reclist_rewind(struct reclist *l);
struct reclist_sortparms *reclist_parse_sortparms(NMEM nmem, const char *parms);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
