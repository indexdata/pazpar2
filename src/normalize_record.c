/* This file is part of Pazpar2.
   Copyright (C) 2006-2009 Index Data

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

#include "marcmap.h"
#include <libxslt/xslt.h>
#include <libxslt/transform.h>

struct normalize_step {
    struct normalize_step *next;
    xsltStylesheet *stylesheet;
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
    int i, num;
    int no_errors = 0;
    char **stylesheets;

    nt->nmem = nmem;

    nmem_strsplit(nt->nmem, ",", spec, &stylesheets, &num);
    for (i = 0; i < num; i++)
    {
        WRBUF fname = conf_get_fname(service, stylesheets[i]);
        
        *m = nmem_malloc(nt->nmem, sizeof(**m));
        (*m)->marcmap = NULL;
        (*m)->stylesheet = NULL;
        
        // XSLT
        if (!strcmp(&stylesheets[i][strlen(stylesheets[i])-4], ".xsl")) 
        {    
            if (!((*m)->stylesheet =
                  xsltParseStylesheetFile((xmlChar *) wrbuf_cstr(fname))))
            {
                yaz_log(YLOG_FATAL|YLOG_ERRNO, "Unable to load stylesheet: %s",
                        stylesheets[i]);
                no_errors++;
            }
        }
        // marcmap
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
    struct normalize_step *m;
    if (nt) {
	for (m = nt->steps; m; m = m->next)
	{
	    xmlNodePtr root = 0;
	    xmlDoc *new;
	    if (m->stylesheet)
	    {
		new = xsltApplyStylesheet(m->stylesheet, *doc, parms);
	    }
	    else if (m->marcmap)
	    {
		new = marcmap_apply(m->marcmap, *doc);
	    }
	    
	    root = xmlDocGetRootElement(new);
	    
	    if (!new || !root || !root->children)
	    {
		if (new)
		    xmlFreeDoc(new);
		xmlFreeDoc(*doc);
		return -1;
	    }
	    xmlFreeDoc(*doc);
	    *doc = new;
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

