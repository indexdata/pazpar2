/* $Id: http_command.c,v 1.32 2007-04-10 08:48:56 adam Exp $
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

/*
 * $Id: http_command.c,v 1.32 2007-04-10 08:48:56 adam Exp $
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>
#include <sys/time.h>

#if HAVE_CONFIG_H
#include <cconfig.h>
#endif

#include <yaz/yaz-util.h>

#include "config.h"
#include "util.h"
#include "eventl.h"
#include "pazpar2.h"
#include "http.h"
#include "http_command.h"

extern struct parameters global_parameters;
extern IOCHAN channel_list;

struct http_session {
    IOCHAN timeout_iochan;     // NOTE: This is NOT associated with a socket
    struct session *psession;
    unsigned int session_id;
    int timestamp;
    NMEM nmem;
    struct http_session *next;
};

static struct http_session *session_list = 0;

void http_session_destroy(struct http_session *s);

static void session_timeout(IOCHAN i, int event)
{
    struct http_session *s = iochan_getdata(i);
    http_session_destroy(s);
}

struct http_session *http_session_create()
{
    NMEM nmem = nmem_create();
    struct http_session *r = nmem_malloc(nmem, sizeof(*r));

    r->psession = new_session(nmem);
    r->session_id = 0;
    r->timestamp = 0;
    r->nmem = nmem;
    r->next = session_list;
    session_list = r;
    r->timeout_iochan = iochan_create(-1, session_timeout, 0);
    iochan_setdata(r->timeout_iochan, r);
    iochan_settimeout(r->timeout_iochan, global_parameters.session_timeout);
    r->timeout_iochan->next = channel_list;
    channel_list = r->timeout_iochan;
    return r;
}

void http_session_destroy(struct http_session *s)
{
    struct http_session **p;

    for (p = &session_list; *p; p = &(*p)->next)
        if (*p == s)
        {
            *p = (*p)->next;
            break;
        }
    iochan_destroy(s->timeout_iochan);
    destroy_session(s->psession);
    nmem_destroy(s->nmem);
}

static void error(struct http_response *rs, char *code, char *msg, char *txt)
{
    struct http_channel *c = rs->channel;
    char tmp[1024];

    if (!txt)
        txt = msg;
    rs->msg = nmem_strdup(c->nmem, msg);
    strcpy(rs->code, code);
    sprintf(tmp, "<error code=\"general\">%s</error>", txt);
    rs->payload = nmem_strdup(c->nmem, tmp);
    http_send_response(c);
}

unsigned int make_sessionid()
{
    struct timeval t;
    unsigned int res;
    static int seq = 0;

    seq++;
    if (gettimeofday(&t, 0) < 0)
        abort();
    res = t.tv_sec;
    res = ((res << 8) | (seq & 0xff)) & ((1U << 31) - 1);
    return res;
}

static struct http_session *locate_session(struct http_request *rq, struct http_response *rs)
{
    struct http_session *p;
    char *session = http_argbyname(rq, "session");
    unsigned int id;

    if (!session)
    {
        error(rs, "417", "Must supply session", 0);
        return 0;
    }
    id = atoi(session);
    for (p = session_list; p; p = p->next)
        if (id == p->session_id)
        {
            iochan_activity(p->timeout_iochan);
            return p;
        }
    error(rs, "417", "Session does not exist, or it has expired", 0);
    return 0;
}

static void cmd_exit(struct http_channel *c)
{
    yaz_log(YLOG_WARN, "exit");
    exit(0);
}


static void cmd_init(struct http_channel *c)
{
    unsigned int sesid;
    char buf[1024];
    struct http_session *s = http_session_create();
    struct http_response *rs = c->response;

    yaz_log(YLOG_DEBUG, "HTTP Session init");
    sesid = make_sessionid();
    s->session_id = sesid;
    sprintf(buf, "<init><status>OK</status><session>%u</session></init>", sesid);
    rs->payload = nmem_strdup(c->nmem, buf);
    http_send_response(c);
}

// Compares two hitsbytarget nodes by hitcount
static int cmp_ht(const void *p1, const void *p2)
{
    const struct hitsbytarget *h1 = p1;
    const struct hitsbytarget *h2 = p2;
    return h2->hits - h1->hits;
}

// This implements functionality somewhat similar to 'bytarget', but in a termlist form
static void targets_termlist(WRBUF wrbuf, struct session *se, int num)
{
    struct hitsbytarget *ht;
    int count, i;

    if (!(ht = hitsbytarget(se, &count)))
        return;
    qsort(ht, count, sizeof(struct hitsbytarget), cmp_ht);
    for (i = 0; i < count && i < num && ht[i].hits > 0; i++)
    {
        wrbuf_puts(wrbuf, "\n<term>\n");
        wrbuf_printf(wrbuf, "<id>%s</id>\n", ht[i].id);
        wrbuf_printf(wrbuf, "<name>%s</name>\n", ht[i].name);
        wrbuf_printf(wrbuf, "<frequency>%d</frequency>\n", ht[i].hits);
        wrbuf_printf(wrbuf, "<state>%s</state>\n", ht[i].state);
        wrbuf_printf(wrbuf, "<diagnostic>%d</diagnostic>\n", ht[i].diagnostic);
        wrbuf_puts(wrbuf, "\n</term>\n");
    }
}

static void cmd_termlist(struct http_channel *c)
{
    struct http_response *rs = c->response;
    struct http_request *rq = c->request;
    struct http_session *s = locate_session(rq, rs);
    struct termlist_score **p;
    int len;
    int i;
    char *name = http_argbyname(rq, "name");
    char *nums = http_argbyname(rq, "num");
    int num = 15;
    int status;

    if (!s)
        return;

    status = session_active_clients(s->psession);

    if (!name)
        name = "subject";
    if (strlen(name) > 255)
        return;
    if (nums)
        num = atoi(nums);

    wrbuf_rewind(c->wrbuf);

    wrbuf_puts(c->wrbuf, "<termlist>");
    wrbuf_printf(c->wrbuf, "\n<activeclients>%d</activeclients>", status);
    while (*name)
    {
        char tname[256];
        char *tp;

        if (!(tp = strchr(name, ',')))
            tp = name + strlen(name);
        strncpy(tname, name, tp - name);
        tname[tp - name] = '\0';

        wrbuf_printf(c->wrbuf, "\n<list name=\"%s\">\n", tname);
        if (!strcmp(tname, "xtargets"))
            targets_termlist(c->wrbuf, s->psession, num);
        else
        {
            p = termlist(s->psession, tname, &len);
            if (p)
                for (i = 0; i < len && i < num; i++)
                {
                    wrbuf_puts(c->wrbuf, "\n<term>");
                    wrbuf_printf(c->wrbuf, "<name>%s</name>", p[i]->term);
                    wrbuf_printf(c->wrbuf, "<frequency>%d</frequency>", p[i]->frequency);
                    wrbuf_puts(c->wrbuf, "</term>");
                }
        }
        wrbuf_puts(c->wrbuf, "\n</list>");
        name = tp;
        if (*name == ',')
            name++;
    }
    wrbuf_puts(c->wrbuf, "</termlist>");
    rs->payload = nmem_strdup(rq->channel->nmem, wrbuf_cstr(c->wrbuf));
    http_send_response(c);
}


static void cmd_bytarget(struct http_channel *c)
{
    struct http_response *rs = c->response;
    struct http_request *rq = c->request;
    struct http_session *s = locate_session(rq, rs);
    struct hitsbytarget *ht;
    int count, i;

    if (!s)
        return;
    if (!(ht = hitsbytarget(s->psession, &count)))
    {
        error(rs, "500", "Failed to retrieve hitcounts", 0);
        return;
    }
    wrbuf_rewind(c->wrbuf);
    wrbuf_puts(c->wrbuf, "<bytarget><status>OK</status>");

    for (i = 0; i < count; i++)
    {
        wrbuf_puts(c->wrbuf, "\n<target>");
        wrbuf_printf(c->wrbuf, "<id>%s</id>\n", ht[i].id);
        wrbuf_printf(c->wrbuf, "<hits>%d</hits>\n", ht[i].hits);
        wrbuf_printf(c->wrbuf, "<diagnostic>%d</diagnostic>\n", ht[i].diagnostic);
        wrbuf_printf(c->wrbuf, "<records>%d</records>\n", ht[i].records);
        wrbuf_printf(c->wrbuf, "<state>%s</state>\n", ht[i].state);
        wrbuf_puts(c->wrbuf, "</target>");
    }

    wrbuf_puts(c->wrbuf, "</bytarget>");
    rs->payload = nmem_strdup(c->nmem, wrbuf_cstr(c->wrbuf));
    http_send_response(c);
}

static void write_metadata(WRBUF w, struct conf_service *service,
        struct record_metadata **ml, int full)
{
    int imeta;

    for (imeta = 0; imeta < service->num_metadata; imeta++)
    {
        struct conf_metadata *cmd = &service->metadata[imeta];
        struct record_metadata *md;
        if (!cmd->brief && !full)
            continue;
        for (md = ml[imeta]; md; md = md->next)
        {
            wrbuf_printf(w, "<md-%s>", cmd->name);
            switch (cmd->type)
            {
                case Metadata_type_generic:
                    wrbuf_puts(w, md->data.text);
                    break;
                case Metadata_type_year:
                    wrbuf_printf(w, "%d", md->data.number.min);
                    if (md->data.number.min != md->data.number.max)
                        wrbuf_printf(w, "-%d", md->data.number.max);
                    break;
                default:
                    wrbuf_puts(w, "[can't represent]");
            }
            wrbuf_printf(w, "</md-%s>", cmd->name);
        }
    }
}

static void write_subrecord(struct record *r, WRBUF w, struct conf_service *service)
{
    wrbuf_printf(w, "<location id=\"%s\" name=\"%s\">\n",
            r->client->database->url,
            r->client->database->name ? r->client->database->name : "");
    write_metadata(w, service, r->metadata, 1);
    wrbuf_puts(w, "</location>\n");
}

static void cmd_record(struct http_channel *c)
{
    struct http_response *rs = c->response;
    struct http_request *rq = c->request;
    struct http_session *s = locate_session(rq, rs);
    struct record_cluster *rec;
    struct record *r;
    struct conf_service *service = global_parameters.server->service;
    char *idstr = http_argbyname(rq, "id");
    int id;

    if (!s)
        return;
    if (!idstr)
    {
        error(rs, "417", "Must supply id", 0);
        return;
    }
    wrbuf_rewind(c->wrbuf);
    id = atoi(idstr);
    if (!(rec = show_single(s->psession, id)))
    {
        error(rs, "500", "Record missing", 0);
        return;
    }
    wrbuf_puts(c->wrbuf, "<record>\n");
    wrbuf_printf(c->wrbuf, "<recid>%d</recid>", rec->recid);
    write_metadata(c->wrbuf, service, rec->metadata, 1);
    for (r = rec->records; r; r = r->next)
        write_subrecord(r, c->wrbuf, service);
    wrbuf_puts(c->wrbuf, "</record>\n");
    rs->payload = nmem_strdup(c->nmem, wrbuf_cstr(c->wrbuf));
    http_send_response(c);
}

static void show_records(struct http_channel *c, int active)
{
    struct http_request *rq = c->request;
    struct http_response *rs = c->response;
    struct http_session *s = locate_session(rq, rs);
    struct record_cluster **rl;
    struct reclist_sortparms *sp;
    char *start = http_argbyname(rq, "start");
    char *num = http_argbyname(rq, "num");
    char *sort = http_argbyname(rq, "sort");
    int startn = 0;
    int numn = 20;
    int total;
    int total_hits;
    int i;

    if (!s)
        return;

    // We haven't counted clients yet if we're called on a block release
    if (active < 0)
        active = session_active_clients(s->psession);

    if (start)
        startn = atoi(start);
    if (num)
        numn = atoi(num);
    if (!sort)
        sort = "relevance";
    if (!(sp = reclist_parse_sortparms(c->nmem, sort)))
    {
        error(rs, "500", "Bad sort parameters", 0);
        return;
    }

    rl = show(s->psession, sp, startn, &numn, &total, &total_hits, c->nmem);

    wrbuf_rewind(c->wrbuf);
    wrbuf_puts(c->wrbuf, "<show>\n<status>OK</status>\n");
    wrbuf_printf(c->wrbuf, "<activeclients>%d</activeclients>\n", active);
    wrbuf_printf(c->wrbuf, "<merged>%d</merged>\n", total);
    wrbuf_printf(c->wrbuf, "<total>%d</total>\n", total_hits);
    wrbuf_printf(c->wrbuf, "<start>%d</start>\n", startn);
    wrbuf_printf(c->wrbuf, "<num>%d</num>\n", numn);

    for (i = 0; i < numn; i++)
    {
        int ccount;
        struct record *p;
        struct record_cluster *rec = rl[i];
        struct conf_service *service = global_parameters.server->service;

        wrbuf_puts(c->wrbuf, "<hit>\n");
        write_metadata(c->wrbuf, service, rec->metadata, 0);
        for (ccount = 0, p = rl[i]->records; p;  p = p->next, ccount++)
            ;
        if (ccount > 1)
            wrbuf_printf(c->wrbuf, "<count>%d</count>\n", ccount);
        wrbuf_printf(c->wrbuf, "<recid>%d</recid>\n", rec->recid);
        wrbuf_puts(c->wrbuf, "</hit>\n");
    }

    wrbuf_puts(c->wrbuf, "</show>\n");
    rs->payload = nmem_strdup(c->nmem, wrbuf_cstr(c->wrbuf));
    http_send_response(c);
}

static void show_records_ready(void *data)
{
    struct http_channel *c = (struct http_channel *) data;

    show_records(c, -1);
}

static void cmd_show(struct http_channel *c)
{
    struct http_request *rq = c->request;
    struct http_response *rs = c->response;
    struct http_session *s = locate_session(rq, rs);
    char *block = http_argbyname(rq, "block");
    int status;

    if (!s)
        return;

    status = session_active_clients(s->psession);

    if (block)
    {
        if (status && (!s->psession->reclist || !s->psession->reclist->num_records))
        {
            session_set_watch(s->psession, SESSION_WATCH_RECORDS, show_records_ready, c);
            yaz_log(YLOG_DEBUG, "Blocking on cmd_show");
            return;
        }
    }

    show_records(c, status);
}

static void cmd_ping(struct http_channel *c)
{
    struct http_request *rq = c->request;
    struct http_response *rs = c->response;
    struct http_session *s = locate_session(rq, rs);
    if (!s)
        return;
    rs->payload = "<ping><status>OK</status></ping>";
    http_send_response(c);
}

static void cmd_search(struct http_channel *c)
{
    struct http_request *rq = c->request;
    struct http_response *rs = c->response;
    struct http_session *s = locate_session(rq, rs);
    char *query = http_argbyname(rq, "query");
    char *filter = http_argbyname(rq, "filter");
    char *res;

    if (!s)
        return;
    if (!query)
    {
        error(rs, "417", "Must supply query", 0);
        return;
    }
    res = search(s->psession, query, filter);
    if (res)
    {
        error(rs, "417", res, res);
        return;
    }
    rs->payload = "<search><status>OK</status></search>";
    http_send_response(c);
}


static void cmd_stat(struct http_channel *c)
{
    struct http_request *rq = c->request;
    struct http_response *rs = c->response;
    struct http_session *s = locate_session(rq, rs);
    struct statistics stat;
    int clients;

    if (!s)
        return;

    clients = session_active_clients(s->psession);
    statistics(s->psession, &stat);

    wrbuf_rewind(c->wrbuf);
    wrbuf_puts(c->wrbuf, "<stat>");
    wrbuf_printf(c->wrbuf, "<activeclients>%d</activeclients>\n", clients);
    wrbuf_printf(c->wrbuf, "<hits>%d</hits>\n", stat.num_hits);
    wrbuf_printf(c->wrbuf, "<records>%d</records>\n", stat.num_records);
    wrbuf_printf(c->wrbuf, "<clients>%d</clients>\n", stat.num_clients);
    wrbuf_printf(c->wrbuf, "<unconnected>%d</unconnected>\n", stat.num_no_connection);
    wrbuf_printf(c->wrbuf, "<connecting>%d</connecting>\n", stat.num_connecting);
    wrbuf_printf(c->wrbuf, "<initializing>%d</initializing>\n", stat.num_initializing);
    wrbuf_printf(c->wrbuf, "<searching>%d</searching>\n", stat.num_searching);
    wrbuf_printf(c->wrbuf, "<presenting>%d</presenting>\n", stat.num_presenting);
    wrbuf_printf(c->wrbuf, "<idle>%d</idle>\n", stat.num_idle);
    wrbuf_printf(c->wrbuf, "<failed>%d</failed>\n", stat.num_failed);
    wrbuf_printf(c->wrbuf, "<error>%d</error>\n", stat.num_error);
    wrbuf_puts(c->wrbuf, "</stat>");
    rs->payload = nmem_strdup(c->nmem, wrbuf_cstr(c->wrbuf));
    http_send_response(c);
}

static void cmd_info(struct http_channel *c)
{
    char yaz_version_str[20];
    struct http_response *rs = c->response;

    wrbuf_rewind(c->wrbuf);
    wrbuf_puts(c->wrbuf, "<info>\n");
    wrbuf_printf(c->wrbuf, " <version>\n");
    wrbuf_printf(c->wrbuf, "  <pazpar2>%s</pazpar2>\n", VERSION);

    yaz_version(yaz_version_str, 0);
    wrbuf_printf(c->wrbuf, "  <yaz compiled=\"%s\">%s</yaz>\n",
                 YAZ_VERSION, yaz_version_str);
    wrbuf_printf(c->wrbuf, " </version>\n");
    
    wrbuf_puts(c->wrbuf, "</info>");
    rs->payload = nmem_strdup(c->nmem, wrbuf_cstr(c->wrbuf));
    http_send_response(c);
}

struct {
    char *name;
    void (*fun)(struct http_channel *c);
} commands[] = {
    { "init", cmd_init },
    { "stat", cmd_stat },
    { "bytarget", cmd_bytarget },
    { "show", cmd_show },
    { "search", cmd_search },
    { "termlist", cmd_termlist },
    { "exit", cmd_exit },
    { "ping", cmd_ping },
    { "record", cmd_record },
    { "info", cmd_info },
    {0,0}
};

void http_command(struct http_channel *c)
{
    char *command = http_argbyname(c->request, "command");
    struct http_response *rs = http_create_response(c);
    int i;

    c->response = rs;

    http_addheader(rs, "Expires", "Thu, 19 Nov 1981 08:52:00 GMT");
    http_addheader(rs, "Cache-Control", "no-store, no-cache, must-revalidate, post-check=0, pre-check=0");

    if (!command)
    {
        error(rs, "417", "Must supply command", 0);
        return;
    }
    for (i = 0; commands[i].name; i++)
        if (!strcmp(commands[i].name, command))
        {
            (*commands[i].fun)(c);
            break;
        }
    if (!commands[i].name)
        error(rs, "417", "Unknown command", 0);

    return;
}

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
