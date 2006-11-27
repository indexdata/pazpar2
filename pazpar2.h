#ifndef PAZPAR2_H
#define PAZPAR2_H

struct record;

#include <yaz/pquery.h>
#include "termlists.h"
#include "relevance.h"

struct record {
    struct target *target;
    int target_offset;
    char *buf;
    char *merge_key;
    char *title;
    int relevance;
    int *term_frequency_vec;
    struct record *next_cluster;
};

struct session {
    struct target *targets;
    YAZ_PQF_Parser pqf_parser;
    int requestid; 
    char query[1024];
    NMEM nmem;
    WRBUF wrbuf;
    struct termlist *termlist;
    struct relevance *relevance;
    struct reclist *reclist;
    int total_hits;
    int total_records;
    yaz_marc_t yaz_marc;
};

struct statistics {
    int num_connections;
    int num_no_connection;
    int num_connecting;
    int num_initializing;
    int num_searching;
    int num_presenting;
    int num_idle;
    int num_failed;
    int num_error;
    int num_hits;
    int num_records;
};

struct hitsbytarget {
    char id[256];
    int hits;
    int diagnostic;
    int records;
    char* state;
};

struct hitsbytarget *hitsbytarget(struct session *s, int *count);
struct session *new_session();
void session_destroy(struct session *s);
int load_targets(struct session *s, const char *fn);
void statistics(struct session *s, struct statistics *stat);
char *search(struct session *s, char *query);
struct record **show(struct session *s, int start, int *num);
struct termlist_score **termlist(struct session *s, int *num);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
