/* This file is part of Pazpar2.
   Copyright (C) 2006-2012 Index Data

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
#include <yaz/yaz-version.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

#include "charsets.h"
#include "normalize7bit.h"

#if YAZ_HAVE_ICU
#include <yaz/icu.h>
#endif

typedef struct pp2_charset_s *pp2_charset_t;
static pp2_charset_t pp2_charset_create_xml(xmlNode *xml_node);
static pp2_charset_t pp2_charset_create(struct icu_chain * icu_chn);
static pp2_charset_t pp2_charset_create_a_to_z(void);
static void pp2_charset_destroy(pp2_charset_t pct);
static pp2_charset_token_t pp2_charset_tokenize(pp2_charset_t pct);

/* charset handle */
struct pp2_charset_s {
    const char *(*token_next_handler)(pp2_charset_token_t prt);
    const char *(*get_sort_handler)(pp2_charset_token_t prt);
    const char *(*get_display_handler)(pp2_charset_token_t prt);
#if YAZ_HAVE_ICU
    struct icu_chain * icu_chn;
    UErrorCode icu_sts;
#endif
};

static const char *pp2_charset_token_null(pp2_charset_token_t prt);
static const char *pp2_charset_token_a_to_z(pp2_charset_token_t prt);
static const char *pp2_get_sort_ascii(pp2_charset_token_t prt);
static const char *pp2_get_display_ascii(pp2_charset_token_t prt);

#if YAZ_HAVE_ICU
static const char *pp2_charset_token_icu(pp2_charset_token_t prt);
static const char *pp2_get_sort_icu(pp2_charset_token_t prt);
static const char *pp2_get_display_icu(pp2_charset_token_t prt);
#endif

/* tokenzier handle */
struct pp2_charset_token_s {
    const char *cp;     /* unnormalized buffer we're tokenizing */
    const char *last_cp;  /* pointer to last token we're dealing with */
    pp2_charset_t pct;  /* our main charset handle (type+config) */
    WRBUF norm_str;     /* normized string we return (temporarily) */
    WRBUF sort_str;     /* sort string we return (temporarily) */
#if YAZ_HAVE_ICU
    yaz_icu_iter_t iter;
#endif
};

struct pp2_charset_fact_s {
    struct pp2_charset_entry *list;
    int ref_count;
};

struct pp2_charset_entry {
    struct pp2_charset_entry *next;
    pp2_charset_t pct;
    char *name;
};


static int pp2_charset_fact_add(pp2_charset_fact_t pft,
                                pp2_charset_t pct, const char *default_id);

pp2_charset_fact_t pp2_charset_fact_create(void)
{
    pp2_charset_fact_t pft = xmalloc(sizeof(*pft));
    pft->list = 0;
    pft->ref_count = 1;

    pp2_charset_fact_add(pft, pp2_charset_create_a_to_z(), "relevance");
    pp2_charset_fact_add(pft, pp2_charset_create_a_to_z(), "sort");
    pp2_charset_fact_add(pft, pp2_charset_create_a_to_z(), "mergekey");
    pp2_charset_fact_add(pft, pp2_charset_create(0), "facet");
    return pft;
}

void pp2_charset_fact_destroy(pp2_charset_fact_t pft)
{
    if (pft)
    {
        assert(pft->ref_count >= 1);
        --(pft->ref_count);
        if (pft->ref_count == 0)
        {
            struct pp2_charset_entry *pce = pft->list;
            while (pce)
            {
                struct pp2_charset_entry *next = pce->next;
                pp2_charset_destroy(pce->pct);
                xfree(pce->name);
                xfree(pce);
                pce = next;
            }
            xfree(pft);
        }
    }
}

int pp2_charset_fact_add(pp2_charset_fact_t pft,
                         pp2_charset_t pct, const char *default_id)
{
    struct pp2_charset_entry *pce;

    for (pce = pft->list; pce; pce = pce->next)
        if (!strcmp(default_id, pce->name))
            break;

    if (!pce)
    {
        pce = xmalloc(sizeof(*pce));
        pce->name = xstrdup(default_id);
        pce->next = pft->list;
        pft->list = pce;
    }
    else
    {
        pp2_charset_destroy(pce->pct);
    }
    pce->pct = pct;
    return 0;
}

int pp2_charset_fact_define(pp2_charset_fact_t pft,
                            xmlNode *xml_node, const char *default_id)
{
    int r;
    pp2_charset_t pct;
    xmlChar *id = 0;

    assert(xml_node);
    pct = pp2_charset_create_xml(xml_node);
    if (!pct)
        return -1;
    if (!default_id)
    {
        id = xmlGetProp(xml_node, (xmlChar*) "id");
        if (!id)
        {
            yaz_log(YLOG_WARN, "Missing id for icu_chain");
            pp2_charset_destroy(pct);
            return -1;
        }
        default_id = (const char *) id;
    }
    r = pp2_charset_fact_add(pft, pct, default_id);
    if (id)
        xmlFree(id);
    return r;
}

void pp2_charset_fact_incref(pp2_charset_fact_t pft)
{
    (pft->ref_count)++;
}

pp2_charset_t pp2_charset_create_xml(xmlNode *xml_node)
{
#if YAZ_HAVE_ICU
    UErrorCode status = U_ZERO_ERROR;
    struct icu_chain *chain = 0;
    while (xml_node && xml_node->type != XML_ELEMENT_NODE)
        xml_node = xml_node->next;
    chain = icu_chain_xml_config(xml_node, 1, &status);
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
#else // YAZ_HAVE_ICU
    yaz_log(YLOG_FATAL, "Error: ICU support requested with element:\n"
            "<%s>\n ... \n</%s>",
            xml_node->name, xml_node->name);
    yaz_log(YLOG_FATAL, 
            "But no ICU support is compiled into the YAZ library.");
    return 0;
#endif // YAZ_HAVE_ICU
}

pp2_charset_t pp2_charset_create_a_to_z(void)
{
    pp2_charset_t pct = pp2_charset_create(0);
    pct->token_next_handler = pp2_charset_token_a_to_z;
    return pct;
}

pp2_charset_t pp2_charset_create(struct icu_chain *icu_chn)
{
    pp2_charset_t pct = xmalloc(sizeof(*pct));

    pct->token_next_handler = pp2_charset_token_null;
    pct->get_sort_handler  = pp2_get_sort_ascii;
    pct->get_display_handler  = pp2_get_display_ascii;
#if YAZ_HAVE_ICU
    pct->icu_chn = 0;
    if (icu_chn)
    {
        pct->icu_chn = icu_chn;
        pct->icu_sts = U_ZERO_ERROR;
        pct->token_next_handler = pp2_charset_token_icu;
        pct->get_sort_handler = pp2_get_sort_icu;
        pct->get_display_handler = pp2_get_display_icu;
    }
#endif // YAZ_HAVE_ICU
    return pct;
}

void pp2_charset_destroy(pp2_charset_t pct)
{
#if YAZ_HAVE_ICU
    icu_chain_destroy(pct->icu_chn);
#endif
    xfree(pct);
}

pp2_charset_token_t pp2_charset_token_create(pp2_charset_fact_t pft,
                                               const char *id)
{
    struct pp2_charset_entry *pce;
    for (pce = pft->list; pce; pce = pce->next)
        if (!strcmp(id, pce->name))
            return pp2_charset_tokenize(pce->pct);
    return 0;
}

pp2_charset_token_t pp2_charset_tokenize(pp2_charset_t pct)
{
    pp2_charset_token_t prt = xmalloc(sizeof(*prt));

    assert(pct);

    prt->norm_str = wrbuf_alloc();
    prt->sort_str = wrbuf_alloc();
    prt->cp = 0;
    prt->last_cp = 0;
    prt->pct = pct;

#if YAZ_HAVE_ICU
    prt->iter = 0;
    if (pct->icu_chn)
        prt->iter = icu_iter_create(pct->icu_chn);
#endif
    return prt;
}

void pp2_charset_token_first(pp2_charset_token_t prt,
                             const char *buf, int skip_article)
{ 
    if (skip_article)
    {
        const char *p = buf;
        char firstword[64];
        char *pout = firstword;
        char articles[] = "the den der die des an a "; // must end in space
        
        for (; *p && *p != ' ' && pout - firstword < (sizeof(firstword)-2); p++)
            *pout++ = tolower(*(unsigned char *)p);
        *pout++ = ' ';
        *pout++ = '\0';
        if (strstr(articles, firstword))
            buf = p;
    }

    wrbuf_rewind(prt->norm_str);
    wrbuf_rewind(prt->sort_str);
    prt->cp = buf;
    prt->last_cp = 0;

#if YAZ_HAVE_ICU
    if (prt->iter)
    {
        icu_iter_first(prt->iter, buf);
    }
#endif // YAZ_HAVE_ICU
}

void pp2_charset_token_destroy(pp2_charset_token_t prt)
{
    assert(prt);
#if YAZ_HAVE_ICU
    if (prt->iter)
        icu_iter_destroy(prt->iter);
#endif
    if(prt->norm_str) 
        wrbuf_destroy(prt->norm_str);
    if(prt->sort_str) 
        wrbuf_destroy(prt->sort_str);
    xfree(prt);
}

const char *pp2_charset_token_next(pp2_charset_token_t prt)
{
    assert(prt);
    return (prt->pct->token_next_handler)(prt);
}

const char *pp2_get_sort(pp2_charset_token_t prt)
{
    return prt->pct->get_sort_handler(prt);
}

const char *pp2_get_display(pp2_charset_token_t prt)
{
    return prt->pct->get_display_handler(prt);
}

#define raw_char(c) (((c) >= 'a' && (c) <= 'z') ? (c) : -1)
/* original tokenizer with our tokenize interface, but we
   add +1 to ensure no '\0' are in our string (except for EOF)
*/
static const char *pp2_charset_token_a_to_z(pp2_charset_token_t prt)
{
    const char *cp = prt->cp;
    int c;

    /* skip white space */
    while (*cp && (c = raw_char(tolower(*(const unsigned char *)cp))) < 0)
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

static const char *pp2_get_sort_ascii(pp2_charset_token_t prt)
{
    if (prt->last_cp == 0)
        return 0;
    else
    {
        char *tmp = xstrdup(prt->last_cp);
        char *result = 0;
        result = normalize7bit_mergekey(tmp);
        
        wrbuf_rewind(prt->sort_str);
        wrbuf_puts(prt->sort_str, result);
        xfree(tmp);
        return wrbuf_cstr(prt->sort_str);
    }
}

static const char *pp2_get_display_ascii(pp2_charset_token_t prt)
{
    if (prt->last_cp == 0)
        return 0;
    else
    {
        return wrbuf_cstr(prt->norm_str);
    }
}

static const char *pp2_charset_token_null(pp2_charset_token_t prt)
{
    const char *cp = prt->cp;

    prt->last_cp = *cp ? cp : 0;
    while (*cp)
        cp++;
    prt->cp = cp;
    return prt->last_cp;
}

#if YAZ_HAVE_ICU
static const char *pp2_charset_token_icu(pp2_charset_token_t prt)
{
    if (icu_iter_next(prt->iter))
    {
        return icu_iter_get_norm(prt->iter);
    }
    return 0;
}

static const char *pp2_get_sort_icu(pp2_charset_token_t prt)
{
    return icu_iter_get_sortkey(prt->iter);
}

static const char *pp2_get_display_icu(pp2_charset_token_t prt)
{
    return icu_iter_get_display(prt->iter);
}

#endif // YAZ_HAVE_ICU


/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

