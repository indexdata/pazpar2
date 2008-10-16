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

/*
 * Based on  ParaZ - a simple tool for harvesting performance data for
 * parallel operations using Z39.50.
 * Copyright (c) 2000-2004 Index Data ApS
 * See LICENSE file for details.
 */

/*
 * Based on revision YAZ' server/eventl.c 1.29.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <assert.h>

#ifdef WIN32
#include <winsock.h>
#else
#include <unistd.h>
#endif
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <yaz/yconfig.h>
#include <yaz/log.h>
#include <yaz/comstack.h>
#include <yaz/xmalloc.h>
#include "eventl.h"
#include <yaz/statserv.h>

IOCHAN iochan_create(int fd, IOC_CALLBACK cb, int flags)
{
    IOCHAN new_iochan;

    if (!(new_iochan = (IOCHAN)xmalloc(sizeof(*new_iochan))))
    	return 0;
    new_iochan->destroyed = 0;
    new_iochan->fd = fd;
    new_iochan->flags = flags;
    new_iochan->fun = cb;
    new_iochan->socketfun = NULL;
    new_iochan->maskfun = NULL;
    new_iochan->force_event = 0;
    new_iochan->last_event = new_iochan->max_idle = 0;
    new_iochan->next = NULL;
    return new_iochan;
}

int event_loop(IOCHAN *iochans)
{
    do /* loop as long as there are active associations to process */
    {
    	IOCHAN p, nextp;
	fd_set in, out, except;
	int res, max;
	static struct timeval nullto = {0, 0}, to;
	struct timeval *timeout;

	FD_ZERO(&in);
	FD_ZERO(&out);
	FD_ZERO(&except);
	timeout = &to; /* hang on select */
	to.tv_sec = 15;
	to.tv_usec = 0;
	max = 0;
    	for (p = *iochans; p; p = p->next)
    	{
            if (p->maskfun)
                p->flags = (*p->maskfun)(p);
            if (p->socketfun)
                p->fd = (*p->socketfun)(p);
            if (p->fd < 0)
                continue;
	    if (p->force_event)
		timeout = &nullto;        /* polling select */
	    if (p->flags & EVENT_INPUT)
		FD_SET(p->fd, &in);
	    if (p->flags & EVENT_OUTPUT)
	        FD_SET(p->fd, &out);
	    if (p->flags & EVENT_EXCEPT)
	        FD_SET(p->fd, &except);
	    if (p->fd > max)
	        max = p->fd;
            if (p->max_idle && p->max_idle < to.tv_sec)
                to.tv_sec = p->max_idle;
	}
        res = select(max + 1, &in, &out, &except, timeout);        
        if (res < 0)
	{
	    if (errno == EINTR)
    		continue;
            else
            {
                yaz_log(YLOG_ERRNO|YLOG_WARN, "select");
                return 0;
            }
	}
    	for (p = *iochans; p; p = p->next)
    	{
	    int force_event = p->force_event;
	    time_t now = time(0);

	    p->force_event = 0;
	    if (!p->destroyed && ((p->max_idle && now - p->last_event >
	        p->max_idle) || force_event == EVENT_TIMEOUT))
	    {
	        p->last_event = now;
	        (*p->fun)(p, EVENT_TIMEOUT);
	    }
            if (p->fd < 0)
                continue;
	    if (!p->destroyed && (FD_ISSET(p->fd, &in) ||
		force_event == EVENT_INPUT))
	    {
    		p->last_event = now;
                yaz_log(YLOG_DEBUG, "Eventl input event");
		(*p->fun)(p, EVENT_INPUT);
	    }
	    if (!p->destroyed && (FD_ISSET(p->fd, &out) ||
	        force_event == EVENT_OUTPUT))
	    {
	  	p->last_event = now;
                yaz_log(YLOG_DEBUG, "Eventl output event");
	    	(*p->fun)(p, EVENT_OUTPUT);
	    }
	    if (!p->destroyed && (FD_ISSET(p->fd, &except) ||
	        force_event == EVENT_EXCEPT))
	    {
		p->last_event = now;
	    	(*p->fun)(p, EVENT_EXCEPT);
	    }
	}
	for (p = *iochans; p; p = nextp)
	{
	    nextp = p->next;

	    if (p->destroyed)
	    {
		IOCHAN tmp = p, pr;

	    	/* Now reset the pointers */
                if (p == *iochans)
		    *iochans = p->next;
		else
		{
		    for (pr = *iochans; pr; pr = pr->next)
		        if (pr->next == p)
    		            break;
		    assert(pr); /* grave error if it weren't there */
		    pr->next = p->next;
		}
		if (nextp == p)
		    nextp = p->next;
		xfree(tmp);
	    }
	}
    }
    while (*iochans);
    return 0;
}

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
