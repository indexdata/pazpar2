/*
 * $Id: relevance.c,v 1.1 2006-11-24 20:29:07 quinn Exp $
 */

#include <ctype.h>

#include "relevance.h"
#include "pazpar2.h"

struct relevance
{
    struct relevance_record *records;
    int num_records;
    int *doc_frequency_vec;
    int vec_len;
    struct word_trie *wt;
    NMEM nmem;
};

struct relevance_record
{
    struct record *record;
    int *term_frequency_vec;
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
            if (!n->list[c].child)
            {
                struct word_trie *new = create_word_trie_node(nmem);
                n->list[c].child = new;
            }
            if (!*(++term))
                n->list[c].termno = num;
            else
                word_trie_addterm(nmem, n->list[c].child, term, num);
            break;
        }
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
    res->num_records = 0;
    res->records = nmem_malloc(nmem, numrecs * sizeof(struct relevance_record *));
    res->wt = build_word_trie(nmem, terms);
    return res;
}

struct relevance_record *relevance_newrec(struct relevance *r, struct record *rec)
{
    struct relevance_record *res = nmem_malloc(r->nmem,
            sizeof(struct relevance_record));
    res->record = rec;
    res->term_frequency_vec = nmem_malloc(r->nmem, r->vec_len * sizeof(int));
    bzero(res->term_frequency_vec, r->vec_len * sizeof(int));
    return res;
}

void relevance_countwords(struct relevance_record *rec, const char *words, int len)
{
}

void relevance_donerecord(struct relevance_record *rec)
{
}

// Prepare for a relevance-sorted read of up to num entries
void relevance_prepare_read(struct relevance *r, int num)
{
}

struct record *relevance_read(struct relevance *r)
{
    return 0;
}

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
