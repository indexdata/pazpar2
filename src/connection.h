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

/** \file connection.h
    \brief Z39.50 connection (low-level client)
*/

#ifndef CONNECTION_H
#define CONNECTION_H
#include <yaz/zoom.h>

#include "eventl.h"

struct connection;
struct host;

void connect_resolver_host(struct host *host, iochan_man_t iochan);
ZOOM_connection connection_get_link(struct connection *co);
void connection_continue(struct connection *co);
void connect_resolver_host(struct host *host, iochan_man_t iochan_man);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

