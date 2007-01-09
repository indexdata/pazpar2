#ifndef RECLISTS_H
#define RECLISTS_H

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

struct reclist *reclist_create(NMEM, int numrecs);
struct record_cluster *reclist_insert(struct reclist *tl, struct record  *record,
		char *merge_key, int *total);
struct record_cluster *reclist_read_record(struct reclist *l);
void reclist_rewind(struct reclist *l);

#endif
