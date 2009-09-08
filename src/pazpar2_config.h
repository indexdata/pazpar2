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

#ifndef PAZPAR2_CONFIG_H
#define PAZPAR2_CONFIG_H

#include <libxslt/xslt.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

#include <yaz/nmem.h>
#include "charsets.h"

enum conf_metadata_type {
    Metadata_type_generic,    // Generic text field
    Metadata_type_number,     // A number
    Metadata_type_year,        // A number
    Metadata_type_date        // A number
};

enum conf_metadata_merge {
    Metadata_merge_no,        // Don't merge
    Metadata_merge_unique,    // Include unique elements in merged block
    Metadata_merge_longest,   // Include the longest (strlen) value
    Metadata_merge_range,     // Store value as a range of lowest-highest
    Metadata_merge_all        // Just include all elements found
};

enum conf_sortkey_type {
    Metadata_sortkey_relevance,
    Metadata_sortkey_numeric,       // Standard numerical sorting
    Metadata_sortkey_skiparticle,   // Skip leading article when sorting
    Metadata_sortkey_string         // Flat string
};

// This controls the ability to insert 'static' values from settings into retrieval recs
enum conf_setting_type {
    Metadata_setting_no,
    Metadata_setting_postproc,      // Insert setting value into normalized record
    Metadata_setting_parameter      // Expose value to normalization stylesheets
};

enum conf_metadata_mergekey {
    Metadata_mergekey_no,
    Metadata_mergekey_yes
};

// Describes known metadata elements and how they are to be manipulated
// An array of these structure provides a 'map' against which
// discovered metadata elements are matched. It also governs storage,
// to minimize number of cycles needed at various tages of processing
struct conf_metadata 
{
    char *name;  // The field name. Output by normalization stylesheet
    int brief;   // Is this element to be returned in the brief format?
    int termlist;// Is this field to be treated as a termlist for browsing?
    int rank;    // Rank factor. 0 means don't use this field for ranking, 
                 // 1 is default
                 // values >1  give additional significance to a field
    int sortkey_offset; // -1 if it's not a sortkey, otherwise index
                        // into service/record_cluster->sortkey array
    enum conf_metadata_type type;
    enum conf_metadata_merge merge;
    enum conf_setting_type setting; // Value is to be taken from session/db settings?
    enum conf_metadata_type mergekey;
};



// Controls sorting
struct conf_sortkey
{
    char *name;
    enum conf_sortkey_type type;
};

// It is conceivable that there will eventually be several 'services'
// offered from one server, with separate configuration -- possibly
// more than one services associated with the same port. For now,
// however, only a single service is possible.
struct conf_service
{
    int num_metadata;
    struct conf_metadata *metadata;
    int num_sortkeys;
    struct conf_sortkey *sortkeys;
    struct setting_dictionary *dictionary;
    struct conf_service *next;
    char *id;
    char *settings;
    NMEM nmem;

    /* duplicated from conf_server */
    pp2_charset_t relevance_pct;
    pp2_charset_t sort_pct;
    pp2_charset_t mergekey_pct;

    struct database *databases;
    struct conf_targetprofiles *targetprofiles;
};

struct conf_service * conf_service_create(int num_metadata, int num_sortkeys,
    const char *service_id);

struct conf_metadata* conf_service_add_metadata(struct conf_service *service,
                                                int field_id,
                                                const char *name,
                                                enum conf_metadata_type type,
                                                enum conf_metadata_merge merge,
                                                enum conf_setting_type setting,
                                                int brief,
                                                int termlist,
                                                int rank,
                                                int sortkey_offset,
                                                enum conf_metadata_mergekey mt);

struct conf_sortkey * conf_service_add_sortkey(struct conf_service *service,
                                               int field_id,
                                               const char *name,
                                               enum conf_sortkey_type type);


int conf_service_metadata_field_id(struct conf_service *service, const char * name);

int conf_service_sortkey_field_id(struct conf_service *service, const char * name);


struct conf_server
{
    char *host;
    int port;
    char *proxy_host;
    int proxy_port;
    char *myurl;
    struct sockaddr_in *proxy_addr;

    char *server_settings;

    pp2_charset_t relevance_pct;
    pp2_charset_t sort_pct;
    pp2_charset_t mergekey_pct;
    struct conf_service *service;
    struct conf_server *next;
};

struct conf_targetprofiles
{
    enum {
        Targetprofiles_local
    } type;
    char *src;
};

struct conf_config
{
    NMEM nmem; /* for conf_config and servers memory */
    struct conf_server *servers;
};

struct conf_config *read_config(const char *fname);
xsltStylesheet *conf_load_stylesheet(const char *fname);

void config_read_settings(struct conf_config *config,
                          const char *path_override);

struct conf_service *locate_service(struct conf_server *server,
                                    const char *service_id);


#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

