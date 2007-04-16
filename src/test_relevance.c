/* $Id: test_relevance.c,v 1.1 2007-04-16 13:58:20 marc Exp $
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
  struct conf_service service; 
  struct record record;
  struct reclist list;
  struct record_cluster *cluster = 0;
  struct relevance *rel = 0;
  int numrecs = 10;
  char mergekey[128];
  //const char * terms[] = 
  //    {"ål",  "abe", "økologi", "fisk", "æble", "yoghurt"};
  const char * terms[] = 
      {"abe", "fisk"};
  int total = 0;
  struct record_metadata *metadata = 0;
  

  relevance_create(nmem, terms, numrecs);

  relevance_prepare_read(rel, &list); 

  cluster = reclist_insert(&service, &list, &record, mergekey, &total);

  //relevance_newrec(rel, cluster);

  //relevance_donerecord(se->relevance, cluster);
  //          relevance_countwords(se->relevance, cluster, 
  //                                   (char *) value, md->rank);
  //      

  nmem_destroy(nmem);

  YAZ_CHECK(0 == 0);
  //YAZ_CHECK_EQ(0, 1);

  

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
