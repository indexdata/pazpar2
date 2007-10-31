/* $Id: normalize7bit.h,v 1.3 2007-10-31 05:29:08 quinn Exp $
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

#ifndef NORMALIZE7BIT_H
#define NORMALIZE7BIT_H

char *normalize7bit_mergekey(char *buf, int skiparticle);
char * normalize7bit_generic(char * str, const char * rm_chars);
int extract7bit_dates(const char *buf, int *first, int *last, int longdate);


#endif
