// $Id: settings.c,v 1.5 2007-03-31 19:55:25 marc Exp $
// This module implements a generic system of settings (attribute-value) that can 
// be associated with search targets. The system supports both default values,
// per-target overrides, and per-user settings.

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

#include "pazpar2.h"
#include "database.h"
#include "settings.h"

static NMEM nmem = 0;

// Used for initializing setting_dictionary with pazpar2-specific settings
static char *hard_settings[] = {
    "pz:piggyback",
    "pz:elements",
    "pz:syntax",
    "pz:cclmap:",
    0
};

struct setting_dictionary
{
    char **dict;
    int size;
    int num;
};

static struct setting_dictionary *dictionary = 0;

int settings_offset(const char *name)
{
    int i;

    for (i = 0; i < dictionary->num; i++)
        if (!strcmp(name, dictionary->dict[i]))
            return i;
    return -1;
}

// Ignores everything after second colon, if present
// A bit of a hack to support the pz:cclmap: scheme (and more to come?)
static int settings_offset_cprefix(const char *name)
{
    const char *p;
    int maxlen = 100;
    int i;

    if (!strncmp("pz:", name, 3) && (p = strchr(name + 3, ':')))
        maxlen = (p - name) + 1;
    for (i = 0; i < dictionary->num; i++)
        if (!strncmp(name, dictionary->dict[i], maxlen))
            return i;
    return -1;
}

char *settings_name(int offset)
{
    return dictionary->dict[offset];
}

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

// Read settings from an XML file, calling handler function for each setting
static void read_settings_file(const char *path,
        void (*fun)(struct setting *set))
{
    xmlDoc *doc = xmlParseFile(path);
    xmlNode *n;
    xmlChar *namea, *targeta, *valuea, *usera, *precedencea;

    if (!doc)
    {
        yaz_log(YLOG_FATAL, "Failed to parse %s", path);
        exit(1);
    }
    n = xmlDocGetRootElement(doc);
    namea = xmlGetProp(n, (xmlChar *) "name");
    targeta = xmlGetProp(n, (xmlChar *) "target");
    valuea = xmlGetProp(n, (xmlChar *) "value");
    usera = xmlGetProp(n, (xmlChar *) "user");
    precedencea = xmlGetProp(n, (xmlChar *) "precedence");
    for (n = n->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp((const char *) n->name, "set"))
        {
            char *name, *target, *value, *user, *precedence;

            name = (char *) xmlGetProp(n, (xmlChar *) "name");
            target = (char *) xmlGetProp(n, (xmlChar *) "target");
            value = (char *) xmlGetProp(n, (xmlChar *) "value");
            user = (char *) xmlGetProp(n, (xmlChar *) "user");
            precedence = (char *) xmlGetProp(n, (xmlChar *) "precedence");

            if ((!name && !namea) || (!value && !valuea) || (!target && !targeta))
            {
                yaz_log(YLOG_FATAL, "set must specify name, value, and target");
                exit(1);
            }
            else
            {
                struct setting set;
                char nameb[1024];
                char targetb[1024];
                char userb[1024];
                char valueb[1024];

                // Copy everything into a temporary buffer -- we decide
                // later if we are keeping it.
                if (precedence)
                    set.precedence = atoi((char *) precedence);
                else if (precedencea)
                    set.precedence = atoi((char *) precedencea);
                else
                    set.precedence = 0;
                set.user = userb;
                if (user)
                    strcpy(userb, user);
                else if (usera)
                    strcpy(userb, (const char *) usera);
                else
                    set.user = "";
                if (target)
                    strcpy(targetb, target);
                else
                    strcpy(targetb, (const char *) targeta);
                set.target = targetb;
                if (name)
                    strcpy(nameb, name);
                else
                    strcpy(nameb, (const char *) namea);
                set.name = nameb;
                if (value)
                    strcpy(valueb, value);
                else
                    strcpy(valueb, (const char *) valuea);
                set.value = valueb;
                set.next = 0;
                (*fun)(&set);
            }
            xmlFree(name);
            xmlFree(precedence);
            xmlFree(value);
            xmlFree(user);
            xmlFree(target);
        }
        else
        {
            yaz_log(YLOG_FATAL, "Unknown element %s in settings file", (char*) n->name);
            exit(1);
        }
    }
    xmlFree(namea);
    xmlFree(precedencea);
    xmlFree(valuea);
    xmlFree(usera);
    xmlFree(targeta);
}
 
// Recursively read files in a directory structure, calling 
// callback for each one
static void read_settings(const char *path,
		void (*fun)(struct setting *set))
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
            read_settings(tmp, fun);
        else
        {
            char *dot;
            if ((dot = rindex(de->d_name, '.')) && !strcmp(dot + 1, "xml"))
                read_settings_file(tmp, fun);
        }
    }
    closedir(d);
}

// Callback. Adds a new entry to the dictionary if necessary
// This is used in pass 1 to determine layout of dictionary
static void prepare_dictionary(struct setting *set)
{
    int i;
    char *p;

    if (!strncmp(set->name, "pz:", 3) && (p = strchr(set->name + 3, ':')))
        *(p + 1) = '\0';
    for (i = 0; i < dictionary->num; i++)
        if (!strcmp(dictionary->dict[i], set->name))
            return;
    // Create a new dictionary entry
    // Grow dictionary if necessary
    if (!dictionary->size)
        dictionary->dict = nmem_malloc(nmem, (dictionary->size = 50) * sizeof(char*));
    else if (dictionary->num + 1 > dictionary->size)
    {
        char **tmp = nmem_malloc(nmem, dictionary->size * 2 * sizeof(char*));
        memcpy(tmp, dictionary->dict, dictionary->size * sizeof(char*));
        dictionary->dict = tmp;
        dictionary->size *= 2;
    }
    dictionary->dict[dictionary->num++] = nmem_strdup(nmem, set->name);
}

// This is called from grep_databases -- adds/overrides setting for a target
// This is also where the rules for precedence of settings are implemented
static void update_database(void *context, struct database *db)
{
    struct setting *set = (struct setting *) context;
    struct setting *s, **sp;
    int offset;

    if (!db->settings)
    {
        db->settings = nmem_malloc(nmem, sizeof(struct settings*) * dictionary->num);
        memset(db->settings, 0, sizeof(struct settings*) * dictionary->num);
    }
    if ((offset = settings_offset_cprefix(set->name)) < 0)
        abort(); // Should never get here

    // First we determine if this setting is overriding  any existing settings
    // with the same name.
    for (s = db->settings[offset], sp = &db->settings[offset]; s;
            sp = &s->next, s = s->next)
        if (!strcmp(s->user, set->user) && !strcmp(s->name, set->name))
        {
            if (s->precedence < set->precedence)
                // We discard the value (nmem keeps track of the space)
                *sp = (*sp)->next;
            else if (s->precedence > set->precedence)
                // Db contains a higher-priority setting. Abort 
                break;
            if (*s->target == '*' && *set->target != '*')
                // target-specific value trumps wildcard. Delete.
                *sp = (*sp)->next;
            else if (*s->target != '*' && *set->target == '*')
                // Db already contains higher-priority setting. Abort
                break;
        }
    if (!s) // s will be null when there are no higher-priority settings -- we add one
    {
        struct setting *new = nmem_malloc(nmem, sizeof(*new));

        memset(new, 0, sizeof(*new));
        new->precedence = set->precedence;
        new->target = nmem_strdup(nmem, set->target);
        new->name = nmem_strdup(nmem, set->name);
        new->value = nmem_strdup(nmem, set->value);
        new->user = nmem_strdup(nmem, set->user);
        new->next = db->settings[offset];
        db->settings[offset] = new;
    }
}

// Callback -- updates database records with dictionary entries as appropriate
// This is used in pass 2 to assign name/value pairs to databases
static void update_databases(struct setting *set)
{
    struct database_criterion crit;
    struct database_criterion_value val;

    // Update all databases which match pattern in set->target
    crit.name = "id";
    crit.values = &val;
    crit.next = 0;
    val.value = set->target;
    val.next = 0;
    grep_databases(set, &crit, update_database);
}

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
    memset(new, 0, sizeof(*new));
    initialize_hard_settings(new);
    dictionary = new;
    read_settings(path, prepare_dictionary);
    read_settings(path, update_databases);
}

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
