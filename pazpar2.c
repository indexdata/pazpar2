/* $Id: pazpar2.c,v 1.2 2006-11-18 05:00:38 quinn Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/socket.h>
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

#define PAZPAR2_VERSION "0.1"
#define MAX_DATABASES 512
#define MAX_CHUNK 10

struct target
{
    struct session *session;
    char fullname[256];
    char hostport[128];
    char *ibuf;
    int ibufsize;
    char databases[MAX_DATABASES][128];
    COMSTACK link;
    ODR odr_in, odr_out;
    struct target *next;
    void *addr;
    int hits;
    int records;
    int setno;
    int requestid;                              // ID of current outstanding request
    int diagnostic;
    enum target_state
    {
	No_connection,
        Connecting,
        Connected,
        Initializing,
        Searching,
        Presenting,
        Error,
	Idle,
        Failed
    } state;
};

static char *state_strings[] = {
    "No_connection",
    "Connecting",
    "Connected",
    "Initializing",
    "Searching",
    "Presenting",
    "Error",
    "Idle",
    "Failed"
};


IOCHAN channel_list = 0;

static struct parameters {
    int timeout;		/* operations timeout, in seconds */
    char implementationId[128];
    char implementationName[128];
    char implementationVersion[128];
    struct timeval base_time;
    int toget;
    int chunk;
} global_parameters = 
{
    30,
    "81",
    "Index Data PazPar2 (MasterKey)",
    PAZPAR2_VERSION,
    {0,0},
    100,
    MAX_CHUNK
};


static int send_apdu(struct target *t, Z_APDU *a)
{
    char *buf;
    int len, r;

    if (!z_APDU(t->odr_out, &a, 0, 0))
    {
        odr_perror(t->odr_out, "Encoding APDU");
	abort();
    }
    buf = odr_getbuf(t->odr_out, &len, 0);
    r = cs_put(t->link, buf, len);
    if (r < 0)
    {
        yaz_log(YLOG_WARN, "cs_put: %s", cs_errmsg(cs_errno(t->link)));
        return -1;
    }
    else if (r == 1)
    {
        fprintf(stderr, "cs_put incomplete (ParaZ does not handle that)\n");
    }
    odr_reset(t->odr_out); /* release the APDU structure  */
    return 0;
}


static void send_init(IOCHAN i)
{
    struct target *t = iochan_getdata(i);
    Z_APDU *a = zget_APDU(t->odr_out, Z_APDU_initRequest);

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
    if (send_apdu(t, a) >= 0)
    {
	iochan_setflags(i, EVENT_INPUT);
	t->state = Initializing;
    }
    else
    {
	iochan_destroy(i);
	t->state = Failed;
	cs_close(t->link);
    }
}

static void send_search(IOCHAN i)
{
    struct target *t = iochan_getdata(i);
    struct session *s = t->session;
    Z_APDU *a = zget_APDU(t->odr_out, Z_APDU_searchRequest);
    int ndb;
    char **databaselist;
    Z_Query *zquery;

    yaz_log(YLOG_DEBUG, "Sending search");
    a->u.searchRequest->query = zquery = odr_malloc(t->odr_out, sizeof(Z_Query));
    zquery->which = Z_Query_type_1;
    zquery->u.type_1 = p_query_rpn(t->odr_out, PROTO_Z3950, s->query);

    for (ndb = 0; *t->databases[ndb]; ndb++)
	;
    databaselist = odr_malloc(t->odr_out, sizeof(char*) * ndb);
    for (ndb = 0; *t->databases[ndb]; ndb++)
	databaselist[ndb] = t->databases[ndb];

    a->u.searchRequest->resultSetName = "Default";
    a->u.searchRequest->databaseNames = databaselist;
    a->u.searchRequest->num_databaseNames = ndb;

    if (send_apdu(t, a) >= 0)
    {
	iochan_setflags(i, EVENT_INPUT);
	t->state = Searching;
        t->requestid = s->requestid;
    }
    else
    {
	iochan_destroy(i);
	t->state = Failed;
	cs_close(t->link);
    }
    odr_reset(t->odr_out);
}

static void send_present(IOCHAN i)
{
    struct target *t = iochan_getdata(i);
    Z_APDU *a = zget_APDU(t->odr_out, Z_APDU_presentRequest);
    int toget;
    int start = t->records + 1;

    toget = global_parameters.chunk;
    if (toget > t->hits - t->records)
	toget = t->hits - t->records;

    yaz_log(YLOG_DEBUG, "Trying to present %d records\n", toget);

    a->u.presentRequest->resultSetStartPoint = &start;
    a->u.presentRequest->numberOfRecordsRequested = &toget;

    a->u.presentRequest->resultSetId = "Default";

    if (send_apdu(t, a) >= 0)
    {
	iochan_setflags(i, EVENT_INPUT);
	t->state = Presenting;
    }
    else
    {
	iochan_destroy(i);
	t->state = Failed;
	cs_close(t->link);
    }
    odr_reset(t->odr_out);
}

static void do_initResponse(IOCHAN i, Z_APDU *a)
{
    struct target *t = iochan_getdata(i);
    Z_InitResponse *r = a->u.initResponse;

    yaz_log(YLOG_DEBUG, "Received init response");

    if (*r->result)
    {
	t->state = Idle;
    }
    else
    {
	t->state = Failed;
	iochan_destroy(i);
	cs_close(t->link);
    }
}

static void do_searchResponse(IOCHAN i, Z_APDU *a)
{
    struct target *t = iochan_getdata(i);
    Z_SearchResponse *r = a->u.searchResponse;

    yaz_log(YLOG_DEBUG, "Searchresponse (status=%d)", *r->searchStatus);

    if (*r->searchStatus)
    {
	t->hits = *r->resultCount;
        t->state = Idle;
    }
    else
    {          /*"FAILED"*/
	t->hits = 0;
        t->state = Failed;
        if (r->records) {
            Z_Records *recs = r->records;
            if (recs->which == Z_Records_NSD)
            {
                yaz_log(YLOG_WARN, "Non-surrogate diagnostic");
                t->diagnostic = *recs->u.nonSurrogateDiagnostic->condition;
                t->state = Error;
            }
        }
    }
}

const char *find_field(const char *rec, const char *field)
{
    const char *line = rec;

    while (*line)
    {
        if (!strncmp(line, field, 3) && line[3] == ' ')
            return line;
        while (*(line++) != '\n')
            ;
    }
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
                return ++p;
        }
    }
    return 0;
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
    struct record *res = s->recheap[0];
    int p = 0;
    int lastnonleaf = (s->recheap_max - 1) >> 1;

    if (s->recheap_max < 0)
        return 0;

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

struct record *ingest_record(struct target *t, char *buf, int len)
{
    struct session *s = t->session;
    struct record *res;
    const char *recbuf;

    wrbuf_rewind(s->wrbuf);
    yaz_marc_xml(s->yaz_marc, YAZ_MARC_LINE);
    if (yaz_marc_decode_wrbuf(s->yaz_marc, buf, len, s->wrbuf) < 0)
    {
        yaz_log(YLOG_WARN, "Failed to decode MARC record");
        return 0;
    }
    wrbuf_putc(s->wrbuf, '\0');
    recbuf = wrbuf_buf(s->wrbuf);

    res = nmem_malloc(s->nmem, sizeof(struct record));

    res->merge_key = extract_mergekey(s, recbuf);
    if (!res->merge_key)
        return 0;
    res->buf = nmem_strdupn(s->nmem, recbuf, wrbuf_len(s->wrbuf));
    res->target = t;
    res->next_cluster = 0;
    res->target_offset = -1;

    yaz_log(YLOG_DEBUG, "Key: %s", res->merge_key);

    push_record(s, res);

    return res;
}

void ingest_records(struct target *t, Z_Records *r)
{
    //struct session *s = t->session;
    struct record *rec;
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

        rec = ingest_record(t, buf, len);
        if (!rec)
            continue;
        yaz_log(YLOG_DEBUG, "Ingested a fooking record");
    }
}

static void do_presentResponse(IOCHAN i, Z_APDU *a)
{
    struct target *t = iochan_getdata(i);
    Z_PresentResponse *r = a->u.presentResponse;

    if (r->records) {
        Z_Records *recs = r->records;
        if (recs->which == Z_Records_NSD)
        {
            yaz_log(YLOG_WARN, "Non-surrogate diagnostic");
            t->diagnostic = *recs->u.nonSurrogateDiagnostic->condition;
            t->state = Error;
        }
        else
        {
            yaz_log(YLOG_DEBUG, "Got Records!");
        }
    }

    if (!*r->presentStatus && t->state != Error)
    {
        yaz_log(YLOG_DEBUG, "Good Present response");
        t->records += *r->numberOfRecordsReturned;
        ingest_records(t, r->records);
        t->state = Idle;
    }
    else if (*r->presentStatus) 
    {
        yaz_log(YLOG_WARN, "Bad Present response");
        t->state = Error;
    }
}

static void handler(IOCHAN i, int event)
{
    struct target *t = iochan_getdata(i);
    struct session *s = t->session;
    //static int waiting = 0;

    if (t->state == No_connection) /* Start connection */
    {
	int res = cs_connect(t->link, t->addr);

	t->state = Connecting;
	if (!res) /* we are go */
	    iochan_setevent(i, EVENT_OUTPUT);
	else if (res == 1)
	    iochan_setflags(i, EVENT_OUTPUT);
	else
	{
	    yaz_log(YLOG_WARN|YLOG_ERRNO, "ERROR %s connect\n", t->hostport);
	    cs_close(t->link);
	    t->state = Failed;
	    iochan_destroy(i);
	}
    }

    else if (t->state == Connecting && event & EVENT_OUTPUT)
    {
	int errcode;
        socklen_t errlen = sizeof(errcode);

	if (getsockopt(cs_fileno(t->link), SOL_SOCKET, SO_ERROR, &errcode,
	    &errlen) < 0 || errcode != 0)
	{
	    cs_close(t->link);
	    iochan_destroy(i);
	    t->state = Failed;
	    return;
	}
	else
	{
            yaz_log(YLOG_DEBUG, "Connect OK");
	    t->state = Connected;
	}
    }

    else if (event & EVENT_INPUT)
    {
	int len = cs_get(t->link, &t->ibuf, &t->ibufsize);

	if (len < 0)
	{
	    cs_close(t->link);
	    iochan_destroy(i);
	    t->state = Failed;
	    return;
	}
	if (len == 0)
	{
	    cs_close(t->link);
	    iochan_destroy(i);
	    t->state = Failed;
	    return;
	}
	else if (len > 1)
	{
            if (t->requestid == s->requestid || t->state == Initializing) 
            {
                Z_APDU *a;

                odr_reset(t->odr_in);
                odr_setbuf(t->odr_in, t->ibuf, len, 0);
                if (!z_APDU(t->odr_in, &a, 0, 0))
                {
                    cs_close(t->link);
                    iochan_destroy(i);
                    t->state = Failed;
                    return;
                }
                yaz_log(YLOG_DEBUG, "Successfully decoded %d oct PDU", len);
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
                        cs_close(t->link);
                        iochan_destroy(i);
                        t->state = Failed;
                        return;
                }
                // if (cs_more(t->link))
                //    iochan_setevent(i, EVENT_INPUT);
            }
            else  // we throw away response and go to idle mode
                t->state = Idle;
	}
	/* if len==1 we do nothing but wait for more input */
    }

    else if (t->state == Connected) {
        send_init(i);
    }

    if (t->state == Idle)
    {
        if (t->requestid != s->requestid) {
            send_search(i);
        }
        else if (t->hits > 0 && t->records < global_parameters.toget &&
            t->records < t->hits) {
            send_present(i);
        }
    }
}

int load_targets(struct session *s, const char *fn)
{
    FILE *f = fopen(fn, "r");
    char line[256];
    struct target **target_p;

    if (!f)
    {
        yaz_log(YLOG_WARN|YLOG_ERRNO, "open %s", fn);
        return -1;
    }

    target_p = &s->targets;
    while (fgets(line, 255, f))
    {
        char *url, *p;
        struct target *target;
        IOCHAN new;

        if (strncmp(line, "target ", 7))
            continue;
        url = line + 7;
        url[strlen(url) - 1] = '\0';
        yaz_log(LOG_DEBUG, "Target: %s", url);

        *target_p = target = xmalloc(sizeof(**target_p));
        target->next = 0;
        target_p = &target->next;
        target->state = No_connection;
        target->ibuf = 0;
        target->ibufsize = 0;
        target->odr_in = odr_createmem(ODR_DECODE);
        target->odr_out = odr_createmem(ODR_ENCODE);
        target->hits = -1;
        target->setno = 0;
        target->session = s;
        target->requestid = -1;
        target->records = 0;
        target->diagnostic = 0;
        strcpy(target->fullname, url);
        if ((p = strchr(url, '/')))
        {		    
            *p = '\0';
            strcpy(target->hostport, url);
            *p = '/';
            p++;
            strcpy(target->databases[0], p);
            target->databases[1][0] = '\0';
        }
        else
        {
            strcpy(target->hostport, url);
            strcpy(target->databases[0], "Default");
            target->databases[1][0] = '\0';
        }

	if (!(target->link = cs_create(tcpip_type, 0, PROTO_Z3950)))
        {
	    yaz_log(YLOG_FATAL|YLOG_ERRNO, "Failed to create comstack");
            exit(1);
        }
	if (!(target->addr = cs_straddr(target->link, target->hostport)))
	{
	    printf("ERROR %s bad-address", target->hostport);
	    target->state = Failed;
	    continue;
	}
	new = iochan_create(cs_fileno(target->link), handler, 0);
	iochan_setdata(new, target);
	iochan_setevent(new, EVENT_EXCEPT);
	new->next = channel_list;
	channel_list = new;
    }
    fclose(f);

    return 0;
}

void search(struct session *s, char *query)
{
    IOCHAN c;
    int live_channels = 0;

    yaz_log(YLOG_DEBUG, "Search");

    // Determine what iochans belong to this session
    // It might have been better to have a list of them

    strcpy(s->query, query);
    s->requestid++;
    nmem_reset(s->nmem);
    for (c = channel_list; c; c = c->next)
    {
        struct target *t;

        if (iochan_getfun(c) != handler) // Not a Z target
            continue;
        t = iochan_getdata(c);
        if (t->session == s)
        {
            t->hits = -1;
            t->records = 0;
            t->diagnostic = 0;

            if (t->state == Error)
                t->state = Idle;

            if (t->state == Idle) 
                iochan_setflag(c, EVENT_OUTPUT);

            live_channels++;
        }
    }
    if (live_channels)
    {
        int maxrecs = live_channels * global_parameters.toget;
        if (!s->recheap_size)
        {
            s->recheap = xmalloc(maxrecs * sizeof(struct record *));
            s->recheap_size = maxrecs;
        }
        else if (s->recheap_size < maxrecs)
        {
            s->recheap = xrealloc(s->recheap, maxrecs * sizeof(struct record*));
            s->recheap_size = maxrecs;
        }
    }
    s->recheap_max = -1;
    s->recheap_scratch = -1;
}

struct session *new_session() 
{
    struct session *session = xmalloc(sizeof(*session));

    yaz_log(YLOG_DEBUG, "New pazpar2 session");
    
    session->requestid = -1;
    session->targets = 0;
    session->pqf_parser = yaz_pqf_create();
    session->query[0] = '\0';
    session->nmem = nmem_create();
    session->yaz_marc = yaz_marc_create();
    yaz_marc_subfield_str(session->yaz_marc, "\t");
    session->wrbuf = wrbuf_alloc();
    session->recheap = 0;
    session->recheap_size = 0;

    return session;
}

struct hitsbytarget *hitsbytarget(struct session *s, int *count)
{
    static struct hitsbytarget res[1000]; // FIXME MM
    IOCHAN c;

    *count = 0;
    for (c = channel_list; c; c = c->next)
        if (iochan_getfun(c) == handler)
        {
            struct target *t = iochan_getdata(c);
            if (t->session == s)
            {
                strcpy(res[*count].id, t->hostport);
                res[*count].hits = t->hits;
                res[*count].records = t->records;
                res[*count].diagnostic = t->diagnostic;
                res[*count].state = state_strings[(int) t->state];
                (*count)++;
            }
        }

    return res;
}

struct record **show(struct session *s, int start, int *num)
{
    struct record **recs = nmem_malloc(s->nmem, *num * sizeof(struct record *));
    int i;

    // FIXME -- skip initial records

    for (i = 0; i < *num; i++)
    {
        recs[i] = read_recheap(s);
        if (!recs[i])
        {
            *num = i;
            break;
        }
    }
    rewind_recheap(s);
    return recs;
}

void statistics(struct session *s, struct statistics *stat)
{
    IOCHAN c;
    int i;

    bzero(stat, sizeof(*stat));
    for (i = 0, c = channel_list; c; i++, c = c->next)
    {
        struct target *t;
        if (iochan_getfun(c) != handler)
            continue;
        t = iochan_getdata(c);
        switch (t->state)
        {
            case No_connection: stat->num_no_connection++; break;
            case Connecting: stat->num_connecting++; break;
            case Initializing: stat->num_initializing++; break;
            case Searching: stat->num_searching++; break;
            case Presenting: stat->num_presenting++; break;
            case Idle: stat->num_idle++; break;
            case Failed: stat->num_failed++; break;
            case Error: stat->num_error++; break;
            default: break;
        }
    }

    stat->num_connections = i;
}

int main(int argc, char **argv)
{
    int ret;
    char *arg;

    if (signal(SIGPIPE, SIG_IGN) < 0)
        yaz_log(YLOG_WARN|YLOG_ERRNO, "signal");

    yaz_log_init(YLOG_DEFAULT_LEVEL|YLOG_DEBUG, "pazpar2", 0);

    while ((ret = options("c:", argv, argc, &arg)) != -2)
    {
	switch (ret) {
	    case 0:
		break;
	    case 'c':
		command_init(atoi(arg));
		break;
	    default:
		fprintf(stderr, "Usage: pazpar2 -d comport");
		exit(1);
	}
	    
    }

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
