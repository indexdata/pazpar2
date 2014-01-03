/* This file is part of Pazpar2.
   Copyright (C) Index Data

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

/** \file charsets.h
    \brief Pazpar2 Character set facilities
*/

#ifndef PAZPAR_CHARSETS_H
#define PAZPAR_CHARSETS_H

#include <yaz/wrbuf.h>
#include <yaz/xmltypes.h>

typedef struct pp2_charset_token_s *pp2_charset_token_t;
typedef struct pp2_charset_fact_s *pp2_charset_fact_t;

pp2_charset_fact_t pp2_charset_fact_create(void);
void pp2_charset_fact_destroy(pp2_charset_fact_t pft);
int pp2_charset_fact_define(pp2_charset_fact_t pft,
                            xmlNode *xml_node, const char *default_id);
void pp2_charset_fact_incref(pp2_charset_fact_t pft);
pp2_charset_token_t pp2_charset_token_create(pp2_charset_fact_t pft,
                                             const char *id);

void pp2_charset_token_first(pp2_charset_token_t prt,
                             const char *buf,
                             int skip_article);
void pp2_charset_token_destroy(pp2_charset_token_t prt);
const char *pp2_charset_token_next(pp2_charset_token_t prt);
const char *pp2_get_sort(pp2_charset_token_t prt);
const char *pp2_get_display(pp2_charset_token_t prt);
void pp2_get_org(pp2_charset_token_t prt, size_t *start, size_t *len);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

