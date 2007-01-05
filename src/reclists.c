/*
 * $Id: reclists.c,v 1.2 2007-01-05 20:33:05 adam Exp $
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

struct record *reclist_insert(struct reclist *l, struct record  *record)
{
    unsigned int bucket;
    struct reclist_bucket **p;
    struct record *head = 0;

    bucket = hash((unsigned char*) record->merge_key) & l->hashmask;
    for (p = &l->hashtable[bucket]; *p; p = &(*p)->next)
    {
        // We found a matching record. Merge them
        if (!strcmp(record->merge_key, (*p)->record->merge_key))
        {
            struct record *existing = (*p)->record;
            record->next_cluster = existing->next_cluster;
            existing->next_cluster = record;
            head = existing;
            break;
        }
    }
    if (!head && l->num_records < l->flatlist_size)
    {
        struct reclist_bucket *new =
            nmem_malloc(l->nmem, sizeof(struct reclist_bucket));
        
        assert(!*p);
        
        new->record = record;
        record->next_cluster = 0;
        new->next = 0;
        *p = new;
        assert(l->num_records < l->flatlist_size);
        l->flatlist[l->num_records++] = record;
        head = record;
    }
    return head;
}


/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
