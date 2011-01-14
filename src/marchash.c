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

/** \file 
    \brief MARC MAP utilities (hash lookup etc)
*/

#if HAVE_CONFIG_H
#include <config.h>
#else
/* disable inline if AC_C_INLINE is not in use (Windows) */
#define inline
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <yaz/nmem.h>

#include "jenkins_hash.h"
#include "marchash.h"

static inline void strtrimcat(char *dest, const char *src)
{
    const char *in;
    char *out;
    char *last_nonspace;
    in = src;
    out = dest;
    // move to end of dest
    while (*out)
        out++;
    // initialise last non-space charater
    last_nonspace = out;
    // skip leading whitespace
    while (isspace(*in))
    	in++;
    while (*in)
    {
        *out = *in;
        if (!isspace(*in))
            last_nonspace = out;
        out++;
        in++;
    }
    *(++last_nonspace) = '\0';
}

static inline void strtrimcpy(char *dest, const char *src)
{
    *dest = '\0';
    strtrimcat(dest, src);
}

struct marchash *marchash_create(NMEM nmem)
{
    struct marchash *new;
    new = nmem_malloc(nmem, sizeof (struct marchash));
    memset(new, 0, sizeof (struct marchash));
    new->nmem = nmem;
    return new;
}

void marchash_ingest_marcxml(struct marchash *marchash, xmlNodePtr rec_node)
{
     xmlNodePtr field_node;
     xmlNodePtr sub_node;
     struct marcfield *field;
     field_node = rec_node->children;

     while (field_node)
     {
         if (field_node->type == XML_ELEMENT_NODE)
         {
             field = NULL;
             if (!strcmp((const char *) field_node->name, "controlfield"))
             {
                 xmlChar *content = xmlNodeGetContent(field_node);
                 xmlChar *tag = xmlGetProp(field_node, BAD_CAST "tag");
                 if (tag && content)
                     field = marchash_add_field(
                         marchash, (const char *) tag, (const char *) content);
                 xmlFree(content);
                 xmlFree(tag);
             }
             else if (!strcmp((const char *) field_node->name, "datafield"))
             {
                 xmlChar *content = xmlNodeGetContent(field_node);
                 xmlChar *tag = xmlGetProp(field_node, BAD_CAST "tag");
                 if (tag && content)
                     field = marchash_add_field(
                         marchash, (const char *) tag, (const char *) content);
                 xmlFree(content);
                 xmlFree(tag);
             }
             if (field)
             {
                 sub_node = field_node->children;
                 while (sub_node) 
                 {
                     if ((sub_node->type == XML_ELEMENT_NODE) &&
                         !strcmp((const char *) sub_node->name, "subfield"))
                     {
                         xmlChar *content = xmlNodeGetContent(sub_node);
                         xmlChar *code = xmlGetProp(sub_node, BAD_CAST "code");
                         if (code && content)
                             marchash_add_subfield(
                                 marchash, field,
                                 code[0], (const char *) content);
                         xmlFree(content);
                         xmlFree(code);
                     }
                     sub_node = sub_node->next;
                 } 
             }
         }
         field_node = field_node->next;
     }
}

struct marcfield *marchash_add_field(struct marchash *marchash,
                                     const char *key, const char *val)
{
    int slot;
    struct marcfield *new;
    struct marcfield *last;
    
    slot = jenkins_hash((const unsigned char *) key) & MARCHASH_MASK;
    new = marchash->table[slot];
    last = NULL;
    
    while (new) 
    {
        last = new; 
        new = new->next;     
    }

    new = nmem_malloc(marchash->nmem, sizeof (struct marcfield));

    if (last)
        last->next = new;
    else
        marchash->table[slot] = new;

    new->next = NULL;
    new->subfields = NULL;
    strncpy(new->key, key, 4);
    
    // only 3 char in a marc field name 
    if (new->key[3] != '\0')
        return 0;

    new->val = nmem_malloc(marchash->nmem, sizeof (char) * strlen(val) + 1);
    strtrimcpy(new->val, val);

    return new;
}

struct marcsubfield *marchash_add_subfield(struct marchash *marchash,
                                           struct marcfield *field,
                                           const char key, const char *val)
{
    struct marcsubfield *new;
    struct marcsubfield *last;
    last = NULL;
    new = field->subfields;

    while (new)
    {
        last = new;
        new = new->next;
    }

    new = nmem_malloc(marchash->nmem, sizeof (struct marcsubfield));

    if (last)
        last->next = new;
    else
        field->subfields = new;

    new->next = NULL;
    new->key = key;
    new->val = nmem_malloc(marchash->nmem, sizeof (char) * strlen(val) + 1);
    strcpy(new->val, val);
    return new;
}

struct marcfield *marchash_get_field (struct marchash *marchash,
                                      const char *key, struct marcfield *last)
{
    struct marcfield *cur;
    if (last)
        cur = last->next;
    else 
        cur = marchash->table[jenkins_hash((const unsigned char *)key) & MARCHASH_MASK];
    while (cur)
    {
        if (!strcmp(cur->key, key))
            return cur;
        cur = cur->next;
    }
    return NULL;
}

struct marcsubfield *marchash_get_subfield(char key,
                                           struct marcfield *field,
                                           struct marcsubfield *last)
{
    struct marcsubfield *cur;
    if (last)
        cur = last->next;
    else
        cur = field->subfields;
    while (cur)
    {
        if (cur->key == key)
          return cur;
        cur = cur->next;
    }
    return NULL;
}

char *marchash_catenate_subfields(struct marcfield *field,
                                  const char *delim, NMEM nmem)
{
    char *output;
    struct marcsubfield *cur;
    int delimsize = strlen(delim);
    int outsize = 1-delimsize;
    // maybe it would make sense to have an nmem strcpy/strcat?
    cur = field -> subfields;
    while (cur)
    {
        outsize += strlen(cur->val) + delimsize;
        cur = cur->next;
    }  
    if (outsize > 0)
        output = nmem_malloc(nmem, outsize); 
    else
        return NULL;
    *output = '\0';
    cur = field -> subfields;
    while (cur)
    {
        strtrimcat(output, cur->val);
        if (cur->next)
            strcat(output, delim); 
        cur = cur->next;
    } 
    return output;
}
/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
