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
};

struct http_proxy //  attached to iochan for proxy connection
{
    IOCHAN iochan;
    struct http_channel *channel;
    struct http_buf *oqueue;
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

void http_set_proxyaddr(char *url);
void http_init(int port);
void http_addheader(struct http_response *r, const char *name, const char *value);
char *http_argbyname(struct http_request *r, char *name);
char *http_headerbyname(struct http_request *r, char *name);
struct http_response *http_create_response(struct http_channel *c);
void http_send_response(struct http_channel *c);

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
#endif
