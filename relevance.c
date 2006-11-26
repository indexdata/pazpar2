/*
 * $Id: relevance.c,v 1.2 2006-11-26 05:15:43 quinn Exp $
 */

#include <ctype.h>
#include <math.h>
#include <stdlib.h>

#include "relevance.h"
#include "pazpar2.h"

struct relevance
{
    int *doc_frequency_vec;
    int vec_len;
    struct word_trie *wt;
    NMEM nmem;
};

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

#define raw_char(c) (((c) >= 'a' && (c) <= 'z') ? (c) - 'a' : -1)

static int word_trie_match(struct word_trie *t, const char *word, int len, int *skipped)
{
    int c = raw_char(tolower(*word));

    if (!len)
        return 0;

    word++; len--;
    (*skipped)++;
    if (!len || raw_char(*word) < 0)
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
            return word_trie_match(t->list[c].child, word, len, skipped);
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

struct relevance *relevance_create(NMEM nmem, const char **terms, int numrecs)
{
    struct relevance *res = nmem_malloc(nmem, sizeof(struct relevance));
    const char **p;
    int i;

    for (p = terms, i = 0; *p; p++, i++)
        ;
    res->vec_len = ++i;
    res->doc_frequency_vec = nmem_malloc(nmem, res->vec_len * sizeof(int));
    bzero(res->doc_frequency_vec, res->vec_len * sizeof(int));
    res->nmem = nmem;
    res->wt = build_word_trie(nmem, terms);
    return res;
}

void relevance_newrec(struct relevance *r, struct record *rec)
{
    if (!rec->term_frequency_vec)
    {
        rec->term_frequency_vec = nmem_malloc(r->nmem, r->vec_len * sizeof(int));
        bzero(rec->term_frequency_vec, r->vec_len * sizeof(int));
    }
}


// FIXME. The definition of a word is crude here.. should support
// some form of localization mechanism?
void relevance_countwords(struct relevance *r, struct record *head,
        const char *words, int len)
{
    while (len)
    {
        char c;
        int res;
        int skipped;
        while (len && (c = raw_char(tolower(*words))) < 0)
        {
            words++;
            len--;
        }
        if (!len)
            return;
        skipped = 0;
        if ((res = word_trie_match(r->wt, words, len, &skipped)))
        {
            words += skipped;
            len -= skipped;
            head->term_frequency_vec[res]++;
        }
        else
        {
            while (len && (c = raw_char(tolower(*words))) >= 0)
            {
                words++;
                len--;
            }
        }
        head->term_frequency_vec[0]++;
    }
}

void relevance_donerecord(struct relevance *r, struct record *head)
{
    int i;

    for (i = 1; i < r->vec_len; i++)
        if (head->term_frequency_vec[i] > 0)
            r->doc_frequency_vec[i]++;

    r->doc_frequency_vec[0]++;
}

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
    struct record **r1 = (struct record **) p1;
    struct record **r2 = (struct record **) p2;
    return (*r2)->relevance - (*r1)->relevance;
}
#endif

// Prepare for a relevance-sorted read of up to num entries
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
            idfvec[i] = log((float) rel->doc_frequency_vec[0] / rel->doc_frequency_vec[i]);
    }
    // Calculate relevance for each document
    for (i = 0; i < reclist->num_records; i++)
    {
        int t;
        struct record *rec = reclist->flatlist[i];
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
    qsort(reclist->flatlist, reclist->num_records, sizeof(struct record*), comp);
    reclist->pointer = 0;
}

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
