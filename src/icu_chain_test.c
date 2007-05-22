/**
 gcc -I/usr/include/libxml2 -lxml2 -o icu-xml-convert icu-xml-convert.c
 */

#include <stdio.h>
#include <string.h>

#include "icu_I18N.h"

/* commando line parameters */
static struct config_t { 
  //char infile[1024];
  //char locale[128];
  char conffile[1024];
  //char outfile[1024];
  int verbatim;
  int print;
} config;


  
void print_option_error(const struct config_t *p_config)
{  
  fprintf(stderr, "Calling error, valid options are :\n");
  fprintf(stderr, "icu_chain_test\n"
          "   [-c (path/to/config/file.xml)]\n"
          "   [-p (c|l|t)] print available info \n"
          "   [-v] verbouse output\n"
          "\n");
  exit(1);
}

void read_params(int argc, char **argv, struct config_t *p_config){    
  char *arg;
  int ret;
  
  /* set default parameters */
  p_config->conffile[0] = 0;
  
  /* set up command line parameters */
  
  while ((ret = options("c:p:v", argv, argc, &arg)) != -2)
    {
      switch (ret)
        {
        case 'c':
          strcpy(p_config->conffile, arg);
          break;
        case 'p':
          strcpy(p_config->print, arg);
          break;
        case 'v':
          if (arg)  
            p_config->verbatim = atoi(arg);
          else  
            p_config->verbatim = 1;
          break;
        default:
          print_option_error(p_config);
        }
    }


  if (! strlen(p_config->conffile))
    print_option_error();
}

/*     UConverter *conv; */
/*     conv = ucnv_open("utf-8", &status); */
/*     assert(U_SUCCESS(status)); */

/*     *ustr16_len  */
/*       = ucnv_toUChars(conv, ustr16, 1024,  */
/*                       (const char *) *xstr8, strlen((const char *) *xstr8), */
/*                       &status); */
  


/*      ucnv_fromUChars(conv, */
/*                      (char *) *xstr8, strlen((const char *) *xstr8), */
/*                      ustr16, *ustr16_len, */
/*                      &status); */
/*      ucnv_close(conv); */


static void print_icu_converters(const struct config_t *p_config)
{
  int32_t count;
  int32_t i;

  count = ucnv_countAvailable();
  printf("Available ICU converters: %d\n", count);
  
  for(i=0;i<count;i++) 
  {
    printf("%s ", ucnv_getAvailableName(i));
  }
  printf("\n");
  printf("Default ICU Converter is: '%s'\n", ucnv_getDefaultName());

  exit(0);
}

static void print_icu_transliterators(const struct config_t *p_config)
{
  int32_t count;
  int32_t i;

  count = utrans_countAvailableIDs();

  int32_t buf_cap = 128;
  char buf[buf_cap];

  if (1 < p_config->verbatim){
    printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    printf("<icu>\n<transliterators actions=\"%d\">\n",  count);
  } else 
    printf("Available ICU transliterators: %d\n", count);

  for(i=0;i<count;i++)
    {
      utrans_getAvailableID(i, buf, buf_cap);
       if (1 < p_config->verbatim)
         printf("<transliterator action=\"%s\"/>\n", buf);
       else
         printf(" %s", buf);
    }

  if (1 < p_config->verbatim){
    printf("</transliterators>\n</icu>\n");
  }
  else
    {
      printf("\n\nUnicode Set Patterns:\n"
             "   Pattern         Description\n"
             "   Ranges          [a-z] 	The lower case letters a through z\n"
             "   Named Chars     [abc123] The six characters a,b,c,1,2 and 3\n"
             "   String          [abc{def}] chars a, b and c, and string 'def'\n"
             "   Categories      [\\p{Letter}] Perl General Category 'Letter'.\n"
             "   Categories      [:Letter:] Posix General Category 'Letter'.\n"
             "\n"
             "   Combination     Example\n"
             "   Union           [[:Greek:] [:letter:]]\n"
             "   Intersection    [[:Greek:] & [:letter:]]\n"
             "   Set Complement  [[:Greek:] - [:letter:]]\n"
             "   Complement      [^[:Greek:] [:letter:]]\n"
             "\n"
             "see: http://icu.sourceforge.net/userguide/unicodeSet.html\n"
             "\n"
             "Examples:\n"
             "   [:Punctuation:] Any-Remove\n"
             "   [:Cased-Letter:] Any-Upper\n"
             "   [:Control:] Any-Remove\n"
             "   [:Decimal_Number:] Any-Remove\n"
             "   [:Final_Punctuation:] Any-Remove\n"
             "   [:Georgian:] Any-Upper\n"
             "   [:Katakana:] Any-Remove\n"
             "   [:Arabic:] Any-Remove\n"
             "   [:Punctuation:] Remove\n"
             "   [[:Punctuation:]-[.,]] Remove\n"
             "   [:Line_Separator:] Any-Remove\n"
             "   [:Math_Symbol:] Any-Remove\n"
             "   Lower; [:^Letter:] Remove (word tokenization)\n"
             "   [:^Number:] Remove (numeric tokenization)\n"
             "   [:^Katagana:] Remove (remove everything except Katagana)\n"
             "   Lower;[[:WhiteSpace:][:Punctuation:]] Remove (word tokenization)\n"
             "   NFD; [:Nonspacing Mark:] Remove; NFC   (removes accents from characters)\n"
             "   [A-Za-z]; Lower(); Latin-Katakana; Katakana-Hiragana (transforms latin and katagana to hiragana)\n"
             "   [[:separator:][:start punctuation:][:initial punctuation:]] Remove \n"
             "\n"
             "see http://icu.sourceforge.net/userguide/Transform.html\n"
             "    http://www.unicode.org/Public/UNIDATA/UCD.html\n"
             "    http://icu.sourceforge.net/userguide/Transform.html\n"
             "    http://icu.sourceforge.net/userguide/TransformRule.html\n"
             );
    }


  printf("\n\n");


  exit(0);
}

static void print_icu_xml_locales(const struct config_t *p_config)
{
  int32_t count;
  int32_t i;
  UErrorCode status = U_ZERO_ERROR;

  UChar keyword[64];
  int32_t keyword_len = 0;
  char keyword_str[128];
  int32_t keyword_str_len = 0;

  UChar language[64];
  int32_t language_len = 0;
  char lang_str[128];
  int32_t lang_str_len = 0;

  UChar script[64];
  int32_t script_len = 0;
  char script_str[128];
  int32_t script_str_len = 0;

  UChar location[64];
  int32_t location_len = 0;
  char location_str[128];
  int32_t location_str_len = 0;

  UChar variant[64];
  int32_t variant_len = 0;
  char variant_str[128];
  int32_t variant_str_len = 0;

  UChar name[64];
  int32_t name_len = 0;
  char name_str[128];
  int32_t name_str_len = 0;

  UChar localname[64];
  int32_t localname_len = 0;
  char localname_str[128];
  int32_t localname_str_len = 0;

  count = uloc_countAvailable() ;

  if (1 < p_config->verbatim){
    printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    printf("<icu>\n<locales count=\"%d\" default=\"%s\" collations=\"%d\">\n", 
           count, uloc_getDefault(), ucol_countAvailable());
  }
  
  for(i=0;i<count;i++) 
  {

    keyword_len 
      = uloc_getDisplayKeyword(uloc_getAvailable(i), "en", 
                                keyword, 64, 
                                &status);

    u_strToUTF8(keyword_str, 128, &keyword_str_len,
                keyword, keyword_len,
                &status);
    
    
    language_len 
      = uloc_getDisplayLanguage(uloc_getAvailable(i), "en", 
                                language, 64, 
                                &status);

    u_strToUTF8(lang_str, 128, &lang_str_len,
                language, language_len,
                &status);


    script_len 
      = uloc_getDisplayScript(uloc_getAvailable(i), "en", 
                                script, 64, 
                                &status);

    u_strToUTF8(script_str, 128, &script_str_len,
                script, script_len,
                &status);

    location_len 
      = uloc_getDisplayCountry(uloc_getAvailable(i), "en", 
                                location, 64, 
                                &status);

    u_strToUTF8(location_str, 128, &location_str_len,
                location, location_len,
                &status);

    variant_len 
      = uloc_getDisplayVariant(uloc_getAvailable(i), "en", 
                                variant, 64, 
                                &status);

    u_strToUTF8(variant_str, 128, &variant_str_len,
                variant, variant_len,
                &status);

    name_len 
      = uloc_getDisplayName(uloc_getAvailable(i), "en", 
                                name, 64, 
                                &status);

    u_strToUTF8(name_str, 128, &name_str_len,
                name, name_len,
                &status);

    localname_len 
      = uloc_getDisplayName(uloc_getAvailable(i), uloc_getAvailable(i), 
                                localname, 64, 
                                &status);

    u_strToUTF8(localname_str, 128, &localname_str_len,
                localname, localname_len,
                &status);


    if (1 < p_config->verbatim){
      printf("<locale");
      printf(" xml:lang=\"%s\"", uloc_getAvailable(i)); 
      /* printf(" locale=\"%s\"", uloc_getAvailable(i)); */
      /* if (strlen(keyword_str)) */
      /*   printf(" keyword=\"%s\"", keyword_str); */
      /* if (ucol_getAvailable(i)) */
      /*   printf(" collation=\"1\""); */
      if (strlen(lang_str))
        printf(" language=\"%s\"", lang_str);
      if (strlen(script_str))
        printf(" script=\"%s\"", script_str);
      if (strlen(location_str))
        printf(" location=\"%s\"", location_str);
      if (strlen(variant_str))
        printf(" variant=\"%s\"", variant_str);
      if (strlen(name_str))
        printf(" name=\"%s\"", name_str);
      if (strlen(localname_str))
        printf(" localname=\"%s\"", localname_str);
      printf(">");
      if (strlen(localname_str))
        printf("%s", localname_str);
      printf("</locale>\n"); 
    }
    else if (1 == p_config->verbatim){
      printf("%s", uloc_getAvailable(i)); 
      printf(" | ");
      if (strlen(name_str))
        printf("%s", name_str);
      printf(" | ");
      if (strlen(localname_str))
        printf("%s", localname_str);
      printf("\n");
    }
    else
      printf("%s ", uloc_getAvailable(i));
  }
  if (1 < p_config->verbatim)
    printf("</locales>\n</icu>\n");
  else
    printf("\n");

  if(U_FAILURE(status)) {
    fprintf(stderr, "ICU Error: %d %s\n", status, u_errorName(status));
    exit(status);
  }
  exit(0);
}


int main(int argc, char **argv) {

  //LIBXML_TEST_VERSION;

  read_params(argc, argv, &config);

  if (config.debug)
    print_options(&config);

  if ('c' == config.print[0])
    print_icu_converters(&config);

  if ('l' == config.print[0])
    print_icu_xml_locales(&config);

  if ('t' == config.print[0])
    print_icu_transliterators(&config);
  
  //xmlCleanupParser();
  //xmlMemoryDump();
  return(0);
}


