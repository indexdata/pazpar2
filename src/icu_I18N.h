/* $Id: icu_I18N.h,v 1.1 2007-04-30 13:56:52 marc Exp $
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

#ifndef ICU_I18NL_H
#define ICU_I18NL_H

#ifdef HAVE_ICU

#include <yaz/nmem.h>


//#include <unicode/utypes.h>   /* Basic ICU data types */
#include <unicode/uchar.h>    /* char names           */

//#include <unicode/ustdio.h>
//#include <unicode/ucol.h> 
//#include <unicode/ucnv.h>     /* C   Converter API    */
//#include <unicode/ustring.h>  /* some more string fcns*/
//#include <unicode/uloc.h>
//#include <unicode/ubrk.h>
//#include <unicode/unistr.h>


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

char * icu_casemap(NMEM nmem, char *buf, size_t buf_cap, 
                   size_t *dest8_len,  const char *src8,
                   const char *locale, char action);


#endif // HAVE_ICU
#endif // ICU_I18NL_H
