/*
 * ParaZ - a simple tool for harvesting performance data for parallel
 * operations using Z39.50.
 * Copyright (c) 2000-2004 Index Data ApS
 * See LICENSE file for details.
 */

/*
 * $Id: eventl.c,v 1.4 2007-04-08 23:04:20 adam Exp $
 * Based on revision YAZ' server/eventl.c 1.29.
 */

#include <stdio.h>
#include <assert.h>

#if HAVE_CONFIG_H
#include <cconfig.h>
#endif

#ifdef WIN32
#include <winsock.h>
#else
#include <unistd.h>
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
	to.tv_sec = 30;
	to.tv_usec = 0;
	max = 0;
    	for (p = *iochans; p; p = p->next)
    	{
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
	}
	if ((res = select(max + 1, &in, &out, &except, timeout)) < 0)
	{
	    if (errno == EINTR)
    		continue;
            else
	 	abort();
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
		(*p->fun)(p, EVENT_INPUT);
	    }
	    if (!p->destroyed && (FD_ISSET(p->fd, &out) ||
	        force_event == EVENT_OUTPUT))
	    {
	  	p->last_event = now;
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
