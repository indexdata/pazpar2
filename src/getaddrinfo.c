/* $Id: getaddrinfo.c,v 1.2 2007-04-22 16:41:42 adam Exp $
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

#if HAVE_CONFIG_H
#include "cconfig.h"
#endif

#include "sel_thread.h"

#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#include <yaz/log.h>
#include <yaz/nmem.h>
#include <yaz/tcpip.h>

#include "pazpar2.h"

struct work {
    char *hostport;  /* hostport to be resolved in separate thread */
    char *ipport;    /* result or NULL if it could not be resolved */
    struct host *host; /* host that we're dealing with - mother thread */
};

static int log_level = YLOG_LOG;

void perform_getaddrinfo(struct work *w)
{
    int res = 0;
    char *port;
    struct addrinfo *addrinfo, hints;
    char *hostport = xstrdup(w->hostport);
    
    if ((port = strchr(hostport, ':')))
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
    // This is not robust code. It assumes that getaddrinfo always
    // returns AF_INET address.
    if ((res = getaddrinfo(hostport, port, &hints, &addrinfo)))
    {
        yaz_log(YLOG_WARN, "Failed to resolve %s: %s", 
                w->hostport, gai_strerror(res));
    }
    else
    {
        char ipport[128];
        unsigned char addrbuf[4];
        assert(addrinfo->ai_family == PF_INET);
        memcpy(addrbuf, 
               &((struct sockaddr_in*)addrinfo->ai_addr)->sin_addr.s_addr, 4);
        sprintf(ipport, "%u.%u.%u.%u:%s",
                addrbuf[0], addrbuf[1], addrbuf[2], addrbuf[3], port);
        freeaddrinfo(addrinfo);
        w->ipport = xstrdup(ipport);
        yaz_log(log_level, "%s -> %s", hostport, ipport);
    }
    xfree(hostport);
}

static void work_handler(void *vp)
{
    struct work *w = vp;

    int sec = 0;  /* >0 for debugging/testing purposes */
    if (sec)
    {
        yaz_log(log_level, "waiting %d seconds", sec);
        sleep(sec);
    }
    perform_getaddrinfo(w);
}

void iochan_handler(struct iochan *i, int event)
{
    sel_thread_t p = iochan_getdata(i);

    if (event & EVENT_INPUT)
    {
        struct work *w = sel_thread_result(p);
        if (w->ipport)
            yaz_log(log_level, "resolved result %s", w->ipport);
        else
            yaz_log(log_level, "unresolved result");
        w->host->ipport = w->ipport;
        connect_resolver_host(w->host);
        xfree(w);
    }
}

static sel_thread_t resolver_thread = 0;

static void getaddrinfo_start(void)
{
    int fd;
    sel_thread_t p = resolver_thread = sel_thread_create(work_handler, &fd);
    if (!p)
    {
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "sel_create_create failed");
        exit(1);
    }
    else
    {
        IOCHAN chan = iochan_create(fd, iochan_handler, EVENT_INPUT);
        iochan_setdata(chan, p);
        pazpar2_add_channel(chan);
    }
    yaz_log(log_level, "resolver start");
    resolver_thread = p;
}

int host_getaddrinfo(struct host *host)
{
    struct work *w = xmalloc(sizeof(*w));
    int use_thread = 1; /* =0 to disable threading entirely */

    w->hostport = host->hostport;
    w->ipport = 0;
    w->host = host;
    if (use_thread)
    {
        if (resolver_thread == 0)
            getaddrinfo_start();
        assert(resolver_thread);
        sel_thread_add(resolver_thread, w);
    }
    else
    {
        perform_getaddrinfo(w);
        host->ipport = w->ipport;
        xfree(w);
        if (!host->ipport)
            return -1;
    }
    return 0;
}

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
