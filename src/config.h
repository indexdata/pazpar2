#ifndef CONFIG_H
#define CONFIG_H

#include <libxslt/xslt.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

// Describes known metadata elements and how they are to be manipulated
struct conf_metadata 
{
    char *name;  // The name of this element. Output by normalization stylesheet
    int brief;   // Is this element to be returned in the brief format?
    int termlist;// Is this field to be treated as a termlist for browsing?
    int rank;    // Rank factor. 0 means don't use this field for ranking, 1 is default
                 // values >1  give additional significance to a field
    enum
    {
        Metadata_type_generic,          // Generic text field
        Metadata_type_integer,          // Integer type
        Metadata_type_year              // A year
    } type;
    enum
    {
        Metadata_sortkey_no,            // This is not to be used as a sortkey
        Metadata_sortkey_numeric,       // Standard numerical sorting
        Metadata_sortkey_range,         // Range sorting (pick lowest or highest)
        Metadata_sortkey_skiparticle,   // Skip leading article when sorting
        Metadata_sortkey_string
    } sortkey;
    enum
    {
        Metadata_merge_no,              // Don't merge
        Metadata_merge_unique,          // Include unique elements in merged block
        Metadata_merge_longest,         // Include the longest (strlen) value
        Metadata_merge_range,           // Store value as a range of lowest-highest
        Metadata_merge_all              // Just include all elements found
    } merge;
};

struct conf_service
{
    int num_metadata;
    struct conf_metadata *metadata;
};

struct conf_server
{
    char *host;
    int port;
    char *proxy_host;
    int proxy_port;
    struct conf_service *service;
    struct conf_server *next;
};

struct conf_queryprofile
{
    int dummy;
};

struct conf_retrievalmap
{
    enum {
        Map_xslt
    } type;
    char *charset;
    char *format;
    xsltStylesheet *stylesheet;
    struct conf_retrievalmap *next;
};

struct conf_retrievalprofile
{
    char *requestsyntax;
    enum {
        Nativesyn_xml,
        Nativesyn_iso2709
    } native_syntax;
    enum {
        Nativeform_na,
        Nativeform_marc21,
    } native_format;
    char *native_encoding;
    enum {
        Nativemapto_na,
        Nativemapto_marcxml,
        Nativemapto_marcxchange
    } native_mapto;
    yaz_marc_t yaz_marc;
    struct conf_retrievalmap *maplist;
    struct conf_retrievalprofile *next;
};

struct conf_config
{
    struct conf_server *servers;
    struct conf_queryprofile *queryprofiles;
    struct conf_retrievalprofile *retrievalprofiles;
};

#ifndef CONFIG_NOEXTERNS

extern struct conf_config *config;

#endif

int read_config(const char *fname);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
