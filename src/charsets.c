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

/** \file charsets.c
    \brief Pazpar2 Character set facilities
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <yaz/xmalloc.h>
#include <yaz/wrbuf.h>
#include <yaz/log.h>
#include <ctype.h>
#include <assert.h>

#include "charsets.h"
#include "normalize7bit.h"

#ifdef HAVE_ICU
#include "icu_I18N.h"
#endif // HAVE_ICU

/* charset handle */
struct pp2_charset_s {
    const char *(*token_next_handler)(pp2_relevance_token_t prt);
    const char *(*get_sort_handler)(pp2_relevance_token_t prt, int skip);
#ifdef HAVE_ICU
    struct icu_chain * icu_chn;
    UErrorCode icu_sts;
#endif // HAVE_ICU
};

static const char *pp2_relevance_token_a_to_z(pp2_relevance_token_t prt);
static const char *pp2_get_sort_ascii(pp2_relevance_token_t prt, int skip_article);

#ifdef HAVE_ICU
static const char *pp2_relevance_token_icu(pp2_relevance_token_t prt);
static const char *pp2_get_sort_icu(pp2_relevance_token_t prt, int skip_article);
#endif // HAVE_ICU

/* tokenzier handle */
struct pp2_relevance_token_s {
    const char *cp;     /* unnormalized buffer we're tokenizing */
    const char *last_cp;  /* pointer to last token we're dealing with */
    pp2_charset_t pct;  /* our main charset handle (type+config) */
    WRBUF norm_str;     /* normized string we return (temporarily) */
    WRBUF sort_str;     /* sort string we return (temporarily) */
};


pp2_charset_t pp2_charset_create_xml(xmlNode *xml_node)
{
#ifdef HAVE_ICU
    UErrorCode status = U_ZERO_ERROR;
    struct icu_chain *chain = 0;
    if (xml_node)
        xml_node = xml_node->children;
    while (xml_node && xml_node->type != XML_ELEMENT_NODE)
        xml_node = xml_node->next;
    chain = icu_chain_xml_config(xml_node, &status);
    if (!chain || U_FAILURE(status)){
        //xmlDocPtr icu_doc = 0;
        //xmlChar *xmlstr = 0;
                //int size = 0;
                //xmlDocDumpMemory(icu_doc, size);
        
        yaz_log(YLOG_FATAL, "Could not parse ICU chain config:\n"
                "<%s>\n ... \n</%s>",
                xml_node->name, xml_node->name);
        return 0;
    }
    return pp2_charset_create(chain);
#else // HAVE_ICU
    yaz_log(YLOG_FATAL, "Error: ICU support requested with element:\n"
            "<%s>\n ... \n</%s>",
            xml_node->name, xml_node->name);
    yaz_log(YLOG_FATAL, 
            "But no ICU support compiled into pazpar2 server.");
    yaz_log(YLOG_FATAL, 
            "Please install libicu36-dev and icu-doc or similar, "
            "re-configure and re-compile");            
    return 0;
#endif // HAVE_ICU
}


pp2_charset_t pp2_charset_create(struct icu_chain * icu_chn)
{
    pp2_charset_t pct = xmalloc(sizeof(*pct));

    pct->token_next_handler = pp2_relevance_token_a_to_z;
    pct->get_sort_handler  = pp2_get_sort_ascii;
#ifdef HAVE_ICU
    pct->icu_chn = 0;
    if (icu_chn)
    {
        pct->icu_chn = icu_chn;
        pct->icu_sts = U_ZERO_ERROR;
        pct->token_next_handler = pp2_relevance_token_icu;
        pct->get_sort_handler = pp2_get_sort_icu;
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
    prt->sort_str = wrbuf_alloc();
    prt->cp = buf;
    prt->last_cp = 0;
    prt->pct = pct;

#ifdef HAVE_ICU
    if (pct->icu_chn)
    {
        int ok = 0;
        pct->icu_sts = U_ZERO_ERROR;
        ok = icu_chain_assign_cstr(pct->icu_chn, buf, &pct->icu_sts);
        //printf("\nfield ok: %d '%s'\n", ok, buf);
        prt->pct = pct;
    }
#endif // HAVE_ICU
    return prt;
}


void pp2_relevance_token_destroy(pp2_relevance_token_t prt)
{
    assert(prt);
    if(prt->norm_str) 
        wrbuf_destroy(prt->norm_str);
    if(prt->sort_str) 
        wrbuf_destroy(prt->sort_str);
    xfree(prt);
}

const char *pp2_relevance_token_next(pp2_relevance_token_t prt)
{
    assert(prt);
    return (prt->pct->token_next_handler)(prt);
}

const char *pp2_get_sort(pp2_relevance_token_t prt, int skip)
{
    return prt->pct->get_sort_handler(prt, skip);
}

#define raw_char(c) (((c) >= 'a' && (c) <= 'z') ? (c) : -1)
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
        prt->last_cp = 0;
        return 0;
    }
    /* now read the term itself */

    prt->last_cp = cp;
    wrbuf_rewind(prt->norm_str);
    while (*cp && (c = raw_char(tolower(*cp))) >= 0)
    {
        wrbuf_putc(prt->norm_str, c);
        cp++;
    }
    prt->cp = cp;
    return wrbuf_cstr(prt->norm_str);
}

static const char *pp2_get_sort_ascii(pp2_relevance_token_t prt,
                                    int skip_article)
{
    if (prt->last_cp == 0)
        return 0;
    else
    {
        char *tmp = xstrdup(prt->last_cp);
        char *result = 0;
        result = normalize7bit_mergekey(tmp, skip_article);
        
        wrbuf_rewind(prt->sort_str);
        wrbuf_puts(prt->sort_str, result);
        xfree(tmp);
        return wrbuf_cstr(prt->sort_str);
    }
}


#ifdef HAVE_ICU
static const char *pp2_relevance_token_icu(pp2_relevance_token_t prt)
{
    if (icu_chain_next_token(prt->pct->icu_chn, &prt->pct->icu_sts))
    {
        if (U_FAILURE(prt->pct->icu_sts))
        {
            return 0;
        }
        return icu_chain_get_norm(prt->pct->icu_chn);
    }
    return 0;
}

static const char *pp2_get_sort_icu(pp2_relevance_token_t prt,
                                    int skip_article)
{
    return icu_chain_get_sort(prt->pct->icu_chn);
}

#endif // HAVE_ICU



/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
