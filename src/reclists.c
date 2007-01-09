/*
 * $Id: reclists.c,v 1.5 2007-01-09 22:06:49 quinn Exp $
 */

#include <assert.h>

#if HAVE_CONFIG_H
#include <cconfig.h>
#endif

#include <yaz/yaz-util.h>

#include "pazpar2.h"
#include "reclists.h"

extern struct parameters global_parameters;

struct reclist_bucket
{
    struct record_cluster *record;
    struct reclist_bucket *next;
};

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
    bzero(res->hashtable, hashsize * sizeof(struct reclist_bucket*));
    res->hashtable_size = hashsize;
    res->nmem = nmem;
    res->hashmask = hashsize - 1; // Creates a bitmask

    res->num_records = 0;
    res->flatlist = nmem_malloc(nmem, numrecs * sizeof(struct record_cluster*));
    res->flatlist_size = numrecs;

    return res;
}

// Insert a record. Return record cluster (newly formed or pre-existing)
struct record_cluster *reclist_insert(struct reclist *l, struct record  *record,
        char *merge_key, int *total)
{
    unsigned int bucket;
    struct reclist_bucket **p;
    struct record_cluster *cluster = 0;
    struct conf_service *service = global_parameters.server->service;

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
        newc->metadata = 0;
        newc->metadata = nmem_malloc(l->nmem,
                sizeof(struct record_metadata*) * service->num_metadata);
        bzero(newc->metadata, sizeof(struct record_metadata*) * service->num_metadata);

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
