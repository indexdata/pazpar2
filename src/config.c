/* $Id: config.c,v 1.36 2007-06-06 11:56:35 marc Exp $
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

/* $Id: config.c,v 1.36 2007-06-06 11:56:35 marc Exp $ */

#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxslt/xslt.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

#if HAVE_CONFIG_H
#include <cconfig.h>
#endif

#include <yaz/yaz-util.h>
#include <yaz/nmem.h>

#define CONFIG_NOEXTERNS
#include "config.h"


static NMEM nmem = 0;
static char confdir[256] = ".";

struct conf_config *config = 0;


struct conf_metadata * conf_metadata_assign(NMEM nmem, 
                                            struct conf_metadata * metadata,
                                            const char *name,
                                            enum conf_metadata_type type,
                                            enum conf_metadata_merge merge,
                                            int brief,
                                            int termlist,
                                            int rank,
                                            int sortkey_offset)
{
    if (!nmem || !metadata || !name)
        return 0;
    
    metadata->name = nmem_strdup(nmem, name);

    // enforcing that merge_range is always type_year 
    if (merge == Metadata_merge_range)
        metadata->type = Metadata_type_year;
    else
        metadata->type = type;

    // enforcing that type_year is always range_merge
    if (metadata->type == Metadata_type_year)
        metadata->merge = Metadata_merge_range;
    else
        metadata->merge = merge;    

    metadata->brief = brief;   
    metadata->termlist = termlist;
    metadata->rank = rank;    
    metadata->sortkey_offset = sortkey_offset;
    return metadata;
}


struct conf_sortkey * conf_sortkey_assign(NMEM nmem, 
                                          struct conf_sortkey * sortkey,
                                          const char *name,
                                          enum conf_sortkey_type type)
{
    if (!nmem || !sortkey || !name)
        return 0;
    
    sortkey->name = nmem_strdup(nmem, name);
    sortkey->type = type;

    return sortkey;
}


struct conf_service * conf_service_create(NMEM nmem,
                                          int num_metadata, int num_sortkeys)
{
    struct conf_service * service = 0;

    //assert(nmem);
    
    service = nmem_malloc(nmem, sizeof(struct conf_service));

    service->num_metadata = num_metadata;
    service->metadata = 0;
    if (service->num_metadata)
      service->metadata 
          = nmem_malloc(nmem, 
                        sizeof(struct conf_metadata) * service->num_metadata);
    service->num_sortkeys = num_sortkeys;
    service->sortkeys = 0;
    if (service->num_sortkeys)
        service->sortkeys 
            = nmem_malloc(nmem, 
                          sizeof(struct conf_sortkey) * service->num_sortkeys);

    return service; 
}

struct conf_metadata* conf_service_add_metadata(NMEM nmem, 
                                                struct conf_service *service,
                                                int field_id,
                                                const char *name,
                                                enum conf_metadata_type type,
                                                enum conf_metadata_merge merge,
                                                int brief,
                                                int termlist,
                                                int rank,
                                                int sortkey_offset)
{
    struct conf_metadata * md = 0;

    if (!service || !service->metadata || !service->num_metadata
        || field_id < 0  || !(field_id < service->num_metadata))
        return 0;

    //md = &((service->metadata)[field_id]);
    md = service->metadata + field_id;
    md = conf_metadata_assign(nmem, md, name, type, merge, 
                             brief, termlist, rank, sortkey_offset);
    return md;
}


struct conf_sortkey * conf_service_add_sortkey(NMEM nmem,
                                               struct conf_service *service,
                                               int field_id,
                                               const char *name,
                                               enum conf_sortkey_type type)
{
    struct conf_sortkey * sk = 0;

    if (!service || !service->sortkeys || !service->num_sortkeys
        || field_id < 0  || !(field_id < service->num_sortkeys))
        return 0;

    //sk = &((service->sortkeys)[field_id]);
    sk = service->sortkeys + field_id;
    sk = conf_sortkey_assign(nmem, sk, name, type);

    return sk;
}


int conf_service_metadata_field_id(struct conf_service *service,
                                   const char * name)
{
    int i = 0;

    if (!service || !service->metadata || !service->num_metadata)
        return -1;

    for(i = 0; i < service->num_metadata; i++) {
        if (!strcmp(name, (service->metadata[i]).name))
            return i;
    }
   
    return -1;
};


int conf_service_sortkey_field_id(struct conf_service *service,
                                  const char * name)
{
    int i = 0;

    if (!service || !service->sortkeys || !service->num_sortkeys)
        return -1;

    for(i = 0; i < service->num_sortkeys; i++) {
        if (!strcmp(name, (service->sortkeys[i]).name))
            return i;
    }
   
    return -1;
};



/* Code to parse configuration file */
/* ==================================================== */

static struct conf_service *parse_service(xmlNode *node)
{
    xmlNode *n;
    int md_node = 0;
    int sk_node = 0;

    struct conf_service *service = 0;
    int num_metadata = 0;
    int num_sortkeys = 0;
    
    // count num_metadata and num_sortkeys
    for (n = node->children; n; n = n->next)
        if (n->type == XML_ELEMENT_NODE && !strcmp((const char *)
                                                   n->name, "metadata"))
        {
            xmlChar *sortkey = xmlGetProp(n, (xmlChar *) "sortkey");
            num_metadata++;
            if (sortkey && strcmp((const char *) sortkey, "no"))
                num_sortkeys++;
            xmlFree(sortkey);
        }

    service = conf_service_create(nmem, num_metadata, num_sortkeys);    

    for (n = node->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp((const char *) n->name, (const char *) "metadata"))
        {
            xmlChar *xml_name = xmlGetProp(n, (xmlChar *) "name");
            xmlChar *xml_brief = xmlGetProp(n, (xmlChar *) "brief");
            xmlChar *xml_sortkey = xmlGetProp(n, (xmlChar *) "sortkey");
            xmlChar *xml_merge = xmlGetProp(n, (xmlChar *) "merge");
            xmlChar *xml_type = xmlGetProp(n, (xmlChar *) "type");
            xmlChar *xml_termlist = xmlGetProp(n, (xmlChar *) "termlist");
            xmlChar *xml_rank = xmlGetProp(n, (xmlChar *) "rank");

            enum conf_metadata_type type = Metadata_type_generic;
            enum conf_metadata_merge merge = Metadata_merge_no;
            int brief = 0;
            int termlist = 0;
            int rank = 0;
            int sortkey_offset = 0;
            enum conf_sortkey_type sk_type = Metadata_sortkey_relevance;
            
            // now do the parsing logic
            if (!xml_name)
            {
                yaz_log(YLOG_FATAL, "Must specify name in metadata element");
                return 0;
            }
            if (xml_brief)
            {
                if (!strcmp((const char *) xml_brief, "yes"))
                    brief = 1;
                 else if (strcmp((const char *) xml_brief, "no"))
                {
                    yaz_log(YLOG_FATAL, "metadata/brief must be yes or no");
                    return 0;
                }
            }
            else
                brief = 0;

            if (xml_termlist)
            {
                if (!strcmp((const char *) xml_termlist, "yes"))
                    termlist = 1;
                else if (strcmp((const char *) xml_termlist, "no"))
                {
                    yaz_log(YLOG_FATAL, "metadata/termlist must be yes or no");
                    return 0;
                }
            }
            else
                termlist = 0;

            if (xml_rank)
                rank = atoi((const char *) xml_rank);
            else
                rank = 0;

            if (xml_type)
            {
                if (!strcmp((const char *) xml_type, "generic"))
                    type = Metadata_type_generic;
                else if (!strcmp((const char *) xml_type, "year"))
                    type = Metadata_type_year;
                else
                {
                    yaz_log(YLOG_FATAL, 
                            "Unknown value for metadata/type: %s", xml_type);
                    return 0;
                }
            }
            else
                type = Metadata_type_generic;

            if (xml_merge)
            {
                if (!strcmp((const char *) xml_merge, "no"))
                    merge = Metadata_merge_no;
                else if (!strcmp((const char *) xml_merge, "unique"))
                    merge = Metadata_merge_unique;
                else if (!strcmp((const char *) xml_merge, "longest"))
                    merge = Metadata_merge_longest;
                else if (!strcmp((const char *) xml_merge, "range"))
                    merge = Metadata_merge_range;
                else if (!strcmp((const char *) xml_merge, "all"))
                    merge = Metadata_merge_all;
                else
                {
                    yaz_log(YLOG_FATAL, 
                            "Unknown value for metadata/merge: %s", xml_merge);
                    return 0;
                }
            }
            else
                merge = Metadata_merge_no;

            // add a sortkey if so specified
            if (xml_sortkey && strcmp((const char *) xml_sortkey, "no"))
            {
                if (merge == Metadata_merge_no)
                {
                    yaz_log(YLOG_FATAL, 
                            "Can't specify sortkey on a non-merged field");
                    return 0;
                }
                if (!strcmp((const char *) xml_sortkey, "numeric"))
                    sk_type = Metadata_sortkey_numeric;
                else if (!strcmp((const char *) xml_sortkey, "skiparticle"))
                    sk_type = Metadata_sortkey_skiparticle;
                else
                {
                    yaz_log(YLOG_FATAL,
                            "Unknown sortkey in metadata element: %s", 
                            xml_sortkey);
                    return 0;
                }
                sortkey_offset = sk_node;

                conf_service_add_sortkey(nmem, service, sk_node,
                                         (const char *) xml_name, sk_type);
                
                sk_node++;
            }
            else
                sortkey_offset = -1;

            // metadata known, assign values
            conf_service_add_metadata(nmem, service, md_node,
                                      (const char *) xml_name,
                                      type, merge,
                                      brief, termlist, rank, sortkey_offset);

            xmlFree(xml_name);
            xmlFree(xml_brief);
            xmlFree(xml_sortkey);
            xmlFree(xml_merge);
            xmlFree(xml_type);
            xmlFree(xml_termlist);
            xmlFree(xml_rank);
            md_node++;
        }
        else
        {
            yaz_log(YLOG_FATAL, "Bad element: %s", n->name);
            return 0;
        }
    }
    return service;
}

static char *parse_settings(xmlNode *node)
{
    xmlChar *src = xmlGetProp(node, (xmlChar *) "src");
    char *r;

    if (src)
        r = nmem_strdup(nmem, (const char *) src);
    else
    {
        yaz_log(YLOG_FATAL, "Must specify src in targetprofile");
        return 0;
    }
    xmlFree(src);
    return r;
}

static struct conf_server *parse_server(xmlNode *node)
{
    xmlNode *n;
    struct conf_server *server = nmem_malloc(nmem, sizeof(struct conf_server));

    server->host = 0;
    server->port = 0;
    server->proxy_host = 0;
    server->proxy_port = 0;
    server->myurl = 0;
    //server->zproxy_host = 0;
    //server->zproxy_port = 0;
    server->service = 0;
    server->next = 0;
    server->settings = 0;

#ifdef HAVE_ICU
    server->icu_chn = 0;
#endif // HAVE_ICU


    for (n = node->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp((const char *) n->name, "listen"))
        {
            xmlChar *port = xmlGetProp(n, (xmlChar *) "port");
            xmlChar *host = xmlGetProp(n, (xmlChar *) "host");
            if (port)
                server->port = atoi((const char *) port);
            if (host)
                server->host = nmem_strdup(nmem, (const char *) host);
            xmlFree(port);
            xmlFree(host);
        }
        else if (!strcmp((const char *) n->name, "proxy"))
        {
            xmlChar *port = xmlGetProp(n, (xmlChar *) "port");
            xmlChar *host = xmlGetProp(n, (xmlChar *) "host");
            xmlChar *myurl = xmlGetProp(n, (xmlChar *) "myurl");
            if (port)
                server->proxy_port = atoi((const char *) port);
            if (host)
                server->proxy_host = nmem_strdup(nmem, (const char *) host);
            if (myurl)
                server->myurl = nmem_strdup(nmem, (const char *) myurl);
#ifdef GAGA
            else
            {
                yaz_log(YLOG_FATAL, "Must specify @myurl for proxy");
                return 0;
            }
#endif
            xmlFree(port);
            xmlFree(host);
            xmlFree(myurl);
        }
        else if (!strcmp((const char *) n->name, "settings"))
        {
            if (server->settings)
            {
                yaz_log(YLOG_FATAL, "Can't repeat 'settings'");
                return 0;
            }
            if (!(server->settings = parse_settings(n)))
                return 0;
        }
        else if (!strcmp((const char *) n->name, "icu_chain"))
        {
#ifdef HAVE_ICU
            UErrorCode status = U_ZERO_ERROR;
            struct icu_chain *chain = icu_chain_xml_config(n, &status);
            if (!chain || U_FAILURE(status)){
                //xmlDocPtr icu_doc = 0;
                //xmlChar *xmlstr = 0;
                //int size = 0;
                //xmlDocDumpMemory(icu_doc, size);
                
                yaz_log(YLOG_FATAL, "Could not parse ICU chain config:\n"
                        "<%s>\n ... \n</%s>",
                        n->name, n->name);
                return 0;
            }
            server->icu_chn = chain;
#else // HAVE_ICU
            yaz_log(YLOG_FATAL, "Error: ICU support requested with element:\n"
                    "<%s>\n ... \n</%s>",
                    n->name, n->name);
            yaz_log(YLOG_FATAL, 
                    "But no ICU support compiled into pazpar2 server.");
            yaz_log(YLOG_FATAL, 
                    "Please install libicu36-dev and icu-doc or similar, "
                    "re-configure and re-compile");            
            return 0;
#endif // HAVE_ICU
        }
        else if (!strcmp((const char *) n->name, "service"))
        {
            struct conf_service *s = parse_service(n);
            if (!s)
                return 0;
            server->service = s;
        }
        else
        {
            yaz_log(YLOG_FATAL, "Bad element: %s", n->name);
            return 0;
        }
    }
    return server;
}

xsltStylesheet *conf_load_stylesheet(const char *fname)
{
    char path[256];
    sprintf(path, "%s/%s", confdir, fname);
    return xsltParseStylesheetFile((xmlChar *) path);
}

static struct conf_targetprofiles *parse_targetprofiles(xmlNode *node)
{
    struct conf_targetprofiles *r = nmem_malloc(nmem, sizeof(*r));
    xmlChar *type = xmlGetProp(node, (xmlChar *) "type");
    xmlChar *src = xmlGetProp(node, (xmlChar *) "src");

    memset(r, 0, sizeof(*r));

    if (type)
    {
        if (!strcmp((const char *) type, "local"))
            r->type = Targetprofiles_local;
        else
        {
            yaz_log(YLOG_FATAL, "Unknown targetprofile type");
            return 0;
        }
    }
    else
    {
        yaz_log(YLOG_FATAL, "Must specify type for targetprofile");
        return 0;
    }

    if (src)
        r->src = nmem_strdup(nmem, (const char *) src);
    else
    {
        yaz_log(YLOG_FATAL, "Must specify src in targetprofile");
        return 0;
    }
    xmlFree(type);
    xmlFree(src);
    return r;
}

static struct conf_config *parse_config(xmlNode *root)
{
    xmlNode *n;
    struct conf_config *r = nmem_malloc(nmem, sizeof(struct conf_config));

    r->servers = 0;
    r->targetprofiles = 0;

    for (n = root->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp((const char *) n->name, "server"))
        {
            struct conf_server *tmp = parse_server(n);
            if (!tmp)
                return 0;
            tmp->next = r->servers;
            r->servers = tmp;
        }
        else if (!strcmp((const char *) n->name, "targetprofiles"))
        {
            // It would be fun to be able to fix this sometime
            if (r->targetprofiles)
            {
                yaz_log(YLOG_FATAL, "Can't repeat targetprofiles");
                return 0;
            }
            if (!(r->targetprofiles = parse_targetprofiles(n)))
                return 0;
        }
        else
        {
            yaz_log(YLOG_FATAL, "Bad element: %s", n->name);
            return 0;
        }
    }
    return r;
}

int read_config(const char *fname)
{
    xmlDoc *doc = xmlParseFile(fname);
    const char *p;

    if (!nmem)  // Initialize
    {
        nmem = nmem_create();
        xmlSubstituteEntitiesDefault(1);
        xmlLoadExtDtdDefaultValue = 1;
    }
    if (!doc)
    {
        yaz_log(YLOG_FATAL, "Failed to read %s", fname);
        exit(1);
    }
    if ((p = strrchr(fname, '/')))
    {
        int len = p - fname;
        strncpy(confdir, fname, len);
        confdir[len] = '\0';
    }
    config = parse_config(xmlDocGetRootElement(doc));
    xmlFreeDoc(doc);

    if (config)
        return 1;
    else
        return 0;
}


/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
