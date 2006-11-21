#ifndef HTTP_H
#define HTTP_H

struct http_channel
{
    IOCHAN iochan;
#define IBUF_SIZE 10240
    char ibuf[IBUF_SIZE];
    char version[10];
    int read;
    char *obuf;
    int writ;
    NMEM nmem;
    WRBUF wrbuf;
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
    enum
    {
        Method_GET,
        Method_other
    } method;
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

void http_init(int port);
void http_addheader(struct http_response *r, const char *name, const char *value);
char *argbyname(struct http_request *r, char *name);
char *headerbyname(struct http_request *r, char *name);
struct http_response *http_create_response(struct http_channel *c);

#endif
