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

/** \brief how work is destructed */
static void work_destroy(void *vp)
{
    struct my_work_data *p = vp;
    xfree(p);
}

/** \brief see if we can create and destroy without problems */
static void test_create_destroy(void)
{
    int fd;
    sel_thread_t p = sel_thread_create(work_handler, 0, &fd, 1);
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
static void test_for_real_work(int no_threads)
{
    int thread_fd;
    sel_thread_t p = sel_thread_create(work_handler, work_destroy, 
                                       &thread_fd, no_threads);
    YAZ_CHECK(p);
    if (p)
    {
        IOCHAN chan = iochan_create(thread_fd, iochan_handler,
                                    EVENT_INPUT|EVENT_TIMEOUT);
        iochan_settimeout(chan, 1);
        iochan_setdata(chan, p);

        event_loop(&chan);
        sel_thread_destroy(p);
    }
}

int main(int argc, char **argv)
{
    YAZ_CHECK_INIT(argc, argv); 
    YAZ_CHECK_LOG(); 

    test_create_destroy();
    test_for_real_work(1);
    test_for_real_work(3);

    YAZ_CHECK_TERM;
}




/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
