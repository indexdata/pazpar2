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
#include "relevance.h"
#include "record.h"
#include "reclists.h"

#if 0

void test_relevance_7bit(int argc, char **argv)
{
  NMEM 	nmem = nmem_create();

  struct conf_service *service = 0; 
  service =  conf_service_create(nmem, 1, 1);

  conf_service_add_metadata(nmem, service, 0, "title",
                            Metadata_type_generic, Metadata_merge_unique,
                            1, 1, 1, 0);
  
  conf_service_add_sortkey(nmem, service, 0, "title",
                           Metadata_sortkey_string);

  //conf_service_add_sortkey(nmem, service, 1, "relevance",
  //                         Metadata_sortkey_relevance);
  



  // setting up records
  
  // why on earth do we have a client dangeling from the record ??
  // record->client = client;

  union data_types data_ape = {"ape"};
  union data_types data_bee = {"bee"};
  union data_types data_fish = {"fish"};
  union data_types data_zebra = {"zebra"};
  

  //union data_types data_year;
  //data_num.number.min = 2005;
  //data_num.number.max = 2007;

  int no_recs = 4;

  const char *mk_ape_fish = "ape fish";
  struct record *rec_ape_fish = 0;
  rec_ape_fish 
      = record_create(nmem, service->num_metadata, service->num_sortkeys);
  record_add_metadata(nmem, rec_ape_fish, service, "title", data_ape);
  //record_assign_sortkey(nmem, rec_ape_fish, service, "relevance", data_ape);
  record_assign_sortkey(nmem, rec_ape_fish, service, "title", data_ape);
  record_add_metadata(nmem, rec_ape_fish, service, "title", data_fish);
  YAZ_CHECK(rec_ape_fish);  

  const char *mk_bee_fish = "bee fish";
  struct record *rec_bee_fish = 0;
  rec_bee_fish 
      = record_create(nmem, service->num_metadata, service->num_sortkeys);
  record_add_metadata(nmem, rec_bee_fish, service, "title", data_bee);
  //record_assign_sortkey(nmem, rec_bee_fish, service, "relevance", data_bee);
  record_assign_sortkey(nmem, rec_bee_fish, service, "title", data_bee);
  record_add_metadata(nmem, rec_bee_fish, service, "title", data_fish);
  YAZ_CHECK(rec_bee_fish);
 
  const char *mk_fish_bee = "fish bee";
  struct record *rec_fish_bee = 0;
  rec_fish_bee 
      = record_create(nmem, service->num_metadata, service->num_sortkeys);
  record_add_metadata(nmem, rec_fish_bee, service, "title", data_fish);
  //record_assign_sortkey(nmem, rec_fish_bee, service, "relevance", data_fish);
  record_assign_sortkey(nmem, rec_fish_bee, service, "title", data_fish);
  record_add_metadata(nmem, rec_fish_bee, service, "title", data_bee);
  YAZ_CHECK(rec_fish_bee);
  
  const char *mk_zebra_bee = "zebra bee";
  struct record *rec_zebra_bee = 0;
    rec_zebra_bee 
      = record_create(nmem, service->num_metadata, service->num_sortkeys);
  record_add_metadata(nmem, rec_zebra_bee, service, "title", data_zebra);
  //record_assign_sortkey(nmem, rec_zebra_bee, service, "relevance", data_zebra);
  record_assign_sortkey(nmem, rec_zebra_bee, service, "title", data_zebra);
  record_add_metadata(nmem, rec_zebra_bee, service, "title", data_bee);
  YAZ_CHECK(rec_zebra_bee);

  
  struct reclist *list = 0;
  list = reclist_create(nmem, no_recs);
  YAZ_CHECK(list);

  int no_merged = 0;


  const char * queryterms[] = 
      {"ape", "fish", 0};
  //    {"ål", "økologi", "æble", 0};


  //struct relevance *rel = 0;
  //rel = relevance_create(nmem, queryterms, no_recs);
  //YAZ_CHECK(rel);
  
  struct record_cluster *cluster = 0;


  // insert records into recordlist and get clusters 
  // since metadata keys differ, we get multiple clusters ?? 
  cluster 
      = reclist_insert(list, service, rec_ape_fish, mk_ape_fish, &no_merged);
  YAZ_CHECK(cluster);
  data_types_assign(nmem, &cluster->sortkeys[0], *rec_ape_fish->sortkeys[0]);
  //relevance_newrec(rel, cluster);

  cluster 
      = reclist_insert(list, service, rec_bee_fish, mk_bee_fish, &no_merged);
  YAZ_CHECK(cluster);
  data_types_assign(nmem, &cluster->sortkeys[0], *rec_bee_fish->sortkeys[0]);
  //relevance_newrec(rel, cluster);

  cluster 
      = reclist_insert(list, service, rec_fish_bee, mk_fish_bee, &no_merged);
  YAZ_CHECK(cluster);
  data_types_assign(nmem, &cluster->sortkeys[0], *rec_fish_bee->sortkeys[0]);
  //relevance_newrec(rel, cluster);

  cluster 
      = reclist_insert(list, service, rec_zebra_bee, mk_zebra_bee, &no_merged);
  YAZ_CHECK(cluster);
  data_types_assign(nmem, &cluster->sortkeys[0], *rec_zebra_bee->sortkeys[0]);
  //relevance_newrec(rel, cluster);


  YAZ_CHECK(no_recs == no_merged);

  // now sorting according to sorting criteria, here ascending title
  struct reclist_sortparms *sort_parms = 0;

  reclist_sortparms_insert(nmem, &sort_parms, service, "title", 1);

  //reclist_sortparms_insert(nmem, &sort_parms, service, "relevance", 1);

  // crashes with a fat segmentation fault! To be traced tomorrow
  reclist_sort(list, sort_parms);
  

                        
  //mergekey_norm = (xmlChar *) nmem_strdup(se->nmem, (char*) mergekey);
  //normalize_mergekey((char *) mergekey_norm, 0);






  //relevance_prepare_read(rel, list);


  //relevance_donerecord(rel, cluster);
  // relevance_countwords(se->rel, cluster, 
  //                                   (char *) value, service->metadata->rank);
  //      


  nmem_destroy(nmem);

  //YAZ_CHECK(0 == 0);
  //YAZ_CHECK_EQ(0, 1);
}

#endif

int main(int argc, char **argv)
{
    YAZ_CHECK_INIT(argc, argv); 
    YAZ_CHECK_LOG(); 


    //test_relevance_7bit(argc, argv); 

    
    YAZ_CHECK_TERM;
}




/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
