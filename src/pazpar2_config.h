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

#ifndef PAZPAR2_CONFIG_H
#define PAZPAR2_CONFIG_H

#include "normalize_cache.h"

#include <yaz/nmem.h>
#include <yaz/mutex.h>
#include <yaz/ccl.h>
#include "charsets.h"
#include "http.h"
#include "database.h"

enum conf_metadata_type {
    Metadata_type_generic,    // Generic text field
    Metadata_type_year,       // year YYYY - YYYY
    Metadata_type_date,       // date YYYYMMDD - YYYYMMDD
    Metadata_type_float,      // float number
    Metadata_type_skiparticle,
    Metadata_type_relevance,
    Metadata_type_position,
};

enum conf_metadata_merge {
    Metadata_merge_no,        // Don't merge
    Metadata_merge_unique,    // Include unique elements in merged block
    Metadata_merge_longest,   // Include the longest (strlen) value
    Metadata_merge_range,     // Store value as a range of lowest-highest
    Metadata_merge_all,       // Just include all elements found
    Metadata_merge_first      // All from first target
};

// This controls the ability to insert 'static' values from settings into retrieval recs
enum conf_setting_type {
    Metadata_setting_no,
    Metadata_setting_postproc,      // Insert setting value into normalized record
    Metadata_setting_parameter      // Expose value to normalization stylesheets
};

enum conf_metadata_mergekey {
    Metadata_mergekey_no,
    Metadata_mergekey_optional,
    Metadata_mergekey_required
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
    const char *rank;
    int sortkey_offset; // -1 if it's not a sortkey, otherwise index
                        // into service/record_cluster->sortkey array
    enum conf_metadata_type type;
    enum conf_metadata_merge merge;
    enum conf_setting_type setting; // Value is to be taken from session/db settings?
    enum conf_metadata_mergekey mergekey;
    char *facetrule;

    char *limitmap;  // Should be expanded into service-wide default e.g. pz:limitmap:<name>=value setting
    char *limitcluster;
};



// Controls sorting
struct conf_sortkey
{
    char *name;
    enum conf_metadata_type type;
};

struct conf_server;

// It is conceivable that there will eventually be several 'services'
// offered from one server, with separate configuration -- possibly
// more than one services associated with the same port. For now,
// however, only a single service is possible.
struct conf_service
{
    YAZ_MUTEX mutex;
    int num_metadata;
    struct conf_metadata *metadata;
    int num_sortkeys;
    struct conf_sortkey *sortkeys;
    struct setting_dictionary *dictionary;
    struct settings_array *settings;
    struct conf_service *next;
    char *id;
    NMEM nmem;
    int session_timeout;
    int z3950_session_timeout;
    int z3950_operation_timeout;
    int rank_cluster;
    int rank_debug;
    double rank_follow;
    double rank_lead;
    int rank_length;
    char *default_sort;

    int ref_count;
    /* duplicated from conf_server */
    pp2_charset_fact_t charsets;

    struct service_xslt *xslt_list;

    CCL_bibset ccl_bibset;
    struct database *databases;
    struct conf_server *server;
    char *xml_node;
};

int conf_service_metadata_field_id(struct conf_service *service, const char * name);

int conf_service_sortkey_field_id(struct conf_service *service, const char * name);

struct conf_server
{
    char *host;
    char *port;
    char *proxy_host;
    int proxy_port;
    char *myurl;
    char *settings_fname;
    char *server_id;

    pp2_charset_fact_t charsets;

    struct conf_service *service;
    struct conf_server *next;
    struct conf_config *config;
    http_server_t http_server;
    iochan_man_t iochan_man;
};

struct conf_config *config_create(const char *fname);
void config_destroy(struct conf_config *config);
void config_process_events(struct conf_config *config);
void info_services(struct conf_server *server, WRBUF w);

struct conf_service *locate_service(struct conf_server *server,
                                    const char *service_id);

struct conf_service *service_create(struct conf_server *server,
                                    xmlNode *node);
void service_incref(struct conf_service *service);
void service_destroy(struct conf_service *service);

int config_start_listeners(struct conf_config *conf,
                           const char *listener_override,
                           const char *record_fname);

void config_stop_listeners(struct conf_config *conf);

WRBUF conf_get_fname(struct conf_config *config, const char *fname);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

