/* $Id: eventl.h,v 1.4 2007-04-10 08:48:56 adam Exp $
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

#ifndef EVENTL_H
#define EVENTL_H

#include <time.h>

struct iochan;

typedef void (*IOC_CALLBACK)(struct iochan *i, int event);

typedef struct iochan
{
    int fd;
    int flags;
#define EVENT_INPUT     0x01
#define EVENT_OUTPUT    0x02
#define EVENT_EXCEPT    0x04
#define EVENT_TIMEOUT   0x08
#define EVENT_WORK      0x10
    int force_event;
    IOC_CALLBACK fun;
    void *data;
    int destroyed;
    time_t last_event;
    time_t max_idle;
    
    struct iochan *next;
} *IOCHAN;

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
#define iochan_getnext(i) ((i)->next)
#define iochan_settimeout(i, t) ((i)->max_idle = (t), (i)->last_event = time(0))
#define iochan_activity(i) ((i)->last_event = time(0))

IOCHAN iochan_create(int fd, IOC_CALLBACK cb, int flags);
int event_loop(IOCHAN *iochans);

#endif
