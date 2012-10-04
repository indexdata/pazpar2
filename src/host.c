/* This file is part of Pazpar2.
   Copyright (C) 2006-2012 Index Data

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

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <yaz/log.h>
#include <yaz/nmem.h>

#include "ppmutex.h"
#include "session.h"
#include "host.h"

#include <sys/types.h>

struct database_hosts {
    struct host *hosts;
    YAZ_MUTEX mutex;
};

// Create a new host structure for hostport
static struct host *create_host(const char *url, const char *proxy,
                                int default_port,
                                iochan_man_t iochan_man)
{
    struct host *host;
    char *db_comment;

    host = xmalloc(sizeof(struct host));
    host->url = xstrdup(url);
    host->proxy = 0;
    host->tproxy = 0;
    if (proxy && *proxy)
        host->proxy = xstrdup(proxy);
    else
    {
        char *cp;

        host->tproxy = xmalloc (strlen(url) + 10); /* so we can add :port */
        strcpy(host->tproxy, url);
        for (cp = host->tproxy; *cp; cp++)
            if (strchr("/?#~", *cp))
            {
                *cp = '\0';
                break;
            }
        if (!strchr(host->tproxy, ':'))
            sprintf(cp, ":%d", default_port); /* no port given, add it */
    }

    db_comment = strchr(host->url, '#');
    if (db_comment)
        *db_comment = '\0';
    host->connections = 0;
    host->ipport = 0;
    host->mutex = 0;

    if (host_getaddrinfo(host, iochan_man))
    {
        xfree(host->ipport);
        xfree(host->tproxy);
        xfree(host->proxy);
        xfree(host->url);
        xfree(host);
        return 0;
    }
    pazpar2_mutex_create(&host->mutex, "host");

    yaz_cond_create(&host->cond_ready);

    return host;
}

struct host *find_host(database_hosts_t hosts, const char *url,
                       const char *proxy, int port,
                       iochan_man_t iochan_man)
{
    struct host *p;
    yaz_mutex_enter(hosts->mutex);
    for (p = hosts->hosts; p; p = p->next)
        if (!strcmp(p->url, url))
        {
            if (p->proxy && proxy && !strcmp(p->proxy, proxy))
                break;
            if (!p->proxy && !proxy)
                break;
        }
    if (!p)
    {
        p = create_host(url, proxy, port, iochan_man);
        if (p)
        {
            p->next = hosts->hosts;
            hosts->hosts = p;
        }
    }
    yaz_mutex_leave(hosts->mutex);
    return p;
}

database_hosts_t database_hosts_create(void)
{
    database_hosts_t p = xmalloc(sizeof(*p));
    p->hosts = 0;
    p->mutex = 0;
    pazpar2_mutex_create(&p->mutex, "database");
    return p;
}

void database_hosts_destroy(database_hosts_t *pp)
{
    if (*pp)
    {
        struct host *p = (*pp)->hosts;
        while (p)
        {
            struct host *p_next = p->next;
            yaz_mutex_destroy(&p->mutex);
            yaz_cond_destroy(&p->cond_ready);
            xfree(p->url);
            xfree(p->ipport);
            xfree(p);
            p = p_next;
        }
        yaz_mutex_destroy(&(*pp)->mutex);
        xfree(*pp);
    }
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

