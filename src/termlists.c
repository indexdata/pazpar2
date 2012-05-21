/* This file is part of Pazpar2.
   Copyright (C) 2006-2012 Index Data

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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <yaz/yaz-util.h>

#include "termlists.h"
#include "jenkins_hash.h"

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
    unsigned hash_size;

    struct termlist_score **highscore;
    int highscore_size;
    int highscore_num;
    int highscore_min;

    NMEM nmem;
};

struct termlist *termlist_create(NMEM nmem, int highscore_size)
{
    struct termlist *res = nmem_malloc(nmem, sizeof(struct termlist));
    res->hash_size = 399;
    res->hashtable =
        nmem_malloc(nmem, res->hash_size * sizeof(struct termlist_bucket*));
    memset(res->hashtable, 0, res->hash_size * sizeof(struct termlist_bucket*));
    res->nmem = nmem;

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

void termlist_insert(struct termlist *tl, const char *display_term,
                     const char *norm_term, int freq)
{
    unsigned int bucket;
    struct termlist_bucket **p;
    char buf[256], *cp;

    if (strlen(norm_term) > 255)
        return;
    strcpy(buf, norm_term);
    /* chop right */
    /*
    for (cp = buf + strlen(buf); cp != buf && strchr(",. -", cp[-1]); cp--)
        cp[-1] = '\0';
    */
    bucket = jenkins_hash((unsigned char *)buf) % tl->hash_size;
    for (p = &tl->hashtable[bucket]; *p; p = &(*p)->next)
    {
        if (!strcmp(buf, (*p)->term.norm_term))
        {
            (*p)->term.frequency += freq;
            update_highscore(tl, &((*p)->term));
            break;
        }
    }
    if (!*p) // We made it to the end of the bucket without finding match
    {
        struct termlist_bucket *new = nmem_malloc(tl->nmem,
                sizeof(struct termlist_bucket));
        new->term.norm_term = nmem_strdup(tl->nmem, buf);
        new->term.display_term = *display_term ?
            nmem_strdup(tl->nmem, display_term) : new->term.norm_term;
        new->term.frequency = freq;
        new->next = 0;
        *p = new;
        update_highscore(tl, &new->term);
    }
}

static int compare(const void *s1, const void *s2)
{
    struct termlist_score **p1 = (struct termlist_score **) s1;
    struct termlist_score **p2 = (struct termlist_score **) s2;
    int d = (*p2)->frequency - (*p1)->frequency;
    if (d)
        return d;
    return strcmp((*p1)->display_term, (*p2)->display_term);
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
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

