/* $Id: sel_thread.c,v 1.3 2007-04-20 11:44:58 adam Exp $
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
#include <yaz/log.h>
#include <yaz/nmem.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

struct work_item {
    void *data;
    struct work_item *next;
};

static struct work_item *queue_remove_last(struct work_item **q)
{
    struct work_item **work_p = q, *work_this = 0;

    while (*work_p && (*work_p)->next)
        work_p = &(*work_p)->next;
    if (*work_p)
    {
        work_this = *work_p;
        *work_p = 0;
    }
    return work_this;
}

struct sel_thread {
    int fd[2];
    NMEM nmem;
    pthread_t thread_id;
    pthread_mutex_t mutex;
    pthread_cond_t input_data;
    int stop_flag;
    struct work_item *input_queue;
    struct work_item *output_queue;
    struct work_item *free_queue;
    void (*work_handler)(void *work_data);;
};

static void *sel_thread_handler(void *vp)
{
    sel_thread_t p = (sel_thread_t) vp;

    while(1)
    {
        struct work_item *work_this = 0;
        /* wait for some work */
        pthread_mutex_lock(&p->mutex);
        while (!p->stop_flag && !p->input_queue)
            pthread_cond_wait(&p->input_data, &p->mutex);
        /* see if we were waken up because we're shutting down */
        if (p->stop_flag)
            break;
        /* got something. Take the last one out of input_queue */

        assert(p->input_queue);
        work_this = queue_remove_last(&p->input_queue);
        assert(work_this);

        pthread_mutex_unlock(&p->mutex);

        /* work on this item */
        p->work_handler(work_this->data);
        
        /* put it back into output queue */
        pthread_mutex_lock(&p->mutex);
        work_this->next = p->output_queue;
        p->output_queue = work_this;
        pthread_mutex_unlock(&p->mutex);

        /* wake up select/poll with a single byte */
        write(p->fd[1], "", 1);
    }        
    pthread_mutex_unlock(&p->mutex);
    return 0;
}

sel_thread_t sel_thread_create(void (*work_handler)(void *work_data),
                               int *read_fd)
{
    NMEM nmem = nmem_create();
    sel_thread_t p = nmem_malloc(nmem, sizeof(*p));

    p->nmem = nmem;
    if (pipe(p->fd))
    {
        nmem_destroy(nmem);
        return 0;
    }
    *read_fd = p->fd[0];
    p->input_queue = 0;
    p->output_queue = 0;
    p->free_queue = 0;
    p->work_handler = work_handler;

    p->stop_flag = 0;
    pthread_mutex_init(&p->mutex, 0);
    pthread_cond_init(&p->input_data, 0);
    pthread_create (&p->thread_id, 0, sel_thread_handler, p);
    return p;
}

void sel_thread_destroy(sel_thread_t p)
{
    pthread_mutex_lock(&p->mutex);
    p->stop_flag = 1;
    pthread_cond_broadcast(&p->input_data);
    pthread_mutex_unlock(&p->mutex);
    
    pthread_join(p->thread_id, 0);

    close(p->fd[0]);
    close(p->fd[1]);
    pthread_cond_destroy(&p->input_data);
    pthread_mutex_destroy(&p->mutex);
    nmem_destroy(p->nmem);
}

void sel_thread_add(sel_thread_t p, void *data)
{
    struct work_item *work_p;

    pthread_mutex_lock(&p->mutex);

    if (p->free_queue)
    {
        work_p = p->free_queue;
        p->free_queue = p->free_queue->next;
    }
    else
        work_p = nmem_malloc(p->nmem, sizeof(*work_p));

    work_p->data = data;
    work_p->next = p->input_queue;
    p->input_queue = work_p;

    pthread_cond_signal(&p->input_data);
    pthread_mutex_unlock(&p->mutex);
}

void *sel_thread_result(sel_thread_t p)
{
    struct work_item *work_this = 0;
    void *data = 0;
    char read_buf[1];

    pthread_mutex_lock(&p->mutex);

    /* got something. Take the last one out of output_queue */
    work_this = queue_remove_last(&p->output_queue);
    if (work_this)
    {
        /* put freed item in free list */
        work_this->next = p->free_queue;
        p->free_queue = work_this;
        
        data = work_this->data;
        read(p->fd[0], read_buf, 1);
    }
    pthread_mutex_unlock(&p->mutex);
    return data;
}

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
