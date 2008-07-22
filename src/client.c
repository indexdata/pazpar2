/* This file is part of Pazpar2.
   Copyright (C) 2006-2008 Index Data

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
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
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
#include <yaz/diagbib1.h>
#include <yaz/snprintf.h>

#define USE_TIMING 0
#if USE_TIMING
#include <yaz/timing.h>
#endif

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include "pazpar2.h"

#include "client.h"
#include "connection.h"
#include "settings.h"

/** \brief Represents client state for a connection to one search target */
struct client {
    struct session_database *database;
    struct connection *connection;
    struct session *session;
    char *pquery; // Current search
    int hits;
    int records;
    int setno;
    int requestid;            // ID of current outstanding request
    int diagnostic;
    enum client_state state;
    struct show_raw *show_raw;
    struct client *next;     // next client in session or next in free list
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
    "Client_Connected",
    "Client_Idle",
    "Client_Initializing",
    "Client_Searching",
    "Client_Presenting",
    "Client_Error",
    "Client_Failed",
    "Client_Disconnected",
    "Client_Stopped",
    "Client_Continue"
};

static struct client *client_freelist = 0;

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
    //client_show_raw_error(cl, "client connection failure");
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

void client_set_requestid(struct client *cl, int id)
{
    cl->requestid = id;
}


static void client_send_raw_present(struct client *cl);

int client_show_raw_begin(struct client *cl, int position,
                          const char *syntax, const char *esn,
                          void *data,
                          void (*error_handler)(void *data, const char *addinfo),
                          void (*record_handler)(void *data, const char *buf,
                                                 size_t sz),
                          void **data2,
                          int binary)
{
    struct show_raw *rr, **rrp;
    if (!cl->connection)
    {   /* the client has no connection */
        return -1;
    }
    rr = xmalloc(sizeof(*rr));
    *data2 = rr;
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
    return 0;
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
        xfree(rr);
    }
}

void client_show_raw_dequeue(struct client *cl)
{
    struct show_raw *rr = cl->show_raw;

    cl->show_raw = rr->next;
    xfree(rr);
}

static void client_show_raw_error(struct client *cl, const char *addinfo)
{
    while (cl->show_raw)
    {
        cl->show_raw->error_handler(cl->show_raw->data, addinfo);
        client_show_raw_dequeue(cl);
    }
}

static void client_show_raw_cancel(struct client *cl)
{
    while (cl->show_raw)
    {
        cl->show_raw->error_handler(cl->show_raw->data, "cancel");
        client_show_raw_dequeue(cl);
    }
}

static void client_send_raw_present(struct client *cl)
{
    struct session_database *sdb = client_get_database(cl);
    struct connection *co = client_get_connection(cl);
    ZOOM_resultset set = connection_get_resultset(co);

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

static int nativesyntax_to_type(struct session_database *sdb, char *type)
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
        yaz_log(YLOG_LOG, "Returned type %s", type);
        return 0;
    }
    return -1;
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
        nativesyntax_to_type(sdb, type);
    }

    buf = ZOOM_record_get(rec, type, &len);
    cl->show_raw->record_handler(cl->show_raw->data,  buf, len);
    client_show_raw_dequeue(cl);
}

#ifdef RETIRED

static void ingest_raw_records(struct client *cl, Z_Records *r)
{
    Z_NamePlusRecordList *rlist;
    Z_NamePlusRecord *npr;
    xmlDoc *doc;
    xmlChar *buf_out;
    int len_out;
    if (r->which != Z_Records_DBOSD)
    {
        client_show_raw_error(cl, "non-surrogate diagnostics");
        return;
    }

    rlist = r->u.databaseOrSurDiagnostics;
    if (rlist->num_records != 1 || !rlist->records || !rlist->records[0])
    {
        client_show_raw_error(cl, "no records");
        return;
    }
    npr = rlist->records[0];
    if (npr->which != Z_NamePlusRecord_databaseRecord)
    {
        client_show_raw_error(cl, "surrogate diagnostic");
        return;
    }

    if (cl->show_raw && cl->show_raw->binary)
    {
        Z_External *rec = npr->u.databaseRecord;
        if (rec->which == Z_External_octet)
        {
            cl->show_raw->record_handler(cl->show_raw->data,
                                         (const char *)
                                         rec->u.octet_aligned->buf,
                                         rec->u.octet_aligned->len);
            client_show_raw_dequeue(cl);
        }
        else
            client_show_raw_error(cl, "no records");
    }

    doc = record_to_xml(client_get_database(cl), npr->u.databaseRecord);
    if (!doc)
    {
        client_show_raw_error(cl, "unable to convert record to xml");
        return;
    }

    xmlDocDumpMemory(doc, &buf_out, &len_out);
    xmlFreeDoc(doc);

    if (cl->show_raw)
    {
        cl->show_raw->record_handler(cl->show_raw->data,
                                     (const char *) buf_out, len_out);
        client_show_raw_dequeue(cl);
    }
    xmlFree(buf_out);
}

#endif // RETIRED show raw

void client_search_response(struct client *cl)
{
    struct connection *co = cl->connection;
    struct session *se = cl->session;
    ZOOM_connection link = connection_get_link(co);
    ZOOM_resultset resultset = connection_get_resultset(co);
    const char *error, *addinfo;

    if (ZOOM_connection_error(link, &error, &addinfo))
    {
        cl->hits = 0;
        cl->state = Client_Error;
        yaz_log(YLOG_WARN, "Search error %s (%s): %s",
            error, addinfo, client_get_url(cl));
    }
    else
    {
        cl->hits = ZOOM_resultset_size(resultset);
        se->total_hits += cl->hits;
    }
}


void client_record_response(struct client *cl)
{
    struct connection *co = cl->connection;
    ZOOM_connection link = connection_get_link(co);
    ZOOM_resultset resultset = connection_get_resultset(co);
    const char *error, *addinfo;

    yaz_log(YLOG_LOG, "client_record_response");
    if (ZOOM_connection_error(link, &error, &addinfo))
    {
        cl->state = Client_Error;
        yaz_log(YLOG_WARN, "Search error %s (%s): %s",
            error, addinfo, client_get_url(cl));
    }
    else
    {
        ZOOM_record rec = 0;
        const char *msg, *addinfo;
        
        yaz_log(YLOG_LOG, "show_raw=%p show_raw->active=%d",
                cl->show_raw, cl->show_raw ? cl->show_raw->active : 0);
        if (cl->show_raw && cl->show_raw->active)
        {
            if ((rec = ZOOM_resultset_record(resultset,
                                             cl->show_raw->position-1)))
            {
                cl->show_raw->active = 0;
                ingest_raw_record(cl, rec);
            }
        }
        else
        {
            int offset = cl->records;
            if ((rec = ZOOM_resultset_record(resultset, offset)))
            {
                yaz_log(YLOG_LOG, "Record with offset %d", offset);
                
                cl->records++;
                if (ZOOM_record_error(rec, &msg, &addinfo, 0))
                    yaz_log(YLOG_WARN, "Record error %s (%s): %s (rec #%d)",
                            error, addinfo, client_get_url(cl), cl->records);
                else
                {
                    struct session_database *sdb = client_get_database(cl);
                    const char *xmlrec;
                    char type[80];
                    nativesyntax_to_type(sdb, type);
                    if ((xmlrec = ZOOM_record_get(rec, type, NULL)))
                    {
                        if (ingest_record(cl, xmlrec, cl->records))
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
        }
        if (!rec)
            yaz_log(YLOG_WARN, "Expected record, but got NULL");
    }
}

#ifdef RETIRED

void client_present_response(struct client *cl, Z_APDU *a)
{
    Z_PresentResponse *r = a->u.presentResponse;
    Z_Records *recs = r->records;
        
    if (recs && recs->which == Z_Records_NSD)
    {
        WRBUF w = wrbuf_alloc();
        
        Z_DiagRec dr, *dr_p = &dr;
        dr.which = Z_DiagRec_defaultFormat;
        dr.u.defaultFormat = recs->u.nonSurrogateDiagnostic;
        
        wrbuf_printf(w, "Present response NSD %s: ",
                     cl->database->database->url);
        
        cl->diagnostic = diag_to_wrbuf(&dr_p, 1, w);
        
        yaz_log(YLOG_WARN, "%s", wrbuf_cstr(w));
        
        cl->state = Client_Error;
        wrbuf_destroy(w);

        client_show_raw_error(cl, "non surrogate diagnostics");
    }
    else if (recs && recs->which == Z_Records_multipleNSD)
    {
        WRBUF w = wrbuf_alloc();
        
        wrbuf_printf(w, "Present response multipleNSD %s: ",
                     cl->database->database->url);
        cl->diagnostic = 
            diag_to_wrbuf(recs->u.multipleNonSurDiagnostics->diagRecs,
                          recs->u.multipleNonSurDiagnostics->num_diagRecs,
                          w);
        yaz_log(YLOG_WARN, "%s", wrbuf_cstr(w));
        cl->state = Client_Error;
        wrbuf_destroy(w);
    }
    else if (recs && !*r->presentStatus && cl->state != Client_Error)
    {
        yaz_log(YLOG_DEBUG, "Good Present response %s",
                cl->database->database->url);

        // we can mix show raw and normal show ..
        if (cl->show_raw && cl->show_raw->active)
        {
            cl->show_raw->active = 0; // no longer active
            ingest_raw_records(cl, recs);
        }
        else
            ingest_records(cl, recs);
        cl->state = Client_Continue;
    }
    else if (*r->presentStatus) 
    {
        yaz_log(YLOG_WARN, "Bad Present response %s",
                cl->database->database->url);
        cl->state = Client_Error;
        client_show_raw_error(cl, "bad present response");
    }
}

void client_close_response(struct client *cl, Z_APDU *a)
{
    struct connection *co = cl->connection;
    /* Z_Close *r = a->u.close; */

    yaz_log(YLOG_WARN, "Close response %s", cl->database->database->url);

    cl->state = Client_Failed;
    connection_destroy(co);
}

#endif // RETIRED show raw

#ifdef RETIRED
int client_is_our_response(struct client *cl)
{
    struct session *se = client_get_session(cl);

    if (cl && (cl->requestid == se->requestid || 
               cl->state == Client_Initializing))
        return 1;
    return 0;
}
#endif

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

    assert(link);

    cl->hits = -1;
    cl->records = 0;
    cl->diagnostic = 0;

    if (*opt_piggyback)
        ZOOM_connection_option_set(link, "piggyback", opt_piggyback);
    else
        ZOOM_connection_option_set(link, "piggyback", "1");
    if (*opt_queryenc)
        ZOOM_connection_option_set(link, "rpnCharset", opt_queryenc);
    if (*opt_elements)
        ZOOM_connection_option_set(link, "elementSetName", opt_elements);
    if (*opt_requestsyn)
        ZOOM_connection_option_set(link, "preferredRecordSyntax", opt_requestsyn);
    if (*opt_maxrecs)
        ZOOM_connection_option_set(link, "count", opt_maxrecs);
    else
    {
        char n[128];
        sprintf(n, "%d", global_parameters.toget);
        ZOOM_connection_option_set(link, "count", n);
    }
    if (!databaseName || !*databaseName)
        databaseName = "Default";
    ZOOM_connection_option_set(link, "databaseName", databaseName);

    ZOOM_connection_option_set(link, "presentChunk", "20");

    rs = ZOOM_connection_search_pqf(link, cl->pquery);
    connection_set_resultset(co, rs);
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
    r->pquery = 0;
    r->database = 0;
    r->connection = 0;
    r->session = 0;
    r->hits = 0;
    r->records = 0;
    r->setno = 0;
    r->requestid = -1;
    r->diagnostic = 0;
    r->state = Client_Disconnected;
    r->show_raw = 0;
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

    if (c->connection)
        connection_release(c->connection);
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

// Parse the query given the settings specific to this client
int client_parse_query(struct client *cl, const char *query)
{
    struct session *se = client_get_session(cl);
    struct ccl_rpn_node *cn;
    int cerror, cpos;
    CCL_bibset ccl_map = prepare_cclmap(cl);

    if (!ccl_map)
        return -1;

    cn = ccl_find_str(ccl_map, query, &cerror, &cpos);
    ccl_qual_rm(&ccl_map);
    if (!cn)
    {
        cl->state = Client_Error;
        yaz_log(YLOG_WARN, "Failed to parse query for %s",
                         client_get_database(cl)->database->url);
        return -1;
    }
    wrbuf_rewind(se->wrbuf);
    ccl_pquery(se->wrbuf, cn);
    xfree(cl->pquery);
    cl->pquery = xstrdup(wrbuf_cstr(se->wrbuf));

    if (!se->relevance)
    {
        // Initialize relevance structure with query terms
        char *p[512];
        extract_terms(se->nmem, cn, p);
        se->relevance = relevance_create(
            global_parameters.server->relevance_pct,
            se->nmem, (const char **) p,
            se->expected_maxrecs);
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
    if (cl->connection && (cl->state == Client_Continue ||
                           cl->state == Client_Connecting ||
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
    return cl->records;
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

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
