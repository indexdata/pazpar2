/* This file is part of Pazpar2.
   Copyright (C) 2006-2013 Index Data

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

#ifndef RECORD_H
#define RECORD_H


struct client;
struct conf_service;

union data_types {
    struct {
        const char *disp;
        const char *sort;
        const char *snippet;
    } text;
    struct {
        int min;
        int max;
    } number;
    double fnumber;
};


struct record_metadata_attr {
    char *name;
    char *value;
    struct record_metadata_attr *next;
};

struct record_metadata {
    union data_types data;
    // next item of this name
    struct record_metadata *next;
    struct record_metadata_attr *attributes;
};

union data_types * data_types_assign(NMEM nmem,
                                     union data_types ** data1,
                                     union data_types data2);


struct record {
    struct client *client;
    // Array mirrors list of metadata fields in config
    struct record_metadata **metadata;
    // Array mirrors list of sortkey fields in config
    union data_types **sortkeys;
    // Next in cluster of merged records
    struct record *next;
    // client result set position;
    int position;
    // checksum
    unsigned checksum;
};


struct record * record_create(NMEM nmem, int num_metadata, int num_sortkeys,
                              struct client *client, int position);

struct record_metadata * record_metadata_create(NMEM nmem);

int record_compare(struct record *r1, struct record *r2, struct conf_service *service);

struct record_cluster
{
    // Array mirrors list of metadata fields in config
    struct record_metadata **metadata;
    union data_types **sortkeys;
    // char *merge_key;
    struct record_metadata_attr *merge_keys;

    int relevance_score;
    int *term_frequency_vec;
    float *term_frequency_vecf;
    // Set-specific ID for this record
    char *recid;
    WRBUF relevance_explain1;
    WRBUF relevance_explain2;
    struct record *records;
    struct record_cluster *sorted_next;
    struct reclist_sortparms *sort_parms;
};

#endif // RECORD_H

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

