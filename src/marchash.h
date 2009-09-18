#ifndef MARCHASH_H
#define MARCHASH_H

#define MARCHASH_MASK 127

struct marchash
{
    struct marcfield *table[MARCHASH_MASK + 1];
    NMEM nmem;
};

struct marcfield
{
   char key[4];
   char *val;
   struct marcsubfield *subfields;
   struct marcfield *next;
};

struct marcsubfield
{
   char key;
   char *val;
   struct marcsubfield *next;
};

struct marchash *marchash_create (NMEM nmem);
int marchash_ingest_marcxml (struct marchash *marchash, xmlNodePtr rec_node);
struct marcfield *marchash_add_field (struct marchash *marchash, char *key, char *value);
struct marcsubfield *marchash_add_subfield (struct marchash *marchash, struct marcfield *field, char key, char *value);
struct marcfield *marchash_get_field (struct marchash *marchash, char *key, struct marcfield *last);
struct marcsubfield *marchash_get_subfield (char key, struct marcfield *field, struct marcsubfield *last);
#endif
