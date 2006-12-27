/* $Id: config.c,v 1.2 2006-12-27 21:11:10 quinn Exp $ */

#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <yaz/yaz-util.h>
#include <yaz/nmem.h>

#define CONFIG_NOEXTERNS
#include "config.h"

static NMEM nmem = 0;

struct conf_config *config = 0;

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

static struct conf_config *parse_config(xmlNode *root)
{
    xmlNode *n;
    struct conf_config *r = nmem_malloc(nmem, sizeof(struct conf_config));

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
    if (!nmem)
        nmem = nmem_create();
    if (!doc)
    {
        yaz_log(YLOG_FATAL, "Failed to read %s", fname);
        exit(1);
    }
    if ((config = parse_config(xmlDocGetRootElement(doc))))
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
