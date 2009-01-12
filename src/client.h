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

/** \file client.h
    \brief Z39.50 client 
*/

#ifndef CLIENT_H
#define CLIENT_H

struct client;
struct connection;

enum client_state
{
    Client_Connecting,
    Client_Idle,
    Client_Working,
    Client_Error,
    Client_Failed,
    Client_Disconnected
};

int client_show_raw_begin(struct client *cl, int position,
                          const char *syntax, const char *esn,
                          void *data,
                          void (*error_handler)(void *data, const char *addinfo),
                          void (*record_handler)(void *data, const char *buf,
                                                 size_t sz),
                          void **data2,
                          int binary);
void client_show_raw_remove(struct client *cl, void *rr);

const char *client_get_state_str(struct client *cl);
enum client_state client_get_state(struct client *cl);
void client_set_state(struct client *cl, enum client_state st);
void client_fatal(struct client *cl);
struct connection *client_get_connection(struct client *cl);
struct session_database *client_get_database(struct client *cl);
void client_set_database(struct client *cl, struct session_database *db);
struct session *client_get_session(struct client *cl);
const char *client_get_pquery(struct client *cl);

void client_init_response(struct client *cl, Z_APDU *a);
void client_search_response(struct client *cl);
void client_record_response(struct client *cl);
void client_close_response(struct client *cl, Z_APDU *a);

int client_is_our_response(struct client *cl);

void client_continue(struct client *cl);

struct client *client_create(void);
void client_destroy(struct client *c);

void client_set_connection(struct client *cl, struct connection *con);
void client_disconnect(struct client *cl);
int client_prep_connection(struct client *cl);
void client_start_search(struct client *cl);
void client_set_session(struct client *cl, struct session *se);
int client_is_active(struct client *cl);
struct client *client_next_in_session(struct client *cl);

int client_parse_query(struct client *cl, const char *query);
int client_get_hits(struct client *cl);
int client_get_num_records(struct client *cl);
int client_get_diagnostic(struct client *cl);
void client_set_database(struct client *cl, struct session_database *db);
struct host *client_get_host(struct client *cl);
const char *client_get_url(struct client *cl);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
