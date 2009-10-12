/* This file is part of Pazpar2.
   Copyright (C) 2006-2009 Index Data

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

#define USE_TIMING 0
#if USE_TIMING
#include <yaz/timing.h>
#endif

#include "pazpar2.h"
#include "parameters.h"
#include "client.h"
#include "connection.h"
#include "settings.h"
#include "relevance.h"

/** \brief Represents client state for a connection to one search target */
struct client {
    struct session_database *database;
    struct connection *connection;
    struct session *session;
    char *pquery; // Current search
    char *cqlquery; // used for SRU targets only
    int hits;
    int record_offset;
    int maxrecs;
    int diagnostic;
    enum client_state state;
    struct show_raw *show_raw;
    struct client *next;     // next client in session or next in free list
    ZOOM_resultset resultset;
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

static struct client *client_freelist = 0; /* thread pr */

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
    cl->state = st;
    if (cl->session)
    {
        int no_active = session_active_clients(cl->session);
        if (no_active == 0)
            session_alert_watch(cl->session, SESSION_WATCH_SHOW);
    }
}

static void client_show_raw_error(struct client *cl, const char *addinfo);

// Close connection and set state to error
void client_fatal(struct client *cl)
{
    yaz_log(YLOG_WARN, "Fatal error from %s", client_get_url(cl));
    connection_destroy(cl->connection);
    client_set_state(cl, Client_Error);
}

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
            client_get_url(cl), 1, offset);

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

void client_search_response(struct client *cl)
{
    struct connection *co = cl->connection;
    struct session *se = cl->session;
    ZOOM_connection link = connection_get_link(co);
    ZOOM_resultset resultset = cl->resultset;
    const char *error, *addinfo;

    if (ZOOM_connection_error(link, &error, &addinfo))
    {
        cl->hits = 0;
        client_set_state(cl, Client_Error);
        yaz_log(YLOG_WARN, "Search error %s (%s): %s",
            error, addinfo, client_get_url(cl));
    }
    else
    {
        cl->record_offset = 0;
        cl->hits = ZOOM_resultset_size(resultset);
        se->total_hits += cl->hits;
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
            error, addinfo, client_get_url(cl));
    }
    else
    {
        ZOOM_record rec = 0;
        const char *msg, *addinfo;
        
        if (cl->show_raw && cl->show_raw->active)
        {
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
            int offset = cl->record_offset;
            if ((rec = ZOOM_resultset_record(resultset, offset)))
            {
                cl->record_offset++;
                if (ZOOM_record_error(rec, &msg, &addinfo, 0))
                    yaz_log(YLOG_WARN, "Record error %s (%s): %s (rec #%d)",
                            error, addinfo, client_get_url(cl),
                            cl->record_offset);
                else
                {
                    struct session_database *sdb = client_get_database(cl);
                    const char *xmlrec;
                    char type[80];
                    if (nativesyntax_to_type(sdb, type, rec))
                        yaz_log(YLOG_WARN, "Failed to determine record type");
                    if ((xmlrec = ZOOM_record_get(rec, type, NULL)))
                    {
                        if (ingest_record(cl, xmlrec, cl->record_offset))
                        {
                            session_alert_watch(cl->session, SESSION_WATCH_SHOW);
                            session_alert_watch(cl->session, SESSION_WATCH_RECORD);
                        }
                        else
                            yaz_log(YLOG_WARN, "Failed to ingest");
                    }
                    else
                        yaz_log(YLOG_WARN, "Failed to extract ZOOM record");
                }

            }
            else
            {
                yaz_log(YLOG_WARN, "Expected record, but got NULL, offset=%d",
                        offset);
            }
        }
    }
}

void client_start_search(struct client *cl)
{
    struct session_database *sdb = client_get_database(cl);
    struct connection *co = client_get_connection(cl);
    ZOOM_connection link = connection_get_link(co);
    ZOOM_resultset rs;
    char *databaseName = sdb->database->databases[0];
    const char *opt_piggyback = session_setting_oneval(sdb, PZ_PIGGYBACK);
    const char *opt_queryenc = session_setting_oneval(sdb, PZ_QUERYENCODING);
    const char *opt_elements = session_setting_oneval(sdb, PZ_ELEMENTS);
    const char *opt_requestsyn = session_setting_oneval(sdb, PZ_REQUESTSYNTAX);
    const char *opt_maxrecs = session_setting_oneval(sdb, PZ_MAXRECS);
    const char *opt_sru = session_setting_oneval(sdb, PZ_SRU);
    const char *opt_sort = session_setting_oneval(sdb, PZ_SORT);
    char maxrecs_str[24];

    assert(link);

    cl->hits = -1;
    cl->record_offset = 0;
    cl->diagnostic = 0;
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

    if (!*opt_maxrecs)
    {
        sprintf(maxrecs_str, "%d", cl->maxrecs);
        opt_maxrecs = maxrecs_str;
    }
    ZOOM_connection_option_set(link, "count", opt_maxrecs);

    if (atoi(opt_maxrecs) > 20)
        ZOOM_connection_option_set(link, "presentChunk", "20");
    else
        ZOOM_connection_option_set(link, "presentChunk", opt_maxrecs);
        
    if (databaseName)
        ZOOM_connection_option_set(link, "databaseName", databaseName);

    if (cl->cqlquery)
    {
        ZOOM_query q = ZOOM_query_create();
        yaz_log(YLOG_LOG, "Search %s CQL: %s", sdb->database->url, cl->cqlquery);
        ZOOM_query_cql(q, cl->cqlquery);
	if (*opt_sort)
	    ZOOM_query_sortby(q, opt_sort);
        rs = ZOOM_connection_search(link, q);
        ZOOM_query_destroy(q);
    }
    else
    {
        yaz_log(YLOG_LOG, "Search %s PQF: %s", sdb->database->url, cl->pquery);
        rs = ZOOM_connection_search_pqf(link, cl->pquery);
    }
    ZOOM_resultset_destroy(cl->resultset);
    cl->resultset = rs;
    connection_continue(co);
}

struct client *client_create(void)
{
    struct client *r;
    if (client_freelist)
    {
        r = client_freelist;
        client_freelist = client_freelist->next;
    }
    else
        r = xmalloc(sizeof(struct client));
    r->maxrecs = 100;
    r->pquery = 0;
    r->cqlquery = 0;
    r->database = 0;
    r->connection = 0;
    r->session = 0;
    r->hits = 0;
    r->record_offset = 0;
    r->diagnostic = 0;
    r->state = Client_Disconnected;
    r->show_raw = 0;
    r->resultset = 0;
    r->next = 0;
    return r;
}

void client_destroy(struct client *c)
{
    struct session *se = c->session;
    if (c == se->clients)
        se->clients = c->next;
    else
    {
        struct client *cc;
        for (cc = se->clients; cc && cc->next != c; cc = cc->next)
            ;
        if (cc)
            cc->next = c->next;
    }
    xfree(c->pquery);
    xfree(c->cqlquery);

    if (c->connection)
        connection_release(c->connection);

    ZOOM_resultset_destroy(c->resultset);
    c->resultset = 0;
    c->next = client_freelist;
    client_freelist = c;
}

void client_set_connection(struct client *cl, struct connection *con)
{
    cl->connection = con;
}

void client_disconnect(struct client *cl)
{
    if (cl->state != Client_Idle)
        client_set_state(cl, Client_Disconnected);
    client_set_connection(cl, 0);
}

// Extract terms from query into null-terminated termlist
static void extract_terms(NMEM nmem, struct ccl_rpn_node *query, char **termlist)
{
    int num = 0;

    pull_terms(nmem, query, termlist, &num);
    termlist[num] = 0;
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

// Parse the query given the settings specific to this client
int client_parse_query(struct client *cl, const char *query)
{
    struct session *se = client_get_session(cl);
    struct session_database *sdb = client_get_database(cl);
    struct ccl_rpn_node *cn;
    int cerror, cpos;
    CCL_bibset ccl_map = prepare_cclmap(cl);
    const char *sru = session_setting_oneval(sdb, PZ_SRU);
    const char *pqf_prefix = session_setting_oneval(sdb, PZ_PQF_PREFIX);

    if (!ccl_map)
        return -1;

    cn = ccl_find_str(ccl_map, query, &cerror, &cpos);
    ccl_qual_rm(&ccl_map);
    if (!cn)
    {
        client_set_state(cl, Client_Error);
        yaz_log(YLOG_WARN, "Failed to parse CCL query %s for %s",
                query,
                client_get_database(cl)->database->url);
        return -1;
    }
    wrbuf_rewind(se->wrbuf);
    if (*pqf_prefix)
    {
        wrbuf_puts(se->wrbuf, pqf_prefix);
        wrbuf_puts(se->wrbuf, " ");
    }
    ccl_pquery(se->wrbuf, cn);
    xfree(cl->pquery);
    cl->pquery = xstrdup(wrbuf_cstr(se->wrbuf));

    xfree(cl->cqlquery);
    if (*sru)
    {
        if (!(cl->cqlquery = make_cqlquery(cl)))
            return -1;
    }
    else
        cl->cqlquery = 0;

    if (!se->relevance)
    {
        // Initialize relevance structure with query terms
        char *p[512];
        extract_terms(se->nmem, cn, p);
        se->relevance = relevance_create(
            se->service->relevance_pct,
            se->nmem, (const char **) p);
    }

    ccl_rpn_delete(cn);
    return 0;
}

void client_set_session(struct client *cl, struct session *se)
{
    cl->session = se;
    cl->next = se->clients;
    se->clients = cl;
}

int client_is_active(struct client *cl)
{
    if (cl->connection && (cl->state == Client_Connecting ||
                           cl->state == Client_Working))
        return 1;
    return 0;
}

struct client *client_next_in_session(struct client *cl)
{
    if (cl)
        return cl->next;
    return 0;

}

int client_get_hits(struct client *cl)
{
    return cl->hits;
}

int client_get_num_records(struct client *cl)
{
    return cl->record_offset;
}

int client_get_diagnostic(struct client *cl)
{
    return cl->diagnostic;
}

void client_set_database(struct client *cl, struct session_database *db)
{
    cl->database = db;
}

struct host *client_get_host(struct client *cl)
{
    return client_get_database(cl)->database->host;
}

const char *client_get_url(struct client *cl)
{
    return client_get_database(cl)->database->url;
}

void client_set_maxrecs(struct client *cl, int v)
{
    cl->maxrecs = v;
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

