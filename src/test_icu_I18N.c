/* $Id: test_icu_I18N.c,v 1.2 2007-04-30 13:56:52 marc Exp $
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



#ifdef HAVE_ICU
#include "icu_I18N.h"
#include "string.h"

void test_icu_I18N_casemap_en(int argc, char **argv)
{

    size_t buf_cap = 128;
    char buf[buf_cap];
    size_t dest8_len = 0;
    NMEM nmem = nmem_create();
    char * dest8 = 0;

    const char * src8 =  "A ReD fOx hunTS sQUirriLs";
    size_t src8_len = strlen(src8);
    
    printf("original string:   '%s' (%d)\n", src8, (int) src8_len);

    //these shall succeed
    dest8 = icu_casemap(nmem, buf, buf_cap, &dest8_len,
                        src8, "en", 'l');
    YAZ_CHECK(dest8_len);
    printf("icu_casemap 'en:l' '%s' (%d)\n", dest8, (int) dest8_len);


    dest8 = icu_casemap(nmem, buf, buf_cap, &dest8_len,
                        src8, "en", 'u');
    YAZ_CHECK(dest8_len);
    printf("icu_casemap 'en:u' '%s' (%d)\n", dest8, (int) dest8_len);


    dest8 = icu_casemap(nmem, buf, buf_cap, &dest8_len,
                        src8, "en", 'f');
    YAZ_CHECK(dest8_len);
    printf("icu_casemap 'en:f' '%s' (%d)\n", dest8, (int) dest8_len);


    // some calling error needs investigation
    dest8 = icu_casemap(nmem, buf, buf_cap, &dest8_len,
                        src8, "en", 't');
    YAZ_CHECK(0 == dest8_len);
    printf("icu_casemap 'en:t' '%s' (%d)\n", dest8, (int) dest8_len);


    // attention: does not fail even if no locale 'xy_zz' defined
    // it seems to default to english locale
    dest8 = icu_casemap(nmem, buf, buf_cap, &dest8_len,
                        src8, "zz_abc", 'l');
    YAZ_CHECK(dest8_len);
    printf("icu_casemap 'zz:l' '%s' (%d)\n", dest8, (int) dest8_len);


    // shall fail - no buf buffer defined
    dest8 = icu_casemap(nmem, 0, buf_cap, &dest8_len,
                        src8, "en", 'l');
    YAZ_CHECK(0 == dest8_len);
    //printf("icu_casemap 'en:l' '%s' (%d)\n", dest8, (int) dest8_len);

    // shall fail - no buf_cap  defined
    dest8 = icu_casemap(nmem, buf, 0, &dest8_len,
                        src8, "en", 'l');
    YAZ_CHECK(0 == dest8_len);
    //printf("icu_casemap 'en:l' '%s' (%d)\n", dest8, (int) dest8_len);

    // shall fail - no action 'x' defined
    dest8 = icu_casemap(nmem, buf, buf_cap, &dest8_len,
                        src8, "en", 'x');
    YAZ_CHECK(0 == dest8_len);
    //printf("icu_casemap 'en:x' '%s' (%d)\n", dest8, (int) dest8_len);





    nmem_destroy(nmem);

    YAZ_CHECK(0 == 0);
    //YAZ_CHECK_EQ(0, 1);
}

void test_icu_I18N_casemap_da(int argc, char **argv)
{

    size_t buf_cap = 128;
    char buf[buf_cap];
    size_t dest8_len = 0;
    NMEM nmem = nmem_create();
    char * dest8 = 0;

    const char * src8 =  "åh ÆbLE, øs fLØde i Åen efter bLåBærGRødeN";
    size_t src8_len = strlen(src8);
    
    printf("original string:   '%s' (%d)\n", src8, (int) src8_len);

    //these shall succeed
    dest8 = icu_casemap(nmem, buf, buf_cap, &dest8_len,
                        src8, "da", 'l');
    YAZ_CHECK(dest8_len);
    printf("icu_casemap 'da:l' '%s' (%d)\n", dest8, (int) dest8_len);


    dest8 = icu_casemap(nmem, buf, buf_cap, &dest8_len,
                        src8, "da", 'u');
    YAZ_CHECK(dest8_len);
    printf("icu_casemap 'da:u' '%s' (%d)\n", dest8, (int) dest8_len);


    dest8 = icu_casemap(nmem, buf, buf_cap, &dest8_len,
                        src8, "da", 'f');
    YAZ_CHECK(dest8_len);
    printf("icu_casemap 'da:f' '%s' (%d)\n", dest8, (int) dest8_len);


    dest8 = icu_casemap(nmem, buf, buf_cap, &dest8_len,
                        src8, "da", 't');
    YAZ_CHECK(dest8_len);
    printf("icu_casemap 'da:t' '%s' (%d)\n", dest8, (int) dest8_len);

    nmem_destroy(nmem);

    YAZ_CHECK(0 == 0);
    //YAZ_CHECK_EQ(0, 1);
}

void test_icu_I18N_casemap_de(int argc, char **argv)
{

    size_t buf_cap = 128;
    char buf[buf_cap];
    size_t dest8_len = 0;
    NMEM nmem = nmem_create();
    char * dest8 = 0;

    const char * src8 = "zWÖlf ärgerliche Würste rollen ÜBer die StRAße";
    size_t src8_len = strlen(src8);
    
    printf("original string:   '%s' (%d)\n", src8, (int) src8_len);

    //these shall succeed
    dest8 = icu_casemap(nmem, buf, buf_cap, &dest8_len,
                        src8, "da", 'l');
    YAZ_CHECK(dest8_len);
    printf("icu_casemap 'da:l' '%s' (%d)\n", dest8, (int) dest8_len);


    dest8 = icu_casemap(nmem, buf, buf_cap, &dest8_len,
                        src8, "da", 'u');
    YAZ_CHECK(dest8_len);
    printf("icu_casemap 'da:u' '%s' (%d)\n", dest8, (int) dest8_len);


    dest8 = icu_casemap(nmem, buf, buf_cap, &dest8_len,
                        src8, "da", 'f');
    YAZ_CHECK(dest8_len);
    printf("icu_casemap 'da:f' '%s' (%d)\n", dest8, (int) dest8_len);


    dest8 = icu_casemap(nmem, buf, buf_cap, &dest8_len,
                        src8, "da", 't');
    YAZ_CHECK(dest8_len);
    printf("icu_casemap 'da:t' '%s' (%d)\n", dest8, (int) dest8_len);

    nmem_destroy(nmem);

    YAZ_CHECK(0 == 0);
    //YAZ_CHECK_EQ(0, 1);
}

void test_icu_I18N_casemap_el(int argc, char **argv)
{


#if 0

    size_t buf_cap = 128;
    char buf[buf_cap];
    size_t dest8_len = 0;
    NMEM nmem = nmem_create();
    char * dest8 = 0;

    const char * src8 = ""
    size_t src8_len = strlen(src8);
    
    printf("original string:   '%s' (%d)\n", src8, (int) src8_len);

    //these shall succeed
    dest8 = icu_casemap(nmem, buf, buf_cap, &dest8_len,
                        src8, "el", 'l');
    YAZ_CHECK(dest8_len);
    printf("icu_casemap 'el:l' '%s' (%d)\n", dest8, (int) dest8_len);


    dest8 = icu_casemap(nmem, buf, buf_cap, &dest8_len,
                        src8, "el", 'u');
    YAZ_CHECK(dest8_len);
    printf("icu_casemap 'el:u' '%s' (%d)\n", dest8, (int) dest8_len);


    dest8 = icu_casemap(nmem, buf, buf_cap, &dest8_len,
                        src8, "el", 'f');
    YAZ_CHECK(dest8_len);
    printf("icu_casemap 'el:f' '%s' (%d)\n", dest8, (int) dest8_len);


    dest8 = icu_casemap(nmem, buf, buf_cap, &dest8_len,
                        src8, "el", 't');
    YAZ_CHECK(dest8_len);
    printf("icu_casemap 'el:t' '%s' (%d)\n", dest8, (int) dest8_len);

    nmem_destroy(nmem);

    YAZ_CHECK(0 == 0);
    //YAZ_CHECK_EQ(0, 1);
#endif 

}


#endif    

int main(int argc, char **argv)
{

    YAZ_CHECK_INIT(argc, argv); 
    YAZ_CHECK_LOG();

#ifdef HAVE_ICU

    test_icu_I18N_casemap_en(argc, argv);
    test_icu_I18N_casemap_da(argc, argv); 
    test_icu_I18N_casemap_de(argc, argv); 
    test_icu_I18N_casemap_el(argc, argv); 
 
#else

    printf("ICU unit tests omitted.\n"
           "Please install libicu36-dev and icu-doc or similar\n".);
    YAZ_CHECK(0 == 0);

#endif    
   
    YAZ_CHECK_TERM;
}




/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
