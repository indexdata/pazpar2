/* This file is part of Pazpar2.
   Copyright (C) 2006-2010 Index Data

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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <assert.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <yaz/yaz-util.h>
#include <yaz/nmem.h>
#include <yaz/snprintf.h>
#include <yaz/tpath.h>

#if HAVE_GLOB_H
#define USE_POSIX_GLOB 1
#else
#define USE_POSIX_GLOB 0
#endif


#if USE_POSIX_GLOB
#include <glob.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "pazpar2_config.h"
#include "settings.h"
#include "eventl.h"
#include "http.h"

struct conf_config
{
    NMEM nmem; /* for conf_config and servers memory */
    struct conf_server *servers;
    WRBUF confdir;
};


static char *parse_settings(struct conf_config *config,
                            NMEM nmem, xmlNode *node);

static struct conf_targetprofiles *parse_targetprofiles(NMEM nmem,
                                                        xmlNode *node);

static void conf_metadata_assign(NMEM nmem, 
                                 struct conf_metadata * metadata,
                                 const char *name,
                                 enum conf_metadata_type type,
                                 enum conf_metadata_merge merge,
                                 enum conf_setting_type setting,
                                 int brief,
                                 int termlist,
                                 int rank,
                                 int sortkey_offset,
                                 enum conf_metadata_mergekey mt)
{
    assert(nmem && metadata && name);
    
    metadata->name = nmem_strdup(nmem, name);

    metadata->type = type;

    // enforcing that type_year is always range_merge
    if (metadata->type == Metadata_type_year)
        metadata->merge = Metadata_merge_range;
    else
        metadata->merge = merge;    

    metadata->setting = setting;
    metadata->brief = brief;   
    metadata->termlist = termlist;
    metadata->rank = rank;    
    metadata->sortkey_offset = sortkey_offset;
    metadata->mergekey = mt;
}


static void conf_sortkey_assign(NMEM nmem, 
                                struct conf_sortkey * sortkey,
                                const char *name,
                                enum conf_sortkey_type type)
{
    assert(nmem && sortkey && name);
    
    sortkey->name = nmem_strdup(nmem, name);
    sortkey->type = type;
}


static struct conf_service *service_init(struct conf_server *server,
                                         int num_metadata, int num_sortkeys,
                                         const char *service_id)
{
    struct conf_service * service = 0;
    NMEM nmem = nmem_create();

    service = nmem_malloc(nmem, sizeof(struct conf_service));
    service->ref_count = 1;
    service->nmem = nmem;
    service->next = 0;
    service->settings = 0;
    service->databases = 0;
    service->targetprofiles = 0;
    service->server = server;
    service->session_timeout = 60; /* default session timeout */
    service->z3950_session_timeout = 180;
    service->z3950_operation_timeout = 30;

    service->relevance_pct = 0;
    service->sort_pct = 0;
    service->mergekey_pct = 0;

    service->id = service_id ? nmem_strdup(nmem, service_id) : 0;
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
    service->dictionary = 0;
    return service; 
}

static struct conf_metadata* conf_service_add_metadata(
    struct conf_service *service,
    int field_id,
    const char *name,
    enum conf_metadata_type type,
    enum conf_metadata_merge merge,
    enum conf_setting_type setting,
    int brief,
    int termlist,
    int rank,
    int sortkey_offset,
    enum conf_metadata_mergekey mt)
{
    struct conf_metadata * md = 0;

    if (!service || !service->metadata || !service->num_metadata
        || field_id < 0  || !(field_id < service->num_metadata))
        return 0;

    md = service->metadata + field_id;
    conf_metadata_assign(service->nmem, md, name, type, merge, setting,
                         brief, termlist, rank, sortkey_offset,
                         mt);
    return md;
}


static struct conf_sortkey * conf_service_add_sortkey(
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
    conf_sortkey_assign(service->nmem, sk, name, type);

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
}


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
}

static void conf_dir_path(struct conf_config *config, WRBUF w, const char *src)
{
    if (config->confdir && wrbuf_len(config->confdir) > 0 &&
        !yaz_is_abspath(src))
    {
        wrbuf_printf(w, "%s/%s", wrbuf_cstr(config->confdir), src);
    }
    else
        wrbuf_puts(w, src);
}

void service_destroy(struct conf_service *service)
{
    if (service)
    {
        assert(service->ref_count > 0);
        service->ref_count--;
        if (service->ref_count == 0)
        {
            pp2_charset_destroy(service->relevance_pct);
            pp2_charset_destroy(service->sort_pct);
            pp2_charset_destroy(service->mergekey_pct);
            nmem_destroy(service->nmem);
        }
    }
}

void service_incref(struct conf_service *service)
{
    service->ref_count++;
}

static int parse_metadata(struct conf_service *service, xmlNode *n,
                          int *md_node, int *sk_node)
{
    xmlChar *xml_name = xmlGetProp(n, (xmlChar *) "name");
    xmlChar *xml_brief = xmlGetProp(n, (xmlChar *) "brief");
    xmlChar *xml_sortkey = xmlGetProp(n, (xmlChar *) "sortkey");
    xmlChar *xml_merge = xmlGetProp(n, (xmlChar *) "merge");
    xmlChar *xml_type = xmlGetProp(n, (xmlChar *) "type");
    xmlChar *xml_termlist = xmlGetProp(n, (xmlChar *) "termlist");
    xmlChar *xml_rank = xmlGetProp(n, (xmlChar *) "rank");
    xmlChar *xml_setting = xmlGetProp(n, (xmlChar *) "setting");
    xmlChar *xml_mergekey = xmlGetProp(n, (xmlChar *) "mergekey");
    
    enum conf_metadata_type type = Metadata_type_generic;
    enum conf_metadata_merge merge = Metadata_merge_no;
    enum conf_setting_type setting = Metadata_setting_no;
    enum conf_sortkey_type sk_type = Metadata_sortkey_relevance;
    enum conf_metadata_mergekey mergekey_type = Metadata_mergekey_no;
    int brief = 0;
    int termlist = 0;
    int rank = 0;
    int sortkey_offset = 0;
    
    // now do the parsing logic
    if (!xml_name)
    {
        yaz_log(YLOG_FATAL, "Must specify name in metadata element");
        return -1;
    }
    if (xml_brief)
    {
        if (!strcmp((const char *) xml_brief, "yes"))
            brief = 1;
        else if (strcmp((const char *) xml_brief, "no"))
        {
            yaz_log(YLOG_FATAL, "metadata/brief must be yes or no");
            return -1;
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
            return -1;
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
        else if (!strcmp((const char *) xml_type, "date"))
            type = Metadata_type_date;
        else
        {
            yaz_log(YLOG_FATAL, 
                    "Unknown value for metadata/type: %s", xml_type);
            return -1;
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
            return -1;
        }
    }
    else
        merge = Metadata_merge_no;
    
    if (xml_setting)
    {
        if (!strcmp((const char *) xml_setting, "no"))
            setting = Metadata_setting_no;
        else if (!strcmp((const char *) xml_setting, "postproc"))
            setting = Metadata_setting_postproc;
        else if (!strcmp((const char *) xml_setting, "parameter"))
            setting = Metadata_setting_parameter;
        else
        {
            yaz_log(YLOG_FATAL,
                    "Unknown value for medadata/setting: %s", xml_setting);
            return -1;
        }
    }
    
    // add a sortkey if so specified
    if (xml_sortkey && strcmp((const char *) xml_sortkey, "no"))
    {
        if (merge == Metadata_merge_no)
        {
            yaz_log(YLOG_FATAL, 
                    "Can't specify sortkey on a non-merged field");
            return -1;
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
            return -1;
        }
        sortkey_offset = *sk_node;
        
        conf_service_add_sortkey(service, *sk_node,
                                 (const char *) xml_name, sk_type);
        
        (*sk_node)++;
    }
    else
        sortkey_offset = -1;
    
    if (xml_mergekey)
    {
        if (!strcmp((const char *) xml_mergekey, "required"))
            mergekey_type = Metadata_mergekey_required;
        else if (!strcmp((const char *) xml_mergekey, "optional"))
            mergekey_type = Metadata_mergekey_optional;
        else if (!strcmp((const char *) xml_mergekey, "no"))
            mergekey_type = Metadata_mergekey_no;
        else
        {
            yaz_log(YLOG_FATAL, "Unknown value for mergekey: %s", xml_mergekey);
            return -1;
        }
    }
    
    
    // metadata known, assign values
    conf_service_add_metadata(service, *md_node,
                              (const char *) xml_name,
                              type, merge, setting,
                              brief, termlist, rank, sortkey_offset,
                              mergekey_type);
    
    xmlFree(xml_name);
    xmlFree(xml_brief);
    xmlFree(xml_sortkey);
    xmlFree(xml_merge);
    xmlFree(xml_type);
    xmlFree(xml_termlist);
    xmlFree(xml_rank);
    xmlFree(xml_setting);
    xmlFree(xml_mergekey);
    (*md_node)++;
    return 0;
}

static struct conf_service *service_create_static(struct conf_server *server,
                                                  xmlNode *node,
                                                  const char *service_id)
{
    xmlNode *n;
    int md_node = 0;
    int sk_node = 0;

    struct conf_service *service = 0;
    int num_metadata = 0;
    int num_sortkeys = 0;
    int got_settings = 0;
    
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

    service = service_init(server, num_metadata, num_sortkeys, service_id);

    for (n = node->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp((const char *) n->name, "timeout"))
        {
            xmlChar *src = xmlGetProp(n, (xmlChar *) "session");
            if (src)
            {
                service->session_timeout = atoi((const char *) src);
                xmlFree(src);
                if (service->session_timeout < 9)
                {
                    yaz_log(YLOG_FATAL, "session timeout out of range");
                    return 0;
                }
            }
            src = xmlGetProp(n, (xmlChar *) "z3950_operation");
            if (src)
            {
                service->z3950_operation_timeout = atoi((const char *) src);
                xmlFree(src);
                if (service->z3950_session_timeout < 9)
                {
                    yaz_log(YLOG_FATAL, "Z39.50 operation timeout out of range");
                    return 0;
                }
            }
            src = xmlGetProp(n, (xmlChar *) "z3950_session");
            if (src)
            {
                service->z3950_session_timeout = atoi((const char *) src);
                xmlFree(src);
                if (service->z3950_session_timeout < 9)
                {
                    yaz_log(YLOG_FATAL, "Z39.50 session timeout out of range");
                    return 0;
                }
            }
        }
        else if (!strcmp((const char *) n->name, "settings"))
            got_settings++;
        else if (!strcmp((const char *) n->name, (const char *) "targetprofiles"))
        {
            if (service->targetprofiles)
            {
                yaz_log(YLOG_FATAL, "Can't repeat targetprofiles");
                return 0;
            }
            if (!(service->targetprofiles = 
                  parse_targetprofiles(service->nmem, n)))
                return 0;
        }
        else if (!strcmp((const char *) n->name, "relevance"))
        {
            if (service->relevance_pct)
            {
                yaz_log(YLOG_LOG, "relevance may not repeat in service");
                return 0;
            }
            else
            {
                service->relevance_pct = pp2_charset_create_xml(n);
                if (!service->relevance_pct)
                    return 0;
            }
        }
        else if (!strcmp((const char *) n->name, "sort"))
        {
            if (service->sort_pct)
            {
                yaz_log(YLOG_LOG, "sort may not repeat in service");
                return 0;
            }
            else
            {
                service->sort_pct = pp2_charset_create_xml(n);
                if (!service->sort_pct)
                    return 0;
            }
        }
        else if (!strcmp((const char *) n->name, "mergekey"))
        {
            if (service->mergekey_pct)
            {
                yaz_log(YLOG_LOG, "mergekey may not repeat in service");
                return 0;
            }
            else
            {
                service->mergekey_pct = pp2_charset_create_xml(n);
                if (!service->mergekey_pct)
                    return 0;
            }
        }
        else if (!strcmp((const char *) n->name, (const char *) "metadata"))
        {
            if (parse_metadata(service, n, &md_node, &sk_node))
                return 0;
        }
        else
        {
            yaz_log(YLOG_FATAL, "Bad element: %s", n->name);
            return 0;
        }
    }
    if (got_settings)
    {
        int pass;
        /* metadata has been read.. Consider now settings */
        init_settings(service);
        for (pass = 1; pass <= 2; pass++)
        {
            for (n = node->children; n; n = n->next)
            {
                if (n->type != XML_ELEMENT_NODE)
                    continue;
                if (!strcmp((const char *) n->name, "settings"))
                {
                    xmlChar *src = xmlGetProp(n, (xmlChar *) "src");
                    if (src)
                    {
                        WRBUF w = wrbuf_alloc();
                        conf_dir_path(server->config, w, (const char *) src);
                        settings_read_file(service, wrbuf_cstr(w), pass);
                        wrbuf_destroy(w);
                        xmlFree(src);
                    }
                    else
                    {
                        settings_read_node(service, n, pass);
                    }
                }
            }
        }
    }
    return service;
}

static char *parse_settings(struct conf_config *config,
                            NMEM nmem, xmlNode *node)
{
    xmlChar *src = xmlGetProp(node, (xmlChar *) "src");
    char *r;

    if (src)
    {
        WRBUF w = wrbuf_alloc();
        conf_dir_path(config, w, (const char *) src);
        r = nmem_strdup(nmem, wrbuf_cstr(w));
        wrbuf_destroy(w);
    }
    else
    {
        yaz_log(YLOG_FATAL, "Must specify src in targetprofile");
        return 0;
    }
    xmlFree(src);
    return r;
}

static void inherit_server_settings(struct conf_service *s)
{
    struct conf_server *server = s->server;
    if (!s->dictionary) /* service has no config settings ? */
    {
        if (server->server_settings)
        {
            /* inherit settings from server */
            init_settings(s);
            settings_read_file(s, server->server_settings, 1);
            settings_read_file(s, server->server_settings, 2);
        }
        else
        {
            yaz_log(YLOG_WARN, "service '%s' has no settings",
                    s->id ? s->id : "unnamed");
            init_settings(s);
        }
    }
    
    /* use relevance/sort/mergekey from server if not defined
       for this service.. */
    if (!s->relevance_pct)
    {
        if (server->relevance_pct)
        {
            s->relevance_pct = server->relevance_pct;
            pp2_charset_incref(s->relevance_pct);
        }
        else
            s->relevance_pct = pp2_charset_create(0);
    }
    
    if (!s->sort_pct)
    {
        if (server->sort_pct)
        {
            s->sort_pct = server->sort_pct;
            pp2_charset_incref(s->sort_pct);
        }
        else
            s->sort_pct = pp2_charset_create(0);
    }
    
    if (!s->mergekey_pct)
    {
        if (server->mergekey_pct)
        {
            s->mergekey_pct = server->mergekey_pct;
            pp2_charset_incref(s->mergekey_pct);
        }
        else
            s->mergekey_pct = pp2_charset_create(0);
    }
}

struct conf_service *service_create(struct conf_server *server,
                                    xmlNode *node)
{
    struct conf_service *service = service_create_static(server,
                                                         node, 0);
    if (service)
    {
        inherit_server_settings(service);
        resolve_databases(service);
    }
    return service;
}

static struct conf_server *server_create(struct conf_config *config,
                                         NMEM nmem, xmlNode *node)
{
    xmlNode *n;
    struct conf_server *server = nmem_malloc(nmem, sizeof(struct conf_server));

    server->host = 0;
    server->port = 0;
    server->proxy_host = 0;
    server->proxy_port = 0;
    server->myurl = 0;
    server->proxy_addr = 0;
    server->service = 0;
    server->config = config;
    server->next = 0;
    server->relevance_pct = 0;
    server->sort_pct = 0;
    server->mergekey_pct = 0;
    server->server_settings = 0;

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
            xmlFree(port);
            xmlFree(host);
            xmlFree(myurl);
        }
        else if (!strcmp((const char *) n->name, "settings"))
        {
            if (server->server_settings)
            {
                yaz_log(YLOG_FATAL, "Can't repeat 'settings'");
                return 0;
            }
            if (!(server->server_settings = parse_settings(config, nmem, n)))
                return 0;
        }
        else if (!strcmp((const char *) n->name, "relevance"))
        {
            server->relevance_pct = pp2_charset_create_xml(n);
            if (!server->relevance_pct)
                return 0;
        }
        else if (!strcmp((const char *) n->name, "sort"))
        {
            server->sort_pct = pp2_charset_create_xml(n);
            if (!server->sort_pct)
                return 0;
        }
        else if (!strcmp((const char *) n->name, "mergekey"))
        {
            server->mergekey_pct = pp2_charset_create_xml(n);
            if (!server->mergekey_pct)
                return 0;
        }
        else if (!strcmp((const char *) n->name, "service"))
        {
            char *service_id = (char *)
                xmlGetProp(n, (xmlChar *) "id");

            struct conf_service **sp = &server->service;
            for (; *sp; sp = &(*sp)->next)
                if ((*sp)->id && service_id &&
                    0 == strcmp((*sp)->id, service_id))
                {
                    yaz_log(YLOG_FATAL, "Duplicate service: %s", service_id);
                    break;
                }
                else if (!(*sp)->id && !service_id)
                {
                    yaz_log(YLOG_FATAL, "Duplicate unnamed service '%s'",
                        service_id);
                    break;
                }

            if (*sp)  /* service already exist */
            {
                xmlFree(service_id);
                return 0;
            }
            else
            {
                struct conf_service *s = service_create_static(server, n,
                                                               service_id);
                xmlFree(service_id);
                if (!s)
                    return 0;
                *sp = s;
            }
        }
        else
        {
            yaz_log(YLOG_FATAL, "Bad element: %s", n->name);
            return 0;
        }
    }
    if (server->service)
    {
        struct conf_service *s;
        for (s = server->service; s; s = s->next)
            inherit_server_settings(s);
    }
    return server;
}

WRBUF conf_get_fname(struct conf_service *service, const char *fname)
{
    struct conf_config *config = service->server->config;
    WRBUF w = wrbuf_alloc();

    conf_dir_path(config, w, fname);
    return w;
}

static struct conf_targetprofiles *parse_targetprofiles(NMEM nmem,
                                                        xmlNode *node)
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

struct conf_service *locate_service(struct conf_server *server,
                                    const char *service_id)
{
    struct conf_service *s = server->service;
    for (; s; s = s->next)
        if (s->id && service_id && 0 == strcmp(s->id, service_id))
            return s;
        else if (!s->id && !service_id)
            return s;
    return 0;
}


static int parse_config(struct conf_config *config, xmlNode *root)
{
    xmlNode *n;

    for (n = root->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp((const char *) n->name, "server"))
        {
            struct conf_server *tmp = server_create(config, config->nmem, n);
            if (!tmp)
                return -1;
            tmp->next = config->servers;
            config->servers = tmp;
        }
        else if (!strcmp((const char *) n->name, "targetprofiles"))
        {
            yaz_log(YLOG_FATAL, "targetprofiles unsupported here. Must be part of service");
            return -1;

        }
        else
        {
            yaz_log(YLOG_FATAL, "Bad element: %s", n->name);
            return -1;
        }
    }
    return 0;
}

static int process_config_includes(struct conf_config *config, xmlNode *n);

static int config_include_one(struct conf_config *config, xmlNode **sib,
    const char *path)
{
    struct stat st;
    if (stat(path, &st) < 0)
    {
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "stat %s", path);
        return -1;
    }
    else
    {
        if ((st.st_mode & S_IFMT) == S_IFREG)
        {
            xmlDoc *doc = xmlParseFile(path);
            if (doc)
            {
                xmlNodePtr t = xmlDocGetRootElement(doc);
                int ret = process_config_includes(config, t);
                *sib = xmlAddNextSibling(*sib, xmlCopyNode(t, 1));
                xmlFreeDoc(doc);
                if (ret)
                    return -1;
            }
            else
            {
                yaz_log(YLOG_FATAL, "Could not parse %s", path);
                return -1;
            }
        }
    }
    return 0;
}

static int config_include_src(struct conf_config *config, xmlNode **np,
                              const char *src)
{
    int ret = 0; /* return code. OK so far */
    WRBUF w = wrbuf_alloc();
    xmlNodePtr sib; /* our sibling that we append */
    xmlNodePtr c; /* tmp node */

    wrbuf_printf(w, " begin include src=\"%s\" ", src);

    /* replace include element with a 'begin' comment */
    sib = xmlNewComment((const xmlChar *) wrbuf_cstr(w));
    xmlReplaceNode(*np, sib);

    xmlFreeNode(*np);

    wrbuf_rewind(w);
    conf_dir_path(config, w, src);
#if USE_POSIX_GLOB
    {
        size_t i;
        glob_t glob_res;
        glob(wrbuf_cstr(w), 0 /* flags */, 0 /* errfunc */, &glob_res);
        
        for (i = 0; ret == 0 && i < glob_res.gl_pathc; i++)
        {
            const char *path = glob_res.gl_pathv[i];
            ret = config_include_one(config, &sib, path);
        }
        globfree(&glob_res);
    }
#else
    ret = config_include_one(config, &sib, wrbuf_cstr(w));
#endif
    wrbuf_rewind(w);
    wrbuf_printf(w, " end include src=\"%s\" ", src);
    c = xmlNewComment((const xmlChar *) wrbuf_cstr(w));
    sib = xmlAddNextSibling(sib, c);
    
    *np = sib;
    wrbuf_destroy(w);
    return ret;
}

static int process_config_includes(struct conf_config *config, xmlNode *n)
{
    for (; n; n = n->next)
    {
        if (n->type == XML_ELEMENT_NODE)
        {
            if (!strcmp((const char *) n->name, "include"))
            {
                xmlChar *src = xmlGetProp(n, (xmlChar *) "src");
                if (src)
                {
                    int ret = config_include_src(config, &n,
                                                 (const char *) src);
                    xmlFree(src);
                    if (ret)
                        return ret;
                        
                }
            }
            else
            {
                if (process_config_includes(config, n->children))
                    return -1;
            }
        }
    }
    return 0;
}

struct conf_config *config_create(const char *fname, int verbose)
{
    xmlDoc *doc = xmlParseFile(fname);
    xmlNode *n;
    const char *p;
    int r;
    NMEM nmem = nmem_create();
    struct conf_config *config = nmem_malloc(nmem, sizeof(struct conf_config));

    xmlSubstituteEntitiesDefault(1);
    xmlLoadExtDtdDefaultValue = 1;
    if (!doc)
    {
        yaz_log(YLOG_FATAL, "Failed to read %s", fname);
        nmem_destroy(nmem);
        return 0;
    }

    config->nmem = nmem;
    config->servers = 0;

    config->confdir = wrbuf_alloc();
    if ((p = strrchr(fname, 
#ifdef WIN32
                     '\\'
#else
                     '/'
#endif
             )))
    {
        int len = p - fname;
        wrbuf_write(config->confdir, fname, len);
    }
    wrbuf_puts(config->confdir, "");
    
    n = xmlDocGetRootElement(doc);
    r = process_config_includes(config, n);
    if (r == 0) /* OK */
    {
        if (verbose)
        {
            yaz_log(YLOG_LOG, "Configuration %s after include processing",
                    fname);
#if LIBXML_VERSION >= 20600
            xmlDocFormatDump(yaz_log_file(), doc, 0);
#else
            xmlDocDump(yaz_log_file(), doc);
#endif
        }
        r = parse_config(config, n);
    }
    xmlFreeDoc(doc);

    if (r)
    {
        config_destroy(config);
        return 0;
    }
    return config;
}

void server_destroy(struct conf_server *server)
{
    struct conf_service *s = server->service;
    while (s)
    {
        struct conf_service *s_next = s->next;
        service_destroy(s);
        s = s_next;
    }
    pp2_charset_destroy(server->relevance_pct);
    pp2_charset_destroy(server->sort_pct);
    pp2_charset_destroy(server->mergekey_pct);
}

void config_destroy(struct conf_config *config)
{
    if (config)
    {
        struct conf_server *server = config->servers;
        while (server)
        {
            struct conf_server *s_next = server->next;
            server_destroy(server);
            server = s_next;
        }
        wrbuf_destroy(config->confdir);
        nmem_destroy(config->nmem);
    }
}

void config_stop_listeners(struct conf_config *conf)
{
    struct conf_server *ser;
    for (ser = conf->servers; ser; ser = ser->next)
        http_close_server(ser);
}

void config_start_databases(struct conf_config *conf)
{
    struct conf_server *ser;
    for (ser = conf->servers; ser; ser = ser->next)
    {
        struct conf_service *s = ser->service;
        for (;s ; s = s->next)
            resolve_databases(s);
    }
}

int config_start_listeners(struct conf_config *conf,
                           const char *listener_override)
{
    struct conf_server *ser;
    for (ser = conf->servers; ser; ser = ser->next)
    {
        WRBUF w = wrbuf_alloc();
        int r;
        if (listener_override)
        {
            wrbuf_puts(w, listener_override);
            listener_override = 0; /* only first server is overriden */
        }
        else
        {
            if (ser->host)
                wrbuf_puts(w, ser->host);
            if (ser->port)
            {
                if (wrbuf_len(w))
                    wrbuf_puts(w, ":");
                wrbuf_printf(w, "%d", ser->port);
            }
        }
        r = http_init(wrbuf_cstr(w), ser);
        wrbuf_destroy(w);
        if (r)
            return -1;

        w = wrbuf_alloc();
        if (ser->proxy_host || ser->proxy_port)
        {
            if (ser->proxy_host)
                wrbuf_puts(w, ser->proxy_host);
            if (ser->proxy_port)
            {
                if (wrbuf_len(w))
                    wrbuf_puts(w, ":");
                wrbuf_printf(w, "%d", ser->proxy_port);
            }
        }
        if (wrbuf_len(w))
            http_set_proxyaddr(wrbuf_cstr(w), ser);
        wrbuf_destroy(w);
    }
    return 0;
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

