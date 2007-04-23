/* $Id: record.h,v 1.3 2007-04-23 08:48:50 marc Exp $
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

#ifndef RECORD_H
#define RECORD_H


struct record;
struct client;
struct conf_service;

union data_types {
    char *text;
    struct {
        int min;
        int max;
    } number;
};

struct record_metadata {
    union data_types data;
    // next item of this name
    struct record_metadata *next; 
};

struct record {
    struct client *client;
    // Array mirrors list of metadata fields in config
    struct record_metadata **metadata; 
    // Array mirrors list of sortkey fields in config
    union data_types **sortkeys;
    // Next in cluster of merged records       
    struct record *next;  
};


struct record * record_create(NMEM nmem, int num_metadata, int num_sortkeys);

struct record_metadata * record_metadata_insert(NMEM nmem, 
                                                struct record_metadata ** rmd,
                                                union data_types data);


struct record_metadata * record_add_metadata_field_id(NMEM nmem, 
                                                      struct record * record,
                                                      int field_id, 
                                                      union data_types data);


struct record_metadata * record_add_metadata(NMEM nmem, 
                                             struct record * record,
                                             struct conf_service * service,
                                             const char * name,
                                             union data_types data);


struct record_cluster
{
    // Array mirrors list of metadata fields in config
    struct record_metadata **metadata; 
    union data_types **sortkeys;
    char *merge_key;
    int relevance;
    int *term_frequency_vec;
    // Set-specific ID for this record
    int recid; 
    struct record *records;
};


#endif // RECORD_H

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
