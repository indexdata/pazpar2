/* $Id: client.c,v 1.12 2007-06-19 12:25:29 adam Exp $
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

/** \file client.c
    \brief Z39.50 client 
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
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

#if HAVE_CONFIG_H
#include "cconfig.h"
#endif

#define USE_TIMING 0
#if USE_TIMING
#include <yaz/timing.h>
#endif

#include <netinet/in.h>

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
    char *syntax;
    char *esn;
    void (*error_handler)(void *data, const char *addinfo);
    void (*record_handler)(void *data, const char *buf, size_t sz);
    void *data;
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
    "Client_Stopped"
};

static struct client *client_freelist = 0;

static int send_apdu(struct client *c, Z_APDU *a)
{
    return connection_send_apdu(client_get_connection(c), a);
}


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
}

static void client_show_raw_error(struct client *cl, const char *addinfo);

// Close connection and set state to error
void client_fatal(struct client *cl)
{
    client_show_raw_error(cl, "client connection failure");
    yaz_log(YLOG_WARN, "Fatal error from %s", client_get_url(cl));
    connection_destroy(cl->connection);
    cl->state = Client_Error;
}


static int diag_to_wrbuf(Z_DiagRec **pp, int num, WRBUF w)
{
    int code = 0;
    int i;
    for (i = 0; i<num; i++)
    {
        Z_DiagRec *p = pp[i];
        if (i)
            wrbuf_puts(w, "; ");
        if (p->which != Z_DiagRec_defaultFormat)
        {
            wrbuf_puts(w, "? Not in default format");
        }
        else
        {
            Z_DefaultDiagFormat *r = p->u.defaultFormat;
            
            if (!r->diagnosticSetId)
                wrbuf_puts(w, "? Missing diagset");
            else
            {
                oid_class oclass;
                char diag_name_buf[OID_STR_MAX];
                const char *diag_name = 0;
                diag_name = yaz_oid_to_string_buf
                    (r->diagnosticSetId, &oclass, diag_name_buf);
                wrbuf_puts(w, diag_name);
            }
            if (!code)
                code = *r->condition;
            wrbuf_printf(w, " %d %s", *r->condition,
                         diagbib1_str(*r->condition));
            switch (r->which)
            {
            case Z_DefaultDiagFormat_v2Addinfo:
                wrbuf_printf(w, " -- v2 addinfo '%s'", r->u.v2Addinfo);
                break;
            case Z_DefaultDiagFormat_v3Addinfo:
                wrbuf_printf(w, " -- v3 addinfo '%s'", r->u.v3Addinfo);
                break;
            }
        }
    }
    return code;
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

int client_show_raw_begin(struct client *cl, int position,
                          const char *syntax, const char *esn,
                          void *data,
                          void (*error_handler)(void *data, const char *addinfo),
                          void (*record_handler)(void *data, const char *buf,
                                                 size_t sz))
{
    if (cl->show_raw)
        return -1;
    cl->show_raw = xmalloc(sizeof(*cl->show_raw));
    cl->show_raw->position = position;
    cl->show_raw->active = 0;
    cl->show_raw->data = data;
    cl->show_raw->error_handler = error_handler;
    cl->show_raw->record_handler = record_handler;
    if (syntax)
        cl->show_raw->syntax = xstrdup(syntax);
    else
        cl->show_raw->syntax = 0;
    if (esn)
        cl->show_raw->esn = xstrdup(esn);
    else
        cl->show_raw->esn = 0;
    client_continue(cl);
    return 0;
}

void client_show_raw_reset(struct client *cl)
{
    xfree(cl->show_raw);
    cl->show_raw = 0;
}

static void client_show_raw_error(struct client *cl, const char *addinfo)
{
    if (cl->show_raw)
    {
        cl->show_raw->error_handler(cl->show_raw->data, addinfo);
        client_show_raw_reset(cl);
    }
}

static void client_show_raw_cancel(struct client *cl)
{
    if (cl->show_raw)
    {
        cl->show_raw->error_handler(cl->show_raw->data, "cancel");
        client_show_raw_reset(cl);
    }
}

void client_send_raw_present(struct client *cl)
{
    Z_APDU *a = zget_APDU(global_parameters.odr_out, Z_APDU_presentRequest);
    int toget = 1;
    int start = cl->show_raw->position;

    assert(cl->show_raw);

    yaz_log(YLOG_DEBUG, "Trying to present %d record(s) from %d",
            toget, start);

    a->u.presentRequest->resultSetStartPoint = &start;
    a->u.presentRequest->numberOfRecordsRequested = &toget;

    if (cl->show_raw->syntax)  // syntax is optional
        a->u.presentRequest->preferredRecordSyntax =
            yaz_string_to_oid_odr(yaz_oid_std(),
                                  CLASS_RECSYN, cl->show_raw->syntax,
                                  global_parameters.odr_out);
    if (cl->show_raw->esn)  // element set is optional
    {
        Z_ElementSetNames *elementSetNames =
            odr_malloc(global_parameters.odr_out, sizeof(*elementSetNames));
        Z_RecordComposition *compo = 
            odr_malloc(global_parameters.odr_out, sizeof(*compo));
        a->u.presentRequest->recordComposition = compo;

        compo->which = Z_RecordComp_simple;
        compo->u.simple = elementSetNames;

        elementSetNames->which = Z_ElementSetNames_generic;
        elementSetNames->u.generic = 
            odr_strdup(global_parameters.odr_out, cl->show_raw->esn);
    }
    if (send_apdu(cl, a) >= 0)
    {
        cl->show_raw->active = 1;
	cl->state = Client_Presenting;
    }
    else
    {
        client_show_raw_error(cl, "send_apdu failed");
        cl->state = Client_Error;
    }
    odr_reset(global_parameters.odr_out);
}

void client_send_present(struct client *cl)
{
    struct session_database *sdb = client_get_database(cl);
    Z_APDU *a = zget_APDU(global_parameters.odr_out, Z_APDU_presentRequest);
    int toget;
    int start = cl->records + 1;
    char *recsyn;

    toget = global_parameters.chunk;
    if (toget > global_parameters.toget - cl->records)
        toget = global_parameters.toget - cl->records;
    if (toget > cl->hits - cl->records)
	toget = cl->hits - cl->records;

    yaz_log(YLOG_DEBUG, "Trying to present %d record(s) from %d",
            toget, start);

    a->u.presentRequest->resultSetStartPoint = &start;
    a->u.presentRequest->numberOfRecordsRequested = &toget;

    if ((recsyn = session_setting_oneval(sdb, PZ_REQUESTSYNTAX)))
    {
        a->u.presentRequest->preferredRecordSyntax =
            yaz_string_to_oid_odr(yaz_oid_std(),
                                  CLASS_RECSYN, recsyn,
                                  global_parameters.odr_out);
    }

    if (send_apdu(cl, a) >= 0)
	cl->state = Client_Presenting;
    else
        cl->state = Client_Error;
    odr_reset(global_parameters.odr_out);
}


void client_send_search(struct client *cl)
{
    struct session *se = client_get_session(cl);
    struct session_database *sdb = client_get_database(cl);
    Z_APDU *a = zget_APDU(global_parameters.odr_out, Z_APDU_searchRequest);
    int ndb;
    char **databaselist;
    Z_Query *zquery;
    int ssub = 0, lslb = 100000, mspn = 10;
    char *recsyn = 0;
    char *piggyback = 0;
    char *queryenc = 0;
    yaz_iconv_t iconv = 0;

    yaz_log(YLOG_DEBUG, "Sending search to %s", sdb->database->url);

    
    // constructing RPN query
    a->u.searchRequest->query = zquery = odr_malloc(global_parameters.odr_out,
                                                    sizeof(Z_Query));
    zquery->which = Z_Query_type_1;
    zquery->u.type_1 = p_query_rpn(global_parameters.odr_out, 
                                   client_get_pquery(cl));

    // converting to target encoding
    if ((queryenc = session_setting_oneval(sdb, PZ_QUERYENCODING))){
        iconv = yaz_iconv_open(queryenc, "UTF-8");
        if (iconv){
            yaz_query_charset_convert_rpnquery(zquery->u.type_1, 
                                               global_parameters.odr_out, 
                                               iconv);
            yaz_iconv_close(iconv);
        } else
            yaz_log(YLOG_WARN, "Query encoding failed %s %s", 
                    client_get_database(cl)->database->url, queryenc);
    }

    for (ndb = 0; sdb->database->databases[ndb]; ndb++)
	;
    databaselist = odr_malloc(global_parameters.odr_out, sizeof(char*) * ndb);
    for (ndb = 0; sdb->database->databases[ndb]; ndb++)
	databaselist[ndb] = sdb->database->databases[ndb];

    if (!(piggyback = session_setting_oneval(sdb, PZ_PIGGYBACK)) 
        || *piggyback == '1')
    {
        if ((recsyn = session_setting_oneval(sdb, PZ_REQUESTSYNTAX)))
        {
            a->u.searchRequest->preferredRecordSyntax =
                yaz_string_to_oid_odr(yaz_oid_std(),
                                      CLASS_RECSYN, recsyn,
                                      global_parameters.odr_out);
        }
        a->u.searchRequest->smallSetUpperBound = &ssub;
        a->u.searchRequest->largeSetLowerBound = &lslb;
        a->u.searchRequest->mediumSetPresentNumber = &mspn;
    }
    a->u.searchRequest->databaseNames = databaselist;
    a->u.searchRequest->num_databaseNames = ndb;

    
    {  //scope for sending and logging queries 
        WRBUF wbquery = wrbuf_alloc();
        yaz_query_to_wrbuf(wbquery, a->u.searchRequest->query);


        if (send_apdu(cl, a) >= 0)
        {
            client_set_state(cl, Client_Searching);
            client_set_requestid(cl, se->requestid);
            yaz_log(YLOG_LOG, "SearchRequest %s %s %s", 
                    client_get_database(cl)->database->url,
                    queryenc ? queryenc : "UTF-8",
                    wrbuf_cstr(wbquery));
        }
        else {
            client_set_state(cl, Client_Error);
            yaz_log(YLOG_WARN, "Failed SearchRequest %s  %s %s", 
                    client_get_database(cl)->database->url, 
                    queryenc ? queryenc : "UTF-8",
                    wrbuf_cstr(wbquery));
        }
        
        wrbuf_destroy(wbquery);
    }    

    odr_reset(global_parameters.odr_out);
}

void client_init_response(struct client *cl, Z_APDU *a)
{
    Z_InitResponse *r = a->u.initResponse;

    yaz_log(YLOG_DEBUG, "Init response %s", cl->database->database->url);

    if (*r->result)
    {
	cl->state = Client_Idle;
    }
    else
        cl->state = Client_Failed; // FIXME need to do something to the connection
}


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

    doc = record_to_xml(client_get_database(cl), npr->u.databaseRecord);
    if (!doc)
    {
        client_show_raw_error(cl, "unable to convert record to xml");
        return;
    }

    xmlDocDumpMemory(doc, &buf_out, &len_out);
    xmlFreeDoc(doc);

    cl->show_raw->record_handler(cl->show_raw->data,
                                 (const char *) buf_out, len_out);
    
    xmlFree(buf_out);
    xfree(cl->show_raw);
    cl->show_raw = 0;
}

static void ingest_records(struct client *cl, Z_Records *r)
{
#if USE_TIMING
    yaz_timing_t t = yaz_timing_create();
#endif
    struct record *rec;
    struct session *s = client_get_session(cl);
    Z_NamePlusRecordList *rlist;
    int i;

    if (r->which != Z_Records_DBOSD)
        return;
    rlist = r->u.databaseOrSurDiagnostics;
    for (i = 0; i < rlist->num_records; i++)
    {
        Z_NamePlusRecord *npr = rlist->records[i];

        cl->records++;
        if (npr->which != Z_NamePlusRecord_databaseRecord)
        {
            yaz_log(YLOG_WARN, 
                    "Unexpected record type, probably diagnostic %s",
                    cl->database->database->url);
            continue;
        }

        rec = ingest_record(cl, npr->u.databaseRecord, cl->records);
        if (!rec)
            continue;
    }
    if (rlist->num_records)
        session_alert_watch(s, SESSION_WATCH_RECORDS);

#if USE_TIMING
    yaz_timing_stop(t);
    yaz_log(YLOG_LOG, "ingest_records %6.5f %3.2f %3.2f", 
            yaz_timing_get_real(t), yaz_timing_get_user(t),
            yaz_timing_get_sys(t));
    yaz_timing_destroy(&t);
#endif
}


void client_search_response(struct client *cl, Z_APDU *a)
{
    struct session *se = cl->session;
    Z_SearchResponse *r = a->u.searchResponse;

    yaz_log(YLOG_DEBUG, "Search response %s (status=%d)", 
            cl->database->database->url, *r->searchStatus);

    if (*r->searchStatus)
    {
	cl->hits = *r->resultCount;
        se->total_hits += cl->hits;
        if (r->presentStatus && !*r->presentStatus && r->records)
        {
            yaz_log(YLOG_DEBUG, "Records in search response %s", 
                    cl->database->database->url);
            ingest_records(cl, r->records);
        }
        cl->state = Client_Idle;
    }
    else
    {          /*"FAILED"*/
        Z_Records *recs = r->records;
	cl->hits = 0;
        cl->state = Client_Error;
        if (recs && recs->which == Z_Records_NSD)
        {
            WRBUF w = wrbuf_alloc();

            Z_DiagRec dr, *dr_p = &dr;
            dr.which = Z_DiagRec_defaultFormat;
            dr.u.defaultFormat = recs->u.nonSurrogateDiagnostic;
            
            wrbuf_printf(w, "Search response NSD %s: ",
                         cl->database->database->url);
            
            cl->diagnostic = diag_to_wrbuf(&dr_p, 1, w);

            yaz_log(YLOG_WARN, "%s", wrbuf_cstr(w));

            cl->state = Client_Error;
            wrbuf_destroy(w);
        }
        else if (recs && recs->which == Z_Records_multipleNSD)
        {
            WRBUF w = wrbuf_alloc();

            wrbuf_printf(w, "Search response multipleNSD %s: ",
                         cl->database->database->url);
            cl->diagnostic = 
                diag_to_wrbuf(recs->u.multipleNonSurDiagnostics->diagRecs,
                              recs->u.multipleNonSurDiagnostics->num_diagRecs,
                              w);
            yaz_log(YLOG_WARN, "%s", wrbuf_cstr(w));
            cl->state = Client_Error;
            wrbuf_destroy(w);
        }
    }
}

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
        cl->state = Client_Idle;
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

int client_is_our_response(struct client *cl)
{
    struct session *se = client_get_session(cl);

    if (cl && (cl->requestid == se->requestid || 
               cl->state == Client_Initializing))
        return 1;
    return 0;
}

// Set authentication token in init if one is set for the client
// TODO: Extend this to handle other schemes than open (should be simple)
static void init_authentication(struct client *cl, Z_InitRequest *req)
{
    struct session_database *sdb = client_get_database(cl);
    char *auth = session_setting_oneval(sdb, PZ_AUTHENTICATION);

    if (*auth)
    {
        struct connection *co = client_get_connection(cl);
        struct session *se = client_get_session(cl);
        Z_IdAuthentication *idAuth = odr_malloc(global_parameters.odr_out,
                sizeof(*idAuth));
        idAuth->which = Z_IdAuthentication_open;
        idAuth->u.open = auth;
        req->idAuthentication = idAuth;
        connection_set_authentication(co, nmem_strdup(se->session_nmem, auth));
    }
}

static void init_zproxy(struct client *cl, Z_InitRequest *req)
{
    struct session_database *sdb = client_get_database(cl);
    char *ztarget = sdb->database->url;
    //char *ztarget = sdb->url;    
    char *zproxy = session_setting_oneval(sdb, PZ_ZPROXY);

    if (*zproxy)
        yaz_oi_set_string_oid(&req->otherInfo,
                              global_parameters.odr_out,
                              yaz_oid_userinfo_proxy,
                              1, ztarget);
}


static void client_init_request(struct client *cl)
{
    Z_APDU *a = zget_APDU(global_parameters.odr_out, Z_APDU_initRequest);

    a->u.initRequest->implementationId = global_parameters.implementationId;
    a->u.initRequest->implementationName = global_parameters.implementationName;
    a->u.initRequest->implementationVersion =
	global_parameters.implementationVersion;
    ODR_MASK_SET(a->u.initRequest->options, Z_Options_search);
    ODR_MASK_SET(a->u.initRequest->options, Z_Options_present);
    ODR_MASK_SET(a->u.initRequest->options, Z_Options_namedResultSets);

    ODR_MASK_SET(a->u.initRequest->protocolVersion, Z_ProtocolVersion_1);
    ODR_MASK_SET(a->u.initRequest->protocolVersion, Z_ProtocolVersion_2);
    ODR_MASK_SET(a->u.initRequest->protocolVersion, Z_ProtocolVersion_3);

    init_authentication(cl, a->u.initRequest);
    init_zproxy(cl, a->u.initRequest);

    if (send_apdu(cl, a) >= 0)
	client_set_state(cl, Client_Initializing);
    else
        client_set_state(cl, Client_Error);
    odr_reset(global_parameters.odr_out);
}

void client_continue(struct client *cl)
{
    if (cl->state == Client_Connected) {
        client_init_request(cl);
    }

    if (cl->state == Client_Idle)
    {
        struct session *se = client_get_session(cl);
        if (cl->requestid != se->requestid && cl->pquery) {
            // we'll have to abort this because result set is to be deleted
            client_show_raw_cancel(cl);   
            client_send_search(cl);
        }
        else if (cl->show_raw)
        {
            client_send_raw_present(cl);
        }
        else if (cl->hits > 0 && cl->records < global_parameters.toget &&
            cl->records < cl->hits) {
            client_send_present(cl);
        }
    }
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
        cl->state = Client_Disconnected;
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
        se->relevance = relevance_create(client_get_database(cl)->pct,
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
    if (cl->connection && (cl->state == Client_Connecting ||
                           cl->state == Client_Initializing ||
                           cl->state == Client_Searching ||
                           cl->state == Client_Presenting))
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
