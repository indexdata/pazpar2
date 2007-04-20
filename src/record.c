/* $Id: record.c,v 1.1 2007-04-20 14:37:17 marc Exp $
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

/* $Id: record.c,v 1.1 2007-04-20 14:37:17 marc Exp $ */


#include <string.h>

#include <yaz/yaz-util.h>
#include <yaz/nmem.h>

#if HAVE_CONFIG_H
#include <cconfig.h>
#endif

//#define CONFIG_NOEXTERNS
#include "config.h"
#include "record.h"


struct record * record_create(NMEM nmem, int num_metadata, int num_sortkeys)
{
    struct record * record = 0;
    
    // assert(nmem);

    record = nmem_malloc(nmem, sizeof(struct record));

    record->next = 0;
    // which client should I use for record->client = cl;  ??
    record->client = 0;

    record->metadata 
        = nmem_malloc(nmem, 
                      sizeof(struct record_metadata*) * num_metadata);
    memset(record->metadata, 0, 
           sizeof(struct record_metadata*) * num_metadata);
    
    record->sortkeys  
        = nmem_malloc(nmem, 
                      sizeof(union data_types*) * num_sortkeys);
    memset(record->metadata, 0, 
           sizeof(union data_types*) * num_sortkeys);
    
    
    return record;
}


struct record_metadata * record_add_metadata_fieldno(NMEM nmem, 
                                                     struct record * record,
                                                     int fieldno, 
                                                     union data_types data)
{
    struct record_metadata * rmd = 0;
    
    if (!record || fieldno < 0 
        || !record->metadata || !record->metadata[fieldno] )
        return 0;

    // construct new record_metadata    
    rmd  = nmem_malloc(nmem, sizeof(struct record_metadata));
    rmd->data = data;
    rmd->next = 0;

    // still needs to be assigned ..

    return rmd;
    
};


/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
