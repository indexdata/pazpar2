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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#define USE_TIMING 0
#if USE_TIMING
#include <yaz/timing.h>
#endif

#include <yaz/test.h>

#include "pazpar2_config.h"



void test_conf_service(int argc, char **argv)
{
    struct conf_service *service = 0;
    service = conf_service_create(0, 4, 3, 0);

    YAZ_CHECK(service);

    // expected metadata failures
    YAZ_CHECK(!conf_service_add_metadata(0, 0, "service_needed",
                                         Metadata_type_generic, 
                                         Metadata_merge_unique,
                                         Metadata_setting_no,
                                         1, 1, 1, 0,
                                         Metadata_mergekey_no));

    YAZ_CHECK(!conf_service_add_metadata(service, -1, "out_of_bounds",
                                         Metadata_type_generic,
                                         Metadata_merge_unique,
                                         Metadata_setting_no,
                                         1, 1, 1, 0,
                                         Metadata_mergekey_no));

    YAZ_CHECK(!conf_service_add_metadata(service, 4, "out_of_bounds",
                                         Metadata_type_generic,
                                         Metadata_merge_unique,
                                         Metadata_setting_no,
                                         1, 1, 1, 0,
                                         Metadata_mergekey_no));

    YAZ_CHECK(!conf_service_add_metadata(service, 0, 0,  //missing name
                                         Metadata_type_generic,
                                         Metadata_merge_unique,
                                         Metadata_setting_no,
                                         1, 1, 1, 0,
                                         Metadata_mergekey_no));

    // expected metadata sucesses
    YAZ_CHECK(conf_service_add_metadata(service, 0, "title",
                                        Metadata_type_generic,
                                        Metadata_merge_unique,
                                        Metadata_setting_no,
                                        1, 1, 1, 0,
                                        Metadata_mergekey_no));

    YAZ_CHECK(conf_service_add_metadata(service, 1, "author",
                                        Metadata_type_generic,
                                        Metadata_merge_longest,
                                        Metadata_setting_no,
                                        1, 1, 1, 0,
                                        Metadata_mergekey_no));

    YAZ_CHECK(conf_service_add_metadata(service, 2, "isbn",
                                        Metadata_type_number,
                                        Metadata_merge_no,
                                        Metadata_setting_no,
                                        1, 1, 1, 0,
                                        Metadata_mergekey_no));

    YAZ_CHECK(conf_service_add_metadata(service, 3, "year",
                                        Metadata_type_year,
                                        Metadata_merge_range,
                                        Metadata_setting_no,
                                        1, 1, 1, 0,
                                        Metadata_mergekey_no));


    // expected sortkey failures
    YAZ_CHECK(!conf_service_add_sortkey(service, -1, "out_of_bounds",
                                        Metadata_sortkey_skiparticle));

    YAZ_CHECK(!conf_service_add_sortkey(service, -1, "out_of_bounds",
                                        Metadata_sortkey_string));

    YAZ_CHECK(!conf_service_add_sortkey(service, 3, "out_of_bounds",
                                        Metadata_sortkey_relevance));

    YAZ_CHECK(!conf_service_add_sortkey(service, 0, 0, //missing name
                                        Metadata_sortkey_relevance));


    // expected sortkey sucess
    YAZ_CHECK(conf_service_add_sortkey(service, 0, "relevance",
                                       Metadata_sortkey_relevance));

    YAZ_CHECK(conf_service_add_sortkey(service, 1, "title",
                                       Metadata_sortkey_string));
  
    YAZ_CHECK(conf_service_add_sortkey(service, 2, "year",
                                       Metadata_sortkey_numeric));
}


int main(int argc, char **argv)
{
    YAZ_CHECK_INIT(argc, argv); 
    YAZ_CHECK_LOG();

    test_conf_service(argc, argv);
    
    YAZ_CHECK_TERM;
}




/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

