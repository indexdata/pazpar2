/* This file is part of Pazpar2.
   Copyright (C) 2006-2010 Index Data

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

#ifndef EVENTL_H
#define EVENTL_H

#include <time.h>

struct iochan;

typedef void (*IOC_CALLBACK)(struct iochan *i, int event);
typedef int (*IOC_SOCKETFUN)(struct iochan *i);
typedef int (*IOC_MASKFUN)(struct iochan *i);

typedef struct iochan_man_s *iochan_man_t;

typedef struct iochan
{
    int fd;
    int flags;
#define EVENT_INPUT     0x01
#define EVENT_OUTPUT    0x02
#define EVENT_EXCEPT    0x04
#define EVENT_TIMEOUT   0x08
    int force_event;
    IOC_CALLBACK fun;
    IOC_SOCKETFUN socketfun;
    IOC_MASKFUN maskfun;
    void *data;
    int destroyed;
    time_t last_event;
    time_t max_idle;
    int this_event;
    int thread_users;

    iochan_man_t man;
    char *name;
    struct iochan *next;
} *IOCHAN;


iochan_man_t iochan_man_create(int no_threads);
void iochan_add(iochan_man_t man, IOCHAN chan);
void iochan_man_events(iochan_man_t man);
void iochan_man_destroy(iochan_man_t *mp);

#define iochan_destroy(i) (void)((i)->destroyed = 1)
#define iochan_getfd(i) ((i)->fd)
#define iochan_setfd(i, f) ((i)->fd = (f))
#define iochan_getdata(i) ((i)->data)
#define iochan_setdata(i, d) ((i)->data = d)
#define iochan_getflags(i) ((i)->flags)
#define iochan_setflags(i, d) ((i)->flags = d)
#define iochan_setflag(i, d) ((i)->flags |= d)
#define iochan_clearflag(i, d) ((i)->flags &= ~(d))
#define iochan_getflag(i, d) ((i)->flags & d ? 1 : 0)
#define iochan_getfun(i) ((i)->fun)
#define iochan_setfun(i, d) ((i)->fun = d)
#define iochan_setevent(i, e) ((i)->force_event = (e))
#define iochan_settimeout(i, t) ((i)->max_idle = (t), (i)->last_event = time(0))
#define iochan_activity(i) ((i)->last_event = time(0))
#define iochan_setsocketfun(i, f) ((i)->socketfun = (f))
#define iochan_getsocketfun(i) ((i)->socketfun)
#define iochan_setmaskfun(i, f) ((i)->maskfun = (f))
#define iochan_getmaskfun(i) ((i)->maskfun)

IOCHAN iochan_create(int fd, IOC_CALLBACK cb, int flags, const char *name);

#endif
/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

