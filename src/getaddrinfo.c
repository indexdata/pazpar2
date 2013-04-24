/* This file is part of Pazpar2.
   Copyright (C) 2006-2013 Index Data

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

#include "sel_thread.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>

#include <assert.h>
#include <sys/types.h>
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif

#include <yaz/log.h>
#include <yaz/nmem.h>
#include <yaz/tcpip.h>

#include "session.h"
#include "connection.h"
#include "host.h"

/* Only use a threaded resolver on Unix that offers getaddrinfo.
   gethostbyname is NOT reentrant.
 */
#ifndef WIN32
#define USE_THREADED_RESOLVER 1
#endif

struct work {
    char *hostport;  /* hostport to be resolved in separate thread */
    char *ipport;    /* result or NULL if it could not be resolved */
    struct host *host; /* host that we're dealing with - mother thread */
    iochan_man_t iochan_man; /* iochan manager */
};

static int log_level = YLOG_LOG;

void perform_getaddrinfo(struct work *w)
{
    struct addrinfo hints, *res;
    char host[512], *cp;
    char *port = 0;
    int error;

    hints.ai_flags = 0;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_addrlen        = 0;
    hints.ai_addr           = NULL;
    hints.ai_canonname      = NULL;
    hints.ai_next           = NULL;

    strncpy(host, w->hostport, sizeof(host)-1);
    host[sizeof(host)-1] = 0;
    if ((cp = strrchr(host, ':')))
    {
        *cp = '\0';
        port = cp + 1;
    }
    error = getaddrinfo(host, port ? port : "210", &hints, &res);
    if (error)
    {
        yaz_log(YLOG_WARN, "Failed to resolve %s: %s",
                w->hostport, gai_strerror(error));
    }
    else
    {
        char n_host[512];
        if (getnameinfo((struct sockaddr *) res->ai_addr, res->ai_addrlen,
                        n_host, sizeof(n_host)-1,
                        0, 0,
                        NI_NUMERICHOST) == 0)
        {
            w->ipport = xmalloc(strlen(n_host) + (port ? strlen(port) : 0) + 2);
            strcpy(w->ipport, n_host);
            if (port)
            {
                strcat(w->ipport, ":");
                strcat(w->ipport, port);
            }
            yaz_log(log_level, "Resolved %s -> %s", w->hostport, w->ipport);
        }
        else
        {
            yaz_log(YLOG_LOG|YLOG_ERRNO, "getnameinfo failed for %s",
                    w->hostport);
        }
        freeaddrinfo(res);
    }
}

static void work_handler(void *vp)
{
    struct work *w = vp;

    int sec = 0;  /* >0 for debugging/testing purposes */
    if (sec)
    {
        yaz_log(log_level, "waiting %d seconds", sec);
#if HAVE_UNISTD_H
        sleep(sec);
#endif
    }
    perform_getaddrinfo(w);
}

#if USE_THREADED_RESOLVER
void iochan_handler(struct iochan *i, int event)
{
    sel_thread_t p = iochan_getdata(i);

    if (event & EVENT_INPUT)
    {
        struct work *w = sel_thread_result(p);
        w->host->ipport = w->ipport;
        connect_resolver_host(w->host, w->iochan_man);
        xfree(w);
    }
}

static sel_thread_t resolver_thread = 0;

static void getaddrinfo_start(iochan_man_t iochan_man)
{
    int fd;
    sel_thread_t p = resolver_thread =
        sel_thread_create(work_handler, 0 /* work_destroy */, &fd,
                          3 /* no of resolver threads */);
    if (!p)
    {
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "sel_create_create failed");
        exit(1);
    }
    else
    {
        IOCHAN chan = iochan_create(fd, iochan_handler, EVENT_INPUT,
            "getaddrinfo_socket");
        iochan_setdata(chan, p);
        iochan_add(iochan_man, chan);
    }
    yaz_log(log_level, "resolver start");
    resolver_thread = p;
}
#endif

int host_getaddrinfo(struct host *host, iochan_man_t iochan_man)
{
    struct work *w = xmalloc(sizeof(*w));
    int use_thread = 0; /* =0 to disable threading entirely */

    w->hostport = host->tproxy ? host->tproxy : host->proxy;
    w->ipport = 0;
    w->host = host;
    w->iochan_man = iochan_man;
#if USE_THREADED_RESOLVER
    if (use_thread)
    {
        if (resolver_thread == 0)
            getaddrinfo_start(iochan_man);
        assert(resolver_thread);
        sel_thread_add(resolver_thread, w);
        return 0;
    }
#endif
    perform_getaddrinfo(w);
    host->ipport = w->ipport;
    xfree(w);
    if (!host->ipport)
        return -1;
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

