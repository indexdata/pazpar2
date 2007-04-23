/* $Id: sel_thread.h,v 1.2 2007-04-23 08:06:21 adam Exp $
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

#ifndef SEL_THREAD_H
#define SEL_THREAD_H
#include <yaz/yconfig.h>

YAZ_BEGIN_CDECL

/** \brief select thread handler type */
typedef struct sel_thread *sel_thread_t;

/** \brief creates select thread 
    \param work_handler handler that does work in worker thread
    \param work_destroy optional destroy handler for work (0 = no handler)
    \param read_fd pointer to readable socket upon completion
    \param no_of_threads number of worker threads
    \returns select thread handler

    Creates a worker thread. The worker thread will signal "completed"
    work by sending one byte to the read_fd file descriptor.
    You are supposed to select or poll on that for reading and
    call sel_thread_result accordingly.
*/
sel_thread_t sel_thread_create(void (*work_handler)(void *work_data),
                               void (*work_destroy)(void *work_data),
                               int *read_fd, int no_of_threads);

/** \brief destorys select thread 
    \param p select thread handler
*/
void sel_thread_destroy(sel_thread_t p);

/** \brief adds work to be carried out in thread
    \param p select thread handler
    \param data pointer to data that work_handler knows about
*/
void sel_thread_add(sel_thread_t p, void *data);

/** \brief gets result of work 
    \param p select thread handler
    \returns data for work (which work_handler has been working on)
*/
void *sel_thread_result(sel_thread_t p);

YAZ_END_CDECL


#endif


/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
