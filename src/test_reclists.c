/* $Id: test_reclists.c,v 1.2 2007-05-11 08:41:07 marc Exp $
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

#if HAVE_CONFIG_H
#include "cconfig.h"
#endif

#define USE_TIMING 0
#if USE_TIMING
#include <yaz/timing.h>
#endif

#include <yaz/test.h>


#include "config.h"
//#include "record.h"
#include "reclists.h"


#if 0

void test_reclist_sortparms(int argc, char **argv)
{
  NMEM 	nmem = nmem_create();

  struct conf_service *service = 0; 
  service =  conf_service_create(nmem, 1, 2);

  conf_service_add_metadata(nmem, service, 0, "title",
                            Metadata_type_generic, Metadata_merge_unique,
                            1, 1, 1, 0);

  conf_service_add_sortkey(nmem, service, 0, "relevance",
                           Metadata_sortkey_relevance);
  
  conf_service_add_sortkey(nmem, service, 1, "title",
                           Metadata_sortkey_string);
  

  // initializing of sort parameters is controlled by service descriptions
  struct reclist_sortparms *sort_parms = 0;

  // sorting ascending according to relevance, then title
  YAZ_CHECK(0 == sort_parms);
  YAZ_CHECK(reclist_sortparms_insert(nmem, &sort_parms, service, "title", 1));
  YAZ_CHECK(sort_parms);

  YAZ_CHECK(0 == sort_parms->next);
  YAZ_CHECK(reclist_sortparms_insert(nmem, &sort_parms, service, 
                                     "relevance", 1));
  YAZ_CHECK(sort_parms->next);


  nmem_destroy(nmem);

  //YAZ_CHECK(0 == 0);
  //YAZ_CHECK_EQ(0, 1);
}


#endif 

int main(int argc, char **argv)
{
    YAZ_CHECK_INIT(argc, argv); 
    YAZ_CHECK_LOG(); 


    //test_reclist_sortparms(argc, argv); 

    
    YAZ_CHECK_TERM;
}




/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
