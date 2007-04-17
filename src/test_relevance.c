/* $Id: test_relevance.c,v 1.2 2007-04-17 13:30:24 marc Exp $
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
#include "relevance.h"








void test_relevance(int argc, char **argv)
{
  NMEM 	nmem = nmem_create();
  int numrecs = 10;

  const char * queryterms[] = 
      {"abe", "fisk", 0};
  //    {"ål", "økologi", "æble", 0};

  struct record_cluster *cluster = 0;
  struct conf_service service; 
  struct reclist *list = 0;
  struct record *record = 0;
  const char *mergekey = "amergekey";
  int total = 0;

  struct relevance *rel = 0;

  rel = relevance_create(nmem, queryterms, numrecs);
  list = reclist_create(nmem, numrecs);

  // setting up service - sic!
  // this should have been done by a nice service_create function !!!

  service.num_metadata = 1;
  service.metadata = 0;
  service.num_sortkeys = 0;
  service.sortkeys = 0;
  if (service.num_metadata)
      service.metadata 
          = nmem_malloc(nmem, 
                        sizeof(struct conf_metadata) * service.num_metadata);
  else
      service.metadata = 0;
  service.metadata->name = nmem_strdup(nmem, "aname");
  //service.metadata->brief = 1;
  //service.metadata->termlist = 1;
  //service.metadata->rank = 1;
  //service.metadata->type = Metadata_type_generic;
  //service.metadata->type = Metadata_type_year;
  //service.metadata->merge = Metadata_merge_no;
  //service.metadata->merge = Metadata_merge_unique;
  //service.metadata->merge = Metadata_merge_longest;
  //service.metadata->merge = Metadata_merge_range;
  //service.metadata->merge = Metadata_merge_all;


  if (service.num_sortkeys)
      service.sortkeys 
          = nmem_malloc(nmem, 
                        sizeof(struct conf_sortkey) * service.num_sortkeys);
  else
      service.sortkeys = 0;
  // service.sortkeys.type = Metadata_sortkey_numeric;
  // service.sortkeys.type = Metadata_sortkey_skiparticle;
  // service.sortkeys.name = service.metadata->name;
  //service.metadata->sortkey_offset = sk_node;



  // preparing one record
  // this should have been done by a nice record_create function
  // why the heck does the record know which client it belongs to ?? 

  record = nmem_malloc(nmem, sizeof(struct record));
  record->next = 0;
  // which client should I use for record->client = cl;  ??
  record->client = 0;
  // and which sortkeys data_types list should I use ??
  record->sortkeys = 0;
  record->metadata 
      = nmem_malloc(nmem, 
                    sizeof(struct record_metadata*) * service.num_metadata);
  memset(record->metadata, 0, 
         sizeof(struct record_metadata*) * service.num_metadata);


  // now we need to put some actual data into the record ... how ??
  // there is a hell of a lot spagetti code in logic.c ingest_record()
  // line 694 ff snippet from there:
  // this code needs to be analyzed and the fundamental stuff extracted

#if 0 
  service->metadata[imeta].name;
  md = &service->metadata[imeta];
   if (md->sortkey_offset >= 0)
                        sk = &service->sortkeys[md->sortkey_offset];

 
  // Find out where we are putting it           if (md->merge == Metadata_merge_no)
                wheretoput = &res->metadata[imeta];
            else
                wheretoput = &cluster->metadata[imeta];
            
            
            // Put it there
            newm = nmem_malloc(se->nmem, sizeof(struct record_metadata));
            newm->next = 0;
            if (md->type == Metadata_type_generic)
            {
                char *p, *pe;
                for (p = (char *) value; *p && isspace(*p); p++)
                    ;
                for (pe = p + strlen(p) - 1;
                        pe > p && strchr(" ,/.:([", *pe); pe--)
                    *pe = '\0';
                newm->data.text = nmem_strdup(se->nmem, p);

            }
            else if (md->type == Metadata_type_year)
            {
                if (extract_years((char *) value, &first, &last) < 0)
                    continue;
            }
            else
            {
                yaz_log(YLOG_WARN, "Unknown type in metadata element %s", type);
                continue;
            }
            if (md->type == Metadata_type_year && md->merge != Metadata_merge_range)
            {
                yaz_log(YLOG_WARN, "Only range merging supported for years");
                continue;
            }
            if (md->merge == Metadata_merge_unique)
            {
                struct record_metadata *mnode;
                for (mnode = *wheretoput; mnode; mnode = mnode->next)
                    if (!strcmp((const char *) mnode->data.text, newm->data.text))
                        break;
                if (!mnode)
                {
                    newm->next = *wheretoput;
                    *wheretoput = newm;
                }
            }
            else if (md->merge == Metadata_merge_longest)
            {
                if (!*wheretoput ||
                        strlen(newm->data.text) > strlen((*wheretoput)->data.text))
                {
                    *wheretoput = newm;
                    if (sk)
                    {
                        char *s = nmem_strdup(se->nmem, newm->data.text);
                        if (!cluster->sortkeys[md->sortkey_offset])
                            cluster->sortkeys[md->sortkey_offset] = 
                                nmem_malloc(se->nmem, sizeof(union data_types));
                        normalize_mergekey(s,
                                (sk->type == Metadata_sortkey_skiparticle));
                        cluster->sortkeys[md->sortkey_offset]->text = s;
                    }
                }
            }
            else if (md->merge == Metadata_merge_all || md->merge == Metadata_merge_no)
            {
                newm->next = *wheretoput;
                *wheretoput = newm;
            }
            else if (md->merge == Metadata_merge_range)
            {
                assert(md->type == Metadata_type_year);
                if (!*wheretoput)
                {
                    *wheretoput = newm;
                    (*wheretoput)->data.number.min = first;
                    (*wheretoput)->data.number.max = last;
                    if (sk)
                        cluster->sortkeys[md->sortkey_offset] = &newm->data;
                }
                else
                {
                    if (first < (*wheretoput)->data.number.min)
                        (*wheretoput)->data.number.min = first;
                    if (last > (*wheretoput)->data.number.max)
                        (*wheretoput)->data.number.max = last;
                }
            if (md->rank)
                relevance_countwords(se->relevance, cluster, 
                                     (char *) value, md->rank);
            if (md->termlist)
            {
                if (md->type == Metadata_type_year)
                {
                    char year[64];
                    sprintf(year, "%d", last);
                    add_facet(se, (char *) type, year);
                    if (first != last)
                    {
                        sprintf(year, "%d", first);
                        add_facet(se, (char *) type, year);
                    }
                }
                else
                    add_facet(se, (char *) type, (char *) value);
            }
#endif

  //mergekey_norm = (xmlChar *) nmem_strdup(se->nmem, (char*) mergekey);
  //normalize_mergekey((char *) mergekey_norm, 0);


  // insert one record into recordlist/cluster - what's a cluster, exactly??
  cluster = reclist_insert(list, &service, record, (char *)mergekey, &total);


  relevance_newrec(rel, cluster);







  //relevance_prepare_read(rel, list);


  //relevance_donerecord(rel, cluster);
  // relevance_countwords(se->rel, cluster, 
  //                                   (char *) value, service.metadata->rank);
  //      


  nmem_destroy(nmem);

  YAZ_CHECK(0 == 0);
  YAZ_CHECK_EQ(0, 1);

}


int main(int argc, char **argv)
{
    YAZ_CHECK_INIT(argc, argv); 
    YAZ_CHECK_LOG(); 


    test_relevance(argc, argv); 

    
    YAZ_CHECK_TERM;
}




/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
