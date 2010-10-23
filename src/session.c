/* This file is part of Pazpar2.
   Copyright (C) 2006-2010 Index Data

Pazpar2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

Pazpar2 is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/

/** \file session.c
    \brief high-level logic; mostly user sessions and settings
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef WIN32
#include <windows.h>
#endif
#include <signal.h>
#include <ctype.h>
#include <assert.h>

#include <yaz/marcdisp.h>
#include <yaz/comstack.h>
#include <yaz/tcpip.h>
#include <yaz/proto.h>
#include <yaz/readconf.h>
#include <yaz/pquery.h>
#include <yaz/otherinfo.h>
#include <yaz/yaz-util.h>
#include <yaz/nmem.h>
#include <yaz/query-charset.h>
#include <yaz/querytowrbuf.h>
#include <yaz/oid_db.h>
#include <yaz/snprintf.h>
#include <yaz/gettimeofday.h>

#define USE_TIMING 0
#if USE_TIMING
#include <yaz/timing.h>
#endif

#include "ppmutex.h"
#include "parameters.h"
#include "session.h"
#include "eventl.h"
#include "http.h"
#include "termlists.h"
#include "reclists.h"
#include "relevance.h"
#include "database.h"
#include "client.h"
#include "settings.h"
#include "normalize7bit.h"

#define TERMLIST_HIGH_SCORE 25

#define MAX_CHUNK 15

// Note: Some things in this structure will eventually move to configuration
struct parameters global_parameters = 
{
    0,   // dump_records
    0    // debug_mode
};

struct client_list {
    struct client *client;
    struct client_list *next;
};

static void log_xml_doc(xmlDoc *doc)
{
    FILE *lf = yaz_log_file();
    xmlChar *result = 0;
    int len = 0;
#if LIBXML_VERSION >= 20600
    xmlDocDumpFormatMemory(doc, &result, &len, 1);
#else
    xmlDocDumpMemory(doc, &result, &len);
#endif
    if (lf && len)
    {
        (void) fwrite(result, 1, len, lf);
        fprintf(lf, "\n");
    }
    xmlFree(result);
}

static void session_enter(struct session *s)
{
    yaz_mutex_enter(s->session_mutex);
}

static void session_leave(struct session *s)
{
    yaz_mutex_leave(s->session_mutex);
}

// Recursively traverse query structure to extract terms.
void pull_terms(NMEM nmem, struct ccl_rpn_node *n, char **termlist, int *num)
{
    char **words;
    int numwords;
    int i;

    switch (n->kind)
    {
    case CCL_RPN_AND:
    case CCL_RPN_OR:
    case CCL_RPN_NOT:
    case CCL_RPN_PROX:
        pull_terms(nmem, n->u.p[0], termlist, num);
        pull_terms(nmem, n->u.p[1], termlist, num);
        break;
    case CCL_RPN_TERM:
        nmem_strsplit(nmem, " ", n->u.t.term, &words, &numwords);
        for (i = 0; i < numwords; i++)
            termlist[(*num)++] = words[i];
        break;
    default: // NOOP
        break;
    }
}


void add_facet(struct session *s, const char *type, const char *value, int count)
{
    int i;

    if (!*value)
        return;
    for (i = 0; i < s->num_termlists; i++)
        if (!strcmp(s->termlists[i].name, type))
            break;
    if (i == s->num_termlists)
    {
        if (i == SESSION_MAX_TERMLISTS)
        {
            yaz_log(YLOG_FATAL, "Too many termlists");
            return;
        }

        s->termlists[i].name = nmem_strdup(s->nmem, type);
        s->termlists[i].termlist 
            = termlist_create(s->nmem, TERMLIST_HIGH_SCORE);
        s->num_termlists = i + 1;
    }
    yaz_log(YLOG_DEBUG, "Session: facets for %s: %s (%d)", type, value, count);

    termlist_insert(s->termlists[i].termlist, value, count);
}

static xmlDoc *record_to_xml(struct session_database *sdb, const char *rec)
{
    struct database *db = sdb->database;
    xmlDoc *rdoc = 0;

    rdoc = xmlParseMemory(rec, strlen(rec));

    if (!rdoc)
    {
        yaz_log(YLOG_FATAL, "Non-wellformed XML received from %s",
                db->url);
        return 0;
    }

    if (global_parameters.dump_records)
    {
        yaz_log(YLOG_LOG, "Un-normalized record from %s", db->url);
        log_xml_doc(rdoc);
    }

    return rdoc;
}

#define MAX_XSLT_ARGS 16

// Add static values from session database settings if applicable
static void insert_settings_parameters(struct session_database *sdb,
                                       struct conf_service *service,
                                       char **parms,
                                       NMEM nmem)
{
    int i;
    int nparms = 0;
    int offset = 0;

    for (i = 0; i < service->num_metadata; i++)
    {
        struct conf_metadata *md = &service->metadata[i];
        int setting;

        if (md->setting == Metadata_setting_parameter &&
            (setting = settings_lookup_offset(service, md->name)) >= 0)
        {
            const char *val = session_setting_oneval(sdb, setting);
            if (val && nparms < MAX_XSLT_ARGS)
            {
                char *buf;
                int len = strlen(val);
                buf = nmem_malloc(nmem, len + 3);
                buf[0] = '\'';
                strcpy(buf + 1, val);
                buf[len+1] = '\'';
                buf[len+2] = '\0';
                parms[offset++] = md->name;
                parms[offset++] = buf;
                nparms++;
            }
        }
    }
    parms[offset] = 0;
}

// Add static values from session database settings if applicable
static void insert_settings_values(struct session_database *sdb, xmlDoc *doc,
    struct conf_service *service)
{
    int i;

    for (i = 0; i < service->num_metadata; i++)
    {
        struct conf_metadata *md = &service->metadata[i];
        int offset;

        if (md->setting == Metadata_setting_postproc &&
            (offset = settings_lookup_offset(service, md->name)) >= 0)
        {
            const char *val = session_setting_oneval(sdb, offset);
            if (val)
            {
                xmlNode *r = xmlDocGetRootElement(doc);
                xmlNode *n = xmlNewTextChild(r, 0, (xmlChar *) "metadata",
                                             (xmlChar *) val);
                xmlSetProp(n, (xmlChar *) "type", (xmlChar *) md->name);
            }
        }
    }
}

static xmlDoc *normalize_record(struct session_database *sdb,
                                struct conf_service *service,
                                const char *rec, NMEM nmem)
{
    xmlDoc *rdoc = record_to_xml(sdb, rec);

    if (rdoc)
    {
        char *parms[MAX_XSLT_ARGS*2+1];
        
        insert_settings_parameters(sdb, service, parms, nmem);
        
        if (normalize_record_transform(sdb->map, &rdoc, (const char **)parms))
        {
            yaz_log(YLOG_WARN, "Normalize failed from %s", sdb->database->url);
        }
        else
        {
            insert_settings_values(sdb, rdoc, service);
            
            if (global_parameters.dump_records)
            {
                yaz_log(YLOG_LOG, "Normalized record from %s", 
                        sdb->database->url);
                log_xml_doc(rdoc);
            }
        }
    }
    return rdoc;
}

void session_settings_dump(struct session *se,
                           struct session_database *db,
                           WRBUF w)
{
    if (db->settings)
    {
        int i, num = db->num_settings;
        for (i = 0; i < num; i++)
        {
            struct setting *s = db->settings[i];
            for (;s ; s = s->next)
            {
                wrbuf_puts(w, "<set name=\"");
                wrbuf_xmlputs(w, s->name);
                wrbuf_puts(w, "\" value=\"");
                wrbuf_xmlputs(w, s->value);
                wrbuf_puts(w, "\"/>");
            }
            if (db->settings[i])
                wrbuf_puts(w, "\n");
        }
    }
}

// Retrieve first defined value for 'name' for given database.
// Will be extended to take into account user associated with session
const char *session_setting_oneval(struct session_database *db, int offset)
{
    if (offset >= db->num_settings || !db->settings[offset])
        return "";
    return db->settings[offset]->value;
}

// Prepare XSLT stylesheets for record normalization
// Structures are allocated on the session_wide nmem to avoid having
// to recompute this for every search. This would lead
// to leaking if a single session was to repeatedly change the PZ_XSLT
// setting. However, this is not a realistic use scenario.
static int prepare_map(struct session *se, struct session_database *sdb)
{
    const char *s;

    if (!sdb->settings)
    {
        yaz_log(YLOG_WARN, "No settings on %s", sdb->database->url);
        return -1;
    }
    if ((s = session_setting_oneval(sdb, PZ_XSLT)))
    {
        char auto_stylesheet[256];

        if (!strcmp(s, "auto"))
        {
            const char *request_syntax = session_setting_oneval(
                sdb, PZ_REQUESTSYNTAX);
            if (request_syntax)
            {
                char *cp;
                yaz_snprintf(auto_stylesheet, sizeof(auto_stylesheet),
                             "%s.xsl", request_syntax);
                for (cp = auto_stylesheet; *cp; cp++)
                {
                    /* deliberately only consider ASCII */
                    if (*cp > 32 && *cp < 127)
                        *cp = tolower(*cp);
                }
                s = auto_stylesheet;
            }
            else
            {
                yaz_log(YLOG_WARN, "No pz:requestsyntax for auto stylesheet");
            }
        }
        sdb->map = normalize_cache_get(se->normalize_cache,
                                       se->service->server->config, s);
        if (!sdb->map)
            return -1;
    }
    return 0;
}

// This analyzes settings and recomputes any supporting data structures
// if necessary.
static int prepare_session_database(struct session *se, 
                                    struct session_database *sdb)
{
    if (!sdb->settings)
    {
        yaz_log(YLOG_WARN, 
                "No settings associated with %s", sdb->database->url);
        return -1;
    }
    if (sdb->settings[PZ_XSLT] && !sdb->map)
    {
        if (prepare_map(se, sdb) < 0)
            return -1;
    }
    return 0;
}

// called if watch should be removed because http_channel is to be destroyed
static void session_watch_cancel(void *data, struct http_channel *c,
                                 void *data2)
{
    struct session_watchentry *ent = data;

    ent->fun = 0;
    ent->data = 0;
    ent->obs = 0;
}

// set watch. Returns 0=OK, -1 if watch is already set
int session_set_watch(struct session *s, int what, 
                      session_watchfun fun, void *data,
                      struct http_channel *chan)
{
    int ret;
    session_enter(s);
    if (s->watchlist[what].fun)
        ret = -1;
    else
    {
        
        s->watchlist[what].fun = fun;
        s->watchlist[what].data = data;
        s->watchlist[what].obs = http_add_observer(chan, &s->watchlist[what],
                                                   session_watch_cancel);
        ret = 0;
    }
    session_leave(s);
    return 0;
}

void session_alert_watch(struct session *s, int what)
{
    assert(s);
    session_enter(s);
    if (s->watchlist[what].fun)
    {
        /* our watch is no longer associated with http_channel */
        void *data;
        session_watchfun fun;

        http_remove_observer(s->watchlist[what].obs);
        fun  = s->watchlist[what].fun;
        data = s->watchlist[what].data;

        /* reset watch before fun is invoked - in case fun wants to set
           it again */
        s->watchlist[what].fun = 0;
        s->watchlist[what].data = 0;
        s->watchlist[what].obs = 0;

        session_leave(s);
        yaz_log(YLOG_DEBUG, "session_alert_watch: %d calling function: %p", what, fun);
        fun(data);
    }
    else
        session_leave(s);
}

//callback for grep_databases
static void select_targets_callback(void *context, struct session_database *db)
{
    struct session *se = (struct session*) context;
    struct client *cl = client_create();
    struct client_list *l;
    client_set_database(cl, db);

    client_set_session(cl, se);
    l = xmalloc(sizeof(*l));
    l->client = cl;
    l->next = se->clients;
    se->clients = l;
}

static void session_remove_clients(struct session *se)
{
    struct client_list *l;

    session_enter(se);
    l = se->clients;
    se->clients = 0;
    session_leave(se);

    while (l)
    {
        struct client_list *l_next = l->next;
        client_lock(l->client);
        client_set_session(l->client, 0);
        client_set_database(l->client, 0);
        client_unlock(l->client);
        client_destroy(l->client);
        xfree(l);
        l = l_next;
    }
}

// Associates a set of clients with a session;
// Note: Session-databases represent databases with per-session 
// setting overrides
static int select_targets(struct session *se, const char *filter)
{
    return session_grep_databases(se, filter, select_targets_callback);
}

int session_active_clients(struct session *s)
{
    struct client_list *l;
    int res = 0;

    for (l = s->clients; l; l = l->next)
        if (client_is_active(l->client))
            res++;

    return res;
}

int session_is_preferred_clients_ready(struct session *s)
{
    struct client_list *l;
    int res = 0;

    for (l = s->clients; l; l = l->next)
        if (client_is_active_preferred(l->client))
            res++;
    yaz_log(YLOG_DEBUG, "%p Session has %d active preferred clients.", s, res);
    return res == 0;
}



enum pazpar2_error_code search(struct session *se,
                               const char *query,
                               const char *startrecs, const char *maxrecs,
                               const char *filter,
                               const char **addinfo)
{
    int live_channels = 0;
    int no_working = 0;
    int no_failed = 0;
    struct client_list *l;
    struct timeval tval;

    yaz_log(YLOG_DEBUG, "Search");

    *addinfo = 0;

    session_remove_clients(se);
    
    session_enter(se);
    reclist_destroy(se->reclist);
    se->reclist = 0;
    relevance_destroy(&se->relevance);
    nmem_reset(se->nmem);
    se->total_records = se->total_hits = se->total_merged = 0;
    se->num_termlists = 0;
    live_channels = select_targets(se, filter);
    if (!live_channels)
    {
        session_leave(se);
        return PAZPAR2_NO_TARGETS;
    }
    se->reclist = reclist_create(se->nmem);

    yaz_gettimeofday(&tval);
    
    tval.tv_sec += 5;

    for (l = se->clients; l; l = l->next)
    {
        struct client *cl = l->client;

        if (maxrecs)
            client_set_maxrecs(cl, atoi(maxrecs));
        if (startrecs)
            client_set_startrecs(cl, atoi(startrecs));
        if (prepare_session_database(se, client_get_database(cl)) < 0)
            ;
        else if (client_parse_query(cl, query) < 0)
            no_failed++;
        else
        {
            no_working++;
            if (client_prep_connection(cl, se->service->z3950_operation_timeout,
                                       se->service->z3950_session_timeout,
                                       se->service->server->iochan_man,
                                       &tval))
                client_start_search(cl);
        }
    }
    session_leave(se);
    if (no_working == 0)
    {
        if (no_failed > 0)
        {
            *addinfo = "query";
            return PAZPAR2_MALFORMED_PARAMETER_VALUE;
        }
        else
            return PAZPAR2_NO_TARGETS;
    }
    return PAZPAR2_NO_ERROR;
}

// Creates a new session_database object for a database
static void session_init_databases_fun(void *context, struct database *db)
{
    struct session *se = (struct session *) context;
    struct session_database *new = nmem_malloc(se->session_nmem, sizeof(*new));
    int i;

    new->database = db;
    
    new->map = 0;
    assert(db->settings);
    new->settings = nmem_malloc(se->session_nmem,
                                sizeof(struct settings *) * db->num_settings);
    new->num_settings = db->num_settings;
    for (i = 0; i < db->num_settings; i++)
    {
        struct setting *setting = db->settings[i];
        new->settings[i] = setting;
    }
    new->next = se->databases;
    se->databases = new;
}

// Doesn't free memory associated with sdb -- nmem takes care of that
static void session_database_destroy(struct session_database *sdb)
{
    sdb->map = 0;
}

// Initialize session_database list -- this represents this session's view
// of the database list -- subject to modification by the settings ws command
void session_init_databases(struct session *se)
{
    se->databases = 0;
    predef_grep_databases(se, se->service, session_init_databases_fun);
}

// Probably session_init_databases_fun should be refactored instead of
// called here.
static struct session_database *load_session_database(struct session *se, 
                                                      char *id)
{
    struct database *db = new_database(id, se->session_nmem);

    resolve_database(se->service, db);

    session_init_databases_fun((void*) se, db);

    // New sdb is head of se->databases list
    return se->databases;
}

// Find an existing session database. If not found, load it
static struct session_database *find_session_database(struct session *se, 
                                                      char *id)
{
    struct session_database *sdb;

    for (sdb = se->databases; sdb; sdb = sdb->next)
        if (!strcmp(sdb->database->url, id))
            return sdb;
    return load_session_database(se, id);
}

// Apply a session override to a database
void session_apply_setting(struct session *se, char *dbname, char *setting,
                           char *value)
{
    struct session_database *sdb = find_session_database(se, dbname);
    struct conf_service *service = se->service;
    struct setting *new = nmem_malloc(se->session_nmem, sizeof(*new));
    int offset = settings_create_offset(service, setting);

    expand_settings_array(&sdb->settings, &sdb->num_settings, offset,
                          se->session_nmem);
    new->precedence = 0;
    new->target = dbname;
    new->name = setting;
    new->value = value;
    new->next = sdb->settings[offset];
    sdb->settings[offset] = new;

    // Force later recompute of settings-driven data structures
    // (happens when a search starts and client connections are prepared)
    switch (offset)
    {
    case PZ_XSLT:
        if (sdb->map)
        {
            sdb->map = 0;
        }
        break;
    }
}

void destroy_session(struct session *se)
{
    struct session_database *sdb;

    yaz_log(YLOG_DEBUG, "%p Pazpar2 session destroy", se);
    session_remove_clients(se);

    for (sdb = se->databases; sdb; sdb = sdb->next)
        session_database_destroy(sdb);
    normalize_cache_destroy(se->normalize_cache);
    relevance_destroy(&se->relevance);
    reclist_destroy(se->reclist);
    nmem_destroy(se->nmem);
    service_destroy(se->service);
    yaz_mutex_destroy(&se->session_mutex);
    wrbuf_destroy(se->wrbuf);
}

struct session *new_session(NMEM nmem, struct conf_service *service,
                            const char *name)
{
    int i;
    struct session *session = nmem_malloc(nmem, sizeof(*session));

    yaz_log(YLOG_DEBUG, "%p New Pazpar2 session", session);

    session->service = service;
    session->relevance = 0;
    session->total_hits = 0;
    session->total_records = 0;
    session->number_of_warnings_unknown_elements = 0;
    session->number_of_warnings_unknown_metadata = 0;
    session->num_termlists = 0;
    session->reclist = 0;
    session->clients = 0;
    session->session_nmem = nmem;
    session->nmem = nmem_create();
    session->wrbuf = wrbuf_alloc();
    session->databases = 0;
    for (i = 0; i <= SESSION_WATCH_MAX; i++)
    {
        session->watchlist[i].data = 0;
        session->watchlist[i].fun = 0;
    }
    session->normalize_cache = normalize_cache_create();
    session->session_mutex = 0;
    pazpar2_mutex_create(&session->session_mutex, name);

    return session;
}

struct hitsbytarget *hitsbytarget(struct session *se, int *count, NMEM nmem)
{
    struct hitsbytarget *res = 0;
    struct client_list *l;
    size_t sz = 0;

    session_enter(se);
    for (l = se->clients; l; l = l->next)
        sz++;

    res = nmem_malloc(nmem, sizeof(*res) * sz);
    *count = 0;
    for (l = se->clients; l; l = l->next)
    {
        struct client *cl = l->client;
        WRBUF w = wrbuf_alloc();
        const char *name = session_setting_oneval(client_get_database(cl),
                                                  PZ_NAME);

        res[*count].id = client_get_database(cl)->database->url;
        res[*count].name = *name ? name : "Unknown";
        res[*count].hits = client_get_hits(cl);
        res[*count].records = client_get_num_records(cl);
        res[*count].diagnostic = client_get_diagnostic(cl);
        res[*count].state = client_get_state_str(cl);
        res[*count].connected  = client_get_connection(cl) ? 1 : 0;
        session_settings_dump(se, client_get_database(cl), w);
        res[*count].settings_xml = w;
        (*count)++;
    }
    session_leave(se);
    return res;
}

struct termlist_score **termlist(struct session *se, const char *name, int *num)
{
    int i;
    struct termlist_score **tl = 0;

    session_enter(se);
    for (i = 0; i < se->num_termlists; i++)
        if (!strcmp((const char *) se->termlists[i].name, name))
        {
            tl = termlist_highscore(se->termlists[i].termlist, num);
            break;
        }
    session_leave(se);
    return tl;
}

#ifdef MISSING_HEADERS
void report_nmem_stats(void)
{
    size_t in_use, is_free;

    nmem_get_memory_in_use(&in_use);
    nmem_get_memory_free(&is_free);

    yaz_log(YLOG_LOG, "nmem stat: use=%ld free=%ld", 
            (long) in_use, (long) is_free);
}
#endif

struct record_cluster *show_single_start(struct session *se, const char *id,
                                         struct record_cluster **prev_r,
                                         struct record_cluster **next_r)
{
    struct record_cluster *r;

    session_enter(se);
    reclist_enter(se->reclist);
    *prev_r = 0;
    *next_r = 0;
    while ((r = reclist_read_record(se->reclist)))
    {
        if (!strcmp(r->recid, id))
        {
            *next_r = reclist_read_record(se->reclist);
            break;
        }
        *prev_r = r;
    }
    reclist_leave(se->reclist);
    if (!r)
        session_leave(se);
    return r;
}

void show_single_stop(struct session *se, struct record_cluster *rec)
{
    session_leave(se);
}

struct record_cluster **show_range_start(struct session *se,
                                         struct reclist_sortparms *sp, 
                                         int start, int *num, int *total, Odr_int *sumhits)
{
    struct record_cluster **recs;
    struct reclist_sortparms *spp;
    int i;
#if USE_TIMING    
    yaz_timing_t t = yaz_timing_create();
#endif
    session_enter(se);
    recs = nmem_malloc(se->nmem, *num * sizeof(struct record_cluster *));
    if (!se->relevance)
    {
        *num = 0;
        *total = 0;
        *sumhits = 0;
        recs = 0;
    }
    else
    {
        for (spp = sp; spp; spp = spp->next)
            if (spp->type == Metadata_sortkey_relevance)
            {
                relevance_prepare_read(se->relevance, se->reclist);
                break;
            }
        reclist_sort(se->reclist, sp);
        
        reclist_enter(se->reclist);
        *total = reclist_get_num_records(se->reclist);
        *sumhits = se->total_hits;
        
        for (i = 0; i < start; i++)
            if (!reclist_read_record(se->reclist))
            {
                *num = 0;
                recs = 0;
                break;
            }
        
        for (i = 0; i < *num; i++)
        {
            struct record_cluster *r = reclist_read_record(se->reclist);
            if (!r)
            {
                *num = i;
                break;
            }
            recs[i] = r;
        }
        reclist_leave(se->reclist);
    }
#if USE_TIMING
    yaz_timing_stop(t);
    yaz_log(YLOG_LOG, "show %6.5f %3.2f %3.2f", 
            yaz_timing_get_real(t), yaz_timing_get_user(t),
            yaz_timing_get_sys(t));
    yaz_timing_destroy(&t);
#endif
    return recs;
}

void show_range_stop(struct session *se, struct record_cluster **recs)
{
    session_leave(se);
}

void statistics(struct session *se, struct statistics *stat)
{
    struct client_list *l;
    int count = 0;

    memset(stat, 0, sizeof(*stat));
    for (l = se->clients; l; l = l->next)
    {
        struct client *cl = l->client;
        if (!client_get_connection(cl))
            stat->num_no_connection++;
        switch (client_get_state(cl))
        {
        case Client_Connecting: stat->num_connecting++; break;
        case Client_Working: stat->num_working++; break;
        case Client_Idle: stat->num_idle++; break;
        case Client_Failed: stat->num_failed++; break;
        case Client_Error: stat->num_error++; break;
        default: break;
        }
        count++;
    }
    stat->num_hits = se->total_hits;
    stat->num_records = se->total_records;

    stat->num_clients = count;
}

static struct record_metadata *record_metadata_init(
    NMEM nmem, const char *value, enum conf_metadata_type type,
    struct _xmlAttr *attr)
{
    struct record_metadata *rec_md = record_metadata_create(nmem);
    struct record_metadata_attr **attrp = &rec_md->attributes;
    
    for (; attr; attr = attr->next)
    {
        if (attr->children && attr->children->content)
        {
            if (strcmp((const char *) attr->name, "type"))
            {  /* skip the "type" attribute.. Its value is already part of
                  the element in output (md-%s) and so repeating it here
                  is redundant */
                *attrp = nmem_malloc(nmem, sizeof(**attrp));
                (*attrp)->name =
                    nmem_strdup(nmem, (const char *) attr->name);
                (*attrp)->value =
                    nmem_strdup(nmem, (const char *) attr->children->content);
                attrp = &(*attrp)->next;
            }
        }
    }
    *attrp = 0;

    if (type == Metadata_type_generic)
    {
        char *p = nmem_strdup(nmem, value);

        p = normalize7bit_generic(p, " ,/.:([");
        
        rec_md->data.text.disp = p;
        rec_md->data.text.sort = 0;
    }
    else if (type == Metadata_type_year || type == Metadata_type_date)
    {
        int first, last;
        int longdate = 0;

        if (type == Metadata_type_date)
            longdate = 1;
        if (extract7bit_dates((char *) value, &first, &last, longdate) < 0)
            return 0;

        rec_md->data.number.min = first;
        rec_md->data.number.max = last;
    }
    else
        return 0;
    return rec_md;
}

static int get_mergekey_from_doc(xmlDoc *doc, xmlNode *root, const char *name,
                                 struct conf_service *service, WRBUF norm_wr)
{
    xmlNode *n;
    int no_found = 0;
    for (n = root->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp((const char *) n->name, "metadata"))
        {
            xmlChar *type = xmlGetProp(n, (xmlChar *) "type");
            if (type == NULL) {
                yaz_log(YLOG_FATAL, "Missing type attribute on metadata element. Skipping!");
            }
            else if (!strcmp(name, (const char *) type))
            {
                xmlChar *value = xmlNodeListGetString(doc, n->children, 1);
                if (value)
                {
                    const char *norm_str;
                    pp2_relevance_token_t prt =
                        pp2_relevance_tokenize(service->mergekey_pct);
                    
                    pp2_relevance_first(prt, (const char *) value, 0);
                    if (wrbuf_len(norm_wr) > 0)
                        wrbuf_puts(norm_wr, " ");
                    wrbuf_puts(norm_wr, name);
                    while ((norm_str =
                            pp2_relevance_token_next(prt)))
                    {
                        if (*norm_str)
                        {
                            wrbuf_puts(norm_wr, " ");
                            wrbuf_puts(norm_wr, norm_str);
                        }
                    }
                    xmlFree(value);
                    pp2_relevance_token_destroy(prt);
                    no_found++;
                }
            }
            xmlFree(type);
        }
    }
    return no_found;
}

static const char *get_mergekey(xmlDoc *doc, struct client *cl, int record_no,
                                struct conf_service *service, NMEM nmem)
{
    char *mergekey_norm = 0;
    xmlNode *root = xmlDocGetRootElement(doc);
    WRBUF norm_wr = wrbuf_alloc();

    /* consider mergekey from XSL first */
    xmlChar *mergekey = xmlGetProp(root, (xmlChar *) "mergekey");
    if (mergekey)
    {
        const char *norm_str;
        pp2_relevance_token_t prt =
            pp2_relevance_tokenize(service->mergekey_pct);

        pp2_relevance_first(prt, (const char *) mergekey, 0);
        while ((norm_str = pp2_relevance_token_next(prt)))
        {
            if (*norm_str)
            {
                if (wrbuf_len(norm_wr))
                    wrbuf_puts(norm_wr, " ");
                wrbuf_puts(norm_wr, norm_str);
            }
        }
        pp2_relevance_token_destroy(prt);
        xmlFree(mergekey);
    }
    else
    {
        /* no mergekey defined in XSL. Look for mergekey metadata instead */
        int field_id;
        for (field_id = 0; field_id < service->num_metadata; field_id++)
        {
            struct conf_metadata *ser_md = &service->metadata[field_id];
            if (ser_md->mergekey != Metadata_mergekey_no)
            {
                int r = get_mergekey_from_doc(doc, root, ser_md->name,
                                              service, norm_wr);
                if (r == 0 && ser_md->mergekey == Metadata_mergekey_required)
                {
                    /* no mergekey on this one and it is required.. 
                       Generate unique key instead */
                    wrbuf_rewind(norm_wr);
                    break;
                }
            }
        }
    }

    /* generate unique key if none is not generated already or is empty */
    if (wrbuf_len(norm_wr) == 0)
    {
        wrbuf_printf(norm_wr, "%s-%d",
                     client_get_database(cl)->database->url, record_no);
    }
    if (wrbuf_len(norm_wr) > 0)
        mergekey_norm = nmem_strdup(nmem, wrbuf_cstr(norm_wr));
    wrbuf_destroy(norm_wr);
    return mergekey_norm;
}

/** \brief see if metadata for pz:recordfilter exists 
    \param root xml root element of normalized record
    \param sdb session database for client
    \retval 0 if there is no metadata for pz:recordfilter
    \retval 1 if there is metadata for pz:recordfilter

    If there is no pz:recordfilter defined, this function returns 1
    as well.
*/
    
static int check_record_filter(xmlNode *root, struct session_database *sdb)
{
    int match = 0;
    xmlNode *n;
    const char *s;
    s = session_setting_oneval(sdb, PZ_RECORDFILTER);

    if (!s || !*s)
        return 1;

    for (n = root->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp((const char *) n->name, "metadata"))
        {
            xmlChar *type = xmlGetProp(n, (xmlChar *) "type");
            if (type)
            {
                size_t len;
		int substring;
                const char *eq;

                if ((eq = strchr(s, '=')))
		    substring = 0;
		else if ((eq = strchr(s, '~')))
		    substring = 1;
		if (eq)
		    len = eq - s;
                else
                    len = strlen(s);
                if (len == strlen((const char *)type) &&
                    !memcmp((const char *) type, s, len))
                {
                    xmlChar *value = xmlNodeGetContent(n);
                    if (value && *value)
                    {
                        if (!eq ||
			    (substring && strstr((const char *) value, eq+1)) ||
			    (!substring && !strcmp((const char *) value, eq + 1)))
                            match = 1;
                    }
                    xmlFree(value);
                }
                xmlFree(type);
            }
        }
    }
    return match;
}


static int ingest_to_cluster(struct client *cl,
                             xmlDoc *xdoc,
                             xmlNode *root,
                             int record_no,
                             const char *mergekey_norm);

/** \brief ingest XML record
    \param cl client holds the result set for record
    \param rec record buffer (0 terminated)
    \param record_no record position (1, 2, ..)
    \param nmem working NMEM
    \retval 0 OK
    \retval -1 failure
*/
int ingest_record(struct client *cl, const char *rec,
                  int record_no, NMEM nmem)
{
    struct session *se = client_get_session(cl);
    int ret = 0;
    struct session_database *sdb = client_get_database(cl);
    struct conf_service *service = se->service;
    xmlDoc *xdoc = normalize_record(sdb, service, rec, nmem);
    xmlNode *root;
    const char *mergekey_norm;
    
    if (!xdoc)
        return -1;
    
    root = xmlDocGetRootElement(xdoc);
    
    if (!check_record_filter(root, sdb))
    {
        yaz_log(YLOG_WARN, "Filtered out record no %d from %s", record_no,
                sdb->database->url);
        xmlFreeDoc(xdoc);
        return -1;
    }
    
    mergekey_norm = get_mergekey(xdoc, cl, record_no, service, nmem);
    if (!mergekey_norm)
    {
        yaz_log(YLOG_WARN, "Got no mergekey");
        xmlFreeDoc(xdoc);
        return -1;
    }
    session_enter(se);
    if (client_get_session(cl) == se)
        ret = ingest_to_cluster(cl, xdoc, root, record_no, mergekey_norm);
    session_leave(se);
    
    xmlFreeDoc(xdoc);
    return ret;
}

static int ingest_to_cluster(struct client *cl,
                             xmlDoc *xdoc,
                             xmlNode *root,
                             int record_no,
                             const char *mergekey_norm)
{
    xmlNode *n;
    xmlChar *type = 0;
    xmlChar *value = 0;
    struct session_database *sdb = client_get_database(cl);
    struct session *se = client_get_session(cl);
    struct conf_service *service = se->service;
    struct record *record = record_create(se->nmem, 
                                          service->num_metadata,
                                          service->num_sortkeys, cl,
                                          record_no);
    struct record_cluster *cluster = reclist_insert(se->reclist,
                                                    service, 
                                                    record,
                                                    mergekey_norm,
                                                    &se->total_merged);
    if (!cluster)
        return -1;
    if (global_parameters.dump_records)
        yaz_log(YLOG_LOG, "Cluster id %s from %s (#%d)", cluster->recid,
                sdb->database->url, record_no);
    relevance_newrec(se->relevance, cluster);
    
    // now parsing XML record and adding data to cluster or record metadata
    for (n = root->children; n; n = n->next)
    {
        pp2_relevance_token_t prt;
        if (type)
            xmlFree(type);
        if (value)
            xmlFree(value);
        type = value = 0;
        
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp((const char *) n->name, "metadata"))
        {
            struct conf_metadata *ser_md = 0;
            struct conf_sortkey *ser_sk = 0;
            struct record_metadata **wheretoput = 0;
            struct record_metadata *rec_md = 0;
            int md_field_id = -1;
            int sk_field_id = -1;
            
            type = xmlGetProp(n, (xmlChar *) "type");
            value = xmlNodeListGetString(xdoc, n->children, 1);
            
            if (!type || !value || !*value)
                continue;
            
            md_field_id 
                = conf_service_metadata_field_id(service, (const char *) type);
            if (md_field_id < 0)
            {
                if (se->number_of_warnings_unknown_metadata == 0)
                {
                    yaz_log(YLOG_WARN, 
                            "Ignoring unknown metadata element: %s", type);
                }
                se->number_of_warnings_unknown_metadata++;
                continue;
            }
            
            ser_md = &service->metadata[md_field_id];
            
            if (ser_md->sortkey_offset >= 0){
                sk_field_id = ser_md->sortkey_offset;
                ser_sk = &service->sortkeys[sk_field_id];
            }

            // non-merged metadata
            rec_md = record_metadata_init(se->nmem, (const char *) value,
                                          ser_md->type, n->properties);
            if (!rec_md)
            {
                yaz_log(YLOG_WARN, "bad metadata data '%s' for element '%s'",
                        value, type);
                continue;
            }
            wheretoput = &record->metadata[md_field_id];
            while (*wheretoput)
                wheretoput = &(*wheretoput)->next;
            *wheretoput = rec_md;

            // merged metadata
            rec_md = record_metadata_init(se->nmem, (const char *) value,
                                          ser_md->type, 0);
            wheretoput = &cluster->metadata[md_field_id];

            // and polulate with data:
            // assign cluster or record based on merge action
            if (ser_md->merge == Metadata_merge_unique)
            {
                struct record_metadata *mnode;
                for (mnode = *wheretoput; mnode; mnode = mnode->next)
                    if (!strcmp((const char *) mnode->data.text.disp, 
                                rec_md->data.text.disp))
                        break;
                if (!mnode)
                {
                    rec_md->next = *wheretoput;
                    *wheretoput = rec_md;
                }
            }
            else if (ser_md->merge == Metadata_merge_longest)
            {
                if (!*wheretoput 
                    || strlen(rec_md->data.text.disp) 
                    > strlen((*wheretoput)->data.text.disp))
                {
                    *wheretoput = rec_md;
                    if (ser_sk)
                    {
                        const char *sort_str = 0;
                        int skip_article = 
                            ser_sk->type == Metadata_sortkey_skiparticle;

                        if (!cluster->sortkeys[sk_field_id])
                            cluster->sortkeys[sk_field_id] = 
                                nmem_malloc(se->nmem, 
                                            sizeof(union data_types));
                         
                        prt = pp2_relevance_tokenize(service->sort_pct);

                        pp2_relevance_first(prt, rec_md->data.text.disp,
                                            skip_article);

                        pp2_relevance_token_next(prt);
                         
                        sort_str = pp2_get_sort(prt);
                         
                        cluster->sortkeys[sk_field_id]->text.disp = 
                            rec_md->data.text.disp;
                        if (!sort_str)
                        {
                            sort_str = rec_md->data.text.disp;
                            yaz_log(YLOG_WARN, 
                                    "Could not make sortkey. Bug #1858");
                        }
                        cluster->sortkeys[sk_field_id]->text.sort = 
                            nmem_strdup(se->nmem, sort_str);
#if 0
                        yaz_log(YLOG_LOG, "text disp=%s",
                                cluster->sortkeys[sk_field_id]->text.disp);
                        yaz_log(YLOG_LOG, "text sort=%s",
                                cluster->sortkeys[sk_field_id]->text.sort);
#endif
                        pp2_relevance_token_destroy(prt);
                    }
                }
            }
            else if (ser_md->merge == Metadata_merge_all)
            {
                rec_md->next = *wheretoput;
                *wheretoput = rec_md;
            }
            else if (ser_md->merge == Metadata_merge_range)
            {
                if (!*wheretoput)
                {
                    *wheretoput = rec_md;
                    if (ser_sk)
                        cluster->sortkeys[sk_field_id] 
                            = &rec_md->data;
                }
                else
                {
                    int this_min = rec_md->data.number.min;
                    int this_max = rec_md->data.number.max;
                    if (this_min < (*wheretoput)->data.number.min)
                        (*wheretoput)->data.number.min = this_min;
                    if (this_max > (*wheretoput)->data.number.max)
                        (*wheretoput)->data.number.max = this_max;
                }
            }


            // ranking of _all_ fields enabled ... 
            if (ser_md->rank)
                relevance_countwords(se->relevance, cluster, 
                                     (char *) value, ser_md->rank,
                                     ser_md->name);

            // construct facets ... unless the client already has reported them
            if (ser_md->termlist && !client_has_facet(cl, (char *) type))
            {

                if (ser_md->type == Metadata_type_year)
                {
                    char year[64];
                    sprintf(year, "%d", rec_md->data.number.max);
                    add_facet(se, (char *) type, year, 1);
                    if (rec_md->data.number.max != rec_md->data.number.min)
                    {
                        sprintf(year, "%d", rec_md->data.number.min);
                        add_facet(se, (char *) type, year, 1);
                    }
                }
                else
                    add_facet(se, (char *) type, (char *) value, 1);
            }

            // cleaning up
            xmlFree(type);
            xmlFree(value);
            type = value = 0;
        }
        else
        {
            if (se->number_of_warnings_unknown_elements == 0)
                yaz_log(YLOG_WARN,
                        "Unexpected element in internal record: %s", n->name);
            se->number_of_warnings_unknown_elements++;
        }
    }
    if (type)
        xmlFree(type);
    if (value)
        xmlFree(value);

    relevance_donerecord(se->relevance, cluster);
    se->total_records++;

    return 0;
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

