/* $Id: http.h,v 1.7 2007-04-10 08:48:56 adam Exp $
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

#ifndef HTTP_H
#define HTTP_H

// Generic I/O buffer
struct http_buf
{
#define HTTP_BUF_SIZE 4096
    char buf[4096];
    int offset;
    int len;
    struct http_buf *next;
};

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
    NMEM nmem;
    WRBUF wrbuf;
    struct http_request *request;
    struct http_response *response;
    struct http_channel *next; // for freelist
    char *addr; /* forwarded address */
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
};

void http_set_proxyaddr(char *url, char *baseurl);
void http_init(const char *addr);
void http_addheader(struct http_response *r, 
                    const char *name, const char *value);
struct http_header * http_header_append(struct http_channel *ch, 
                                        struct http_header * hp, 
                                        const char *name, 
                                        const char *value);
char *http_argbyname(struct http_request *r, char *name);
char *http_headerbyname(struct http_header *r, char *name);
struct http_response *http_create_response(struct http_channel *c);
void http_send_response(struct http_channel *c);
void urlencode(const char *i, char *o);

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
#endif
