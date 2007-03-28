// $Id: settings.c,v 1.2 2007-03-28 04:33:41 quinn Exp $
// This module implements a generic system of settings (attribute-value) that can 
// be associated with search targets. The system supports both default values,
// per-target overrides, and per-user settings.
//

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <yaz/nmem.h>
#include <yaz/log.h>

static NMEM nmem = 0;

#define PZ_PIGGYBACK    0 
#define PZ_ELEMENTS     1
#define PZ_SYNTAX       2

static char *hard_settings[] = {
    "pz:piggyback",
    "pz:elements",
    "pz::syntax",
    0
};

struct setting
{
    char *target;
    char *name;
    char *value;
    char *user;
};

struct setting_dictionary
{
    char **dict;
    int size;
    int num;
};

static int isdir(const char *path)
{
    struct stat st;

    if (stat(path, &st) < 0)
    {
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "%s", path);
        exit(1);
    }
    return st.st_mode & S_IFDIR;
}

static void read_settings_file(const char *path, void *context,
        void (*fun)(void *context, struct setting *set))
{
    xmlDoc *doc = xmlParseFile(path);
    xmlNode *n;
    //xmlChar *namea, *targeta, *valuea, *usera;

    if (!doc)
    {
        yaz_log(YLOG_FATAL, "Failed to parse %s", path);
        exit(1);
    }
    n = xmlDocGetRootElement(doc);

    for (n = n->children; n; n = n->next)
    {
        fprintf(stderr, "Node name: %s\n", n->name);
    }
}
 
// Recursively read files in a directory structure, calling 
static void read_settings(const char *path, void *context,
		void (*fun)(void *context, struct setting *set))
{
    DIR *d;
    struct dirent *de;

    if (!(d = opendir(path)))
    {
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "%s", path);
        exit(1);
    }
    while ((de = readdir(d)))
    {
        char tmp[1024];
        if (*de->d_name == '.' || !strcmp(de->d_name, "CVS"))
            continue;
        sprintf(tmp, "%s/%s", path, de->d_name);
        if (isdir(tmp))
            read_settings(tmp, context, fun);
        else
        {
            char *dot;
            if ((dot = rindex(de->d_name, '.')) && !strcmp(dot + 1, "xml"))
                read_settings_file(tmp, context, fun);
        }
    }
    closedir(d);
}

// Callback. Adds a new entry to the dictionary if necessary
// This is used in pass 1 to determine layout of dictionary
static void prepare_dictionary(void *context, struct setting *set)
{
    struct setting_dictionary *dict = (struct setting_dictionary *) context;
    int i;

    for (i = 0; i < dict->num; i++)
        if (!strcmp(set->name, set->name))
            return;
    // Create a new dictionary entry
    // Grow dictionary if necessary
    if (!dict->size)
        dict->dict = nmem_malloc(nmem, (dict->size = 50) * sizeof(char*));
    else if (dict->num + 1 > dict->size)
    {
        char **tmp = nmem_malloc(nmem, dict->size * 2 * sizeof(char*));
        memcpy(tmp, dict->dict, dict->size * sizeof(char*));
        dict->dict = tmp;
        dict->size *= 2;
    }
    dict->dict[dict->num++] = nmem_strdup(nmem, set->name);
}

#ifdef GAGA
// Callback -- updates database records with dictionary entries as appropriate
static void update_databases(void *context, struct setting *set)
{
}
#endif

// This simply copies the 'hard' (application-specific) settings
// to the settings dictionary.
static void initialize_hard_settings(struct setting_dictionary *dict)
{
    dict->dict = nmem_malloc(nmem, sizeof(hard_settings) - sizeof(char*));
    dict->size = (sizeof(hard_settings) - sizeof(char*)) / sizeof(char*);
    memcpy(dict->dict, hard_settings, dict->size * sizeof(char*));
    dict->num = dict->size;
}

// If we ever decide we need to be able to specify multiple settings directories,
// the two calls to read_settings must be split -- so the dictionary is prepared
// for the contents of every directory before the databases are updated.
void settings_read(const char *path)
{
    struct setting_dictionary *new;
    if (!nmem)
        nmem = nmem_create();
    else
        nmem_reset(nmem);
    new = nmem_malloc(nmem, sizeof(*new));
    initialize_hard_settings(new);
    memset(new, sizeof(*new), 0);
    read_settings(path, new, prepare_dictionary);
    //read_settings(path, new, update_databases);
}

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
