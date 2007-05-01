/* $Id: icu_I18N.c,v 1.2 2007-05-01 08:17:05 marc Exp $
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

#include <unicode/ustring.h>  /* some more string fcns*/
#include <unicode/uchar.h>    /* char names           */


//#include <unicode/ustdio.h>
//#include <unicode/utypes.h>   /* Basic ICU data types */
//#include <unicode/ucol.h> 
//#include <unicode/ucnv.h>     /* C   Converter API    */
//#include <unicode/uloc.h>
//#include <unicode/ubrk.h>
/* #include <unicode/unistr.h> */


// forward declarations for helper functions

int icu_check_status (UErrorCode status);

UChar* icu_utf16_from_utf8(UChar *utf16,
                           int32_t utf16_cap,
                           int32_t *utf16_len,
                           const char *utf8);

UChar* icu_utf16_from_utf8n(UChar *utf16,
                            int32_t utf16_cap,
                            int32_t *utf16_len,
                            const char *utf8, 
                            size_t utf8_len);


char* icu_utf16_to_utf8(char *utf8,           
                        size_t utf8_cap,
                        size_t *utf8_len,
                        const UChar *utf16, 
                        int32_t utf16_len);


int32_t icu_utf16_casemap(UChar *dest16, int32_t dest16_cap,
                          const UChar *src16, int32_t src16_len,
                          const char *locale, char action);


// source code

int icu_check_status (UErrorCode status)
{
  if(U_FAILURE(status))
      yaz_log(YLOG_WARN, 
              "ICU Error: %d %s\n", status, u_errorName(status));
  return status;
}


UChar* icu_utf16_from_utf8(UChar *utf16,             
                           int32_t utf16_cap,
                           int32_t *utf16_len,
                           const char *utf8)
{
    size_t utf8_len = strlen(utf8);
    return icu_utf16_from_utf8n(utf16, utf16_cap, utf16_len,
                                  utf8, utf8_len);    
}


UChar* icu_utf16_from_utf8n(UChar *utf16,             
                            int32_t utf16_cap,
                            int32_t *utf16_len,
                            const char *utf8, 
                            size_t utf8_len)
{
    UErrorCode status = U_ZERO_ERROR;
    u_strFromUTF8(utf16, utf16_cap, utf16_len, utf8, (int32_t) utf8_len,
                  &status);
    if (U_ZERO_ERROR != icu_check_status(status))
        return 0;
    else
        return utf16;
}


char* icu_utf16_to_utf8(char *utf8,           
                        size_t utf8_cap,
                        size_t *utf8_len,
                        const UChar *utf16, 
                        int32_t utf16_len)
{
    UErrorCode status = U_ZERO_ERROR;
    u_strToUTF8(utf8, (int32_t) utf8_cap, (int32_t *)utf8_len, 
                utf16, utf16_len, &status);
    if (U_ZERO_ERROR != icu_check_status(status))
        return 0;
    else
        return utf8;
}


int32_t icu_utf16_casemap(UChar *dest16, int32_t dest16_cap,
                          const UChar *src16, int32_t src16_len,
                          const char *locale, char action)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t dest16_len = 0;
    
    switch(action) {    
    case 'l':    
        dest16_len = u_strToLower(dest16, dest16_cap, src16, src16_len, 
                                  locale, &status);
        break;
    case 'u':    
        dest16_len = u_strToUpper(dest16, dest16_cap, src16, src16_len, 
                                  locale, &status);
        break;
    case 't':    
        dest16_len = u_strToTitle(dest16, dest16_cap, src16, src16_len,
                                  0, locale, &status);
        break;
    case 'f':    
        dest16_len = u_strFoldCase(dest16, dest16_cap, src16, src16_len,
                                   U_FOLD_CASE_DEFAULT, &status);
        break;
        
    default:
        return 0;
        break;
    }

    if (U_ZERO_ERROR != icu_check_status(status))
        return 0;
    else
        return dest16_len;
}


char * icu_casemap(NMEM nmem, char *buf, size_t buf_cap, 
                   size_t *dest8_len,  const char *src8,
                   const char *locale, char action)
{
    size_t src8_len = strlen(src8);
    int32_t buf_len = 0;
    char * dest8 = 0;
    
    if (dest8_len)
        *dest8_len = 0;

    if (!buf || !(buf_cap > 0) || !src8_len)
        return 0;

    // converting buf to utf16
    buf = (char *)icu_utf16_from_utf8n((UChar *) buf, 
                                       (int32_t) buf_cap, &buf_len,
                                       src8, src8_len);
    
    // case mapping
    buf_len = (size_t) icu_utf16_casemap((UChar *)buf, (int32_t) buf_cap,
                                         (const UChar *)buf, (int32_t) buf_len,
                                         locale, action);

    // converting buf to utf8
    buf = icu_utf16_to_utf8(buf, buf_cap, (size_t *) &buf_len,
                            (const UChar *) buf, (int32_t) buf_len);

    
    // copying out to nmem
    buf[buf_len] = '\0';

    if(dest8_len)
        *dest8_len = buf_len;

    dest8 =  nmem_strdup(nmem, buf);
    return dest8;
}




#endif // HAVE_ICU    




/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
