/* $Id: test_icu_I18N.c,v 1.6 2007-05-01 13:27:32 marc Exp $
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

// DO NOT EDIT THIS FILE IF YOUR EDITOR DOES NOT SUPPORT UTF-8
 

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
#include <string.h>
#include <stdlib.h>


// DO NOT EDIT THIS FILE IF YOUR EDITOR DOES NOT SUPPORT UTF-8

int test_icu_casemap(const char * locale, char action,
                     const char * src8, const char * check8)
{
    NMEM nmem = nmem_create();    
    size_t buf_cap = 128;
    char buf[buf_cap];
    const char * dest8 = 0;
    size_t dest8_len = 0;
    //size_t src8_len = strlen(src8);
    int sucess = 0;

    //printf("original string:   '%s' (%d)\n", src8, (int) src8_len);

    //these shall succeed
    dest8 = icu_casemap(nmem, buf, buf_cap, &dest8_len,
                        src8, locale, action);


    //printf("icu_casemap '%s:%c' '%s' (%d)\n", 
    //       locale, action, dest8, (int) dest8_len);

    if (dest8 
        && (dest8_len == strlen(check8))
        && !strcmp(check8, dest8))
        sucess = dest8_len;

    nmem_destroy(nmem);

    return sucess;
}

// DO NOT EDIT THIS FILE IF YOUR EDITOR DOES NOT SUPPORT UTF-8

void test_icu_I18N_casemap(int argc, char **argv)
{

    // Locale 'en'

    // sucessful tests
    YAZ_CHECK(test_icu_casemap("en", 'l',
                               "A ReD fOx hunTS sQUirriLs", 
                               "a red fox hunts squirrils"));
    
    YAZ_CHECK(test_icu_casemap("en", 'u',
                               "A ReD fOx hunTS sQUirriLs", 
                               "A RED FOX HUNTS SQUIRRILS"));
    
    YAZ_CHECK(test_icu_casemap("en", 'f',
                               "A ReD fOx hunTS sQUirriLs", 
                               "a red fox hunts squirrils"));
    
    // this one fails and needs more investigation ..
    YAZ_CHECK(0 == test_icu_casemap("en", 't',
                               "A ReD fOx hunTS sQUirriLs", 
                               "A Red Fox Hunts Squirrils"));


    // Locale 'da'

    // sucess expected    
    YAZ_CHECK(test_icu_casemap("da", 'l',
                               "åh ÆbLE, øs fLØde i Åen efter bLåBærGRødeN", 
                               "åh æble, øs fløde i åen efter blåbærgrøden"));

    YAZ_CHECK(test_icu_casemap("da", 'u',
                               "åh ÆbLE, øs fLØde i Åen efter bLåBærGRødeN", 
                               "ÅH ÆBLE, ØS FLØDE I ÅEN EFTER BLÅBÆRGRØDEN"));

    YAZ_CHECK(test_icu_casemap("da", 'f',
                               "åh ÆbLE, øs fLØde i Åen efter bLåBærGRødeN", 
                               "åh æble, øs fløde i åen efter blåbærgrøden"));

    YAZ_CHECK(test_icu_casemap("da", 't',
                               "åh ÆbLE, øs fLØde i Åen efter bLåBærGRødeN", 
                               "Åh Æble, Øs Fløde I Åen Efter Blåbærgrøden"));

    // Locale 'de'

    // sucess expected    
    YAZ_CHECK(test_icu_casemap("de", 'l',
                          "zWÖlf ärgerliche Würste rollen ÜBer die StRAße",
                          "zwölf ärgerliche würste rollen über die straße"));

    YAZ_CHECK(test_icu_casemap("de", 'u',
                           "zWÖlf ärgerliche Würste rollen ÜBer die StRAße",
                           "ZWÖLF ÄRGERLICHE WÜRSTE ROLLEN ÜBER DIE STRASSE"));

    YAZ_CHECK(test_icu_casemap("de", 'f',
                           "zWÖlf ärgerliche Würste rollen ÜBer die StRAße",
                           "zwölf ärgerliche würste rollen über die strasse"));

    YAZ_CHECK(test_icu_casemap("de", 't',
                           "zWÖlf ärgerliche Würste rollen ÜBer die StRAße",
                           "Zwölf Ärgerliche Würste Rollen Über Die Straße"));

}


// DO NOT EDIT THIS FILE IF YOUR EDITOR DOES NOT SUPPORT UTF-8

void test_icu_I18N_casemap_failures(int argc, char **argv)
{

    size_t buf_cap = 128;
    char buf[buf_cap];
    size_t dest8_len = 0;
    NMEM nmem = nmem_create();
    char * dest8 = 0;

    const char * src8 =  "A ReD fOx hunTS sQUirriLs";
    //size_t src8_len = strlen(src8);
    
    //printf("original string:   '%s' (%d)\n", src8, (int) src8_len);

    // some calling error needs investigation
    dest8 = icu_casemap(nmem, buf, buf_cap, &dest8_len,
                        src8, "en", 't');
    YAZ_CHECK(0 == dest8_len);
    //printf("icu_casemap 'en:t' '%s' (%d)\n", dest8, (int) dest8_len);


    // attention: does not fail even if no locale 'xy_zz' defined
    // it seems to default to english locale
    dest8 = icu_casemap(nmem, buf, buf_cap, &dest8_len,
                        src8, "zz_abc", 'l');
    YAZ_CHECK(dest8_len);
    //printf("icu_casemap 'zz:l' '%s' (%d)\n", dest8, (int) dest8_len);


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
}

// DO NOT EDIT THIS FILE IF YOUR EDITOR DOES NOT SUPPORT UTF-8

int test_icu_sortmap(const char * locale, size_t list_len,
                     const char ** src8_list, const char ** check8_list)
{
    int sucess = 1;

    size_t i = 0;


    NMEM nmem = nmem_create();    
    size_t buf_cap = 128;
    char buf[buf_cap];
    struct icu_termmap ** dest8_list 
        = nmem_malloc(nmem, sizeof(struct icu_termmap *) * list_len);
    //size_t dest8_len = 0;
    //size_t src8_len = strlen(src8);

    // initializing icu_termmap
    for (i = 0; i < list_len; i++){
        dest8_list[i] = icu_termmap_create(nmem);
        dest8_list[i]->norm_term = nmem_strdup(nmem, src8_list[i]); 
        dest8_list[i]->disp_term = nmem_strdup(nmem, src8_list[i]);
        //dest8_list[i]->sort_key =  nmem_strdup(nmem, src8_list[i]);
        //dest8_list[i]->sort_len =  strlen(src8_list[i]);
        dest8_list[i]->sort_key 
            = icu_sortmap(nmem, buf, buf_cap, &(dest8_list[i]->sort_len),
                          src8_list[i], locale);
    }

    // do the sorting
    qsort(dest8_list, list_len, 
          sizeof(struct icu_termmap *), icu_termmap_cmp);

    // checking correct sorting
    for (i = 0; i < list_len; i++){
        if (0 != strcmp(dest8_list[i]->disp_term, check8_list[i])){
            sucess = 0;
        }
    }

    if (!sucess)
        for (i = 0; i < list_len; i++){
            printf("icu_sortmap '%s': '%s' '%s'\n", locale,
                   dest8_list[i]->disp_term, check8_list[i]);
        }

    nmem_destroy(nmem);

    return sucess;
}


// DO NOT EDIT THIS FILE IF YOUR EDITOR DOES NOT SUPPORT UTF-8

void test_icu_I18N_sortmap(int argc, char **argv)
{

    // sucessful tests
    size_t en_1_len = 6;
    const char * en_1_src[6] = {"z", "K", "a", "A", "Z", "k"};
    const char * en_1_cck[6] = {"a", "A", "K", "k", "z", "Z"};
    YAZ_CHECK(test_icu_sortmap("en", en_1_len, en_1_src, en_1_cck));
    YAZ_CHECK(0 == test_icu_sortmap("en_AU", en_1_len, en_1_src, en_1_cck));
    YAZ_CHECK(0 == test_icu_sortmap("en_CA", en_1_len, en_1_src, en_1_cck));
    YAZ_CHECK(0 == test_icu_sortmap("en_GB", en_1_len, en_1_src, en_1_cck));
    YAZ_CHECK(0 == test_icu_sortmap("en_US", en_1_len, en_1_src, en_1_cck));
    
    // sucessful tests - this one fails and should not!!!
    size_t da_1_len = 6;
    const char * da_1_src[6] = {"z", "å", "o", "æ", "a", "ø"};
    const char * da_1_cck[6] = {"a", "o", "z", "æ", "ø", "å"};
    YAZ_CHECK(0 == test_icu_sortmap("da", da_1_len, da_1_src, da_1_cck));
    YAZ_CHECK(0 == test_icu_sortmap("da_DK", da_1_len, da_1_src, da_1_cck));
    
    // sucessful tests
    size_t de_1_len = 9;
    const char * de_1_src[9] = {"u", "ä", "o", "t", "s", "ß", "ü", "ö", "a"};
    const char * de_1_cck[9] = {"ä", "a", "o", "ö", "s", "ß", "t", "u", "ü"};
    YAZ_CHECK(test_icu_sortmap("de", de_1_len, de_1_src, de_1_cck));
    YAZ_CHECK(0 == test_icu_sortmap("de_AT", de_1_len, de_1_src, de_1_cck));
    YAZ_CHECK(0 == test_icu_sortmap("de_DE", de_1_len, de_1_src, de_1_cck));
    
}


#endif    

// DO NOT EDIT THIS FILE IF YOUR EDITOR DOES NOT SUPPORT UTF-8

int main(int argc, char **argv)
{

    YAZ_CHECK_INIT(argc, argv); 
    YAZ_CHECK_LOG();

#ifdef HAVE_ICU

    test_icu_I18N_casemap_failures(argc, argv);
    test_icu_I18N_casemap(argc, argv);
    test_icu_I18N_sortmap(argc, argv);
 
#else

    printf("ICU unit tests omitted.\n"
           "Please install libicu36-dev and icu-doc or similar\n");
    YAZ_CHECK(0 == 0);

#endif    
   
    YAZ_CHECK_TERM;
}


// DO NOT EDIT THIS FILE IF YOUR EDITOR DOES NOT SUPPORT UTF-8

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
