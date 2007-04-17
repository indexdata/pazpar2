/* $Id: pazpar2.h,v 1.25 2007-04-17 21:25:26 quinn Exp $
   Copyright (c) 2006-2007, Index Data.

This file is part of Pazpar2.

Pazpar2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

Pazpar2 is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with Pazpar2; see the file LICENSE.  If not, write to the
Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.
 */

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
    char **databases;
    int errors;
    struct zr_explain *explain;
    struct setting **settings;
    struct database *next;
    CCL_bibset ccl_map;
    yaz_marc_t yaz_marc;
    struct database_retrievalmap *map;
};

// Normalization filter. Turns incoming record into internal representation
// Simple sequence of stylesheets run in series.
struct database_retrievalmap {
    xsltStylesheet *stylesheet;
    struct database_retrievalmap *next;
};

struct database_criterion_value {
    char *value;
    struct database_criterion_value *next;
};

struct database_criterion {
    char *name;
    struct database_criterion_value *values;
    struct database_criterion *next;
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
    struct session_database *database;
    struct connection *connection;
    struct session *session;
    char *pquery; // Current search
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

// Represents a database as viewed from one session, possibly with settings overriden
// for that session (to support authorization/authentication)
struct session_database
{
    struct database *database;
    struct setting **settings;
    struct session_database *next;
};

// End-user session
struct session {
    struct session_database *databases;  // All databases, settings overriden
    struct client *clients;              // Clients connected for current search
    int requestid; 
    char query[1024];
    NMEM session_nmem;  // Nmem for session-permanent storage
    NMEM nmem;          // Nmem for each operation (i.e. search, result set, etc)
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
    char zproxy_override[128];
    char settings_path_override[128];
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
    ODR odr_out;
    ODR odr_in;
};

struct hitsbytarget *hitsbytarget(struct session *s, int *count);
int select_targets(struct session *se, struct database_criterion *crit);
struct session *new_session(NMEM nmem);
void destroy_session(struct session *s);
int load_targets(struct session *s, const char *fn);
void statistics(struct session *s, struct statistics *stat);
char *search(struct session *s, char *query, char *filter);
struct record_cluster **show(struct session *s, struct reclist_sortparms *sp, int start,
        int *num, int *total, int *sumhits, NMEM nmem_show);
struct record_cluster *show_single(struct session *s, int id);
struct termlist_score **termlist(struct session *s, const char *name, int *num);
void session_set_watch(struct session *s, int what, session_watchfun fun, void *data);
int session_active_clients(struct session *s);
void session_apply_setting(struct session *se, char *dbname, char *setting, char *value);
char *session_setting_oneval(struct session_database *db, int offset);

void start_http_listener(void);
void start_proxy(void);
void start_zproxy(void);

extern struct parameters global_parameters;
extern IOCHAN channel_list;

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
