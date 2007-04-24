/* $Id: test_record.c,v 1.2 2007-04-24 13:50:07 marc Exp $
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <ctype.h>
#include <assert.h>

#if HAVE_CONFIG_H
#include "cconfig.h"
#endif

#define USE_TIMING 0
#if USE_TIMING
#include <yaz/timing.h>
#endif

#include <yaz/test.h>


//#include "pazpar2.h"
#include "config.h"
#include "record.h"
//#include "pazpar2.h"


void test_record(int argc, char **argv)
{
  NMEM 	nmem = nmem_create();

  struct conf_service *service = 0; 
  struct record *record = 0;

  struct client *client = 0;

  service =  conf_service_create(nmem, 4, 3);
  YAZ_CHECK(service);

  YAZ_CHECK(conf_service_add_metadata(nmem, service, 0, "title",
                            Metadata_type_generic, Metadata_merge_unique,
                            1, 1, 1, 0));

  YAZ_CHECK(conf_service_add_metadata(nmem, service, 1, "author",
                            Metadata_type_generic, Metadata_merge_longest,
                            1, 1, 1, 0));

  YAZ_CHECK(conf_service_add_metadata(nmem, service, 2, "isbn",
                            Metadata_type_number, Metadata_merge_no,
                            1, 1, 1, 0));

  YAZ_CHECK(conf_service_add_metadata(nmem, service, 3, "year",
                            Metadata_type_year, Metadata_merge_range,
                            1, 1, 1, 0));

  YAZ_CHECK(conf_service_add_sortkey(nmem, service, 0, "relevance",
                                     Metadata_sortkey_relevance));

  YAZ_CHECK(conf_service_add_sortkey(nmem, service, 1, "title",
                                     Metadata_sortkey_string));
  
  YAZ_CHECK(conf_service_add_sortkey(nmem, service, 2, "year",
                                     Metadata_sortkey_numeric));
  



  // testing record things
  record = record_create(nmem, 4, 3);
  YAZ_CHECK(record);

  // why on earth do we have a client dangeling from the record ??
  record->client = client;

  char * bla = "blabla";
  union data_types data_text;
  data_text.text = bla;

  
  union data_types data_num;
  data_num.number.min = 2;
  data_num.number.max = 5;

  struct record_metadata * tmp_md = 0;
  tmp_md = record_metadata_insert(nmem, &(record->metadata[0]), data_text);
  YAZ_CHECK(tmp_md);
  YAZ_CHECK(0 == record->metadata[0]->next);

  tmp_md = record_metadata_insert(nmem, &(record->metadata[0]->next), 
                                  data_text);
  YAZ_CHECK(tmp_md);
  YAZ_CHECK(record->metadata[0]->next);

  YAZ_CHECK(record_add_metadata_field_id(nmem, record, 3, data_num));
  YAZ_CHECK(0 == record->metadata[3]->next);
  YAZ_CHECK(record_add_metadata_field_id(nmem, record, 3, data_num));
  YAZ_CHECK(record->metadata[3]->next);

  YAZ_CHECK(record_add_metadata(nmem, record, service, "author", data_text));
  YAZ_CHECK(0 == record->metadata[1]->next);
  YAZ_CHECK(record_add_metadata(nmem, record, service, "author", data_text));
  YAZ_CHECK(record->metadata[1]->next);


  YAZ_CHECK(record_assign_sortkey_field_id(nmem, record, 0, data_text));
  YAZ_CHECK(record_assign_sortkey_field_id(nmem, record, 1, data_text));
  YAZ_CHECK(record_assign_sortkey_field_id(nmem, record, 2, data_num));


  YAZ_CHECK(record_assign_sortkey(nmem, record, service, "relevance", data_text));
  YAZ_CHECK(record_assign_sortkey(nmem, record, service, "title", data_text));
  YAZ_CHECK(record_assign_sortkey(nmem, record, service, "year", data_num));




  nmem_destroy(nmem);

  //YAZ_CHECK(0 == 0);
  //YAZ_CHECK_EQ(0, 1);
}


int main(int argc, char **argv)
{
    YAZ_CHECK_INIT(argc, argv); 
    YAZ_CHECK_LOG(); 


    test_record(argc, argv); 

    
    YAZ_CHECK_TERM;
}




/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
