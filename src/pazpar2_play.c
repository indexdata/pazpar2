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

#include <time.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>

#include <yaz/options.h>
#include <yaz/log.h>
#include <yaz/xmalloc.h>

struct con {
    int fd;
    long long id;
    struct con *next;
};


static int run(FILE *inf, struct addrinfo *res)
{
    long long tv_sec0 = 0;
    long long tv_usec0 = 0;
    struct con *cons = 0;

    while (1)
    {
        long long tv_sec1;
        long long tv_usec1;
        long long id;
        int sz, r, c;
        char req[100];
        char request_type[100];
        size_t i;
        struct con **conp;
        c = fgetc(inf);
        if (c == EOF)
            break;

        for (i = 0; c != '\n' && i < (sizeof(req)-2); i++)
        {
            req[i] = c;
            c = fgetc(inf);
        }
        req[i] = 0;
        r = sscanf(req, "%s %lld %lld %lld %d", request_type,
                   &tv_sec1, &tv_usec1, &id, &sz);
        if (r != 5)
        {
            fprintf(stderr, "bad line %s\n", req);
            return -1;
        }
        if (tv_sec0)
        {
            struct timeval spec;

            spec.tv_sec = tv_sec1 - tv_sec0;
            if (tv_usec0 > tv_usec1)
            {
                spec.tv_usec = 1000000 + tv_usec1 - tv_usec0;
                spec.tv_sec--;
            }
            else
                spec.tv_usec = tv_usec1 - tv_usec0;

            select(0, 0, 0, 0, &spec);
        }
        tv_sec0 = tv_sec1;
        tv_usec0 = tv_usec1;
        for (conp = &cons; *conp; conp = &(*conp)->next)
            if ((*conp)->id == id)
                break;
        if (!*conp)
        {
            struct addrinfo *rp;
            int r, fd = -1;
            for (rp = res; rp; rp = rp->ai_next)
            {
                fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
                if (fd != -1)
                    break;
            }
            if (fd == -1)
            {
                fprintf(stderr, "socket: cannot create\n");
                return -1;
            }
            r = connect(fd, rp->ai_addr, rp->ai_addrlen);
            if (r)
            {
                fprintf(stderr, "socket: cannot connect\n");
                return -1;
            }

            *conp = xmalloc(sizeof(**conp));
            (*conp)->id = id;
            (*conp)->fd = fd;
            (*conp)->next = 0;
        }
        if (sz == 0)
        {
            struct con *c = *conp;
            *conp = c->next;
            close(c->fd);
            xfree(c);
        }
        else
        {
            size_t cnt = 0;
            while (cnt < sz)
            {
                char buf[1024];
                ssize_t w;
                size_t r;
                size_t toread = sz - cnt;

                if (toread > sizeof(buf))
                    toread = sizeof(buf);
                r = fread(buf, 1, toread, inf);
                if (r != toread)
                {
                    fprintf(stderr, "fread truncated. toread=%lld r=%lld\n",
                            (long long) toread, (long long) r);
                    return -1;
                }
                if (*request_type == 'r')
                {   /* Only deal with things tha Pazpar2 received */
                    w = write((*conp)->fd, buf, toread);
                    if (w != toread)
                    {
                        fprintf(stderr, "write truncated\n");
                        return -1;
                    }
                }
                cnt += toread;
            }
        }
    }
    return 0;
}

static void usage(void)
{
    fprintf(stderr, "Usage: pazpar2_play infile host\n"
            "    -v level                Set log level\n");
    exit(1);
}

int main(int argc, char **argv)
{
    int ret;
    char *arg;
    char *host = 0;
    const char *file = 0;
    while ((ret = options("v:", argv, argc, &arg)) != -2)
    {
	switch (ret)
        {
        case 'v':
            yaz_log_init_level(yaz_log_mask_str(arg));
            break;
        case 0:
            if (!file)
                file = arg;
            else if (!host)
                host = xstrdup(arg);
            else
            {
                usage();
            }
            break;
        default:
            usage();
            exit(1);
	}
    }
    if (host && file)
    {
        char *port;
        char *cp;
        FILE *inf;

        struct addrinfo hints, *res;
        hints.ai_flags = 0;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = 0;
        hints.ai_addrlen        = 0;
        hints.ai_addr           = NULL;
        hints.ai_canonname      = NULL;
        hints.ai_next           = NULL;

        cp = strchr(host, ':');
        if (*cp)
        {
            *cp = 0;
            port = cp+1;
        }
        else
        {
            port = "80";
        }
        if (getaddrinfo(host, port, &hints, &res))
        {
            fprintf(stderr, "cannot resolve %s:%s\n", host, port);
            exit(1);
        }

        inf = fopen(file, "rb");
        if (!inf)
        {
            fprintf(stderr, "cannot open %s\n", file);
            exit(1);
        }
        run(inf, res);
        fclose(inf);
    }
    else
        usage();
    return 0;
}


/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

