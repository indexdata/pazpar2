/* $Id: config.c,v 1.25 2007-04-19 11:57:53 marc Exp $
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

/* $Id: config.c,v 1.25 2007-04-19 11:57:53 marc Exp $ */

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

struct conf_service * conf_service_create(NMEM nmem)
{
    struct conf_service * service
        = nmem_malloc(nmem, sizeof(struct conf_service));
    service->num_metadata = 0;
    service->metadata = 0;
    service->num_sortkeys = 0;
    service->sortkeys = 0;
    return service; 
}


struct conf_metadata * conf_metadata_create(NMEM nmem, 
                                            const char *name,
                                            enum conf_metadata_type type,
                                            enum conf_metadata_merge merge,
                                            int brief,
                                            int termlist,
                                            int rank,
                                            int sortkey_offset)
{

    struct conf_metadata * metadata
        = nmem_malloc(nmem, sizeof(struct conf_metadata));

    metadata->name = nmem_strdup(nmem, name);
    metadata->type = type;
    metadata->merge = merge;
    metadata->brief = brief;   
    metadata->termlist = termlist;
    metadata->rank = rank;    
    metadata->sortkey_offset = sortkey_offset;
    return metadata;
}

struct conf_metadata* conf_service_add_metadata(NMEM nmem,
                                                struct conf_service *service,
                                                const char *name,
                                                enum conf_metadata_type type,
                                                enum conf_metadata_merge merge,
                                                int brief,
                                                int termlist,
                                                int rank,
                                                int sortkey_offset)
{
    struct conf_metadata * m = 0;

    if (!service)
        return m;

    m = conf_metadata_create(nmem, name, type, merge, 
                             brief, termlist, rank, sortkey_offset);

    // Not finished, checked temporarily in for file move  // if (m)

    return m;
}




/* Code to parse configuration file */
/* ==================================================== */

static struct conf_service *parse_service(xmlNode *node)
{
    xmlNode *n;
    struct conf_service *r = nmem_malloc(nmem, sizeof(struct conf_service));
    int md_node = 0;
    int sk_node = 0;

    r->num_sortkeys = r->num_metadata = 0;
    // Allocate array of conf metadata and sortkey tructs, if necessary
    for (n = node->children; n; n = n->next)
        if (n->type == XML_ELEMENT_NODE && !strcmp((const char *)
                                                   n->name, "metadata"))
        {
            xmlChar *sortkey = xmlGetProp(n, (xmlChar *) "sortkey");
            r->num_metadata++;
            if (sortkey && strcmp((const char *) sortkey, "no"))
                r->num_sortkeys++;
            xmlFree(sortkey);
        }
    if (r->num_metadata)
        r->metadata = nmem_malloc(nmem, sizeof(struct conf_metadata) * r->num_metadata);
    else
        r->metadata = 0;
    if (r->num_sortkeys)
        r->sortkeys = nmem_malloc(nmem, sizeof(struct conf_sortkey) * r->num_sortkeys);
    else
        r->sortkeys = 0;

    for (n = node->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp((const char *) n->name, (const char *) "metadata"))
        {
            struct conf_metadata *md = &r->metadata[md_node];
            xmlChar *name = xmlGetProp(n, (xmlChar *) "name");
            xmlChar *brief = xmlGetProp(n, (xmlChar *) "brief");
            xmlChar *sortkey = xmlGetProp(n, (xmlChar *) "sortkey");
            xmlChar *merge = xmlGetProp(n, (xmlChar *) "merge");
            xmlChar *type = xmlGetProp(n, (xmlChar *) "type");
            xmlChar *termlist = xmlGetProp(n, (xmlChar *) "termlist");
            xmlChar *rank = xmlGetProp(n, (xmlChar *) "rank");

            if (!name)
            {
                yaz_log(YLOG_FATAL, "Must specify name in metadata element");
                return 0;
            }
            md->name = nmem_strdup(nmem, (const char *) name);
            if (brief)
            {
                if (!strcmp((const char *) brief, "yes"))
                    md->brief = 1;
                else if (strcmp((const char *) brief, "no"))
                {
                    yaz_log(YLOG_FATAL, "metadata/brief must be yes or no");
                    return 0;
                }
            }
            else
                md->brief = 0;

            if (termlist)
            {
                if (!strcmp((const char *) termlist, "yes"))
                    md->termlist = 1;
                else if (strcmp((const char *) termlist, "no"))
                {
                    yaz_log(YLOG_FATAL, "metadata/termlist must be yes or no");
                    return 0;
                }
            }
            else
                md->termlist = 0;

            if (rank)
                md->rank = atoi((const char *) rank);
            else
                md->rank = 0;

            if (type)
            {
                if (!strcmp((const char *) type, "generic"))
                    md->type = Metadata_type_generic;
                else if (!strcmp((const char *) type, "year"))
                    md->type = Metadata_type_year;
                else
                {
                    yaz_log(YLOG_FATAL, "Unknown value for metadata/type: %s", type);
                    return 0;
                }
            }
            else
                md->type = Metadata_type_generic;

            if (merge)
            {
                if (!strcmp((const char *) merge, "no"))
                    md->merge = Metadata_merge_no;
                else if (!strcmp((const char *) merge, "unique"))
                    md->merge = Metadata_merge_unique;
                else if (!strcmp((const char *) merge, "longest"))
                    md->merge = Metadata_merge_longest;
                else if (!strcmp((const char *) merge, "range"))
                    md->merge = Metadata_merge_range;
                else if (!strcmp((const char *) merge, "all"))
                    md->merge = Metadata_merge_all;
                else
                {
                    yaz_log(YLOG_FATAL, "Unknown value for metadata/merge: %s", merge);
                    return 0;
                }
            }
            else
                md->merge = Metadata_merge_no;

            if (sortkey && strcmp((const char *) sortkey, "no"))
            {
                struct conf_sortkey *sk = &r->sortkeys[sk_node];
                if (md->merge == Metadata_merge_no)
                {
                    yaz_log(YLOG_FATAL, "Can't specify sortkey on a non-merged field");
                    return 0;
                }
                if (!strcmp((const char *) sortkey, "numeric"))
                    sk->type = Metadata_sortkey_numeric;
                else if (!strcmp((const char *) sortkey, "skiparticle"))
                    sk->type = Metadata_sortkey_skiparticle;
                else
                {
                    yaz_log(YLOG_FATAL, "Unknown sortkey in metadata element: %s", sortkey);
                    return 0;
                }
                sk->name = md->name;
                md->sortkey_offset = sk_node;
                sk_node++;
            }
            else
                md->sortkey_offset = -1;

            xmlFree(name);
            xmlFree(brief);
            xmlFree(sortkey);
            xmlFree(merge);
            xmlFree(type);
            xmlFree(termlist);
            xmlFree(rank);
            md_node++;
        }
        else
        {
            yaz_log(YLOG_FATAL, "Bad element: %s", n->name);
            return 0;
        }
    }
    return r;
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
    struct conf_server *r = nmem_malloc(nmem, sizeof(struct conf_server));

    r->host = 0;
    r->port = 0;
    r->proxy_host = 0;
    r->proxy_port = 0;
    r->myurl = 0;
    r->zproxy_host = 0;
    r->zproxy_port = 0;
    r->service = 0;
    r->next = 0;
    r->settings = 0;

    for (n = node->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp((const char *) n->name, "listen"))
        {
            xmlChar *port = xmlGetProp(n, (xmlChar *) "port");
            xmlChar *host = xmlGetProp(n, (xmlChar *) "host");
            if (port)
                r->port = atoi((const char *) port);
            if (host)
                r->host = nmem_strdup(nmem, (const char *) host);
            xmlFree(port);
            xmlFree(host);
        }
        else if (!strcmp((const char *) n->name, "proxy"))
        {
            xmlChar *port = xmlGetProp(n, (xmlChar *) "port");
            xmlChar *host = xmlGetProp(n, (xmlChar *) "host");
            xmlChar *myurl = xmlGetProp(n, (xmlChar *) "myurl");
            if (port)
                r->proxy_port = atoi((const char *) port);
            if (host)
                r->proxy_host = nmem_strdup(nmem, (const char *) host);
            if (myurl)
                r->myurl = nmem_strdup(nmem, (const char *) myurl);
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
        else if (!strcmp((const char *) n->name, "zproxy"))
        {
            xmlChar *port = 0;
            xmlChar *host = 0;

            port = xmlGetProp(n, (xmlChar *) "port");
            host = xmlGetProp(n, (xmlChar *) "host");

            if (port)
                r->zproxy_port = atoi((const char *) port);
            if (host)
                r->zproxy_host = nmem_strdup(nmem, (const char *) host);

            xmlFree(port);
            xmlFree(host);
        }
        else if (!strcmp((const char *) n->name, "settings"))
        {
            if (r->settings)
            {
                yaz_log(YLOG_FATAL, "Can't repeat 'settings'");
                return 0;
            }
            if (!(r->settings = parse_settings(n)))
                return 0;
        }
        else if (!strcmp((const char *) n->name, "service"))
        {
            struct conf_service *s = parse_service(n);
            if (!s)
                return 0;
            r->service = s;
        }
        else
        {
            yaz_log(YLOG_FATAL, "Bad element: %s", n->name);
            return 0;
        }
    }
    return r;
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
