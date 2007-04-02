/* $Id: config.c,v 1.21 2007-04-02 09:43:08 marc Exp $ */

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

static xsltStylesheet *load_stylesheet(const char *fname)
{
    char path[256];
    sprintf(path, "%s/%s", confdir, fname);
    return xsltParseStylesheetFile((xmlChar *) path);
}

static void setup_marc(struct conf_retrievalprofile *r)
{
    yaz_iconv_t cm;
    r->yaz_marc = yaz_marc_create();
    if (!(cm = yaz_iconv_open("utf-8", r->native_encoding)))
    {
        yaz_log(YLOG_WARN, "Unable to support mapping from %s", r->native_encoding);
        return;
    }
    yaz_marc_iconv(r->yaz_marc, cm);
}

static struct conf_retrievalprofile *parse_retrievalprofile(xmlNode *node)
{
    struct conf_retrievalprofile *r = nmem_malloc(nmem, sizeof(struct conf_retrievalprofile));
    xmlNode *n;
    struct conf_retrievalmap **rm = &r->maplist;

    r->requestsyntax = 0;
    r->native_syntax = Nativesyn_xml;
    r->native_format = Nativeform_na;
    r->native_encoding = 0;
    r->native_mapto = Nativemapto_na;
    r->yaz_marc = 0;
    r->maplist = 0;
    r->next = 0;

    for (n = node->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp((const char *) n->name, "requestsyntax"))
        {
            xmlChar *content = xmlNodeGetContent(n);
            if (content)
                r->requestsyntax = nmem_strdup(nmem, (const char *) content);
        }
        else if (!strcmp((const char *) n->name, "nativesyntax"))
        {
            xmlChar *name = xmlGetProp(n, (xmlChar *) "name");
            xmlChar *format = xmlGetProp(n, (xmlChar *) "format");
            xmlChar *encoding = xmlGetProp(n, (xmlChar *) "encoding");
            xmlChar *mapto = xmlGetProp(n, (xmlChar *) "mapto");
            if (!name)
            {
                yaz_log(YLOG_WARN, "Missing name in 'nativesyntax' element");
                return 0;
            }
            if (encoding)
                r->native_encoding = (char *) encoding;
            if (!strcmp((const char *) name, "iso2709"))
            {
                r->native_syntax = Nativesyn_iso2709;
                // Set a few defaults, too
                r->native_format = Nativeform_marc21;
                r->native_mapto = Nativemapto_marcxml;
                if (!r->native_encoding)
                    r->native_encoding = "marc-8";
                setup_marc(r);
            }
            else if (!strcmp((const char *) name, "xml"))
                r->native_syntax = Nativesyn_xml;
            else
            {
                yaz_log(YLOG_WARN, "Unknown native syntax name %s", name);
                return 0;
            }
            if (format)
            {
                if (!strcmp((const char *) format, "marc21") 
                    || !strcmp((const char *) format, "usmarc"))
                    r->native_format = Nativeform_marc21;
                else
                {
                    yaz_log(YLOG_WARN, "Unknown native format name %s", format);
                    return 0;
                }
            }
            if (mapto)
            {
                if (!strcmp((const char *) mapto, "marcxml"))
                    r->native_mapto = Nativemapto_marcxml;
                else if (!strcmp((const char *)mapto, "marcxchange"))
                    r->native_mapto = Nativemapto_marcxchange;
                else
                {
                    yaz_log(YLOG_WARN, "Unknown mapto target %s", format);
                    return 0;
                }
            }
            xmlFree(name);
            xmlFree(format);
            xmlFree(encoding);
            xmlFree(mapto);
        }
        else if (!strcmp((const char *) n->name, "map"))
        {
            struct conf_retrievalmap *m = nmem_malloc(nmem, sizeof(struct conf_retrievalmap));
            xmlChar *type = xmlGetProp(n, (xmlChar *) "type");
            xmlChar *charset = xmlGetProp(n, (xmlChar *) "charset");
            xmlChar *format = xmlGetProp(n, (xmlChar *) "format");
            xmlChar *stylesheet = xmlGetProp(n, (xmlChar *) "stylesheet");
            memset(m, 0, sizeof(*m));
            if (type)
            {
                if (!strcmp((const char *) type, "xslt"))
                    m->type = Map_xslt;
                else
                {
                    yaz_log(YLOG_WARN, "Unknown map type: %s", type);
                    return 0;
                }
            }
            if (charset)
                m->charset = nmem_strdup(nmem, (const char *) charset);
            if (format)
                m->format = nmem_strdup(nmem, (const char *) format);
            if (stylesheet)
            {
                if (!(m->stylesheet = load_stylesheet((char *) stylesheet)))
                    return 0;
            }
            *rm = m;
            rm = &m->next;
            xmlFree(type);
            xmlFree(charset);
            xmlFree(format);
            xmlFree(stylesheet);
        }
        else
        {
            yaz_log(YLOG_FATAL, "Bad element in retrievalprofile: %s", n->name);
            return 0;
        }
    }

    return r;
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
    struct conf_retrievalprofile **rp = &r->retrievalprofiles;

    r->servers = 0;
    r->retrievalprofiles = 0;
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
        else if (!strcmp((const char *) n->name, "retrievalprofile"))
        {
            if (!(*rp = parse_retrievalprofile(n)))
                return 0;
            rp = &(*rp)->next;
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
