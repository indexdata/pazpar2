/* This file is part of Pazpar2.
   Copyright (C) 2006-2011 Index Data

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

/** \file client.c
    \brief Z39.50 client 
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>
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
#include <yaz/diagbib1.h>
#include <yaz/snprintf.h>
#include <yaz/rpn2cql.h>
#include <yaz/rpn2solr.h>

#define USE_TIMING 0
#if USE_TIMING
#include <yaz/timing.h>
#endif

#include "ppmutex.h"
#include "session.h"
#include "parameters.h"
#include "client.h"
#include "connection.h"
#include "settings.h"
#include "relevance.h"
#include "incref.h"

static YAZ_MUTEX g_mutex = 0;
static int no_clients = 0;
static int no_clients_total = 0;

static int client_use(int delta)
{
    int clients;
    if (!g_mutex)
        yaz_mutex_create(&g_mutex);
    yaz_mutex_enter(g_mutex);
    no_clients += delta;
    if (delta > 0)
        no_clients_total += delta;
    clients = no_clients;
    yaz_mutex_leave(g_mutex);
    yaz_log(YLOG_DEBUG, "%s clients=%d", delta == 0 ? "" : (delta > 0 ? "INC" : "DEC"), clients);
    return clients;
}

int  clients_count(void) {
    return client_use(0);
}

int  clients_count_total(void) {
    int total = 0;
    if (!g_mutex)
        return 0;
    yaz_mutex_enter(g_mutex);
    total = no_clients_total;
    yaz_mutex_leave(g_mutex);
    return total;
}


/** \brief Represents client state for a connection to one search target */
struct client {
    struct session_database *database;
    struct connection *connection;
    struct session *session;
    char *pquery; // Current search
    char *cqlquery; // used for SRU targets only
    Odr_int hits;
    int record_offset;
    int maxrecs;
    int startrecs;
    int diagnostic;
    int preferred;
    struct suggestions *suggestions;
    enum client_state state;
    struct show_raw *show_raw;
    ZOOM_resultset resultset;
    YAZ_MUTEX mutex;
    int ref_count;
    char *id;
};

struct suggestions {
    NMEM nmem;
    int num;
    char **misspelled;
    char **suggest;
    char *passthrough;
};

struct show_raw {
    int active; // whether this request has been sent to the server
    int position;
    int binary;
    char *syntax;
    char *esn;
    void (*error_handler)(void *data, const char *addinfo);
    void (*record_handler)(void *data, const char *buf, size_t sz);
    void *data;
    struct show_raw *next;
};

static const char *client_states[] = {
    "Client_Connecting",
    "Client_Idle",
    "Client_Working",
    "Client_Error",
    "Client_Failed",
    "Client_Disconnected"
};

const char *client_get_state_str(struct client *cl)
{
    return client_states[cl->state];
}

enum client_state client_get_state(struct client *cl)
{
    return cl->state;
}

void client_set_state(struct client *cl, enum client_state st)
{
    int was_active = 0;
    if (client_is_active(cl))
        was_active = 1;
    cl->state = st;
    /* If client is going from being active to inactive and all clients
       are now idle we fire a watch for the session . The assumption is
       that session is not mutex locked if client is already active */
    if (was_active && !client_is_active(cl) && cl->session)
    {

        int no_active = session_active_clients(cl->session);
        yaz_log(YLOG_DEBUG, "%s: releasing watches on zero active: %d",
                client_get_id(cl), no_active);
        if (no_active == 0) {
            session_alert_watch(cl->session, SESSION_WATCH_SHOW);
            session_alert_watch(cl->session, SESSION_WATCH_SHOW_PREF);
        }
    }
}

static void client_show_raw_error(struct client *cl, const char *addinfo);

struct connection *client_get_connection(struct client *cl)
{
    return cl->connection;
}

struct session_database *client_get_database(struct client *cl)
{
    return cl->database;
}

struct session *client_get_session(struct client *cl)
{
    return cl->session;
}

const char *client_get_pquery(struct client *cl)
{
    return cl->pquery;
}

static void client_send_raw_present(struct client *cl);
static int nativesyntax_to_type(struct session_database *sdb, char *type,
                                ZOOM_record rec);

static void client_show_immediate(
    ZOOM_resultset resultset, struct session_database *sdb, int position,
    void *data,
    void (*error_handler)(void *data, const char *addinfo),
    void (*record_handler)(void *data, const char *buf, size_t sz),
    int binary)
{
    ZOOM_record rec = 0;
    char type[80];
    const char *buf;
    int len;

    if (!resultset)
    {
        error_handler(data, "no resultset");
        return;
    }
    rec = ZOOM_resultset_record(resultset, position-1);
    if (!rec)
    {
        error_handler(data, "no record");
        return;
    }
    if (binary)
        strcpy(type, "raw");
    else
        nativesyntax_to_type(sdb, type, rec);
    buf = ZOOM_record_get(rec, type, &len);
    if (!buf)
    {
        error_handler(data, "no record");
        return;
    }
    record_handler(data, buf, len);
}


int client_show_raw_begin(struct client *cl, int position,
                          const char *syntax, const char *esn,
                          void *data,
                          void (*error_handler)(void *data, const char *addinfo),
                          void (*record_handler)(void *data, const char *buf,
                                                 size_t sz),
                          int binary)
{
    if (syntax == 0 && esn == 0)
        client_show_immediate(cl->resultset, client_get_database(cl),
                              position, data,
                              error_handler, record_handler,
                              binary);
    else
    {
        struct show_raw *rr, **rrp;

        if (!cl->connection)
            return -1;
    

        rr = xmalloc(sizeof(*rr));
        rr->position = position;
        rr->active = 0;
        rr->data = data;
        rr->error_handler = error_handler;
        rr->record_handler = record_handler;
        rr->binary = binary;
        if (syntax)
            rr->syntax = xstrdup(syntax);
        else
            rr->syntax = 0;
        if (esn)
            rr->esn = xstrdup(esn);
        else
            rr->esn = 0;
        rr->next = 0;
        
        for (rrp = &cl->show_raw; *rrp; rrp = &(*rrp)->next)
            ;
        *rrp = rr;
        
        if (cl->state == Client_Failed)
        {
            client_show_raw_error(cl, "client failed");
        }
        else if (cl->state == Client_Disconnected)
        {
            client_show_raw_error(cl, "client disconnected");
        }
        else
        {
            client_send_raw_present(cl);
        }
    }
    return 0;
}

static void client_show_raw_delete(struct show_raw *r)
{
    xfree(r->syntax);
    xfree(r->esn);
    xfree(r);
}

void client_show_raw_remove(struct client *cl, void *data)
{
    struct show_raw *rr = data;
    struct show_raw **rrp = &cl->show_raw;
    while (*rrp != rr)
        rrp = &(*rrp)->next;
    if (*rrp)
    {
        *rrp = rr->next;
        client_show_raw_delete(rr);
    }
}

void client_show_raw_dequeue(struct client *cl)
{
    struct show_raw *rr = cl->show_raw;

    cl->show_raw = rr->next;
    client_show_raw_delete(rr);
}

static void client_show_raw_error(struct client *cl, const char *addinfo)
{
    while (cl->show_raw)
    {
        cl->show_raw->error_handler(cl->show_raw->data, addinfo);
        client_show_raw_dequeue(cl);
    }
}

static void client_send_raw_present(struct client *cl)
{
    struct session_database *sdb = client_get_database(cl);
    struct connection *co = client_get_connection(cl);
    ZOOM_resultset set = cl->resultset;

    int offset = cl->show_raw->position;
    const char *syntax = 0;
    const char *elements = 0;

    assert(cl->show_raw);
    assert(set);

    yaz_log(YLOG_DEBUG, "%s: trying to present %d record(s) from %d",
            client_get_id(cl), 1, offset);

    if (cl->show_raw->syntax)
        syntax = cl->show_raw->syntax;
    else
        syntax = session_setting_oneval(sdb, PZ_REQUESTSYNTAX);
    ZOOM_resultset_option_set(set, "preferredRecordSyntax", syntax);

    if (cl->show_raw->esn)
        elements = cl->show_raw->esn;
    else
        elements = session_setting_oneval(sdb, PZ_ELEMENTS);
    if (elements && *elements)
        ZOOM_resultset_option_set(set, "elementSetName", elements);

    ZOOM_resultset_records(set, 0, offset-1, 1);
    cl->show_raw->active = 1;

    connection_continue(co);
}

static int nativesyntax_to_type(struct session_database *sdb, char *type,
                                ZOOM_record rec)
{
    const char *s = session_setting_oneval(sdb, PZ_NATIVESYNTAX);

    if (s && *s)
    {
        if (!strncmp(s, "iso2709", 7))
        {
            const char *cp = strchr(s, ';');
            yaz_snprintf(type, 80, "xml; charset=%s", cp ? cp+1 : "marc-8s");
        }
        else if (!strncmp(s, "xml", 3))
        {
            strcpy(type, "xml");
        }
        else if (!strncmp(s, "txml", 4))
        {
            const char *cp = strchr(s, ';');
            yaz_snprintf(type, 80, "txml; charset=%s", cp ? cp+1 : "marc-8s");
        }
        else
            return -1;
        return 0;
    }
    else  /* attempt to deduce structure */
    {
        const char *syntax = ZOOM_record_get(rec, "syntax", NULL);
        if (syntax)
        {
            if (!strcmp(syntax, "XML"))
            {
                strcpy(type, "xml");
                return 0;
            }
            else if (!strcmp(syntax, "USmarc") || !strcmp(syntax, "MARC21"))
            {
                strcpy(type, "xml; charset=marc8-s");
                return 0;
            }
            else return -1;
        }
        else return -1;
    }
}

/**
 * TODO Consider thread safety!!!
 *
 */
void client_report_facets(struct client *cl, ZOOM_resultset rs)
{
    struct session_database *sdb = client_get_database(cl);
    ZOOM_facet_field *facets = ZOOM_resultset_facets(rs);

    if (sdb && facets)
    {
        struct session *se = client_get_session(cl);
        int facet_num = ZOOM_resultset_facets_size(rs);
        struct setting *s;

        for (s = sdb->settings[PZ_FACETMAP]; s; s = s->next)
        {
            const char *p = strchr(s->name + 3, ':');
            if (p && p[1] && s->value && s->value[0])
            {
                int facet_idx;
                p++; /* p now holds logical facet name */
                for (facet_idx = 0; facet_idx < facet_num; facet_idx++)
                {
                    const char *native_name =
                        ZOOM_facet_field_name(facets[facet_idx]);
                    if (native_name && !strcmp(s->value, native_name))
                    {
                        size_t term_idx;
                        size_t term_num =
                            ZOOM_facet_field_term_count(facets[facet_idx]);
                        for (term_idx = 0; term_idx < term_num; term_idx++ )
                        {
                            int freq;
                            const char *term =
                                ZOOM_facet_field_get_term(facets[facet_idx],
                                                          term_idx, &freq);
                            if (term)
                                add_facet(se, p, term, freq);
                        }
                        break;
                    }
                }
            }
        }
    }
}

static void ingest_raw_record(struct client *cl, ZOOM_record rec)
{
    const char *buf;
    int len;
    char type[80];

    if (cl->show_raw->binary)
        strcpy(type, "raw");
    else
    {
        struct session_database *sdb = client_get_database(cl);
        nativesyntax_to_type(sdb, type, rec);
    }

    buf = ZOOM_record_get(rec, type, &len);
    cl->show_raw->record_handler(cl->show_raw->data,  buf, len);
    client_show_raw_dequeue(cl);
}

void client_check_preferred_watch(struct client *cl)
{
    struct session *se = cl->session;
    yaz_log(YLOG_DEBUG, "client_check_preferred_watch: %s ", client_get_id(cl));
    if (se)
    {
        client_unlock(cl);
        /* TODO possible threading issue. Session can have been destroyed */
        if (session_is_preferred_clients_ready(se)) {
            session_alert_watch(se, SESSION_WATCH_SHOW_PREF);
        }
        else
            yaz_log(YLOG_DEBUG, "client_check_preferred_watch: Still locked on preferred targets.");

        client_lock(cl);
    }
    else
        yaz_log(YLOG_WARN, "client_check_preferred_watch: %s. No session!", client_get_id(cl));

}

struct suggestions* client_suggestions_create(const char* suggestions_string);
static void client_suggestions_destroy(struct client *cl);

void client_search_response(struct client *cl)
{
    struct connection *co = cl->connection;
    ZOOM_connection link = connection_get_link(co);
    ZOOM_resultset resultset = cl->resultset;

    const char *error, *addinfo = 0;
    
    if (ZOOM_connection_error(link, &error, &addinfo))
    {
        cl->hits = 0;
        client_set_state(cl, Client_Error);
        yaz_log(YLOG_WARN, "Search error %s (%s): %s",
                error, addinfo, client_get_id(cl));
    }
    else
    {
        yaz_log(YLOG_DEBUG, "client_search_response: hits "
                ODR_INT_PRINTF, cl->hits);
        client_report_facets(cl, resultset);
        cl->record_offset = cl->startrecs;
        cl->hits = ZOOM_resultset_size(resultset);
        if (cl->suggestions)
            client_suggestions_destroy(cl);
        cl->suggestions = client_suggestions_create(ZOOM_resultset_option_get(resultset, "suggestions"));
    }
}

void client_got_records(struct client *cl)
{
    struct session *se = cl->session;
    if (se)
    {
        client_unlock(cl);
        session_alert_watch(se, SESSION_WATCH_SHOW);
        session_alert_watch(se, SESSION_WATCH_RECORD);
        client_lock(cl);
    }
}

static void client_record_ingest(struct client *cl)
{
    const char *msg, *addinfo;
    ZOOM_record rec = 0;
    ZOOM_resultset resultset = cl->resultset;
    int offset = cl->record_offset;
    if ((rec = ZOOM_resultset_record(resultset, offset)))
    {
        cl->record_offset++;
        if (cl->session == 0)
            ;
        else if (ZOOM_record_error(rec, &msg, &addinfo, 0))
        {
            yaz_log(YLOG_WARN, "Record error %s (%s): %s (rec #%d)",
                    msg, addinfo, client_get_id(cl),
                    cl->record_offset);
        }
        else
        {
            struct session_database *sdb = client_get_database(cl);
            NMEM nmem = nmem_create();
            const char *xmlrec;
            char type[80];
            
            if (nativesyntax_to_type(sdb, type, rec))
                yaz_log(YLOG_WARN, "Failed to determine record type");
            xmlrec = ZOOM_record_get(rec, type, NULL);
            if (!xmlrec)
                yaz_log(YLOG_WARN, "ZOOM_record_get failed from %s",
                        client_get_id(cl));
            else
            {
                /* OK = 0, -1 = failure, -2 = Filtered */
                if (ingest_record(cl, xmlrec, cl->record_offset, nmem) == -1)
                    yaz_log(YLOG_WARN, "Failed to ingest from %s", client_get_id(cl));
            }
            nmem_destroy(nmem);
        }
    }
    else
    {
        yaz_log(YLOG_WARN, "Expected record, but got NULL, offset=%d",
                offset);
    }
}

void client_record_response(struct client *cl)
{
    struct connection *co = cl->connection;
    ZOOM_connection link = connection_get_link(co);
    ZOOM_resultset resultset = cl->resultset;
    const char *error, *addinfo;

    if (ZOOM_connection_error(link, &error, &addinfo))
    {
        client_set_state(cl, Client_Error);
        yaz_log(YLOG_WARN, "Search error %s (%s): %s",
            error, addinfo, client_get_id(cl));
    }
    else
    {
        if (cl->show_raw && cl->show_raw->active)
        {
            ZOOM_record rec = 0;
            if ((rec = ZOOM_resultset_record(resultset,
                                             cl->show_raw->position-1)))
            {
                cl->show_raw->active = 0;
                ingest_raw_record(cl, rec);
            }
            else
            {
                yaz_log(YLOG_WARN, "Expected record, but got NULL, offset=%d",
                        cl->show_raw->position-1);
            }
        }
        else
        {
            client_record_ingest(cl);
        }
    }
}

void client_reingest(struct client *cl)
{
    int i = cl->startrecs;
    int to = cl->record_offset;

    cl->record_offset = i;
    for (; i < to; i++)
        client_record_ingest(cl);
}

static void client_set_facets_request(struct client *cl, ZOOM_connection link)
{
    struct session_database *sdb = client_get_database(cl);

    WRBUF w = wrbuf_alloc();
    
    struct setting *s;

    for (s = sdb->settings[PZ_FACETMAP]; s; s = s->next)
    {
        const char *p = strchr(s->name + 3, ':');
        if (!p)
        {
            yaz_log(YLOG_WARN, "Malformed facetmap name: %s", s->name);
        }
        else if (s->value && s->value[0])
        {
            wrbuf_puts(w, "@attr 1=");
            yaz_encode_pqf_term(w, s->value, strlen(s->value));
            if (s->next)
                wrbuf_puts(w, ",");
        }
    }
    yaz_log(YLOG_LOG, "using facets str: %s", wrbuf_cstr(w));
    ZOOM_connection_option_set(link, "facets",
                               wrbuf_len(w) ? wrbuf_cstr(w) : 0);
    wrbuf_destroy(w);
}

int client_has_facet(struct client *cl, const char *name)
{
    struct session_database *sdb = client_get_database(cl);
    struct setting *s;

    for (s = sdb->settings[PZ_FACETMAP]; s; s = s->next)
    {
        const char *p = strchr(s->name + 3, ':');
        if (p && !strcmp(name, p + 1))
            return 1;
    }
    return 0;
}

void client_start_search(struct client *cl, const char *sort_strategy_and_spec,
                         int increasing)
{
    struct session_database *sdb = client_get_database(cl);
    struct connection *co = client_get_connection(cl);
    ZOOM_connection link = connection_get_link(co);
    ZOOM_resultset rs;
    const char *opt_piggyback   = session_setting_oneval(sdb, PZ_PIGGYBACK);
    const char *opt_queryenc    = session_setting_oneval(sdb, PZ_QUERYENCODING);
    const char *opt_elements    = session_setting_oneval(sdb, PZ_ELEMENTS);
    const char *opt_requestsyn  = session_setting_oneval(sdb, PZ_REQUESTSYNTAX);
    const char *opt_maxrecs     = session_setting_oneval(sdb, PZ_MAXRECS);
    const char *opt_sru         = session_setting_oneval(sdb, PZ_SRU);
    const char *opt_sort        = session_setting_oneval(sdb, PZ_SORT);
    const char *opt_preferred   = session_setting_oneval(sdb, PZ_PREFERRED);
    const char *extra_args      = session_setting_oneval(sdb, PZ_EXTRA_ARGS);
    char maxrecs_str[24], startrecs_str[24];
    ZOOM_query q;

    assert(link);

    cl->hits = 0;
    cl->record_offset = 0;
    cl->diagnostic = 0;

    if (extra_args && *extra_args)
        ZOOM_connection_option_set(link, "extraArgs", extra_args);

    if (opt_preferred) {
        cl->preferred = atoi(opt_preferred);
        if (cl->preferred)
            yaz_log(YLOG_LOG, "Target %s has preferred status: %d",
                    client_get_id(cl), cl->preferred);
    }
    client_set_state(cl, Client_Working);

    if (*opt_piggyback)
        ZOOM_connection_option_set(link, "piggyback", opt_piggyback);
    else
        ZOOM_connection_option_set(link, "piggyback", "1");
    if (*opt_queryenc)
        ZOOM_connection_option_set(link, "rpnCharset", opt_queryenc);
    if (*opt_sru && *opt_elements)
        ZOOM_connection_option_set(link, "schema", opt_elements);
    else if (*opt_elements)
        ZOOM_connection_option_set(link, "elementSetName", opt_elements);
    if (*opt_requestsyn)
        ZOOM_connection_option_set(link, "preferredRecordSyntax", opt_requestsyn);

    if (opt_maxrecs && *opt_maxrecs)
    {
        cl->maxrecs = atoi(opt_maxrecs);
    }

    /* convert back to string representation used in ZOOM API */
    sprintf(maxrecs_str, "%d", cl->maxrecs);
    ZOOM_connection_option_set(link, "count", maxrecs_str);

    if (cl->maxrecs > 20)
        ZOOM_connection_option_set(link, "presentChunk", "20");
    else
        ZOOM_connection_option_set(link, "presentChunk", maxrecs_str);

    sprintf(startrecs_str, "%d", cl->startrecs);
    ZOOM_connection_option_set(link, "start", startrecs_str);

    /* TODO Verify does it break something for CQL targets(non-SOLR) ? */
    /* facets definition is in PQF */
    client_set_facets_request(cl, link);

    q = ZOOM_query_create();
    if (cl->cqlquery)
    {
        yaz_log(YLOG_LOG, "Search %s CQL: %s", client_get_id(cl),
                cl->cqlquery);
        ZOOM_query_cql(q, cl->cqlquery);
        if (*opt_sort)
            ZOOM_query_sortby(q, opt_sort);
    }
    else
    {
        yaz_log(YLOG_LOG, "Search %s PQF: %s", client_get_id(cl), cl->pquery);
        
        ZOOM_query_prefix(q, cl->pquery);
    }
    if (sort_strategy_and_spec &&
        strlen(sort_strategy_and_spec) < 40 /* spec below */)
    {
        char spec[50], *p;
        strcpy(spec, sort_strategy_and_spec);
        p = strchr(spec, ':');
        if (p)
        {
            *p++ = '\0'; /* cut the string in two */
            while (*p == ' ')
                p++;
            if (increasing)
                strcat(p, " <");
            else
                strcat(p, " >");
            yaz_log(YLOG_LOG, "applying %s %s", spec, p);
            ZOOM_query_sortby2(q, spec, p);
        }
    }
    rs = ZOOM_connection_search(link, q);
    ZOOM_query_destroy(q);
    ZOOM_resultset_destroy(cl->resultset);
    cl->resultset = rs;
    connection_continue(co);
}

struct client *client_create(const char *id)
{
    struct client *cl = xmalloc(sizeof(*cl));
    cl->maxrecs = 100;
    cl->startrecs = 0;
    cl->pquery = 0;
    cl->cqlquery = 0;
    cl->database = 0;
    cl->connection = 0;
    cl->session = 0;
    cl->hits = 0;
    cl->record_offset = 0;
    cl->diagnostic = 0;
    cl->state = Client_Disconnected;
    cl->show_raw = 0;
    cl->resultset = 0;
    cl->suggestions = 0;
    cl->mutex = 0;
    pazpar2_mutex_create(&cl->mutex, "client");
    cl->preferred = 0;
    cl->ref_count = 1;
    assert(id);
    cl->id = xstrdup(id);
    client_use(1);
    
    return cl;
}

void client_lock(struct client *c)
{
    yaz_mutex_enter(c->mutex);
}

void client_unlock(struct client *c)
{
    yaz_mutex_leave(c->mutex);
}

void client_incref(struct client *c)
{
    pazpar2_incref(&c->ref_count, c->mutex);
    yaz_log(YLOG_DEBUG, "client_incref c=%p %s cnt=%d",
            c, client_get_id(c), c->ref_count);
}

int client_destroy(struct client *c)
{
    if (c)
    {
        yaz_log(YLOG_DEBUG, "client_destroy c=%p %s cnt=%d",
                c, client_get_id(c), c->ref_count);
        if (!pazpar2_decref(&c->ref_count, c->mutex))
        {
            xfree(c->pquery);
            c->pquery = 0;
            xfree(c->cqlquery);
            c->cqlquery = 0;
            xfree(c->id);
            assert(!c->connection);

            if (c->resultset)
            {
                ZOOM_resultset_destroy(c->resultset);
            }
            yaz_mutex_destroy(&c->mutex);
            xfree(c);
            client_use(-1);
            return 1;
        }
    }
    return 0;
}

void client_set_connection(struct client *cl, struct connection *con)
{
    if (cl->resultset)
        ZOOM_resultset_release(cl->resultset);
    if (con)
    {
        assert(cl->connection == 0);
        cl->connection = con;
        client_incref(cl);
    }
    else
    {
        cl->connection = con;
        client_destroy(cl);
    }
}

void client_disconnect(struct client *cl)
{
    if (cl->state != Client_Idle)
        client_set_state(cl, Client_Disconnected);
    client_set_connection(cl, 0);
}


// Initialize CCL map for a target
static CCL_bibset prepare_cclmap(struct client *cl)
{
    struct session_database *sdb = client_get_database(cl);
    struct setting *s;
    CCL_bibset res;

    if (!sdb->settings)
        return 0;
    res = ccl_qual_mk();
    for (s = sdb->settings[PZ_CCLMAP]; s; s = s->next)
    {
        char *p = strchr(s->name + 3, ':');
        if (!p)
        {
            yaz_log(YLOG_WARN, "Malformed cclmap name: %s", s->name);
            ccl_qual_rm(&res);
            return 0;
        }
        p++;
        ccl_qual_fitem(res, s->value, p);
    }
    return res;
}

// returns a xmalloced CQL query corresponding to the pquery in client
static char *make_cqlquery(struct client *cl)
{
    cql_transform_t cqlt = cql_transform_create();
    Z_RPNQuery *zquery;
    char *r;
    WRBUF wrb = wrbuf_alloc();
    int status;
    ODR odr_out = odr_createmem(ODR_ENCODE);

    zquery = p_query_rpn(odr_out, cl->pquery);
    yaz_log(YLOG_LOG, "PQF: %s", cl->pquery);
    if ((status = cql_transform_rpn2cql_wrbuf(cqlt, wrb, zquery)))
    {
        yaz_log(YLOG_WARN, "Failed to generate CQL query, code=%d", status);
        r = 0;
    }
    else
    {
        r = xstrdup(wrbuf_cstr(wrb));
    }     
    wrbuf_destroy(wrb);
    odr_destroy(odr_out);
    cql_transform_close(cqlt);
    return r;
}

// returns a xmalloced SOLR query corresponding to the pquery in client
// TODO Could prob. be merge with the similar make_cqlquery
static char *make_solrquery(struct client *cl)
{
    solr_transform_t sqlt = solr_transform_create();
    Z_RPNQuery *zquery;
    char *r;
    WRBUF wrb = wrbuf_alloc();
    int status;
    ODR odr_out = odr_createmem(ODR_ENCODE);

    zquery = p_query_rpn(odr_out, cl->pquery);
    if (zquery == 0) {
        yaz_log(YLOG_WARN, "Failed to generate RPN from PQF: %s", cl->pquery);
        return 0;
    }
    yaz_log(YLOG_LOG, "PQF: %s", cl->pquery);
    if ((status = solr_transform_rpn2solr_wrbuf(sqlt, wrb, zquery)))
    {
        yaz_log(YLOG_WARN, "Failed to generate SOLR query from PQF %s, code=%d", cl->pquery, status);
        r = 0;
    }
    else
    {
        r = xstrdup(wrbuf_cstr(wrb));
    }
    wrbuf_destroy(wrb);
    odr_destroy(odr_out);
    solr_transform_close(sqlt);
    return r;
}

static void apply_limit(struct session_database *sdb,
                        facet_limits_t facet_limits,
                        WRBUF w_pqf, WRBUF w_ccl)
{
    int i = 0;
    const char *name;
    const char *value;
    NMEM nmem_tmp = nmem_create();
    for (i = 0; (name = facet_limits_get(facet_limits, i, &value)); i++)
    {
        struct setting *s = 0;
        
        for (s = sdb->settings[PZ_LIMITMAP]; s; s = s->next)
        {
            const char *p = strchr(s->name + 3, ':');
            if (p && !strcmp(p + 1, name) && s->value)
            {
                char **values = 0;
                int i, num = 0;
                nmem_strsplit_escape2(nmem_tmp, "|", value, &values,
                                      &num, 1, '\\', 1);

                if (!strncmp(s->value, "rpn:", 4))
                {
                    const char *pqf = s->value + 4;

                    wrbuf_puts(w_pqf, "@and ");
                    wrbuf_puts(w_pqf, pqf);
                    wrbuf_puts(w_pqf, " ");
                    for (i = 0; i < num; i++)
                    {
                        if (i < num - 1)
                            wrbuf_puts(w_pqf, "@or ");
                        yaz_encode_pqf_term(w_pqf, values[i],
                                            strlen(values[i]));
                    }
                }
                else if (!strncmp(s->value, "ccl:", 4))
                {
                    const char *ccl = s->value + 4;

                    wrbuf_puts(w_ccl, " and (");

                    for (i = 0; i < num; i++)
                    {
                        if (i)
                            wrbuf_puts(w_ccl, " or ");
                        wrbuf_puts(w_ccl, ccl);
                        wrbuf_puts(w_ccl, "=\"");
                        wrbuf_puts(w_ccl, values[i]);
                        wrbuf_puts(w_ccl, "\"");
                    }
                    wrbuf_puts(w_ccl, ")");

                }
                break;
            }
        }
        nmem_reset(nmem_tmp);
        if (!s)
        {
            yaz_log(YLOG_WARN, "Target %s: limit %s used, but no limitmap defined",
                    (sdb->database ? sdb->database->id : "<no id>"), name);
        }
    }
    nmem_destroy(nmem_tmp);
}
                        
// Parse the query given the settings specific to this client
int client_parse_query(struct client *cl, const char *query,
                       facet_limits_t facet_limits,
                       const char *startrecs, const char *maxrecs)
{
    struct session *se = client_get_session(cl);
    struct session_database *sdb = client_get_database(cl);
    struct ccl_rpn_node *cn;
    int cerror, cpos;
    CCL_bibset ccl_map = prepare_cclmap(cl);
    const char *sru = session_setting_oneval(sdb, PZ_SRU);
    const char *pqf_prefix = session_setting_oneval(sdb, PZ_PQF_PREFIX);
    const char *pqf_strftime = session_setting_oneval(sdb, PZ_PQF_STRFTIME);
    const char *query_syntax = session_setting_oneval(sdb, PZ_QUERY_SYNTAX);
    WRBUF w_ccl, w_pqf;
    int ret_value = 1;

    if (!ccl_map)
        return -1;


    if (maxrecs && atoi(maxrecs) != cl->maxrecs)
    {
        ret_value = 0;
        cl->maxrecs = atoi(maxrecs);
    }

    if (startrecs && atoi(startrecs) != cl->startrecs)
    {
        ret_value = 0;
        cl->startrecs = atoi(startrecs);
    }

    w_ccl = wrbuf_alloc();
    wrbuf_puts(w_ccl, query);

    w_pqf = wrbuf_alloc();
    if (*pqf_prefix)
    {
        wrbuf_puts(w_pqf, pqf_prefix);
        wrbuf_puts(w_pqf, " ");
    }

    apply_limit(sdb, facet_limits, w_pqf, w_ccl);

    yaz_log(YLOG_LOG, "CCL query: %s", wrbuf_cstr(w_ccl));
    cn = ccl_find_str(ccl_map, wrbuf_cstr(w_ccl), &cerror, &cpos);
    ccl_qual_rm(&ccl_map);
    if (!cn)
    {
        client_set_state(cl, Client_Error);
        session_log(se, YLOG_WARN, "Failed to parse CCL query '%s' for %s",
                    wrbuf_cstr(w_ccl),
                    client_get_id(cl));
        wrbuf_destroy(w_ccl);
        wrbuf_destroy(w_pqf);
        return -1;
    }
    wrbuf_destroy(w_ccl);

    if (!pqf_strftime || !*pqf_strftime)
        ccl_pquery(w_pqf, cn);
    else
    {
        time_t cur_time = time(0);
        struct tm *tm =  localtime(&cur_time);
        char tmp_str[300];
        const char *cp = tmp_str;

        /* see man strftime(3) for things .. In particular %% gets converted
         to %.. And That's our original query .. */
        strftime(tmp_str, sizeof(tmp_str)-1, pqf_strftime, tm);
        for (; *cp; cp++)
        {
            if (cp[0] == '%')
                ccl_pquery(w_pqf, cn);
            else
                wrbuf_putc(w_pqf, cp[0]);
        }
    }
    if (!cl->pquery || strcmp(cl->pquery, wrbuf_cstr(w_pqf)))
    {
        xfree(cl->pquery);
        cl->pquery = xstrdup(wrbuf_cstr(w_pqf));
        ret_value = 0;
    }
    wrbuf_destroy(w_pqf);

    yaz_log(YLOG_LOG, "PQF query: %s", cl->pquery);

    xfree(cl->cqlquery);

    /* Support for PQF on SRU targets. */
    /* TODO Refactor */
    yaz_log(YLOG_DEBUG, "Query syntax: %s", query_syntax);
    if (strcmp(query_syntax, "pqf") != 0 && *sru)
    {
        if (!strcmp(sru, "solr")) {
            if (!(cl->cqlquery = make_solrquery(cl)))
                return -1;
        }
        else {
            if (!(cl->cqlquery = make_cqlquery(cl)))
                return -1;
        }
    }
    else
        cl->cqlquery = 0;

    /* TODO FIX Not thread safe */
    if (!se->relevance)
    {
        // Initialize relevance structure with query terms
        se->relevance = relevance_create_ccl(
            se->service->charsets, se->nmem, cn);
    }

    ccl_rpn_delete(cn);
    return ret_value;
}

void client_set_session(struct client *cl, struct session *se)
{
    cl->session = se;
}

int client_is_active(struct client *cl)
{
    if (cl->connection && (cl->state == Client_Connecting ||
                           cl->state == Client_Working))
        return 1;
    return 0;
}

int client_is_active_preferred(struct client *cl)
{
    /* only count if this is a preferred target. */
    if (!cl->preferred)
        return 0;
    /* TODO No sure this the condition that Seb wants */
    if (cl->connection && (cl->state == Client_Connecting ||
                           cl->state == Client_Working))
        return 1;
    return 0;
}

Odr_int client_get_hits(struct client *cl)
{
    return cl->hits;
}

int client_get_num_records(struct client *cl)
{
    return cl->record_offset;
}

void client_set_diagnostic(struct client *cl, int diagnostic)
{
    cl->diagnostic = diagnostic;
}

int client_get_diagnostic(struct client *cl)
{
    return cl->diagnostic;
}

const char * client_get_suggestions_xml(struct client *cl, WRBUF wrbuf)
{
    /* int idx; */
    struct suggestions *suggestions = cl->suggestions;

    if (!suggestions) {
        yaz_log(YLOG_DEBUG, "No suggestions found");
        return "";
    }
    if (suggestions->passthrough) {
        yaz_log(YLOG_DEBUG, "Passthrough Suggestions: \n%s\n", suggestions->passthrough);
        return suggestions->passthrough;
    }
    if (suggestions->num == 0) {
        return "";
    }
    /*
    for (idx = 0; idx < suggestions->num; idx++) {
        wrbuf_printf(wrbuf, "<suggest term=\"%s\"", suggestions->suggest[idx]);
        if (suggestions->misspelled[idx] && suggestions->misspelled[idx]) {
            wrbuf_puts(wrbuf, suggestions->misspelled[idx]);
            wrbuf_puts(wrbuf, "</suggest>\n");
        }
        else
            wrbuf_puts(wrbuf, "/>\n");
    }
    */
    return wrbuf_cstr(wrbuf);
}


void client_set_database(struct client *cl, struct session_database *db)
{
    cl->database = db;
}

const char *client_get_id(struct client *cl)
{
    return cl->id;
}

int client_get_maxrecs(struct client *cl)
{
    return cl->maxrecs;
}

void client_set_preferred(struct client *cl, int v)
{
    cl->preferred = v;
}


struct suggestions* client_suggestions_create(const char* suggestions_string)
{
    int i;
    NMEM nmem;
    struct suggestions *suggestions;
    if (suggestions_string == 0)
        return 0;
    nmem = nmem_create();
    suggestions = nmem_malloc(nmem, sizeof(*suggestions));
    yaz_log(YLOG_DEBUG, "client target suggestions: %s", suggestions_string);

    suggestions->nmem = nmem;
    suggestions->num = 0;
    suggestions->misspelled = 0;
    suggestions->suggest = 0;
    suggestions->passthrough = nmem_strdup_null(nmem, suggestions_string);

    if (suggestions_string)
        nmem_strsplit_escape2(suggestions->nmem, "\n", suggestions_string, &suggestions->suggest,
                              &suggestions->num, 1, '\\', 0);
    /* Set up misspelled array */
    suggestions->misspelled = (char **) nmem_malloc(nmem, suggestions->num * sizeof(**suggestions->misspelled));
    /* replace = with \0 .. for each item */
    for (i = 0; i < suggestions->num; i++)
    {
        char *cp = strchr(suggestions->suggest[i], '=');
        if (cp) {
            *cp = '\0';
            suggestions->misspelled[i] = cp+1;
        }
    }
    return suggestions;
}

static void client_suggestions_destroy(struct client *cl)
{
    NMEM nmem = cl->suggestions->nmem;
    cl->suggestions = 0;
    nmem_destroy(nmem);
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

