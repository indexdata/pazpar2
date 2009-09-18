#ifndef MARCMAP_H
#define MARCMAP_H

struct marcmap
{ 
    char *field;
    char subfield;
    char *pz;
    struct marcmap *next;
};

struct marcmap *marcmap_load(char *filename, NMEM nmem);
xmlDoc *marcmap_apply(struct marcmap *marcmap, xmlDoc *xml_in);

#endif
