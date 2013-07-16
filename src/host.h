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

#ifndef HOST_H
#define HOST_H

#include <yaz/mutex.h>

typedef struct database_hosts *database_hosts_t;

/** \brief Represents a host (irrespective of databases) */
struct host {
    char *tproxy;   // tproxy address (no Z39.50 UI)
    char *proxy;    // logical proxy address
    char *ipport;   // tproxy or proxy resolved
    struct connection *connections; // All connections to this
    struct host *next;
    YAZ_MUTEX mutex;
    YAZ_COND cond_ready;
};

database_hosts_t database_hosts_create(void);
void database_hosts_destroy(database_hosts_t *);

struct host *find_host(database_hosts_t hosts, const char *hostport,
		       const char *proxy, int port, iochan_man_t iochan_man);

int host_getaddrinfo(struct host *host, iochan_man_t iochan_man);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

