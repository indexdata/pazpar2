/* $Id: config.c,v 1.3 2007-01-03 06:23:44 quinn Exp $ */

#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxslt/xslt.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

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

    r->termlists = 0;

    for (n = node->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp(n->name, "termlist"))
        {
            struct conf_termlist *tl = nmem_malloc(nmem, sizeof(struct conf_termlist));
            xmlChar *name = xmlGetProp(n, "name");
            if (!name)
            {
                yaz_log(YLOG_WARN, "Missing name attribute in termlist");
                continue;
            }
            tl->name = nmem_strdup(nmem, name);
            tl->next = r->termlists;
            r->termlists = tl;
        }
        else
        {
            yaz_log(YLOG_FATAL, "Bad element: %s", n->name);
            return 0;
        }
    }
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
    r->service = 0;
    r->next = 0;

    for (n = node->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp(n->name, "listen"))
        {
            xmlChar *port = xmlGetProp(n, "port");
            xmlChar *host = xmlGetProp(n, "host");
            if (port)
                r->port = atoi(port);
            if (host)
                r->host = nmem_strdup(nmem, host);
        }
        else if (!strcmp(n->name, "proxy"))
        {
            xmlChar *port = xmlGetProp(n, "port");
            xmlChar *host = xmlGetProp(n, "host");
            if (port)
                r->proxy_port = atoi(port);
            if (host)
                r->proxy_host = nmem_strdup(nmem, host);
        }
        else if (!strcmp(n->name, "service"))
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
    return xsltParseStylesheetFile(path);
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
        if (!strcmp(n->name, "requestsyntax"))
        {
            xmlChar *content = xmlNodeGetContent(n);
            if (content)
                r->requestsyntax = nmem_strdup(nmem, content);
        }
        else if (!strcmp(n->name, "nativesyntax"))
        {
            xmlChar *name = xmlGetProp(n, "name");
            xmlChar *format = xmlGetProp(n, "format");
            xmlChar *encoding = xmlGetProp(n, "encoding");
            xmlChar *mapto = xmlGetProp(n, "mapto");
            if (!name)
            {
                yaz_log(YLOG_WARN, "Missing name in 'nativesyntax' element");
                return 0;
            }
            if (!strcmp(name, "iso2709"))
            {
                r->native_syntax = Nativesyn_iso2709;
                // Set a few defaults, too
                r->native_format = Nativeform_marc21;
                r->native_mapto = Nativemapto_marcxml;
                r->native_encoding = "marc-8";
                setup_marc(r);
            }
            else if (!strcmp(name, "xml"))
                r->native_syntax = Nativesyn_xml;
            else
            {
                yaz_log(YLOG_WARN, "Unknown native syntax name %s", name);
                return 0;
            }
            if (format)
            {
                if (!strcmp(format, "marc21") || !strcmp(format, "usmarc"))
                    r->native_format = Nativeform_marc21;
                else
                {
                    yaz_log(YLOG_WARN, "Unknown native format name %s", format);
                    return 0;
                }
            }
            if (encoding)
                r->native_encoding = encoding;
            if (mapto)
            {
                if (!strcmp(mapto, "marcxml"))
                    r->native_mapto = Nativemapto_marcxml;
                else if (!strcmp(mapto, "marcxchange"))
                    r->native_mapto = Nativemapto_marcxchange;
                else
                {
                    yaz_log(YLOG_WARN, "Unknown mapto target %s", format);
                    return 0;
                }
            }
        }
        else if (!strcmp(n->name, "map"))
        {
            struct conf_retrievalmap *m = nmem_malloc(nmem, sizeof(struct conf_retrievalmap));
            xmlChar *type = xmlGetProp(n, "type");
            xmlChar *charset = xmlGetProp(n, "charset");
            xmlChar *format = xmlGetProp(n, "format");
            xmlChar *stylesheet = xmlGetProp(n, "stylesheet");
            bzero(m, sizeof(*m));
            if (type)
            {
                if (!strcmp(type, "xslt"))
                    m->type = Map_xslt;
                else
                {
                    yaz_log(YLOG_WARN, "Unknown map type: %s", type);
                    return 0;
                }
            }
            if (charset)
                m->charset = nmem_strdup(nmem, charset);
            if (format)
                m->format = nmem_strdup(nmem, format);
            if (stylesheet)
            {
                if (!(m->stylesheet = load_stylesheet(stylesheet)))
                    return 0;
            }
            *rm = m;
            rm = &m->next;
        }
        else
        {
            yaz_log(YLOG_FATAL, "Bad element in retrievalprofile: %s", n->name);
            return 0;
        }
    }

    return r;
}

static struct conf_config *parse_config(xmlNode *root)
{
    xmlNode *n;
    struct conf_config *r = nmem_malloc(nmem, sizeof(struct conf_config));
    struct conf_retrievalprofile **rp = &r->retrievalprofiles;

    r->servers = 0;
    r->queryprofiles = 0;
    r->retrievalprofiles = 0;

    for (n = root->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp(n->name, "server"))
        {
            struct conf_server *tmp = parse_server(n);
            if (!tmp)
                return 0;
            tmp->next = r->servers;
            r->servers = tmp;
        }
        else if (!strcmp(n->name, "queryprofile"))
        {
        }
        else if (!strcmp(n->name, "retrievalprofile"))
        {
            if (!(*rp = parse_retrievalprofile(n)))
                return 0;
            rp = &(*rp)->next;
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
    xmlDoc *doc = xmlReadFile(fname, NULL, 0);
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
    if ((p = rindex(fname, '/')))
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
