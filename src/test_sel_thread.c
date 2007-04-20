/* $Id: test_sel_thread.c,v 1.2 2007-04-20 11:44:58 adam Exp $
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

#if HAVE_CONFIG_H
#include "cconfig.h"
#endif

#include "sel_thread.h"
#include "eventl.h"
#include <yaz/test.h>
#include <yaz/xmalloc.h>

/** \brief stuff we work on in separate thread */
struct my_work_data {
    int x;
    int y;
};

/** \brief work to be carried out in separate thrad */
static void work_handler(void *vp)
{
    struct my_work_data *p = vp;
    p->y = p->x * 2;
}

/** \brief see if we can create and destroy without problems */
static void test_1(void)
{
    int fd;
    sel_thread_t p = sel_thread_create(work_handler, &fd);
    YAZ_CHECK(p);
    if (!p)
        return;

    sel_thread_destroy(p);
}


void iochan_handler(struct iochan *i, int event)
{
    static int number = 0;
    sel_thread_t p = iochan_getdata(i);

    if (event & EVENT_INPUT)
    {
        struct my_work_data *work;

        work = sel_thread_result(p);

        YAZ_CHECK(work);
        if (work)
        {
            YAZ_CHECK_EQ(work->x * 2, work->y);
            /* stop work after a couple of iterations */
            if (work->x > 10)
                iochan_destroy(i);

            xfree(work);
        }

    }
    if (event & EVENT_TIMEOUT)
    {
        struct my_work_data *work;

        work = xmalloc(sizeof(*work));
        work->x = number;
        sel_thread_add(p, work);

        work = xmalloc(sizeof(*work));
        work->x = number+1;
        sel_thread_add(p, work);

        number += 10;
    }
}

/** brief use the fd for something */
static void test_2(void)
{
    int thread_fd;
    sel_thread_t p = sel_thread_create(work_handler, &thread_fd);
    YAZ_CHECK(p);
    if (p)
    {
        IOCHAN chan = iochan_create(thread_fd, iochan_handler,
                                    EVENT_INPUT|EVENT_TIMEOUT);
        iochan_settimeout(chan, 1);
        iochan_setdata(chan, p);

        event_loop(&chan);
    }
    sel_thread_destroy(p);
}

int main(int argc, char **argv)
{
    YAZ_CHECK_INIT(argc, argv); 
    YAZ_CHECK_LOG(); 

    test_1();
    test_2();

    YAZ_CHECK_TERM;
}




/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
