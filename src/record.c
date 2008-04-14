/* This file is part of Pazpar2.
   Copyright (C) 2006-2008 Index Data

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

/* $Id: record.c,v 1.10 2007-06-13 21:29:04 adam Exp $ */


#include <string.h>

#include <yaz/yaz-util.h>
#include <yaz/nmem.h>

#if HAVE_CONFIG_H
#include <config.h>
#endif

//#define CONFIG_NOEXTERNS
#include "pazpar2_config.h"
#include "record.h"



union data_types * data_types_assign(NMEM nmem, 
                                     union data_types ** data1, 
                                     union data_types data2)
{
    // assert(nmem);

    if (!data1)
        return 0;

    if (!*data1){
        if (!nmem)
            return 0;
        else
            *data1  = nmem_malloc(nmem, sizeof(union data_types));
    }
    
    **data1 = data2;
    return *data1;
}


struct record * record_create(NMEM nmem, int num_metadata, int num_sortkeys,
                              struct client *client, int position)
{
    struct record * record = 0;
    int i = 0;
    
    // assert(nmem);

    record = nmem_malloc(nmem, sizeof(struct record));

    record->next = 0;
    record->client = client;

    record->metadata 
        = nmem_malloc(nmem, 
                      sizeof(struct record_metadata*) * num_metadata);
    for (i = 0; i < num_metadata; i++)
        record->metadata[i] = 0;
    
    record->sortkeys  
        = nmem_malloc(nmem, 
                      sizeof(union data_types*) * num_sortkeys);
    for (i = 0; i < num_sortkeys; i++)
        record->sortkeys[i] = 0;

    record->position = position;
    
    return record;
}

struct record_metadata * record_metadata_create(NMEM nmem)
{
    struct record_metadata * rec_md 
        = nmem_malloc(nmem, sizeof(struct record_metadata));
    rec_md->next = 0;
    return rec_md;
}


struct record_metadata * record_metadata_insert(NMEM nmem, 
                                                struct record_metadata ** rmd,
                                                union data_types data)
{
    struct record_metadata * tmp_rmd = 0;
    // assert(nmem);

    if(!rmd)
        return 0;

    // construct new record_metadata
    tmp_rmd  = nmem_malloc(nmem, sizeof(struct record_metadata));
    tmp_rmd->data = data;


    // insert in *rmd's place, moving *rmd one down the list
    tmp_rmd->next = *rmd;
    *rmd = tmp_rmd;

    return *rmd;
}

struct record_metadata * record_add_metadata_field_id(NMEM nmem, 
                                                     struct record * record,
                                                     int field_id, 
                                                     union data_types data)
{
    if (field_id < 0 || !record || !record->metadata)
        return 0;

    return record_metadata_insert(nmem, &(record->metadata[field_id]), data);
}


struct record_metadata * record_add_metadata(NMEM nmem, 
                                             struct record * record,
                                             struct conf_service * service,
                                             const char * name,
                                             union data_types data)
{
    int field_id = 0;

    if (!record || !record->metadata || !service || !name)  
        return 0;
    
    field_id = conf_service_metadata_field_id(service, name);

    if (-1 == field_id)
        return 0;
    
    return record_metadata_insert(nmem, &(record->metadata[field_id]), data);
}






union data_types * record_assign_sortkey_field_id(NMEM nmem, 
                                               struct record * record,
                                               int field_id, 
                                               union data_types data)
{
    if (field_id < 0 || !record || !record->sortkeys)
        return 0;

    return data_types_assign(nmem, &(record->sortkeys[field_id]), data);
}



union data_types * record_assign_sortkey(NMEM nmem, 
                                      struct record * record,
                                      struct conf_service * service,
                                      const char * name,
                                      union data_types data)
{
    int field_id = 0;

    if (!record || !service || !name)  
        return 0;
    
    field_id = conf_service_sortkey_field_id(service, name);

    if (!(-1 < field_id) || !(field_id < service->num_sortkeys))
        return 0;

    return record_assign_sortkey_field_id(nmem, record, field_id, data);
}



/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
