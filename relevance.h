#ifndef RELEVANCE_H
#define RELEVANCE_H

#include <yaz/yaz-util.h>

#include "pazpar2.h"
#include "reclists.h"

struct relevance;

struct relevance *relevance_create(NMEM nmem, const char **terms, int numrecs);
void relevance_newrec(struct relevance *r, struct record *rec);
void relevance_countwords(struct relevance *r, struct record *rec,
        const char *words, int len);
void relevance_donerecord(struct relevance *r, struct record *rec);

void relevance_prepare_read(struct relevance *rel, struct reclist *rec);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
