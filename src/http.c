/* $Id: http.c,v 1.26 2007-04-15 00:35:57 quinn Exp $
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

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#if HAVE_CONFIG_H
#include <cconfig.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <yaz/yaz-util.h>
#include <yaz/comstack.h>
#include <yaz/nmem.h>

#include "cconfig.h"
#include "util.h"
#include "eventl.h"
#include "pazpar2.h"
#include "http.h"
#include "http_command.h"

static void proxy_io(IOCHAN i, int event);
static struct http_channel *http_create(const char *addr);
static void http_destroy(IOCHAN i);

extern IOCHAN channel_list;
extern struct parameters global_parameters;

// If this is set, we proxy normal HTTP requests
static struct sockaddr_in *proxy_addr = 0; 
static char proxy_url[256] = "";
static char myurl[256] = "";
static struct http_buf *http_buf_freelist = 0;
static struct http_channel *http_channel_freelist = 0;

static struct http_buf *http_buf_create()
{
    struct http_buf *r;

    if (http_buf_freelist)
    {
        r = http_buf_freelist;
        http_buf_freelist = http_buf_freelist->next;
    }
    else
        r = xmalloc(sizeof(struct http_buf));
    r->offset = 0;
    r->len = 0;
    r->next = 0;
    return r;
}

static void http_buf_destroy(struct http_buf *b)
{
    b->next = http_buf_freelist;
    http_buf_freelist = b;
}

static void http_buf_destroy_queue(struct http_buf *b)
{
    struct http_buf *p;
    while (b)
    {
        p = b->next;
        http_buf_destroy(b);
        b = p;
    }
}

#ifdef GAGA
// Calculate length of chain
static int http_buf_len(struct http_buf *b)
{
    int sum = 0;
    for (; b; b = b->next)
        sum += b->len;
    return sum;
}
#endif

static struct http_buf *http_buf_bybuf(char *b, int len)
{
    struct http_buf *res = 0;
    struct http_buf **p = &res;

    while (len)
    {
        int tocopy = len;
        if (tocopy > HTTP_BUF_SIZE)
            tocopy = HTTP_BUF_SIZE;
        *p = http_buf_create();
        memcpy((*p)->buf, b, tocopy);
        (*p)->len = tocopy;
        len -= tocopy;
        b += tocopy;
        p = &(*p)->next;
    }
    return res;
}

// Add a (chain of) buffers to the end of an existing queue.
static void http_buf_enqueue(struct http_buf **queue, struct http_buf *b)
{
    while (*queue)
        queue = &(*queue)->next;
    *queue = b;
}

static struct http_buf *http_buf_bywrbuf(WRBUF wrbuf)
{
    // Heavens to Betsy (buf)!
    return http_buf_bybuf(wrbuf_buf(wrbuf), wrbuf_len(wrbuf));
}

// Non-destructively collapse chain of buffers into a string (max *len)
// Return
static int http_buf_peek(struct http_buf *b, char *buf, int len)
{
    int rd = 0;
    while (b && rd < len)
    {
        int toread = len - rd;
        if (toread > b->len)
            toread = b->len;
        memcpy(buf + rd, b->buf + b->offset, toread);
        rd += toread;
        b = b->next;
    }
    buf[rd] = '\0';
    return rd;
}

// Ddestructively munch up to len  from head of queue.
static int http_buf_read(struct http_buf **b, char *buf, int len)
{
    int rd = 0;
    while ((*b) && rd < len)
    {
        int toread = len - rd;
        if (toread > (*b)->len)
            toread = (*b)->len;
        memcpy(buf + rd, (*b)->buf + (*b)->offset, toread);
        rd += toread;
        if (toread < (*b)->len)
        {
            (*b)->len -= toread;
            (*b)->offset += toread;
            break;
        }
        else
        {
            struct http_buf *n = (*b)->next;
            http_buf_destroy(*b);
            *b = n;
        }
    }
    buf[rd] = '\0';
    return rd;
}

// Buffers may overlap.
static void urldecode(char *i, char *o)
{
    while (*i)
    {
        if (*i == '+')
        {
            *(o++) = ' ';
            i++;
        }
        else if (*i == '%')
        {
            i++;
            sscanf(i, "%2hhx", o);
            i += 2;
            o++;
        }
        else
            *(o++) = *(i++);
    }
    *o = '\0';
}

// Warning: Buffers may not overlap
void urlencode(const char *i, char *o)
{
    while (*i)
    {
        if (strchr(" /:", *i))
        {
            sprintf(o, "%%%.2X", (int) *i);
            o += 3;
        }
        else
            *(o++) = *i;
        i++;
    }
    *o = '\0';
}

void http_addheader(struct http_response *r, const char *name, const char *value)
{
    struct http_channel *c = r->channel;
    struct http_header *h = nmem_malloc(c->nmem, sizeof *h);
    h->name = nmem_strdup(c->nmem, name);
    h->value = nmem_strdup(c->nmem, value);
    h->next = r->headers;
    r->headers = h;
}

char *http_argbyname(struct http_request *r, char *name)
{
    struct http_argument *p;
    if (!name)
        return 0;
    for (p = r->arguments; p; p = p->next)
        if (!strcmp(p->name, name))
            return p->value;
    return 0;
}

char *http_headerbyname(struct http_header *h, char *name)
{
    for (; h; h = h->next)
        if (!strcmp(h->name, name))
            return h->value;
    return 0;
}

struct http_response *http_create_response(struct http_channel *c)
{
    struct http_response *r = nmem_malloc(c->nmem, sizeof(*r));
    strcpy(r->code, "200");
    r->msg = "OK";
    r->channel = c;
    r->headers = 0;
    r->payload = 0;
    return r;
}

// Check if buf contains a package (minus payload)
static int package_check(const char *buf)
{
    int len = 0;
    while (*buf) // Check if we have a sequence of lines terminated by an empty line
    {
        char *b = strstr(buf, "\r\n");

        if (!b)
            return 0;

        len += (b - buf) + 2;
        if (b == buf)
            return len;
        buf = b + 2;
    }
    return 0;
}

// Check if we have a request. Return 0 or length
// (including trailing CRNL) FIXME: Does not deal gracefully with requests
// carrying payload but this is kind of OK since we will reject anything
// other than an empty GET
static int request_check(struct http_buf *queue)
{
    char tmp[4096];

    http_buf_peek(queue, tmp, 4096);
    return package_check(tmp);
}

struct http_response *http_parse_response_buf(struct http_channel *c, const char *buf, int len)
{
    char tmp[4096];
    struct http_response *r = http_create_response(c);
    char *p, *p2;
    struct http_header **hp = &r->headers;

    if (len >= 4096)
        return 0;
    memcpy(tmp, buf, len);
    for (p = tmp; *p && *p != ' '; p++) // Skip HTTP version
        ;
    p++;
    // Response code
    for (p2 = p; *p2 && *p2 != ' ' && p2 - p < 3; p2++)
        r->code[p2 - p] = *p2;
    if (!(p = strstr(tmp, "\r\n")))
        return 0;
    p += 2;
    while (*p)
    {
        if (!(p2 = strstr(p, "\r\n")))
            return 0;
        if (p == p2) // End of headers
            break;
        else
        {
            struct http_header *h = *hp = nmem_malloc(c->nmem, sizeof(*h));
            char *value = strchr(p, ':');
            if (!value)
                return 0;
            *(value++) = '\0';
            h->name = nmem_strdup(c->nmem, p);
            while (isspace(*value))
                value++;
            if (value >= p2)  // Empty header;
            {
                h->value = "";
                p = p2 + 2;
                continue;
            }
            *p2 = '\0';
            h->value = nmem_strdup(c->nmem, value);
            h->next = 0;
            hp = &h->next;
            p = p2 + 2;
        }
    }
    return r;
}

struct http_request *http_parse_request(struct http_channel *c, struct http_buf **queue,
        int len)
{
    struct http_request *r = nmem_malloc(c->nmem, sizeof(*r));
    char *p, *p2;
    char tmp[4096];
    char *buf = tmp;

    if (len > 4096)
        return 0;
    if (http_buf_read(queue, buf, len) < len)
        return 0;

    r->search = "";
    r->channel = c;
    r->arguments = 0;
    r->headers = 0;
    // Parse first line
    for (p = buf, p2 = r->method; *p && *p != ' ' && p - buf < 19; p++)
        *(p2++) = *p;
    if (*p != ' ')
    {
        yaz_log(YLOG_WARN, "Unexpected HTTP method in request");
        return 0;
    }
    *p2 = '\0';

    if (!(buf = strchr(buf, ' ')))
    {
        yaz_log(YLOG_WARN, "Syntax error in request (1)");
        return 0;
    }
    buf++;
    if (!(p = strchr(buf, ' ')))
    {
        yaz_log(YLOG_WARN, "Syntax error in request (2)");
        return 0;
    }
    *(p++) = '\0';
    if ((p2 = strchr(buf, '?'))) // Do we have arguments?
        *(p2++) = '\0';
    r->path = nmem_strdup(c->nmem, buf);
    if (p2)
    {
        r->search = nmem_strdup(c->nmem, p2);
        // Parse Arguments
        while (*p2)
        {
            struct http_argument *a;
            char *equal = strchr(p2, '=');
            char *eoa = strchr(p2, '&');
            if (!equal)
            {
                yaz_log(YLOG_WARN, "Expected '=' in argument");
                return 0;
            }
            if (!eoa)
                eoa = equal + strlen(equal); // last argument
            else
                *(eoa++) = '\0';
            a = nmem_malloc(c->nmem, sizeof(struct http_argument));
            *(equal++) = '\0';
            a->name = nmem_strdup(c->nmem, p2);
            urldecode(equal, equal);
            a->value = nmem_strdup(c->nmem, equal);
            a->next = r->arguments;
            r->arguments = a;
            p2 = eoa;
        }
    }
    buf = p;

    if (strncmp(buf, "HTTP/", 5))
        strcpy(r->http_version, "1.0");
    else
    {
        buf += 5;
        if (!(p = strstr(buf, "\r\n")))
            return 0;
        *(p++) = '\0';
        p++;
        strcpy(r->http_version, buf);
        buf = p;
    }
    strcpy(c->version, r->http_version);

    r->headers = 0;
    while (*buf)
    {
        if (!(p = strstr(buf, "\r\n")))
            return 0;
        if (p == buf)
            break;
        else
        {
            struct http_header *h = nmem_malloc(c->nmem, sizeof(*h));
            if (!(p2 = strchr(buf, ':')))
                return 0;
            *(p2++) = '\0';
            h->name = nmem_strdup(c->nmem, buf);
            while (isspace(*p2))
                p2++;
            if (p2 >= p) // Empty header?
            {
                buf = p + 2;
                continue;
            }
            *p = '\0';
            h->value = nmem_strdup(c->nmem, p2);
            h->next = r->headers;
            r->headers = h;
            buf = p + 2;
        }
    }

    return r;
}

static struct http_buf *http_serialize_response(struct http_channel *c,
        struct http_response *r)
{
    struct http_header *h;

    wrbuf_rewind(c->wrbuf);
    wrbuf_printf(c->wrbuf, "HTTP/1.1 %s %s\r\n", r->code, r->msg);
    for (h = r->headers; h; h = h->next)
        wrbuf_printf(c->wrbuf, "%s: %s\r\n", h->name, h->value);
    if (r->payload)
    {
        wrbuf_printf(c->wrbuf, "Content-length: %d\r\n", r->payload ?
                (int) strlen(r->payload) : 0);
        wrbuf_printf(c->wrbuf, "Content-type: text/xml\r\n");
    }
    wrbuf_puts(c->wrbuf, "\r\n");

    if (r->payload)
        wrbuf_puts(c->wrbuf, r->payload);

    return http_buf_bywrbuf(c->wrbuf);
}

// Serialize a HTTP request
static struct http_buf *http_serialize_request(struct http_request *r)
{
    struct http_channel *c = r->channel;
    struct http_header *h;
    struct http_argument *a;

    wrbuf_rewind(c->wrbuf);
    wrbuf_printf(c->wrbuf, "%s %s", r->method, r->path);

    if (r->arguments)
    {
        wrbuf_putc(c->wrbuf, '?');
        for (a = r->arguments; a; a = a->next) {
            if (a != r->arguments)
                wrbuf_putc(c->wrbuf, '&');
            wrbuf_printf(c->wrbuf, "%s=%s", a->name, a->value);
        }
    }

    wrbuf_printf(c->wrbuf, " HTTP/%s\r\n", r->http_version);

    for (h = r->headers; h; h = h->next)
        wrbuf_printf(c->wrbuf, "%s: %s\r\n", h->name, h->value);

    wrbuf_puts(c->wrbuf, "\r\n");
    
    return http_buf_bywrbuf(c->wrbuf);
}


static int http_weshouldproxy(struct http_request *rq)
{
    if (proxy_addr && !strstr(rq->path, "search.pz2"))
        return 1;
    return 0;
}


struct http_header * http_header_append(struct http_channel *ch, 
                                        struct http_header * hp, 
                                        const char *name, 
                                        const char *value)
{
    struct http_header *hpnew = 0; 

    if (!hp | !ch)
        return 0;

    while (hp && hp->next)
        hp = hp->next;

    if(name && strlen(name)&& value && strlen(value)){
        hpnew = nmem_malloc(ch->nmem, sizeof *hpnew);
        hpnew->name = nmem_strdup(ch->nmem, name);
        hpnew->value = nmem_strdup(ch->nmem, value);
        
        hpnew->next = 0;
        hp->next = hpnew;
        hp = hp->next;
        
        return hpnew;
    }

    return hp;
}

    

static int http_proxy(struct http_request *rq)
{
    struct http_channel *c = rq->channel;
    struct http_proxy *p = c->proxy;
    struct http_header *hp;
    struct http_buf *requestbuf;
    char server_via[128] = "";
    char server_port[16] = "";
    struct conf_server *ser = global_parameters.server;

    if (!p) // This is a new connection. Create a proxy channel
    {
        int sock;
        struct protoent *pe;
        int one = 1;
        int flags;

        if (!(pe = getprotobyname("tcp"))) {
            abort();
        }
        if ((sock = socket(PF_INET, SOCK_STREAM, pe->p_proto)) < 0)
        {
            yaz_log(YLOG_WARN|YLOG_ERRNO, "socket");
            return -1;
        }
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)
                        &one, sizeof(one)) < 0)
            abort();
        if ((flags = fcntl(sock, F_GETFL, 0)) < 0) 
            yaz_log(YLOG_FATAL|YLOG_ERRNO, "fcntl");
        if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0)
            yaz_log(YLOG_FATAL|YLOG_ERRNO, "fcntl2");
        if (connect(sock, (struct sockaddr *) proxy_addr, 
                    sizeof(*proxy_addr)) < 0)
            if (errno != EINPROGRESS)
            {
                yaz_log(YLOG_WARN|YLOG_ERRNO, "Proxy connect");
                return -1;
            }

        p = xmalloc(sizeof(struct http_proxy));
        p->oqueue = 0;
        p->channel = c;
        p->first_response = 1;
        c->proxy = p;
        // We will add EVENT_OUTPUT below
        p->iochan = iochan_create(sock, proxy_io, EVENT_INPUT);
        iochan_setdata(p->iochan, p);
        p->iochan->next = channel_list;
        channel_list = p->iochan;
    }

    // Do _not_ modify Host: header, just checking it's existence
    for (hp = rq->headers; hp; hp = hp->next)
        if (!strcmp(hp->name, "Host"))
            break;
    if (!hp)
    {
        yaz_log(YLOG_WARN, "Failed to find Host header in proxy");
        return -1;
    }
    
    // Add new header about paraz2 version, host, remote client address, etc.
    {
        hp = rq->headers;
        hp = http_header_append(c, hp, 
                                "X-Pazpar2-Version", PACKAGE_VERSION);
        hp = http_header_append(c, hp, 
                                "X-Pazpar2-Server-Host", ser->host);
        sprintf(server_port, "%d",  ser->port);
        hp = http_header_append(c, hp, 
                                "X-Pazpar2-Server-Port", server_port);
        sprintf(server_via,  "1.1 %s:%s (%s/%s)",  
                ser->host, server_port, PACKAGE_NAME, PACKAGE_VERSION);
        hp = http_header_append(c, hp, "Via" , server_via);
        hp = http_header_append(c, hp, "X-Forwarded-For", c->addr);
    }
    
    requestbuf = http_serialize_request(rq);
    http_buf_enqueue(&p->oqueue, requestbuf);
    iochan_setflag(p->iochan, EVENT_OUTPUT);
    return 0;
}

void http_send_response(struct http_channel *ch)
{
    struct http_response *rs = ch->response;
    struct http_buf *hb;

    assert(rs);
    hb = http_serialize_response(ch, rs);
    if (!hb)
    {
        yaz_log(YLOG_WARN, "Failed to serialize HTTP response");
        http_destroy(ch->iochan);
    }
    else
    {
        http_buf_enqueue(&ch->oqueue, hb);
        iochan_setflag(ch->iochan, EVENT_OUTPUT);
        ch->state = Http_Idle;
    }
}

static void http_io(IOCHAN i, int event)
{
    struct http_channel *hc = iochan_getdata(i);

    switch (event)
    {
        int res, reqlen;
        struct http_buf *htbuf;

        case EVENT_INPUT:
            htbuf = http_buf_create();
            res = read(iochan_getfd(i), htbuf->buf, HTTP_BUF_SIZE -1);
            if (res == -1 && errno == EAGAIN)
            {
                http_buf_destroy(htbuf);
                return;
            }
            if (res <= 0)
            {
                http_buf_destroy(htbuf);
                http_destroy(i);
                return;
            }
            if (res > 0)
            {
                htbuf->buf[res] = '\0';
                htbuf->len = res;
                http_buf_enqueue(&hc->iqueue, htbuf);
            }

            if (hc->state == Http_Busy)
                return;
            if ((reqlen = request_check(hc->iqueue)) <= 2)
                return;

            nmem_reset(hc->nmem);
            if (!(hc->request = http_parse_request(hc, &hc->iqueue, reqlen)))
            {
                yaz_log(YLOG_WARN, "Failed to parse request");
                http_destroy(i);
                return;
            }
            hc->response = 0;
            yaz_log(YLOG_LOG, "Request: %s %s%s%s", hc->request->method,
                    hc->request->path,
                    *hc->request->search ? "?" : "",
                    hc->request->search);
            if (http_weshouldproxy(hc->request))
                http_proxy(hc->request);
            else
            {
                // Execute our business logic!
                hc->state = Http_Busy;
                http_command(hc);
            }
            if (hc->iqueue)
            {
                yaz_log(YLOG_DEBUG, "We think we have more input to read. Forcing event");
                iochan_setevent(i, EVENT_INPUT);
            }

            break;

        case EVENT_OUTPUT:
            if (hc->oqueue)
            {
                struct http_buf *wb = hc->oqueue;
                res = write(iochan_getfd(hc->iochan), wb->buf + wb->offset, wb->len);
                if (res <= 0)
                {
                    yaz_log(YLOG_WARN|YLOG_ERRNO, "write");
                    http_destroy(i);
                    return;
                }
                if (res == wb->len)
                {
                    hc->oqueue = hc->oqueue->next;
                    http_buf_destroy(wb);
                }
                else
                {
                    wb->len -= res;
                    wb->offset += res;
                }
                if (!hc->oqueue) {
                    if (!strcmp(hc->version, "1.0"))
                    {
                        http_destroy(i);
                        return;
                    }
                    else
                    {
                        iochan_clearflag(i, EVENT_OUTPUT);
                        if (hc->iqueue)
                            iochan_setevent(hc->iochan, EVENT_INPUT);
                    }
                }
            }

            if (!hc->oqueue && hc->proxy && !hc->proxy->iochan) 
                http_destroy(i); // Server closed; we're done
            break;
        default:
            yaz_log(YLOG_WARN, "Unexpected event on connection");
            http_destroy(i);
    }
}

#ifdef GAGA
// If this hostname contains our proxy host as a prefix, replace with myurl
static char *sub_hostname(struct http_channel *c, char *buf)
{
    char tmp[1024];
    if (strlen(buf) > 1023)
        return buf;
    if (strncmp(buf, "http://", 7))
        return buf;
    if (!strncmp(buf + 7, proxy_url, strlen(proxy_url)))
    {
        strcpy(tmp, myurl);
        strcat(tmp, buf + strlen(proxy_url) + 7);
        return nmem_strdup(c->nmem, tmp);
    }
    return buf;
}
#endif

// Handles I/O on a client connection to a backend web server (proxy mode)
static void proxy_io(IOCHAN pi, int event)
{
    struct http_proxy *pc = iochan_getdata(pi);
    struct http_channel *hc = pc->channel;

    switch (event)
    {
        int res;
        struct http_buf *htbuf;

        case EVENT_INPUT:
            htbuf = http_buf_create();
            res = read(iochan_getfd(pi), htbuf->buf, HTTP_BUF_SIZE -1);
            if (res == 0 || (res < 0 && errno != EINPROGRESS))
            {
                if (hc->oqueue)
                {
                    yaz_log(YLOG_WARN, "Proxy read came up short");
                    // Close channel and alert client HTTP channel that we're gone
                    http_buf_destroy(htbuf);
                    close(iochan_getfd(pi));
                    iochan_destroy(pi);
                    pc->iochan = 0;
                }
                else
                {
                    http_destroy(hc->iochan);
                    return;
                }
            }
            else
            {
                htbuf->buf[res] = '\0';
                htbuf->offset = 0;
                htbuf->len = res;
#ifdef GAGA
                if (pc->first_response) // Check if this is a redirect
                {
                    int len;
                    if ((len = package_check(htbuf->buf)))
                    {
                        struct http_response *res = http_parse_response_buf(hc, htbuf->buf, len);
                        if (res)
                        {
                            struct http_header *h;
                            for (h = res->headers; h; h = h->next)
                                if (!strcmp(h->name, "Location"))
                                {
                                    // We found a location header. Rewrite it.
                                    struct http_buf *buf;
                                    h->value = sub_hostname(hc, h->value);
                                    buf = http_serialize_response(hc, res);
                                    yaz_log(YLOG_LOG, "Proxy rewrite");
                                    http_buf_enqueue(&hc->oqueue, buf);
                                    htbuf->offset = len;
                                    break;
                                }
                        }
                    }
                    pc->first_response = 0;
                }
#endif
                // Write any remaining payload
                if (htbuf->len - htbuf->offset > 0)
                    http_buf_enqueue(&hc->oqueue, htbuf);
            }
            iochan_setflag(hc->iochan, EVENT_OUTPUT);
            break;
        case EVENT_OUTPUT:
            if (!(htbuf = pc->oqueue))
            {
                iochan_clearflag(pi, EVENT_OUTPUT);
                return;
            }
            res = write(iochan_getfd(pi), htbuf->buf + htbuf->offset, htbuf->len);
            if (res <= 0)
            {
                yaz_log(YLOG_WARN|YLOG_ERRNO, "write");
                http_destroy(hc->iochan);
                return;
            }
            if (res == htbuf->len)
            {
                struct http_buf *np = htbuf->next;
                http_buf_destroy(htbuf);
                pc->oqueue = np;
            }
            else
            {
                htbuf->len -= res;
                htbuf->offset += res;
            }

            if (!pc->oqueue) {
                iochan_setflags(pi, EVENT_INPUT); // Turns off output flag
            }
            break;
        default:
            yaz_log(YLOG_WARN, "Unexpected event on connection");
            http_destroy(hc->iochan);
    }
}

// Cleanup channel
static void http_destroy(IOCHAN i)
{
    struct http_channel *s = iochan_getdata(i);

    if (s->proxy)
    {
        if (s->proxy->iochan)
        {
            close(iochan_getfd(s->proxy->iochan));
            iochan_destroy(s->proxy->iochan);
        }
        http_buf_destroy_queue(s->proxy->oqueue);
        xfree(s->proxy);
    }
    s->next = http_channel_freelist;
    http_channel_freelist = s;
    close(iochan_getfd(i));
    iochan_destroy(i);
}

static struct http_channel *http_create(const char *addr)
{
    struct http_channel *r = http_channel_freelist;

    if (r)
    {
        http_channel_freelist = r->next;
        nmem_reset(r->nmem);
        wrbuf_rewind(r->wrbuf);
    }
    else
    {
        r = xmalloc(sizeof(struct http_channel));
        r->nmem = nmem_create();
        r->wrbuf = wrbuf_alloc();
    }
    r->proxy = 0;
    r->iochan = 0;
    r->iqueue = r->oqueue = 0;
    r->state = Http_Idle;
    r->request = 0;
    r->response = 0;
    if (!addr)
    {
        yaz_log(YLOG_WARN, "Invalid HTTP forward address");
        exit(1);
    }
    strcpy(r->addr, addr);
    return r;
}


/* Accept a new command connection */
static void http_accept(IOCHAN i, int event)
{
    struct sockaddr_in addr;
    int fd = iochan_getfd(i);
    socklen_t len;
    int s;
    IOCHAN c;
    int flags;
    struct http_channel *ch;

    len = sizeof addr;
    if ((s = accept(fd, (struct sockaddr *) &addr, &len)) < 0)
    {
        yaz_log(YLOG_WARN|YLOG_ERRNO, "accept");
        return;
    }
    if ((flags = fcntl(s, F_GETFL, 0)) < 0) 
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "fcntl");
    if (fcntl(s, F_SETFL, flags | O_NONBLOCK) < 0)
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "fcntl2");

    yaz_log(YLOG_DEBUG, "New command connection");
    c = iochan_create(s, http_io, EVENT_INPUT | EVENT_EXCEPT);
    
    ch = http_create(inet_ntoa(addr.sin_addr));
    ch->iochan = c;
    iochan_setdata(c, ch);

    c->next = channel_list;
    channel_list = c;
}

/* Create a http-channel listener, syntax [host:]port */
void http_init(const char *addr)
{
    IOCHAN c;
    int l;
    struct protoent *p;
    struct sockaddr_in myaddr;
    int one = 1;
    const char *pp;
    int port;

    yaz_log(YLOG_LOG, "HTTP listener %s", addr);

    memset(&myaddr, 0, sizeof myaddr);
    myaddr.sin_family = AF_INET;
    pp = strchr(addr, ':');
    if (pp)
    {
        int len = pp - addr;
        char hostname[128];
        struct hostent *he;

        strncpy(hostname, addr, len);
        hostname[len] = '\0';
        if (!(he = gethostbyname(hostname)))
        {
            yaz_log(YLOG_FATAL, "Unable to resolve '%s'", hostname);
            exit(1);
        }
        
        memcpy(&myaddr.sin_addr.s_addr, he->h_addr_list[0], he->h_length);
        port = atoi(pp + 1);

        yaz_log(YLOG_LOG, "HTTP address  %s:%d", 
                "" == he->h_addr_list[0] ? he->h_addr_list[0] : "127.0.0.1" , 
                    port);

    }
    else
    {
        port = atoi(addr);
        myaddr.sin_addr.s_addr = INADDR_ANY;
    }

    myaddr.sin_port = htons(port);

    if (!(p = getprotobyname("tcp"))) {
        abort();
    }
    if ((l = socket(PF_INET, SOCK_STREAM, p->p_proto)) < 0)
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "socket");
    if (setsockopt(l, SOL_SOCKET, SO_REUSEADDR, (char*)
                    &one, sizeof(one)) < 0)
        abort();

    if (bind(l, (struct sockaddr *) &myaddr, sizeof myaddr) < 0) 
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "bind");
    if (listen(l, SOMAXCONN) < 0) 
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "listen");

    c = iochan_create(l, http_accept, EVENT_INPUT | EVENT_EXCEPT);
    c->next = channel_list;
    channel_list = c;
}

void http_set_proxyaddr(char *host, char *base_url)
{
    char *p;
    int port;
    struct hostent *he;

    strcpy(myurl, base_url);
    strcpy(proxy_url, host);
    p = strchr(host, ':');
    yaz_log(YLOG_DEBUG, "Proxying for %s", host);
    yaz_log(YLOG_LOG, "HTTP backend  %s", proxy_url);
    if (p) {
        port = atoi(p + 1);
        *p = '\0';
    }
    else
        port = 80;
    if (!(he = gethostbyname(host))) 
    {
        fprintf(stderr, "Failed to lookup '%s'\n", host);
        exit(1);
    }
    proxy_addr = xmalloc(sizeof(struct sockaddr_in));
    proxy_addr->sin_family = he->h_addrtype;
    memcpy(&proxy_addr->sin_addr.s_addr, he->h_addr_list[0], he->h_length);
    proxy_addr->sin_port = htons(port);
}

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
