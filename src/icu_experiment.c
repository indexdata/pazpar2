// Make command on debian 64 bit testing dist  
/*
gcc -g -Wall `icu-config --cppflags`  `icu-config --ldflags` -o icu_experiment icu_experiment.c
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
//#define MAX_BUFFER_SIZE 10000
//#define MAX_LIST_LENGTH 5
 



struct icu_termmap
{
  uint8_t sort_key[MAX_KEY_SIZE]; // standard C string '\0' terminated
  UChar utf16_term[MAX_KEY_SIZE]; // ICU utf-16 string
  int32_t utf16_len;               // ICU utf-16 string lenght
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
  
  int i;
  uint8_t *tmp = 0;
  int32_t tmp_len = 0;
  int32_t sort_key_len; 
  
  struct icu_termmap * list[src_list_len];
  

  UErrorCode status = U_ZERO_ERROR;
  
 
  for( i = 0; i < src_list_len; i++) 
    {
      int text_len = strlen(src_list[i]);

      list[i] = (struct icu_termmap *) malloc(sizeof(struct icu_termmap));

      strcpy(list[i]->disp_term, src_list[i]);    

      // transforming to UTF16
      u_strFromUTF8(list[i]->utf16_term, MAX_KEY_SIZE, 
                    &(list[i]->utf16_len), src_list[i], (int32_t) text_len,
                    &status);
      icu_check_status(status);

      //u_uastrcpy(list[i]->utf16_term, src_list[i]);   
   } 

  printf("\n"); 
  printf("Input str: '%s' : ", locale); 
  for (i = 0; i < src_list_len; i++) {
    printf(" '%s'", list[i]->disp_term); 
  }
  printf("\n");

  UCollator *coll = ucol_open(locale, &status); 
  icu_check_status(status);

  if(!U_SUCCESS(status))
    return 0;
  


  for(i=0; i < src_list_len; i++) {
    sort_key_len 
      = ucol_getSortKey(coll, list[i]->utf16_term, list[i]->utf16_len,
                        tmp, tmp_len);
    
    // reallocating business ..
    if (sort_key_len > tmp_len) {
      printf("sort_key_len: %d tmp_len: %d, reallocating tmp buf\n", 
             (int) sort_key_len, (int) tmp_len);
      if (tmp == 0) { 
        tmp = (uint8_t *) malloc(sort_key_len);
      } else
        tmp = (uint8_t *) realloc(tmp, sort_key_len);
      tmp_len = sort_key_len;
      
      // one more round ..
      sort_key_len 
        = ucol_getSortKey(coll, list[i]->utf16_term, list[i]->utf16_len,
                          tmp, tmp_len);
    }
    
    // copy result out
    memcpy(list[i]->sort_key, tmp, sort_key_len);    
  }
  icu_check_status(status);
  
  
  //printf("Sortkeys assigned, now sorting\n");  
  
  // do the sorting
  qsort(list, src_list_len, 
        sizeof(struct icu_termmap *), icu_termmap_cmp);
  
  
  printf("ICU sort:  '%s' : ", locale); 
  for (i = 0; i < src_list_len; i++) {
    printf(" '%s'", list[i]->disp_term); 
  }
  printf("\n"); 
  
ucol_close(coll);

 return 1;
};


int main(int argc, char **argv)
{
  
    size_t en_1_len = 6;
    const char * en_1_src[6] = {"z", "K", "a", "A", "Z", "k"};
    const char * en_1_cck[6] = {"a", "A", "K", "k", "z", "Z"};
    icu_coll_sort("en", en_1_len, en_1_src, en_1_cck);
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

  return 0;
};

