/* $Id: pazpar2.c,v 1.61 2007-04-03 04:05:01 quinn Exp $ */

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

#if HAVE_CONFIG_H
#include "cconfig.h"
#endif

#define USE_TIMING 0
#if USE_TIMING
#include <yaz/timing.h>
#endif

#include <netinet/in.h>

#include "pazpar2.h"
#include "eventl.h"
#include "http.h"
#include "termlists.h"
#include "reclists.h"
#include "relevance.h"
#include "config.h"
#include "database.h"
#include "settings.h"

#define MAX_CHUNK 15

static void client_fatal(struct client *cl);
static void connection_destroy(struct connection *co);
static int client_prep_connection(struct client *cl);
static void ingest_records(struct client *cl, Z_Records *r);
//static struct conf_retrievalprofile *database_retrieval_profile(struct database *db);
void session_alert_watch(struct session *s, int what);
char *session_setting_oneval(struct session *s, struct database *db, const char *name);

IOCHAN channel_list = 0;  // Master list of connections we're handling events to

static struct connection *connection_freelist = 0;
static struct client *client_freelist = 0;

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

// Note: Some things in this structure will eventually move to configuration
struct parameters global_parameters = 
{
    "",
    "",
    "",
    "",
    0,
    0,
    30,
    "81",
    "Index Data PazPar2 (MasterKey)",
    VERSION,
    600, // 10 minutes
    60,
    100,
    MAX_CHUNK,
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


    /* add virtual host if tunneling through Z39.50 proxy */
    
    if (0 < strlen(global_parameters.zproxy_override) 
        && 0 < strlen(cl->database->url))
        yaz_oi_set_string_oidval(&a->u.initRequest->otherInfo, 
                                 global_parameters.odr_out, VAL_PROXY,
                                 1, cl->database->url);
    


    if (send_apdu(cl, a) >= 0)
    {
	iochan_setflags(i, EVENT_INPUT);
	cl->state = Client_Initializing;
    }
    else
        cl->state = Client_Error;
    odr_reset(global_parameters.odr_out);
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
static void extract_terms(NMEM nmem, struct ccl_rpn_node *query, char **termlist)
{
    int num = 0;

    pull_terms(nmem, query, termlist, &num);
    termlist[num] = 0;
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
    char *recsyn;
    char *piggyback;

    yaz_log(YLOG_DEBUG, "Sending search");

    cn = ccl_find_str(db->ccl_map, se->query, &cerror, &cpos);
    if (!cn)
        return;

    if (!se->relevance)
    {
        // Initialize relevance structure with query terms
        char *p[512];
        extract_terms(se->nmem, cn, p);
        se->relevance = relevance_create(se->nmem, (const char **) p,
                se->expected_maxrecs);
    }

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

    if (!(piggyback = session_setting_oneval(se, db, "pz:piggyback")) || *piggyback == '1')
    {
        if ((recsyn = session_setting_oneval(se, db, "pz:syntax")))
            a->u.searchRequest->preferredRecordSyntax =
                    yaz_str_to_z3950oid(global_parameters.odr_out,
                    CLASS_RECSYN, recsyn);
        a->u.searchRequest->smallSetUpperBound = &ssub;
        a->u.searchRequest->largeSetLowerBound = &lslb;
        a->u.searchRequest->mediumSetPresentNumber = &mspn;
    }
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
    struct session *se = cl->session;
    struct database *db = cl->database;
    Z_APDU *a = zget_APDU(global_parameters.odr_out, Z_APDU_presentRequest);
    int toget;
    int start = cl->records + 1;
    char *recsyn;

    toget = global_parameters.chunk;
    if (toget > global_parameters.toget - cl->records)
        toget = global_parameters.toget - cl->records;
    if (toget > cl->hits - cl->records)
	toget = cl->hits - cl->records;

    yaz_log(YLOG_DEBUG, "Trying to present %d records\n", toget);

    a->u.presentRequest->resultSetStartPoint = &start;
    a->u.presentRequest->numberOfRecordsRequested = &toget;

    a->u.presentRequest->resultSetId = "Default";

    if ((recsyn = session_setting_oneval(se, db, "pz:syntax")))
        a->u.presentRequest->preferredRecordSyntax =
                yaz_str_to_z3950oid(global_parameters.odr_out,
                CLASS_RECSYN, recsyn);

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

char *normalize_mergekey(char *buf, int skiparticle)
{
    char *p = buf, *pout = buf;

    if (skiparticle)
    {
        char firstword[64];
        char articles[] = "the den der die des an a "; // must end in space

        while (*p && !isalnum(*p))
            p++;
        pout = firstword;
        while (*p && *p != ' ' && pout - firstword < 62)
            *(pout++) = tolower(*(p++));
        *(pout++) = ' ';
        *(pout++) = '\0';
        if (!strstr(articles, firstword))
            p = buf;
        pout = buf;
    }

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
        do {
            *(pout--) = '\0';
        }
        while (pout > buf && *pout == ' ');

    return buf;
}

static void add_facet(struct session *s, const char *type, const char *value)
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
        rdoc = xmlNewDoc((xmlChar *) "1.0");
        xmlDocSetRootElement(rdoc, res);
    }
    else
    {
        yaz_log(YLOG_FATAL, "Unknown native_syntax in normalize_record");
        exit(1);
    }

    if (global_parameters.dump_records)
    {
        fprintf(stderr, "Input Record (normalized):\n----------------\n");
#if LIBXML_VERSION >= 20600
        xmlDocFormatDump(stderr, rdoc, 1);
#else
        xmlDocDump(stderr, rdoc);
#endif
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
#if LIBXML_VERSION >= 20600
        xmlDocFormatDump(stderr, rdoc, 1);
#else
        xmlDocDump(stderr, rdoc);
#endif
    }
    return rdoc;
}

// Extract what appears to be years from buf, storing highest and
// lowest values.
static int extract_years(const char *buf, int *first, int *last)
{
    *first = -1;
    *last = -1;
    while (*buf)
    {
        const char *e;
        int len;

        while (*buf && !isdigit(*buf))
            buf++;
        len = 0;
        for (e = buf; *e && isdigit(*e); e++)
            len++;
        if (len == 4)
        {
            int value = atoi(buf);
            if (*first < 0 || value < *first)
                *first = value;
            if (*last < 0 || value > *last)
                *last = value;
        }
        buf = e;
    }
    return *first;
}

static struct record *ingest_record(struct client *cl, Z_External *rec)
{
    xmlDoc *xdoc = normalize_record(cl, rec);
    xmlNode *root, *n;
    struct record *res;
    struct record_cluster *cluster;
    struct session *se = cl->session;
    xmlChar *mergekey, *mergekey_norm;
    xmlChar *type = 0;
    xmlChar *value = 0;
    struct conf_service *service = global_parameters.server->service;

    if (!xdoc)
        return 0;

    root = xmlDocGetRootElement(xdoc);
    if (!(mergekey = xmlGetProp(root, (xmlChar *) "mergekey")))
    {
        yaz_log(YLOG_WARN, "No mergekey found in record");
        xmlFreeDoc(xdoc);
        return 0;
    }

    res = nmem_malloc(se->nmem, sizeof(struct record));
    res->next = 0;
    res->client = cl;
    res->metadata = nmem_malloc(se->nmem,
            sizeof(struct record_metadata*) * service->num_metadata);
    memset(res->metadata, 0, sizeof(struct record_metadata*) * service->num_metadata);

    mergekey_norm = (xmlChar *) nmem_strdup(se->nmem, (char*) mergekey);
    xmlFree(mergekey);
    normalize_mergekey((char *) mergekey_norm, 0);

    cluster = reclist_insert(se->reclist, res, (char *) mergekey_norm, 
                             &se->total_merged);
    if (global_parameters.dump_records)
        yaz_log(YLOG_LOG, "Cluster id %d from %s (#%d)", cluster->recid,
                cl->database->url, cl->records);
    if (!cluster)
    {
        /* no room for record */
        xmlFreeDoc(xdoc);
        return 0;
    }
    relevance_newrec(se->relevance, cluster);

    for (n = root->children; n; n = n->next)
    {
        if (type)
            xmlFree(type);
        if (value)
            xmlFree(value);
        type = value = 0;

        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp((const char *) n->name, "metadata"))
        {
            struct conf_metadata *md = 0;
            struct conf_sortkey *sk = 0;
            struct record_metadata **wheretoput, *newm;
            int imeta;
            int first, last;

            type = xmlGetProp(n, (xmlChar *) "type");
            value = xmlNodeListGetString(xdoc, n->children, 0);

            if (!type || !value)
                continue;

            // First, find out what field we're looking at
            for (imeta = 0; imeta < service->num_metadata; imeta++)
                if (!strcmp((const char *) type, service->metadata[imeta].name))
                {
                    md = &service->metadata[imeta];
                    if (md->sortkey_offset >= 0)
                        sk = &service->sortkeys[md->sortkey_offset];
                    break;
                }
            if (!md)
            {
                yaz_log(YLOG_WARN, "Ignoring unknown metadata element: %s", type);
                continue;
            }

            // Find out where we are putting it
            if (md->merge == Metadata_merge_no)
                wheretoput = &res->metadata[imeta];
            else
                wheretoput = &cluster->metadata[imeta];
            
            // Put it there
            newm = nmem_malloc(se->nmem, sizeof(struct record_metadata));
            newm->next = 0;
            if (md->type == Metadata_type_generic)
            {
                char *p, *pe;
                for (p = (char *) value; *p && isspace(*p); p++)
                    ;
                for (pe = p + strlen(p) - 1;
                        pe > p && strchr(" ,/.:([", *pe); pe--)
                    *pe = '\0';
                newm->data.text = nmem_strdup(se->nmem, p);

            }
            else if (md->type == Metadata_type_year)
            {
                if (extract_years((char *) value, &first, &last) < 0)
                    continue;
            }
            else
            {
                yaz_log(YLOG_WARN, "Unknown type in metadata element %s", type);
                continue;
            }
            if (md->type == Metadata_type_year && md->merge != Metadata_merge_range)
            {
                yaz_log(YLOG_WARN, "Only range merging supported for years");
                continue;
            }
            if (md->merge == Metadata_merge_unique)
            {
                struct record_metadata *mnode;
                for (mnode = *wheretoput; mnode; mnode = mnode->next)
                    if (!strcmp((const char *) mnode->data.text, newm->data.text))
                        break;
                if (!mnode)
                {
                    newm->next = *wheretoput;
                    *wheretoput = newm;
                }
            }
            else if (md->merge == Metadata_merge_longest)
            {
                if (!*wheretoput ||
                        strlen(newm->data.text) > strlen((*wheretoput)->data.text))
                {
                    *wheretoput = newm;
                    if (sk)
                    {
                        char *s = nmem_strdup(se->nmem, newm->data.text);
                        if (!cluster->sortkeys[md->sortkey_offset])
                            cluster->sortkeys[md->sortkey_offset] = 
                                nmem_malloc(se->nmem, sizeof(union data_types));
                        normalize_mergekey(s,
                                (sk->type == Metadata_sortkey_skiparticle));
                        cluster->sortkeys[md->sortkey_offset]->text = s;
                    }
                }
            }
            else if (md->merge == Metadata_merge_all || md->merge == Metadata_merge_no)
            {
                newm->next = *wheretoput;
                *wheretoput = newm;
            }
            else if (md->merge == Metadata_merge_range)
            {
                assert(md->type == Metadata_type_year);
                if (!*wheretoput)
                {
                    *wheretoput = newm;
                    (*wheretoput)->data.number.min = first;
                    (*wheretoput)->data.number.max = last;
                    if (sk)
                        cluster->sortkeys[md->sortkey_offset] = &newm->data;
                }
                else
                {
                    if (first < (*wheretoput)->data.number.min)
                        (*wheretoput)->data.number.min = first;
                    if (last > (*wheretoput)->data.number.max)
                        (*wheretoput)->data.number.max = last;
                }
#ifdef GAGA
                if (sk)
                {
                    union data_types *sdata = cluster->sortkeys[md->sortkey_offset];
                    yaz_log(YLOG_LOG, "SK range: %d-%d", sdata->number.min, sdata->number.max);
                }
#endif
            }
            else
                yaz_log(YLOG_WARN, "Don't know how to merge on element name %s", md->name);

            if (md->rank)
                relevance_countwords(se->relevance, cluster, 
                                     (char *) value, md->rank);
            if (md->termlist)
            {
                if (md->type == Metadata_type_year)
                {
                    char year[64];
                    sprintf(year, "%d", last);
                    add_facet(se, (char *) type, year);
                    if (first != last)
                    {
                        sprintf(year, "%d", first);
                        add_facet(se, (char *) type, year);
                    }
                }
                else
                    add_facet(se, (char *) type, (char *) value);
            }
            xmlFree(type);
            xmlFree(value);
            type = value = 0;
        }
        else
            yaz_log(YLOG_WARN, "Unexpected element %s in internal record", n->name);
    }
    if (type)
        xmlFree(type);
    if (value)
        xmlFree(value);

    xmlFreeDoc(xdoc);

    relevance_donerecord(se->relevance, cluster);
    se->total_records++;

    return res;
}

// Retrieve first defined value for 'name' for given database.
// Will be extended to take into account user associated with session
char *session_setting_oneval(struct session *s, struct database *db, const char *name)
{
    int offset = settings_offset(name);

    if (offset < 0)
        return 0;
    if (!db->settings[offset])
        return 0;
    return db->settings[offset]->value;
}

static void ingest_records(struct client *cl, Z_Records *r)
{
#if USE_TIMING
    yaz_timing_t t = yaz_timing_create();
#endif
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

        cl->records++;
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

#if USE_TIMING
    yaz_timing_stop(t);
    yaz_log(YLOG_LOG, "ingest_records %6.5f %3.2f %3.2f", 
            yaz_timing_get_real(t), yaz_timing_get_user(t),
            yaz_timing_get_sys(t));
    yaz_timing_destroy(&t);
#endif
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


    if (!(link = cs_create(tcpip_type, 0, PROTO_Z3950)))
        {
            yaz_log(YLOG_FATAL|YLOG_ERRNO, "Failed to create comstack");
            exit(1);
        }
    
    if (0 == strlen(global_parameters.zproxy_override)){
        /* no Z39.50 proxy needed - direct connect */
        yaz_log(YLOG_DEBUG, "Connection create %s", cl->database->url);
        
        if (!(addr = cs_straddr(link, cl->database->host->ipport)))
            {
                yaz_log(YLOG_WARN|YLOG_ERRNO, 
                        "Lookup of IP address %s failed", 
                        cl->database->host->ipport);
                return 0;
            }
    
    } else {
        /* Z39.50 proxy connect */
        yaz_log(YLOG_DEBUG, "Connection create %s proxy %s", 
                cl->database->url, global_parameters.zproxy_override);

        yaz_log(YLOG_LOG, "Connection cs_create_host %s proxy %s", 
                cl->database->url, global_parameters.zproxy_override);
        
        if (!(addr = cs_straddr(link, global_parameters.zproxy_override)))
            {
                yaz_log(YLOG_WARN|YLOG_ERRNO, 
                        "Lookup of IP address %s failed", 
                        global_parameters.zproxy_override);
                return 0;
            }
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

    new->iochan = iochan_create(cs_fileno(link), 0, handler, 0);
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

#ifdef GAGA // Moved to database.c

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
        char *name;
        struct host *host;
        struct database *database;

        if (strncmp(line, "target ", 7))
            continue;
        line[strlen(line) - 1] = '\0';

        if ((name = strchr(line, ';')))
            *(name++) = '\0';

        url = line + 7;
        if ((db = strchr(url, '/')))
            *(db++) = '\0';
        else
            db = "Default";

        yaz_log(YLOG_LOG, "Target: %s, '%s'", url, db);
        for (host = hosts; host; host = host->next)
            if (!strcmp((const char *) url, host->hostport))
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
            sprintf(ipport, "%u.%u.%u.%u:%s",
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
        if (name)
            database->name = xstrdup(name);
        else
            database->name = 0;
        
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

#endif

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

//callback for grep_databases
static void select_targets_callback(void *context, struct database *db)
{
    struct session *se = (struct session*) context;
    struct client *cl = client_create();
    cl->database = db;
    cl->session = se;
    cl->next = se->clients;
    se->clients = cl;
}

// This should be extended with parameters to control selection criteria
// Associates a set of clients with a session;
int select_targets(struct session *se, struct database_criterion *crit)
{
    while (se->clients)
        client_destroy(se->clients);

    return grep_databases(se, crit, select_targets_callback);
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

// parses crit1=val1,crit2=val2|val3,...
static struct database_criterion *parse_filter(NMEM m, const char *buf)
{
    struct database_criterion *res = 0;
    char **values;
    int num;
    int i;

    if (!buf || !*buf)
        return 0;
    nmem_strsplit(m, ",", buf,  &values, &num);
    for (i = 0; i < num; i++)
    {
        char **subvalues;
        int subnum;
        int subi;
        struct database_criterion *new = nmem_malloc(m, sizeof(*new));
        char *eq = strchr(values[i], '=');
        if (!eq)
        {
            yaz_log(YLOG_WARN, "Missing equal-sign in filter");
            return 0;
        }
        *(eq++) = '\0';
        new->name = values[i];
        nmem_strsplit(m, "|", eq, &subvalues, &subnum);
        new->values = 0;
        for (subi = 0; subi < subnum; subi++)
        {
            struct database_criterion_value *newv = nmem_malloc(m, sizeof(*newv));
            newv->value = subvalues[subi];
            newv->next = new->values;
            new->values = newv;
        }
        new->next = res;
        res = new;
    }
    return res;
}

char *search(struct session *se, char *query, char *filter)
{
    int live_channels = 0;
    struct client *cl;
    struct database_criterion *criteria;

    yaz_log(YLOG_DEBUG, "Search");

    nmem_reset(se->nmem);
    criteria = parse_filter(se->nmem, filter);
    strcpy(se->query, query);
    se->requestid++;
    // Release any existing clients
    select_targets(se, criteria);
    for (cl = se->clients; cl; cl = cl->next)
    {
        if (client_prep_connection(cl))
            live_channels++;
    }
    if (live_channels)
    {
        int maxrecs = live_channels * global_parameters.toget;
        se->num_termlists = 0;
        se->reclist = reclist_create(se->nmem, maxrecs);
        // This will be initialized in send_search()
        se->relevance = 0;
        se->total_records = se->total_hits = se->total_merged = 0;
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
    wrbuf_destroy(s->wrbuf);
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

    return session;
}

struct hitsbytarget *hitsbytarget(struct session *se, int *count)
{
    static struct hitsbytarget res[1000]; // FIXME MM
    struct client *cl;

    *count = 0;
    for (cl = se->clients; cl; cl = cl->next)
    {
        res[*count].id = cl->database->url;
        res[*count].name = cl->database->name;
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
        if (!strcmp((const char *) s->termlists[i].name, name))
            return termlist_highscore(s->termlists[i].termlist, num);
    return 0;
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

struct record_cluster *show_single(struct session *s, int id)
{
    struct record_cluster *r;

    reclist_rewind(s->reclist);
    while ((r = reclist_read_record(s->reclist)))
        if (r->recid == id)
            return r;
    return 0;
}

struct record_cluster **show(struct session *s, struct reclist_sortparms *sp, int start,
        int *num, int *total, int *sumhits, NMEM nmem_show)
{
    struct record_cluster **recs = nmem_malloc(nmem_show, *num 
                                       * sizeof(struct record_cluster *));
    struct reclist_sortparms *spp;
    int i;
#if USE_TIMING    
    yaz_timing_t t = yaz_timing_create();
#endif

    for (spp = sp; spp; spp = spp->next)
        if (spp->type == Metadata_sortkey_relevance)
        {
            relevance_prepare_read(s->relevance, s->reclist);
            break;
        }
    reclist_sort(s->reclist, sp);

    *total = s->reclist->num_records;
    *sumhits = s->total_hits;

    for (i = 0; i < start; i++)
        if (!reclist_read_record(s->reclist))
        {
            *num = 0;
            recs = 0;
            break;
        }

    for (i = 0; i < *num; i++)
    {
        struct record_cluster *r = reclist_read_record(s->reclist);
        if (!r)
        {
            *num = i;
            break;
        }
        recs[i] = r;
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

void statistics(struct session *se, struct statistics *stat)
{
    struct client *cl;
    int count = 0;

    memset(stat, 0, sizeof(*stat));
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

static void start_http_listener(void)
{
    char hp[128] = "";
    struct conf_server *ser = global_parameters.server;

    if (*global_parameters.listener_override)
        strcpy(hp, global_parameters.listener_override);
    else
    {
        strcpy(hp, ser->host ? ser->host : "");
        if (ser->port)
        {
            if (*hp)
                strcat(hp, ":");
            sprintf(hp + strlen(hp), "%d", ser->port);
        }
    }
    http_init(hp);
}

// Initialize CCL map for a target
// Note: This approach ignores user-specific CCL maps, for which I
// don't presently see any application.
static void prepare_cclmap(void *context, struct database *db)
{
    struct setting *s;

    if (!db->settings)
        return;
    db->ccl_map = ccl_qual_mk();
    for (s = db->settings[PZ_CCLMAP]; s; s = s->next)
        if (!*s->user)
        {
            char *p = strchr(s->name + 3, ':');
            if (!p)
            {
                yaz_log(YLOG_FATAL, "Malformed cclmap name: %s", s->name);
                exit(1);
            }
            p++;
            ccl_qual_fitem(db->ccl_map, s->value, p);
        }
}

// Read settings for each database, and prepare a CCL map for that database
static void prepare_cclmaps(void)
{
    grep_databases(0, 0, prepare_cclmap);
}

static void start_proxy(void)
{
    char hp[128] = "";
    struct conf_server *ser = global_parameters.server;

    if (*global_parameters.proxy_override)
        strcpy(hp, global_parameters.proxy_override);
    else if (ser->proxy_host || ser->proxy_port)
    {
        strcpy(hp, ser->proxy_host ? ser->proxy_host : "");
        if (ser->proxy_port)
        {
            if (*hp)
                strcat(hp, ":");
            sprintf(hp + strlen(hp), "%d", ser->proxy_port);
        }
    }
    else
        return;

    http_set_proxyaddr(hp, ser->myurl ? ser->myurl : "");
}

static void start_zproxy(void)
{
    struct conf_server *ser = global_parameters.server;

    if (*global_parameters.zproxy_override){
        yaz_log(YLOG_LOG, "Z39.50 proxy  %s", 
                global_parameters.zproxy_override);
        return;
    }

    else if (ser->zproxy_host || ser->zproxy_port)
    {
        char hp[128] = "";

        strcpy(hp, ser->zproxy_host ? ser->zproxy_host : "");
        if (ser->zproxy_port)
        {
            if (*hp)
                strcat(hp, ":");
            else
                strcat(hp, "@:");

            sprintf(hp + strlen(hp), "%d", ser->zproxy_port);
        }
        strcpy(global_parameters.zproxy_override, hp);
        yaz_log(YLOG_LOG, "Z39.50 proxy  %s", 
                global_parameters.zproxy_override);

    }
    else
        return;
}



int main(int argc, char **argv)
{
    int ret;
    char *arg;

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        yaz_log(YLOG_WARN|YLOG_ERRNO, "signal");

    yaz_log_init(YLOG_DEFAULT_LEVEL, "pazpar2", 0);

    while ((ret = options("t:f:x:h:p:z:s:d", argv, argc, &arg)) != -2)
    {
	switch (ret) {
            case 'f':
                if (!read_config(arg))
                    exit(1);
                break;
            case 'h':
                strcpy(global_parameters.listener_override, arg);
                break;
            case 'p':
                strcpy(global_parameters.proxy_override, arg);
                break;
            case 'z':
                strcpy(global_parameters.zproxy_override, arg);
                break;
            case 't':
                strcpy(global_parameters.settings_path_override, arg);
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
                        "    -C cclconfig\n"
                        "    -s simpletargetfile\n"
                        "    -p hostname[:portno]    (HTTP proxy)\n"
                        "    -z hostname[:portno]    (Z39.50 proxy)\n"
                        "    -d                      (show internal records)\n");
		exit(1);
	}
    }

    if (!config)
    {
        yaz_log(YLOG_FATAL, "Load config with -f");
        exit(1);
    }
    global_parameters.server = config->servers;

    start_http_listener();
    start_proxy();
    start_zproxy();

    if (*global_parameters.settings_path_override)
        settings_read(global_parameters.settings_path_override);
    else if (global_parameters.server->settings)
        settings_read(global_parameters.server->settings);
    else
        yaz_log(YLOG_WARN, "No settings-directory specified. Problems may ensue!");
    prepare_cclmaps();
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
