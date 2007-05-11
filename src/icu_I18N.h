/* $Id: icu_I18N.h,v 1.10 2007-05-11 09:35:50 marc Exp $
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


#include <unicode/utypes.h>   /* Basic ICU data types */
#include <unicode/uchar.h>    /* char names           */

//#include <unicode/ustdio.h>
#include <unicode/ucol.h> 
//#include <unicode/ucnv.h>     /* C   Converter API    */
//#include <unicode/ustring.h>  /* some more string fcns*/
//#include <unicode/uloc.h>
#include <unicode/ubrk.h>
//#include <unicode/unistr.h>
#include <unicode/utrans.h>



// forward declarations
//struct UBreakIterator;




// declared structs and functions


int icu_check_status (UErrorCode status);

struct icu_buf_utf16
{
  UChar * utf16;
  int32_t utf16_len;
  int32_t utf16_cap;
};

struct icu_buf_utf16 * icu_buf_utf16_create(size_t capacity);
struct icu_buf_utf16 * icu_buf_utf16_resize(struct icu_buf_utf16 * buf16,
                                            size_t capacity);
void icu_buf_utf16_destroy(struct icu_buf_utf16 * buf16);



struct icu_buf_utf8
{
  uint8_t * utf8;
  int32_t utf8_len;
  int32_t utf8_cap;
};

struct icu_buf_utf8 * icu_buf_utf8_create(size_t capacity);
struct icu_buf_utf8 * icu_buf_utf8_resize(struct icu_buf_utf8 * buf8,
                                          size_t capacity);
void icu_buf_utf8_destroy(struct icu_buf_utf8 * buf8);


UErrorCode icu_utf16_from_utf8(struct icu_buf_utf16 * dest16,
                               struct icu_buf_utf8 * src8,
                               UErrorCode * status);

UErrorCode icu_utf16_from_utf8_cstr(struct icu_buf_utf16 * dest16,
                                    const char * src8cstr,
                                    UErrorCode * status);


UErrorCode icu_utf16_to_utf8(struct icu_buf_utf8 * dest8,
                             struct icu_buf_utf16 * src16,
                             UErrorCode * status);

int icu_utf16_casemap(struct icu_buf_utf16 * dest16,
                      struct icu_buf_utf16 * src16,
                      const char *locale, char action,
                      UErrorCode *status);

UErrorCode icu_sortkey8_from_utf16(UCollator *coll,
                                   struct icu_buf_utf8 * dest8, 
                                   struct icu_buf_utf16 * src16,
                                   UErrorCode * status);

struct icu_tokenizer
{
  char locale[16];
  char action;
  UBreakIterator* bi;
  struct icu_buf_utf16 * buf16;
  int32_t token_count;
  int32_t token_id;
  int32_t token_start;
  int32_t token_end;
  // keep always invariant
  // 0 <= token_start 
  //   <= token_end 
  //   <= buf16->utf16_len
  // and invariant
  // 0 <= token_id <= token_count
};

struct icu_tokenizer * icu_tokenizer_create(const char *locale, char action,
                                            UErrorCode *status);

void icu_tokenizer_destroy(struct icu_tokenizer * tokenizer);

int icu_tokenizer_attach(struct icu_tokenizer * tokenizer, 
                         struct icu_buf_utf16 * src16, UErrorCode *status);

int32_t icu_tokenizer_next_token(struct icu_tokenizer * tokenizer, 
                                 struct icu_buf_utf16 * tkn16, 
                                 UErrorCode *status);

int32_t icu_tokenizer_token_id(struct icu_tokenizer * tokenizer);
int32_t icu_tokenizer_token_start(struct icu_tokenizer * tokenizer);
int32_t icu_tokenizer_token_end(struct icu_tokenizer * tokenizer);
int32_t icu_tokenizer_token_length(struct icu_tokenizer * tokenizer);
int32_t icu_tokenizer_token_count(struct icu_tokenizer * tokenizer);



struct icu_normalizer
{
  char action;
  struct icu_buf_utf16 * rules16;
  UParseError parse_error[256];
  UTransliterator * trans;
};

struct icu_normalizer * icu_normalizer_create(const char *rules, char action,
                                              UErrorCode *status);


void icu_normalizer_destroy(struct icu_normalizer * normalizer);


#endif // HAVE_ICU
#endif // ICU_I18NL_H
