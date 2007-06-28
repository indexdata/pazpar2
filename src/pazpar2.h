/* $Id: pazpar2.h,v 1.44 2007-06-28 09:36:10 adam Exp $
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
#include "parameters.h"
#include "http.h"

struct record;
struct client;


enum pazpar2_error_code {
    PAZPAR2_NO_ERROR = 0,

    PAZPAR2_NO_SESSION,
    PAZPAR2_MISSING_PARAMETER,
    PAZPAR2_MALFORMED_PARAMETER_VALUE,
    PAZPAR2_MALFORMED_PARAMETER_ENCODING,
    PAZPAR2_MALFORMED_SETTING,
    PAZPAR2_HITCOUNTS_FAILED,
    PAZPAR2_RECORD_MISSING,
    PAZPAR2_NO_TARGETS,
    PAZPAR2_CONFIG_TARGET,
    PAZPAR2_RECORD_FAIL,
    PAZPAR2_NOT_IMPLEMENTED,

    PAZPAR2_LAST_ERROR
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

// Normalization filter. Turns incoming record into internal representation
// Simple sequence of stylesheets run in series.
struct database_retrievalmap {
    xsltStylesheet *stylesheet;
    struct database_retrievalmap *next;
};

// Represents a database as viewed from one session, possibly with settings overriden
// for that session
struct session_database
{
    pp2_charset_t pct;
    struct database *database;
    struct setting **settings;
    yaz_marc_t yaz_marc;
    struct database_retrievalmap *map;
    struct session_database *next;
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

struct session_watchentry {
    void *data;
    http_channel_observer_t obs;
    session_watchfun fun;
};

// End-user session
struct session {
    struct session_database *databases;  // All databases, settings overriden
    struct client *clients;              // Clients connected for current search
    int requestid; 
    NMEM session_nmem;  // Nmem for session-permanent storage
    NMEM nmem;          // Nmem for each operation (i.e. search, result set, etc)
    WRBUF wrbuf;        // Wrbuf for scratch(i.e. search)
    int num_termlists;
    struct named_termlist termlists[SESSION_MAX_TERMLISTS];
    struct relevance *relevance;
    struct reclist *reclist;
    struct session_watchentry watchlist[SESSION_WATCH_MAX + 1];
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
    const char *state;
    int connected;
};

struct hitsbytarget *hitsbytarget(struct session *s, int *count);
int select_targets(struct session *se, struct database_criterion *crit);
struct session *new_session(NMEM nmem);
void destroy_session(struct session *s);
void session_init_databases(struct session *s);
int load_targets(struct session *s, const char *fn);
void statistics(struct session *s, struct statistics *stat);
enum pazpar2_error_code search(struct session *s, char *query, 
                               char *filter, const char **addinfo);
struct record_cluster **show(struct session *s, struct reclist_sortparms *sp, int start,
        int *num, int *total, int *sumhits, NMEM nmem_show);
struct record_cluster *show_single(struct session *s, int id);
struct termlist_score **termlist(struct session *s, const char *name, int *num);
int session_set_watch(struct session *s, int what, session_watchfun fun, void *data, struct http_channel *c);
int session_active_clients(struct session *s);
void session_apply_setting(struct session *se, char *dbname, char *setting, char *value);
char *session_setting_oneval(struct session_database *db, int offset);

void start_http_listener(void);
void start_proxy(void);

void pazpar2_add_channel(IOCHAN c);
void pazpar2_event_loop(void);

int host_getaddrinfo(struct host *host);

xmlDoc *normalize_record(struct session_database *sdb, Z_External *rec);
xmlDoc *record_to_xml(struct session_database *sdb, Z_External *rec);

struct record *ingest_record(struct client *cl, Z_External *rec,
                             int record_no);
void session_alert_watch(struct session *s, int what);
void pull_terms(NMEM nmem, struct ccl_rpn_node *n, char **termlist, int *num);

int pazpar2_process(int debug, int daemon,
                    void (*work)(void *data), void *data,
                    const char *pidfile, const char *uid);


#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
