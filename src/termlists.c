/*
 * $Id: termlists.c,v 1.3 2007-01-08 18:32:35 quinn Exp $
 */

#include <stdlib.h>
#include <string.h>
#include <yaz/yaz-util.h>

#if HAVE_CONFIG_H
#include <cconfig.h>
#endif

#include "termlists.h"

// Discussion:
// As terms are found in incoming records, they are added to (or updated in) a
// Hash table. When term records are updated, a frequency value is updated. At
// the same time, a highscore is maintained for the most frequent terms.

struct termlist_bucket
{
    struct termlist_score term;
    struct termlist_bucket *next;
};

struct termlist
{
    struct termlist_bucket **hashtable;
    int hashtable_size;
    int hashmask;

    struct termlist_score **highscore;
    int highscore_size;
    int highscore_num;
    int highscore_min;

    NMEM nmem;
};


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

struct termlist *termlist_create(NMEM nmem, int numterms, int highscore_size)
{
    int hashsize = 1;
    int halfnumterms;
    struct termlist *res;

    // Calculate a hash size smallest power of 2 larger than 50% of expected numterms
    halfnumterms = numterms >> 1;
    if (halfnumterms < 0)
        halfnumterms = 1;
    while (hashsize < halfnumterms)
        hashsize <<= 1;
    res = nmem_malloc(nmem, sizeof(struct termlist));
    res->hashtable = nmem_malloc(nmem, hashsize * sizeof(struct termlist_bucket*));
    bzero(res->hashtable, hashsize * sizeof(struct termlist_bucket*));
    res->hashtable_size = hashsize;
    res->nmem = nmem;
    res->hashmask = hashsize - 1; // Creates a bitmask

    res->highscore = nmem_malloc(nmem, highscore_size * sizeof(struct termlist_score *));
    res->highscore_size = highscore_size;
    res->highscore_num = 0;
    res->highscore_min = 0;

    return res;
}

static void update_highscore(struct termlist *tl, struct termlist_score *t)
{
    int i;
    int smallest;
    int me = -1;

    if (tl->highscore_num > tl->highscore_size && t->frequency < tl->highscore_min)
        return;

    smallest = 0;
    for (i = 0; i < tl->highscore_num; i++)
    {
        if (tl->highscore[i]->frequency < tl->highscore[smallest]->frequency)
            smallest = i;
        if (tl->highscore[i] == t)
            me = i;
    }
    if (tl->highscore_num)
        tl->highscore_min = tl->highscore[smallest]->frequency;
    if (t->frequency < tl->highscore_min)
        tl->highscore_min = t->frequency;
    if (me >= 0)
        return;
    if (tl->highscore_num < tl->highscore_size)
    {
        tl->highscore[tl->highscore_num++] = t;
        if (t->frequency < tl->highscore_min)
            tl->highscore_min = t->frequency;
    }
    else
    {
        if (t->frequency > tl->highscore[smallest]->frequency)
        {
            tl->highscore[smallest] = t;
        }
    }
}

void termlist_insert(struct termlist *tl, const char *term)
{
    unsigned int bucket;
    struct termlist_bucket **p;

    bucket = hash((unsigned char *)term) & tl->hashmask;
    for (p = &tl->hashtable[bucket]; *p; p = &(*p)->next)
    {
        if (!strcmp(term, (*p)->term.term))
        {
            (*p)->term.frequency++;
            update_highscore(tl, &((*p)->term));
            break;
        }
    }
    if (!*p) // We made it to the end of the bucket without finding match
    {
        struct termlist_bucket *new = nmem_malloc(tl->nmem,
                sizeof(struct termlist_bucket));
        new->term.term = nmem_strdup(tl->nmem, term);
        new->term.frequency = 1;
        new->next = 0;
        *p = new;
        update_highscore(tl, &new->term);
    }
}

static int compare(const void *s1, const void *s2)
{
    struct termlist_score **p1 = (struct termlist_score**) s1, **p2 = (struct termlist_score **) s2;
    return (*p2)->frequency - (*p1)->frequency;
}

struct termlist_score **termlist_highscore(struct termlist *tl, int *len)
{
    qsort(tl->highscore, tl->highscore_num, sizeof(struct termlist_score*), compare);
    *len = tl->highscore_num;
    return tl->highscore;
}

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
