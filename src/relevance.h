/* $Id: relevance.h,v 1.4 2007-04-10 08:48:56 adam Exp $
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

#ifndef RELEVANCE_H
#define RELEVANCE_H

#include <yaz/yaz-util.h>

#include "pazpar2.h"
#include "reclists.h"

struct relevance;

struct relevance *relevance_create(NMEM nmem, const char **terms, int numrecs);
void relevance_newrec(struct relevance *r, struct record_cluster *cluster);
void relevance_countwords(struct relevance *r, struct record_cluster *cluster,
        const char *words, int multiplier);
void relevance_donerecord(struct relevance *r, struct record_cluster *cluster);

void relevance_prepare_read(struct relevance *rel, struct reclist *rec);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
