/* This file is part of Pazpar2.
   Copyright (C) Index Data

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

/** \file connection.c
    \brief Z39.50 connection (low-level client)
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

#include <yaz/log.h>
#include <yaz/comstack.h>
#include <yaz/tcpip.h>
#include "connection.h"
#include "session.h"
#include "client.h"
#include "settings.h"

/* connection counting (1) , disable connection counting (0) */
#if 1
static YAZ_MUTEX g_mutex = 0;
static int no_connections = 0;
static int total_no_connections = 0;

static int connection_use(int delta)
{
    int result;
    if (!g_mutex)
        yaz_mutex_create(&g_mutex);
    yaz_mutex_enter(g_mutex);
    no_connections += delta;
    result = no_connections;
    if (delta > 0)
        total_no_connections += delta;
    yaz_mutex_leave(g_mutex);
    if (delta == 0)
            return result;
    yaz_log(YLOG_LOG, "%s connections=%d", delta > 0 ? "INC" : "DEC",
            no_connections);
    return result;
}

int connections_count(void)
{
    return connection_use(0);
}


#else
#define connection_use(x)
#define connections_count(x) 0
#define connections_count_total(x) 0
#endif


/** \brief Represents a physical, reusable  connection to a remote Z39.50 host
 */
struct connection {
    IOCHAN iochan;
    ZOOM_connection link;
    struct client *client;
    char *zproxy;
    char *url;
    enum {
        Conn_Closed,
        Conn_Connecting,
        Conn_Open
    } state;
    int operation_timeout;
    int session_timeout;
    struct connection *next; // next for same host or next in free list
};

static int connection_connect(struct connection *con, iochan_man_t iochan_man);

ZOOM_connection connection_get_link(struct connection *co)
{
    return co->link;
}

void connection_mark_dead(struct connection *co)
{ 
    iochan_settimeout(co->iochan, 1);
}

// Close connection and recycle structure
static void connection_destroy(struct connection *co)
{
    if (co->link)
        ZOOM_connection_destroy(co->link);
    if (co->iochan)
        iochan_destroy(co->iochan);
    yaz_log(YLOG_DEBUG, "%p Connection destroy %s", co, co->url);

    if (co->client)
    {
        client_disconnect(co->client);
    }

    xfree(co->zproxy);
    xfree(co->url);
    xfree(co);
    connection_use(-1);
}

// Creates a new connection for client, associated with the host of
// client's database
static struct connection *connection_create(struct client *cl,
                                            const char *url,
                                            const char *zproxy,
                                            int operation_timeout,
                                            int session_timeout,
                                            iochan_man_t iochan_man)
{
    struct connection *co;
    int ret;

    co = xmalloc(sizeof(*co));

    co->client = cl;
    co->url = xstrdup(url);
    co->zproxy = 0;
    co->iochan = 0;
    if (zproxy)
        co->zproxy = xstrdup(zproxy);

    client_set_connection(cl, co);
    co->link = 0;
    co->state = Conn_Closed;
    co->operation_timeout = operation_timeout;
    co->session_timeout = session_timeout;

    ret = connection_connect(co, iochan_man);
    connection_use(1);
    if (ret)
    {   /* error */
        connection_destroy(co);
        co = 0;
    }
    return co;
}

static void non_block_events(struct connection *co)
{
    int got_records = 0;
    IOCHAN iochan = co->iochan;
    ZOOM_connection link = co->link;
    while (1)
    {
        struct client *cl = co->client;
        int ev;
        int r = ZOOM_event_nonblock(1, &link);
        if (!r)
            break;
        if (!cl)
            continue;
        ev = ZOOM_connection_last_event(link);

#if 1
        yaz_log(YLOG_DEBUG, "%p Connection ZOOM_EVENT_%s", co, ZOOM_get_event_str(ev));
#endif
        switch (ev)
        {
        case ZOOM_EVENT_TIMEOUT:
            break;
        case ZOOM_EVENT_END:
            {
                const char *error, *addinfo;
                int err;
                if ((err = ZOOM_connection_error(link, &error, &addinfo)))
                {
                    struct session *se = client_get_session(cl);

                    session_log(se, YLOG_WARN, "%s: Error %s (%s)",
                                client_get_id(cl), error, addinfo);
                    client_set_diagnostic(cl, err, error, addinfo);
                    client_set_state(cl, Client_Error);
                }
                else
                {
                    iochan_settimeout(iochan, co->session_timeout);
                    client_set_state(cl, Client_Idle);
                }
            }
            break;
        case ZOOM_EVENT_SEND_DATA:
            break;
        case ZOOM_EVENT_RECV_DATA:
            break;
        case ZOOM_EVENT_UNKNOWN:
            break;
        case ZOOM_EVENT_SEND_APDU:
            client_set_state(co->client, Client_Working);
            iochan_settimeout(iochan, co->operation_timeout);
            break;
        case ZOOM_EVENT_RECV_APDU:
            break;
        case ZOOM_EVENT_CONNECT:
            co->state = Conn_Open;
            break;
        case ZOOM_EVENT_RECV_SEARCH:
            client_search_response(cl);
            break;
        case ZOOM_EVENT_RECV_RECORD:
            client_record_response(cl, &got_records);
            break;
        case ZOOM_EVENT_NONE:
            break;
        default:
            yaz_log(YLOG_LOG, "Unhandled event (%d) from %s",
                    ev, client_get_id(cl));
            break;
        }
    }
    if (got_records)
    {
        struct client *cl = co->client;
        if (cl)
            client_got_records(cl);
    }
}

static void iochan_update(struct connection *co)
{
    if (co->link)
    {
        int m = ZOOM_connection_get_mask(co->link);

        if (m == 0)
            m = ZOOM_SELECT_READ;
        iochan_setflags(co->iochan, m);
        iochan_setfd(co->iochan, ZOOM_connection_get_socket(co->link));
    }
}

void connection_continue(struct connection *co)
{
    int r = ZOOM_connection_exec_task(co->link);
    if (!r)
    {
        struct client *cl = co->client;

        client_lock(cl);
        non_block_events(co);
        client_unlock(cl);
    }
    else
        iochan_update(co);
}

static void connection_handler(IOCHAN iochan, int event)
{
    struct connection *co = iochan_getdata(iochan);
    struct client *cl;

    cl = co->client;
    if (!cl)
    {
        /* no client associated with it.. We are probably getting
           a closed connection from the target.. Or, perhaps, an unexpected
           package.. We will just close the connection */
        yaz_log(YLOG_LOG, "timeout connection %p event=%d", co, event);
        connection_destroy(co);
    }
    else if (event & EVENT_TIMEOUT)
    {
        ZOOM_connection_fire_event_timeout(co->link);
        client_lock(cl);
        non_block_events(co);
        client_unlock(cl);

        connection_destroy(co);
    }
    else
    {
        if (ZOOM_connection_is_idle(co->link))
        {
            connection_destroy(co);
            return;
        }
        client_lock(cl);
        non_block_events(co);

        ZOOM_connection_fire_event_socket(co->link, event);

        non_block_events(co);
        client_unlock(cl);

        iochan_update(co);
    }
}

void connection_release2(struct connection *co)
{
    co->client = 0;
}

static int connection_connect(struct connection *con, iochan_man_t iochan_man)
{
    ZOOM_options zoptions = ZOOM_options_create();
    const char *auth;
    const char *charset;
    const char *sru;
    const char *sru_version = 0;
    const char *value;
    int r = 0;
    WRBUF w;

    struct client *cl = con->client;
    struct session_database *sdb = client_get_database(cl);
    const char *apdulog = session_setting_oneval(sdb, PZ_APDULOG);
    struct session *se = client_get_session(cl);
    const char *memcached = session_setting_oneval(sdb, PZ_MEMCACHED);
    const char *redis = session_setting_oneval(sdb, PZ_REDIS);
    const char *error, *addinfo;
    int err;

    assert(con);

    ZOOM_options_set(zoptions, "async", "1");
    ZOOM_options_set(zoptions, "implementationName", PACKAGE_NAME);
    ZOOM_options_set(zoptions, "implementationVersion", VERSION);

    if ((charset = session_setting_oneval(sdb, PZ_NEGOTIATION_CHARSET)))
        ZOOM_options_set(zoptions, "charset", charset);
    if (memcached && *memcached)
        ZOOM_options_set(zoptions, "memcached", memcached);
    if (redis && *redis)
        ZOOM_options_set(zoptions, "redis", redis);

    if (con->zproxy)
    {
        yaz_log(YLOG_LOG, "proxy=%s", con->zproxy);
        ZOOM_options_set(zoptions, "proxy", con->zproxy);
    }
    if (apdulog && *apdulog)
        ZOOM_options_set(zoptions, "apdulog", apdulog);


    if ((sru = session_setting_oneval(sdb, PZ_SRU)) && *sru)
        ZOOM_options_set(zoptions, "sru", sru);
    if ((sru_version = session_setting_oneval(sdb, PZ_SRU_VERSION))
        && *sru_version)
        ZOOM_options_set(zoptions, "sru_version", sru_version);

    if ((auth = session_setting_oneval(sdb, PZ_AUTHENTICATION)))
    {
        /* allow splitting user and reset with a blank always */
        const char *cp1 = strchr(auth, ' ');
        if (!cp1 && sru && *sru)
            cp1 =  strchr(auth, '/');
        if (!cp1)
        {
            /* Z39.50 user/password style, or no password for SRU */
            ZOOM_options_set(zoptions, "user", auth);
        }
        else
        {
            /* now consider group as well */
            const char *cp2 = strchr(cp1 + 1, ' ');

            ZOOM_options_setl(zoptions, "user", auth, cp1 - auth);
            if (!cp2)
                ZOOM_options_set(zoptions, "password", cp1 + 1);
            else
            {
                ZOOM_options_setl(zoptions, "group", cp1 + 1, cp2 - cp1 - 1);
                ZOOM_options_set(zoptions, "password", cp2 + 1);
            }
        }
    }

    value = session_setting_oneval(sdb, PZ_AUTHENTICATION_MODE);
    if (value && *value)
        ZOOM_options_set(zoptions, "authenticationMode", value);

    if (!(con->link = ZOOM_connection_create(zoptions)))
    {
        session_log(se, YLOG_WARN, "%s: ZOOM_connection_create failed",
                    client_get_id(cl));
        client_set_state_nb(cl, Client_Error);
        client_set_diagnostic(cl, 2,
                              ZOOM_diag_str(2),
                              "Cannot create connection");
        ZOOM_options_destroy(zoptions);
        return -1;
    }

    w = wrbuf_alloc();
    if (sru && *sru && !strstr(con->url, "://"))
        wrbuf_puts(w, "http://");
    if (strchr(con->url, '#'))
    {
        const char *cp = strchr(con->url, '#');
        wrbuf_write(w, con->url, cp - con->url);
    }
    else
        wrbuf_puts(w, con->url);

    ZOOM_connection_connect(con->link, wrbuf_cstr(w), 0);

    if ((err = ZOOM_connection_error(con->link, &error, &addinfo)))
    {
        struct session *se = client_get_session(cl);
        session_log(se, YLOG_WARN, "%s: Error %s (%s)",
                    client_get_id(cl), error, addinfo);
        client_set_diagnostic(cl, err, error, addinfo);
        client_set_state_nb(cl, Client_Error);
        ZOOM_connection_destroy(con->link);
        con->link = 0;
        r = -1;
    }
    else
    {
        con->iochan = iochan_create(-1, connection_handler, 0,
                                    "connection_socket");
        con->state = Conn_Connecting;
        iochan_settimeout(con->iochan, con->operation_timeout);
        iochan_setdata(con->iochan, con);
        if (iochan_add(iochan_man, con->iochan, 20))
        {
            session_log(se, YLOG_WARN, "%s: out of connections",
                        client_get_id(cl));
            iochan_destroy(con->iochan);
            con->iochan = 0;
            ZOOM_connection_destroy(con->link);
            con->link = 0;
            r = -1;
        }
        else
        {
            client_set_state(cl, Client_Connecting);
        }
    }
    ZOOM_options_destroy(zoptions);
    wrbuf_destroy(w);
    return r;
}

// Ensure that client has a connection associated
int client_prep_connection(struct client *cl,
                           int operation_timeout, int session_timeout,
                           iochan_man_t iochan_man,
                           const struct timeval *abstime)
{
    struct connection *co;
    struct session_database *sdb = client_get_database(cl);
    const char *zproxy = session_setting_oneval(sdb, PZ_ZPROXY);
    const char *url = session_setting_oneval(sdb, PZ_URL);

    if (zproxy && zproxy[0] == '\0')
        zproxy = 0;

    if (!url || !*url)
        url = sdb->database->id;

    yaz_log(YLOG_DEBUG, "client_prep_connection: target=%s url=%s",
            client_get_id(cl), url);

    co = client_get_connection(cl);
    if (co)
        return 2;
    if (!co)
    {
        co = connection_create(cl, url, zproxy,
                               operation_timeout, session_timeout,
                               iochan_man);
    }

    if (co && co->link)
        return 1;
    else
        return 0;
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

