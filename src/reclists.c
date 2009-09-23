/* This file is part of Pazpar2.
   Copyright (C) 2006-2009 Index Data

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

#include <assert.h>

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <yaz/yaz-util.h>

#include "pazpar2.h"
#include "reclists.h"
#include "jenkins_hash.h"

static struct reclist_sortparms *qsort_sortparms = 0; /* thread pr */

struct reclist_bucket
{
    struct record_cluster *record;
    struct reclist_bucket *next;
};

#if 0
struct reclist_sortparms * 
reclist_sortparms_insert_field_id(NMEM nmem,
                         struct reclist_sortparms **sortparms,
                         int field_id,
                         enum conf_sortkey_type type,
                         int increasing)
{
    struct reclist_sortparms * tmp_rlsp = 0;
    // assert(nmem);

    if(!sortparms || field_id < 0)
        return 0;

    // construct new reclist_sortparms
    tmp_rlsp  = nmem_malloc(nmem, sizeof(struct reclist_sortparms));
    tmp_rlsp->offset = field_id;
    tmp_rlsp->type = type;
    tmp_rlsp->increasing = increasing;


    // insert in *sortparms place, moving *sortparms one down the list
    tmp_rlsp->next = *sortparms;
    *sortparms = tmp_rlsp;

    return *sortparms;
};
#endif

#if 0
struct reclist_sortparms * 
reclist_sortparms_insert(NMEM nmem, 
                         struct reclist_sortparms **sortparms,
                         struct conf_service * service,
                         const char * name,
                         int increasing)
{
    int field_id = 0;

    if (!sortparms || !service || !name)  
        return 0;
    
    field_id = conf_service_sortkey_field_id(service, name);

    if (-1 == field_id)
        return 0;

    return reclist_sortparms_insert_field_id(nmem, sortparms, field_id,
                                             service->sortkeys[field_id].type,
                                             increasing);
};
#endif


struct reclist_sortparms *reclist_parse_sortparms(NMEM nmem, const char *parms,
    struct conf_service *service)
{
    struct reclist_sortparms *res = 0;
    struct reclist_sortparms **rp = &res;

    if (strlen(parms) > 256)
        return 0;
    while (*parms)
    {
        char parm[256];
        char *pp;
        const char *cpp;
        int increasing;
        int i;
        int offset = 0;
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
    int res = 0;

    for (s = qsort_sortparms; s && res == 0; s = s->next)
    {
        union data_types *ut1 = r1->sortkeys[s->offset];
        union data_types *ut2 = r2->sortkeys[s->offset];
        switch (s->type)
        {
            const char *s1, *s2;
            
            case Metadata_sortkey_relevance:
                res = r2->relevance - r1->relevance;
                break;
            case Metadata_sortkey_string:
                s1 = ut1 ? ut1->text.sort : "";
                s2 = ut2 ? ut2->text.sort : "";
                res = strcmp(s2, s1);
                if (res)
                {
                    if (s->increasing)
                        res *= -1;
                }
                break;
            case Metadata_sortkey_numeric:
                if (ut1 && ut2)
                {
                    if (s->increasing)
                        res = ut1->number.min  - ut2->number.min;
                    else
                        res = ut2->number.max  - ut1->number.max;
                }
                else if (ut1 && !ut2)
                    res = -1;
                else if (!ut1 && ut2)
                    res = 1;
                else
                    res = 0;
                break;
            default:
                yaz_log(YLOG_WARN, "Bad sort type: %d", s->type);
                res = 0;
        }
    }
    return res;
}

void reclist_sort(struct reclist *l, struct reclist_sortparms *parms)
{
    qsort_sortparms = parms;
    qsort(l->flatlist, l->num_records, 
          sizeof(struct record_cluster*), reclist_cmp);
    reclist_rewind(l);
}

struct record_cluster *reclist_read_record(struct reclist *l)
{
    if (l && l->pointer < l->num_records)
        return l->flatlist[l->pointer++];
    else
        return 0;
}

void reclist_rewind(struct reclist *l)
{
    if (l)
        l->pointer = 0;
}

struct reclist *reclist_create(NMEM nmem, int numrecs)
{
    int hashsize = 1;
    struct reclist *res;

    assert(numrecs);
    while (hashsize < numrecs)
        hashsize <<= 1;
    res = nmem_malloc(nmem, sizeof(struct reclist));
    res->hashtable 
        = nmem_malloc(nmem, hashsize * sizeof(struct reclist_bucket*));
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
struct record_cluster *reclist_insert( struct reclist *l,
                                       struct conf_service *service, 
                                       struct record  *record,
                                       char *merge_key, int *total)
{
    unsigned int bucket;
    struct reclist_bucket **p;
    struct record_cluster *cluster = 0;
    
    assert(service);
    assert(l);
    assert(record);
    assert(merge_key);
    assert(total);

    bucket = jenkins_hash((unsigned char*) merge_key) & l->hashmask;

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
        newc->recid = merge_key;
        (*total)++;
        newc->metadata = nmem_malloc(l->nmem,
                sizeof(struct record_metadata*) * service->num_metadata);
        memset(newc->metadata, 0, 
               sizeof(struct record_metadata*) * service->num_metadata);
        newc->sortkeys = nmem_malloc(l->nmem,
                sizeof(struct record_metadata*) * service->num_sortkeys);
        memset(newc->sortkeys, 0,
               sizeof(union data_types*) * service->num_sortkeys);

        *p = new;
        l->flatlist[l->num_records++] = newc;
        cluster = newc;
    }
    return cluster;
}


/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

