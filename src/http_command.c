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

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <sys/types.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <yaz/snprintf.h>
#include <yaz/yaz-util.h>

#include "util.h"
#include "eventl.h"
#include "pazpar2.h"
#include "http.h"
#include "http_command.h"
#include "settings.h"
#include "client.h"

// Update this when the protocol changes
#define PAZPAR2_PROTOCOL_VERSION "1"

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

struct http_session *http_session_create(struct conf_service *service)
{
    NMEM nmem = nmem_create();
    struct http_session *r = nmem_malloc(nmem, sizeof(*r));

    r->psession = new_session(nmem, service);
    r->session_id = 0;
    r->timestamp = 0;
    r->nmem = nmem;
    r->next = session_list;
    session_list = r;
    r->timeout_iochan = iochan_create(-1, session_timeout, 0);
    iochan_setdata(r->timeout_iochan, r);
    iochan_settimeout(r->timeout_iochan, global_parameters.session_timeout);

    pazpar2_add_channel(r->timeout_iochan);
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
    yaz_log(YLOG_LOG, "Destroying session %u", s->session_id);
    iochan_destroy(s->timeout_iochan);
    destroy_session(s->psession);
    nmem_destroy(s->nmem);
}

static const char *get_msg(enum pazpar2_error_code code)
{
    struct pazpar2_error_msg {
        enum pazpar2_error_code code;
        const char *msg;
    };
    static const struct pazpar2_error_msg ar[] = {
        { PAZPAR2_NO_SESSION, "Session does not exist or it has expired"},
        { PAZPAR2_MISSING_PARAMETER, "Missing parameter"},
        { PAZPAR2_MALFORMED_PARAMETER_VALUE, "Malformed parameter value"},
        { PAZPAR2_MALFORMED_PARAMETER_ENCODING, "Malformed parameter encoding"},
        { PAZPAR2_MALFORMED_SETTING, "Malformed setting argument"},
        { PAZPAR2_HITCOUNTS_FAILED, "Failed to retrieve hitcounts"},
        { PAZPAR2_RECORD_MISSING, "Record missing"},
        { PAZPAR2_NO_TARGETS, "No targets"},
        { PAZPAR2_CONFIG_TARGET, "Target cannot be configured"},
        { PAZPAR2_RECORD_FAIL, "Record command failed"},
        { PAZPAR2_NOT_IMPLEMENTED, "Not implemented"},
        { PAZPAR2_LAST_ERROR, "Last error"},
        { 0, 0 }
    };
    int i = 0;
    while (ar[i].msg)
    {
        if (code == ar[i].code)
            return ar[i].msg;
        i++;
    }
    return "No error";
}

static void error(struct http_response *rs, 
                  enum pazpar2_error_code code,
                  const char *addinfo)
{
    struct http_channel *c = rs->channel;
    WRBUF text = wrbuf_alloc();
    const char *http_status = "417";
    const char *msg = get_msg(code);
    
    rs->msg = nmem_strdup(c->nmem, msg);
    strcpy(rs->code, http_status);

    wrbuf_printf(text, "<error code=\"%d\" msg=\"%s\">", (int) code,
               msg);
    if (addinfo)
        wrbuf_xmlputs(text, addinfo);
    wrbuf_puts(text, "</error>");

    yaz_log(YLOG_WARN, "HTTP %s %s%s%s", http_status,
            msg, addinfo ? ": " : "" , addinfo ? addinfo : "");
    rs->payload = nmem_strdup(c->nmem, wrbuf_cstr(text));
    wrbuf_destroy(text);
    http_send_response(c);
}

unsigned int make_sessionid(void)
{
    static int seq = 0;
    unsigned int res;

    seq++;
    if (global_parameters.debug_mode)
        res = seq;
    else
    {
#ifdef WIN32
        res = seq;
#else
        struct timeval t;

        if (gettimeofday(&t, 0) < 0)
        {
            yaz_log(YLOG_WARN|YLOG_ERRNO, "gettimeofday");
            exit(1);
        }
        /* at most 256 sessions per second .. 
           (long long would be more appropriate)*/
        res = t.tv_sec;
        res = ((res << 8) | (seq & 0xff)) & ((1U << 31) - 1);
#endif
    }
    return res;
}

static struct http_session *locate_session(struct http_request *rq, struct http_response *rs)
{
    struct http_session *p;
    char *session = http_argbyname(rq, "session");
    unsigned int id;

    if (!session)
    {
        error(rs, PAZPAR2_MISSING_PARAMETER, "session");
        return 0;
    }
    id = atoi(session);
    for (p = session_list; p; p = p->next)
        if (id == p->session_id)
        {
            iochan_activity(p->timeout_iochan);
            return p;
        }
    error(rs, PAZPAR2_NO_SESSION, session);
    return 0;
}

// Decode settings parameters and apply to session
// Syntax: setting[target]=value
static int process_settings(struct session *se, struct http_request *rq,
        struct http_response *rs)
{
    struct http_argument *a;

    for (a = rq->arguments; a; a = a->next)
        if (strchr(a->name, '['))
        {
            char **res;
            int num;
            char *dbname;
            char *setting;

            // Nmem_strsplit *rules*!!!
            nmem_strsplit(se->session_nmem, "[]", a->name, &res, &num);
            if (num != 2)
            {
                error(rs, PAZPAR2_MALFORMED_SETTING, a->name);
                return -1;
            }
            setting = res[0];
            dbname = res[1];
            session_apply_setting(se, dbname, setting,
                    nmem_strdup(se->session_nmem, a->value));
        }
    return 0;
}

static void cmd_exit(struct http_channel *c)
{
    yaz_log(YLOG_WARN, "exit");
    http_close_server();
}

static void cmd_init(struct http_channel *c)
{
    unsigned int sesid;
    char buf[1024];
    const char *clear = http_argbyname(c->request, "clear");
    const char *service_name = http_argbyname(c->request, "service");
    struct conf_service *service = locate_service(service_name);
    struct http_session *s = http_session_create(service);
    struct http_response *rs = c->response;

    if (!service)
    {
        error(rs, PAZPAR2_MALFORMED_PARAMETER_VALUE, "service");
        return;
    }

    yaz_log(YLOG_DEBUG, "HTTP Session init");
    if (!clear || *clear == '0')
        session_init_databases(s->psession);
    else
        yaz_log(YLOG_LOG, "No databases preloaded");
    sesid = make_sessionid();
    s->session_id = sesid;
    if (process_settings(s->psession, c->request, c->response) < 0)
        return;
    sprintf(buf, "<init><status>OK</status><session>%u</session>"
            "<protocol>" PAZPAR2_PROTOCOL_VERSION "</protocol></init>", sesid);
    rs->payload = nmem_strdup(c->nmem, buf);
    http_send_response(c);
}

static void cmd_settings(struct http_channel *c)
{
    struct http_response *rs = c->response;
    struct http_request *rq = c->request;
    struct http_session *s = locate_session(rq, rs);

    if (!s)
        return;

    if (process_settings(s->psession, rq, rs) < 0)
        return;
    rs->payload = "<settings><status>OK</status></settings>";
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
static void targets_termlist(WRBUF wrbuf, struct session *se, int num,
                             NMEM nmem)
{
    struct hitsbytarget *ht;
    int count, i;

    ht = hitsbytarget(se, &count, nmem);
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
        
        wrbuf_printf(wrbuf, "<frequency>%d</frequency>\n", ht[i].hits);
        
        wrbuf_puts(wrbuf, "<state>");
        wrbuf_xmlputs(wrbuf, ht[i].state);
        wrbuf_puts(wrbuf, "</state>\n");
        
        wrbuf_printf(wrbuf, "<diagnostic>%d</diagnostic>\n", 
                     ht[i].diagnostic);
        wrbuf_puts(wrbuf, "</term>\n");
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

    wrbuf_puts(c->wrbuf, "<termlist>\n");
    wrbuf_printf(c->wrbuf, "<activeclients>%d</activeclients>\n", status);
    while (*name)
    {
        char tname[256];
        char *tp;

        if (!(tp = strchr(name, ',')))
            tp = name + strlen(name);
        strncpy(tname, name, tp - name);
        tname[tp - name] = '\0';

        wrbuf_puts(c->wrbuf, "<list name=\"");
        wrbuf_xmlputs(c->wrbuf, tname);
        wrbuf_puts(c->wrbuf, "\">\n");
        if (!strcmp(tname, "xtargets"))
            targets_termlist(c->wrbuf, s->psession, num, c->nmem);
        else
        {
            p = termlist(s->psession, tname, &len);
            if (p)
                for (i = 0; i < len && i < num; i++){
                    // prevnt sending empty term elements
                    if (!p[i]->term || !p[i]->term[0])
                        continue;

                    wrbuf_puts(c->wrbuf, "<term>");
                    wrbuf_puts(c->wrbuf, "<name>");
                    wrbuf_xmlputs(c->wrbuf, p[i]->term);
                    wrbuf_puts(c->wrbuf, "</name>");
                        
                    wrbuf_printf(c->wrbuf, 
                                 "<frequency>%d</frequency>", 
                                 p[i]->frequency);
                    wrbuf_puts(c->wrbuf, "</term>\n");
               }
        }
        wrbuf_puts(c->wrbuf, "</list>\n");
        name = tp;
        if (*name == ',')
            name++;
    }
    wrbuf_puts(c->wrbuf, "</termlist>\n");
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
    ht = hitsbytarget(s->psession, &count, c->nmem);
    wrbuf_rewind(c->wrbuf);
    wrbuf_puts(c->wrbuf, "<bytarget><status>OK</status>");

    for (i = 0; i < count; i++)
    {
        wrbuf_puts(c->wrbuf, "\n<target>");

        wrbuf_puts(c->wrbuf, "<id>");
        wrbuf_xmlputs(c->wrbuf, ht[i].id);
        wrbuf_puts(c->wrbuf, "</id>\n");

        if (ht[i].name && ht[i].name[0]) 
        {
            wrbuf_puts(c->wrbuf, "<name>");
            wrbuf_xmlputs(c->wrbuf, ht[i].name);
            wrbuf_puts(c->wrbuf, "</name>\n");
        }

        wrbuf_printf(c->wrbuf, "<hits>%d</hits>\n", ht[i].hits);
        wrbuf_printf(c->wrbuf, "<diagnostic>%d</diagnostic>\n", ht[i].diagnostic);
        wrbuf_printf(c->wrbuf, "<records>%d</records>\n", ht[i].records);

        wrbuf_puts(c->wrbuf, "<state>");
        wrbuf_xmlputs(c->wrbuf, ht[i].state);
        wrbuf_puts(c->wrbuf, "</state>\n");

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
            wrbuf_printf(w, "\n<md-%s>", cmd->name);

            switch (cmd->type)
            {
                case Metadata_type_generic:
                    wrbuf_xmlputs(w, md->data.text.disp);
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

static void write_subrecord(struct record *r, WRBUF w,
        struct conf_service *service, int show_details)
{
    const char *name = session_setting_oneval(
        client_get_database(r->client), PZ_NAME);

    wrbuf_puts(w, "<location id=\"");
    wrbuf_xmlputs(w, client_get_database(r->client)->database->url);
    wrbuf_puts(w, "\" ");

    wrbuf_puts(w, "name=\"");
    wrbuf_xmlputs(w,  *name ? name : "Unknown");
    wrbuf_puts(w, "\">");

    write_metadata(w, service, r->metadata, show_details);
    wrbuf_puts(w, "</location>\n");
}

static void show_raw_record_error(void *data, const char *addinfo)
{
    http_channel_observer_t obs = data;
    struct http_channel *c = http_channel_observer_chan(obs);
    struct http_response *rs = c->response;

    http_remove_observer(obs);

    error(rs, PAZPAR2_RECORD_FAIL, addinfo);
}

static void show_raw_record_ok(void *data, const char *buf, size_t sz)
{
    http_channel_observer_t obs = data;
    struct http_channel *c = http_channel_observer_chan(obs);
    struct http_response *rs = c->response;

    http_remove_observer(obs);

    wrbuf_write(c->wrbuf, buf, sz);
    rs->payload = nmem_strdup(c->nmem, wrbuf_cstr(c->wrbuf));
    http_send_response(c);
}


static void show_raw_record_ok_binary(void *data, const char *buf, size_t sz)
{
    http_channel_observer_t obs = data;
    struct http_channel *c = http_channel_observer_chan(obs);
    struct http_response *rs = c->response;

    http_remove_observer(obs);

    wrbuf_write(c->wrbuf, buf, sz);
    rs->payload = nmem_strdup(c->nmem, wrbuf_cstr(c->wrbuf));

    rs->content_type = "application/octet-stream";
    http_send_response(c);
}


void show_raw_reset(void *data, struct http_channel *c, void *data2)
{
    //struct client *client = data;
    //client_show_raw_remove(client, data2);
}

static void cmd_record_ready(void *data);

static void cmd_record(struct http_channel *c)
{
    struct http_response *rs = c->response;
    struct http_request *rq = c->request;
    struct http_session *s = locate_session(rq, rs);
    struct record_cluster *rec, *prev_r, *next_r;
    struct record *r;
    struct conf_service *service = s->psession->service;
    const char *idstr = http_argbyname(rq, "id");
    const char *offsetstr = http_argbyname(rq, "offset");
    const char *binarystr = http_argbyname(rq, "binary");
    
    if (!s)
        return;
    if (!idstr)
    {
        error(rs, PAZPAR2_MISSING_PARAMETER, "id");
        return;
    }
    wrbuf_rewind(c->wrbuf);
    if (!(rec = show_single(s->psession, idstr, &prev_r, &next_r)))
    {
        if (session_set_watch(s->psession, SESSION_WATCH_RECORD,
                              cmd_record_ready, c, c) != 0)
        {
            error(rs, PAZPAR2_RECORD_MISSING, idstr);
        }
        return;
    }
    if (offsetstr)
    {
        int offset = atoi(offsetstr);
        const char *syntax = http_argbyname(rq, "syntax");
        const char *esn = http_argbyname(rq, "esn");
        int i;
        struct record*r = rec->records;
        int binary = 0;

        if (binarystr && *binarystr != '0')
            binary = 1;

        for (i = 0; i < offset && r; r = r->next, i++)
            ;
        if (!r)
        {
            error(rs, PAZPAR2_RECORD_FAIL, "no record at offset given");
            return;
        }
        else
        {
            http_channel_observer_t obs =
                http_add_observer(c, r->client, show_raw_reset);
            int ret = client_show_raw_begin(r->client, r->position,
                                        syntax, esn, 
                                        obs /* data */,
                                        show_raw_record_error,
                                        (binary ? 
                                         show_raw_record_ok_binary : 
                                         show_raw_record_ok),
                                        (binary ? 1 : 0));
            if (ret == -1)
            {
                http_remove_observer(obs);
                error(rs, PAZPAR2_NO_SESSION, 0);
            }
        }
    }
    else
    {
        wrbuf_puts(c->wrbuf, "<record>\n");
        wrbuf_puts(c->wrbuf, "<recid>");
        wrbuf_xmlputs(c->wrbuf, rec->recid);
        wrbuf_puts(c->wrbuf, "</recid>\n");
        if (prev_r)
        {
            wrbuf_puts(c->wrbuf, "<prevrecid>");
            wrbuf_xmlputs(c->wrbuf, prev_r->recid);
            wrbuf_puts(c->wrbuf, "</prevrecid>\n");
        }
        if (next_r)
        {
            wrbuf_puts(c->wrbuf, "<nextrecid>");
            wrbuf_xmlputs(c->wrbuf, next_r->recid);
            wrbuf_puts(c->wrbuf, "</nextrecid>\n");
        }
        wrbuf_printf(c->wrbuf, "<activeclients>%d</activeclients>\n", 
                     session_active_clients(s->psession));
        write_metadata(c->wrbuf, service, rec->metadata, 1);
        for (r = rec->records; r; r = r->next)
            write_subrecord(r, c->wrbuf, service, 1);
        wrbuf_puts(c->wrbuf, "</record>\n");
        rs->payload = nmem_strdup(c->nmem, wrbuf_cstr(c->wrbuf));
        http_send_response(c);
    }
}

static void cmd_record_ready(void *data)
{
    struct http_channel *c = (struct http_channel *) data;

    cmd_record(c);
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
    if (!(sp = reclist_parse_sortparms(c->nmem, sort, s->psession->service)))
    {
        error(rs, PAZPAR2_MALFORMED_PARAMETER_VALUE, "sort");
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
        struct conf_service *service = s->psession->service;

        wrbuf_puts(c->wrbuf, "<hit>\n");
        write_metadata(c->wrbuf, service, rec->metadata, 0);
        for (ccount = 0, p = rl[i]->records; p;  p = p->next, ccount++)
            write_subrecord(p, c->wrbuf, service, 0); // subrecs w/o details
        if (ccount > 1)
            wrbuf_printf(c->wrbuf, "<count>%d</count>\n", ccount);
        wrbuf_puts(c->wrbuf, "<recid>");
        wrbuf_xmlputs(c->wrbuf, rec->recid);
        wrbuf_puts(c->wrbuf, "</recid>\n");
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
            // if there is already a watch/block. we do not block this one
            if (session_set_watch(s->psession, SESSION_WATCH_SHOW,
                                  show_records_ready, c, c) != 0)
            {
                yaz_log(YLOG_DEBUG, "Blocking on cmd_show");
            }
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

static int utf_8_valid(const char *str)
{
    yaz_iconv_t cd = yaz_iconv_open("utf-8", "utf-8");
    if (cd)
    {
        /* check that query is UTF-8 encoded */
        char *inbuf = (char *) str; /* we know iconv does not alter this */
        size_t inbytesleft = strlen(inbuf);

        size_t outbytesleft = strlen(inbuf) + 10;
        char *out = xmalloc(outbytesleft);
        char *outbuf = out;
        size_t r = yaz_iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);

        /* if OK, try flushing the rest  */
        if (r != (size_t) (-1))
            r = yaz_iconv(cd, 0, 0, &outbuf, &outbytesleft);
        yaz_iconv_close(cd);
        xfree(out);
        if (r == (size_t) (-1))
            return 0;
    }
    return 1;
}

static void cmd_search(struct http_channel *c)
{
    struct http_request *rq = c->request;
    struct http_response *rs = c->response;
    struct http_session *s = locate_session(rq, rs);
    char *query = http_argbyname(rq, "query");
    char *filter = http_argbyname(rq, "filter");
    enum pazpar2_error_code code;
    const char *addinfo = 0;

    if (!s)
        return;
    if (!query)
    {
        error(rs, PAZPAR2_MISSING_PARAMETER, "query");
        return;
    }
    if (!utf_8_valid(query))
    {
        error(rs, PAZPAR2_MALFORMED_PARAMETER_ENCODING, "query");
        return;
    }
    code = search(s->psession, query, filter, &addinfo);
    if (code)
    {
        error(rs, code, addinfo);
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

    float progress = 0;

    if (!s)
        return;

    clients = session_active_clients(s->psession);
    statistics(s->psession, &stat);

    if (stat.num_clients > 0) {
    	progress = (stat.num_clients  - clients) / (float)stat.num_clients;
    }

    wrbuf_rewind(c->wrbuf);
    wrbuf_puts(c->wrbuf, "<stat>");
    wrbuf_printf(c->wrbuf, "<activeclients>%d</activeclients>\n", clients);
    wrbuf_printf(c->wrbuf, "<hits>%d</hits>\n", stat.num_hits);
    wrbuf_printf(c->wrbuf, "<records>%d</records>\n", stat.num_records);
    wrbuf_printf(c->wrbuf, "<clients>%d</clients>\n", stat.num_clients);
    wrbuf_printf(c->wrbuf, "<unconnected>%d</unconnected>\n", stat.num_no_connection);
    wrbuf_printf(c->wrbuf, "<connecting>%d</connecting>\n", stat.num_connecting);
    wrbuf_printf(c->wrbuf, "<working>%d</working>\n", stat.num_working);
    wrbuf_printf(c->wrbuf, "<idle>%d</idle>\n", stat.num_idle);
    wrbuf_printf(c->wrbuf, "<failed>%d</failed>\n", stat.num_failed);
    wrbuf_printf(c->wrbuf, "<error>%d</error>\n", stat.num_error);
    wrbuf_printf(c->wrbuf, "<progress>%.2f</progress>\n", progress);
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
    wrbuf_puts(c->wrbuf, " <version>\n");
    wrbuf_puts(c->wrbuf, "<pazpar2");
#ifdef PAZPAR2_VERSION_SHA1
    wrbuf_printf(c->wrbuf, " sha1=\"%s\"", PAZPAR2_VERSION_SHA1);
#endif
    wrbuf_puts(c->wrbuf, ">");
    wrbuf_xmlputs(c->wrbuf, VERSION);
    wrbuf_puts(c->wrbuf, "</pazpar2>");


    yaz_version(yaz_version_str, 0);
    wrbuf_puts(c->wrbuf, "  <yaz compiled=\"");
    wrbuf_xmlputs(c->wrbuf, YAZ_VERSION);
    wrbuf_puts(c->wrbuf, "\">");
    wrbuf_xmlputs(c->wrbuf, yaz_version_str);
    wrbuf_puts(c->wrbuf, "</yaz>\n");

    wrbuf_puts(c->wrbuf, " </version>\n");
    
    wrbuf_puts(c->wrbuf, "</info>");
    rs->payload = nmem_strdup(c->nmem, wrbuf_cstr(c->wrbuf));
    http_send_response(c);
}

struct {
    char *name;
    void (*fun)(struct http_channel *c);
} commands[] = {
    { "init", cmd_init },
    { "settings", cmd_settings },
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
        error(rs, PAZPAR2_MISSING_PARAMETER, "command");
        return;
    }
    for (i = 0; commands[i].name; i++)
        if (!strcmp(commands[i].name, command))
        {
            (*commands[i].fun)(c);
            break;
        }
    if (!commands[i].name)
        error(rs, PAZPAR2_MALFORMED_PARAMETER_VALUE, "command");

    return;
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

