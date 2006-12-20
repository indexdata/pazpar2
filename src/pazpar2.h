#ifndef PAZPAR2_H
#define PAZPAR2_H

struct record;

#include <netdb.h>

#include <yaz/comstack.h>
#include <yaz/pquery.h>
#include <yaz/ccl.h>
#include <yaz/yaz-ccl.h>
#include "termlists.h"
#include "relevance.h"
#include "eventl.h"

#define MAX_DATABASES 512

struct record {
    struct client *client;
    int target_offset;
    char *buf;
    char *merge_key;
    char *title;
    int relevance;
    int *term_frequency_vec;
    struct record *next_cluster;
};

struct connection;

// Represents a host (irrespective of databases)
struct host {
    char *hostport;
    char *ipport;
    struct connection *connections; // All connections to this
    struct host *next;
};

// Represents a (virtual) database on a host
struct database {
    struct host *host;
    char *url;
    char databases[MAX_DATABASES][128];
    int errors;
    struct database *next;
};

struct client;

// Represents a physical, reusable  connection to a remote Z39.50 host
struct connection {
    IOCHAN iochan;
    COMSTACK link;
    struct host *host;
    struct client *client;
    char *ibuf;
    int ibufsize;
    enum {
        Conn_Connecting,
        Conn_Open,
        Conn_Waiting,
    } state;
    struct connection *next;
};

// Represents client state for a connection to one search target
struct client {
    struct database *database;
    struct connection *connection;
    struct session *session;
    int hits;
    int records;
    int setno;
    int requestid;                              // ID of current outstanding request
    int diagnostic;
    enum client_state
    {
        Client_Connecting,
        Client_Connected,
	Client_Idle,
        Client_Initializing,
        Client_Searching,
        Client_Presenting,
        Client_Error,
        Client_Failed,
        Client_Disconnected,
        Client_Stopped
    } state;
    struct client *next;
};

#define SESSION_WATCH_RECORDS   0
#define SESSION_WATCH_MAX       0

typedef void (*session_watchfun)(void *data);

// End-user session
struct session {
    struct client *clients;
    int requestid; 
    char query[1024];
    NMEM nmem;          // Nmem for each operation (i.e. search)
    WRBUF wrbuf;        // Wrbuf for scratch(i.e. search)
    struct termlist *termlist;
    struct relevance *relevance;
    struct reclist *reclist;
    struct {
        void *data;
        session_watchfun fun;
    } watchlist[SESSION_WATCH_MAX + 1];
    int total_hits;
    int total_records;
};

struct statistics {
    int num_clients;
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
    int connected;
};

struct parameters {
    int timeout;		/* operations timeout, in seconds */
    char implementationId[128];
    char implementationName[128];
    char implementationVersion[128];
    int target_timeout; // seconds
    int session_timeout;
    int toget;
    int chunk;
    CCL_bibset ccl_filter;
    yaz_marc_t yaz_marc;
    ODR odr_out;
    ODR odr_in;
};

struct hitsbytarget *hitsbytarget(struct session *s, int *count);
int select_targets(struct session *se);
struct session *new_session();
void destroy_session(struct session *s);
int load_targets(struct session *s, const char *fn);
void statistics(struct session *s, struct statistics *stat);
char *search(struct session *s, char *query);
struct record **show(struct session *s, int start, int *num, int *total, int *sumhits);
struct termlist_score **termlist(struct session *s, int *num);
void session_set_watch(struct session *s, int what, session_watchfun fun, void *data);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
