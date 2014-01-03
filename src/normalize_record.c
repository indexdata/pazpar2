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

#include <string.h>

#include <yaz/yaz-util.h>
#include <yaz/nmem.h>

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "normalize_record.h"

#include "pazpar2_config.h"
#include "service_xslt.h"
#include "marcmap.h"
#include <libxslt/xslt.h>
#include <libxslt/transform.h>

struct normalize_step {
    struct normalize_step *next;
    xsltStylesheet *stylesheet;  /* created by normalize_record */
    xsltStylesheet *stylesheet2; /* external stylesheet (service) */
    struct marcmap *marcmap;
};

struct normalize_record_s {
    struct normalize_step *steps;
    NMEM nmem;
};

normalize_record_t normalize_record_create(struct conf_service *service,
                                           const char *spec)
{
    NMEM nmem = nmem_create();
    normalize_record_t nt = nmem_malloc(nmem, sizeof(*nt));
    struct normalize_step **m = &nt->steps;
    int no_errors = 0;
    int embed = 0;

    if (*spec == '<')
        embed = 1;

    nt->nmem = nmem;

    if (embed)
    {
        xmlDoc *xsp_doc = xmlParseMemory(spec, strlen(spec));

        if (!xsp_doc)
            no_errors++;
        {
            *m = nmem_malloc(nt->nmem, sizeof(**m));
            (*m)->marcmap = NULL;
            (*m)->stylesheet = NULL;
            (*m)->stylesheet2 = NULL;


            (*m)->stylesheet = xsltParseStylesheetDoc(xsp_doc);
            if (!(*m)->stylesheet)
                no_errors++;
            m = &(*m)->next;
        }
    }
    else
    {
        struct conf_config *conf = service->server->config;
        int i, num;
        char **stylesheets;
        nmem_strsplit(nt->nmem, ",", spec, &stylesheets, &num);

        for (i = 0; i < num; i++)
        {
            WRBUF fname = conf_get_fname(conf, stylesheets[i]);

            *m = nmem_malloc(nt->nmem, sizeof(**m));
            (*m)->marcmap = NULL;
            (*m)->stylesheet = NULL;

            (*m)->stylesheet2 = service_xslt_get(service, stylesheets[i]);
            if ((*m)->stylesheet2)
                ;
            else if (!strcmp(&stylesheets[i][strlen(stylesheets[i])-4], ".xsl"))
            {
                if (!((*m)->stylesheet =
                      xsltParseStylesheetFile((xmlChar *) wrbuf_cstr(fname))))
                {
                    yaz_log(YLOG_FATAL|YLOG_ERRNO, "Unable to load stylesheet: %s",
                            stylesheets[i]);
                    no_errors++;
                }
            }
            else if (!strcmp(&stylesheets[i][strlen(stylesheets[i])-5], ".mmap"))
            {
                if (!((*m)->marcmap = marcmap_load(wrbuf_cstr(fname), nt->nmem)))
                {
                    yaz_log(YLOG_FATAL|YLOG_ERRNO, "Unable to load marcmap: %s",
                            stylesheets[i]);
                    no_errors++;
                }
            }
            else
            {
                yaz_log(YLOG_FATAL, "Cannot handle stylesheet: %s", stylesheets[i]);
                no_errors++;
            }

            wrbuf_destroy(fname);
            m = &(*m)->next;
        }
    }
    *m = 0;  /* terminate list of steps */

    if (no_errors)
    {
        normalize_record_destroy(nt);
        nt = 0;
    }
    return nt;
}

void normalize_record_destroy(normalize_record_t nt)
{
    if (nt)
    {
        struct normalize_step *m;
        for (m = nt->steps; m; m = m->next)
        {
            if (m->stylesheet)
                xsltFreeStylesheet(m->stylesheet);
        }
        nmem_destroy(nt->nmem);
    }
}

int normalize_record_transform(normalize_record_t nt, xmlDoc **doc,
                               const char **parms)
{
    if (nt)
    {
        struct normalize_step *m;
	for (m = nt->steps; m; m = m->next)
	{
	    xmlNodePtr root = 0;
	    xmlDoc *ndoc;
	    if (m->stylesheet)
		ndoc = xsltApplyStylesheet(m->stylesheet, *doc, parms);
	    else if (m->stylesheet2)
		ndoc = xsltApplyStylesheet(m->stylesheet2, *doc, parms);
	    else if (m->marcmap)
		ndoc = marcmap_apply(m->marcmap, *doc);
            else
                ndoc = 0;
	    xmlFreeDoc(*doc);
            *doc = 0;

            if (ndoc)
                root = xmlDocGetRootElement(ndoc);

            if (ndoc && root && root->children)
                *doc = ndoc;
            else
	    {
                if (!ndoc)
                    yaz_log(YLOG_WARN, "XSLT produced no document");
                else if (!root)
                    yaz_log(YLOG_WARN, "XSLT produced XML with no root node");
                else if (!root->children)
                    yaz_log(YLOG_WARN, "XSLT produced XML with no root children nodes");
		if (ndoc)
		    xmlFreeDoc(ndoc);
		return -1;
	    }
	}
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

