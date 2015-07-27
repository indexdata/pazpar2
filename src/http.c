/* This file is part of Pazpar2.
   Copyright (C) Index Data

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

#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <stdio.h>
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
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

#include <yaz/yaz-util.h>
#include <yaz/comstack.h>
#include <yaz/nmem.h>
#include <yaz/mutex.h>

#include "ppmutex.h"
#include "session.h"
#include "http.h"
#include "parameters.h"

#define MAX_HTTP_HEADER 4096

#ifdef WIN32
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

struct http_buf
{
#define HTTP_BUF_SIZE 4096
    char buf[4096];
    int offset;
    int len;
    struct http_buf *next;
};


static void proxy_io(IOCHAN i, int event);
static struct http_channel *http_channel_create(http_server_t http_server,
                                                const char *addr,
                                                struct conf_server *server);
static void http_channel_destroy(IOCHAN i);
static http_server_t http_server_create(void);
static void http_server_incref(http_server_t hs);

#ifdef WIN32
#define CLOSESOCKET(x) closesocket(x)
#else
#define CLOSESOCKET(x) close(x)
#endif

struct http_server
{
    YAZ_MUTEX mutex;
    int listener_socket;
    int ref_count;
    http_sessions_t http_sessions;
    struct sockaddr_in *proxy_addr;
    FILE *record_file;
};

struct http_channel_observer_s {
    void *data;
    void *data2;
    http_channel_destroy_t destroy;
    struct http_channel_observer_s *next;
    struct http_channel *chan;
};


const char *http_lookup_header(struct http_header *header,
                               const char *name)
{
    for (; header; header = header->next)
        if (!strcasecmp(name, header->name))
            return header->value;
    return 0;
}

static struct http_buf *http_buf_create(http_server_t hs)
{
    struct http_buf *r = xmalloc(sizeof(*r));
    r->offset = 0;
    r->len = 0;
    r->next = 0;
    return r;
}

static void http_buf_destroy(http_server_t hs, struct http_buf *b)
{
    xfree(b);
}

static void http_buf_destroy_queue(http_server_t hs, struct http_buf *b)
{
    struct http_buf *p;
    while (b)
    {
        p = b->next;
        http_buf_destroy(hs, b);
        b = p;
    }
}

static struct http_buf *http_buf_bybuf(http_server_t hs, char *b, int len)
{
    struct http_buf *res = 0;
    struct http_buf **p = &res;

    while (len)
    {
        int tocopy = len;
        if (tocopy > HTTP_BUF_SIZE)
            tocopy = HTTP_BUF_SIZE;
        *p = http_buf_create(hs);
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

static struct http_buf *http_buf_bywrbuf(http_server_t hs, WRBUF wrbuf)
{
    // Heavens to Betsy (buf)!
    return http_buf_bybuf(hs, wrbuf_buf(wrbuf), wrbuf_len(wrbuf));
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
static int http_buf_read(http_server_t hs,
                         struct http_buf **b, char *buf, int len)
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
            http_buf_destroy(hs, *b);
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

const char *http_argbyname(struct http_request *r, const char *name)
{
    struct http_argument *p;
    if (!name)
        return 0;
    for (p = r->arguments; p; p = p->next)
        if (!strcmp(p->name, name))
            return p->value;
    return 0;
}

const char *http_headerbyname(struct http_header *h, const char *name)
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

    if (http_buf_read(c->http_server, queue, buf, len) < len)
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

    if (global_parameters.dump_records > 1)
    {
        FILE *lf = yaz_log_file();
        yaz_log(YLOG_LOG, "Response:");
        fwrite(wrbuf_buf(c->wrbuf), 1, wrbuf_len(c->wrbuf), lf);
    }
    return http_buf_bywrbuf(c->http_server, c->wrbuf);
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
    return http_buf_bywrbuf(c->http_server, c->wrbuf);
}


static int http_weshouldproxy(struct http_request *rq)
{
    struct http_channel *c = rq->channel;
    if (c->server->http_server->proxy_addr && !strstr(rq->path, "search.pz2"))
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
    struct conf_server *ser = c->server;

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
        if (connect(sock, (struct sockaddr *)
                    c->server->http_server->proxy_addr,
                    sizeof(*c->server->http_server->proxy_addr)) < 0)
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
        p->iochan = iochan_create(sock, proxy_io, EVENT_INPUT, "http_proxy");
        iochan_setdata(p->iochan, p);

        iochan_add(ser->iochan_man, p->iochan);
    }

    // Do _not_ modify Host: header, just checking it's existence

    if (!http_lookup_header(rq->headers, "Host"))
    {
        yaz_log(YLOG_WARN, "Failed to find Host header in proxy");
        return -1;
    }

    // Add new header about paraz2 version, host, remote client address, etc.
    {
        char server_via[128];

        hp = rq->headers;
        hp = http_header_append(c, hp,
                                "X-Pazpar2-Version", PACKAGE_VERSION);
        hp = http_header_append(c, hp,
                                "X-Pazpar2-Server-Host", ser->host);
        hp = http_header_append(c, hp,
                                "X-Pazpar2-Server-Port", ser->port);
        yaz_snprintf(server_via, sizeof(server_via),
                     "1.1 %s:%s (%s/%s)",
                     ser->host, ser->port,
                     PACKAGE_NAME, PACKAGE_VERSION);
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

    yaz_timing_stop(ch->yt);
    if (ch->request)
    {
        yaz_log(YLOG_LOG, "Response: %6.5f %d %s%s%s ",
                yaz_timing_get_real(ch->yt),
                iochan_getfd(ch->iochan),
                ch->request->path,
                *ch->request->search ? "?" : "",
                ch->request->search);
    }
    assert(rs);
    hb = http_serialize_response(ch, rs);
    if (!hb)
    {
        yaz_log(YLOG_WARN, "Failed to serialize HTTP response");
        http_channel_destroy(ch->iochan);
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
    while (event)
    {
        if (event == EVENT_INPUT)
        {
            int res, reqlen;
            struct http_buf *htbuf;

            htbuf = http_buf_create(hc->http_server);
            res = recv(iochan_getfd(i), htbuf->buf, HTTP_BUF_SIZE -1, 0);
            if (res == -1 && errno == EAGAIN)
            {
                http_buf_destroy(hc->http_server, htbuf);
                return;
            }
            if (res <= 0)
            {
#if HAVE_SYS_TIME_H
                if (hc->http_server->record_file)
                {
                    struct timeval tv;
                    gettimeofday(&tv, 0);
                    fprintf(hc->http_server->record_file, "r %lld %lld %lld 0\n",
                            (long long) tv.tv_sec, (long long) tv.tv_usec,
                            (long long) iochan_getfd(i));
                }
#endif
                http_buf_destroy(hc->http_server, htbuf);
                fflush(hc->http_server->record_file);
                http_channel_destroy(i);
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
#if HAVE_SYS_TIME_H
                if (hc->http_server->record_file)
                {
                    struct timeval tv;
                    int sz = 0;
                    struct http_buf *hb;
                    for (hb = hc->iqueue; hb; hb = hb->next)
                        sz += hb->len;
                    gettimeofday(&tv, 0);
                    fprintf(hc->http_server->record_file, "r %lld %lld %lld %d\n",
                            (long long) tv.tv_sec, (long long) tv.tv_usec,
                            (long long) iochan_getfd(i), sz);
                    for (hb = hc->iqueue; hb; hb = hb->next)
                        fwrite(hb->buf, 1, hb->len, hc->http_server->record_file);
                    fflush(hc->http_server->record_file);
                }
 #endif
                yaz_timing_start(hc->yt);
                if (!(hc->request = http_parse_request(hc, &hc->iqueue, reqlen)))
                {
                    yaz_log(YLOG_WARN, "Failed to parse request");
                    http_error(hc, 400, "Bad Request");
                    return;
                }
                hc->response = 0;
                yaz_log(YLOG_LOG, "Request: - %d %s %s%s%s",
                        iochan_getfd(i),
                        hc->request->method,
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
        }
        else if (event == EVENT_OUTPUT)
        {
            event = 0;
            if (hc->oqueue)
            {
                struct http_buf *wb = hc->oqueue;
                int res;
                res = send(iochan_getfd(hc->iochan),
                           wb->buf + wb->offset, wb->len, 0);
                if (res <= 0)
                {
                    yaz_log(YLOG_WARN|YLOG_ERRNO, "write");
                    http_channel_destroy(i);
                    return;
                }
                if (res == wb->len)
                {
#if HAVE_SYS_TIME_H
                    if (hc->http_server->record_file)
                    {
                        struct timeval tv;
                        int sz = wb->offset + wb->len;
                        gettimeofday(&tv, 0);
                        fprintf(hc->http_server->record_file, "w %lld %lld %lld %d\n",
                                (long long) tv.tv_sec, (long long) tv.tv_usec,
                                (long long) iochan_getfd(i), sz);
                        fwrite(wb->buf, 1, wb->offset + wb->len,
                               hc->http_server->record_file);
                        fputc('\n', hc->http_server->record_file);
                        fflush(hc->http_server->record_file);
                    }
 #endif
                    hc->oqueue = hc->oqueue->next;
                    http_buf_destroy(hc->http_server, wb);
                }
                else
                {
                    wb->len -= res;
                    wb->offset += res;
                }
                if (!hc->oqueue)
                {
                    if (!hc->keep_alive)
                    {
                        http_channel_destroy(i);
                        return;
                    }
                    else
                    {
                        iochan_clearflag(i, EVENT_OUTPUT);
                        if (hc->iqueue)
                            event = EVENT_INPUT;
                    }
                }
            }
            if (!hc->oqueue && hc->proxy && !hc->proxy->iochan)
                http_channel_destroy(i); // Server closed; we're done
        }
        else
        {
            yaz_log(YLOG_WARN, "Unexpected event on connection");
            http_channel_destroy(i);
            event = 0;
        }
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
            htbuf = http_buf_create(hc->http_server);
            res = recv(iochan_getfd(pi), htbuf->buf, HTTP_BUF_SIZE -1, 0);
            if (res == 0 || (res < 0 && !is_inprogress()))
            {
                if (hc->oqueue)
                {
                    yaz_log(YLOG_WARN, "Proxy read came up short");
                    // Close channel and alert client HTTP channel that we're gone
                    http_buf_destroy(hc->http_server, htbuf);
                    CLOSESOCKET(iochan_getfd(pi));
                    iochan_destroy(pi);
                    pc->iochan = 0;
                }
                else
                {
                    http_channel_destroy(hc->iochan);
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
                http_channel_destroy(hc->iochan);
                return;
            }
            if (res == htbuf->len)
            {
                struct http_buf *np = htbuf->next;
                http_buf_destroy(hc->http_server, htbuf);
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
            http_channel_destroy(hc->iochan);
            break;
    }
}

static void http_fire_observers(struct http_channel *c);
static void http_destroy_observers(struct http_channel *c);

// Cleanup channel
static void http_channel_destroy(IOCHAN i)
{
    struct http_channel *s = iochan_getdata(i);
    http_server_t http_server;

    if (s->proxy)
    {
        if (s->proxy->iochan)
        {
            CLOSESOCKET(iochan_getfd(s->proxy->iochan));
            iochan_destroy(s->proxy->iochan);
        }
        http_buf_destroy_queue(s->http_server, s->proxy->oqueue);
        xfree(s->proxy);
    }
    yaz_timing_destroy(&s->yt);
    http_buf_destroy_queue(s->http_server, s->iqueue);
    http_buf_destroy_queue(s->http_server, s->oqueue);
    http_fire_observers(s);
    http_destroy_observers(s);

    http_server = s->http_server; /* save it for destroy (decref) */

    http_server_destroy(http_server);

    CLOSESOCKET(iochan_getfd(i));

    iochan_destroy(i);
    nmem_destroy(s->nmem);
    wrbuf_destroy(s->wrbuf);
    xfree(s);
}

static struct http_channel *http_channel_create(http_server_t hs,
                                                const char *addr,
                                                struct conf_server *server)
{
    struct http_channel *r;

    r = xmalloc(sizeof(struct http_channel));
    r->nmem = nmem_create();
    r->wrbuf = wrbuf_alloc();

    http_server_incref(hs);
    r->http_server = hs;
    r->http_sessions = hs->http_sessions;
    assert(r->http_sessions);
    r->server = server;
    r->proxy = 0;
    r->iochan = 0;
    r->iqueue = r->oqueue = 0;
    r->state = Http_Idle;
    r->keep_alive = 0;
    r->request = 0;
    r->response = 0;
    strcpy(r->version, "1.0");
    if (!addr)
    {
        yaz_log(YLOG_WARN, "Invalid HTTP forward address");
        exit(1);
    }
    strcpy(r->addr, addr);
    r->observers = 0;
    r->yt = yaz_timing_create();
    return r;
}


/* Accept a new command connection */
static void http_accept(IOCHAN i, int event)
{
    char host[256];
    struct sockaddr_storage addr;
    int fd = iochan_getfd(i);
    socklen_t len = sizeof addr;
    int s;
    IOCHAN c;
    struct http_channel *ch;
    struct conf_server *server = iochan_getdata(i);

    if ((s = accept(fd, (struct sockaddr *) &addr, &len)) < 0)
    {
        yaz_log(YLOG_WARN|YLOG_ERRNO, "accept");
        return;
    }
    if (getnameinfo((struct sockaddr *) &addr, len, host, sizeof(host)-1, 0, 0,
        NI_NUMERICHOST))
    {
        yaz_log(YLOG_WARN|YLOG_ERRNO, "getnameinfo");
        CLOSESOCKET(s);
        return;
    }
    enable_nonblock(s);

    yaz_log(YLOG_DEBUG, "New command connection");
    c = iochan_create(s, http_io, EVENT_INPUT | EVENT_EXCEPT,
                      "http_session_socket");


    ch = http_channel_create(server->http_server, host, server);
    ch->iochan = c;
    iochan_setdata(c, ch);
    iochan_add(server->iochan_man, c);
}

/* Create a http-channel listener, syntax [host:]port */
int http_init(struct conf_server *server, const char *record_fname)
{
    IOCHAN c;
    int s = -1;
    int one = 1;
    FILE *record_file = 0;
    struct addrinfo hints, *af = 0, *ai;
    int error;
    int ipv6_only = -1;

    yaz_log(YLOG_LOG, "HTTP listener %s:%s", server->host, server->port);

    hints.ai_flags = 0;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_addrlen        = 0;
    hints.ai_addr           = NULL;
    hints.ai_canonname      = NULL;
    hints.ai_next           = NULL;

    if (!strcmp(server->host, "@"))
    {
        ipv6_only = 0;
        hints.ai_flags = AI_PASSIVE;
        error = getaddrinfo(0, server->port, &hints, &af);
    }
    else
        error = getaddrinfo(server->host, server->port, &hints, &af);

    if (error)
    {
        yaz_log(YLOG_FATAL, "Failed to resolve %s: %s", server->host,
                gai_strerror(error));
        return 1;
    }
    for (ai = af; ai; ai = ai->ai_next)
    {
        if (ai->ai_family == AF_INET6)
        {
            s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
            if (s != -1)
                break;
        }
    }
    if (s == -1)
    {
        for (ai = af; ai; ai = ai->ai_next)
        {
            s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
            if (s != -1)
                break;
        }
    }
    if (s == -1)
    {
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "socket");
        freeaddrinfo(af);
        return 1;
    }
    if (ipv6_only >= 0 && ai->ai_family == AF_INET6 &&
        setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6_only, sizeof(ipv6_only)))
    {
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "setsockopt IPV6_V6ONLY %s:%s %d",
                server->host, server->port, ipv6_only);
        freeaddrinfo(af);
        CLOSESOCKET(s);
        return 1;
    }
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)))
    {
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "setsockopt SO_REUSEADDR %s:%s",
                server->host, server->port);
        freeaddrinfo(af);
        CLOSESOCKET(s);
        return 1;
    }
    if (bind(s, ai->ai_addr, ai->ai_addrlen) < 0)
    {
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "bind %s:%s",
                server->host, server->port);
        freeaddrinfo(af);
        CLOSESOCKET(s);
        return 1;
    }
    freeaddrinfo(af);
    if (listen(s, SOMAXCONN) < 0)
    {
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "listen %s:%s",
                server->host, server->port);
        CLOSESOCKET(s);
        return 1;
    }

    if (record_fname)
    {
        record_file = fopen(record_fname, "wb");
        if (!record_file)
        {
            yaz_log(YLOG_FATAL|YLOG_ERRNO, "fopen %s", record_fname);
            CLOSESOCKET(s);
            return 1;
        }
    }
    server->http_server = http_server_create();

    server->http_server->record_file = record_file;
    server->http_server->listener_socket = s;

    c = iochan_create(s, http_accept, EVENT_INPUT | EVENT_EXCEPT, "http_server");
    iochan_setdata(c, server);

    iochan_add(server->iochan_man, c);
    return 0;
}

void http_close_server(struct conf_server *server)
{
    /* break the event_loop (select) by closing down the HTTP listener sock */
    if (server->http_server->listener_socket)
    {
#ifdef WIN32
        closesocket(server->http_server->listener_socket);
#else
        close(server->http_server->listener_socket);
#endif
    }
}

void http_set_proxyaddr(const char *host, struct conf_server *server)
{
    const char *p;
    short port;
    struct hostent *he;
    WRBUF w = wrbuf_alloc();

    yaz_log(YLOG_LOG, "HTTP backend  %s", host);

    p = strchr(host, ':');
    if (p)
    {
        port = atoi(p + 1);
        wrbuf_write(w, host, p - host);
        wrbuf_puts(w, "");
    }
    else
    {
        port = 80;
        wrbuf_puts(w, host);
    }
    if (!(he = gethostbyname(wrbuf_cstr(w))))
    {
        fprintf(stderr, "Failed to lookup '%s'\n", wrbuf_cstr(w));
        exit(1);
    }
    wrbuf_destroy(w);

    server->http_server->proxy_addr = xmalloc(sizeof(struct sockaddr_in));
    server->http_server->proxy_addr->sin_family = he->h_addrtype;
    memcpy(&server->http_server->proxy_addr->sin_addr.s_addr,
           he->h_addr_list[0], he->h_length);
    server->http_server->proxy_addr->sin_port = htons(port);
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

http_server_t http_server_create(void)
{
    http_server_t hs = xmalloc(sizeof(*hs));
    hs->mutex = 0;
    hs->proxy_addr = 0;
    hs->ref_count = 1;
    hs->http_sessions = 0;

    hs->record_file = 0;
    return hs;
}

void http_server_destroy(http_server_t hs)
{
    if (hs)
    {
        int r;

        yaz_mutex_enter(hs->mutex); /* OK: hs->mutex may be NULL */
        r = --(hs->ref_count);
        yaz_mutex_leave(hs->mutex);

        if (r == 0)
        {
            http_sessions_destroy(hs->http_sessions);
            xfree(hs->proxy_addr);
            yaz_mutex_destroy(&hs->mutex);
            if (hs->record_file)
                fclose(hs->record_file);
            xfree(hs);
        }
    }
}

void http_server_incref(http_server_t hs)
{
    assert(hs);
    yaz_mutex_enter(hs->mutex);
    (hs->ref_count)++;
    yaz_mutex_leave(hs->mutex);
}

void http_mutex_init(struct conf_server *server)
{
    assert(server);

    assert(server->http_server->mutex == 0);
    pazpar2_mutex_create(&server->http_server->mutex, "http_server");
    server->http_server->http_sessions = http_sessions_create();
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

