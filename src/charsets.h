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

/** \file charsets.h
    \brief Pazpar2 Character set facilities
*/

#ifndef PAZPAR_CHARSETS_H
#define PAZPAR_CHARSETS_H

#include <yaz/wrbuf.h>
#include <yaz/xmltypes.h>

struct icu_chain;

typedef struct pp2_charset_s *pp2_charset_t;
typedef struct pp2_relevance_token_s *pp2_relevance_token_t;

pp2_charset_t pp2_charset_create_xml(xmlNode *xml_node);
pp2_charset_t pp2_charset_create(struct icu_chain * icu_chn);
pp2_charset_t pp2_charset_create_a_to_z(void);

void pp2_charset_destroy(pp2_charset_t pct);
void pp2_charset_incref(pp2_charset_t pct);

pp2_relevance_token_t pp2_relevance_tokenize(pp2_charset_t pct);
void pp2_relevance_first(pp2_relevance_token_t prt,
                         const char *buf,
                         int skip_article);

void pp2_relevance_token_destroy(pp2_relevance_token_t prt);
const char *pp2_relevance_token_next(pp2_relevance_token_t prt);
const char *pp2_get_sort(pp2_relevance_token_t prt);

#if 0
typedef int pp2_charset_normalize_t(pp2_charset_t pct,
                                    const char *buf,
                                    WRBUF norm_str, WRBUF sort_str,
                                    int skip_article);

pp2_charset_normalize_t pp2_charset_metadata_norm;
#endif

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

