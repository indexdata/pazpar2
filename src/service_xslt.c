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
#include <yaz/xml_include.h>

#include "service_xslt.h"
#include "pazpar2_config.h"

struct service_xslt
{
    char *id;
    xsltStylesheetPtr xsp;
    struct service_xslt *next;
};

xsltStylesheetPtr service_xslt_get(struct conf_service *service,
                                   const char *id)
{
    struct service_xslt *sx;
    for (sx = service->xslt_list; sx; sx = sx->next)
        if (!strcmp(id, sx->id))
            return sx->xsp;
    return 0;
}

void service_xslt_destroy(struct conf_service *service)
{
    struct service_xslt *sx = service->xslt_list;
    for (; sx; sx = sx->next)
        xsltFreeStylesheet(sx->xsp);
}

int service_xslt_config(struct conf_service *service, xmlNode *n)
{
    xmlDoc *xsp_doc;
    xmlNode *root = n->children;
    struct service_xslt *sx;
    const char *id = 0;
    struct _xmlAttr *attr;
    for (attr = n->properties; attr; attr = attr->next)
        if (!strcmp((const char *) attr->name, "id"))
            id = (const char *) attr->children->content;
        else
        {
            yaz_log(YLOG_FATAL, "Invalid attribute %s for xslt element",
                    (const char *) attr->name);
            return -1;
        }
    if (!id)
    {
        yaz_log(YLOG_FATAL, "Missing attribute id for xslt element");
        return -1;
    }
    while (root && root->type != XML_ELEMENT_NODE)
        root = root->next;
    if (!root)
    {
        yaz_log(YLOG_FATAL, "Missing content for xslt element");
        return -1;
    }
    for (sx = service->xslt_list; sx; sx = sx->next)
        if (!strcmp(sx->id, id))
        {
            yaz_log(YLOG_FATAL, "Multiple xslt with id=%s", id);
            return -1;
        }

    sx = nmem_malloc(service->nmem, sizeof(*sx));
    sx->id = nmem_strdup(service->nmem, id);
    sx->next = service->xslt_list;
    service->xslt_list = sx;

    xsp_doc = xmlNewDoc(BAD_CAST "1.0");
    xmlDocSetRootElement(xsp_doc, xmlCopyNode(root, 1));
    sx->xsp = xsltParseStylesheetDoc(xsp_doc);
    if (!sx->xsp)
    {
        xmlFreeDoc(xsp_doc);
        yaz_log(YLOG_FATAL, "Failed to parse XSLT");
        return -1;
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

