#ifndef CONFIG_H
#define CONFIG_H

#include <libxslt/xslt.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

struct conf_termlist
{
    char *name;
    struct conf_termlist *next;
};

struct conf_service
{
    struct conf_termlist *termlists;
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
