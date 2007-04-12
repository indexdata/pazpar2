/* $Id: settings.h,v 1.14 2007-04-12 11:35:08 marc Exp $
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

#ifndef SETTINGS_H
#define SETTINGS_H

#define PZ_PIGGYBACK      0
#define PZ_ELEMENTS       1
#define PZ_REQUESTSYNTAX  2
#define PZ_CCLMAP         3
#define PZ_ENCODING       4
#define PZ_XSLT           5
#define PZ_NATIVESYNTAX   6
#define PZ_AUTHENTICATION 7
#define PZ_ALLOW          8
#define PZ_MAXRECS        9
#define PZ_ID            10
#define PZ_NAME          11
#define PZ_QUERYENCODING 12


struct setting
{
    int precedence;
    char *target;
    char *name;
    char *value;
    struct setting *next;
};

int settings_num(void);
void settings_read(const char *path);
int settings_offset(const char *name);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
