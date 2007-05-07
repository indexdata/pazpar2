/* $Id: icu_I18N.c,v 1.7 2007-05-07 12:52:04 marc Exp $
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


#ifdef HAVE_ICU
#include "icu_I18N.h"

#include <yaz/log.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <unicode/ustring.h>  /* some more string fcns*/
#include <unicode/uchar.h>    /* char names           */


//#include <unicode/ustdio.h>
//#include <unicode/utypes.h>   /* Basic ICU data types */
#include <unicode/ucol.h> 
//#include <unicode/ucnv.h>     /* C   Converter API    */
//#include <unicode/uloc.h>
//#include <unicode/ubrk.h>
/* #include <unicode/unistr.h> */




int icu_check_status (UErrorCode status)
{
    //if(U_FAILURE(status))
    if(!U_SUCCESS(status))
        yaz_log(YLOG_WARN, 
                "ICU: %d %s\n", status, u_errorName(status));
    return status;
}



struct icu_buf_utf16 * icu_buf_utf16_create(size_t capacity)
{
    struct icu_buf_utf16 * buf16 
        = (struct icu_buf_utf16 *) malloc(sizeof(struct icu_buf_utf16));

    buf16->utf16 = 0;
    buf16->utf16_len = 0;
    buf16->utf16_cap = 0;

    if (capacity > 0){
        buf16->utf16 = (UChar *) malloc(sizeof(UChar) * capacity);
        buf16->utf16[0] = (UChar) 0;
        buf16->utf16_cap = capacity;
    }
    return buf16;
};


struct icu_buf_utf16 * icu_buf_utf16_resize(struct icu_buf_utf16 * buf16,
                                            size_t capacity)
{
    if (buf16){
        if (capacity >  0){
            if (0 == buf16->utf16)
                buf16->utf16 = (UChar *) malloc(sizeof(UChar) * capacity);
            else
                buf16->utf16 
                    = (UChar *) realloc(buf16->utf16, sizeof(UChar) * capacity);
            buf16->utf16[0] = (UChar) 0;
            buf16->utf16_len = 0;
            buf16->utf16_cap = capacity;
        } 
        else { 
            if (buf16->utf16)
                free(buf16->utf16);
            buf16->utf16 = 0;
            buf16->utf16_len = 0;
            buf16->utf16_cap = 0;
        }
    }

    return buf16;
};


void icu_buf_utf16_destroy(struct icu_buf_utf16 * buf16)
{
    if (buf16){
        if (buf16->utf16)
            free(buf16->utf16);
        free(buf16);
    }
};






struct icu_buf_utf8 * icu_buf_utf8_create(size_t capacity)
{
    struct icu_buf_utf8 * buf8 
        = (struct icu_buf_utf8 *) malloc(sizeof(struct icu_buf_utf8));

    buf8->utf8 = 0;
    buf8->utf8_len = 0;
    buf8->utf8_cap = 0;

    if (capacity > 0){
        buf8->utf8 = (uint8_t *) malloc(sizeof(uint8_t) * capacity);
        buf8->utf8[0] = (uint8_t) 0;
        buf8->utf8_cap = capacity;
    }
    return buf8;
};



struct icu_buf_utf8 * icu_buf_utf8_resize(struct icu_buf_utf8 * buf8,
                                          size_t capacity)
{
    if (buf8){
        if (capacity >  0){
            if (0 == buf8->utf8)
                buf8->utf8 = (uint8_t *) malloc(sizeof(uint8_t) * capacity);
            else
                buf8->utf8 
                    = (uint8_t *) realloc(buf8->utf8, sizeof(uint8_t) * capacity);
            buf8->utf8[0] = (uint8_t) 0;
            buf8->utf8_len = 0;
            buf8->utf8_cap = capacity;
        } 
        else { 
            if (buf8->utf8)
                free(buf8->utf8);
            buf8->utf8 = 0;
            buf8->utf8_len = 0;
            buf8->utf8_cap = 0;
        }
    }

    return buf8;
};



void icu_buf_utf8_destroy(struct icu_buf_utf8 * buf8)
{
    if (buf8){
        if (buf8->utf8)
            free(buf8->utf8);
        free(buf8);
    }
};



UErrorCode icu_utf16_from_utf8(struct icu_buf_utf16 * dest16,
                               struct icu_buf_utf8 * src8,
                               UErrorCode * status)
{
    int32_t utf16_len = 0;
  
    u_strFromUTF8(dest16->utf16, dest16->utf16_cap,
                  &utf16_len,
                  (const char *) src8->utf8, src8->utf8_len, status);
  
    // check for buffer overflow, resize and retry
    if (*status == U_BUFFER_OVERFLOW_ERROR
        //|| dest16->utf16_len > dest16->utf16_cap
        ){
        icu_buf_utf16_resize(dest16, utf16_len * 2);
        *status = U_ZERO_ERROR;
        u_strFromUTF8(dest16->utf16, dest16->utf16_cap,
                      &utf16_len,
                      (const char *) src8->utf8, src8->utf8_len, status);
    }

    //if (*status != U_BUFFER_OVERFLOW_ERROR
    if (U_SUCCESS(*status)  
        && utf16_len < dest16->utf16_cap)
        dest16->utf16_len = utf16_len;
    else {
        dest16->utf16[0] = (UChar) 0;
        dest16->utf16_len = 0;
    }
  
    return *status;
};

 

UErrorCode icu_utf16_from_utf8_cstr(struct icu_buf_utf16 * dest16,
                                    const char * src8cstr,
                                    UErrorCode * status)
{
    size_t src8cstr_len = 0;
    int32_t utf16_len = 0;

    src8cstr_len = strlen(src8cstr);
  
    u_strFromUTF8(dest16->utf16, dest16->utf16_cap,
                  &utf16_len,
                  src8cstr, src8cstr_len, status);
  
    // check for buffer overflow, resize and retry
    if (*status == U_BUFFER_OVERFLOW_ERROR
        //|| dest16->utf16_len > dest16->utf16_cap
        ){
        icu_buf_utf16_resize(dest16, utf16_len * 2);
        *status = U_ZERO_ERROR;
        u_strFromUTF8(dest16->utf16, dest16->utf16_cap,
                      &utf16_len,
                      src8cstr, src8cstr_len, status);
    }

    //  if (*status != U_BUFFER_OVERFLOW_ERROR
    if (U_SUCCESS(*status)  
        && utf16_len < dest16->utf16_cap)
        dest16->utf16_len = utf16_len;
    else {
        dest16->utf16[0] = (UChar) 0;
        dest16->utf16_len = 0;
    }
  
    return *status;
};




UErrorCode icu_utf16_to_utf8(struct icu_buf_utf8 * dest8,
                             struct icu_buf_utf16 * src16,
                             UErrorCode * status)
{
    int32_t utf8_len = 0;
  
    u_strToUTF8((char *) dest8->utf8, dest8->utf8_cap,
                &utf8_len,
                src16->utf16, src16->utf16_len, status);
  
    // check for buffer overflow, resize and retry
    if (*status == U_BUFFER_OVERFLOW_ERROR
        //|| dest8->utf8_len > dest8->utf8_cap
        ){
        icu_buf_utf8_resize(dest8, utf8_len * 2);
        *status = U_ZERO_ERROR;
        u_strToUTF8((char *) dest8->utf8, dest8->utf8_cap,
                    &utf8_len,
                    src16->utf16, src16->utf16_len, status);

    }

    //if (*status != U_BUFFER_OVERFLOW_ERROR
    if (U_SUCCESS(*status)  
        && utf8_len < dest8->utf8_cap)
        dest8->utf8_len = utf8_len;
    else {
        dest8->utf8[0] = (uint8_t) 0;
        dest8->utf8_len = 0;
    }
  
    return *status;
};



int icu_utf16_casemap(struct icu_buf_utf16 * dest16,
                      struct icu_buf_utf16 * src16,
                      const char *locale, char action,
                      UErrorCode *status)
{
    int32_t dest16_len = 0;
    
    switch(action) {    
    case 'l':    
        dest16_len = u_strToLower(dest16->utf16, dest16->utf16_cap,
                                  src16->utf16, src16->utf16_len, 
                                  locale, status);
        break;
    case 'u':    
        dest16_len = u_strToUpper(dest16->utf16, dest16->utf16_cap,
                                  src16->utf16, src16->utf16_len, 
                                  locale, status);
        break;
    case 't':    
        dest16_len = u_strToTitle(dest16->utf16, dest16->utf16_cap,
                                  src16->utf16, src16->utf16_len,
                                  0, locale, status);
        break;
    case 'f':    
        dest16_len = u_strFoldCase(dest16->utf16, dest16->utf16_cap,
                                   src16->utf16, src16->utf16_len,
                                   U_FOLD_CASE_DEFAULT, status);
        break;
        
    default:
        return U_UNSUPPORTED_ERROR;
        break;
    }

    // check for buffer overflow, resize and retry
    if (*status == U_BUFFER_OVERFLOW_ERROR
        //|| dest16_len > dest16->utf16_cap
        ){
        icu_buf_utf16_resize(dest16, dest16_len * 2);
        *status = U_ZERO_ERROR;

    
        switch(action) {    
        case 'l':    
            dest16_len = u_strToLower(dest16->utf16, dest16->utf16_cap,
                                      src16->utf16, src16->utf16_len, 
                                      locale, status);
            break;
        case 'u':    
            dest16_len = u_strToUpper(dest16->utf16, dest16->utf16_cap,
                                      src16->utf16, src16->utf16_len, 
                                      locale, status);
            break;
        case 't':    
            dest16_len = u_strToTitle(dest16->utf16, dest16->utf16_cap,
                                      src16->utf16, src16->utf16_len,
                                      0, locale, status);
            break;
        case 'f':    
            dest16_len = u_strFoldCase(dest16->utf16, dest16->utf16_cap,
                                       src16->utf16, src16->utf16_len,
                                       U_FOLD_CASE_DEFAULT, status);
            break;
        
        default:
            return U_UNSUPPORTED_ERROR;
            break;
        }
    }
    
    if (U_SUCCESS(*status)
        && dest16_len < dest16->utf16_cap)
        dest16->utf16_len = dest16_len;
    else {
        dest16->utf16[0] = (UChar) 0;
        dest16->utf16_len = 0;
    }
  
    return *status;
};



UErrorCode icu_sortkey8_from_utf16(UCollator *coll,
                                   struct icu_buf_utf8 * dest8, 
                                   struct icu_buf_utf16 * src16,
                                   UErrorCode * status)
{ 
  
    int32_t sortkey_len = 0;

    sortkey_len = ucol_getSortKey(coll, src16->utf16, src16->utf16_len,
                                  dest8->utf8, dest8->utf8_cap);

    // check for buffer overflow, resize and retry
    if (sortkey_len > dest8->utf8_cap) {
        icu_buf_utf8_resize(dest8, sortkey_len * 2);
        sortkey_len = ucol_getSortKey(coll, src16->utf16, src16->utf16_len,
                                      dest8->utf8, dest8->utf8_cap);
    }

    if (U_SUCCESS(*status)
        && sortkey_len > 0)
        dest8->utf8_len = sortkey_len;
    else {
        dest8->utf8[0] = (UChar) 0;
        dest8->utf8_len = 0;
    }

    return *status;
};




#endif // HAVE_ICU    




/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
