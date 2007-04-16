/* $Id: reclists.c,v 1.10 2007-04-16 13:57:25 marc Exp $
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

#include <assert.h>

#if HAVE_CONFIG_H
#include <cconfig.h>
#endif

#include <yaz/yaz-util.h>

#include "pazpar2.h"
#include "reclists.h"

extern struct parameters global_parameters;

// Not threadsafe
static struct reclist_sortparms *sortparms = 0;

struct reclist_bucket
{
    struct record_cluster *record;
    struct reclist_bucket *next;
};

struct reclist_sortparms *reclist_parse_sortparms(NMEM nmem, const char *parms)
{
    struct reclist_sortparms *res = 0;
    struct reclist_sortparms **rp = &res;
    struct conf_service *service = config->servers->service;

    if (strlen(parms) > 256)
        return 0;
    while (*parms)
    {
        char parm[256];
        char *pp;
        const char *cpp;
        int increasing;
        int i;
        int offset;
        enum conf_sortkey_type type;
        struct reclist_sortparms *new;

        if (!(cpp = strchr(parms, ',')))
            cpp = parms + strlen(parms);
        strncpy(parm, parms, cpp - parms); 
        parm[cpp-parms] = '\0';

        if ((pp = strchr(parm, ':')))
        {
            increasing = pp[1] == '1' ? 1 : 0;
            *pp = '\0';
        }
        else
            increasing = 0;
        if (!strcmp(parm, "relevance"))
        {
            type = Metadata_sortkey_relevance;
            offset = -1;
        }
        else
        {
            for (i = 0; i < service->num_sortkeys; i++)
            {
                struct conf_sortkey *sk = &service->sortkeys[i];
                if (!strcmp(sk->name, parm))
                {
                    type = sk->type;
                    if (type == Metadata_sortkey_skiparticle)
                        type = Metadata_sortkey_string;
                    break;
                }
            }
            if (i >= service->num_sortkeys)
            {
                yaz_log(YLOG_FATAL, "Bad sortkey: %s", parm);
                return 0;
            }
            else
                offset = i;
        }
        new = *rp = nmem_malloc(nmem, sizeof(struct reclist_sortparms));
        new->next = 0;
        new->offset = offset;
        new->type = type;
        new->increasing = increasing;
        rp = &new->next;
        if (*(parms = cpp))
            parms++;
    }
    return res;
}

static int reclist_cmp(const void *p1, const void *p2)
{
    struct record_cluster *r1 = (*(struct record_cluster**) p1);
    struct record_cluster *r2 = (*(struct record_cluster**) p2);
    struct reclist_sortparms *s;

    for (s = sortparms; s; s = s->next)
    {
        int res;
        switch (s->type)
        {
            case Metadata_sortkey_relevance:
                res = r2->relevance - r1->relevance;
                break;
            case Metadata_sortkey_string:
                res = strcmp(r2->sortkeys[s->offset]->text, r1->sortkeys[s->offset]->text);
                break;
            case Metadata_sortkey_numeric:
                res = 0;
                break;
            default:
                yaz_log(YLOG_FATAL, "Bad sort type: %d", s->type);
                exit(1);
        }
        if (res)
        {
            if (s->increasing)
                res *= -1;
            return res;
        }
    }
    return 0;
}

void reclist_sort(struct reclist *l, struct reclist_sortparms *parms)
{
    sortparms = parms;
    qsort(l->flatlist, l->num_records, sizeof(struct record_cluster*), reclist_cmp);
    reclist_rewind(l);
}

struct record_cluster *reclist_read_record(struct reclist *l)
{
    if (l->pointer < l->num_records)
        return l->flatlist[l->pointer++];
    else
        return 0;
}

void reclist_rewind(struct reclist *l)
{
    l->pointer = 0;
}

// Jenkins one-at-a-time hash (from wikipedia)
static unsigned int hash(const unsigned char *key)
{
    unsigned int hash = 0;

    while (*key)
    {
        hash += *(key++);
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

struct reclist *reclist_create(NMEM nmem, int numrecs)
{
    int hashsize = 1;
    struct reclist *res;

    assert(numrecs);
    while (hashsize < numrecs)
        hashsize <<= 1;
    res = nmem_malloc(nmem, sizeof(struct reclist));
    res->hashtable = nmem_malloc(nmem, hashsize * sizeof(struct reclist_bucket*));
    memset(res->hashtable, 0, hashsize * sizeof(struct reclist_bucket*));
    res->hashtable_size = hashsize;
    res->nmem = nmem;
    res->hashmask = hashsize - 1; // Creates a bitmask

    res->num_records = 0;
    res->flatlist = nmem_malloc(nmem, numrecs * sizeof(struct record_cluster*));
    res->flatlist_size = numrecs;

    return res;
}

// Insert a record. Return record cluster (newly formed or pre-existing)
struct record_cluster *reclist_insert( struct conf_service *service,
                                       struct reclist *l, 
                                       struct record  *record,
                                       char *merge_key, int *total)
{
    unsigned int bucket;
    struct reclist_bucket **p;
    struct record_cluster *cluster = 0;
    
    assert(service);
    assert(service->num_metadata);
    assert(service->num_sortkeys);

    bucket = hash((unsigned char*) merge_key) & l->hashmask;
    for (p = &l->hashtable[bucket]; *p; p = &(*p)->next)
    {
        // We found a matching record. Merge them
        if (!strcmp(merge_key, (*p)->record->merge_key))
        {
            struct record_cluster *existing = (*p)->record;
            record->next = existing->records;
            existing->records = record;
            cluster = existing;
            break;
        }
    }
    if (!cluster && l->num_records < l->flatlist_size)
    {
        struct reclist_bucket *new =
            nmem_malloc(l->nmem, sizeof(struct reclist_bucket));
        struct record_cluster *newc =
            nmem_malloc(l->nmem, sizeof(struct record_cluster));
        
        record->next = 0;
        new->record = newc;
        new->next = 0;
        newc->records = record;
        newc->merge_key = merge_key;
        newc->relevance = 0;
        newc->term_frequency_vec = 0;
        newc->recid = (*total)++;
        newc->metadata = nmem_malloc(l->nmem,
                sizeof(struct record_metadata*) * service->num_metadata);
        memset(newc->metadata, 0, sizeof(struct record_metadata*) * service->num_metadata);
        newc->sortkeys = nmem_malloc(l->nmem,
                sizeof(struct record_metadata*) * service->num_sortkeys);
        memset(newc->sortkeys, 0, sizeof(union data_types*) * service->num_sortkeys);

        *p = new;
        l->flatlist[l->num_records++] = newc;
        cluster = newc;
    }
    return cluster;
}


/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
