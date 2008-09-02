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

#ifndef SETTINGS_H
#define SETTINGS_H

#define PZ_PIGGYBACK      0
#define PZ_ELEMENTS       1
#define PZ_REQUESTSYNTAX  2
#define PZ_CCLMAP         3
#define PZ_XSLT           4
#define PZ_NATIVESYNTAX   5
#define PZ_AUTHENTICATION 6
#define PZ_ALLOW          7
#define PZ_MAXRECS        8
#define PZ_ID             9
#define PZ_NAME          10
#define PZ_QUERYENCODING 11
#define PZ_IP            12
#define PZ_ZPROXY        13
#define PZ_APDULOG       14
#define PZ_SRU           15
#define PZ_SRU_VERSION   16

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
int settings_offset_cprefix(const char *name);
void init_settings(void);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
