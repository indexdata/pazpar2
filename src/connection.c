/* $Id: connection.c,v 1.4 2007-06-06 11:49:48 marc Exp $
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

/** \file connection.c
    \brief Z39.50 connection (low-level client)
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

#if HAVE_CONFIG_H
#include "cconfig.h"
#endif

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
    COMSTACK link;
    struct host *host;
    struct client *client;
    char *ibuf;
    int ibufsize;
    char *authentication; // Empty string or authentication string if set
    enum {
        Conn_Resolving,
        Conn_Connecting,
        Conn_Open,
        Conn_Waiting,
    } state;
    struct connection *next; // next for same host or next in free list
};

static struct connection *connection_freelist = 0;

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
        cs_close(co->link);
        iochan_destroy(co->iochan);
    }

    yaz_log(YLOG_DEBUG, "Connection destroy %s", co->host->hostport);

    remove_connection_from_host(co);
    if (co->client)
    {
        client_disconnect(co->client);
    }
    co->next = connection_freelist;
    connection_freelist = co;
}

// Creates a new connection for client, associated with the host of 
// client's database
struct connection *connection_create(struct client *cl)
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
    new->authentication = "";
    client_set_connection(cl, new);
    new->link = 0;
    new->state = Conn_Resolving;
    if (host->ipport)
        connection_connect(new);
    return new;
}

static void connection_handler(IOCHAN i, int event)
{
    struct connection *co = iochan_getdata(i);
    struct client *cl = co->client;
    struct session *se = 0;

    if (cl)
        se = client_get_session(cl);
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
                client_set_state(cl, Client_Connected);
	}
    }

    else if (event & EVENT_INPUT)
    {
	int len = cs_get(co->link, &co->ibuf, &co->ibufsize);

	if (len < 0)
	{
            yaz_log(YLOG_WARN|YLOG_ERRNO, "Error reading from %s", 
                    client_get_url(cl));
            connection_destroy(co);
	    return;
	}
        else if (len == 0)
	{
            yaz_log(YLOG_WARN, "EOF reading from %s", client_get_url(cl));
            connection_destroy(co);
	    return;
	}
	else if (len > 1) // We discard input if we have no connection
	{
            co->state = Conn_Open;

            if (client_is_our_response(cl))
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
                        client_init_response(cl, a);
                        break;
                    case Z_APDU_searchResponse:
                        client_search_response(cl, a);
                        break;
                    case Z_APDU_presentResponse:
                        client_present_response(cl, a);
                        break;
                    case Z_APDU_close:
                        client_close_response(cl, a);
                        break;
                    default:
                        yaz_log(YLOG_WARN, 
                                "Unexpected Z39.50 response from %s",  
                                client_get_url(cl));
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
                client_set_state(cl, Client_Idle);
            }
	}
	/* if len==1 we do nothing but wait for more input */
    }
    client_continue(cl);
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
            }
            else if (!con->client)
            {
                yaz_log(YLOG_WARN, "connect_unresolved_host : ophan client");
                connection_destroy(con);
                /* start all over .. at some point it will be NULL */
                con = host->connections;
            }
            else
            {
                connection_connect(con);
                con = con->next;
            }
        }
    }
}

int connection_send_apdu(struct connection *co, Z_APDU *a)
{
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
    iochan_setflags(co->iochan, EVENT_INPUT);
    return 0;
}

struct host *connection_get_host(struct connection *con)
{
    return con->host;
}

int connection_connect(struct connection *con)
{
    COMSTACK link = 0;
    struct host *host = connection_get_host(con);
    void *addr;
    int res;

    struct session_database *sdb = client_get_database(con->client);
    char *zproxy = session_setting_oneval(sdb, PZ_ZPROXY);

    assert(host->ipport);
    assert(con);

    if (!(link = cs_create(tcpip_type, 0, PROTO_Z3950)))
    {
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "Failed to create comstack");
        return -1;
    }
    
    if (!zproxy || 0 == strlen(zproxy)){
        /* no Z39.50 proxy needed - direct connect */
        yaz_log(YLOG_DEBUG, "Connection create %s", connection_get_url(con));
        
        if (!(addr = cs_straddr(link, host->ipport)))
        {
            yaz_log(YLOG_WARN|YLOG_ERRNO, 
                    "Lookup of IP address %s failed", host->ipport);
            return -1;
        }
        
    } else {
        /* Z39.50 proxy connect */
        yaz_log(YLOG_DEBUG, "Connection create %s proxy %s", 
                connection_get_url(con), zproxy);
        
        if (!(addr = cs_straddr(link, zproxy)))
        {
            yaz_log(YLOG_WARN|YLOG_ERRNO, 
                    "Lookup of ZProxy IP address %s failed", 
                    zproxy);
            return -1;
        }
    }
    
    res = cs_connect(link, addr);
    if (res < 0)
    {
        yaz_log(YLOG_WARN|YLOG_ERRNO, "cs_connect %s",
                connection_get_url(con));
        return -1;
    }
    con->link = link;
    con->state = Conn_Connecting;
    con->iochan = iochan_create(cs_fileno(link), connection_handler, 0);
    iochan_setdata(con->iochan, con);
    pazpar2_add_channel(con->iochan);

    /* this fragment is bad DRY: from client_prep_connection */
    client_set_state(con->client, Client_Connecting);
    iochan_setflag(con->iochan, EVENT_OUTPUT);
    return 0;
}

const char *connection_get_url(struct connection *co)
{
    return client_get_url(co->client);
}

void connection_set_authentication(struct connection *co, char *auth)
{
    co->authentication = auth;
}

// Ensure that client has a connection associated
int client_prep_connection(struct client *cl)
{
    struct connection *co;
    struct session *se = client_get_session(cl);
    struct host *host = client_get_host(cl);

    co = client_get_connection(cl);

    yaz_log(YLOG_DEBUG, "Client prep %s", client_get_url(cl));

    if (!co)
    {
        // See if someone else has an idle connection
        // We should look at timestamps here to select the longest-idle connection
        for (co = host->connections; co; co = co->next)
            if (co->state == Conn_Open &&
                (!co->client || client_get_session(co->client) != se) &&
                !strcmp(co->authentication,
                    session_setting_oneval(client_get_database(cl),
                    PZ_AUTHENTICATION)))
                break;
        if (co)
        {
            connection_release(co);
            client_set_connection(cl, co);
            co->client = cl;
        }
        else
            co = connection_create(cl);
    }
    if (co)
    {
        if (co->state == Conn_Connecting)
        {
            client_set_state(cl, Client_Connecting);
            iochan_setflag(co->iochan, EVENT_OUTPUT);
        }
        else if (co->state == Conn_Open)
        {
            if (client_get_state(cl) == Client_Error 
                || client_get_state(cl) == Client_Disconnected)
                client_set_state(cl, Client_Idle);
            iochan_setflag(co->iochan, EVENT_OUTPUT);
        }
        return 1;
    }
    else
        return 0;
}



/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
