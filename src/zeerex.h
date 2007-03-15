#ifndef ZEEREX_H
#define ZEEREX_H

// Structures representing a Zeerex record.

typedef enum zr_bool
{
    Zr_bool_unknown,
    Zr_bool_false,
    Zr_bool_true
} Zr_bool;

typedef struct zr_langstr
{
    Zr_bool primary;
    char *lang;
    char *str;
    struct zr_langstr *next;
} Zr_langstr;

struct zr_authentication
{
    char *type;
    char *open;
    char *user;
    char *group;
    char *password;
};

struct zr_serverInfo
{
    char *protocol;
    char *version;
    char *transport;
    char *method;
    char *host;
    int port;
    char *database;
    struct zr_authentication *authentication;
};

struct zr_agent
{
    char *type;
    char *identifier;
    char *value;
    struct zr_agent *next;
};

struct zr_link
{
    char *type;
    char *value;
    struct zr_link *next;
};

struct zr_implementation
{
    char *identifier;
    char *version;
    struct zr_agent *agents;
    Zr_langstr *title;
};

struct zr_databaseInfo
{
    Zr_langstr *title;
    Zr_langstr *description;
    Zr_langstr *history;
    char *lastUpdate;
    Zr_langstr *extent;
    int numberOfRecords;
    Zr_langstr *restrictions;
    Zr_langstr *langUsage;
    char *codes;
    struct zr_agent *agents;
    struct zr_implementation *implementation;
    struct zr_link *links;
};

struct zr_metaInfo
{
    char *dateModified;
    char *dateAggregated;
    char *aggregatedFrom;
};

struct zr_set
{
    Zr_langstr *title;
    char *name;
    char *identifier;
    struct zr_set *next;
};

struct zr_attr
{
    int type;
    char *set;
    char *value;
    struct zr_attr *next;
};

struct zr_map
{
    Zr_bool primary;
    char *lang;
    char *name;
    char *set;
    struct zr_attr *attrs;
    struct zr_map *next;
};

typedef struct zr_setting
{
    char *type;
    char *value;
    struct zr_map *map;
    struct zr_setting *next;
} Zr_setting;

struct zr_configInfo
{
    Zr_setting *defaultv;
    Zr_setting *setting;
    Zr_setting *supports;
};

struct zr_index
{
    Zr_bool search;
    Zr_bool scan;
    Zr_bool sort;
    char *id;
    Zr_langstr *title;
    struct zr_map *maps;
    struct zr_configInfo *configInfo;
    struct zr_index *next;
};

struct zr_sortKeyword
{
    char *value;
    struct zr_sortKeyword *next;
};

struct zr_indexInfo
{
    struct zr_set *sets;
    struct zr_index *indexes;
    struct zr_sortKeyword *sortKeywords;
    struct zr_configInfo *configInfo;
};

struct zr_elementSet
{
    char *name;
    char *identifier;
    Zr_langstr *title;
    struct zr_elementSet *next;
};

struct zr_recordSyntax
{
    char *name;
    char *identifier;
    struct zr_elementSet *elementSets;
    struct zr_recordSyntax *next;
};

struct zr_recordInfo
{
    struct zr_recordSyntax *recordSyntaxes;
};

struct zr_schema
{
    char *name;
    char *identifier;
    Zr_bool retrieve;
    Zr_bool sort;
    char *location;
    Zr_langstr *title;
    struct zr_schema *next;
};

struct zr_schemaInfo
{
    struct zr_schema *schemas;
};

struct zr_explain
{
    struct zr_serverInfo *serverInfo;
    struct zr_databaseInfo *databaseInfo;
    struct zr_metaInfo *metaInfo;
    struct zr_indexInfo *indexInfo;
    struct zr_recordInfo *recordInfo;
    struct zr_schemaInfo *schemaInfo;
    struct zr_configInfo *configInfo;
};

struct zr_explain *zr_read_xml(NMEM m, xmlNode *n);
struct zr_explain *zr_read_file(NMEM m, const char *fn);
const char *zr_langstr(Zr_langstr *s, const char *lang);

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

#endif
