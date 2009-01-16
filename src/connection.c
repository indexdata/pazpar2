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
#include "eventl.h"
#include "pazpar2.h"
#include "host.h"
#include "client.h"
#include "settings.h"
#include "parameters.h"


/** \brief Represents a physical, reusable  connection to a remote Z39.50 host
 */
struct connection {
    IOCHAN iochan;
    ZOOM_connection link;
    ZOOM_resultset resultset;
    struct host *host;
    struct client *client;
    char *ibuf;
    int ibufsize;
    char *zproxy;
    enum {
        Conn_Resolving,
        Conn_Connecting,
        Conn_Open
    } state;
    struct connection *next; // next for same host or next in free list
};

static struct connection *connection_freelist = 0;

static int connection_connect(struct connection *con);

static int connection_is_idle(struct connection *co)
{
    ZOOM_connection link = co->link;
    int event;

    if (co->state != Conn_Open || !link)
        return 0;

    if (!ZOOM_connection_is_idle(link))
        return 0;
    event = ZOOM_connection_peek_event(link);
    if (event == ZOOM_EVENT_NONE || event == ZOOM_EVENT_END)
        return 1;
    else
        return 0;
}

ZOOM_connection connection_get_link(struct connection *co)
{
    return co->link;
}

ZOOM_resultset connection_get_resultset(struct connection *co)
{
    return co->resultset;
}

void connection_set_resultset(struct connection *co, ZOOM_resultset rs)
{
    if (co->resultset)
        ZOOM_resultset_destroy(co->resultset);
    co->resultset = rs;
}

static void remove_connection_from_host(struct connection *con)
{
    struct connection **conp = &con->host->connections;
    assert(con);
    while (*conp)
    {
        if (*conp == con)
        {
            *conp = (*conp)->next;
            return;
        }
        conp = &(*conp)->next;
    }
    assert(*conp == 0);
}

// Close connection and recycle structure
void connection_destroy(struct connection *co)
{
    if (co->link)
    {
        ZOOM_connection_destroy(co->link);
        iochan_destroy(co->iochan);
    }
    if (co->resultset)
        ZOOM_resultset_destroy(co->resultset);

    yaz_log(YLOG_DEBUG, "Connection destroy %s", co->host->hostport);

    remove_connection_from_host(co);
    if (co->client)
    {
        client_disconnect(co->client);
    }
    xfree(co->zproxy);
    co->zproxy = 0;
    co->next = connection_freelist;
    connection_freelist = co;
}

// Creates a new connection for client, associated with the host of 
// client's database
static struct connection *connection_create(struct client *cl)
{
    struct connection *new;
    struct host *host = client_get_host(cl);

    if ((new = connection_freelist))
        connection_freelist = new->next;
    else
    {
        new = xmalloc(sizeof (struct connection));
        new->ibuf = 0;
        new->ibufsize = 0;
    }
    new->host = host;
    new->next = new->host->connections;
    new->host->connections = new;
    new->client = cl;
    new->zproxy = 0;
    client_set_connection(cl, new);
    new->link = 0;
    new->resultset = 0;
    new->state = Conn_Resolving;
    if (host->ipport)
        connection_connect(new);
    return new;
}

static void non_block_events(struct connection *co)
{
    struct client *cl = co->client;
    IOCHAN iochan = co->iochan;
    ZOOM_connection link = co->link;
    while (1)
    {
        int ev;
        int r = ZOOM_event_nonblock(1, &link);
        if (!r)
            break;
        ev = ZOOM_connection_last_event(link);
#if 0
        yaz_log(YLOG_LOG, "ZOOM_EVENT_%s", ZOOM_get_event_str(ev));
#endif
        switch (ev) 
        {
        case ZOOM_EVENT_END:
            {
                const char *error, *addinfo;
                if (ZOOM_connection_error(link, &error, &addinfo))
                {
                    yaz_log(YLOG_LOG, "Error %s from %s",
                            error, client_get_url(cl));
                }
                client_set_state(cl, Client_Idle);
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
            break;
        case ZOOM_EVENT_RECV_APDU:
            break;
        case ZOOM_EVENT_CONNECT:
            yaz_log(YLOG_LOG, "Connected to %s", client_get_url(cl));
            co->state = Conn_Open;
            iochan_settimeout(iochan, global_parameters.z3950_session_timeout);
            break;
        case ZOOM_EVENT_RECV_SEARCH:
            client_search_response(cl);
            break;
        case ZOOM_EVENT_RECV_RECORD:
            client_record_response(cl);
            break;
        default:
            yaz_log(YLOG_LOG, "Unhandled event (%d) from %s",
                    ev, client_get_url(cl));
        }
    }
}

void connection_continue(struct connection *co)
{
    non_block_events(co);
}

static void connection_handler(IOCHAN iochan, int event)
{
    struct connection *co = iochan_getdata(iochan);
    struct client *cl = co->client;
    struct session *se = 0;

    if (cl)
        se = client_get_session(cl);
    else
    {
        connection_destroy(co);
        return;
    }

    if (event & EVENT_TIMEOUT)
    {
        if (co->state == Conn_Connecting)
        {
            yaz_log(YLOG_WARN,  "connect timeout %s", client_get_url(cl));
            client_fatal(cl);
        }
        else
        {
            yaz_log(YLOG_LOG,  "idle timeout %s", client_get_url(cl));
            connection_destroy(co);
        }
    }
    else
    {
        non_block_events(co);

        ZOOM_connection_fire_event_socket(co->link, event);
        
        non_block_events(co);
    }
}


// Disassociate connection from client
void connection_release(struct connection *co)
{
    struct client *cl = co->client;

    yaz_log(YLOG_DEBUG, "Connection release %s", co->host->hostport);
    if (!cl)
        return;
    client_set_connection(cl, 0);
    co->client = 0;
}

void connect_resolver_host(struct host *host)
{
    struct connection *con = host->connections;
    while (con)
    {
        if (con->state == Conn_Resolving)
        {
            if (!host->ipport) /* unresolved */
            {
                connection_destroy(con);
                /* start all over .. at some point it will be NULL */
                con = host->connections;
                continue;
            }
            else if (!con->client)
            {
                connection_destroy(con);
                /* start all over .. at some point it will be NULL */
                con = host->connections;
                continue;
            }
            else
            {
                connection_connect(con);
                client_start_search(con->client);
            }
        }
        else
        {
            yaz_log(YLOG_LOG, "connect_resolver_host: state=%d", con->state);
        }
        con = con->next;
    }
}

static struct host *connection_get_host(struct connection *con)
{
    return con->host;
}

// Callback for use by event loop
// We do this because ZOOM connections don't always have (the same) sockets
static int socketfun(IOCHAN c)
{
    struct connection *co = iochan_getdata(c);
    if (!co->link)
        return -1;
    return ZOOM_connection_get_socket(co->link);
}

// Because ZOOM always knows what events it is interested in; we may not
static int maskfun(IOCHAN c)
{
    struct connection *co = iochan_getdata(c);
    if (!co->link)
        return 0;

    // This is cheating a little, and assuming that eventl mask IDs are always
    // the same as ZOOM-C's.
    return ZOOM_connection_get_mask(co->link);
}

static int connection_connect(struct connection *con)
{
    ZOOM_connection link = 0;
    struct host *host = connection_get_host(con);
    ZOOM_options zoptions = ZOOM_options_create();
    const char *auth;
    const char *sru;
    const char *sru_version = 0;
    char ipport[512] = "";

    struct session_database *sdb = client_get_database(con->client);
    const char *zproxy = session_setting_oneval(sdb, PZ_ZPROXY);
    const char *apdulog = session_setting_oneval(sdb, PZ_APDULOG);

    assert(host->ipport);
    assert(con);

    ZOOM_options_set(zoptions, "async", "1");
    ZOOM_options_set(zoptions, "implementationName",
            global_parameters.implementationName);
    ZOOM_options_set(zoptions, "implementationVersion",
            global_parameters.implementationVersion);
    if (zproxy && *zproxy)
    {
        con->zproxy = xstrdup(zproxy);
        ZOOM_options_set(zoptions, "proxy", zproxy);
    }
    if (apdulog && *apdulog)
        ZOOM_options_set(zoptions, "apdulog", apdulog);

    if ((auth = session_setting_oneval(sdb, PZ_AUTHENTICATION)))
        ZOOM_options_set(zoptions, "user", auth);
    if ((sru = session_setting_oneval(sdb, PZ_SRU)) && *sru)
        ZOOM_options_set(zoptions, "sru", sru);
    if ((sru_version = session_setting_oneval(sdb, PZ_SRU_VERSION)) 
        && *sru_version)
        ZOOM_options_set(zoptions, "sru_version", sru_version);

    if (!(link = ZOOM_connection_create(zoptions)))
    {
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "Failed to create ZOOM Connection");
        ZOOM_options_destroy(zoptions);
        return -1;
    }

    if (sru && *sru)
        strcpy(ipport, "http://");
    strcat(ipport, host->ipport);

    ZOOM_connection_connect(link, ipport, 0);
    
    con->link = link;
    con->iochan = iochan_create(0, connection_handler, 0);
    con->state = Conn_Connecting;
    iochan_settimeout(con->iochan, global_parameters.z3950_connect_timeout);
    iochan_setdata(con->iochan, con);
    iochan_setsocketfun(con->iochan, socketfun);
    iochan_setmaskfun(con->iochan, maskfun);
    pazpar2_add_channel(con->iochan);

    /* this fragment is bad DRY: from client_prep_connection */
    client_set_state(con->client, Client_Connecting);
    ZOOM_options_destroy(zoptions);
    // This creates the connection
    ZOOM_connection_process(link);
    return 0;
}

const char *connection_get_url(struct connection *co)
{
    return client_get_url(co->client);
}

// Ensure that client has a connection associated
int client_prep_connection(struct client *cl)
{
    struct connection *co;
    struct session *se = client_get_session(cl);
    struct host *host = client_get_host(cl);
    struct session_database *sdb = client_get_database(cl);
    const char *zproxy = session_setting_oneval(sdb, PZ_ZPROXY);

    if (zproxy && zproxy[0] == '\0')
        zproxy = 0;

    co = client_get_connection(cl);

    yaz_log(YLOG_DEBUG, "Client prep %s", client_get_url(cl));

    if (!co)
    {
        // See if someone else has an idle connection
        // We should look at timestamps here to select the longest-idle connection
        for (co = host->connections; co; co = co->next)
            if (connection_is_idle(co) &&
                (!co->client || client_get_session(co->client) != se) &&
                !strcmp(ZOOM_connection_option_get(co->link, "user"),
                        session_setting_oneval(client_get_database(cl),
                                               PZ_AUTHENTICATION)))
            {
                if (zproxy == 0 && co->zproxy == 0)
                    break;
                if (zproxy && co->zproxy && !strcmp(zproxy, co->zproxy))
                    break;
            }
        if (co)
        {
            connection_release(co);
            client_set_connection(cl, co);
            co->client = cl;
            /* tells ZOOM to reconnect if necessary. Disabled becuase
               the ZOOM_connection_connect flushes the task queue */
            ZOOM_connection_connect(co->link, 0, 0);
        }
        else
        {
            co = connection_create(cl);
        }
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

