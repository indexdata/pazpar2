/* $Id: pazpar2.c,v 1.17 2006-12-19 04:49:34 quinn Exp $ */;

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

#include <yaz/comstack.h>
#include <yaz/tcpip.h>
#include <yaz/proto.h>
#include <yaz/readconf.h>
#include <yaz/pquery.h>
#include <yaz/yaz-util.h>

#include "pazpar2.h"
#include "eventl.h"
#include "command.h"
#include "http.h"
#include "termlists.h"
#include "reclists.h"
#include "relevance.h"

#define PAZPAR2_VERSION "0.1"
#define MAX_CHUNK 15

static void client_fatal(struct client *cl);
static void connection_destroy(struct connection *co);
static int client_prep_connection(struct client *cl);
static void ingest_records(struct client *cl, Z_Records *r);
void session_alert_watch(struct session *s, int what);

IOCHAN channel_list = 0;  // Master list of connections we're handling events to

static struct connection *connection_freelist = 0;
static struct client *client_freelist = 0;

static struct host *hosts = 0;  // The hosts we know about 
static struct database *databases = 0; // The databases we know about

static char *client_states[] = {
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

struct parameters global_parameters = 
{
    30,
    "81",
    "Index Data PazPar2 (MasterKey)",
    PAZPAR2_VERSION,
    600, // 10 minutes
    60,
    100,
    MAX_CHUNK,
    0,
    0,
    0,
    0
};


static int send_apdu(struct client *c, Z_APDU *a)
{
    struct connection *co = c->connection;
    char *buf;
    int len, r;

    if (!z_APDU(global_parameters.odr_out, &a, 0, 0))
    {
        odr_perror(global_parameters.odr_out, "Encoding APDU");
	abort();
    }
    buf = odr_getbuf(global_parameters.odr_out, &len, 0);
    r = cs_put(co->link, buf, len);
    if (r < 0)
    {
        yaz_log(YLOG_WARN, "cs_put: %s", cs_errmsg(cs_errno(co->link)));
        return -1;
    }
    else if (r == 1)
    {
        fprintf(stderr, "cs_put incomplete (ParaZ does not handle that)\n");
        exit(1);
    }
    odr_reset(global_parameters.odr_out); /* release the APDU structure  */
    co->state = Conn_Waiting;
    return 0;
}


static void send_init(IOCHAN i)
{
    struct connection *co = iochan_getdata(i);
    struct client *cl = co->client;
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
    if (send_apdu(cl, a) >= 0)
    {
	iochan_setflags(i, EVENT_INPUT);
	cl->state = Client_Initializing;
    }
    else
        cl->state = Client_Error;
    odr_reset(global_parameters.odr_out);
}

static void send_search(IOCHAN i)
{
    struct connection *co = iochan_getdata(i);
    struct client *cl = co->client; 
    struct session *se = cl->session;
    struct database *db = cl->database;
    Z_APDU *a = zget_APDU(global_parameters.odr_out, Z_APDU_searchRequest);
    int ndb, cerror, cpos;
    char **databaselist;
    Z_Query *zquery;
    struct ccl_rpn_node *cn;
    int ssub = 0, lslb = 100000, mspn = 10;

    yaz_log(YLOG_DEBUG, "Sending search");

    cn = ccl_find_str(global_parameters.ccl_filter, se->query, &cerror, &cpos);
    if (!cn)
        return;
    a->u.searchRequest->query = zquery = odr_malloc(global_parameters.odr_out,
            sizeof(Z_Query));
    zquery->which = Z_Query_type_1;
    zquery->u.type_1 = ccl_rpn_query(global_parameters.odr_out, cn);
    ccl_rpn_delete(cn);

    for (ndb = 0; *db->databases[ndb]; ndb++)
	;
    databaselist = odr_malloc(global_parameters.odr_out, sizeof(char*) * ndb);
    for (ndb = 0; *db->databases[ndb]; ndb++)
	databaselist[ndb] = db->databases[ndb];

    a->u.presentRequest->preferredRecordSyntax =
            yaz_oidval_to_z3950oid(global_parameters.odr_out,
            CLASS_RECSYN, VAL_USMARC);
    a->u.searchRequest->smallSetUpperBound = &ssub;
    a->u.searchRequest->largeSetLowerBound = &lslb;
    a->u.searchRequest->mediumSetPresentNumber = &mspn;
    a->u.searchRequest->resultSetName = "Default";
    a->u.searchRequest->databaseNames = databaselist;
    a->u.searchRequest->num_databaseNames = ndb;

    if (send_apdu(cl, a) >= 0)
    {
	iochan_setflags(i, EVENT_INPUT);
	cl->state = Client_Searching;
        cl->requestid = se->requestid;
    }
    else
        cl->state = Client_Error;

    odr_reset(global_parameters.odr_out);
}

static void send_present(IOCHAN i)
{
    struct connection *co = iochan_getdata(i);
    struct client *cl = co->client; 
    Z_APDU *a = zget_APDU(global_parameters.odr_out, Z_APDU_presentRequest);
    int toget;
    int start = cl->records + 1;

    toget = global_parameters.chunk;
    if (toget > cl->hits - cl->records)
	toget = cl->hits - cl->records;

    yaz_log(YLOG_DEBUG, "Trying to present %d records\n", toget);

    a->u.presentRequest->resultSetStartPoint = &start;
    a->u.presentRequest->numberOfRecordsRequested = &toget;

    a->u.presentRequest->resultSetId = "Default";

    a->u.presentRequest->preferredRecordSyntax =
            yaz_oidval_to_z3950oid(global_parameters.odr_out,
            CLASS_RECSYN, VAL_USMARC);

    if (send_apdu(cl, a) >= 0)
    {
	iochan_setflags(i, EVENT_INPUT);
	cl->state = Client_Presenting;
    }
    else
        cl->state = Client_Error;
    odr_reset(global_parameters.odr_out);
}

static void do_initResponse(IOCHAN i, Z_APDU *a)
{
    struct connection *co = iochan_getdata(i);
    struct client *cl = co->client;
    Z_InitResponse *r = a->u.initResponse;

    yaz_log(YLOG_DEBUG, "Received init response");

    if (*r->result)
    {
	cl->state = Client_Idle;
    }
    else
        cl->state = Client_Failed; // FIXME need to do something to the connection
}

static void do_searchResponse(IOCHAN i, Z_APDU *a)
{
    struct connection *co = iochan_getdata(i);
    struct client *cl = co->client;
    struct session *se = cl->session;
    Z_SearchResponse *r = a->u.searchResponse;

    yaz_log(YLOG_DEBUG, "Searchresponse (status=%d)", *r->searchStatus);

    if (*r->searchStatus)
    {
	cl->hits = *r->resultCount;
        se->total_hits += cl->hits;
        if (r->presentStatus && !*r->presentStatus && r->records)
        {
            yaz_log(YLOG_DEBUG, "Records in search response");
            cl->records += *r->numberOfRecordsReturned;
            ingest_records(cl, r->records);
        }
        cl->state = Client_Idle;
    }
    else
    {          /*"FAILED"*/
	cl->hits = 0;
        cl->state = Client_Error;
        if (r->records) {
            Z_Records *recs = r->records;
            if (recs->which == Z_Records_NSD)
            {
                yaz_log(YLOG_WARN, "Non-surrogate diagnostic");
                cl->diagnostic = *recs->u.nonSurrogateDiagnostic->condition;
                cl->state = Client_Error;
            }
        }
    }
}

const char *find_field(const char *rec, const char *field)
{
    char lbuf[5];
    char *line;

    lbuf[0] = '\n';
    strcpy(lbuf + 1, field);

    if ((line = strstr(rec, lbuf)))
        return ++line;
    else
        return 0;
}

const char *find_subfield(const char *field, char subfield)
{
    const char *p = field;

    while (*p && *p != '\n')
    {
        while (*p != '\n' && *p != '\t')
            p++;
        if (*p == '\t' && *(++p) == subfield) {
            if (*(++p) == ' ')
            {
                while (isspace(*p))
                    p++;
                return p;
            }
        }
    }
    return 0;
}

// Extract 245 $a $b 100 $a
char *extract_title(struct session *s, const char *rec)
{
    const char *field, *subfield;
    char *e, *ef;
    unsigned char *obuf, *p;

    wrbuf_rewind(s->wrbuf);

    if (!(field = find_field(rec, "245")))
        return 0;
    if (!(subfield = find_subfield(field, 'a')))
        return 0;
    ef = index(subfield, '\n');
    if ((e = index(subfield, '\t')) && e < ef)
        ef = e;
    if (ef)
    {
        wrbuf_write(s->wrbuf, subfield, ef - subfield);
        if ((subfield = find_subfield(field, 'b'))) 
        {
            ef = index(subfield, '\n');
            if ((e = index(subfield, '\t')) && e < ef)
                ef = e;
            if (ef)
            {
                wrbuf_putc(s->wrbuf, ' ');
                wrbuf_write(s->wrbuf, subfield, ef - subfield);
            }
        }
    }
    if ((field = find_field(rec, "100")))
    {
        if ((subfield = find_subfield(field, 'a')))
        {
            ef = index(subfield, '\n');
            if ((e = index(subfield, '\t')) && e < ef)
                ef = e;
            if (ef)
            {
                wrbuf_puts(s->wrbuf, ", by ");
                wrbuf_write(s->wrbuf, subfield, ef - subfield);
            }
        }
    }
    wrbuf_putc(s->wrbuf, '\0');
    obuf = (unsigned char*) nmem_strdup(s->nmem, wrbuf_buf(s->wrbuf));
    for (p = obuf; *p; p++)
        if (*p == '&' || *p == '<' || *p > 122 || *p < ' ')
            *p = ' ';
    return (char*) obuf;
}

// Extract 245 $a $b 100 $a
char *extract_mergekey(struct session *s, const char *rec)
{
    const char *field, *subfield;
    char *e, *ef;
    char *out, *p, *pout;

    wrbuf_rewind(s->wrbuf);

    if (!(field = find_field(rec, "245")))
        return 0;
    if (!(subfield = find_subfield(field, 'a')))
        return 0;
    ef = index(subfield, '\n');
    if ((e = index(subfield, '\t')) && e < ef)
        ef = e;
    if (ef)
    {
        wrbuf_write(s->wrbuf, subfield, ef - subfield);
        if ((subfield = find_subfield(field, 'b'))) 
        {
            ef = index(subfield, '\n');
            if ((e = index(subfield, '\t')) && e < ef)
                ef = e;
            if (ef)
            {
                wrbuf_puts(s->wrbuf, " field "); 
                wrbuf_write(s->wrbuf, subfield, ef - subfield);
            }
        }
    }
    if ((field = find_field(rec, "100")))
    {
        if ((subfield = find_subfield(field, 'a')))
        {
            ef = index(subfield, '\n');
            if ((e = index(subfield, '\t')) && e < ef)
                ef = e;
            if (ef)
            {
                wrbuf_puts(s->wrbuf, " field "); 
                wrbuf_write(s->wrbuf, subfield, ef - subfield);
            }
        }
    }
    wrbuf_putc(s->wrbuf, '\0');
    p = wrbuf_buf(s->wrbuf);
    out = pout = nmem_malloc(s->nmem, strlen(p) + 1);

    while (*p)
    {
        while (isalnum(*p))
            *(pout++) = tolower(*(p++));
        while (*p && !isalnum(*p))
            p++;
        *(pout++) = ' ';
    }
    if (out != pout)
        *(--pout) = '\0';

    return out;
}

#ifdef RECHEAP
static void push_record(struct session *s, struct record *r)
{
    int p;
    assert(s->recheap_max + 1 < s->recheap_size);

    s->recheap[p = ++s->recheap_max] = r;
    while (p > 0)
    {
        int parent = (p - 1) >> 1;
        if (strcmp(s->recheap[p]->merge_key, s->recheap[parent]->merge_key) < 0)
        {
            struct record *tmp;
            tmp = s->recheap[parent];
            s->recheap[parent] = s->recheap[p];
            s->recheap[p] = tmp;
            p = parent;
        }
        else
            break;
    }
}

static struct record *top_record(struct session *s)
{
    return s-> recheap_max >= 0 ?  s->recheap[0] : 0;
}

static struct record *pop_record(struct session *s)
{
    struct record *res;
    int p = 0;
    int lastnonleaf = (s->recheap_max - 1) >> 1;

    if (s->recheap_max < 0)
        return 0;

    res = s->recheap[0];

    s->recheap[p] = s->recheap[s->recheap_max--];

    while (p <= lastnonleaf)
    {
        int right = (p + 1) << 1;
        int left = right - 1;
        int min = left;

        if (right < s->recheap_max &&
                strcmp(s->recheap[right]->merge_key, s->recheap[left]->merge_key) < 0)
            min = right;
        if (strcmp(s->recheap[min]->merge_key, s->recheap[p]->merge_key) < 0)
        {
            struct record *tmp = s->recheap[min];
            s->recheap[min] = s->recheap[p];
            s->recheap[p] = tmp;
            p = min;
        }
        else
            break;
    }
    return res;
}

// Like pop_record but collapses identical (merge_key) records
// The heap will contain multiple independent matching records and possibly
// one cluster, created the last time the list was scanned
static struct record *pop_mrecord(struct session *s)
{
    struct record *this;
    struct record *next;

    if (!(this = pop_record(s)))
        return 0;

    // Collapse identical records
    while ((next = top_record(s)))
    {
        struct record *p, *tmpnext;
        if (strcmp(this->merge_key, next->merge_key))
            break;
        // Absorb record (and clustersiblings) into a supercluster
        for (p = next; p; p = tmpnext) {
            tmpnext = p->next_cluster;
            p->next_cluster = this->next_cluster;
            this->next_cluster = p;
        }

        pop_record(s);
    }
    return this;
}

// Reads records in sort order. Store records in top of heapspace until rewind is called.
static struct record *read_recheap(struct session *s)
{
    struct record *r = pop_mrecord(s);

    if (r)
    {
        if (s->recheap_scratch < 0)
            s->recheap_scratch = s->recheap_size;
        s->recheap[--s->recheap_scratch] = r;
    }

    return r;
}

// Return records to heap after read
static void rewind_recheap(struct session *s)
{
    while (s->recheap_scratch >= 0) {
        push_record(s, s->recheap[s->recheap_scratch++]);
        if (s->recheap_scratch >= s->recheap_size)
            s->recheap_scratch = -1;
    }
}

#endif

// FIXME needs to be generalized. Should flexibly generate X lists per search
static void extract_subject(struct session *s, const char *rec)
{
    const char *field, *subfield;

    while ((field = find_field(rec, "650")))
    {
        rec = field; 
        if ((subfield = find_subfield(field, 'a')))
        {
            char *e, *ef;
            char buf[1024];
            int len;

            ef = index(subfield, '\n');
            if (!ef)
                return;
            if ((e = index(subfield, '\t')) && e < ef)
                ef = e;
            while (ef > subfield && !isalpha(*(ef - 1)) && *(ef - 1) != ')')
                ef--;
            len = ef - subfield;
            assert(len < 1023);
            memcpy(buf, subfield, len);
            buf[len] = '\0';
            if (*buf)
                termlist_insert(s->termlist, buf);
        }
    }
}

static void pull_relevance_field(struct session *s, struct record *head, const char *rec,
        char *field, int mult)
{
    const char *fb;
    while ((fb = find_field(rec, field)))
    {
        char *ffield = strchr(fb, '\t');
        if (!ffield)
            return;
        char *eol = strchr(ffield, '\n');
        if (!eol)
            return;
        relevance_countwords(s->relevance, head, ffield, eol - ffield, mult);
        rec = field + 1; // Crude way to cause a loop through repeating fields
    }
}

static void pull_relevance_keys(struct session *s, struct record *head,  struct record *rec)
{
    relevance_newrec(s->relevance, head);
    pull_relevance_field(s, head, rec->buf, "100", 2);
    pull_relevance_field(s, head, rec->buf, "245", 4);
    //pull_relevance_field(s, head, rec->buf, "530", 1);
    pull_relevance_field(s, head, rec->buf, "630", 1);
    pull_relevance_field(s, head, rec->buf, "650", 1);
    pull_relevance_field(s, head, rec->buf, "700", 1);
    relevance_donerecord(s->relevance, head);
}

static struct record *ingest_record(struct client *cl, char *buf, int len)
{
    struct session *se = cl->session;
    struct record *res;
    struct record *head;
    const char *recbuf;

    wrbuf_rewind(se->wrbuf);
    yaz_marc_xml(global_parameters.yaz_marc, YAZ_MARC_LINE);
    if (yaz_marc_decode_wrbuf(global_parameters.yaz_marc, buf, len, se->wrbuf) < 0)
    {
        yaz_log(YLOG_WARN, "Failed to decode MARC record");
        return 0;
    }
    wrbuf_putc(se->wrbuf, '\0');
    recbuf = wrbuf_buf(se->wrbuf);

    res = nmem_malloc(se->nmem, sizeof(struct record));
    res->buf = nmem_strdup(se->nmem, recbuf);

    extract_subject(se, res->buf);

    res->title = extract_title(se, res->buf);
    res->merge_key = extract_mergekey(se, res->buf);
    if (!res->merge_key)
        return 0;
    res->client = cl;
    res->next_cluster = 0;
    res->target_offset = -1;
    res->term_frequency_vec = 0;

    head = reclist_insert(se->reclist, res);

    pull_relevance_keys(se, head, res);

    se->total_records++;

    return res;
}

static void ingest_records(struct client *cl, Z_Records *r)
{
    struct record *rec;
    struct session *s = cl->session;
    Z_NamePlusRecordList *rlist;
    int i;

    if (r->which != Z_Records_DBOSD)
        return;
    rlist = r->u.databaseOrSurDiagnostics;
    for (i = 0; i < rlist->num_records; i++)
    {
        Z_NamePlusRecord *npr = rlist->records[i];
        Z_External *e;
        char *buf;
        int len;

        if (npr->which != Z_NamePlusRecord_databaseRecord)
        {
            yaz_log(YLOG_WARN, "Unexpected record type, probably diagnostic");
            continue;
        }
        e = npr->u.databaseRecord;
        if (e->which != Z_External_octet)
        {
            yaz_log(YLOG_WARN, "Unexpected external branch, probably BER");
            continue;
        }
        buf = (char*) e->u.octet_aligned->buf;
        len = e->u.octet_aligned->len;

        rec = ingest_record(cl, buf, len);
        if (!rec)
            continue;
    }
    if (s->watchlist[SESSION_WATCH_RECORDS].fun && rlist->num_records)
        session_alert_watch(s, SESSION_WATCH_RECORDS);
}

static void do_presentResponse(IOCHAN i, Z_APDU *a)
{
    struct connection *co = iochan_getdata(i);
    struct client *cl = co->client;
    Z_PresentResponse *r = a->u.presentResponse;

    if (r->records) {
        Z_Records *recs = r->records;
        if (recs->which == Z_Records_NSD)
        {
            yaz_log(YLOG_WARN, "Non-surrogate diagnostic");
            cl->diagnostic = *recs->u.nonSurrogateDiagnostic->condition;
            cl->state = Client_Error;
        }
    }

    if (!*r->presentStatus && cl->state != Client_Error)
    {
        yaz_log(YLOG_DEBUG, "Good Present response");
        cl->records += *r->numberOfRecordsReturned;
        ingest_records(cl, r->records);
        cl->state = Client_Idle;
    }
    else if (*r->presentStatus) 
    {
        yaz_log(YLOG_WARN, "Bad Present response");
        cl->state = Client_Error;
    }
}

static void handler(IOCHAN i, int event)
{
    struct connection *co = iochan_getdata(i);
    struct client *cl = co->client;
    struct session *se = 0;

    if (cl)
        se = cl->session;
    else
    {
        yaz_log(YLOG_WARN, "Destroying orphan connection (fix me?)");
        connection_destroy(co);
        return;
    }

    if (co->state == Conn_Connecting && event & EVENT_OUTPUT)
    {
	int errcode;
        socklen_t errlen = sizeof(errcode);

	if (getsockopt(cs_fileno(co->link), SOL_SOCKET, SO_ERROR, &errcode,
	    &errlen) < 0 || errcode != 0)
	{
            client_fatal(cl);
	    return;
	}
	else
	{
            yaz_log(YLOG_DEBUG, "Connect OK");
	    co->state = Conn_Open;
            if (cl)
                cl->state = Client_Connected;
	}
    }

    else if (event & EVENT_INPUT)
    {
	int len = cs_get(co->link, &co->ibuf, &co->ibufsize);

	if (len < 0)
	{
            yaz_log(YLOG_WARN|YLOG_ERRNO, "Error reading from Z server");
            connection_destroy(co);
	    return;
	}
        else if (len == 0)
	{
            yaz_log(YLOG_WARN, "EOF reading from Z server");
            connection_destroy(co);
	    return;
	}
	else if (len > 1) // We discard input if we have no connection
	{
            co->state = Conn_Open;

            if (cl && (cl->requestid == se->requestid || cl->state == Client_Initializing))
            {
                Z_APDU *a;

                odr_reset(global_parameters.odr_in);
                odr_setbuf(global_parameters.odr_in, co->ibuf, len, 0);
                if (!z_APDU(global_parameters.odr_in, &a, 0, 0))
                {
                    client_fatal(cl);
                    return;
                }
                switch (a->which)
                {
                    case Z_APDU_initResponse:
                        do_initResponse(i, a);
                        break;
                    case Z_APDU_searchResponse:
                        do_searchResponse(i, a);
                        break;
                    case Z_APDU_presentResponse:
                        do_presentResponse(i, a);
                        break;
                    default:
                        yaz_log(YLOG_WARN, "Unexpected result from server");
                        client_fatal(cl);
                        return;
                }
                // We aren't expecting staggered output from target
                // if (cs_more(t->link))
                //    iochan_setevent(i, EVENT_INPUT);
            }
            else  // we throw away response and go to idle mode
            {
                yaz_log(YLOG_DEBUG, "Ignoring result of expired operation");
                cl->state = Client_Idle;
            }
	}
	/* if len==1 we do nothing but wait for more input */
    }

    if (cl->state == Client_Connected) {
        send_init(i);
    }

    if (cl->state == Client_Idle)
    {
        if (cl->requestid != se->requestid && *se->query) {
            send_search(i);
        }
        else if (cl->hits > 0 && cl->records < global_parameters.toget &&
            cl->records < cl->hits) {
            send_present(i);
        }
    }
}

// Disassociate connection from client
static void connection_release(struct connection *co)
{
    struct client *cl = co->client;

    yaz_log(YLOG_DEBUG, "Connection release %s", co->host->hostport);
    if (!cl)
        return;
    cl->connection = 0;
    co->client = 0;
}

// Close connection and recycle structure
static void connection_destroy(struct connection *co)
{
    struct host *h = co->host;
    cs_close(co->link);
    iochan_destroy(co->iochan);

    yaz_log(YLOG_DEBUG, "Connection destroy %s", co->host->hostport);
    if (h->connections == co)
        h->connections = co->next;
    else
    {
        struct connection *pco;
        for (pco = h->connections; pco && pco->next != co; pco = pco->next)
            ;
        if (pco)
            pco->next = co->next;
        else
            abort();
    }
    if (co->client)
    {
        if (co->client->state != Client_Idle)
            co->client->state = Client_Disconnected;
        co->client->connection = 0;
    }
    co->next = connection_freelist;
    connection_freelist = co;
}

// Creates a new connection for client, associated with the host of 
// client's database
static struct connection *connection_create(struct client *cl)
{
    struct connection *new;
    COMSTACK link; 
    int res;
    void *addr;

    yaz_log(YLOG_DEBUG, "Connection create %s", cl->database->url);
    if (!(link = cs_create(tcpip_type, 0, PROTO_Z3950)))
    {
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "Failed to create comstack");
        exit(1);
    }

    if (!(addr = cs_straddr(link, cl->database->host->ipport)))
    {
        yaz_log(YLOG_WARN|YLOG_ERRNO, "Lookup of IP address failed?");
        return 0;
    }

    res = cs_connect(link, addr);
    if (res < 0)
    {
        yaz_log(YLOG_WARN|YLOG_ERRNO, "cs_connect %s", cl->database->url);
        return 0;
    }

    if ((new = connection_freelist))
        connection_freelist = new->next;
    else
    {
        new = xmalloc(sizeof (struct connection));
        new->ibuf = 0;
        new->ibufsize = 0;
    }
    new->state = Conn_Connecting;
    new->host = cl->database->host;
    new->next = new->host->connections;
    new->host->connections = new;
    new->client = cl;
    cl->connection = new;
    new->link = link;

    new->iochan = iochan_create(cs_fileno(link), handler, 0);
    iochan_setdata(new->iochan, new);
    new->iochan->next = channel_list;
    channel_list = new->iochan;
    return new;
}

// Close connection and set state to error
static void client_fatal(struct client *cl)
{
    yaz_log(YLOG_WARN, "Fatal error from %s", cl->database->url);
    connection_destroy(cl->connection);
    cl->state = Client_Error;
}

// Ensure that client has a connection associated
static int client_prep_connection(struct client *cl)
{
    struct connection *co;
    struct session *se = cl->session;
    struct host *host = cl->database->host;

    co = cl->connection;

    yaz_log(YLOG_DEBUG, "Client prep %s", cl->database->url);

    if (!co)
    {
        // See if someone else has an idle connection
        // We should look at timestamps here to select the longest-idle connection
        for (co = host->connections; co; co = co->next)
            if (co->state == Conn_Open && (!co->client || co->client->session != se))
                break;
        if (co)
        {
            connection_release(co);
            cl->connection = co;
            co->client = cl;
        }
        else
            co = connection_create(cl);
    }
    if (co)
    {
        if (co->state == Conn_Connecting)
            cl->state = Client_Connecting;
        else if (co->state == Conn_Open)
        {
            if (cl->state == Client_Error || cl->state == Client_Disconnected)
                cl->state = Client_Idle;
        }
        iochan_setflag(co->iochan, EVENT_OUTPUT);
        return 1;
    }
    else
        return 0;
}

void load_simpletargets(const char *fn)
{
    FILE *f = fopen(fn, "r");
    char line[256];

    if (!f)
    {
        yaz_log(YLOG_WARN|YLOG_ERRNO, "open %s", fn);
        exit(1);
    }

    while (fgets(line, 255, f))
    {
        char *url, *db;
        struct host *host;
        struct database *database;

        if (strncmp(line, "target ", 7))
            continue;
        url = line + 7;
        url[strlen(url) - 1] = '\0';
        yaz_log(YLOG_DEBUG, "Target: %s", url);
        if ((db = strchr(url, '/')))
            *(db++) = '\0';
        else
            db = "Default";

        for (host = hosts; host; host = host->next)
            if (!strcmp(url, host->hostport))
                break;
        if (!host)
        {
            struct addrinfo *addrinfo, hints;
            char *port;
            char ipport[128];
            unsigned char addrbuf[4];
            int res;

            host = xmalloc(sizeof(struct host));
            host->hostport = xstrdup(url);
            host->connections = 0;

            if ((port = strchr(url, ':')))
                *(port++) = '\0';
            else
                port = "210";

            hints.ai_flags = 0;
            hints.ai_family = PF_INET;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
            hints.ai_addrlen = 0;
            hints.ai_addr = 0;
            hints.ai_canonname = 0;
            hints.ai_next = 0;
            // This is not robust code. It assumes that getaddrinfo returns AF_INET
            // address.
            if ((res = getaddrinfo(url, port, &hints, &addrinfo)))
            {
                yaz_log(YLOG_WARN, "Failed to resolve %s: %s", url, gai_strerror(res));
                continue;
            }
            assert(addrinfo->ai_family == PF_INET);
            memcpy(addrbuf, &((struct sockaddr_in*)addrinfo->ai_addr)->sin_addr.s_addr, 4);
            sprintf(ipport, "%hhd.%hhd.%hhd.%hhd:%s",
                    addrbuf[0], addrbuf[1], addrbuf[2], addrbuf[3], port);
            host->ipport = xstrdup(ipport);
            freeaddrinfo(addrinfo);
            host->next = hosts;
            hosts = host;
        }
        database = xmalloc(sizeof(struct database));
        database->host = host;
        database->url = xmalloc(strlen(url) + strlen(db) + 2);
        strcpy(database->url, url);
        strcat(database->url, "/");
        strcat(database->url, db);
        strcpy(database->databases[0], db);
        *database->databases[1] = '\0';
        database->errors = 0;
        database->next = databases;
        databases = database;

    }
    fclose(f);
}

static void pull_terms(NMEM nmem, struct ccl_rpn_node *n, char **termlist, int *num)
{
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
            termlist[(*num)++] = nmem_strdup(nmem, n->u.t.term);
            break;
        default: // NOOP
            break;
    }
}

// Extract terms from query into null-terminated termlist
static int extract_terms(NMEM nmem, char *query, char **termlist)
{
    int error, pos;
    struct ccl_rpn_node *n;
    int num = 0;

    n = ccl_find_str(global_parameters.ccl_filter, query, &error, &pos);
    if (!n)
        return -1;
    pull_terms(nmem, n, termlist, &num);
    termlist[num] = 0;
    ccl_rpn_delete(n);
    return 0;
}

static struct client *client_create(void)
{
    struct client *r;
    if (client_freelist)
    {
        r = client_freelist;
        client_freelist = client_freelist->next;
    }
    else
        r = xmalloc(sizeof(struct client));
    r->database = 0;
    r->connection = 0;
    r->session = 0;
    r->hits = 0;
    r->records = 0;
    r->setno = 0;
    r->requestid = -1;
    r->diagnostic = 0;
    r->state = Client_Disconnected;
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
    if (c->connection)
        connection_release(c->connection);
    c->next = client_freelist;
    client_freelist = c;
}

void session_set_watch(struct session *s, int what, session_watchfun fun, void *data)
{
    s->watchlist[what].fun = fun;
    s->watchlist[what].data = data;
}

void session_alert_watch(struct session *s, int what)
{
    if (!s->watchlist[what].fun)
        return;
    (*s->watchlist[what].fun)(s->watchlist[what].data);
    s->watchlist[what].fun = 0;
    s->watchlist[what].data = 0;
}

// This should be extended with parameters to control selection criteria
// Associates a set of clients with a session;
int select_targets(struct session *se)
{
    struct database *db;
    int c = 0;

    while (se->clients)
        client_destroy(se->clients);
    for (db = databases; db; db = db->next)
    {
        struct client *cl = client_create();
        cl->database = db;
        cl->session = se;
        cl->next = se->clients;
        se->clients = cl;
        c++;
    }
    return c;
}

char *search(struct session *se, char *query)
{
    int live_channels = 0;
    struct client *cl;

    yaz_log(YLOG_DEBUG, "Search");

    strcpy(se->query, query);
    se->requestid++;
    nmem_reset(se->nmem);
    for (cl = se->clients; cl; cl = cl->next)
    {
        cl->hits = -1;
        cl->records = 0;
        cl->diagnostic = 0;

        if (client_prep_connection(cl))
            live_channels++;
    }
    if (live_channels)
    {
        char *p[512];
        int maxrecs = live_channels * global_parameters.toget;
        se->termlist = termlist_create(se->nmem, maxrecs, 15);
        se->reclist = reclist_create(se->nmem, maxrecs);
        extract_terms(se->nmem, query, p);
        se->relevance = relevance_create(se->nmem, (const char **) p, maxrecs);
        se->total_records = se->total_hits = 0;
    }
    else
        return "NOTARGETS";

    return 0;
}

void destroy_session(struct session *s)
{
    yaz_log(YLOG_LOG, "Destroying session");
    while (s->clients)
        client_destroy(s->clients);
    nmem_destroy(s->nmem);
    wrbuf_free(s->wrbuf, 1);
}

struct session *new_session() 
{
    int i;
    struct session *session = xmalloc(sizeof(*session));

    yaz_log(YLOG_DEBUG, "New pazpar2 session");
    
    session->total_hits = 0;
    session->total_records = 0;
    session->termlist = 0;
    session->reclist = 0;
    session->requestid = -1;
    session->clients = 0;
    session->query[0] = '\0';
    session->nmem = nmem_create();
    session->wrbuf = wrbuf_alloc();
    for (i = 0; i <= SESSION_WATCH_MAX; i++)
    {
        session->watchlist[i].data = 0;
        session->watchlist[i].fun = 0;
    }

    select_targets(session);

    return session;
}

struct hitsbytarget *hitsbytarget(struct session *se, int *count)
{
    static struct hitsbytarget res[1000]; // FIXME MM
    struct client *cl;

    *count = 0;
    for (cl = se->clients; cl; cl = cl->next)
    {
        strcpy(res[*count].id, cl->database->host->hostport);
        res[*count].hits = cl->hits;
        res[*count].records = cl->records;
        res[*count].diagnostic = cl->diagnostic;
        res[*count].state = client_states[cl->state];
        res[*count].connected  = cl->connection ? 1 : 0;
        (*count)++;
    }

    return res;
}

struct termlist_score **termlist(struct session *s, int *num)
{
    return termlist_highscore(s->termlist, num);
}

struct record **show(struct session *s, int start, int *num, int *total, int *sumhits)
{
    struct record **recs = nmem_malloc(s->nmem, *num * sizeof(struct record *));
    int i;

    relevance_prepare_read(s->relevance, s->reclist);

    *total = s->reclist->num_records;
    *sumhits = s->total_hits;

    for (i = 0; i < start; i++)
        if (!reclist_read_record(s->reclist))
        {
            *num = 0;
            return 0;
        }

    for (i = 0; i < *num; i++)
    {
        struct record *r = reclist_read_record(s->reclist);
        if (!r)
        {
            *num = i;
            break;
        }
        recs[i] = r;
    }
    return recs;
}

void statistics(struct session *se, struct statistics *stat)
{
    struct client *cl;
    int count = 0;

    bzero(stat, sizeof(*stat));
    for (cl = se->clients; cl; cl = cl->next)
    {
        if (!cl->connection)
            stat->num_no_connection++;
        switch (cl->state)
        {
            case Client_Connecting: stat->num_connecting++; break;
            case Client_Initializing: stat->num_initializing++; break;
            case Client_Searching: stat->num_searching++; break;
            case Client_Presenting: stat->num_presenting++; break;
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

static CCL_bibset load_cclfile(const char *fn)
{
    CCL_bibset res = ccl_qual_mk();
    if (ccl_qual_fname(res, fn) < 0)
    {
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "%s", fn);
        exit(1);
    }
    return res;
}

int main(int argc, char **argv)
{
    int ret;
    char *arg;
    int setport = 0;

    if (signal(SIGPIPE, SIG_IGN) < 0)
        yaz_log(YLOG_WARN|YLOG_ERRNO, "signal");

    yaz_log_init(YLOG_DEFAULT_LEVEL, "pazpar2", 0);

    while ((ret = options("c:h:p:C:s:", argv, argc, &arg)) != -2)
    {
	switch (ret) {
	    case 'c':
		command_init(atoi(arg));
                setport++;
		break;
            case 'h':
                http_init(arg);
                setport++;
                break;
            case 'C':
                global_parameters.ccl_filter = load_cclfile(arg);
                break;
            case 'p':
                http_set_proxyaddr(arg);
                break;
            case 's':
                load_simpletargets(arg);
                break;
	    default:
		fprintf(stderr, "Usage: pazpar2\n"
                        "    -h [host:]port          (REST protocol listener)\n"
                        "    -c cmdport              (telnet-style)\n"
                        "    -C cclconfig\n"
                        "    -s simpletargetfile\n"
                        "    -p hostname[:portno]    (HTTP proxy)\n");
		exit(1);
	}
    }

    if (!setport)
    {
        fprintf(stderr, "Set command port with -h or -c\n");
        exit(1);
    }

    global_parameters.ccl_filter = load_cclfile("default.bib");
    global_parameters.yaz_marc = yaz_marc_create();
    yaz_marc_subfield_str(global_parameters.yaz_marc, "\t");
    global_parameters.odr_in = odr_createmem(ODR_DECODE);
    global_parameters.odr_out = odr_createmem(ODR_ENCODE);

    event_loop(&channel_list);

    return 0;
}

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
