/* $Id: relevance.c,v 1.12 2007-05-10 09:26:19 adam Exp $
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

#include <ctype.h>
#include <math.h>
#include <stdlib.h>

#if HAVE_CONFIG_H
#include <cconfig.h>
#endif

#include "relevance.h"
#include "pazpar2.h"

#define USE_TRIE 0

struct relevance
{
    int *doc_frequency_vec;
    int vec_len;
#if USE_TRIE
    struct word_trie *wt;
#else
    struct word_entry *entries;
#endif
    NMEM nmem;
};

#define raw_char(c) (((c) >= 'a' && (c) <= 'z') ? (c) - 'a' : -1)


#if USE_TRIE
// We use this data structure to recognize terms in input records,
// and map them to record term vectors for counting.
struct word_trie
{
    struct
    {
        struct word_trie *child;
        int termno;
    } list[26];
};

static struct word_trie *create_word_trie_node(NMEM nmem)
{
    struct word_trie *res = nmem_malloc(nmem, sizeof(struct word_trie));
    int i;
    for (i = 0; i < 26; i++)
    {
        res->list[i].child = 0;
        res->list[i].termno = -1;
    }
    return res;
}

static void word_trie_addterm(NMEM nmem, struct word_trie *n, const char *term, int num)
{

    while (*term) {
        int c = tolower(*term);
        if (c < 'a' || c > 'z')
            term++;
        else
        {
            c -= 'a';
            if (!*(++term))
                n->list[c].termno = num;
            else
            {
                if (!n->list[c].child)
                {
                    struct word_trie *new = create_word_trie_node(nmem);
                    n->list[c].child = new;
                }
                word_trie_addterm(nmem, n->list[c].child, term, num);
            }
            break;
        }
    }
}

static int word_trie_match(struct word_trie *t, const char *word, int *skipped)
{
    int c = raw_char(tolower(*word));

    if (!*word)
        return 0;

    word++;
    (*skipped)++;
    if (!*word || raw_char(*word) < 0)
    {
        if (t->list[c].termno > 0)
            return t->list[c].termno;
        else
            return 0;
    }
    else
    {
        if (t->list[c].child)
        {
            return word_trie_match(t->list[c].child, word, skipped);
        }
        else
            return 0;
    }

}


static struct word_trie *build_word_trie(NMEM nmem, const char **terms)
{
    struct word_trie *res = create_word_trie_node(nmem);
    const char **p;
    int i;

    for (i = 1, p = terms; *p; p++, i++)
        word_trie_addterm(nmem, res, *p, i);
    return res;
}

#else

struct word_entry {
    const char *norm_str;
    int termno;
    struct word_entry *next;
};

static void add_word_entry(NMEM nmem, 
                           struct word_entry **entries,
                           const char *norm_str,
                           int term_no)
{
    struct word_entry *ne = nmem_malloc(nmem, sizeof(*ne));
    ne->norm_str = nmem_strdup(nmem, norm_str);
    ne->termno = term_no;
    
    ne->next = *entries;
    *entries = ne;
}


int word_entry_match(struct word_entry *entries, const char *norm_str)
{
    for (; entries; entries = entries->next)
    {
        if (!strcmp(norm_str, entries->norm_str))
            return entries->termno;
    }
    return 0;
}

static struct word_entry *build_word_entries(NMEM nmem,
                                             const char **terms)
{
    int termno = 1; /* >0 signals THERE is an entry */
    struct word_entry *entries = 0;
    const char **p = terms;
    WRBUF norm_str = wrbuf_alloc();

    for (; *p; p++)
    {
        const char *cp = *p;
        for (; *cp; cp++)
        {
            int c = raw_char(*cp);
            if (c >= 0)
                wrbuf_putc(norm_str, c);
            else
            {
                if (wrbuf_len(norm_str))
                    add_word_entry(nmem, &entries, wrbuf_cstr(norm_str),
                                   termno);
                wrbuf_rewind(norm_str);
            }
        }
        if (wrbuf_len(norm_str))
            add_word_entry(nmem, &entries, wrbuf_cstr(norm_str), termno);
        wrbuf_rewind(norm_str);
        termno++;
    }
    wrbuf_destroy(norm_str);
    return entries;
}



#endif



struct relevance *relevance_create(NMEM nmem, const char **terms, int numrecs)
{
    struct relevance *res = nmem_malloc(nmem, sizeof(struct relevance));
    const char **p;
    int i;

    for (p = terms, i = 0; *p; p++, i++)
        ;
    res->vec_len = ++i;
    res->doc_frequency_vec = nmem_malloc(nmem, res->vec_len * sizeof(int));
    memset(res->doc_frequency_vec, 0, res->vec_len * sizeof(int));
    res->nmem = nmem;
#if USE_TRIE
    res->wt = build_word_trie(nmem, terms);
#else
    res->entries = build_word_entries(nmem, terms);
#endif
    return res;
}

void relevance_newrec(struct relevance *r, struct record_cluster *rec)
{
    if (!rec->term_frequency_vec)
    {
        rec->term_frequency_vec = nmem_malloc(r->nmem, r->vec_len * sizeof(int));
        memset(rec->term_frequency_vec, 0, r->vec_len * sizeof(int));
    }
}


// FIXME. The definition of a word is crude here.. should support
// some form of localization mechanism?
void relevance_countwords(struct relevance *r, struct record_cluster *cluster,
        const char *words, int multiplier)
{
#if !USE_TRIE
    WRBUF norm_str = wrbuf_alloc();
#endif
    while (*words)
    {
        char c;
        int res;
#if USE_TRIE
        int skipped = 0;
#endif
        while (*words && (c = raw_char(tolower(*words))) < 0)
            words++;
        if (!*words)
            return;
#if USE_TRIE
        res = word_trie_match(r->wt, words, &skipped);
        if (res)
        {
            words += skipped;
            cluster->term_frequency_vec[res] += multiplier;
        }
        else
        {
            while (*words && (c = raw_char(tolower(*words))) >= 0)
                words++;
        }
#else
        while (*words && (c = raw_char(tolower(*words))) >= 0)
        {
            wrbuf_putc(norm_str, c);
            words++;
        }
        res = word_entry_match(r->entries, wrbuf_cstr(norm_str));
        if (res)
            cluster->term_frequency_vec[res] += multiplier;
        wrbuf_rewind(norm_str);
#endif
        cluster->term_frequency_vec[0]++;
    }
#if !USE_TRIE
    wrbuf_destroy(norm_str);
#endif
}

void relevance_donerecord(struct relevance *r, struct record_cluster *cluster)
{
    int i;

    for (i = 1; i < r->vec_len; i++)
        if (cluster->term_frequency_vec[i] > 0)
            r->doc_frequency_vec[i]++;

    r->doc_frequency_vec[0]++;
}

#ifdef GAGA
#ifdef FLOAT_REL
static int comp(const void *p1, const void *p2)
{
    float res;
    struct record **r1 = (struct record **) p1;
    struct record **r2 = (struct record **) p2;
    res = (*r2)->relevance - (*r1)->relevance;
    if (res > 0)
        return 1;
    else if (res < 0)
        return -1;
    else
        return 0;
}
#else
static int comp(const void *p1, const void *p2)
{
    struct record_cluster **r1 = (struct record_cluster **) p1;
    struct record_cluster **r2 = (struct record_cluster **) p2;
    return (*r2)->relevance - (*r1)->relevance;
}
#endif
#endif

// Prepare for a relevance-sorted read
void relevance_prepare_read(struct relevance *rel, struct reclist *reclist)
{
    int i;
    float *idfvec = xmalloc(rel->vec_len * sizeof(float));

    // Calculate document frequency vector for each term.
    for (i = 1; i < rel->vec_len; i++)
    {
        if (!rel->doc_frequency_vec[i])
            idfvec[i] = 0;
        else
        {
            // This conditional may be terribly wrong
            // It was there to address the situation where vec[0] == vec[i]
            // which leads to idfvec[i] == 0... not sure about this
            // Traditional TF-IDF may assume that a word that occurs in every
            // record is irrelevant, but this is actually something we will
            // see a lot
            if ((idfvec[i] = log((float) rel->doc_frequency_vec[0] /
                            rel->doc_frequency_vec[i])) < 0.0000001)
                idfvec[i] = 1;
        }
    }
    // Calculate relevance for each document
    for (i = 0; i < reclist->num_records; i++)
    {
        int t;
        struct record_cluster *rec = reclist->flatlist[i];
        float relevance;
        relevance = 0;
        for (t = 1; t < rel->vec_len; t++)
        {
            float termfreq;
            if (!rec->term_frequency_vec[0])
                break;
            termfreq = (float) rec->term_frequency_vec[t] / rec->term_frequency_vec[0];
            relevance += termfreq * idfvec[t];
        }
        rec->relevance = (int) (relevance * 100000);
    }
#ifdef GAGA
    qsort(reclist->flatlist, reclist->num_records, sizeof(struct record*), comp);
#endif
    reclist->pointer = 0;
    xfree(idfvec);
}

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
