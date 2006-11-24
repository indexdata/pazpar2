#ifndef RECLISTS_H
#define RECLISTS_H

struct reclist;

struct reclist *reclist_create(NMEM, int numrecs);
void reclist_insert(struct reclist *tl, struct record  *record);
struct record *reclist_read_record(struct reclist *l);
void reclist_rewind(struct reclist *l);

#endif
