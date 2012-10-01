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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <assert.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xinclude.h>

#include <yaz/yaz-util.h>
#include <yaz/nmem.h>
#include <yaz/snprintf.h>
#include <yaz/tpath.h>
#include <yaz/xml_include.h>

#include <sys/types.h>
#include <sys/stat.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "ppmutex.h"
#include "incref.h"
#include "pazpar2_config.h"
#include "service_xslt.h"
#include "settings.h"
#include "eventl.h"
#include "http.h"

struct conf_config
{
    NMEM nmem; /* for conf_config and servers memory */
    struct conf_server *servers;

    int no_threads;
    WRBUF confdir;
    iochan_man_t iochan_man;
    database_hosts_t database_hosts;
};

struct service_xslt
{
    char *id;
    xsltStylesheetPtr xsp;
    struct service_xslt *next;
};

static char *xml_context(const xmlNode *ptr, char *res, size_t len)
{
    xmlAttr *attr = ptr->properties;
    size_t off = len - 1;

    res[off] = '\0';
    for (; attr; attr = attr->next)
    {
        size_t l = strlen((const char *) attr->name);
        if (off <= l + 1)
            break;
        off = off - l;
        memcpy(res + off, attr->name, l);
        res[--off] = '@';
    }
    while (ptr && ptr->type == XML_ELEMENT_NODE)
    {
        size_t l = strlen((const char *) ptr->name);
        if (off <= l + 1)
            break;

        off = off - l;
        memcpy(res + off, ptr->name, l);
        res[--off] = '/';

        ptr = ptr->parent;
    }
    return res + off;
}

struct conf_service *service_init(struct conf_server *server,
                                         int num_metadata, int num_sortkeys,
                                         const char *service_id)
{
    struct conf_service * service = 0;
    NMEM nmem = nmem_create();


    service = nmem_malloc(nmem, sizeof(struct conf_service));
    service->mutex = 0;
    service->ref_count = 1;
    service->nmem = nmem;
    service->next = 0;
    service->databases = 0;
    service->xslt_list = 0;
    service->ccl_bibset = 0;
    service->server = server;
    service->session_timeout = 60; /* default session timeout */
    service->z3950_session_timeout = 180;
    service->z3950_operation_timeout = 30;
    service->rank_cluster = 1;
    service->rank_debug = 0;
    service->rank_follow = 0.0;
    service->rank_lead = 0.0;
    service->rank_length = 2;

    service->charsets = 0;

    service->id = service_id ? nmem_strdup(nmem, service_id) : 0;

    // Setup a dictionary from server.
    service->dictionary = 0;

    service->settings = nmem_malloc(nmem, sizeof(*service->settings));
    service->settings->num_settings = PZ_MAX_EOF;
    service->settings->settings = nmem_malloc(nmem, sizeof(struct setting*) * service->settings->num_settings);
    memset(service->settings->settings, 0, sizeof(struct setting*) * service->settings->num_settings);
    //  inherit_server_settings_values(service);

    service->next = 0;

    service->num_metadata = num_metadata;

    service->metadata = 0;
    if (service->num_metadata)
        service->metadata
            = nmem_malloc(nmem,
                          sizeof(struct conf_metadata) * service->num_metadata);
    service->num_sortkeys = num_sortkeys;

    service->default_sort = nmem_strdup(nmem, "relevance");
    service->sortkeys = 0;
    if (service->num_sortkeys)
        service->sortkeys
            = nmem_malloc(nmem,
                          sizeof(struct conf_sortkey) * service->num_sortkeys);
    service->xml_node = 0;
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
    const char *rank,
    int sortkey_offset,
    enum conf_metadata_mergekey mt,
    const char *facetrule,
    const char *limitmap,
    const char *limitcluster
    )
{
    struct conf_metadata * md = 0;
    NMEM nmem = service->nmem;

    if (!service->metadata || !service->num_metadata
        || field_id < 0  || !(field_id < service->num_metadata))
        return 0;

    md = service->metadata + field_id;
    assert(nmem && md && name);

    md->name = nmem_strdup(nmem, name);

    md->type = type;

    // enforcing that type_year is always range_merge
    if (md->type == Metadata_type_year)
        md->merge = Metadata_merge_range;
    else
        md->merge = merge;

    md->setting = setting;
    md->brief = brief;
    md->termlist = termlist;
    md->rank = nmem_strdup_null(nmem, rank);
    md->sortkey_offset = sortkey_offset;
    md->mergekey = mt;
    md->facetrule = nmem_strdup_null(nmem, facetrule);
    md->limitmap = nmem_strdup_null(nmem, limitmap);
    md->limitcluster = nmem_strdup_null(nmem, limitcluster);
    return md;
}

static struct conf_sortkey *conf_service_add_sortkey(
    struct conf_service *service,
    int field_id,
    const char *name,
    enum conf_metadata_type type)
{
    struct conf_sortkey *sk = 0;
    NMEM nmem = service->nmem;

    if (!service->sortkeys || !service->num_sortkeys
        || field_id < 0 || !(field_id < service->num_sortkeys))
        return 0;

    sk = service->sortkeys + field_id;

    assert(nmem && sk && name);

    sk->name = nmem_strdup(nmem, name);
    sk->type = type;
    return sk;
}

int conf_service_metadata_field_id(struct conf_service *service,
                                   const char * name)
{
    int i = 0;

    if (!service || !service->metadata || !service->num_metadata)
        return -1;

    for (i = 0; i < service->num_metadata; i++)
        if (!strcmp(name, (service->metadata[i]).name))
            return i;
    return -1;
}

int conf_service_sortkey_field_id(struct conf_service *service,
                                  const char * name)
{
    int i = 0;

    if (!service || !service->sortkeys || !service->num_sortkeys)
        return -1;

    for (i = 0; i < service->num_sortkeys; i++)
        if (!strcmp(name, (service->sortkeys[i]).name))
            return i;
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
        if (!pazpar2_decref(&service->ref_count, service->mutex))
        {
            service_xslt_destroy(service);
            pp2_charset_fact_destroy(service->charsets);
            ccl_qual_rm(&service->ccl_bibset);
            yaz_mutex_destroy(&service->mutex);
            nmem_destroy(service->nmem);
        }
    }
}

void service_incref(struct conf_service *service)
{
    pazpar2_incref(&service->ref_count, service->mutex);
}

static int parse_metadata(struct conf_service *service, xmlNode *n,
                          int *md_node, int *sk_node)
{
    enum conf_metadata_type type = Metadata_type_generic;
    enum conf_metadata_merge merge = Metadata_merge_no;
    enum conf_setting_type setting = Metadata_setting_no;
    enum conf_metadata_mergekey mergekey_type = Metadata_mergekey_no;
    int brief = 0;
    int termlist = 0;
    int sortkey_offset = 0;
    xmlChar *xml_name = 0;
    xmlChar *xml_brief = 0;
    xmlChar *xml_sortkey = 0;
    xmlChar *xml_merge = 0;
    xmlChar *xml_type = 0;
    xmlChar *xml_termlist = 0;
    xmlChar *xml_rank = 0;
    xmlChar *xml_setting = 0;
    xmlChar *xml_mergekey = 0;
    xmlChar *xml_limitmap = 0;
    xmlChar *xml_limitcluster = 0;
    xmlChar *xml_icu_chain = 0;

    struct _xmlAttr *attr;

    assert(service);

    for (attr = n->properties; attr; attr = attr->next)
    {
        if (!xmlStrcmp(attr->name, BAD_CAST "name") &&
            attr->children && attr->children->type == XML_TEXT_NODE)
            xml_name = attr->children->content;
        else if (!xmlStrcmp(attr->name, BAD_CAST "brief") &&
                 attr->children && attr->children->type == XML_TEXT_NODE)
            xml_brief = attr->children->content;
        else if (!xmlStrcmp(attr->name, BAD_CAST "sortkey") &&
                 attr->children && attr->children->type == XML_TEXT_NODE)
            xml_sortkey = attr->children->content;
        else if (!xmlStrcmp(attr->name, BAD_CAST "merge") &&
                 attr->children && attr->children->type == XML_TEXT_NODE)
            xml_merge = attr->children->content;
        else if (!xmlStrcmp(attr->name, BAD_CAST "type") &&
                 attr->children && attr->children->type == XML_TEXT_NODE)
            xml_type = attr->children->content;
        else if (!xmlStrcmp(attr->name, BAD_CAST "termlist") &&
                 attr->children && attr->children->type == XML_TEXT_NODE)
            xml_termlist = attr->children->content;
        else if (!xmlStrcmp(attr->name, BAD_CAST "rank") &&
                 attr->children && attr->children->type == XML_TEXT_NODE)
            xml_rank = attr->children->content;
        else if (!xmlStrcmp(attr->name, BAD_CAST "setting") &&
                 attr->children && attr->children->type == XML_TEXT_NODE)
            xml_setting = attr->children->content;
        else if (!xmlStrcmp(attr->name, BAD_CAST "mergekey") &&
                 attr->children && attr->children->type == XML_TEXT_NODE)
            xml_mergekey = attr->children->content;
        else if (!xmlStrcmp(attr->name, BAD_CAST "facetrule") &&
                 attr->children && attr->children->type == XML_TEXT_NODE)
            xml_icu_chain = attr->children->content;
        else if (!xmlStrcmp(attr->name, BAD_CAST "limitmap") &&
                 attr->children && attr->children->type == XML_TEXT_NODE)
            xml_limitmap = attr->children->content;
        else if (!xmlStrcmp(attr->name, BAD_CAST "limitcluster") &&
                 attr->children && attr->children->type == XML_TEXT_NODE)
            xml_limitcluster = attr->children->content;
        else
        {
            yaz_log(YLOG_FATAL, "Unknown metadata attribute '%s'", attr->name);
            return -1;
        }
    }

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

    if (xml_type)
    {
        if (!strcmp((const char *) xml_type, "generic"))
            type = Metadata_type_generic;
        else if (!strcmp((const char *) xml_type, "year"))
            type = Metadata_type_year;
        else if (!strcmp((const char *) xml_type, "date"))
            type = Metadata_type_date;
        else if (!strcmp((const char *) xml_type, "float"))
            type = Metadata_type_float;
        else
        {
            yaz_log(YLOG_FATAL,
                    "Unknown value for metadata/type: %s", xml_type);
            return -1;
        }
    }

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
        else if (!strcmp((const char *) xml_merge, "first"))
            merge = Metadata_merge_first;
        else
        {
            yaz_log(YLOG_FATAL,
                    "Unknown value for metadata/merge: %s", xml_merge);
            return -1;
        }
    }

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
                    "Unknown value for metadata/setting: %s", xml_setting);
            return -1;
        }
    }

    // add a sortkey if so specified
    if (xml_sortkey && strcmp((const char *) xml_sortkey, "no"))
    {
        enum conf_metadata_type sk_type = type;
        if (merge == Metadata_merge_no)
        {
            yaz_log(YLOG_FATAL,
                    "Can't specify sortkey on a non-merged field");
            return -1;
        }
        if (!strcmp((const char *) xml_sortkey, "yes"))
            ;
        if (!strcmp((const char *) xml_sortkey, "numeric"))
            ;
        else if (!strcmp((const char *) xml_sortkey, "skiparticle"))
        {
            if (sk_type == Metadata_type_generic)
                sk_type = Metadata_type_skiparticle;
            else
            {
                yaz_log(YLOG_FATAL,
                        "skiparticle only supported for type=generic: %s",
                    xml_type);
                return -1;
            }
        }
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
                              brief, termlist,
                              (const char *) xml_rank, sortkey_offset,
                              mergekey_type,
                              (const char *) xml_icu_chain,
                              (const char *) xml_limitmap,
                              (const char *) xml_limitcluster);
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
        else if (!strcmp((const char *) n->name, "ccldirective"))
        {
            char *name;
            char *value;
            if (!service->ccl_bibset)
                service->ccl_bibset = ccl_qual_mk();
            name = (char *) xmlGetProp(n, (xmlChar *) "name");
            if (!name)
            {
                yaz_log(YLOG_FATAL, "ccldirective: missing @name");
                return 0;
            }
            value = (char *) xmlGetProp(n, (xmlChar *) "value");
            if (!value)
            {
                xmlFree(name);
                yaz_log(YLOG_FATAL, "ccldirective: missing @value");
                return 0;
            }
            ccl_qual_add_special(service->ccl_bibset, name, value);
            xmlFree(value);
            xmlFree(name);
        }
        else if (!strcmp((const char *) n->name, "settings"))
            got_settings++;
        else if (!strcmp((const char *) n->name, "icu_chain"))
        {
            if (!service->charsets)
                service->charsets = pp2_charset_fact_create();
            if (pp2_charset_fact_define(service->charsets, n, 0))
            {
                yaz_log(YLOG_FATAL, "ICU chain definition error");
                return 0;
            }
        }
        else if (!strcmp((const char *) n->name, "relevance")
                 || !strcmp((const char *) n->name, "sort")
                 || !strcmp((const char *) n->name, "mergekey")
                 || !strcmp((const char *) n->name, "facet"))

        {
            if (!service->charsets)
                service->charsets = pp2_charset_fact_create();
            if (pp2_charset_fact_define(service->charsets,
                                        n->children, (const char *) n->name))
            {
                yaz_log(YLOG_FATAL, "ICU chain definition error");
                return 0;
            }
        }
        else if (!strcmp((const char *) n->name, (const char *) "metadata"))
        {
            if (parse_metadata(service, n, &md_node, &sk_node))
                return 0;
        }
        else if (!strcmp((const char *) n->name, (const char *) "xslt"))
        {
            if (service_xslt_config(service, n))
                return 0;
        }
        else if (!strcmp((const char *) n->name, "rank"))
        {
            char *rank_cluster = (char *) xmlGetProp(n, (xmlChar *) "cluster");
            char *rank_debug = (char *) xmlGetProp(n, (xmlChar *) "debug");
            char *rank_follow = (char *) xmlGetProp(n, (xmlChar *) "follow");
            char *rank_lead = (char *) xmlGetProp(n, (xmlChar *) "lead");
            char *rank_length= (char *) xmlGetProp(n, (xmlChar *) "length");
            if (rank_cluster)
            {
                if (!strcmp(rank_cluster, "yes"))
                    service->rank_cluster = 1;
                else if (!strcmp(rank_cluster, "no"))
                    service->rank_cluster = 0;
                else 
                {
                    yaz_log(YLOG_FATAL, "service: rank@cluster boolean");
                    return 0;
                }
            }
            if (rank_debug)
            {
                if (!strcmp(rank_debug, "yes"))
                    service->rank_debug = 1;
                else if (!strcmp(rank_debug, "no"))
                    service->rank_debug = 0;
                else
                {
                    yaz_log(YLOG_FATAL, "service: rank@debug boolean");
                    return 0;
                }
            }
            if (rank_follow)
            {
                service->rank_follow = atof(rank_follow);
            }
            if (rank_lead)
            {
                service->rank_lead = atof(rank_lead);
            }
            if (rank_length)
            {
                if (!strcmp(rank_length, "linear"))
                    service->rank_length = 2;
                else if (!strcmp(rank_length, "log"))
                    service->rank_length = 1; 
                else if (!strcmp(rank_length, "none"))
                    service->rank_length = 0;
                else
                {
                    yaz_log(YLOG_FATAL, "service: rank@length linear|log|none");
                    return 0;
                }
            }
            xmlFree(rank_cluster);
            xmlFree(rank_debug);
            xmlFree(rank_follow);
            xmlFree(rank_lead);
            xmlFree(rank_length);
        }
        else if (!strcmp((const char *) n->name, "sort-default"))
        {
            char *default_sort = (char *) xmlGetProp(n, (xmlChar *) "field");

            if (default_sort && strcmp(default_sort, "")) {
                service->default_sort = nmem_strdup(service->nmem, default_sort);
                yaz_log(YLOG_LOG, "service %s: default sort order configured to: %s",
                        service_id ? service_id : "unnamed", default_sort);
            }
            else
            {
                yaz_log(YLOG_FATAL, "default sort order is invalid: %s", default_sort);
                return 0;
            }
            xmlFree(default_sort);
        }
        else
        {
            char tmp[80];
            yaz_log(YLOG_FATAL, "Bad element: %s . Context: %s", n->name,
                    xml_context(n, tmp, sizeof tmp));
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
                    int ret;
                    xmlChar *src = xmlGetProp(n, (xmlChar *) "src");
                    if (src)
                    {
                        WRBUF w = wrbuf_alloc();
                        conf_dir_path(server->config, w, (const char *) src);
                        ret = settings_read_file(service, wrbuf_cstr(w), pass);
                        wrbuf_destroy(w);
                        xmlFree(src);
                    }
                    else
                    {
                        ret = settings_read_node(service, n, pass);
                    }
                    if (ret)
                        return 0;
                }
            }
        }
    }

    {
        xmlBufferPtr buf = xmlBufferCreate();
        xmlNodeDump(buf, node->doc, node, 0, 0);
        service->xml_node =
            nmem_strdupn(service->nmem, (const char *) buf->content, buf->use);
        xmlBufferFree(buf);
    }
    return service;
}

static int inherit_server_settings(struct conf_service *s)
{
    int ret = 0;
    struct conf_server *server = s->server;
    if (!s->dictionary) /* service has no config settings ? */
    {
        if (server->settings_fname)
        {
            /* inherit settings from server */
            init_settings(s);
            if (settings_read_file(s, server->settings_fname, 1))
                ret = -1;
            if (settings_read_file(s, server->settings_fname, 2))
                ret = -1;
        }
        else
        {
            yaz_log(YLOG_WARN, "server '%s' has no settings", s->id ? s->id : "unnamed");
            init_settings(s);
        }
    }

    /* use relevance/sort/mergekey/facet from server if not defined
       for this service.. */
    if (!s->charsets)
    {
        if (server->charsets)
        {
            s->charsets = server->charsets;
            pp2_charset_fact_incref(s->charsets);
        }
        else
        {
            s->charsets = pp2_charset_fact_create();
        }
    }
    return ret;
}

struct conf_service *service_create(struct conf_server *server,
                                    xmlNode *node)
{
    struct conf_service *service = service_create_static(server, node, 0);
    if (service)
    {
        inherit_server_settings(service);
        assert(service->mutex == 0);
        pazpar2_mutex_create(&service->mutex, "conf");
    }
    return service;
}

static struct conf_server *server_create(struct conf_config *config,
                                         NMEM nmem, xmlNode *node)
{
    xmlNode *n;
    struct conf_server *server = nmem_malloc(nmem, sizeof(struct conf_server));
    xmlChar *server_id = xmlGetProp(node, (xmlChar *) "id");

    server->host = "@";
    server->port = 0;
    server->proxy_host = 0;
    server->proxy_port = 0;
    server->myurl = 0;
    server->service = 0;
    server->config = config;
    server->next = 0;
    server->charsets = 0;
    server->http_server = 0;
    server->iochan_man = 0;
    server->database_hosts = config->database_hosts;
    server->settings_fname = 0;

    if (server_id)
    {
        server->server_id = nmem_strdup(nmem, (const char *)server_id);
        xmlFree(server_id);
    }
    else
        server->server_id = 0;
    for (n = node->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp((const char *) n->name, "listen"))
        {
            xmlChar *port = xmlGetProp(n, (xmlChar *) "port");
            xmlChar *host = xmlGetProp(n, (xmlChar *) "host");

            if (port)
                server->port = nmem_strdup(nmem, (const char *) port);
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
            xmlChar *src = xmlGetProp(n, (xmlChar *) "src");
            WRBUF w;
            if (!src)
            {
                yaz_log(YLOG_FATAL, "Missing src attribute for settings");
                return 0;
            }
            if (server->settings_fname)
            {
                xmlFree(src);
                yaz_log(YLOG_FATAL, "Can't repeat 'settings'");
                return 0;
            }
            w = wrbuf_alloc();
            conf_dir_path(config, w, (const char *) src);
            server->settings_fname = nmem_strdup(nmem, wrbuf_cstr(w));
            wrbuf_destroy(w);
            xmlFree(src);
        }
        else if (!strcmp((const char *) n->name, "icu_chain"))
        {
            if (!server->charsets)
                server->charsets = pp2_charset_fact_create();
            if (pp2_charset_fact_define(server->charsets, n, 0))
            {
                yaz_log(YLOG_FATAL, "ICU chain definition error");
                return 0;
            }
        }
        else if (!strcmp((const char *) n->name, "relevance")
                 || !strcmp((const char *) n->name, "sort")
                 || !strcmp((const char *) n->name, "mergekey")
                 || !strcmp((const char *) n->name, "facet"))
        {
            if (!server->charsets)
                server->charsets = pp2_charset_fact_create();
            if (pp2_charset_fact_define(server->charsets,
                                        n->children, (const char *) n->name))
            {
                yaz_log(YLOG_FATAL, "ICU chain definition error");
                return 0;
            }
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
                    yaz_log(YLOG_FATAL, "Duplicate unnamed service");
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
    if (!server->port)
    {
        yaz_log(YLOG_FATAL, "No listening port given");
        return 0;
    }
    if (server->service)
    {
        struct conf_service *s;
        for (s = server->service; s; s = s->next)
            inherit_server_settings(s);
    }
    return server;
}

WRBUF conf_get_fname(struct conf_config *config, const char *fname)
{
    WRBUF w = wrbuf_alloc();

    conf_dir_path(config, w, fname);
    return w;
}

struct conf_service *locate_service(struct conf_server *server,
                                    const char *service_id)
{
    struct conf_service *s = server->service;
    for (; s; s = s->next)
        if (s->id && service_id && 0 == strcmp(s->id, service_id))
            break;
        else if (!s->id && !service_id)
            break;
    if (s)
        service_incref(s);
    return s;
}

static void info_service_metadata(struct conf_service *service, WRBUF w)
{
    int i;
    struct conf_metadata *md;

    if (service->num_metadata)
    {
        for (i = 0; i < service->num_metadata; i++)
        {
            md = &(service->metadata[i]);
            wrbuf_puts(w, "   <metadata");
            if (md->name) {
                wrbuf_puts(w, " name=\"");
                wrbuf_xmlputs(w, md->name);
                wrbuf_puts(w, "\"");
            }
            if (md->brief) {
                wrbuf_puts(w, " brief=\"yes\"");
            }
            if (md->termlist) {
                wrbuf_puts(w, " termlist=\"yes\"");
            }
            if (md->rank) {
                wrbuf_puts(w, " rank=\"");
                wrbuf_xmlputs(w, md->rank);
                wrbuf_puts(w, "\"");
            }
            if (md->sortkey_offset > 0) {
                wrbuf_puts(w, " sortkey=\"");
                switch (service->sortkeys[md->sortkey_offset].type)
                {
                case Metadata_type_relevance:
                    wrbuf_puts(w, "relevance");
                    break;
                case Metadata_type_skiparticle:
                    wrbuf_puts(w, "skiparticle");
                    break;
                case Metadata_type_position:
                    wrbuf_puts(w, "position");
                    break;
                default:
                    wrbuf_puts(w, "yes");
                    break;
                }
                wrbuf_puts(w, "\"");
            }
            switch (md->type)
            {
            case Metadata_type_generic:
            case Metadata_type_skiparticle:
                break;
            case Metadata_type_year:
                wrbuf_puts(w, " type=\"year\"");
                break;
            case Metadata_type_date:
                wrbuf_puts(w, " type=\"date\"");
                break;
            case Metadata_type_float:
                wrbuf_puts(w, " type=\"float\"");
                break;
            case Metadata_type_relevance:
            case Metadata_type_position:
                break;
            }
            switch (md->merge)
            {
            case Metadata_merge_no:
                break;
            case Metadata_merge_unique:
                wrbuf_puts(w, " merge=\"unique\"");
                break;
            case Metadata_merge_longest:
                wrbuf_puts(w, " merge=\"longest\"");
                break;
            case Metadata_merge_range:
                wrbuf_puts(w, " merge=\"range\"");
                break;
            case Metadata_merge_all:
                wrbuf_puts(w, " merge=\"all\"");
                break;
            case Metadata_merge_first:
                wrbuf_puts(w, " merge=\"first\"");
                break;
            }
            switch (md->mergekey)
            {
            case Metadata_mergekey_no:
                break;
            case Metadata_mergekey_optional:
                wrbuf_puts(w, " mergekey=\"optional\"");
                break;
            case Metadata_mergekey_required:
                wrbuf_puts(w, " mergekey=\"required\"");
                break;
            }
            wrbuf_puts(w, " />\n");
        }
    }
}

static void info_service_databases(struct conf_service *service, WRBUF w)
{
    struct database *db;
    struct setting *s;
    int i;

    if (service->databases)
    {
        wrbuf_puts(w, "   <databases>\n");
        for(db = service->databases; db; db = db->next)
        {
            wrbuf_puts(w, "    <database");
            if (db->id)
            {
                wrbuf_puts(w, " id=\"");
                wrbuf_printf(w, "%s", db->id);
                wrbuf_puts(w, "\"");
            }
            wrbuf_puts(w, ">\n");
            for (i = 0; i < db->num_settings; i++)
            {
                s = db->settings[i];
                while (s != NULL)
                {
                    wrbuf_puts(w, "     <setting");
                    wrbuf_puts(w, " name=\"");
                    wrbuf_xmlputs(w, s->name);
                    wrbuf_puts(w, "\"");
                    wrbuf_puts(w, " value=\"");
                    wrbuf_xmlputs(w, s->value);
                    wrbuf_puts(w, "\"");
                    wrbuf_puts(w, " />\n");
                    s = s->next;
                }
            }
            wrbuf_puts(w, "    </database>\n");
        }
        wrbuf_puts(w, "   </databases>\n");
    }
}

void info_services(struct conf_server *server, WRBUF w)
{
    struct conf_service *s = server->service;
    int i;
    struct setting *S;

    wrbuf_puts(w, " <services>\n");
    for (; s; s = s->next)
    {
        wrbuf_puts(w, "  <service");
        if (s->id)
        {
            wrbuf_puts(w, " id=\"");
            wrbuf_xmlputs(w, s->id);
            wrbuf_puts(w, "\"");
        }
        wrbuf_puts(w, ">\n");
        if (s->settings)
        {
            for (i=0; i<s->settings->num_settings; i++)
            {
                S = s->settings->settings[i];
                while (S != NULL) {
                    wrbuf_puts(w, "   <setting");
                    wrbuf_puts(w, " name=\"");
                    wrbuf_xmlputs(w,  S->name);
                    wrbuf_puts(w, "\"");
                    wrbuf_puts(w, " value=\"");
                    wrbuf_xmlputs(w, S->value);
                    wrbuf_puts(w, "\"");
                    if (S->target) {
                        wrbuf_puts(w, " target=\"");
                        wrbuf_xmlputs(w, S->target);
                        wrbuf_puts(w, "\"");
                    }

                    wrbuf_puts(w, " />\n");

                    S = S->next;
                }
            }
        }
        info_service_metadata(s, w);
        info_service_databases(s, w);
        wrbuf_puts(w, "  </service>");

        wrbuf_puts(w, "\n");
    }
    wrbuf_puts(w, " </services>\n");
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
        else if (!strcmp((const char *) n->name, "threads"))
        {
            xmlChar *number = xmlGetProp(n, (xmlChar *) "number");
            if (number)
            {
                config->no_threads = atoi((const char *) number);
                xmlFree(number);
            }
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

struct conf_config *config_create(const char *fname, int verbose)
{
    xmlDoc *doc = xmlReadFile(fname,
                              NULL,
                              XML_PARSE_XINCLUDE + XML_PARSE_NOBLANKS
                              + XML_PARSE_NSCLEAN + XML_PARSE_NONET );
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

    // Perform XInclude.
    r = xmlXIncludeProcess(doc);
    if (r == -1)
    {
        yaz_log(YLOG_FATAL, "XInclude processing failed");
        return 0;
    }

    config->nmem = nmem;
    config->servers = 0;
    config->no_threads = 0;
    config->iochan_man = 0;
    config->database_hosts = database_hosts_create();

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
    r = yaz_xml_include_simple(n, wrbuf_cstr(config->confdir));
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
    pp2_charset_fact_destroy(server->charsets);
    http_server_destroy(server->http_server);
}

void config_destroy(struct conf_config *config)
{
    if (config)
    {
        struct conf_server *server = config->servers;
        iochan_man_destroy(&config->iochan_man);
        while (server)
        {
            struct conf_server *s_next = server->next;
            server_destroy(server);
            server = s_next;
            database_hosts_destroy(&config->database_hosts);
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

void config_process_events(struct conf_config *conf)
{
    struct conf_server *ser;

    for (ser = conf->servers; ser; ser = ser->next)
    {
        struct conf_service *s = ser->service;

        for (;s ; s = s->next)
        {
            assert(s->mutex == 0);
            pazpar2_mutex_create(&s->mutex, "service");
        }
        http_mutex_init(ser);
    }
    iochan_man_events(conf->iochan_man);
}

int config_start_listeners(struct conf_config *conf,
                           const char *listener_override,
                           const char *record_fname)
{
    struct conf_server *ser;

    conf->iochan_man = iochan_man_create(conf->no_threads);
    for (ser = conf->servers; ser; ser = ser->next)
    {
        WRBUF w;
        int r;

        ser->iochan_man = conf->iochan_man;
        if (listener_override)
        {
            const char *cp = strrchr(listener_override, ':');
            if (cp)
            {
                ser->host = nmem_strdupn(conf->nmem, listener_override,
                                         cp - listener_override);
                ser->port = nmem_strdup(conf->nmem, cp + 1);
            }
            else
            {
                ser->host = "@";
                ser->port = nmem_strdup(conf->nmem, listener_override);
            }
            listener_override = 0; /* only first server is overriden */
        }
        r = http_init(ser, record_fname);
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

