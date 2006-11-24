/*
 * $Id: reclists.c,v 1.1 2006-11-24 20:29:07 quinn Exp $
 */

#include <assert.h>

#include <yaz/yaz-util.h>

#include "pazpar2.h"
#include "reclists.h"

struct reclist_bucket
{
    struct record *record;
    struct reclist_bucket *next;
};

struct reclist
{
    struct reclist_bucket **hashtable;
    int hashtable_size;
    int hashmask;

    struct record **flatlist;
    int flatlist_size;
    int num_records;
    int pointer;

    NMEM nmem;
};

struct record *reclist_read_record(struct reclist *l)
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
    res->flatlist = nmem_malloc(nmem, numrecs * sizeof(struct record*));
    res->flatlist_size = numrecs;

    return res;
}

void reclist_insert(struct reclist *l, struct record  *record)
{
    unsigned int bucket;
    struct reclist_bucket **p;

    bucket = hash(record->merge_key) & l->hashmask;
    for (p = &l->hashtable[bucket]; *p; p = &(*p)->next)
    {
        // We found a matching record. Merge them
        if (!strcmp(record->merge_key, (*p)->record->merge_key))
        {
            struct record *existing = (*p)->record;
            yaz_log(YLOG_LOG, "Found a matching record: %s", record->merge_key);
            record->next_cluster = existing->next_cluster;
            existing->next_cluster = record;
            break;
        }
    }
    if (!*p) // We made it to the end of the bucket without finding match
    {
        yaz_log(YLOG_DEBUG, "Added a new record: %s", record->merge_key);
        struct reclist_bucket *new = nmem_malloc(l->nmem,
                sizeof(struct reclist_bucket));
        new->record = record;
        record->next_cluster = 0;
        new->next = 0;
        *p = new;
        l->flatlist[l->num_records++] = record;
    }
}


/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
