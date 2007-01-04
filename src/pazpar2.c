/* $Id: pazpar2.c,v 1.10 2007-01-04 03:16:14 quinn Exp $ */;

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
#include <yaz/nmem.h>

#include "pazpar2.h"
#include "eventl.h"
#include "command.h"
#include "http.h"
#include "termlists.h"
#include "reclists.h"
#include "relevance.h"
#include "config.h"

#define PAZPAR2_VERSION "0.1"
#define MAX_CHUNK 15

static void client_fatal(struct client *cl);
static void connection_destroy(struct connection *co);
static int client_prep_connection(struct client *cl);
static void ingest_records(struct client *cl, Z_Records *r);
static struct conf_retrievalprofile *database_retrieval_profile(struct database *db);
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
    0,
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

    for (ndb = 0; db->databases[ndb]; ndb++)
	;
    databaselist = odr_malloc(global_parameters.odr_out, sizeof(char*) * ndb);
    for (ndb = 0; db->databases[ndb]; ndb++)
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

char *normalize_mergekey(char *buf)
{
    char *p = buf, *pout = buf;

    while (*p)
    {
        while (*p && !isalnum(*p))
            p++;
        while (isalnum(*p))
            *(pout++) = tolower(*(p++));
        if (*p)
            *(pout++) = ' ';
        while (*p && !isalnum(*p))
            p++;
    }
    if (buf != pout)
        *pout = '\0';

    return buf;
}


#ifdef GAGA
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
#ifdef FIXME
            if (*buf)
                termlist_insert(s->termlist, buf);
#endif
        }
    }
}
#endif

static void add_facet(struct session *s, const char *type, const char *value)
{
    int i;

    for (i = 0; i < s->num_termlists; i++)
        if (!strcmp(s->termlists[i].name, type))
            break;
    if (i == s->num_termlists)
    {
        if (i == SESSION_MAX_TERMLISTS)
        {
            yaz_log(YLOG_FATAL, "Too many termlists");
            exit(1);
        }
        s->termlists[i].name = nmem_strdup(s->nmem, type);
        s->termlists[i].termlist = termlist_create(s->nmem, s->expected_maxrecs, 15);
        s->num_termlists = i + 1;
    }
    termlist_insert(s->termlists[i].termlist, value);
}

static xmlDoc *normalize_record(struct client *cl, Z_External *rec)
{
    struct conf_retrievalprofile *rprofile = cl->database->rprofile;
    struct conf_retrievalmap *m;
    xmlNode *res;
    xmlDoc *rdoc;

    // First normalize to XML
    if (rprofile->native_syntax == Nativesyn_iso2709)
    {
        char *buf;
        int len;
        if (rec->which != Z_External_octet)
        {
            yaz_log(YLOG_WARN, "Unexpected external branch, probably BER");
            return 0;
        }
        buf = (char*) rec->u.octet_aligned->buf;
        len = rec->u.octet_aligned->len;
        if (yaz_marc_read_iso2709(rprofile->yaz_marc, buf, len) < 0)
        {
            yaz_log(YLOG_WARN, "Failed to decode MARC");
            return 0;
        }
        if (yaz_marc_write_xml(rprofile->yaz_marc, &res,
                    "http://www.loc.gov/MARC21/slim", 0, 0) < 0)
        {
            yaz_log(YLOG_WARN, "Failed to encode as XML");
            return 0;
        }
        rdoc = xmlNewDoc("1.0");
        xmlDocSetRootElement(rdoc, res);
    }
    else
    {
        yaz_log(YLOG_FATAL, "Unknown native_syntax in normalize_record");
        exit(1);
    }
    for (m = rprofile->maplist; m; m = m->next)
    {
        xmlDoc *new;
        if (m->type != Map_xslt)
        {
            yaz_log(YLOG_WARN, "Unknown map type");
            return 0;
        }
        if (!(new = xsltApplyStylesheet(m->stylesheet, rdoc, 0)))
        {
            yaz_log(YLOG_WARN, "XSLT transformation failed");
            return 0;
        }
        xmlFreeDoc(rdoc);
        rdoc = new;
    }
    if (global_parameters.dump_records)
    {
        fprintf(stderr, "Record:\n----------------\n");
        xmlDocFormatDump(stderr, rdoc, 1);
    }
    return rdoc;
}

static struct record *ingest_record(struct client *cl, Z_External *rec)
{
    xmlDoc *xdoc = normalize_record(cl, rec);
    xmlNode *root, *n;
    struct record *res, *head;
    struct session *se = cl->session;
    xmlChar *mergekey, *mergekey_norm;

    if (!xdoc)
        return 0;

    root = xmlDocGetRootElement(xdoc);
    if (!(mergekey = xmlGetProp(root, "mergekey")))
    {
        yaz_log(YLOG_WARN, "No mergekey found in record");
        return 0;
    }

    res = nmem_malloc(se->nmem, sizeof(struct record));
    res->next_cluster = 0;
    res->target_offset = -1;
    res->term_frequency_vec = 0;
    res->title = "Unknown";
    res->relevance = 0;

    mergekey_norm = nmem_strdup(se->nmem, (char*) mergekey);
    xmlFree(mergekey);
    res->merge_key = normalize_mergekey(mergekey_norm);

    head = reclist_insert(se->reclist, res);
    relevance_newrec(se->relevance, head);

    for (n = root->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp(n->name, "facet"))
        {
            xmlChar *type = xmlGetProp(n, "type");
            xmlChar *value = xmlNodeListGetString(xdoc, n->children, 0);
            add_facet(se, type, value);
            relevance_countwords(se->relevance, head, value, 1);
            xmlFree(type);
            xmlFree(value);
        }
        else if (!strcmp(n->name, "metadata"))
        {
            xmlChar *type = xmlGetProp(n, "type"), *value;
            if (!strcmp(type, "title"))
                res->title = nmem_strdup(se->nmem,
                        value = xmlNodeListGetString(xdoc, n->children, 0));

            relevance_countwords(se->relevance, head, value, 4);
            xmlFree(type);
            xmlFree(value);
        }
        else
            yaz_log(YLOG_WARN, "Unexpected element %s in internal record", n->name);
    }

    xmlFreeDoc(xdoc);

    relevance_donerecord(se->relevance, head);
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

        if (npr->which != Z_NamePlusRecord_databaseRecord)
        {
            yaz_log(YLOG_WARN, "Unexpected record type, probably diagnostic");
            continue;
        }

        rec = ingest_record(cl, npr->u.databaseRecord);
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
        yaz_log(YLOG_WARN, "Destroying orphan connection");
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
        {
            cl->state = Client_Connecting;
            iochan_setflag(co->iochan, EVENT_OUTPUT);
        }
        else if (co->state == Conn_Open)
        {
            if (cl->state == Client_Error || cl->state == Client_Disconnected)
                cl->state = Client_Idle;
            iochan_setflag(co->iochan, EVENT_OUTPUT);
        }
        return 1;
    }
    else
        return 0;
}

// This function will most likely vanish when a proper target profile mechanism is
// introduced.
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
                xfree(host->hostport);
                xfree(host);
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
        
        database->databases = xmalloc(2 * sizeof(char *));
        database->databases[0] = xstrdup(db);
        database->databases[1] = 0;
        database->errors = 0;
        database->qprofile = 0;
        database->rprofile = database_retrieval_profile(database);
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

// This needs to be extended with selection criteria
static struct conf_retrievalprofile *database_retrieval_profile(struct database *db)
{
    if (!config)
    {
        yaz_log(YLOG_FATAL, "Must load configuration (-f)");
        exit(1);
    }
    if (!config->retrievalprofiles)
    {
        yaz_log(YLOG_FATAL, "No retrieval profiles defined");
    }
    return config->retrievalprofiles;
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

int session_active_clients(struct session *s)
{
    struct client *c;
    int res = 0;

    for (c = s->clients; c; c = c->next)
        if (c->connection && (c->state == Client_Connecting ||
                    c->state == Client_Initializing ||
                    c->state == Client_Searching ||
                    c->state == Client_Presenting))
            res++;

    return res;
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
        se->num_termlists = 0;
        se->reclist = reclist_create(se->nmem, maxrecs);
        extract_terms(se->nmem, query, p);
        se->relevance = relevance_create(se->nmem, (const char **) p, maxrecs);
        se->total_records = se->total_hits = 0;
        se->expected_maxrecs = maxrecs;
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
    session->num_termlists = 0;
    session->reclist = 0;
    session->requestid = -1;
    session->clients = 0;
    session->expected_maxrecs = 0;
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

struct termlist_score **termlist(struct session *s, const char *name, int *num)
{
    int i;

    for (i = 0; i < s->num_termlists; i++)
        if (!strcmp(s->termlists[i].name, name))
            return termlist_highscore(s->termlists[i].termlist, num);
    return 0;
}

#ifdef REPORT_NMEM
// conditional compilation by SH: This lead to a warning with currently installed
// YAZ header files on us1
void report_nmem_stats(void)
{
    size_t in_use, is_free;

    nmem_get_memory_in_use(&in_use);
    nmem_get_memory_free(&is_free);

    yaz_log(YLOG_LOG, "nmem stat: use=%ld free=%ld", 
            (long) in_use, (long) is_free);
}
#endif

struct record **show(struct session *s, int start, int *num, int *total,
                     int *sumhits, NMEM nmem_show)
{
    struct record **recs = nmem_malloc(nmem_show, *num 
                                       * sizeof(struct record *));
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

    while ((ret = options("f:x:c:h:p:C:s:d", argv, argc, &arg)) != -2)
    {
	switch (ret) {
            case 'f':
                if (!read_config(arg))
                    exit(1);
                break;
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
            case 'd':
                global_parameters.dump_records = 1;
                break;
	    default:
		fprintf(stderr, "Usage: pazpar2\n"
                        "    -f configfile\n"
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

    global_parameters.ccl_filter = load_cclfile("../etc/default.bib");
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
