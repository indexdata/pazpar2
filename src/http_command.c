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

#include "ppmutex.h"
#include "eventl.h"
#include "parameters.h"
#include "session.h"
#include "http.h"
#include "settings.h"
#include "client.h"

#ifdef HAVE_MALLINFO
#include <malloc.h>

void print_meminfo(WRBUF wrbuf)
{
    struct mallinfo minfo;
    minfo = mallinfo();
    wrbuf_printf(wrbuf, "  <memory>\n"
                        "   <arena>%d</arena>\n" 
                        "   <uordblks>%d</uordblks>\n"
                        "   <fordblks>%d</fordblks>\n"
                        "   <ordblks>%d</ordblks>\n"
                        "   <keepcost>%d</keepcost>\n"
                        "   <hblks>%d</hblks>\n" 
                        "   <hblkhd>%d</hblkhd>\n"
                        "   <virt>%d</virt>\n"
                        "   <virtuse>%d</virtuse>\n"
                        "  </memory>\n", 
                 minfo.arena, minfo.uordblks, minfo.fordblks,minfo.ordblks, minfo.keepcost, minfo.hblks, minfo.hblkhd, minfo.arena + minfo.hblkhd, minfo.uordblks + minfo.hblkhd);

}
#else
#define print_meminfo(x)
#endif


// Update this when the protocol changes
#define PAZPAR2_PROTOCOL_VERSION "1"

#define HTTP_COMMAND_RESPONSE_PREFIX "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"

struct http_session {
    IOCHAN timeout_iochan;     // NOTE: This is NOT associated with a socket
    struct session *psession;
    unsigned int session_id;
    int timestamp;
    int destroy_counter;
    int activity_counter;
    NMEM nmem;
    http_sessions_t http_sessions;
    struct http_session *next;
};

struct http_sessions {
    struct http_session *session_list;
    YAZ_MUTEX mutex;
    int log_level;
};

static YAZ_MUTEX g_http_session_mutex = 0;
static int g_http_sessions = 0;

int http_session_use(int delta)
{
    int sessions;
    if (!g_http_session_mutex)
        yaz_mutex_create(&g_http_session_mutex);
    yaz_mutex_enter(g_http_session_mutex);
    g_http_sessions += delta;
    sessions = g_http_sessions;
    yaz_mutex_leave(g_http_session_mutex);
    yaz_log(YLOG_DEBUG, "%s sessions=%d", delta == 0 ? "" : (delta > 0 ? "INC" : "DEC"), sessions);
    return sessions;

}

http_sessions_t http_sessions_create(void)
{
    http_sessions_t hs = xmalloc(sizeof(*hs));
    hs->session_list = 0;
    hs->mutex = 0;
    pazpar2_mutex_create(&hs->mutex, "http_sessions");
    hs->log_level = yaz_log_module_level("HTTP");
    return hs;
}

void http_sessions_destroy(http_sessions_t hs)
{
    if (hs)
    {
        struct http_session *s = hs->session_list;
        while (s)
        {
            struct http_session *s_next = s->next;
            iochan_destroy(s->timeout_iochan);
            destroy_session(s->psession);
            nmem_destroy(s->nmem);
            s = s_next;
        }
        yaz_mutex_destroy(&hs->mutex);
        xfree(hs);
    }
}

void http_session_destroy(struct http_session *s);

static void session_timeout(IOCHAN i, int event)
{
    struct http_session *s = iochan_getdata(i);
    http_session_destroy(s);
}

struct http_session *http_session_create(struct conf_service *service,
                                         http_sessions_t http_sessions,
                                         unsigned int sesid)
{
    NMEM nmem = nmem_create();
    struct http_session *r = nmem_malloc(nmem, sizeof(*r));
    char tmp_str[50];

    sprintf(tmp_str, "session#%u", sesid);
    r->psession = new_session(nmem, service, sesid);
    r->session_id = sesid;
    r->timestamp = 0;
    r->nmem = nmem;
    r->destroy_counter = r->activity_counter = 0;
    r->http_sessions = http_sessions;

    yaz_mutex_enter(http_sessions->mutex);
    r->next = http_sessions->session_list;
    http_sessions->session_list = r;
    yaz_mutex_leave(http_sessions->mutex);

    r->timeout_iochan = iochan_create(-1, session_timeout, 0, "http_session_timeout");
    iochan_setdata(r->timeout_iochan, r);
    yaz_log(http_sessions->log_level, "%p Session %u created. timeout chan=%p timeout=%d", r, sesid, r->timeout_iochan, service->session_timeout);
    iochan_settimeout(r->timeout_iochan, service->session_timeout);

    iochan_add(service->server->iochan_man, r->timeout_iochan);
    http_session_use(1);
    return r;
}

void http_session_destroy(struct http_session *s)
{
    int must_destroy = 0;

    http_sessions_t http_sessions = s->http_sessions;

    yaz_log(http_sessions->log_level, "%p HTTP Session %u destroyed", s, s->session_id);
    yaz_mutex_enter(http_sessions->mutex);
    /* only if http_session has no active http sessions on it can be destroyed */
    if (s->destroy_counter == s->activity_counter)
    {
        struct http_session **p = 0;
        must_destroy = 1;
        for (p = &http_sessions->session_list; *p; p = &(*p)->next)
            if (*p == s)
            {
                *p = (*p)->next;
                break;
            }
    }
    yaz_mutex_leave(http_sessions->mutex);
    if (must_destroy)
    {   /* destroying for real */
        yaz_log(http_sessions->log_level, "%p HTTP Session %u destroyed", s, s->session_id);
        iochan_destroy(s->timeout_iochan);
        destroy_session(s->psession);
        http_session_use(-1);
        nmem_destroy(s->nmem);
    }
    else {
        yaz_log(http_sessions->log_level, "%p HTTP Session %u destroyed delayed. Active clients (%d-%d). Waiting for new timeout.",
                s, s->session_id, s->activity_counter, s->destroy_counter);
    }

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
        { PAZPAR2_NO_SERVICE, "No service"},
        { PAZPAR2_ALREADY_BLOCKED, "Already blocked in session on: "},
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

    wrbuf_printf(text, HTTP_COMMAND_RESPONSE_PREFIX "<error code=\"%d\" msg=\"%s\">", (int) code,
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

static void response_open_no_status(struct http_channel *c, const char *command)
{
    wrbuf_rewind(c->wrbuf);
    wrbuf_printf(c->wrbuf, "%s<%s>",
                 HTTP_COMMAND_RESPONSE_PREFIX, command);
}

static void response_open(struct http_channel *c, const char *command)
{
    response_open_no_status(c, command);
    wrbuf_puts(c->wrbuf, "<status>OK</status>");
}

static void response_close(struct http_channel *c, const char *command)
{
    struct http_response *rs = c->response;

    wrbuf_printf(c->wrbuf, "</%s>", command);
    rs->payload = nmem_strdup(c->nmem, wrbuf_cstr(c->wrbuf));
    http_send_response(c);
}

unsigned int make_sessionid(void)
{
    static int seq = 0; /* thread pr */
    unsigned int res;

    seq++;
    if (global_parameters.predictable_sessions)
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

static struct http_session *locate_session(struct http_channel *c)
{
    struct http_request *rq = c->request;
    struct http_response *rs = c->response;
    struct http_session *p;
    const char *session = http_argbyname(rq, "session");
    http_sessions_t http_sessions = c->http_sessions;
    unsigned int id;

    if (!session)
    {
        error(rs, PAZPAR2_MISSING_PARAMETER, "session");
        return 0;
    }
    id = atoi(session);
    yaz_mutex_enter(http_sessions->mutex);
    for (p = http_sessions->session_list; p; p = p->next)
        if (id == p->session_id)
            break;
    if (p)
        p->activity_counter++;
    yaz_mutex_leave(http_sessions->mutex);
    if (p)
        iochan_activity(p->timeout_iochan);
    else
        error(rs, PAZPAR2_NO_SESSION, session);
    return p;
}

// Call after use of locate_session, in order to increment the destroy_counter
static void release_session(struct http_channel *c,
                            struct http_session *session)
{
    http_sessions_t http_sessions = c->http_sessions;
    yaz_mutex_enter(http_sessions->mutex);
    if (session)
        session->destroy_counter++;
    yaz_mutex_leave(http_sessions->mutex);
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

    response_open(c, "exit");
    response_close(c, "exit");
    http_close_server(c->server);
}

static void cmd_init(struct http_channel *c)
{
    struct http_request *r = c->request;
    const char *clear = http_argbyname(r, "clear");
    const char *content_type = http_lookup_header(r->headers, "Content-Type");
    unsigned int sesid;
    struct http_session *s;
    struct http_response *rs = c->response;
    struct conf_service *service = 0; /* no service (yet) */
    
    if (r->content_len && content_type && 
        !yaz_strcmp_del("text/xml", content_type, "; "))
    {
        xmlDoc *doc = xmlParseMemory(r->content_buf, r->content_len);
        xmlNode *root_n;
        if (!doc)
        {
            error(rs, PAZPAR2_MALFORMED_SETTING, 0);
            return;
        }
        root_n = xmlDocGetRootElement(doc);
        service = service_create(c->server, root_n);
        xmlFreeDoc(doc);
        if (!service)
        {
            error(rs, PAZPAR2_MALFORMED_SETTING, 0);
            return;
        }
    }
    
    if (!service)
    {
        const char *service_name = http_argbyname(c->request, "service");
        service = locate_service(c->server, service_name);
        if (!service)
        {
            error(rs, PAZPAR2_NO_SERVICE, service_name ? service_name : "unnamed");
            return;
        }
    }
    sesid = make_sessionid();
    s = http_session_create(service, c->http_sessions, sesid);
    
    yaz_log(c->http_sessions->log_level, "%p Session init %u ", s, sesid);
    if (!clear || *clear == '0')
        session_init_databases(s->psession);
    else
        yaz_log(YLOG_LOG, "HTTP Session %u init: No databases preloaded", sesid);
    
    if (process_settings(s->psession, c->request, c->response) < 0)
        return;

    response_open(c, "init");
    wrbuf_printf(c->wrbuf, "<session>%d", sesid);
    if (c->server->server_id)
    {
        wrbuf_puts(c->wrbuf, ".");
        wrbuf_puts(c->wrbuf, c->server->server_id);
    }
    wrbuf_puts(c->wrbuf, "</session>"
               "<protocol>" PAZPAR2_PROTOCOL_VERSION "</protocol>");
    
    response_close(c, "init");
}

static void apply_local_setting(void *client_data,
                                struct setting *set)
{
    struct session *se =  (struct session *) client_data;

    session_apply_setting(se, nmem_strdup(se->session_nmem, set->target),
                          nmem_strdup(se->session_nmem, set->name),
                          nmem_strdup(se->session_nmem, set->value));
}

static void cmd_settings(struct http_channel *c)
{
    struct http_response *rs = c->response;
    struct http_request *rq = c->request;
    struct http_session *s = locate_session(c);
    const char *content_type = http_lookup_header(rq->headers, "Content-Type");

    if (!s)
        return;

    if (rq->content_len && content_type && 
        !yaz_strcmp_del("text/xml", content_type, "; "))
    {
        xmlDoc *doc = xmlParseMemory(rq->content_buf, rq->content_len);
        xmlNode *root_n;
        if (!doc)
        {
            error(rs, PAZPAR2_MALFORMED_SETTING, 0);
            return;
        }
        root_n = xmlDocGetRootElement(doc);

        settings_read_node_x(root_n, s->psession, apply_local_setting);

        xmlFreeDoc(doc);
    }
    if (process_settings(s->psession, rq, rs) < 0)
    {
        release_session(c, s);
        return;
    }
    response_open(c, "settings");
    response_close(c, "settings");
    release_session(c, s);
}

static void cmd_termlist(struct http_channel *c)
{
    struct http_request *rq = c->request;
    struct http_session *s = locate_session(c);
    const char *name = http_argbyname(rq, "name");
    const char *nums = http_argbyname(rq, "num");
    int num = 15;
    int status;

    if (!s)
        return;

    status = session_active_clients(s->psession);

    if (nums)
        num = atoi(nums);

    response_open_no_status(c, "termlist");
    wrbuf_printf(c->wrbuf, "<activeclients>%d</activeclients>\n", status);

    perform_termlist(c, s->psession, name, num);

    response_close(c, "termlist");
    release_session(c, s);
}

size_t session_get_memory_status(struct session *session);

static void session_status(struct http_channel *c, struct http_session *s)
{
    size_t session_nmem;
    wrbuf_printf(c->wrbuf, "<http_count>%u</http_count>\n", s->activity_counter);
    wrbuf_printf(c->wrbuf, "<http_nmem>%zu</http_nmem>\n", nmem_total(s->nmem) );
    session_nmem = session_get_memory_status(s->psession);
    wrbuf_printf(c->wrbuf, "<session_nmem>%zu</session_nmem>\n", session_nmem);
}

static void cmd_session_status(struct http_channel *c)
{
    struct http_session *s = locate_session(c);
    if (!s)
        return;

    response_open(c, "session-status");
    session_status(c, s);
    response_close(c, "session-status");
    release_session(c, s);
}

int sessions_count(void);
int clients_count(void);
#ifdef HAVE_RESULTSETS_COUNT
int resultsets_count(void);
#else
#define resultsets_count()      0
#endif

static void cmd_server_status(struct http_channel *c)
{
    int sessions   = sessions_count();
    int clients    = clients_count();
    int resultsets = resultsets_count();

    response_open(c, "server-status");
    wrbuf_printf(c->wrbuf, "\n  <sessions>%u</sessions>\n", sessions);
    wrbuf_printf(c->wrbuf, "  <clients>%u</clients>\n",   clients);
    /* Only works if yaz has been compiled with enabling of this */
    wrbuf_printf(c->wrbuf, "  <resultsets>%u</resultsets>\n",resultsets);
    print_meminfo(c->wrbuf);

/* TODO add all sessions status                         */
/*    http_sessions_t http_sessions = c->http_sessions; */
/*    struct http_session *p;                           */
/*
    yaz_mutex_enter(http_sessions->mutex);
    for (p = http_sessions->session_list; p; p = p->next)
    {
        p->activity_counter++;
        wrbuf_puts(c->wrbuf, "<session-status>\n");
        wrbuf_printf(c->wrbuf, "<id>%s</id>\n", p->session_id);
        yaz_mutex_leave(http_sessions->mutex);
        session_status(c, p);
        wrbuf_puts(c->wrbuf, "</session-status>\n");
        yaz_mutex_enter(http_sessions->mutex);
        p->activity_counter--;
    }
    yaz_mutex_leave(http_sessions->mutex);
*/
    response_close(c, "server-status");
    xmalloc_trav(0);
}

static void bytarget_response(struct http_channel *c) {
    int count, i;
    struct hitsbytarget *ht;
    struct http_request *rq = c->request;
    const char *settings = http_argbyname(rq, "settings");
    struct http_session *s = locate_session(c);

    ht = get_hitsbytarget(s->psession, &count, c->nmem);
    response_open(c, "bytarget");
    if (count == 0)
        yaz_log(YLOG_WARN, "Empty bytarget Response. No targets found!");
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

        wrbuf_printf(c->wrbuf, "<hits>" ODR_INT_PRINTF "</hits>\n", ht[i].hits);
        wrbuf_printf(c->wrbuf, "<diagnostic>%d</diagnostic>\n", ht[i].diagnostic);
        wrbuf_printf(c->wrbuf, "<records>%d</records>\n", ht[i].records);

        wrbuf_puts(c->wrbuf, "<state>");
        wrbuf_xmlputs(c->wrbuf, ht[i].state);
        wrbuf_puts(c->wrbuf, "</state>\n");
        if (settings && *settings == '1')
        {
            wrbuf_puts(c->wrbuf, "<settings>\n");
            wrbuf_puts(c->wrbuf, ht[i].settings_xml);
            wrbuf_puts(c->wrbuf, "</settings>\n");
        }
        if (ht[i].suggestions_xml && ht[i].suggestions_xml[0]) {
            wrbuf_puts(c->wrbuf, "<suggestions>");
            wrbuf_puts(c->wrbuf, ht[i].suggestions_xml);
            wrbuf_puts(c->wrbuf, "</suggestions>");
        }
        wrbuf_puts(c->wrbuf, "</target>");
    }
    response_close(c, "bytarget");
    release_session(c, s);
}

static void bytarget_result_ready(void *data)
{
    struct http_channel *c = (struct http_channel *) data;
    yaz_log(c->http_sessions->log_level, "bytarget watch released");
    bytarget_response(c);
}


static void cmd_bytarget(struct http_channel *c)
{
    struct http_request *rq = c->request;
    struct http_response *rs = c->response;
    struct http_session *s = locate_session(c);
    const char *block = http_argbyname(rq, "block");

    if (!s)
        return;

    if (block && !strcmp("1",block))
    {
        // if there is already a watch/block. we do not block this one
        if (session_set_watch(s->psession, SESSION_WATCH_BYTARGET,
                              bytarget_result_ready, c, c) != 0)
        {
            yaz_log(YLOG_WARN, "Attempt to block multiple times on bytarget block. Not supported!");
            error(rs, PAZPAR2_ALREADY_BLOCKED, "bytarget"); 
        }
        else
        {
            yaz_log(c->http_sessions->log_level, "%p Session %u: Blocking on command bytarget", s, s->session_id);
        }
        release_session(c, s);
        return;
    }
    bytarget_response(c);
    release_session(c, s);
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
            struct record_metadata_attr *attr = md->attributes;
            wrbuf_printf(w, "\n<md-%s", cmd->name);

            for (; attr; attr = attr->next)
            {
                wrbuf_printf(w, " %s=\"", attr->name);
                wrbuf_xmlputs(w, attr->value);
                wrbuf_puts(w, "\"");
            }
            wrbuf_puts(w, ">");
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
    wrbuf_xmlputs(w, client_get_id(r->client));
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
    struct http_session *s = locate_session(c);
    struct record_cluster *rec, *prev_r, *next_r;
    struct record *r;
    struct conf_service *service;
    const char *idstr = http_argbyname(rq, "id");
    const char *offsetstr = http_argbyname(rq, "offset");
    const char *binarystr = http_argbyname(rq, "binary");
    
    if (!s)
        return;
    service = s->psession->service;
    if (!idstr)
    {
        error(rs, PAZPAR2_MISSING_PARAMETER, "id");
        return;
    }
    wrbuf_rewind(c->wrbuf);
    if (!(rec = show_single_start(s->psession, idstr, &prev_r, &next_r)))
    {
        if (session_active_clients(s->psession) == 0)
        {
            error(rs, PAZPAR2_RECORD_MISSING, idstr);
        }
        else if (session_set_watch(s->psession, SESSION_WATCH_RECORD,
                              cmd_record_ready, c, c) != 0)
        {
            error(rs, PAZPAR2_RECORD_MISSING, idstr);
        }
        release_session(c, s);
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
        response_open_no_status(c, "record");
        wrbuf_puts(c->wrbuf, "\n<recid>");
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
        response_close(c, "record");
    }
    show_single_stop(s->psession, rec);
    release_session(c, s);
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
    struct http_session *s = locate_session(c);
    struct record_cluster **rl;
    struct reclist_sortparms *sp;
    const char *start = http_argbyname(rq, "start");
    const char *num = http_argbyname(rq, "num");
    const char *sort = http_argbyname(rq, "sort");
    int startn = 0;
    int numn = 20;
    int total;
    Odr_int total_hits;
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
        release_session(c, s);
        return;

    }

    
    rl = show_range_start(s->psession, sp, startn, &numn, &total, &total_hits);

    response_open(c, "show");
    wrbuf_printf(c->wrbuf, "\n<activeclients>%d</activeclients>\n", active);
    wrbuf_printf(c->wrbuf, "<merged>%d</merged>\n", total);
    wrbuf_printf(c->wrbuf, "<total>" ODR_INT_PRINTF "</total>\n", total_hits);
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
	if (strstr(sort, "relevance"))
	    wrbuf_printf(c->wrbuf, "<relevance>%d</relevance>\n",
                         rec->relevance_score);
        wrbuf_puts(c->wrbuf, "<recid>");
        wrbuf_xmlputs(c->wrbuf, rec->recid);
        wrbuf_puts(c->wrbuf, "</recid>\n");
        wrbuf_puts(c->wrbuf, "</hit>\n");
    }

    show_range_stop(s->psession, rl);

    response_close(c, "show");
    release_session(c, s);
}

static void show_records_ready(void *data)
{
    struct http_channel *c = (struct http_channel *) data;

    show_records(c, -1);
}

static void cmd_show(struct http_channel *c)
{
    struct http_request  *rq = c->request;
    struct http_response *rs = c->response;
    struct http_session *s = locate_session(c);
    const char *block = http_argbyname(rq, "block");
    const char *sort = http_argbyname(rq, "sort");
    struct reclist_sortparms *sp;
    int status;

    if (!s)
        return;

    if (!sort)
        sort = "relevance";
    
    if (!(sp = reclist_parse_sortparms(c->nmem, sort, s->psession->service)))
    {
        error(c->response, PAZPAR2_MALFORMED_PARAMETER_VALUE, "sort");
        release_session(c, s);
        return;
    }
    session_sort(s->psession, sp->name, sp->increasing);

    status = session_active_clients(s->psession);

    if (block)
    {
        if (!strcmp(block, "preferred") && !session_is_preferred_clients_ready(s->psession) && reclist_get_num_records(s->psession->reclist) == 0)
        {
            // if there is already a watch/block. we do not block this one
            if (session_set_watch(s->psession, SESSION_WATCH_SHOW_PREF,
                                  show_records_ready, c, c) == 0)
            {
                yaz_log(c->http_sessions->log_level,
                        "%p Session %u: Blocking on command show (preferred targets)", s, s->session_id);
            }
            else
            {
                yaz_log(YLOG_WARN, "Attempt to block multiple times on show (preferred targets) block. Not supported!");
                error(rs, PAZPAR2_ALREADY_BLOCKED, "show (preferred targets)"); 
            }
            release_session(c, s);
            return;

        }
        else if (status)
        {
            // if there is already a watch/block. we do not block this one
            if (session_set_watch(s->psession, SESSION_WATCH_SHOW,
                                  show_records_ready, c, c) != 0)
            {
                yaz_log(YLOG_WARN, "Attempt to block multiple times on show block. Not supported!");
                error(rs, PAZPAR2_ALREADY_BLOCKED, "show"); 
            }
            else
            {
                yaz_log(c->http_sessions->log_level, "%p Session %u: Blocking on command show", s, s->session_id);
            }
            release_session(c, s);
            return;
        }
    }
    show_records(c, status);
    release_session(c, s);
}

static void cmd_ping(struct http_channel *c)
{
    struct http_session *s = locate_session(c);
    if (!s)
        return;
    response_open(c, "ping");
    response_close(c, "ping");
    release_session(c, s);
}

static void cmd_search(struct http_channel *c)
{
    struct http_request *rq = c->request;
    struct http_response *rs = c->response;
    struct http_session *s = locate_session(c);
    const char *query = http_argbyname(rq, "query");
    const char *filter = http_argbyname(rq, "filter");
    const char *maxrecs = http_argbyname(rq, "maxrecs");
    const char *startrecs = http_argbyname(rq, "startrecs");
    const char *limit = http_argbyname(rq, "limit");
    enum pazpar2_error_code code;
    const char *addinfo = 0;

    if (!s)
        return;
    if (!query)
    {
        error(rs, PAZPAR2_MISSING_PARAMETER, "query");
        release_session(c, s);
        return;
    }
    if (!yaz_utf8_check(query))
    {
        error(rs, PAZPAR2_MALFORMED_PARAMETER_ENCODING, "query");
        release_session(c, s);
        return;
    }
    code = session_search(s->psession, query, startrecs, maxrecs, filter, limit,
                          &addinfo, "relevance", 0);
    if (code)
    {
        error(rs, code, addinfo);
        release_session(c, s);
        return;
    }
    response_open(c, "search");
    response_close(c, "search");
    release_session(c, s);
}


static void cmd_stat(struct http_channel *c)
{
    struct http_session *s = locate_session(c);
    struct statistics stat;
    int clients;

    float progress = 0;

    if (!s)
        return;

    clients = session_active_clients(s->psession);
    statistics(s->psession, &stat);

    if (stat.num_clients > 0)
    {
    	progress = (stat.num_clients  - clients) / (float)stat.num_clients;
    }

    response_open_no_status(c, "stat");
    wrbuf_printf(c->wrbuf, "<activeclients>%d</activeclients>\n", clients);
    wrbuf_printf(c->wrbuf, "<hits>" ODR_INT_PRINTF "</hits>\n", stat.num_hits);
    wrbuf_printf(c->wrbuf, "<records>%d</records>\n", stat.num_records);
    wrbuf_printf(c->wrbuf, "<clients>%d</clients>\n", stat.num_clients);
    wrbuf_printf(c->wrbuf, "<unconnected>%d</unconnected>\n", stat.num_no_connection);
    wrbuf_printf(c->wrbuf, "<connecting>%d</connecting>\n", stat.num_connecting);
    wrbuf_printf(c->wrbuf, "<working>%d</working>\n", stat.num_working);
    wrbuf_printf(c->wrbuf, "<idle>%d</idle>\n", stat.num_idle);
    wrbuf_printf(c->wrbuf, "<failed>%d</failed>\n", stat.num_failed);
    wrbuf_printf(c->wrbuf, "<error>%d</error>\n", stat.num_error);
    wrbuf_printf(c->wrbuf, "<progress>%.2f</progress>\n", progress);
    response_close(c, "stat");
    release_session(c, s);
}

static void cmd_info(struct http_channel *c)
{
    char yaz_version_str[20];

    response_open_no_status(c, "info");
    wrbuf_puts(c->wrbuf, " <version>\n");
    wrbuf_puts(c->wrbuf, "  <pazpar2");
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
    
    info_services(c->server, c->wrbuf);

    response_close(c, "info");
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
    { "session-status", cmd_session_status },
    { "server-status", cmd_server_status },
    { "ping", cmd_ping },
    { "record", cmd_record },
    { "info", cmd_info },
    {0,0}
};

void http_command(struct http_channel *c)
{
    const char *command = http_argbyname(c->request, "command");
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

