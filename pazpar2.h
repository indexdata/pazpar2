#ifndef PAZPAR2_H
#define PAZPAR2_H

#include <yaz/pquery.h>

struct session {
    struct target *targets;
    YAZ_PQF_Parser pqf_parser;
    int requestid; 
    char query[1024];
    NMEM nmem;
};

struct record {
    struct target *target;
    int target_offset;
    char *buf;
    char *merge_key;
    struct record *next_cluster;
    struct record *head_cluster;
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
int load_targets(struct session *s, const char *fn);
void statistics(struct session *s, struct statistics *stat);
void search(struct session *s, char *query);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
