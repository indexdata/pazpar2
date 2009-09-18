#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <yaz/nmem.h>

#include <marcmap.h>
#include <marchash.h>

struct marcmap *marcmap_load(char *filename, NMEM nmem) {
    struct marcmap *mm;
    struct marcmap *mmhead;
    FILE *fp;
    char c;
    char buf[256];
    int len;
    int field;
    int newrec;

    len = 0;
    field = 0;
    newrec = 1;
    mm = NULL;
    mmhead = NULL;
    fp = fopen(filename, "r");

    while ((c = getc(fp) ) != EOF) 
    {
        // allocate some space
        if (newrec)
        {
            if (mm != NULL) 
            {
                mm->next = nmem_malloc(nmem, sizeof(struct marcmap));
                mm = mm->next;
            }
            // first one!
            else 
            { mm = nmem_malloc(nmem, sizeof(struct marcmap));
                mmhead = mm;
            }
            newrec = 0;
        }
	// whitespace saves and moves on
	if (c == ' ' || c == '\n' || c == '\t')
        {
            buf[len] = '\0';
            len++;
            // first field, marc
            if (field == 0)
            {
                // allow blank lines
		if (!(len <3))
                {
                    mm->field = nmem_malloc(nmem, len * sizeof(char));
                    strncpy(mm->field, buf, len);
                }
            }
            // second, marc subfield, just a char
            else if (field == 1)
            {
                mm->subfield = buf[len-2];
            }
            // third, pz fieldname
            else if (field == 2) 
            { 
                mm->pz = nmem_malloc(nmem, len * sizeof(char));
                strncpy(mm->pz, buf, len);
            }

            // new line, new record
            if (c == '\n')
            {
                field = 0;
                newrec = 1;
            }
            else
            {
                field++;
            }
            len = 0;
        }
        else
        {
            buf[len] = c;
            len++;
        }
    }
    mm->next = NULL;
    return mmhead;
}

xmlDoc *marcmap_apply(struct marcmap *marcmap, xmlDoc *xml_in)
{
    char mergekey[1024];
    char medium[32];
    char *s;
    NMEM nmem;
    xmlNsPtr ns_pz;
    xmlDocPtr xml_out;
    xmlNodePtr xml_out_root;
    xmlNodePtr rec_node;
    xmlNodePtr meta_node; 
    struct marchash *marchash;
    struct marcfield *field;
    struct marcsubfield *subfield;
    struct marcmap *mmcur;
     
    xml_out = xmlNewDoc(BAD_CAST "1.0");
    xml_out_root = xmlNewNode(NULL, BAD_CAST "record");
    xmlDocSetRootElement(xml_out, xml_out_root);
    ns_pz = xmlNewNs(xml_out_root, BAD_CAST "http://www.indexdata.com/pazpar2/1.0", BAD_CAST "pz"); 
    nmem = nmem_create();
    rec_node = xmlDocGetRootElement(xml_in);
    marchash = marchash_create(nmem);
    marchash_ingest_marcxml(marchash, rec_node);

    mmcur = marcmap;
    while (mmcur != NULL)
    {
        if (field = marchash_get_field(marchash, mmcur->field, NULL))
            do
            {
                // field value
                if ((mmcur->subfield == '$') && (s = field->val))
                {
                    meta_node = xmlNewChild(xml_out_root, ns_pz, BAD_CAST "metadata", s);
                    xmlSetProp(meta_node, BAD_CAST "type", mmcur->pz); 
                }
                // catenate all subfields
                else if ((mmcur->subfield == '*') && (s = marchash_catenate_subfields(field, " ", nmem)))
                {
                    meta_node = xmlNewChild(xml_out_root, ns_pz, BAD_CAST "metadata", s);
                    xmlSetProp(meta_node, BAD_CAST "type", mmcur->pz);
                }
                // subfield value
                else if (mmcur->subfield) 
                {
                    if (subfield = marchash_get_subfield(mmcur->subfield, field, NULL)) 
                        do
                            if (s = subfield->val)
                            {
                                meta_node = xmlNewChild(xml_out_root, ns_pz, BAD_CAST "metadata", s);
                                xmlSetProp(meta_node, BAD_CAST "type", mmcur->pz);
                            }
                        while (subfield = marchash_get_subfield(mmcur->subfield, field, subfield));
                }
                
            }
            while (field = marchash_get_field(marchash, mmcur->field, field));
        mmcur = mmcur->next;
    }

    // hard coded mappings

    // medium
    if ((field = marchash_get_field(marchash, "245", NULL)) && (subfield = marchash_get_subfield('h', field, NULL)))
    {
       strncpy(medium, subfield->val, 32);
    }
    else if ((field = marchash_get_field(marchash, "900", NULL)) && (subfield = marchash_get_subfield('a', field, NULL)))
       strcpy(medium, "electronic resource");
    else if ((field = marchash_get_field(marchash, "900", NULL)) && (subfield = marchash_get_subfield('b', field, NULL)))
       strcpy(medium, "electronic resource");
    else if ((field = marchash_get_field(marchash, "773", NULL)) && (subfield = marchash_get_subfield('t', field, NULL)))
       strcpy(medium, "article");
    else
       strcpy(medium, "book");

    meta_node = xmlNewChild(xml_out_root, ns_pz, BAD_CAST "metadata", BAD_CAST medium);
    xmlSetProp(meta_node, BAD_CAST "type", BAD_CAST "medium");

    // merge key
    memset(mergekey, 0, 1024);
    strcpy(mergekey, "title ");
    if ((field = marchash_get_field(marchash, "245", NULL)) && (subfield = marchash_get_subfield('a', field, NULL)))
        strncat(mergekey, subfield->val, 1023 - strlen(mergekey));
    strncat(mergekey, " author ", 1023 - strlen(mergekey));
    if ((field = marchash_get_field(marchash, "245", NULL)) && (subfield = marchash_get_subfield('a', field, NULL)))
        strncat(mergekey, subfield->val, 1023 - strlen(mergekey));
    strncat(mergekey, " medium ", 1023 - strlen(mergekey));
    strncat(mergekey, medium, 1023 - strlen(mergekey));

    xmlSetProp(xml_out_root, BAD_CAST "mergekey", BAD_CAST mergekey);

    nmem_destroy(nmem);
    return xml_out;
}
