/* This file is part of Pazpar2.
   Copyright (C) 2006-2011 Index Data

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
#define PZ_ZPROXY        12
#define PZ_APDULOG       13
#define PZ_SRU           14
#define PZ_SRU_VERSION   15
#define PZ_PQF_PREFIX    16
#define PZ_SORT          17
#define PZ_RECORDFILTER	 18
#define PZ_PQF_STRFTIME  19
#define PZ_NEGOTIATION_CHARSET  20
#define PZ_MAX_CONNECTIONS      21
#define PZ_REUSE_CONNECTIONS    22
#define PZ_TERMLIST_TERM_FACTOR 23
#define PZ_PREFERRED            24
#define PZ_EXTRA_ARGS           25
#define PZ_QUERY_SYNTAX         26
#define PZ_FACETMAP             27
#define PZ_LIMITMAP             28
#define PZ_URL                  29
#define PZ_MAX_EOF              30

struct setting
{
    int precedence;
    char *target;
    char *name;
    char *value;
    struct setting *next;
};

void settings_read_file(struct conf_service *service, const char *path,
                        int pass);
void settings_read_node(struct conf_service *service, xmlNode *n,
                        int pass);
int settings_num(struct conf_service *service);
int settings_create_offset(struct conf_service *service, const char *name);
int settings_lookup_offset(struct conf_service *service, const char *name);
void init_settings(struct conf_service *service);
void settings_read_node_x(xmlNode *n,
                          void *client_data,
                          void (*fun)(void *client_data,
                                      struct setting *set));
void expand_settings_array(struct setting ***set_ar, int *num, int offset,
                           NMEM nmem);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

