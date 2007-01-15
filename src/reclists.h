#ifndef RECLISTS_H
#define RECLISTS_H

#include "config.h"

struct reclist
{
    struct reclist_bucket **hashtable;
    int hashtable_size;
    int hashmask;

    struct record_cluster **flatlist;
    int flatlist_size;
    int num_records;
    int pointer;

    NMEM nmem;
};

// This is a recipe for sorting. First node in list has highest priority
struct reclist_sortparms
{
    int offset;
    enum conf_sortkey_type type;
    int increasing;
    struct reclist_sortparms *next;
};

struct reclist *reclist_create(NMEM, int numrecs);
struct record_cluster *reclist_insert(struct reclist *tl, struct record  *record,
		char *merge_key, int *total);
void reclist_sort(struct reclist *l, struct reclist_sortparms *parms);
struct record_cluster *reclist_read_record(struct reclist *l);
void reclist_rewind(struct reclist *l);
struct reclist_sortparms *reclist_parse_sortparms(NMEM nmem, const char *parms);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
