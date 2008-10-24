/* This file is part of Pazpar2.
   Copyright (C) 2006-2008 Index Data

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

#include <stdio.h>
#ifdef WIN32
#include <winsock.h>
typedef int socklen_t;
#endif

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include <sys/types.h>

#include <yaz/snprintf.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#if HAVE_NETDB_H
#include <netdb.h>
#endif

#include <errno.h>
#include <assert.h>
#include <string.h>

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <yaz/yaz-util.h>
#include <yaz/comstack.h>
#include <yaz/nmem.h>

#include "util.h"
#include "eventl.h"
#include "pazpar2.h"
#include "http.h"
#include "http_command.h"

#define MAX_HTTP_HEADER 4096

static void proxy_io(IOCHAN i, int event);
static struct http_channel *http_create(const char *addr);
static void http_destroy(IOCHAN i);

// If this is set, we proxy normal HTTP requests
static struct sockaddr_in *proxy_addr = 0; 
static char proxy_url[256] = "";
static char myurl[256] = "";
static struct http_buf *http_buf_freelist = 0;
static struct http_channel *http_channel_freelist = 0;

struct http_channel_observer_s {
    void *data;
    void *data2;
    http_channel_destroy_t destroy;
    struct http_channel_observer_s *next;
    struct http_channel *chan;
};


static const char *http_lookup_header(struct http_header *header,
                                      const char *name)
{
    for (; header; header = header->next)
        if (!strcasecmp(name, header->name))
            return header->value;
    return 0;
}

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
static void http_buf_peek(struct http_buf *b, char *buf, int len)
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
}

static int http_buf_size(struct http_buf *b)
{
    int sz = 0;
    for (; b; b = b->next)
        sz += b->len;
    return sz;
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
        else if (*i == '%' && i[1] && i[2])
        {
            int v;
            i++;
            sscanf(i, "%2x", &v);
            *o++ = v;
            i += 2;
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
    r->content_type = "text/xml";
    return r;
}


static const char *next_crlf(const char *cp, size_t *skipped)
{
    const char *next_cp = strchr(cp, '\n');
    if (next_cp)
    {
        if (next_cp > cp && next_cp[-1] == '\r')
            *skipped = next_cp - cp - 1;
        else
            *skipped = next_cp - cp;
        next_cp++;
    }
    return next_cp;
}

// Check if buf contains a package (minus payload)
static int package_check(const char *buf, int sz)
{
    int content_len = 0;
    int len = 0;

    while (*buf)
    {
        size_t skipped = 0;
        const char *b = next_crlf(buf, &skipped);

        if (!b)
        {
            // we did not find CRLF.. See if buffer is too large..
            if (sz >= MAX_HTTP_HEADER-1)
                return MAX_HTTP_HEADER-1; // yes. Return that (will fail later)
            break;
        }
        len += (b - buf);
        if (skipped == 0)
        {
            // CRLF CRLF , i.e. end of header
            if (len + content_len <= sz)
                return len + content_len;
            break;
        }
        buf = b;
        // following first skip of \r\n so that we don't consider Method
        if (!strncasecmp(buf, "Content-Length:", 15))
        {
            const char *cp = buf+15;
            while (*cp == ' ')
                cp++;
            content_len = 0;
            while (*cp && isdigit(*(const unsigned char *)cp))
                content_len = content_len*10 + (*cp++ - '0');
            if (content_len < 0) /* prevent negative offsets */
                content_len = 0;
        }
    }
    return 0;     // incomplete request
}

// Check if we have a request. Return 0 or length
static int request_check(struct http_buf *queue)
{
    char tmp[MAX_HTTP_HEADER];

    // only peek at the header..
    http_buf_peek(queue, tmp, MAX_HTTP_HEADER-1);
    // still we only return non-zero if the complete request is received..
    return package_check(tmp, http_buf_size(queue));
}

struct http_response *http_parse_response_buf(struct http_channel *c, const char *buf, int len)
{
    char tmp[MAX_HTTP_HEADER];
    struct http_response *r = http_create_response(c);
    char *p, *p2;
    struct http_header **hp = &r->headers;

    if (len >= MAX_HTTP_HEADER)
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
            while (isspace(*(const unsigned char *) value))
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

static int http_parse_arguments(struct http_request *r, NMEM nmem,
                                const char *args)
{
    const char *p2 = args;

    while (*p2)
    {
        struct http_argument *a;
        const char *equal = strchr(p2, '=');
        const char *eoa = strchr(p2, '&');
        if (!equal)
        {
            yaz_log(YLOG_WARN, "Expected '=' in argument");
            return -1;
        }
        if (!eoa)
            eoa = equal + strlen(equal); // last argument
        else if (equal > eoa)
        {
            yaz_log(YLOG_WARN, "Missing '&' in argument");
            return -1;
        }
        a = nmem_malloc(nmem, sizeof(struct http_argument));
        a->name = nmem_strdupn(nmem, p2, equal - p2);
        a->value = nmem_strdupn(nmem, equal+1, eoa - equal - 1);
        urldecode(a->name, a->name);
        urldecode(a->value, a->value);
        a->next = r->arguments;
        r->arguments = a;
        p2 = eoa;
        while (*p2 == '&')
            p2++;
    }
    return 0;
}

struct http_request *http_parse_request(struct http_channel *c,
                                        struct http_buf **queue,
                                        int len)
{
    struct http_request *r = nmem_malloc(c->nmem, sizeof(*r));
    char *p, *p2;
    char *start = nmem_malloc(c->nmem, len+1);
    char *buf = start;

    if (http_buf_read(queue, buf, len) < len)
    {
        yaz_log(YLOG_WARN, "http_buf_read < len (%d)", len);
        return 0;
    }
    r->search = "";
    r->channel = c;
    r->arguments = 0;
    r->headers = 0;
    r->content_buf = 0;
    r->content_len = 0;
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
        yaz_log(YLOG_WARN, "Missing Request-URI in HTTP request");
        return 0;
    }
    buf++;
    if (!(p = strchr(buf, ' ')))
    {
        yaz_log(YLOG_WARN, "HTTP Request-URI not terminated (too long?)");
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
        http_parse_arguments(r, c->nmem, p2);
    }
    buf = p;

    if (strncmp(buf, "HTTP/", 5))
        strcpy(r->http_version, "1.0");
    else
    {
        size_t skipped;
        buf += 5; // strlen("HTTP/")

        p = (char*) next_crlf(buf, &skipped);
        if (!p || skipped < 3 || skipped > 5)
            return 0;

        memcpy(r->http_version, buf, skipped);
        r->http_version[skipped] = '\0';
        buf = p;
    }
    strcpy(c->version, r->http_version);

    r->headers = 0;
    while (*buf)
    {
        size_t skipped;

        p = (char *) next_crlf(buf, &skipped);
        if (!p)
        {
            return 0;
        }
        else if (skipped == 0)
        {
            buf = p;
            break;
        }
        else
        {
            char *cp;
            char *n_v = nmem_malloc(c->nmem, skipped+1);
            struct http_header *h = nmem_malloc(c->nmem, sizeof(*h));

            memcpy(n_v, buf, skipped);
            n_v[skipped] = '\0';

            if (!(cp = strchr(n_v, ':')))
                return 0;
            h->name = nmem_strdupn(c->nmem, n_v, cp - n_v);
            cp++;
            while (isspace(*cp))
                cp++;
            h->value = nmem_strdup(c->nmem, cp);
            h->next = r->headers;
            r->headers = h;
            buf = p;
        }
    }

    // determine if we do keep alive
    if (!strcmp(c->version, "1.0"))
    {
        const char *v = http_lookup_header(r->headers, "Connection");
        if (v && !strcmp(v, "Keep-Alive"))
            c->keep_alive = 1;
        else
            c->keep_alive = 0;
    }
    else
    {
        const char *v = http_lookup_header(r->headers, "Connection");
        if (v && !strcmp(v, "close"))
            c->keep_alive = 0;
        else
            c->keep_alive = 1;
    }
    if (buf < start + len)
    {
        const char *content_type = http_lookup_header(r->headers,
                                                      "Content-Type");
        r->content_len = start + len - buf;
        r->content_buf = buf;

        if (!yaz_strcmp_del("application/x-www-form-urlencoded",
                            content_type, "; "))
        {
            http_parse_arguments(r, c->nmem, r->content_buf);
        }
    }
    return r;
}

static struct http_buf *http_serialize_response(struct http_channel *c,
        struct http_response *r)
{
    struct http_header *h;

    wrbuf_rewind(c->wrbuf);
    wrbuf_printf(c->wrbuf, "HTTP/%s %s %s\r\n", c->version, r->code, r->msg);
    for (h = r->headers; h; h = h->next)
        wrbuf_printf(c->wrbuf, "%s: %s\r\n", h->name, h->value);
    if (r->payload)
    {
        wrbuf_printf(c->wrbuf, "Content-Length: %d\r\n", r->payload ?
                (int) strlen(r->payload) : 0);
        wrbuf_printf(c->wrbuf, "Content-Type: %s\r\n", r->content_type);
        if (!strcmp(r->content_type, "text/xml"))
        {
            xmlDoc *doc = xmlParseMemory(r->payload, strlen(r->payload));
            if (doc)
            {
                xmlFreeDoc(doc);
            }
            else
            {
                yaz_log(YLOG_WARN, "Sending non-wellformed "
                        "response (bug #1162");
                yaz_log(YLOG_WARN, "payload: %s", r->payload);
            }
        }
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

    wrbuf_rewind(c->wrbuf);
    wrbuf_printf(c->wrbuf, "%s %s%s%s", r->method, r->path,
                 *r->search ? "?" : "", r->search);

    wrbuf_printf(c->wrbuf, " HTTP/%s\r\n", r->http_version);

    for (h = r->headers; h; h = h->next)
        wrbuf_printf(c->wrbuf, "%s: %s\r\n", h->name, h->value);

    wrbuf_puts(c->wrbuf, "\r\n");

    if (r->content_buf)
        wrbuf_write(c->wrbuf, r->content_buf, r->content_len);

#if 0
    yaz_log(YLOG_LOG, "WRITING TO PROXY:\n%s\n----",
            wrbuf_cstr(c->wrbuf));
#endif
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

   
static int is_inprogress(void)
{
#ifdef WIN32
    if (WSAGetLastError() == WSAEWOULDBLOCK)
        return 1;
#else
    if (errno == EINPROGRESS)
        return 1;
#endif
    return 0;
} 

static void enable_nonblock(int sock)
{
    int flags;
#ifdef WIN32
    flags = (flags & CS_FLAGS_BLOCKING) ? 0 : 1;
    if (ioctlsocket(sock, FIONBIO, &flags) < 0)
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "ioctlsocket");
#else
    if ((flags = fcntl(sock, F_GETFL, 0)) < 0) 
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "fcntl");
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0)
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "fcntl2");
#endif
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
        enable_nonblock(sock);
        if (connect(sock, (struct sockaddr *) proxy_addr, 
                    sizeof(*proxy_addr)) < 0)
        {
            if (!is_inprogress()) 
            {
                yaz_log(YLOG_WARN|YLOG_ERRNO, "Proxy connect");
                return -1;
            }
        }
        p = xmalloc(sizeof(struct http_proxy));
        p->oqueue = 0;
        p->channel = c;
        p->first_response = 1;
        c->proxy = p;
        // We will add EVENT_OUTPUT below
        p->iochan = iochan_create(sock, proxy_io, EVENT_INPUT);
        iochan_setdata(p->iochan, p);
        pazpar2_add_channel(p->iochan);
    }

    // Do _not_ modify Host: header, just checking it's existence

    if (!http_lookup_header(rq->headers, "Host"))
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

static void http_error(struct http_channel *hc, int no, const char *msg)
{
    struct http_response *rs = http_create_response(hc);

    hc->response = rs;
    hc->keep_alive = 0;  // not keeping this HTTP session alive

    sprintf(rs->code, "%d", no);

    rs->msg = nmem_strdup(hc->nmem, msg);
    rs->payload = nmem_malloc(hc->nmem, 100);
    yaz_snprintf(rs->payload, 99, "<error>HTTP Error %d: %s</error>\n",
                 no, msg);
    http_send_response(hc);
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
            res = recv(iochan_getfd(i), htbuf->buf, HTTP_BUF_SIZE -1, 0);
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
            htbuf->buf[res] = '\0';
            htbuf->len = res;
            http_buf_enqueue(&hc->iqueue, htbuf);

            while (1)
            {
                if (hc->state == Http_Busy)
                    return;
                reqlen = request_check(hc->iqueue);
                if (reqlen <= 2)
                    return;
                // we have a complete HTTP request
                nmem_reset(hc->nmem);
                if (!(hc->request = http_parse_request(hc, &hc->iqueue, reqlen)))
                {
                    yaz_log(YLOG_WARN, "Failed to parse request");
                    http_error(hc, 400, "Bad Request");
                    return;
                }
                hc->response = 0;
                yaz_log(YLOG_LOG, "Request: %s %s%s%s", hc->request->method,
                        hc->request->path,
                        *hc->request->search ? "?" : "",
                        hc->request->search);
                if (hc->request->content_buf)
                    yaz_log(YLOG_LOG, "%s", hc->request->content_buf);
                if (http_weshouldproxy(hc->request))
                    http_proxy(hc->request);
                else
                {
                    // Execute our business logic!
                    hc->state = Http_Busy;
                    http_command(hc);
                }
            }
            break;
        case EVENT_OUTPUT:
            if (hc->oqueue)
            {
                struct http_buf *wb = hc->oqueue;
                res = send(iochan_getfd(hc->iochan), wb->buf + wb->offset, wb->len, 0);
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
                    if (!hc->keep_alive)
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
            res = recv(iochan_getfd(pi), htbuf->buf, HTTP_BUF_SIZE -1, 0);
            if (res == 0 || (res < 0 && !is_inprogress()))
            {
                if (hc->oqueue)
                {
                    yaz_log(YLOG_WARN, "Proxy read came up short");
                    // Close channel and alert client HTTP channel that we're gone
                    http_buf_destroy(htbuf);
#ifdef WIN32
                    closesocket(iochan_getfd(pi));
#else
                    close(iochan_getfd(pi));
#endif
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
            res = send(iochan_getfd(pi), htbuf->buf + htbuf->offset, htbuf->len, 0);
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

static void http_fire_observers(struct http_channel *c);
static void http_destroy_observers(struct http_channel *c);

// Cleanup channel
static void http_destroy(IOCHAN i)
{
    struct http_channel *s = iochan_getdata(i);

    if (s->proxy)
    {
        if (s->proxy->iochan)
        {
#ifdef WIN32
            closesocket(iochan_getfd(s->proxy->iochan));
#else
            close(iochan_getfd(s->proxy->iochan));
#endif
            iochan_destroy(s->proxy->iochan);
        }
        http_buf_destroy_queue(s->proxy->oqueue);
        xfree(s->proxy);
    }
    http_buf_destroy_queue(s->iqueue);
    http_buf_destroy_queue(s->oqueue);
    http_fire_observers(s);
    http_destroy_observers(s);
    s->next = http_channel_freelist;
    http_channel_freelist = s;
#ifdef WIN32
    closesocket(iochan_getfd(i));
#else
    close(iochan_getfd(i));
#endif
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
    r->keep_alive = 0;
    r->request = 0;
    r->response = 0;
    if (!addr)
    {
        yaz_log(YLOG_WARN, "Invalid HTTP forward address");
        exit(1);
    }
    strcpy(r->addr, addr);
    r->observers = 0;
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
    struct http_channel *ch;

    len = sizeof addr;
    if ((s = accept(fd, (struct sockaddr *) &addr, &len)) < 0)
    {
        yaz_log(YLOG_WARN|YLOG_ERRNO, "accept");
        return;
    }
    enable_nonblock(s);

    yaz_log(YLOG_DEBUG, "New command connection");
    c = iochan_create(s, http_io, EVENT_INPUT | EVENT_EXCEPT);
    
    ch = http_create(inet_ntoa(addr.sin_addr));
    ch->iochan = c;
    iochan_setdata(c, ch);

    pazpar2_add_channel(c);
}

static int listener_socket = 0;

/* Create a http-channel listener, syntax [host:]port */
int http_init(const char *addr)
{
    IOCHAN c;
    int l;
    struct protoent *p;
    struct sockaddr_in myaddr;
    int one = 1;
    const char *pp;
    short port;

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
        if (!(he = gethostbyname(hostname))){
            yaz_log(YLOG_FATAL, "Unable to resolve '%s'", hostname);
            return 1;
        }
        
        memcpy(&myaddr.sin_addr.s_addr, he->h_addr_list[0], he->h_length);
        port = atoi(pp + 1);
    }
    else
    {
        port = atoi(addr);
        myaddr.sin_addr.s_addr = INADDR_ANY;
    }

    myaddr.sin_port = htons(port);

    if (!(p = getprotobyname("tcp"))) {
        return 1;
    }
    if ((l = socket(PF_INET, SOCK_STREAM, p->p_proto)) < 0)
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "socket");
    if (setsockopt(l, SOL_SOCKET, SO_REUSEADDR, (char*)
                    &one, sizeof(one)) < 0)
        return 1;

    if (bind(l, (struct sockaddr *) &myaddr, sizeof myaddr) < 0) 
    {
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "bind");
        return 1;
    }
    if (listen(l, SOMAXCONN) < 0) 
    {
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "listen");
        return 1;
    }

    listener_socket = l;

    c = iochan_create(l, http_accept, EVENT_INPUT | EVENT_EXCEPT);
    pazpar2_add_channel(c);
    return 0;
}

void http_close_server(void)
{
    /* break the event_loop (select) by closing down the HTTP listener sock */
    if (listener_socket)
    {
#ifdef WIN32
        closesocket(listener_socket);
#else
        close(listener_socket);
#endif
    }
}

void http_set_proxyaddr(char *host, char *base_url)
{
    char *p;
    short port;
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

static void http_fire_observers(struct http_channel *c)
{
    http_channel_observer_t p = c->observers;
    while (p)
    {
        p->destroy(p->data, c, p->data2);
        p = p->next;
    }
}

static void http_destroy_observers(struct http_channel *c)
{
    while (c->observers)
    {
        http_channel_observer_t obs = c->observers;
        c->observers = obs->next;
        xfree(obs);
    }
}

http_channel_observer_t http_add_observer(struct http_channel *c, void *data,
                                          http_channel_destroy_t des)
{
    http_channel_observer_t obs = xmalloc(sizeof(*obs));
    obs->chan = c;
    obs->data = data;
    obs->data2 = 0;
    obs->destroy= des;
    obs->next = c->observers;
    c->observers = obs;
    return obs;
}

void http_remove_observer(http_channel_observer_t obs)
{
    struct http_channel *c = obs->chan;
    http_channel_observer_t found, *p = &c->observers;
    while (*p != obs)
        p = &(*p)->next;
    found = *p;
    assert(found);
    *p = (*p)->next;
    xfree(found);
}

struct http_channel *http_channel_observer_chan(http_channel_observer_t obs)
{
    return obs->chan;
}

void http_observer_set_data2(http_channel_observer_t obs, void *data2)
{
    obs->data2 = data2;
}


/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
