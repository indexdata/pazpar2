/* $Id: charsets.c,v 1.5 2007-05-25 10:32:55 marc Exp $
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

/** \file charsets.c
    \brief Pazpar2 Character set facilities
*/

#if HAVE_CONFIG_H
#include "cconfig.h"
#endif

#include <yaz/xmalloc.h>
#include <yaz/wrbuf.h>
#include <yaz/log.h>
#include <ctype.h>
#include <assert.h>

#include "charsets.h"
//#include "config.h"
//#include "parameters.h"

#ifdef HAVE_ICU
#include "icu_I18N.h"
#endif // HAVE_ICU

/* charset handle */
struct pp2_charset_s {
    const char *(*token_next_handler)(pp2_relevance_token_t prt);
    /* other handlers will come as we see fit */
#ifdef HAVE_ICU
    struct icu_chain * icu_chn;
    UErrorCode icu_sts;
#endif // HAVE_ICU
};

static const char *pp2_relevance_token_a_to_z(pp2_relevance_token_t prt);

#ifdef HAVE_ICU
static const char *pp2_relevance_token_icu(pp2_relevance_token_t prt);
#endif // HAVE_ICU

/* tokenzier handle */
struct pp2_relevance_token_s {
    const char *cp;     /* unnormalized buffer we're tokenizing */
    pp2_charset_t pct;  /* our main charset handle (type+config) */
    WRBUF norm_str;     /* normized string we return (temporarily) */
};

pp2_charset_t pp2_charset_create(struct icu_chain * icu_chn)
{
    pp2_charset_t pct = xmalloc(sizeof(*pct));

    pct->token_next_handler = pp2_relevance_token_a_to_z;
#ifdef HAVE_ICU
    pct->icu_chn = 0;
    if (icu_chn){
        pct->icu_chn = icu_chn;
        pct->icu_sts = U_ZERO_ERROR;
        pct->token_next_handler = pp2_relevance_token_icu;
    }
 #endif // HAVE_ICU
    return pct;
}

void pp2_charset_destroy(pp2_charset_t pct)
{
    xfree(pct);
}

pp2_relevance_token_t pp2_relevance_tokenize(pp2_charset_t pct,
                                             const char *buf)
{
    pp2_relevance_token_t prt = xmalloc(sizeof(*prt));

    assert(pct);

    prt->norm_str = wrbuf_alloc();
    prt->cp = buf;
    prt->pct = pct;

#ifdef HAVE_ICU
    if (pct->icu_chn)
    {
        pct->icu_sts = U_ZERO_ERROR;
        int ok = 0;
        ok = icu_chain_assign_cstr(pct->icu_chn, buf, &pct->icu_sts);
        //printf("\nfield ok: %d '%s'\n", ok, buf);
        prt->pct = pct;
        prt->norm_str = 0;
    }
#endif // HAVE_ICU
    return prt;
}


void pp2_relevance_token_destroy(pp2_relevance_token_t prt)
{
    assert(prt);
    if(prt->norm_str) 
        wrbuf_destroy(prt->norm_str);
    xfree(prt);
}

const char *pp2_relevance_token_next(pp2_relevance_token_t prt)
{
    assert(prt);
    return (prt->pct->token_next_handler)(prt);
}

#define raw_char(c) (((c) >= 'a' && (c) <= 'z') ? (c) - 'a' + 1 : -1)
/* original tokenizer with our tokenize interface, but we
   add +1 to ensure no '\0' are in our string (except for EOF)
*/
static const char *pp2_relevance_token_a_to_z(pp2_relevance_token_t prt)
{
    const char *cp = prt->cp;
    int c;

    /* skip white space */
    while (*cp && (c = raw_char(tolower(*cp))) < 0)
        cp++;
    if (*cp == '\0')
    {
        prt->cp = cp;
        return 0;
    }
    /* now read the term itself */
    wrbuf_rewind(prt->norm_str);
    while (*cp && (c = raw_char(tolower(*cp))) >= 0)
    {
        wrbuf_putc(prt->norm_str, c);
        cp++;
    }
    prt->cp = cp;
    return wrbuf_cstr(prt->norm_str);
}


#ifdef HAVE_ICU
static const char *pp2_relevance_token_icu(pp2_relevance_token_t prt)
{
    //&& U_SUCCESS(pct->icu_sts))
    if (icu_chain_next_token(prt->pct->icu_chn, &prt->pct->icu_sts)){
        //printf("'%s' ",  icu_chain_get_norm(prt->pct->icu_chn)); 
        if (U_FAILURE(prt->pct->icu_sts))
        {
            //printf("ICU status failure\n "); 
            return 0;
        }
            
        return icu_chain_get_norm(prt->pct->icu_chn);
    }
    //printf ("EOF\n");
    return 0;
};
#endif // HAVE_ICU



/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
