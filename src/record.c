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

#include <string.h>

#include <yaz/yaz-util.h>
#include <yaz/nmem.h>

#if HAVE_CONFIG_H
#include <config.h>
#endif

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
    rec_md->attributes = 0;
    return rec_md;
}


int record_compare(struct record *r1, struct record *r2,
                   struct conf_service *service)
{
    int i;
    for (i = 0; i < service->num_metadata; i++)
    {
        struct conf_metadata *ser_md = &service->metadata[i];
        enum conf_metadata_type type = ser_md->type;
            
        struct record_metadata *m1 = r1->metadata[i];
        struct record_metadata *m2 = r2->metadata[i];
        while (m1 && m2)
        {
            switch (type)
            {
            case Metadata_type_generic:
                if (strcmp(m1->data.text.disp, m2->data.text.disp))
                    return 0;
                break;
            case Metadata_type_date:
            case Metadata_type_year:
                if (m1->data.number.min != m2->data.number.min ||
                    m1->data.number.max != m2->data.number.max)
                    return 0;
                break;
            }
            m1 = m1->next;
            m2 = m2->next;
        }
        if (m1 || m2)
            return 0;
    }
    return 1;
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

