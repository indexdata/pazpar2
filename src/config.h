#ifndef CONFIG_H
#define CONFIG_H

#include <libxslt/xslt.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

enum conf_sortkey_type
{
    Metadata_sortkey_relevance,
    Metadata_sortkey_numeric,       // Standard numerical sorting
    Metadata_sortkey_skiparticle,   // Skip leading article when sorting
    Metadata_sortkey_string         // Flat string
};

// Describes known metadata elements and how they are to be manipulated
// An array of these structure provides a 'map' against which discovered metadata
// elements are matched. It also governs storage, to minimize number of cycles needed
// at various tages of processing
struct conf_metadata 
{
    char *name;  // The name of this element. Output by normalization stylesheet
    int brief;   // Is this element to be returned in the brief format?
    int termlist;// Is this field to be treated as a termlist for browsing?
    int rank;    // Rank factor. 0 means don't use this field for ranking, 1 is default
                 // values >1  give additional significance to a field
    int sortkey_offset; // -1 if it's not a sortkey, otherwise index
                        // into service/record_cluster->sortkey array
    enum
    {
        Metadata_type_generic,          // Generic text field
        Metadata_type_number,           // A number
        Metadata_type_year              // A number
    } type;
    enum
    {
        Metadata_merge_no,              // Don't merge
        Metadata_merge_unique,          // Include unique elements in merged block
        Metadata_merge_longest,         // Include the longest (strlen) value
        Metadata_merge_range,           // Store value as a range of lowest-highest
        Metadata_merge_all              // Just include all elements found
    } merge;
};

// Controls sorting
struct conf_sortkey
{
    char *name;
    enum conf_sortkey_type type;
};

// It is conceivable that there will eventually be several 'services' offered
// from one server, with separate configuration -- possibly more than one services
// associated with the same port. For now, however, only a single service is possible.
struct conf_service
{
    int num_metadata;
    struct conf_metadata *metadata;
    int num_sortkeys;
    struct conf_sortkey *sortkeys;
};

struct conf_server
{
    char *host;
    int port;
    char *proxy_host;
    int proxy_port;
    char *myurl;
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
