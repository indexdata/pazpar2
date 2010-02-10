/* This file is part of Pazpar2.
   Copyright (C) 2006-2010 Index Data

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

#ifndef HTTP_H
#define HTTP_H

#include "eventl.h"
// Generic I/O buffer
struct http_buf;
typedef struct http_channel_observer_s *http_channel_observer_t;

typedef struct http_server *http_server_t;
typedef struct http_sessions *http_sessions_t;

struct http_channel
{
    IOCHAN iochan;
    struct http_buf *iqueue;
    struct http_buf *oqueue;
    char version[10];
    struct http_proxy *proxy;
    enum
    {
        Http_Idle,
        Http_Busy      // Don't process new HTTP requests while we're busy
    } state;
    int keep_alive;
    NMEM nmem;
    WRBUF wrbuf;
    struct http_request *request;
    struct http_response *response;
    struct http_channel *next; // for freelist
    char addr[20]; // forwarded address
    http_channel_observer_t observers;
    struct conf_server *server;
    http_server_t http_server;
    http_sessions_t http_sessions;
};

struct http_proxy //  attached to iochan for proxy connection
{
    IOCHAN iochan;
    struct http_channel *channel;
    struct http_buf *oqueue;
    int first_response;
};

struct http_header
{
    char *name;
    char *value;
    struct http_header *next;
};

struct http_argument
{
    char *name;
    char *value;
    struct http_argument *next;
};

struct http_request
{
    struct http_channel *channel;
    char http_version[20];
    char method[20];
    char *path;
    char *search;
    char *content_buf;
    int content_len;
    struct http_header *headers;
    struct http_argument *arguments;
};

struct http_response
{
    char code[4];
    char *msg;
    struct http_channel *channel;
    struct http_header *headers;
    char *payload;
    char *content_type;
};

void http_mutex_init(struct conf_server *server);
void http_server_destroy(http_server_t hs);

void http_set_proxyaddr(const char *url, struct conf_server *ser);
int http_init(const char *addr, struct conf_server *ser);
void http_close_server(struct conf_server *ser);
void http_addheader(struct http_response *r, 
                    const char *name, const char *value);
const char *http_lookup_header(struct http_header *header,
                               const char *name);
struct http_header * http_header_append(struct http_channel *ch, 
                                        struct http_header * hp, 
                                        const char *name, 
                                        const char *value);
const char *http_argbyname(struct http_request *r, const char *name);
const char *http_headerbyname(struct http_header *r, const char *name);
struct http_response *http_create_response(struct http_channel *c);
void http_send_response(struct http_channel *c);
void urlencode(const char *i, char *o);

typedef void (*http_channel_destroy_t)(void *data, struct http_channel *c,
                                       void *data2);

http_channel_observer_t http_add_observer(struct http_channel *c, void *data,
                                          http_channel_destroy_t);
void http_observer_set_data2(http_channel_observer_t obs, void *data2);

void http_remove_observer(http_channel_observer_t obs);
struct http_channel *http_channel_observer_chan(http_channel_observer_t obs);

void http_command(struct http_channel *c);

http_sessions_t http_sessions_create(void);
void http_sessions_destroy(http_sessions_t hs);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

