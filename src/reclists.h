/* This file is part of Pazpar2.
   Copyright (C) 2006-2008 Index Data

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

#include "config.h"
#include "record.h"

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

struct reclist_sortparms * 
reclist_sortparms_insert_field_id(NMEM nmem,
                                  struct reclist_sortparms **sortparms,
                                  int field_id ,
                                  enum conf_sortkey_type type,
                                  int increasing);


struct reclist_sortparms * 
reclist_sortparms_insert(NMEM nmem, 
                         struct reclist_sortparms **sortparms,
                         struct conf_service * service,
                         const char * name,
                         int increasing);


struct reclist *reclist_create(NMEM, int numrecs);
struct record_cluster *reclist_insert( struct reclist *tl,
                                       struct conf_service *service,
                                       struct record  *record,
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
