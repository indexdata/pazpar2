#ifndef TERMLISTS_H
#define TERMLISTS_H

#include <yaz/nmem.h>

struct termlist_score
{
    char *term;
    int frequency;
};

struct termlist;

struct termlist *termlist_create(NMEM nmem, int numterms, int highscore_size);
void termlist_insert(struct termlist *tl, const char *term);
struct termlist_score **termlist_highscore(struct termlist *tl, int *len);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
