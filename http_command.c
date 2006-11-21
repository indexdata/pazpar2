/*
 * $Id: http_command.c,v 1.1 2006-11-21 18:46:43 quinn Exp $
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>

#include <yaz/yaz-util.h>

#include "command.h"
#include "util.h"
#include "eventl.h"
#include "pazpar2.h"
#include "http.h"
#include "http_command.h"

struct http_session {
    struct session *psession;
    char session_id[128];
    int timestamp;
    struct http_session *next;
};

static struct http_session *session_list = 0;

struct http_session *http_session_create()
{
    struct http_session *r = xmalloc(sizeof(*r));
    r->psession = 0;
    *r->session_id = '\0';
    r->timestamp = 0;
    r->next = session_list;
    session_list = r;
    return r;
}

void http_session_destroy(struct http_session *s)
{
    struct http_session **p;

    for (p = &session_list; *p; p = &(*p)->next)
        if (*p == s)
        {
            *p = (*p)->next;
            break;
        }
    session_destroy(s->psession);
    xfree(s);
}

static void error(struct http_response *rs, char *code, char *msg, char *txt)
{
    struct http_channel *c = rs->channel;
    char tmp[1024];

    if (!txt)
        txt = msg;
    rs->msg = nmem_strdup(c->nmem, msg);
    strcpy(rs->code, code);
    sprintf(tmp, "<error code=\"general\">%s</error>", txt);
    rs->payload = nmem_strdup(c->nmem, tmp);
}

static void cmd_init(struct http_request *rq, struct http_response *rs)
{
}

static void cmd_stat(struct http_request *rq, struct http_response *rs)
{
}

static void cmd_load(struct http_request *rq, struct http_response *rs)
{
}

struct {
    char *name;
    void (*fun)(struct http_request *rq, struct http_response *rs);
} commands[] = {
    { "init", cmd_init },
    { "stat", cmd_stat },
    { "load", cmd_load },
    {0,0}
};

struct http_response *http_command(struct http_request *rq)
{
    char *command = argbyname(rq, "command");
    struct http_channel *c = rq->channel;
    struct http_response *rs = http_create_response(c);
    int i;

    if (!command)
    {
        error(rs, "417", "Must supply command", 0);
        return rs;
    }
    for (i = 0; commands[i].name; i++)
        if (!strcmp(commands[i].name, command))
        {
            (*commands[i].fun)(rq, rs);
            break;
        }
    if (!commands[i].name)
        error(rs, "417", "Unknown command", 0);

    return rs;
}

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
