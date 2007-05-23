/* $Id: charsets.h,v 1.2 2007-05-23 14:44:18 marc Exp $
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

/** \file charsets.h
    \brief Pazpar2 Character set facilities
*/

#ifndef PAZPAR_CHARSETS_H
#define PAZPAR_CHARSETS_H


struct icu_chain;

typedef struct pp2_charset_s *pp2_charset_t;
typedef struct pp2_relevance_token_s *pp2_relevance_token_t;

pp2_charset_t pp2_charset_create(struct icu_chain * icu_chn);
void pp2_charset_destroy(pp2_charset_t pct);

pp2_relevance_token_t pp2_relevance_tokenize(pp2_charset_t pct,
                                             const char *buf);
void pp2_relevance_token_destroy(pp2_relevance_token_t prt);
const char *pp2_relevance_token_next(pp2_relevance_token_t prt);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
