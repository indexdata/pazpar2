/* $Id: termlists.h,v 1.2 2007-04-10 08:48:56 adam Exp $
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

#ifndef TERMLISTS_H
#define TERMLISTS_H

#include <yaz/nmem.h>

struct termlist_score
{
    char *term;
    int frequency;
};

struct termlist;

struct termlist *termlist_create(NMEM nmem, int numterms, int highscore_size);
void termlist_insert(struct termlist *tl, const char *term);
struct termlist_score **termlist_highscore(struct termlist *tl, int *len);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
