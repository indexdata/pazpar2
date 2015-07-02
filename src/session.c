/* This file is part of Pazpar2.
   Copyright (C) Index Data

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
#include <math.h>

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
#include <yaz/xml_get.h>

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

#include <libxml/tree.h>

#define MAX_CHUNK 15

#define MAX(a,b) ((a)>(b)?(a):(b))

// Note: Some things in this structure will eventually move to configuration
struct parameters global_parameters =
{
    0,   // dump_records
    0,   // debug_mode
    0,   // predictable sessions
};

struct client_list {
    struct client *client;
    struct client_list *next;
};

/* session counting (1) , disable client counting (0) */
static YAZ_MUTEX g_session_mutex = 0;
static int no_sessions = 0;

static int session_use(int delta)
{
    int sessions;
    if (!g_session_mutex)
        yaz_mutex_create(&g_session_mutex);
    yaz_mutex_enter(g_session_mutex);
    no_sessions += delta;
    sessions = no_sessions;
    yaz_mutex_leave(g_session_mutex);
    yaz_log(YLOG_DEBUG, "%s sessions=%d", delta == 0 ? "" :
            (delta > 0 ? "INC" : "DEC"), no_sessions);
    return sessions;
}

int sessions_count(void)
{
    return session_use(0);
}

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

static void session_enter(struct session *s, const char *caller)
{
    if (caller)
        session_log(s, YLOG_DEBUG, "Session lock by %s", caller);
    yaz_mutex_enter(s->session_mutex);
}

static void session_leave(struct session *s, const char *caller)
{
    yaz_mutex_leave(s->session_mutex);
    if (caller)
        session_log(s, YLOG_DEBUG, "Session unlock by %s", caller);
}

static int run_icu(struct session *s, const char *icu_chain_id,
                   const char *value,
                   WRBUF norm_wr, WRBUF disp_wr)
{
    const char *facet_component;
    struct conf_service *service = s->service;
    pp2_charset_token_t prt =
        pp2_charset_token_create(service->charsets, icu_chain_id);
    if (!prt)
    {
        session_log(s, YLOG_FATAL,
                    "Unknown ICU chain '%s'", icu_chain_id);
        return 0;
    }
    pp2_charset_token_first(prt, value, 0);
    while ((facet_component = pp2_charset_token_next(prt)))
    {
        const char *display_component;
        if (*facet_component)
        {
            if (wrbuf_len(norm_wr))
                wrbuf_puts(norm_wr, " ");
            wrbuf_puts(norm_wr, facet_component);
        }
        display_component = pp2_get_display(prt);
        if (display_component)
        {
            if (wrbuf_len(disp_wr))
                wrbuf_puts(disp_wr, " ");
            wrbuf_puts(disp_wr, display_component);
        }
    }
    pp2_charset_token_destroy(prt);
    return 1;
}

static void session_normalize_facet(struct session *s,
                                    const char *type, const char *value,
                                    WRBUF display_wrbuf, WRBUF facet_wrbuf)
{
    struct conf_service *service = s->service;
    int i;
    const char *icu_chain_id = 0;

    for (i = 0; i < service->num_metadata; i++)
        if (!strcmp((service->metadata + i)->name, type))
            icu_chain_id = (service->metadata + i)->facetrule;
    if (!icu_chain_id)
        icu_chain_id = "facet";

    run_icu(s, icu_chain_id, value, facet_wrbuf, display_wrbuf);
}

struct facet_id {
    char *client_id;
    char *type;
    char *id;
    char *term;
    struct facet_id *next;
};

static void session_add_id_facet(struct session *s, struct client *cl,
                                 const char *type,
                                 const char *id,
                                 size_t id_len,
                                 const char *term)
{
    struct facet_id *t = nmem_malloc(s->session_nmem, sizeof(*t));

    t->client_id = nmem_strdup(s->session_nmem, client_get_id(cl));
    t->type = nmem_strdup(s->session_nmem, type);
    t->id = nmem_strdupn(s->session_nmem, id, id_len);
    t->term = nmem_strdup(s->session_nmem, term);
    t->next = s->facet_id_list;
    s->facet_id_list = t;
}


const char *session_lookup_id_facet(struct session *s, struct client *cl,
                                    const char *type,
                                    const char *term)
{
    struct facet_id *t = s->facet_id_list;
    for (; t; t = t->next)
        if (!strcmp(client_get_id(cl), t->client_id) &&
            !strcmp(t->type, type) && !strcmp(t->term, term))
        {
            return t->id;
        }
    return 0;
}

void add_facet(struct session *s, const char *type, const char *value, int count, struct client *cl)
{
    WRBUF facet_wrbuf = wrbuf_alloc();
    WRBUF display_wrbuf = wrbuf_alloc();
    const char *id = 0;
    size_t id_len = 0;

    /* inspect pz:facetmap:split:name ?? */
    if (!strncmp(type, "split:", 6))
    {
        const char *cp = strchr(value, ':');
        if (cp)
        {
            id = value;
            id_len = cp - value;
            value = cp + 1;
        }
        type += 6;
    }

    session_normalize_facet(s, type, value, display_wrbuf, facet_wrbuf);
    if (wrbuf_len(facet_wrbuf))
    {
        struct named_termlist **tp = &s->termlists;
        for (; (*tp); tp = &(*tp)->next)
            if (!strcmp((*tp)->name, type))
                break;
        if (!*tp)
        {
            *tp = nmem_malloc(s->nmem, sizeof(**tp));
            (*tp)->name = nmem_strdup(s->nmem, type);
            (*tp)->termlist = termlist_create(s->nmem);
            (*tp)->next = 0;
        }
        termlist_insert((*tp)->termlist, wrbuf_cstr(display_wrbuf),
                        wrbuf_cstr(facet_wrbuf), id, id_len, count);
        if (id)
            session_add_id_facet(s, cl, type, id, id_len,
                                 wrbuf_cstr(display_wrbuf));
    }
    wrbuf_destroy(facet_wrbuf);
    wrbuf_destroy(display_wrbuf);
}

static xmlDoc *record_to_xml(struct session *se,
                             struct session_database *sdb, const char *rec)
{
    struct database *db = sdb->database;
    xmlDoc *rdoc = 0;

    rdoc = xmlParseMemory(rec, strlen(rec));

    if (!rdoc)
    {
        session_log(se, YLOG_WARN, "Non-wellformed XML");
        return 0;
    }

    if (global_parameters.dump_records)
    {
        session_log(se, YLOG_LOG, "Un-normalized record from %s", db->id);
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
                                   xmlNode *root,
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
                xmlNode *n = xmlNewTextChild(root, 0, (xmlChar *) "metadata",
                                             (xmlChar *) val);
                xmlSetProp(n, (xmlChar *) "type", (xmlChar *) md->name);
            }
        }
    }
}

static xmlDoc *normalize_record(struct session *se,
                                struct session_database *sdb,
                                struct conf_service *service,
                                const char *rec, NMEM nmem)
{
    xmlDoc *rdoc = record_to_xml(se, sdb, rec);

    if (rdoc)
    {
        char *parms[MAX_XSLT_ARGS*2+1];

        insert_settings_parameters(sdb, service, parms, nmem);

        if (normalize_record_transform(sdb->map, &rdoc, (const char **)parms))
        {
            session_log(se, YLOG_WARN, "Normalize failed");
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
    if (sdb->settings && !sdb->map)
    {
        const char *s;

        if (sdb->settings[PZ_XSLT] &&
            (s = session_setting_oneval(sdb, PZ_XSLT)))
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
                    session_log(se, YLOG_WARN,
                                "No pz:requestsyntax for auto stylesheet");
                }
            }
            sdb->map = normalize_cache_get(se->normalize_cache,
                                           se->service, s);
            if (!sdb->map)
                return -1;
        }
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
    session_enter(s, "session_set_watch");
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
    session_leave(s, "session_set_watch");
    return ret;
}

void session_alert_watch(struct session *s, int what)
{
    assert(s);
    session_enter(s, "session_alert_watch");
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

        session_leave(s, "session_alert_watch");
        session_log(s, YLOG_DEBUG,
                    "Alert Watch: %d calling function: %p", what, fun);
        fun(data);
    }
    else
        session_leave(s,"session_alert_watch");
}

//callback for grep_databases
static void select_targets_callback(struct session *se,
                                    struct session_database *db)
{
    struct client *cl;
    struct client_list *l;

    for (l = se->clients_cached; l; l = l->next)
        if (client_get_database(l->client) == db)
            break;

    if (l)
        cl = l->client;
    else
    {
        cl = client_create(db->database->id);
        client_set_database(cl, db);

        l = xmalloc(sizeof(*l));
        l->client = cl;
        l->next = se->clients_cached;
        se->clients_cached = l;
    }
    /* set session always. If may be 0 if client is not active */
    client_set_session(cl, se);

    l = xmalloc(sizeof(*l));
    l->client = cl;
    l->next = se->clients_active;
    se->clients_active = l;
}

static void session_reset_active_clients(struct session *se,
                                         struct client_list *new_list)
{
    struct client_list *l;

    session_enter(se, "session_reset_active_clients");
    l = se->clients_active;
    se->clients_active = new_list;
    session_leave(se, "session_reset_active_clients");

    while (l)
    {
        struct client_list *l_next = l->next;

        client_lock(l->client);
        client_set_session(l->client, 0); /* mark client inactive */
        client_unlock(l->client);

        xfree(l);
        l = l_next;
    }
}

static void session_remove_cached_clients(struct session *se)
{
    struct client_list *l;

    session_reset_active_clients(se, 0);

    session_enter(se, "session_remove_cached_clients");
    l = se->clients_cached;
    se->clients_cached = 0;
    session_leave(se, "session_remove_cached_clients");

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

    for (l = s->clients_active; l; l = l->next)
        if (client_is_active(l->client))
            res++;

    return res;
}

int session_is_preferred_clients_ready(struct session *s)
{
    struct client_list *l;
    int res = 0;

    for (l = s->clients_active; l; l = l->next)
        if (client_is_active_preferred(l->client))
            res++;
    session_log(s, YLOG_DEBUG, "Has %d active preferred clients.", res);
    return res == 0;
}

static void session_clear_set(struct session *se, struct reclist_sortparms *sp)
{
    reclist_destroy(se->reclist);
    if (nmem_total(se->nmem))
        session_log(se, YLOG_DEBUG, "NMEN operation usage %zd",
                    nmem_total(se->nmem));
    nmem_reset(se->nmem);
    se->total_records = se->total_merged = 0;
    se->termlists = 0;
    relevance_clear(se->relevance);

    /* reset list of sorted results and clear to relevance search */
    se->sorted_results = nmem_malloc(se->nmem, sizeof(*se->sorted_results));
    se->sorted_results->name = nmem_strdup(se->nmem, sp->name);
    se->sorted_results->increasing = sp->increasing;
    se->sorted_results->type = sp->type;
    se->sorted_results->next = 0;

    session_log(se, YLOG_DEBUG, "clear_set session_sort: field=%s increasing=%d type=%d configured",
            sp->name, sp->increasing, sp->type);

    se->reclist = reclist_create(se->nmem);
}

void session_sort(struct session *se, struct reclist_sortparms *sp,
                  const char *mergekey, const char *rank)
{
    struct client_list *l;
    const char *field = sp->name;
    int increasing = sp->increasing;
    int type  = sp->type;
    int clients_research = 0;

    session_enter(se, "session_sort");
    session_log(se, YLOG_DEBUG, "session_sort field=%s increasing=%d type=%d",
                field, increasing, type);

    if (rank && (!se->rank || strcmp(se->rank, rank)))
    {
        /* new rank must research/reingest anyway */
        assert(rank);
        xfree(se->rank);
        se->rank = *rank ? xstrdup(rank) : 0;
        clients_research = 1;
        session_log(se, YLOG_DEBUG, "session_sort: new rank = %s",
                    rank);
    }
    if (mergekey && (!se->mergekey || strcmp(se->mergekey, mergekey)))
    {
        /* new mergekey must research/reingest anyway */
        assert(mergekey);
        xfree(se->mergekey);
        se->mergekey = *mergekey ? xstrdup(mergekey) : 0;
        clients_research = 1;
        session_log(se, YLOG_DEBUG, "session_sort: new mergekey = %s",
                    mergekey);
    }
    if (clients_research == 0)
    {
        struct reclist_sortparms *sr;
        for (sr = se->sorted_results; sr; sr = sr->next)
            if (!reclist_sortparms_cmp(sr, sp))
                break;
        if (sr)
        {
            session_log(se, YLOG_DEBUG, "session_sort: field=%s increasing=%d type=%d already fetched",
                        field, increasing, type);
            session_leave(se, "session_sort");
            return;
        }
    }
    session_log(se, YLOG_DEBUG, "session_sort: field=%s increasing=%d type=%d must fetch",
                field, increasing, type);

    // We need to reset reclist on every sort that changes the records, not just for position
    // So if just one client requires new searching, we need to clear set.
    // Ask each of the client if sorting requires re-search due to native sort
    // If it does it will require us to
    for (l = se->clients_active; l; l = l->next)
    {
        struct client *cl = l->client;
        // Assume no re-search is required.
        client_parse_init(cl, 1);
        clients_research += client_parse_sort(cl, sp);
    }
    if (!clients_research || se->clients_starting)
    {
        // A new sorting based on same record set
        struct reclist_sortparms *sr = nmem_malloc(se->nmem, sizeof(*sr));
        sr->name = nmem_strdup(se->nmem, field);
        sr->increasing = increasing;
        sr->type = type;
        sr->next = se->sorted_results;
        se->sorted_results = sr;
        session_log(se, YLOG_DEBUG, "session_sort: no research/ingesting done");
        session_leave(se, "session_sort");
    }
    else
    {
        se->clients_starting = 1;
        session_log(se, YLOG_DEBUG,
                    "session_sort: reset results due to %d clients researching",
                    clients_research);
        session_clear_set(se, sp);
        session_log(se, YLOG_DEBUG, "Re- search/ingesting for clients due to change in sort order");

        session_leave(se, "session_sort");
        for (l = se->clients_active; l; l = l->next)
        {
            struct client *cl = l->client;
            if (client_get_state(cl) == Client_Connecting ||
                client_get_state(cl) == Client_Idle ||
                client_get_state(cl) == Client_Working) {
                client_start_search(cl);
            }
            else
            {
                session_log(se, YLOG_DEBUG,
                            "session_sort: %s: No re-start/ingest in show. "
                            "Wrong client state: %d",
                            client_get_id(cl), client_get_state(cl));
            }
        }
        session_enter(se, "session_sort");
        se->clients_starting = 0;
        session_leave(se, "session_sort");
    }
}

void session_stop(struct session *se)
{
    struct client_list *l;
    session_enter(se, "session_stop1");
    if (se->clients_starting)
    {
        session_leave(se, "session_stop1");
        return;
    }
    se->clients_starting = 1;
    session_leave(se, "session_stop1");

    session_alert_watch(se, SESSION_WATCH_SHOW);
    session_alert_watch(se, SESSION_WATCH_BYTARGET);
    session_alert_watch(se, SESSION_WATCH_TERMLIST);
    session_alert_watch(se, SESSION_WATCH_SHOW_PREF);

    for (l = se->clients_active; l; l = l->next)
    {
        struct client *cl = l->client;
        client_stop(cl);
    }
    session_enter(se, "session_stop2");
    se->clients_starting = 0;
    session_leave(se, "session_stop2");
}

enum pazpar2_error_code session_search(struct session *se,
                                       const char *query,
                                       const char *startrecs,
                                       const char *maxrecs,
                                       const char *filter,
                                       const char *limit,
                                       const char **addinfo,
                                       const char **addinfo2,
                                       struct reclist_sortparms *sp,
                                       const char *mergekey,
                                       const char *rank)
{
    int live_channels = 0;
    int no_working = 0;
    int no_failed_query = 0;
    int no_failed_limit = 0;
    struct client_list *l;

    session_log(se, YLOG_DEBUG, "Search");

    *addinfo = 0;

    session_enter(se, "session_search0");
    if (se->clients_starting)
    {
        session_leave(se, "session_search0");
        return PAZPAR2_NO_ERROR;
    }
    se->clients_starting = 1;
    session_leave(se, "session_search0");

    if (se->settings_modified) {
        session_remove_cached_clients(se);
    }
    else
        session_reset_active_clients(se, 0);

    session_enter(se, "session_search");
    se->settings_modified = 0;

    if (mergekey)
    {
        xfree(se->mergekey);
        se->mergekey = *mergekey ? xstrdup(mergekey) : 0;
    }
    if (rank)
    {
        xfree(se->rank);
        se->rank = *rank ? xstrdup(rank) : 0;
    }

    session_clear_set(se, sp);
    relevance_destroy(&se->relevance);

    live_channels = select_targets(se, filter);
    if (!live_channels)
    {
        session_leave(se, "session_search");
        se->clients_starting = 0;
        return PAZPAR2_NO_TARGETS;
    }

    facet_limits_destroy(se->facet_limits);
    se->facet_limits = facet_limits_create(limit);
    if (!se->facet_limits)
    {
        *addinfo = "limit";
        session_leave(se, "session_search");
        se->clients_starting = 0;
        return PAZPAR2_MALFORMED_PARAMETER_VALUE;
    }

    session_leave(se, "session_search");

    session_alert_watch(se, SESSION_WATCH_SHOW);
    session_alert_watch(se, SESSION_WATCH_BYTARGET);
    session_alert_watch(se, SESSION_WATCH_TERMLIST);
    session_alert_watch(se, SESSION_WATCH_SHOW_PREF);

    for (l = se->clients_active; l; l = l->next)
    {
        int parse_ret;
        struct client *cl = l->client;
        client_parse_init(cl, 1);
        if (prepare_map(se, client_get_database(cl)) < 0)
            continue;

        parse_ret = client_parse_query(cl, query, se->facet_limits, addinfo2);
        if (parse_ret == -1)
            no_failed_query++;
        else if (parse_ret == -2)
            no_failed_limit++;
        else if (parse_ret < 0)
            no_working++; /* other error, such as bad CCL map */
        else
        {
            client_parse_range(cl, startrecs, maxrecs);
            client_parse_sort(cl, sp);
            client_start_search(cl);
            no_working++;
        }
    }
    session_enter(se, "session_search2");
    se->clients_starting = 0;
    session_leave(se, "session_search2");
    if (no_working == 0)
    {
        if (no_failed_query > 0)
        {
            *addinfo = "query";
            return PAZPAR2_MALFORMED_PARAMETER_VALUE;
        }
        else if (no_failed_limit > 0)
        {
            *addinfo = "limit";
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
                                                      const char *id)
{
    struct database *db = new_database_inherit_settings(id, se->session_nmem, se->service->settings);
    session_init_databases_fun((void*) se, db);

    // New sdb is head of se->databases list
    return se->databases;
}

// Find an existing session database. If not found, load it
static struct session_database *find_session_database(struct session *se,
                                                      const char *id)
{
    struct session_database *sdb;

    for (sdb = se->databases; sdb; sdb = sdb->next)
        if (!strcmp(sdb->database->id, id))
            return sdb;
    return load_session_database(se, id);
}

// Apply a session override to a database
void session_apply_setting(struct session *se, const char *dbname,
                           const char *name, const char *value)
{
    session_enter(se, "session_apply_setting");
    {
        struct session_database *sdb = find_session_database(se, dbname);
        struct conf_service *service = se->service;
        struct setting *s;
        int offset = settings_create_offset(service, name);

        expand_settings_array(&sdb->settings, &sdb->num_settings, offset,
                              se->session_nmem);
        // Force later recompute of settings-driven data structures
        // (happens when a search starts and client connections are prepared)
        if (offset == PZ_XSLT)
            sdb->map = 0;
        se->settings_modified = 1;
        for (s = sdb->settings[offset]; s; s = s->next)
            if (!strcmp(s->name, name) &&
                dbname && s->target && !strcmp(dbname, s->target))
                break;
        if (!s)
        {
            s = nmem_malloc(se->session_nmem, sizeof(*s));
            s->precedence = 0;
            s->target = nmem_strdup(se->session_nmem, dbname);
            s->name = nmem_strdup(se->session_nmem, name);
            s->next = sdb->settings[offset];
            sdb->settings[offset] = s;
        }
        s->value = nmem_strdup(se->session_nmem, value);
    }
    session_leave(se, "session_apply_setting");
}

void session_destroy(struct session *se)
{
    struct session_database *sdb;
    session_log(se, YLOG_LOG, "destroy");
    session_use(-1);
    session_remove_cached_clients(se);

    for (sdb = se->databases; sdb; sdb = sdb->next)
        session_database_destroy(sdb);
    normalize_cache_destroy(se->normalize_cache);
    relevance_destroy(&se->relevance);
    reclist_destroy(se->reclist);
    xfree(se->mergekey);
    xfree(se->rank);
    if (nmem_total(se->nmem))
        session_log(se, YLOG_DEBUG, "NMEN operation usage %zd", nmem_total(se->nmem));
    if (nmem_total(se->session_nmem))
        session_log(se, YLOG_DEBUG, "NMEN session usage %zd", nmem_total(se->session_nmem));
    facet_limits_destroy(se->facet_limits);
    nmem_destroy(se->nmem);
    service_destroy(se->service);
    yaz_mutex_destroy(&se->session_mutex);
}

size_t session_get_memory_status(struct session *session) {
    size_t session_nmem;
    if (session == 0)
        return 0;
    session_enter(session, "session_get_memory_status");
    session_nmem = nmem_total(session->nmem);
    session_leave(session, "session_get_memory_status");
    return session_nmem;
}


struct session *new_session(NMEM nmem, struct conf_service *service,
                            unsigned session_id)
{
    int i;
    struct session *session = nmem_malloc(nmem, sizeof(*session));

    char tmp_str[50];

    sprintf(tmp_str, "session#%u", session_id);

    session->session_id = session_id;
    session_log(session, YLOG_DEBUG, "New");
    session->service = service;
    session->relevance = 0;
    session->total_records = 0;
    session->number_of_warnings_unknown_elements = 0;
    session->number_of_warnings_unknown_metadata = 0;
    session->termlists = 0;
    session->reclist = reclist_create(nmem);
    session->clients_active = 0;
    session->clients_cached = 0;
    session->settings_modified = 0;
    session->session_nmem = nmem;
    session->facet_id_list = 0;
    session->nmem = nmem_create();
    session->databases = 0;
    session->sorted_results = 0;
    session->facet_limits = 0;
    session->mergekey = 0;
    session->rank = 0;
    session->clients_starting = 0;

    for (i = 0; i <= SESSION_WATCH_MAX; i++)
    {
        session->watchlist[i].data = 0;
        session->watchlist[i].fun = 0;
    }
    session->normalize_cache = normalize_cache_create();
    session->session_mutex = 0;
    pazpar2_mutex_create(&session->session_mutex, tmp_str);
    session_log(session, YLOG_LOG, "create");

    session_use(1);
    return session;
}

static struct hitsbytarget *hitsbytarget_nb(struct session *se,
                                            int *count, NMEM nmem)
{
    struct hitsbytarget *res = 0;
    struct client_list *l;
    size_t sz = 0;

    for (l = se->clients_active; l; l = l->next)
        sz++;

    res = nmem_malloc(nmem, sizeof(*res) * sz);
    *count = 0;
    for (l = se->clients_active; l; l = l->next)
    {
        struct client *cl = l->client;
        WRBUF w = wrbuf_alloc();
        const char *name = session_setting_oneval(client_get_database(cl),
                                                  PZ_NAME);
        res[*count].id = client_get_id(cl);
        res[*count].name = *name ? name : "Unknown";
        res[*count].hits = client_get_hits(cl);
        res[*count].approximation = client_get_approximation(cl);
        res[*count].records = client_get_num_records(cl,
                                                     &res[*count].filtered,
                                                     0, 0);
        res[*count].diagnostic =
            client_get_diagnostic(cl, &res[*count].message,
                                  &res[*count].addinfo);
        res[*count].state = client_get_state_str(cl);
        res[*count].connected  = client_get_connection(cl) ? 1 : 0;
        session_settings_dump(se, client_get_database(cl), w);
        res[*count].settings_xml = nmem_strdup(nmem, wrbuf_cstr(w));
        wrbuf_rewind(w);
        res[*count].suggestions_xml =
            nmem_strdup(nmem, client_get_suggestions_xml(cl, w));

        res[*count].query_data =
            client_get_query(cl, &res[*count].query_type, nmem);
        wrbuf_destroy(w);
        (*count)++;
    }
    return res;
}

struct hitsbytarget *get_hitsbytarget(struct session *se, int *count, NMEM nmem)
{
    struct hitsbytarget *p;
    session_enter(se, "get_hitsbytarget");
    p = hitsbytarget_nb(se, count, nmem);
    session_leave(se, "get_hitsbytarget");
    return p;
}

// Compares two hitsbytarget nodes by hitcount
static int cmp_ht(const void *p1, const void *p2)
{
    const struct hitsbytarget *h1 = p1;
    const struct hitsbytarget *h2 = p2;
    return h2->hits - h1->hits;
}

// Compares two hitsbytarget nodes by hitcount
static int cmp_ht_approx(const void *p1, const void *p2)
{
    const struct hitsbytarget *h1 = p1;
    const struct hitsbytarget *h2 = p2;
    return h2->approximation - h1->approximation;
}

static int targets_termlist_nb(WRBUF wrbuf, struct session *se, int num,
                               NMEM nmem, int version)
{
    struct hitsbytarget *ht;
    int count, i;

    ht = hitsbytarget_nb(se, &count, nmem);
    if (version >= 2)
        qsort(ht, count, sizeof(struct hitsbytarget), cmp_ht_approx);
    else
        qsort(ht, count, sizeof(struct hitsbytarget), cmp_ht);
    for (i = 0; i < count && i < num && ht[i].hits > 0; i++)
    {

        // do only print terms which have display names

        wrbuf_puts(wrbuf, "<term>\n");

        wrbuf_puts(wrbuf, "<id>");
        wrbuf_xmlputs(wrbuf, ht[i].id);
        wrbuf_puts(wrbuf, "</id>\n");

        wrbuf_puts(wrbuf, "<name>");
        if (!ht[i].name || !ht[i].name[0])
            wrbuf_xmlputs(wrbuf, "NO TARGET NAME");
        else
            wrbuf_xmlputs(wrbuf, ht[i].name);
        wrbuf_puts(wrbuf, "</name>\n");

        wrbuf_printf(wrbuf, "<frequency>" ODR_INT_PRINTF "</frequency>\n",
                     ht[i].hits);

        if (version >= 2) {
            // Should not print if we know it isn't a approximation.
            wrbuf_printf(wrbuf, "<approximation>" ODR_INT_PRINTF "</approximation>\n", ht[i].approximation);
            wrbuf_printf(wrbuf, "<records>%d</records>\n", ht[i].records - ht[i].filtered);
            wrbuf_printf(wrbuf, "<filtered>%d</filtered>\n", ht[i].filtered);
        }

        wrbuf_puts(wrbuf, "<state>");
        wrbuf_xmlputs(wrbuf, ht[i].state);
        wrbuf_puts(wrbuf, "</state>\n");

        wrbuf_printf(wrbuf, "<diagnostic>%d</diagnostic>\n",
                     ht[i].diagnostic);
        wrbuf_puts(wrbuf, "</term>\n");
    }
    return count;
}

void perform_termlist(struct http_channel *c, struct session *se,
                      const char *name, int num, int version)
{
    int j;
    NMEM nmem_tmp = nmem_create();
    char **names;
    int num_names = 0;

    if (!name)
        name = "*";

    nmem_strsplit(nmem_tmp, ",", name, &names, &num_names);

    session_enter(se, "perform_termlist");

    for (j = 0; j < num_names; j++)
    {
        const char *tname;
        int must_generate_empty = 1; /* bug 5350 */

        struct named_termlist *t = se->termlists;
        for (; t; t = t->next)
        {
            tname = t->name;
            if (!strcmp(names[j], tname) || !strcmp(names[j], "*"))
            {
                struct termlist_score **p = 0;
                int len;

                wrbuf_puts(c->wrbuf, "<list name=\"");
                wrbuf_xmlputs(c->wrbuf, tname);
                wrbuf_puts(c->wrbuf, "\">\n");
                must_generate_empty = 0;

                p = termlist_highscore(t->termlist, &len, nmem_tmp);
                if (p)
                {
                    int i;
                    for (i = 0; i < len && i < num; i++)
                    {
                        // prevent sending empty term elements
                        if (!p[i]->display_term || !p[i]->display_term[0])
                            continue;

                        wrbuf_puts(c->wrbuf, "<term>");
                        wrbuf_puts(c->wrbuf, "<name>");
                        wrbuf_xmlputs(c->wrbuf, p[i]->display_term);
                        wrbuf_puts(c->wrbuf, "</name>");
                        wrbuf_printf(c->wrbuf,
                                     "<frequency>%d</frequency>",
                                     p[i]->frequency);
                        wrbuf_puts(c->wrbuf, "</term>\n");
                    }
                }
                wrbuf_puts(c->wrbuf, "</list>\n");
            }
        }
        tname = "xtargets";
        if (!strcmp(names[j], tname) || !strcmp(names[j], "*"))
        {
            wrbuf_puts(c->wrbuf, "<list name=\"");
            wrbuf_xmlputs(c->wrbuf, tname);
            wrbuf_puts(c->wrbuf, "\">\n");

            targets_termlist_nb(c->wrbuf, se, num, c->nmem, version);
            wrbuf_puts(c->wrbuf, "</list>\n");
            must_generate_empty = 0;
        }
        if (must_generate_empty)
        {
            wrbuf_puts(c->wrbuf, "<list name=\"");
            wrbuf_xmlputs(c->wrbuf, names[j]);
            wrbuf_puts(c->wrbuf, "\"/>\n");
        }
    }
    session_leave(se, "perform_termlist");
    nmem_destroy(nmem_tmp);
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
    struct record_cluster *r = 0;

    session_enter(se, "show_single_start");
    *prev_r = 0;
    *next_r = 0;
    reclist_limit(se->reclist, se, 1);

    reclist_enter(se->reclist);
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
        session_leave(se, "show_single_start");
    return r;
}

void show_single_stop(struct session *se, struct record_cluster *rec)
{
    session_leave(se, "show_single_stop");
}


int session_fetch_more(struct session *se)
{
    struct client_list *l;
    int ret = 0;

    for (l = se->clients_active; l; l = l->next)
    {
        struct client *cl = l->client;
        if (client_get_state(cl) == Client_Idle)
        {
            if (client_fetch_more(cl))
            {
                session_log(se, YLOG_LOG, "%s: more to fetch",
                            client_get_id(cl));
                ret = 1;
            }
            else
            {
                int filtered;
                int ingest_failures;
                int record_failures;
                int num = client_get_num_records(
                    cl, &filtered, &ingest_failures, &record_failures);

                session_log(se, YLOG_LOG, "%s: hits=" ODR_INT_PRINTF
                            " fetched=%d filtered=%d",
                            client_get_id(cl),
                            client_get_hits(cl),
                            num, filtered);
                if (ingest_failures || record_failures)
                {
                    session_log(se, YLOG_WARN, "%s:"
                                " ingest failures=%d record failures=%d",
                                client_get_id(cl),
                                ingest_failures, record_failures);
                }
            }
        }
        else
        {
            session_log(se, YLOG_LOG, "%s: no fetch due to state=%s",
                        client_get_id(cl), client_get_state_str(cl));
        }

    }
    return ret;
}

struct record_cluster **show_range_start(struct session *se,
                                         struct reclist_sortparms *sp,
                                         int start, int *num, int *total,
                                         Odr_int *sumhits, Odr_int *approx_hits,
                                         void (*show_records_ready)(void *data),
                                         struct http_channel *chan)
{
    struct record_cluster **recs = 0;
    struct reclist_sortparms *spp;
    struct client_list *l;
    int i;
#if USE_TIMING
    yaz_timing_t t = yaz_timing_create();
#endif
    session_enter(se, "show_range_start");
    *sumhits = 0;
    *approx_hits = 0;
    *total = 0;
    reclist_limit(se->reclist, se, 0);
    if (se->relevance)
    {
        for (spp = sp; spp; spp = spp->next)
            if (spp->type == Metadata_type_relevance)
            {
                relevance_prepare_read(se->relevance, se->reclist);
                break;
            }
        for (l = se->clients_active; l; l = l->next) {
            *sumhits += client_get_hits(l->client);
            *approx_hits += client_get_approximation(l->client);
        }
    }
    reclist_sort(se->reclist, sp);

    reclist_enter(se->reclist);
    *total = reclist_get_num_records(se->reclist);

    for (l = se->clients_active; l; l = l->next)
        client_update_show_stat(l->client, 0);

    for (i = 0; i < start; i++)
    {
        struct record_cluster *r = reclist_read_record(se->reclist);
        if (!r)
        {
            *num = 0;
            break;
        }
        else
        {
            struct record *rec = r->records;
            for (;rec; rec = rec->next)
                client_update_show_stat(rec->client, 1);
        }
    }
    recs = nmem_malloc(se->nmem, (*num > 0 ? *num : 1) * sizeof(*recs));
    for (i = 0; i < *num; i++)
    {
        struct record_cluster *r = reclist_read_record(se->reclist);
        if (!r)
        {
            *num = i;
            break;
        }
        else
        {
            struct record *rec = r->records;
            for (;rec; rec = rec->next)
                client_update_show_stat(rec->client, 1);
            recs[i] = r;
        }
    }
    reclist_leave(se->reclist);
#if USE_TIMING
    yaz_timing_stop(t);
    session_log(se, YLOG_LOG, "show %6.5f %3.2f %3.2f",
            yaz_timing_get_real(t), yaz_timing_get_user(t),
            yaz_timing_get_sys(t));
    yaz_timing_destroy(&t);
#endif

    if (!session_fetch_more(se))
        session_log(se, YLOG_LOG, "can not fetch more");
    else
    {
        show_range_stop(se, recs);
        session_log(se, YLOG_LOG, "fetching more in progress");
        if (session_set_watch(se, SESSION_WATCH_SHOW,
                              show_records_ready, chan, chan))
        {
            session_log(se, YLOG_WARN, "Ignoring show block");
            session_enter(se, "show_range_start");
        }
        else
        {
            session_log(se, YLOG_LOG, "session watch OK");
            return 0;
        }
    }
    return recs;
}

void show_range_stop(struct session *se, struct record_cluster **recs)
{
    session_leave(se, "show_range_stop");
}

void statistics(struct session *se, struct statistics *stat)
{
    struct client_list *l;
    int count = 0;

    memset(stat, 0, sizeof(*stat));
    stat->num_hits = 0;
    for (l = se->clients_active; l; l = l->next)
    {
        struct client *cl = l->client;
        if (!client_get_connection(cl))
            stat->num_no_connection++;
        stat->num_hits += client_get_hits(cl);
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
    stat->num_records = se->total_records;

    stat->num_clients = count;
}

static struct record_metadata *record_metadata_init(
    NMEM nmem, const char *value, const char *norm,
    enum conf_metadata_type type,
    struct _xmlAttr *attr)
{
    struct record_metadata *rec_md = record_metadata_create(nmem);
    struct record_metadata_attr **attrp = &rec_md->attributes;

    for (; attr; attr = attr->next)
    {
        if (attr->children && attr->children->content)
        {
            if (strcmp((const char *) attr->name, "type")
                && strcmp((const char *) attr->name, "empty"))
            {  /* skip the "type" + "empty" attribute..
                  The "Type" is already part of the element in output
                  (md-%s) and so repeating it here is redundant */
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

    switch (type)
    {
    case Metadata_type_generic:
    case Metadata_type_skiparticle:
        if (norm)
        {
            rec_md->data.text.disp = nmem_strdup(nmem, value);
            rec_md->data.text.norm = nmem_strdup(nmem, norm);
        }
        else
        {
            if (strstr(value, "://")) /* looks like a URL */
                rec_md->data.text.disp = nmem_strdup(nmem, value);
            else
                rec_md->data.text.disp =
                    normalize7bit_generic(nmem_strdup(nmem, value), " ,/.:([");
            rec_md->data.text.norm = rec_md->data.text.disp;
        }
        rec_md->data.text.sort = 0;
        rec_md->data.text.snippet = 0;
        break;
    case Metadata_type_year:
    case Metadata_type_date:
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
    break;
    case Metadata_type_float:
        rec_md->data.fnumber = atof(value);
        break;
    case Metadata_type_relevance:
    case Metadata_type_position:
    case Metadata_type_retrieval:
        return 0;
    }
    return rec_md;
}

static void mergekey_norm_wr(pp2_charset_fact_t charsets,
                             WRBUF norm_wr, const char *value)
{
    const char *norm_str;
    pp2_charset_token_t prt =
        pp2_charset_token_create(charsets, "mergekey");

    pp2_charset_token_first(prt, value, 0);
    while ((norm_str = pp2_charset_token_next(prt)))
    {
        if (*norm_str)
        {
            if (wrbuf_len(norm_wr))
                wrbuf_puts(norm_wr, " ");
            wrbuf_puts(norm_wr, norm_str);
        }
    }
    pp2_charset_token_destroy(prt);
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
            const char *type = yaz_xml_get_prop(n, "type");
            if (type == NULL) {
                yaz_log(YLOG_FATAL, "Missing type attribute on metadata element. Skipping!");
            }
            else if (!strcmp(name, (const char *) type))
            {
                xmlChar *value = xmlNodeListGetString(doc, n->children, 1);
                if (value && *value)
                {
                    if (wrbuf_len(norm_wr) > 0)
                        wrbuf_puts(norm_wr, " ");
                    wrbuf_puts(norm_wr, name);
                    mergekey_norm_wr(service->charsets, norm_wr,
                                     (const char *) value);
                    no_found++;
                }
                if (value)
                    xmlFree(value);
            }
        }
    }
    return no_found;
}

static const char *get_mergekey(xmlDoc *doc, xmlNode *root, 
                                struct client *cl, int record_no,
                                struct conf_service *service, NMEM nmem,
                                const char *session_mergekey)
{
    char *mergekey_norm = 0;
    WRBUF norm_wr = wrbuf_alloc();
    const char *mergekey;

    if (session_mergekey)
    {
        int i, num = 0;
        char **values = 0;
        nmem_strsplit_escape2(nmem, ",", session_mergekey, &values,
                              &num, 1, '\\', 1);

        for (i = 0; i < num; i++)
            get_mergekey_from_doc(doc, root, values[i], service, norm_wr);
    }
    else if ((mergekey = yaz_xml_get_prop(root, "mergekey")))
    {
        mergekey_norm_wr(service->charsets, norm_wr, mergekey);
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
        wrbuf_printf(norm_wr, "position: %s-%d",
                     client_get_id(cl), record_no);
    }
    else
    {
        const char *lead = "content: ";
        wrbuf_insert(norm_wr, 0, lead, strlen(lead));
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
            const char *type = yaz_xml_get_prop(n, "type");
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
            }
        }
    }
    return match;
}

static int ingest_to_cluster(struct client *cl,
                             WRBUF wrbuf_disp,
                             WRBUF wrbuf_norm,
                             xmlDoc *xdoc,
                             xmlNode *root,
                             int record_no,
                             struct record_metadata_attr *mergekey);

static int ingest_sub_record(struct client *cl, xmlDoc *xdoc, xmlNode *root,
                             int record_no, NMEM nmem,
                             struct session_database *sdb,
                             struct record_metadata_attr *mergekeys)
{
    int ret = 0;
    struct session *se = client_get_session(cl);
    WRBUF wrbuf_disp, wrbuf_norm;

    if (!check_record_filter(root, sdb))
    {
        session_log(se, YLOG_LOG,
                    "Filtered out record no %d from %s",
                    record_no, sdb->database->id);
        return 0;
    }
    wrbuf_disp = wrbuf_alloc();
    wrbuf_norm = wrbuf_alloc();
    session_enter(se, "ingest_sub_record");
    if (client_get_session(cl) == se && se->relevance)
        ret = ingest_to_cluster(cl, wrbuf_disp, wrbuf_norm,
                                xdoc, root, record_no, mergekeys);
    session_leave(se, "ingest_sub_record");
    wrbuf_destroy(wrbuf_norm);
    wrbuf_destroy(wrbuf_disp);
    return ret;
}

/** \brief ingest XML record
    \param cl client holds the result set for record
    \param rec record buffer (0 terminated)
    \param record_no record position (1, 2, ..)
    \param nmem working NMEM
    \retval 0 OK
    \retval -1 failure
    \retval -2 Filtered
*/
int ingest_record(struct client *cl, const char *rec,
                  int record_no, NMEM nmem)
{
    struct session *se = client_get_session(cl);
    struct session_database *sdb = client_get_database(cl);
    struct conf_service *service = se->service;
    xmlDoc *xdoc = normalize_record(se, sdb, service, rec, nmem);
    int r = ingest_xml_record(cl, xdoc, record_no, nmem, 0);
    client_store_xdoc(cl, record_no, xdoc);
    return r;
}

int ingest_xml_record(struct client *cl, xmlDoc *xdoc,
                      int record_no, NMEM nmem, int cached_copy)
{
    struct session *se = client_get_session(cl);
    struct session_database *sdb = client_get_database(cl);
    struct conf_service *service = se->service;
    xmlNode *root;
    int r = 0;
    if (!xdoc)
        return -1;

    if (global_parameters.dump_records)
    {
        session_log(se, YLOG_LOG, "Normalized record from %s",
                    sdb->database->id);
        log_xml_doc(xdoc);
    }

    root = xmlDocGetRootElement(xdoc);

    if (!strcmp((const char *) root->name, "cluster"))
    {
        int no_merge_keys = 0;
        int no_merge_dups = 0;
        xmlNode *sroot;
        struct record_metadata_attr *mk = 0;

        for (sroot = root->children; sroot; sroot = sroot->next)
            if (sroot->type == XML_ELEMENT_NODE &&
                !strcmp((const char *) sroot->name, "record"))
            {
                struct record_metadata_attr **mkp;
                const char *mergekey_norm =
                    get_mergekey(xdoc, sroot, cl, record_no, service, nmem,
                                 se->mergekey);
                if (!mergekey_norm)
                {
                    r = -1;
                    break;
                }
                for (mkp = &mk; *mkp; mkp = &(*mkp)->next)
                    if (!strcmp((*mkp)->value, mergekey_norm))
                        break;
                if (!*mkp)
                {
                    *mkp = (struct record_metadata_attr*)
                        nmem_malloc(nmem, sizeof(**mkp));
                    (*mkp)->name = 0;
                    (*mkp)->value = nmem_strdup(nmem, mergekey_norm);
                    (*mkp)->next = 0;
                    no_merge_keys++;
                }
                else
                    no_merge_dups++;
            }
        if (no_merge_keys > 1 || no_merge_dups > 0)
        {
            yaz_log(YLOG_LOG, "Got %d mergekeys, %d dups for position %d",
                    no_merge_keys, no_merge_dups, record_no);
        }
        for (sroot = root->children; !r && sroot; sroot = sroot->next)
            if (sroot->type == XML_ELEMENT_NODE &&
                !strcmp((const char *) sroot->name, "record"))
            {
                if (!cached_copy)
                    insert_settings_values(sdb, xdoc, root, service);
                r = ingest_sub_record(cl, xdoc, sroot, record_no, nmem, sdb,
                                      mk);
            }
    }
    else if (!strcmp((const char *) root->name, "record"))
    {
        const char *mergekey_norm =
            get_mergekey(xdoc, root, cl, record_no, service, nmem,
                         se->mergekey);
        if (mergekey_norm)
        {
            struct record_metadata_attr *mk = (struct record_metadata_attr*)
                nmem_malloc(nmem, sizeof(*mk));
            mk->name = 0;
            mk->value = nmem_strdup(nmem, mergekey_norm);
            mk->next = 0;

            if (!cached_copy)
                insert_settings_values(sdb, xdoc, root, service);
            r = ingest_sub_record(cl, xdoc, root, record_no, nmem, sdb, mk);
        }
    }
    else
    {
        session_log(se, YLOG_WARN, "Bad pz root element: %s",
                    (const char *) root->name);
        r = -1;
    }
    return r;
}


//    struct conf_metadata *ser_md = &service->metadata[md_field_id];
//    struct record_metadata *rec_md = record->metadata[md_field_id];
static int match_metadata_local(struct conf_service *service,
                                struct conf_metadata *ser_md,
                                struct record_metadata *rec_md0,
                                char **values, int num_v)
{
    int i;
    struct record_metadata *rec_md = rec_md0;
    WRBUF val_wr = 0;
    WRBUF text_wr = wrbuf_alloc();
    for (i = 0; i < num_v; )
    {
        if (rec_md)
        {
            if (ser_md->type == Metadata_type_year
                || ser_md->type == Metadata_type_date)
            {
                int y = atoi(values[i]);
                if (y >= rec_md->data.number.min
                    && y <= rec_md->data.number.max)
                    break;
            }
            else
            {
                if (!val_wr)
                {
                    val_wr = wrbuf_alloc();
                    mergekey_norm_wr(service->charsets, val_wr, values[i]);
                }
                wrbuf_rewind(text_wr);
                mergekey_norm_wr(service->charsets, text_wr,
                                 rec_md->data.text.disp);
                if (!strcmp(wrbuf_cstr(val_wr), wrbuf_cstr(text_wr)))
                    break;
            }
            rec_md = rec_md->next;
        }
        else
        {
            rec_md = rec_md0;
            wrbuf_destroy(val_wr);
            val_wr = 0;
            i++;
        }
    }
    wrbuf_destroy(val_wr);
    wrbuf_destroy(text_wr);
    return i < num_v ? 1 : 0;
}

int session_check_cluster_limit(struct session *se, struct record_cluster *rec)
{
    int i;
    struct conf_service *service = se->service;
    int ret = 1;
    const char *name;
    const char *value;
    NMEM nmem_tmp = nmem_create();

    for (i = 0; (name = facet_limits_get(se->facet_limits, i, &value)); i++)
    {
        int j;
        for (j = 0; j < service->num_metadata; j++)
        {
            struct conf_metadata *md = service->metadata + j;
            if (!strcmp(md->name, name) && md->limitcluster)
            {
                char **values = 0;
                int num = 0;
                int md_field_id =
                    conf_service_metadata_field_id(service,
                                                   md->limitcluster);

                if (md_field_id < 0)
                {
                    ret = 0;
                    break;
                }

                nmem_strsplit_escape2(nmem_tmp, "|", value, &values,
                                      &num, 1, '\\', 1);

                if (!match_metadata_local(service,
                                          &service->metadata[md_field_id],
                                          rec->metadata[md_field_id],
                                          values, num))
                {
                    ret = 0;
                    break;
                }
            }
        }
    }
    nmem_destroy(nmem_tmp);
    return ret;
}

// Skip record on non-zero
static int check_limit_local(struct client *cl,
                             struct record *record,
                             int record_no)
{
    int skip_record = 0;
    struct session *se = client_get_session(cl);
    struct conf_service *service = se->service;
    NMEM nmem_tmp = nmem_create();
    struct session_database *sdb = client_get_database(cl);
    int l = 0;
    while (!skip_record)
    {
        int md_field_id;
        char **values = 0;
        int num_v = 0;
        const char *name =
            client_get_facet_limit_local(cl, sdb, &l, nmem_tmp,
                                         &num_v, &values);
        if (!name)
            break;

        if (!strcmp(name, "*"))
        {
            for (md_field_id = 0; md_field_id < service->num_metadata;
                 md_field_id++)
            {
                if (match_metadata_local(
                        service,
                        &service->metadata[md_field_id],
                        record->metadata[md_field_id],
                        values, num_v))
                    break;
            }
            if (md_field_id == service->num_metadata)
                skip_record = 1;
        }
        else
        {
            md_field_id = conf_service_metadata_field_id(service, name);
            if (md_field_id < 0)
            {
                skip_record = 1;
                break;
            }
            if (!match_metadata_local(
                    service,
                    &service->metadata[md_field_id],
                    record->metadata[md_field_id],
                    values, num_v))
            {
                skip_record = 1;
            }
        }
    }
    nmem_destroy(nmem_tmp);
    return skip_record;
}

static int ingest_to_cluster(struct client *cl,
                             WRBUF wrbuf_disp,
                             WRBUF wrbuf_norm,
                             xmlDoc *xdoc,
                             xmlNode *root,
                             int record_no,
                             struct record_metadata_attr *merge_keys)
{
    xmlNode *n;
    struct session *se = client_get_session(cl);
    struct conf_service *service = se->service;
    int term_factor = 1;
    struct record_cluster *cluster;
    struct record_metadata **metadata0;
    struct session_database *sdb = client_get_database(cl);
    NMEM ingest_nmem = 0;
    char **rank_values = 0;
    int rank_num = 0;
    struct record *record = record_create(se->nmem,
                                          service->num_metadata,
                                          service->num_sortkeys, cl,
                                          record_no);

    for (n = root->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp((const char *) n->name, "metadata"))
        {
            struct conf_metadata *ser_md = 0;
            struct record_metadata **wheretoput = 0;
            struct record_metadata *rec_md = 0;
            int md_field_id = -1;
            xmlChar *value0;
            const char *type = yaz_xml_get_prop(n, "type");

            if (!type)
                continue;

            md_field_id
                = conf_service_metadata_field_id(service, (const char *) type);
            if (md_field_id < 0)
            {
                if (se->number_of_warnings_unknown_metadata == 0)
                {
                    session_log(se, YLOG_WARN,
                            "Ignoring unknown metadata element: %s", type);
                }
                se->number_of_warnings_unknown_metadata++;
                continue;
            }

            wrbuf_rewind(wrbuf_disp);
            value0 = xmlNodeListGetString(xdoc, n->children, 1);
            if (!value0 || !*value0)
            {
                const char *empty = yaz_xml_get_prop(n, "empty");
                if (!empty)
                    continue;
                wrbuf_puts(wrbuf_disp, (const char *) empty);
            }
            else
            {
                wrbuf_puts(wrbuf_disp, (const char *) value0);
            }
            if (value0)
                xmlFree(value0);
            ser_md = &service->metadata[md_field_id];

            // non-merged metadata
            rec_md = record_metadata_init(se->nmem, wrbuf_cstr(wrbuf_disp), 0,
                                          ser_md->type, n->properties);
            if (!rec_md)
            {
                session_log(se, YLOG_WARN, "bad metadata data '%s' "
                            "for element '%s'", wrbuf_cstr(wrbuf_disp), type);
                continue;
            }

            if (ser_md->type == Metadata_type_generic)
            {
                WRBUF w = wrbuf_alloc();
                if (relevance_snippet(se->relevance,
                                      wrbuf_cstr(wrbuf_disp), ser_md->name, w))
                    rec_md->data.text.snippet = nmem_strdup(se->nmem,
                                                            wrbuf_cstr(w));
                wrbuf_destroy(w);
            }


            wheretoput = &record->metadata[md_field_id];
            while (*wheretoput)
                wheretoput = &(*wheretoput)->next;
            *wheretoput = rec_md;
        }
    }

    if (check_limit_local(cl, record, record_no))
    {
        return -2;
    }
    cluster = reclist_insert(se->reclist, se->relevance, service, record,
                             merge_keys, &se->total_merged);
    if (!cluster)
    {
        return 0; // complete match with existing record
    }

    {
        const char *use_term_factor_str =
            session_setting_oneval(sdb, PZ_TERMLIST_TERM_FACTOR);
        if (use_term_factor_str && use_term_factor_str[0] == '1')
        {
            int maxrecs = client_get_maxrecs(cl);
            int hits = (int) client_get_hits(cl);
            term_factor = MAX(hits, maxrecs) /  MAX(1, maxrecs);
            assert(term_factor >= 1);
            session_log(se, YLOG_DEBUG, "Using term factor: %d (%d / %d)",
                        term_factor, MAX(hits, maxrecs), MAX(1, maxrecs));
        }
    }

    if (global_parameters.dump_records)
        session_log(se, YLOG_LOG, "Cluster id %s from %s (#%d)", cluster->recid,
                    sdb->database->id, record_no);

    // original metadata, to check if first existence of a field
    metadata0 = xmalloc(sizeof(*metadata0) * service->num_metadata);
    memcpy(metadata0, cluster->metadata,
           sizeof(*metadata0) * service->num_metadata);

    ingest_nmem = nmem_create();
    if (se->rank)
    {
        yaz_log(YLOG_LOG, "local in sort : %s", se->rank);
        nmem_strsplit_escape2(ingest_nmem, ",", se->rank, &rank_values,
                              &rank_num, 1, '\\', 1);
    }

    // now parsing XML record and adding data to cluster or record metadata
    for (n = root->children; n; n = n->next)
    {
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
            const char *rank = 0;
            const char *xml_rank = 0;
            const char *type = 0;
            xmlChar *value0;

            type = yaz_xml_get_prop(n, "type");
            if (!type)
                continue;

            md_field_id
                = conf_service_metadata_field_id(service, (const char *) type);
            if (md_field_id < 0)
                continue;

            ser_md = &service->metadata[md_field_id];

            if (ser_md->sortkey_offset >= 0)
            {
                sk_field_id = ser_md->sortkey_offset;
                ser_sk = &service->sortkeys[sk_field_id];
            }

            wrbuf_rewind(wrbuf_disp);
            wrbuf_rewind(wrbuf_norm);

            value0 = xmlNodeListGetString(xdoc, n->children, 1);
            if (!value0 || !*value0)
            {
                if (value0)
                    xmlFree(value0);
                continue;
            }

            if (ser_md->icurule)
            {
                run_icu(se, ser_md->icurule, (const char *) value0,
                        wrbuf_norm, wrbuf_disp);
                yaz_log(YLOG_LOG, "run_icu input=%s norm=%s disp=%s",
                        (const char *) value0,
                        wrbuf_cstr(wrbuf_norm), wrbuf_cstr(wrbuf_disp));
                rec_md = record_metadata_init(se->nmem, wrbuf_cstr(wrbuf_disp),
                                              wrbuf_cstr(wrbuf_norm),
                                              ser_md->type, 0);
            }
            else
            {
                wrbuf_puts(wrbuf_disp, (const char *) value0);
                rec_md = record_metadata_init(se->nmem, wrbuf_cstr(wrbuf_disp),
                                              0,
                                              ser_md->type, 0);
            }

            xmlFree(value0);

            // see if the field was not in cluster already (from beginning)
            if (!rec_md)
                continue;

            if (rank_num)
            {
                int i;
                for (i = 0; i < rank_num; i++)
                {
                    const char *val = rank_values[i];
                    const char *cp = strchr(val, '=');
                    if (!cp)
                        continue;
                    if ((cp - val) == strlen((const char *) type)
                        && !memcmp(val, type, cp - val))
                    {
                        rank = cp + 1;
                        break;
                    }
                }
            }
            else
            {
                xml_rank = yaz_xml_get_prop(n, "rank");
                rank = xml_rank ? (const char *) xml_rank : ser_md->rank;
            }

            wheretoput = &cluster->metadata[md_field_id];

            if (ser_md->merge == Metadata_merge_first)
            {
                if (!metadata0[md_field_id])
                {
                    while (*wheretoput)
                        wheretoput = &(*wheretoput)->next;
                    *wheretoput = rec_md;
                }
            }
            else if (ser_md->merge == Metadata_merge_unique)
            {
                while (*wheretoput)
                {
                    if (!strcmp((const char *) (*wheretoput)->data.text.norm,
                                rec_md->data.text.norm))
                        break;
                    wheretoput = &(*wheretoput)->next;
                }
                if (!*wheretoput)
                    *wheretoput = rec_md;
            }
            else if (ser_md->merge == Metadata_merge_longest)
            {
                if (!*wheretoput
                    || strlen(rec_md->data.text.norm)
                    > strlen((*wheretoput)->data.text.norm))
                {
                    *wheretoput = rec_md;
                    if (ser_sk)
                    {
                        pp2_charset_token_t prt;
                        const char *sort_str = 0;
                        int skip_article =
                            ser_sk->type == Metadata_type_skiparticle;

                        if (!cluster->sortkeys[sk_field_id])
                            cluster->sortkeys[sk_field_id] =
                                nmem_malloc(se->nmem,
                                            sizeof(union data_types));

                        prt =
                            pp2_charset_token_create(service->charsets, "sort");

                        pp2_charset_token_first(prt, rec_md->data.text.disp,
                                                skip_article);

                        pp2_charset_token_next(prt);

                        sort_str = pp2_get_sort(prt);

                        cluster->sortkeys[sk_field_id]->text.disp =
                            rec_md->data.text.disp;
                        if (!sort_str)
                        {
                            sort_str = rec_md->data.text.disp;
                            session_log(se, YLOG_WARN,
                                    "Could not make sortkey. Bug #1858");
                        }
                        cluster->sortkeys[sk_field_id]->text.sort =
                            nmem_strdup(se->nmem, sort_str);
                        pp2_charset_token_destroy(prt);
                    }
                }
            }
            else if (ser_md->merge == Metadata_merge_all)
            {
                while (*wheretoput)
                    wheretoput = &(*wheretoput)->next;
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
            if (rank)
            {
                relevance_countwords(se->relevance, cluster,
                                     wrbuf_cstr(wrbuf_disp),
                                     rank, ser_md->name);
            }
            // construct facets ... unless the client already has reported them
            if (ser_md->termlist && !client_has_facet(cl, (char *) type))
            {
                if (ser_md->type == Metadata_type_year)
                {
                    char year[64];
                    sprintf(year, "%d", rec_md->data.number.max);

                    add_facet(se, (char *) type, year, term_factor, cl);
                    if (rec_md->data.number.max != rec_md->data.number.min)
                    {
                        sprintf(year, "%d", rec_md->data.number.min);
                        add_facet(se, (char *) type, year, term_factor, cl);
                    }
                }
                else
                    add_facet(se, type, wrbuf_cstr(wrbuf_disp), term_factor, cl);
            }
        }
        else
        {
            if (se->number_of_warnings_unknown_elements == 0)
                session_log(se, YLOG_WARN,
                        "Unexpected element in internal record: %s", n->name);
            se->number_of_warnings_unknown_elements++;
        }
    }
    nmem_destroy(ingest_nmem);
    xfree(metadata0);
    relevance_donerecord(se->relevance, cluster);
    se->total_records++;

    return 0;
}

void session_log(struct session *s, int level, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);

    yaz_vsnprintf(buf, sizeof(buf)-30, fmt, ap);
    yaz_log(level, "Session %u: %s", s ? s->session_id : 0, buf);

    va_end(ap);
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

