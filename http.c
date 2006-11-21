/*
 * $Id: http.c,v 1.1 2006-11-21 18:46:43 quinn Exp $
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

#include <yaz/yaz-util.h>
#include <yaz/comstack.h>
#include <netdb.h>

#include "command.h"
#include "util.h"
#include "eventl.h"
#include "pazpar2.h"
#include "http.h"
#include "http_command.h"

extern IOCHAN channel_list;

void http_addheader(struct http_response *r, const char *name, const char *value)
{
    struct http_channel *c = r->channel;
    struct http_header *h = nmem_malloc(c->nmem, sizeof *h);
    h->name = nmem_strdup(c->nmem, name);
    h->value = nmem_strdup(c->nmem, value);
    h->next = r->headers;
    r->headers = h;
}

char *argbyname(struct http_request *r, char *name)
{
    struct http_argument *p;
    for (p = r->arguments; p; p = p->next)
        if (!strcmp(p->name, name))
            return p->value;
    return 0;
}

char *headerbyname(struct http_request *r, char *name)
{
    struct http_header *p;
    for (p = r->headers; p; p = p->next)
        if (!strcmp(p->name, name))
            return p->value;
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

// Check if we have a complete request. Return 0 or length (including trailing newline)
// FIXME: Does not deal gracefully with requests carrying payload
// but this is kind of OK since we will reject anything other than an empty GET
static int request_check(const char *buf)
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

struct http_request *http_parse_request(struct http_channel *c, char *buf)
{
    struct http_request *r = nmem_malloc(c->nmem, sizeof(*r));
    char *p, *p2;

    r->channel = c;
    // Parse first line
    if (!strncmp(buf, "GET ", 4))
        r->method = Method_GET;
    else
    {
        yaz_log(YLOG_WARN, "Unexpected HTTP method in request");
        return 0;
    }
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
        strcpy(r->http_version, buf);
        buf = p;
    }
    strcpy(c->version, r->http_version);

    r->headers = 0; // We might want to parse these someday

    return r;
}


static char *http_serialize_response(struct http_channel *c, struct http_response *r)
{
    wrbuf_rewind(c->wrbuf);
    struct http_header *h;

    wrbuf_printf(c->wrbuf, "HTTP/1.1 %s %s\r\n", r->code, r->msg);
    for (h = r->headers; h; h = h->next)
        wrbuf_printf(c->wrbuf, "%s: %s\r\n", h->name, h->value);
    wrbuf_printf(c->wrbuf, "Content-length: %d\r\n", r->payload ? strlen(r->payload) : 0);
    wrbuf_printf(c->wrbuf, "Content-type: text/xml\r\n");
    wrbuf_puts(c->wrbuf, "\r\n");

    if (r->payload)
        wrbuf_puts(c->wrbuf, r->payload);

    wrbuf_putc(c->wrbuf, '\0');
    return wrbuf_buf(c->wrbuf);
}

// Cleanup
static void http_destroy(IOCHAN i)
{
    struct http_channel *s = iochan_getdata(i);

    yaz_log(YLOG_DEBUG, "Destroying http channel");
    nmem_destroy(s->nmem);
    wrbuf_free(s->wrbuf, 1);
    xfree(s);
    close(iochan_getfd(i));
    iochan_destroy(i);
}

static void http_io(IOCHAN i, int event)
{
    struct http_channel *hc = iochan_getdata(i);
    struct http_request *request;
    struct http_response *response;

    switch (event)
    {
        int res;

        case EVENT_INPUT:
            yaz_log(YLOG_DEBUG, "HTTP Input event");

            res = read(iochan_getfd(i), hc->ibuf + hc->read, IBUF_SIZE - (hc->read + 1));
            if (res <= 0)
            {
                yaz_log(YLOG_WARN|YLOG_ERRNO, "HTTP read");
                http_destroy(i);
                return;
            }
            yaz_log(YLOG_DEBUG, "HTTP read %d octets", res);
            hc->read += res;
            hc->ibuf[hc->read] = '\0';

            if ((res = request_check(hc->ibuf)) <= 2)
            {
                yaz_log(YLOG_DEBUG, "We don't have a complete HTTP request yet");
                return;
            }
            yaz_log(YLOG_DEBUG, "We think we have a complete HTTP request (len %d): \n%s", res,  hc->ibuf);
            nmem_reset(hc->nmem);
            if (!(request = http_parse_request(hc, hc->ibuf)))
            {
                yaz_log(YLOG_WARN, "Failed to parse request");
                http_destroy(i);
                return;
            }
            response = http_command(request);
            if (!response)
            {
                http_destroy(i);
                return;
            }
            // FIXME -- do something to cause the response to be sent to the client
            if (!(hc->obuf = http_serialize_response(hc, response)))
            {
                http_destroy(i);
                return;
            }
            yaz_log(YLOG_DEBUG, "Response ready:\n%s", hc->obuf);
            hc->writ = 0;
            hc->read = 0;
            iochan_setflags(i, EVENT_OUTPUT); // Turns off input selecting
            break;

        case EVENT_OUTPUT:
            yaz_log(YLOG_DEBUG, "HTTP output event");
            res = write(iochan_getfd(hc->iochan), hc->obuf + hc->writ,
                        strlen(hc->obuf + hc->writ));
            if (res <= 0)
            {
                yaz_log(YLOG_WARN|YLOG_ERRNO, "write");
                http_destroy(i);
                return;
            }
            hc->writ += res;
            if (!hc->obuf[hc->writ]) {
                yaz_log(YLOG_DEBUG, "Writing finished");
                if (!strcmp(hc->version, "1.0"))
                {
                    yaz_log(YLOG_DEBUG, "Closing 1.0 connection");
                    http_destroy(i);
                }
                else
                    iochan_setflags(i, EVENT_INPUT); // Turns off output flag
            }
            break;
        default:
            yaz_log(YLOG_WARN, "Unexpected event on connection");
            http_destroy(i);
    }
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

    yaz_log(YLOG_LOG, "New command connection");
    c = iochan_create(s, http_io, EVENT_INPUT | EVENT_EXCEPT);

    ch = xmalloc(sizeof(*ch));
    ch->read = 0;
    ch->nmem = nmem_create();
    ch->wrbuf = wrbuf_alloc();
    ch->iochan = c;
    iochan_setdata(c, ch);

    c->next = channel_list;
    channel_list = c;
}


/* Create a http-channel listener */
void http_init(int port)
{
    IOCHAN c;
    int l;
    struct protoent *p;
    struct sockaddr_in myaddr;
    int one = 1;

    yaz_log(YLOG_LOG, "HTTP port is %d", port);
    if (!(p = getprotobyname("tcp"))) {
        abort();
    }
    if ((l = socket(PF_INET, SOCK_STREAM, p->p_proto)) < 0)
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "socket");
    if (setsockopt(l, SOL_SOCKET, SO_REUSEADDR, (char*)
                    &one, sizeof(one)) < 0)
        abort();

    bzero(&myaddr, sizeof myaddr);
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = INADDR_ANY;
    myaddr.sin_port = htons(port);
    if (bind(l, (struct sockaddr *) &myaddr, sizeof myaddr) < 0) 
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "bind");
    if (listen(l, SOMAXCONN) < 0) 
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "listen");

    c = iochan_create(l, http_accept, EVENT_INPUT | EVENT_EXCEPT);
    c->next = channel_list;
    channel_list = c;
}


/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
