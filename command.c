/* $Id: command.c,v 1.3 2006-11-20 19:46:40 quinn Exp $ */

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

extern IOCHAN channel_list;

struct command_session {
    IOCHAN channel;
    char *outbuf;

    int outbuflen;
    int outbufwrit;

    struct session *psession;
};

void command_destroy(struct command_session *s);
void command_prompt(struct command_session *s);
void command_puts(struct command_session *s, const char *buf);

static int cmd_quit(struct command_session *s, char **argv, int argc)
{
    IOCHAN i = s->channel;
    close(iochan_getfd(i));
    iochan_destroy(i);
    command_destroy(s);
    return 0;
}

static int cmd_load(struct command_session *s, char **argv, int argc)
{
    if (argc != 2) {
        command_puts(s, "Usage: load filename\n");
    }
    if (load_targets(s->psession, argv[1]) < 0)
        command_puts(s, "Failed to open file\n");
    return 1;
}

static int cmd_search(struct command_session *s, char **argv, int argc)
{
    if (argc != 2)
    {
        command_puts(s, "Usage: search word\n");
        return 1;
    }
    search(s->psession, argv[1]);
    return 1;
}

static int cmd_hitsbytarget(struct command_session *s, char **argv, int argc)
{
    int count;
    int i;

    struct hitsbytarget *ht = hitsbytarget(s->psession, &count);
    for (i = 0; i < count; i++)
    {
        char buf[1024];

        sprintf(buf, "%s: %d (%d records, diag=%d, state=%s)\n", ht[i].id, ht[i].hits,
            ht[i].records, ht[i].diagnostic, ht[i].state);
        command_puts(s, buf);
    }
    return 1;
}

static int cmd_show(struct command_session *s, char **argv, int argc)
{
    struct record **recs;
    int num = 10;
    int i;

    if (argc == 2)
        num = atoi(argv[1]);

    recs = show(s->psession, 0, &num);

    for (i = 0; i < num; i++)
    {
        int rc;
        struct record *cnode;
        struct record *r = recs[i];

        command_puts(s, r->merge_key);
        for (rc = 1, cnode = r->next_cluster; cnode; cnode = cnode->next_cluster, rc++)
            ;
        if (rc > 1)
        {
            char buf[256];
            sprintf(buf, " (%d records)", rc);
            command_puts(s, buf);
        }
        command_puts(s, "\n");
    }
    return 1;
}

static int cmd_stat(struct command_session *s, char **argv, int argc)
{
    char buf[1024];
    struct statistics stat;

    statistics(s->psession, &stat);
    sprintf(buf, "Number of connections: %d\n", stat.num_connections);
    command_puts(s, buf);
    if (stat.num_no_connection)
    {
        sprintf(buf, "#No_connection:        %d\n", stat.num_no_connection);
        command_puts(s, buf);
    }
    if (stat.num_connecting)
    {
        sprintf(buf, "#Connecting:           %d\n", stat.num_connecting);
        command_puts(s, buf);
    }
    if (stat.num_initializing)
    {
        sprintf(buf, "#Initializing:         %d\n", stat.num_initializing);
        command_puts(s, buf);
    }
    if (stat.num_searching)
    {
        sprintf(buf, "#Searching:            %d\n", stat.num_searching);
        command_puts(s, buf);
    }
    if (stat.num_presenting)
    {
        sprintf(buf, "#Ppresenting:          %d\n", stat.num_presenting);
        command_puts(s, buf);
    }
    if (stat.num_idle)
    {
        sprintf(buf, "#Idle:                 %d\n", stat.num_idle);
        command_puts(s, buf);
    }
    if (stat.num_failed)
    {
        sprintf(buf, "#Failed:               %d\n", stat.num_failed);
        command_puts(s, buf);
    }
    if (stat.num_error)
    {
        sprintf(buf, "#Error:                %d\n", stat.num_error);
        command_puts(s, buf);
    }
    return 1;
}

static struct {
    char *cmd;
    int (*fun)(struct command_session *s, char *argv[], int argc);
} cmd_array[] = {
    {"quit", cmd_quit},
    {"load", cmd_load},
    {"find", cmd_search},
    {"ht", cmd_hitsbytarget},
    {"stat", cmd_stat},
    {"show", cmd_show},
    {0,0}
};

void command_command(struct command_session *s, char *command)
{
    char *p;
    char *argv[20];
    int argc = 0;
    int i;
    int res = -1;

    p = command;
    while (*p)
    {
        while (isspace(*p))
            p++;
        if (!*p)
            break;
        argv[argc++] = p;
        while (*p && !isspace(*p))
            p++;
        if (!*p)
            break;
        *(p++) = '\0';
    }
    if (argc) {
        for (i = 0; cmd_array[i].cmd; i++)
        {
            if (!strcmp(cmd_array[i].cmd, argv[0])) {
                res = (cmd_array[i].fun)(s, argv, argc);

                break;
            }
        }
        if (res < 0) {
            command_puts(s, "Unknown command.\n");
            command_prompt(s);
        }
        else if (res == 1) {
            command_prompt(s);
        }
    }
    else
        command_prompt(s);

}


static void command_io(IOCHAN i, int event)
{
    int res;
    char buf[1024];
    struct command_session *s;

    s = iochan_getdata(i);


    switch (event)
    {
        case EVENT_INPUT:
            res = read(iochan_getfd(i), buf, 1024);
            if (res <= 0)
            {
                yaz_log(YLOG_WARN|YLOG_ERRNO, "read command");
                close(iochan_getfd(i));
                iochan_destroy(i);
                command_destroy(s);
                return;
            }
            if (!index(buf, '\n')) {
                yaz_log(YLOG_WARN|YLOG_ERRNO, "Did not receive complete command");
                close(iochan_getfd(i));
                iochan_destroy(i);
                command_destroy(s);
                return;
            }
            buf[res] = '\0';
            command_command(s, buf);
            break;
        case EVENT_OUTPUT:
            if (!s->outbuflen || s->outbufwrit < 0)
            {
                yaz_log(YLOG_WARN, "Called with outevent but no data");
                iochan_clearflag(i, EVENT_OUTPUT);
            }
            else
            {
                res = write(iochan_getfd(i), s->outbuf + s->outbufwrit, s->outbuflen -
                    s->outbufwrit);
                if (res < 0) {
                    yaz_log(YLOG_WARN|YLOG_ERRNO, "write command");
                    close(iochan_getfd(i));
                    iochan_destroy(i);
                    command_destroy(s);
                }
                else
                {
                    s->outbufwrit += res;
                    if (s->outbufwrit >= s->outbuflen)
                    {
                        s->outbuflen = s->outbufwrit = 0;
                        iochan_clearflag(i, EVENT_OUTPUT);
                    }
                }
            }
            break;
        default:
            yaz_log(YLOG_WARN, "Bad voodoo on socket");
    }
}

void command_puts(struct command_session *s, const char *buf)
{
    int len = strlen(buf);
    memcpy(s->outbuf + s->outbuflen, buf, len);
    s->outbuflen += len;
    iochan_setflag(s->channel, EVENT_OUTPUT);
}

void command_prompt(struct command_session *s)
{
    command_puts(s, "Pazpar2> ");
}


/* Accept a new command connection */
static void command_accept(IOCHAN i, int event)
{
    struct sockaddr_in addr;
    int fd = iochan_getfd(i);
    socklen_t len;
    int s;
    IOCHAN c;
    struct command_session *ses;
    int flags;

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
    c = iochan_create(s, command_io, EVENT_INPUT | EVENT_EXCEPT);

    ses = xmalloc(sizeof(*ses));
    ses->outbuf = xmalloc(50000);
    ses->outbuflen = 0;
    ses->outbufwrit = 0;
    ses->channel = c;
    ses->psession = new_session();
    iochan_setdata(c, ses);

    command_puts(ses, "Welcome to pazpar2\n\n");
    command_prompt(ses);

    c->next = channel_list;
    channel_list = c;
}

void command_destroy(struct command_session *s) {
    xfree(s->outbuf);
    xfree(s);
}

/* Create a command-channel listener */
void command_init(int port)
{
    IOCHAN c;
    int l;
    struct protoent *p;
    struct sockaddr_in myaddr;
    int one = 1;

    yaz_log(YLOG_LOG, "Command port is %d", port);
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

    c = iochan_create(l, command_accept, EVENT_INPUT | EVENT_EXCEPT);
    //iochan_setdata(c, &l);
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
