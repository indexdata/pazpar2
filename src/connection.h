/* $Id: connection.h,v 1.3 2007-06-02 04:32:28 quinn Exp $
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

/** \file connection.h
    \brief Z39.50 connection (low-level client)
*/

#ifndef CONNECTION_H
#define CONNECTION_H

#include <yaz/proto.h>
#include "eventl.h"

struct client;
struct connection;
struct host;
struct session;

void connection_destroy(struct connection *co);
struct connection *connection_create(struct client *cl);
void connect_resolver_host(struct host *host);
int connection_send_apdu(struct connection *co, Z_APDU *a);
struct host *connection_get_host(struct connection *con);
void connection_set_authentication(struct connection *co, char *auth);
int connection_connect(struct connection *con);
struct connection *connection_get_available(struct connection *con_list,
                                            struct session *se);
int connection_prep_connection(struct connection *co, struct session *se);
const char *connection_get_url(struct connection *co);
void connection_release(struct connection *co);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
