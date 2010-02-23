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

/*
 * Based on  ParaZ - a simple tool for harvesting performance data for
 * parallel operations using Z39.50.
 * Copyright (C) 2006-2010 Index Data ApS
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
#include "sel_thread.h"

struct iochan_man_s {
    IOCHAN channel_list;
    sel_thread_t sel_thread;
    int sel_fd;
    int no_threads;
    int log_level;
};

iochan_man_t iochan_man_create(int no_threads)
{
    iochan_man_t man = xmalloc(sizeof(*man));
    man->channel_list = 0;
    man->sel_thread = 0; /* can't create sel_thread yet because we may fork */
    man->sel_fd = -1;
    man->no_threads = no_threads;
    man->log_level = YLOG_DEBUG;
    return man;
}

void iochan_man_destroy(iochan_man_t *mp)
{
    if (*mp)
    {
        if ((*mp)->sel_thread)
            sel_thread_destroy((*mp)->sel_thread);
        xfree(*mp);
        *mp = 0;
    }
}

void iochan_add(iochan_man_t man, IOCHAN chan)
{
    chan->man = man;
    chan->next = man->channel_list;
    man->channel_list = chan;
}

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
    new_iochan->man = 0;
    new_iochan->thread_users = 0;
    return new_iochan;
}

static void work_handler(void *work_data)
{
    IOCHAN p = work_data;
    if (!p->destroyed && (p->this_event & EVENT_TIMEOUT))
        (*p->fun)(p, EVENT_TIMEOUT);
    if (!p->destroyed && (p->this_event & EVENT_INPUT))
        (*p->fun)(p, EVENT_INPUT);
    if (!p->destroyed && (p->this_event & EVENT_OUTPUT))
        (*p->fun)(p, EVENT_OUTPUT);
    if (!p->destroyed && (p->this_event & EVENT_EXCEPT))
        (*p->fun)(p, EVENT_EXCEPT);
}

static void run_fun(iochan_man_t man, IOCHAN p)
{
    if (p->this_event)
    {
        if (man->sel_thread)
        {
            yaz_log(man->log_level, "eventl: add fun chan=%p event=%d",
                    p, p->this_event);
            p->thread_users++;
            sel_thread_add(man->sel_thread, p);
        }
        else
            work_handler(p);
    }
}

static int event_loop(iochan_man_t man, IOCHAN *iochans)
{
    do /* loop as long as there are active associations to process */
    {
    	IOCHAN p, *nextp;
	fd_set in, out, except;
	int res, max;
	static struct timeval nullto = {0, 0}, to;
	struct timeval *timeout;

	FD_ZERO(&in);
	FD_ZERO(&out);
	FD_ZERO(&except);
	timeout = &to; /* hang on select */
	to.tv_sec = 300;
	to.tv_usec = 0;
	max = 0;
    	for (p = *iochans; p; p = p->next)
    	{
            if (p->thread_users > 0)
                continue;
            if (p->maskfun)
                p->flags = (*p->maskfun)(p);
            if (p->socketfun)
                p->fd = (*p->socketfun)(p);
            if (p->max_idle && p->max_idle < to.tv_sec)
                to.tv_sec = p->max_idle;
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
        if (man->sel_fd != -1)
        {
            if (man->sel_fd > max)
                max = man->sel_fd;
            yaz_log(man->log_level, "select on sel fd=%d", man->sel_fd);
            FD_SET(man->sel_fd, &in);
        }
        yaz_log(man->log_level, "select begin");
        res = select(max + 1, &in, &out, &except, timeout);
        yaz_log(man->log_level, "select returned res=%d", res);
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
        if (man->sel_fd != -1)
        {
            if (FD_ISSET(man->sel_fd, &in))
            {
                IOCHAN chan;

                yaz_log(man->log_level, "eventl: sel input on sel_fd=%d",
                        man->sel_fd);
                while ((chan = sel_thread_result(man->sel_thread)))
                {
                    yaz_log(man->log_level, "eventl: got thread result p=%p",
                            chan);
                    chan->thread_users--;
                }
            }
        }
        for (p = *iochans; p; p = p->next)
        {
            int force_event = p->force_event;
            time_t now = time(0);
            
            if (p->thread_users > 0 || p->destroyed)
            {
                yaz_log(man->log_level, "eventl: skip chan=%p users=%d", p, p->thread_users);
                continue;
            }
            p->this_event = 0;
            p->force_event = 0;

            if ((p->max_idle && now - p->last_event > p->max_idle) 
                || force_event == EVENT_TIMEOUT)
            {
                p->last_event = now;
                p->this_event |= EVENT_TIMEOUT;
            }
            if (p->fd >= 0)
            {
                if (FD_ISSET(p->fd, &in) || force_event == EVENT_INPUT)
                {
                    p->last_event = now;
                    yaz_log(YLOG_DEBUG, "Eventl input event");
                    p->this_event |= EVENT_INPUT;
                }
                if (FD_ISSET(p->fd, &out) || force_event == EVENT_OUTPUT)
                {
                    p->last_event = now;
                    yaz_log(YLOG_DEBUG, "Eventl output event");
                    p->this_event |= EVENT_OUTPUT;
                }
                if (FD_ISSET(p->fd, &except) || force_event == EVENT_EXCEPT)
                {
                    p->last_event = now;
                    p->this_event |= EVENT_EXCEPT;
                }
            }
            run_fun(man, p);
	}
        for (nextp = iochans; *nextp; )
        {
            IOCHAN p = *nextp;
	    if (p->destroyed && p->thread_users == 0)
	    {
                *nextp = p->next;
                xfree(p);
	    }
            else
                nextp = &p->next;
        }
    }
    while (*iochans);
    return 0;
}

void iochan_man_events(iochan_man_t man)
{
    if (man->no_threads > 0 && !man->sel_thread)
    {
        man->sel_thread = sel_thread_create(
            work_handler, 0 /*work_destroy */, &man->sel_fd, man->no_threads);
        yaz_log(man->log_level, "iochan_man_events. Using %d threads",
                man->no_threads);
    }
    event_loop(man, &man->channel_list);
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

