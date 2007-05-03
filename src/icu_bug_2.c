// Make command on debian 64 bit testing dist  
/*
gcc -g -Wall `icu-config --cppflags`  `icu-config --ldflags` -o icu_bug_2 icu_bug_2.c
snatched from http://www.icu-project.org/userguide/Collate_API.html 
and corrected for compile errors
added a struct icu_termmap such that I actually can see the output
*/

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
//#include <unicode/unistr.h> 


#define MAX_KEY_SIZE 256

struct icu_buf_utf16
{
  UChar * utf16;
  int32_t utf16_len;
  int32_t utf16_cap;
};


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
  printf("buf16 resize: %d\n", (int)capacity);
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



struct icu_buf_utf8
{
  uint8_t * utf8;
  int32_t utf8_len;
  int32_t utf8_cap;
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
  printf("buf8  resize: %d\n", (int)capacity);
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
  //if(!U_SUCCESS(*status))
  //  return *status;
  printf("icu_utf16_from_utf8 working\n");

  u_strFromUTF8(dest16->utf16, dest16->utf16_cap, &(dest16->utf16_len),
                (const char *) src8->utf8, src8->utf8_len, status);

  // check for buffer overflow, resize and retry
  if (dest16->utf16_len > dest16->utf16_cap){
    printf("icu_utf16_from_utf8 need resize\n");
    icu_buf_utf16_resize(dest16, dest16->utf16_len * 2);
    *status = U_ZERO_ERROR;
    u_strFromUTF8(dest16->utf16, dest16->utf16_cap, &(dest16->utf16_len),
                  (const char*) src8->utf8, src8->utf8_len, status);
  }

  return *status;
};

 

UErrorCode icu_utf16_from_utf8_cstr(struct icu_buf_utf16 * dest16,
                                    const char * src8cstr,
                                    UErrorCode * status)
{
  size_t src8cstr_len = 0;
  int32_t utf16_len = 0;

  //if(!U_SUCCESS(status))
  //  return *status;

  printf("icu_utf16_from_utf8_cstr working\n");  
  src8cstr_len = strlen(src8cstr);
  
  u_strFromUTF8(dest16->utf16, dest16->utf16_cap,
                &utf16_len,
                //&(dest16->utf16_len),
                src8cstr, src8cstr_len, status);
  
  // check for buffer overflow, resize and retry
  if (*status == U_BUFFER_OVERFLOW_ERROR
      //|| dest16->utf16_len > dest16->utf16_cap
      ){
    printf("icu_utf16_from_utf8_cstr need resize\n");
    icu_buf_utf16_resize(dest16, utf16_len * 2);
    *status = U_ZERO_ERROR;
    u_strFromUTF8(dest16->utf16, dest16->utf16_cap,
                  &utf16_len,
                  //&(dest16->utf16_len),
                  src8cstr, src8cstr_len, status);
  }

  if (*status != U_BUFFER_OVERFLOW_ERROR
      && utf16_len < dest16->utf16_cap)
    dest16->utf16_len = utf16_len;
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
  //if(!U_SUCCESS(status))
  //  return *status;

  printf("icu_sortkey8_from_utf16 working\n");  
  sortkey_len = ucol_getSortKey(coll, src16->utf16, src16->utf16_len,
                                dest8->utf8, dest8->utf8_cap);
  
  // check for buffer overflow, resize and retry
  if (sortkey_len > dest8->utf8_cap) {
    printf("icu_sortkey8_from_utf16 need resize\n");
    icu_buf_utf8_resize(dest8, sortkey_len * 2);
    sortkey_len = ucol_getSortKey(coll, src16->utf16, src16->utf16_len,
                                  dest8->utf8, dest8->utf8_cap);
  }

  return *status;
};

 


struct icu_termmap
{
  uint8_t sort_key[MAX_KEY_SIZE]; // standard C string '\0' terminated
  char disp_term[MAX_KEY_SIZE];  // standard C utf-8 string
};



int icu_termmap_cmp(const void *vp1, const void *vp2)
{
  struct icu_termmap *itmp1 = *(struct icu_termmap **) vp1;
  struct icu_termmap *itmp2 = *(struct icu_termmap **) vp2;

  int cmp = 0;
    
  cmp = strcmp((const char *)itmp1->sort_key, 
               (const char *)itmp2->sort_key);
  return cmp;
}


int icu_check_status(UErrorCode status)
{
  if(!U_SUCCESS(status))
    printf("ICU status: %d %s\n", status, u_errorName(status));
  return status;
}



int icu_coll_sort(const char * locale, int src_list_len,
                  const char ** src_list, const char ** chk_list)
{
  UErrorCode status = U_ZERO_ERROR;
  
  struct icu_buf_utf8 * buf8 = icu_buf_utf8_create(0);
  struct icu_buf_utf16 * buf16 = icu_buf_utf16_create(0);

  int i;

  struct icu_termmap * list[src_list_len];

  UCollator *coll = ucol_open(locale, &status); 
  icu_check_status(status);

  if(!U_SUCCESS(status))
    return 0;

  // assigning display terms and sort keys using buf 8 and buf16
  for( i = 0; i < src_list_len; i++) 
    {

      list[i] = (struct icu_termmap *) malloc(sizeof(struct icu_termmap));

      // copy display term
      strcpy(list[i]->disp_term, src_list[i]);    

      // transforming to UTF16
      icu_utf16_from_utf8_cstr(buf16, list[i]->disp_term, &status);
      icu_check_status(status);

      // computing sortkeys
      icu_sortkey8_from_utf16(coll, buf8, buf16, &status);
      icu_check_status(status);
    
      // assigning sortkeys
      memcpy(list[i]->sort_key, buf8->utf8, buf8->utf8_len);    
    } 

  printf("\n"); 
  printf("Input str: '%s' : ", locale); 
  for (i = 0; i < src_list_len; i++) {
    printf(" '%s'", list[i]->disp_term); 
  }
  printf("\n");

  // do the sorting
  qsort(list, src_list_len, 
        sizeof(struct icu_termmap *), icu_termmap_cmp);
  
  
  printf("ICU sort:  '%s' : ", locale); 
  for (i = 0; i < src_list_len; i++) {
    printf(" '%s'", list[i]->disp_term); 
  }
  printf("\n"); 
  
  ucol_close(coll);

  icu_buf_utf8_destroy(buf8);
  icu_buf_utf16_destroy(buf16);

  return 1;
};


int main(int argc, char **argv)
{
  
  size_t en_1_len = 6;
  const char * en_1_src[6] = {"z", "K", "a", "A", "Z", "k"};
  const char * en_1_cck[6] = {"a", "A", "K", "k", "z", "Z"};
  icu_coll_sort("en", en_1_len, en_1_src, en_1_cck);

#if 0
  icu_coll_sort("en_AU", en_1_len, en_1_src, en_1_cck);
  icu_coll_sort("en_CA", en_1_len, en_1_src, en_1_cck);
  icu_coll_sort("en_GB", en_1_len, en_1_src, en_1_cck);
  icu_coll_sort("en_US", en_1_len, en_1_src, en_1_cck);
    
    
  size_t da_1_len = 6;
  const char * da_1_src[6] = {"z", "å", "o", "æ", "a", "ø"};
  const char * da_1_cck[6] = {"a", "o", "z", "æ", "ø", "å"};
  icu_coll_sort("da", da_1_len, da_1_src, da_1_cck);
  icu_coll_sort("da_DK", da_1_len, da_1_src, da_1_cck);


  size_t de_1_len = 9;
  const char * de_1_src[9] = {"u", "ä", "o", "t", "s", "ß", "ü", "ö", "a"};
  const char * de_1_cck[9] = {"ä", "a", "o", "ö", "s", "ß", "t", "u", "ü"};
  icu_coll_sort("de", de_1_len, de_1_src, de_1_cck);
  icu_coll_sort("de_AT", de_1_len, de_1_src, de_1_cck);
  icu_coll_sort("de_DE", de_1_len, de_1_src, de_1_cck);
#endif
  return 0;
};

