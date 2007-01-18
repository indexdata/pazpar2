#ifndef PAZPAR2_H
#define PAZPAR2_H

struct record;

#include <netdb.h>

#include <libxslt/xsltutils.h>
#include <libxslt/transform.h>

#include <yaz/comstack.h>
#include <yaz/pquery.h>
#include <yaz/ccl.h>
#include <yaz/yaz-ccl.h>

#include "termlists.h"
#include "relevance.h"
#include "reclists.h"
#include "eventl.h"
#include "config.h"

struct client;

union data_types {
    char *text;
    struct {
        int min;
        int max;
    } number;
};

struct record_metadata {
    union data_types data;
    struct record_metadata *next; // next item of this name
};

struct record {
    struct client *client;
    struct record_metadata **metadata; // Array mirrors list of metadata fields in config
    union data_types **sortkeys;       // Array mirrors list of sortkey fields in config
    struct record *next;  // Next in cluster of merged records
};

struct record_cluster
{
    struct record_metadata **metadata; // Array mirrors list of metadata fields in config
    union data_types **sortkeys;
    char *merge_key;
    int relevance;
    int *term_frequency_vec;
    int recid; // Set-specific ID for this record
    struct record *records;
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
    char *name;
    char **databases;
    int errors;
    struct conf_queryprofile *qprofile;
    struct conf_retrievalprofile *rprofile;
    struct database *next;
};


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

#define SESSION_MAX_TERMLISTS 10

typedef void (*session_watchfun)(void *data);

struct named_termlist
{
    char *name;
    struct termlist *termlist;
};

// End-user session
struct session {
    struct client *clients;
    int requestid; 
    char query[1024];
    NMEM nmem;          // Nmem for each operation (i.e. search)
    WRBUF wrbuf;        // Wrbuf for scratch(i.e. search)
    int num_termlists;
    struct named_termlist termlists[SESSION_MAX_TERMLISTS];
    struct relevance *relevance;
    struct reclist *reclist;
    struct {
        void *data;
        session_watchfun fun;
    } watchlist[SESSION_WATCH_MAX + 1];
    int expected_maxrecs;
    int total_hits;
    int total_records;
    int total_merged;
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
    char *id;
    char *name;
    int hits;
    int diagnostic;
    int records;
    char* state;
    int connected;
};

struct parameters {
    char proxy_override[128];
    char listener_override[128];
    struct conf_server *server;
    int dump_records;
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
struct record_cluster **show(struct session *s, struct reclist_sortparms *sp, int start,
        int *num, int *total, int *sumhits, NMEM nmem_show);
struct record_cluster *show_single(struct session *s, int id);
struct termlist_score **termlist(struct session *s, const char *name, int *num);
void session_set_watch(struct session *s, int what, session_watchfun fun, void *data);
int session_active_clients(struct session *s);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
