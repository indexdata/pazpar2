/* This file is part of Pazpar2.
   Copyright (C) Index Data

Pazpar2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

Pazpar2 is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/

// This module implements a generic system of settings
// (attribute-value) that can be associated with search targets. The
// system supports both default values, per-target overrides, and
// per-user settings.

#if HAVE_CONFIG_H
#include <config.h>
#endif


#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <yaz/dirent.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <yaz/nmem.h>
#include <yaz/log.h>

#include "session.h"
#include "database.h"
#include "settings.h"

// Used for initializing setting_dictionary with pazpar2-specific settings
static char *hard_settings[] = {
    "pz:piggyback",
    "pz:elements",
    "pz:requestsyntax",
    "pz:cclmap:",
    "pz:xslt",
    "pz:nativesyntax",
    "pz:authentication",
    "pz:allow",
    "pz:maxrecs",
    "pz:id",
    "pz:name",
    "pz:queryencoding",
    "pz:zproxy",
    "pz:apdulog",
    "pz:sru",
    "pz:sru_version",
    "pz:pqf_prefix",
    "pz:sort",
    "pz:recordfilter",
    "pz:pqf_strftime",
    "pz:negotiation_charset",
    "pz:max_connections",
    "pz:reuse_connections",
    "pz:termlist_term_factor",
    "pz:termlist_term_count",
    "pz:preferred",
    "pz:extra_args",
    "pz:query_syntax",
    "pz:facetmap:",
    "pz:limitmap:",
    "pz:url",
    "pz:sortmap:",
    "pz:present_chunk",
    "pz:block_timeout",
    "pz:extendrecs",
    "pz:authentication_mode",
    "pz:native_score",
    "pz:memcached",
    0
};

struct setting_dictionary
{
    char **dict;
    int size;
    int num;
};

// This establishes the precedence of wildcard expressions
#define SETTING_WILDCARD_NO     0 // No wildcard
#define SETTING_WILDCARD_DB     1 // Database wildcard 'host:port/*'
#define SETTING_WILDCARD_YES    2 // Complete wildcard '*'

// Returns size of settings directory
int settings_num(struct conf_service *service)
{
    return service->dictionary->num;
}

/* Find and possible create a new dictionary entry. Pass valid NMEM pointer if creation is allowed, otherwise null */
static int settings_index_lookup(struct setting_dictionary *dictionary, const char *name, NMEM nmem)
{
    size_t maxlen;
    int i;
    const char *p;

    assert(name);

    if (!strncmp("pz:", name, 3) && (p = strchr(name + 3, ':')))
        maxlen = (p - name) + 1;
    else
        maxlen = strlen(name) + 1;
    for (i = 0; i < dictionary->num; i++)
        if (!strncmp(name, dictionary->dict[i], maxlen))
            return i;
    if (!nmem)
        return -1;
    if (!strncmp("pz:", name, 3))
        yaz_log(YLOG_WARN, "Adding pz-type setting name %s", name);
    if (dictionary->num + 1 > dictionary->size)
    {
        char **tmp =
            nmem_malloc(nmem, dictionary->size * 2 * sizeof(char*));
        memcpy(tmp, dictionary->dict, dictionary->size * sizeof(char*));
        dictionary->dict = tmp;
        dictionary->size *= 2;
    }
    dictionary->dict[dictionary->num] = nmem_strdup(nmem, name);
    dictionary->dict[dictionary->num][maxlen-1] = '\0';
    return dictionary->num++;
}

int settings_create_offset(struct conf_service *service, const char *name)
{
    return settings_index_lookup(service->dictionary, name, service->nmem);
}

int settings_lookup_offset(struct conf_service *service, const char *name)
{
    return settings_index_lookup(service->dictionary, name, 0);
}

char *settings_name(struct conf_service *service, int offset)
{
    assert(offset < service->dictionary->num);
    return service->dictionary->dict[offset];
}


// Apply a session override to a database
void service_apply_setting(struct conf_service *service, char *setting, char *value)
{
    struct setting *new = nmem_malloc(service->nmem, sizeof(*new));
    int offset = settings_create_offset(service, setting);
    expand_settings_array(&service->settings->settings, &service->settings->num_settings, offset, service->nmem);
    new->precedence = 0;
    new->target = NULL;
    new->name = setting;
    new->value = value;
    new->next = service->settings->settings[offset];
    service->settings->settings[offset] = new;
}


static int isdir(const char *path)
{
    struct stat st;

    if (stat(path, &st) < 0)
    {
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "stat %s", path);
        exit(1);
    }
    return st.st_mode & S_IFDIR;
}

// Read settings from an XML file, calling handler function for each setting
int settings_read_node_x(xmlNode *n,
                         void *client_data,
                         void (*fun)(void *client_data,
                                     struct setting *set))
{
    int ret_val = 0; /* success */
    char *namea = (char *) xmlGetProp(n, (xmlChar *) "name");
    char *targeta = (char *) xmlGetProp(n, (xmlChar *) "target");
    char *valuea = (char *) xmlGetProp(n, (xmlChar *) "value");
    char *usera = (char *) xmlGetProp(n, (xmlChar *) "user");
    char *precedencea = (char *) xmlGetProp(n, (xmlChar *) "precedence");

    for (n = n->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp((const char *) n->name, "set"))
        {
            xmlNode *root = n->children;
            struct setting set;
            char *name = (char *) xmlGetProp(n, (xmlChar *) "name");
            char *target = (char *) xmlGetProp(n, (xmlChar *) "target");
            char *value = (char *) xmlGetProp(n, (xmlChar *) "value");
            char *user = (char *) xmlGetProp(n, (xmlChar *) "user");
            char *precedence = (char *) xmlGetProp(n, (xmlChar *) "precedence");
            xmlChar *buf_out = 0;

            set.next = 0;

            if (precedence)
                set.precedence = atoi((char *) precedence);
            else if (precedencea)
                set.precedence = atoi((char *) precedencea);
            else
                set.precedence = 0;

            set.target = target ? target : targeta;
            set.name = name ? name : namea;

            while (root && root->type != XML_ELEMENT_NODE)
                root = root->next;
            if (!root)
                set.value = value ? value : valuea;
            else
            {   /* xml document content for this setting */
                xmlDoc *doc = xmlNewDoc(BAD_CAST "1.0");
                if (!doc)
                {
                    if (set.name)
                        yaz_log(YLOG_WARN, "bad XML content for setting "
                                "name=%s", set.name);
                    else
                        yaz_log(YLOG_WARN, "bad XML content for setting");
                    ret_val = -1;
                }
                else
                {
                    int len_out;
                    xmlDocSetRootElement(doc, xmlCopyNode(root, 1));
                    xmlDocDumpMemory(doc, &buf_out, &len_out);
                    /* xmlDocDumpMemory 0-terminates */
                    set.value = (char *) buf_out;
                    xmlFreeDoc(doc);
                }
            }

            if (set.name && set.value && set.target)
                (*fun)(client_data, &set);
            else
            {
                if (set.name)
                    yaz_log(YLOG_WARN, "missing value and/or target for "
                            "setting name=%s", set.name);
                else
                    yaz_log(YLOG_WARN, "missing name/value/target for setting");
                ret_val = -1;
            }
            xmlFree(buf_out);
            xmlFree(name);
            xmlFree(precedence);
            xmlFree(value);
            xmlFree(user);
            xmlFree(target);
        }
        else
        {
            yaz_log(YLOG_WARN, "Unknown element %s in settings file",
                    (char*) n->name);
            ret_val = -1;
        }
    }
    xmlFree(namea);
    xmlFree(precedencea);
    xmlFree(valuea);
    xmlFree(usera);
    xmlFree(targeta);
    return ret_val;
}

static int read_settings_file(const char *path,
                              void *client_data,
                              void (*fun)(void *client_data,
                                          struct setting *set))
{
    xmlDoc *doc = xmlParseFile(path);
    xmlNode *n;
    int ret;

    if (!doc)
    {
        yaz_log(YLOG_FATAL, "Failed to parse %s", path);
        return -1;
    }
    n = xmlDocGetRootElement(doc);
    ret = settings_read_node_x(n, client_data, fun);

    xmlFreeDoc(doc);
    return ret;
}


// Recursively read files or directories, invoking a
// callback for each one
static int read_settings(const char *path,
                          void *client_data,
                          void (*fun)(void *client_data,
                                      struct setting *set))
{
    int ret = 0;
    DIR *d;
    struct dirent *de;
    char *dot;

    if (isdir(path))
    {
        if (!(d = opendir(path)))
        {
            yaz_log(YLOG_FATAL|YLOG_ERRNO, "%s", path);
            return -1;
        }
        while ((de = readdir(d)))
        {
            char tmp[1024];
            if (*de->d_name == '.' || !strcmp(de->d_name, "CVS"))
                continue;
            sprintf(tmp, "%s/%s", path, de->d_name);
            if (read_settings(tmp, client_data, fun))
                ret = -1;
        }
        closedir(d);
    }
    else if ((dot = strrchr(path, '.')) && !strcmp(dot + 1, "xml"))
        ret = read_settings_file(path, client_data, fun);
    return ret;
}

// Determines if a ZURL is a wildcard, and what kind
static int zurl_wildcard(const char *zurl)
{
    if (!zurl)
        return SETTING_WILDCARD_NO;
    if (*zurl == '*')
        return SETTING_WILDCARD_YES;
    else if (*(zurl + strlen(zurl) - 1) == '*')
        return SETTING_WILDCARD_DB;
    else
        return SETTING_WILDCARD_NO;
}

struct update_database_context {
    struct setting *set;
    struct conf_service *service;
};

void expand_settings_array(struct setting ***set_ar, int *num, int offset,
                           NMEM nmem)
{
    assert(offset >= 0);
    assert(*set_ar);
    if (offset >= *num)
    {
        int i, n_num = offset + 10;
        struct setting **n_ar = nmem_malloc(nmem, n_num * sizeof(*n_ar));
        for (i = 0; i < *num; i++)
            n_ar[i] = (*set_ar)[i];
        for (; i < n_num; i++)
            n_ar[i] = 0;
        *num = n_num;
        *set_ar = n_ar;
    }
}

void expand_settings_array2(struct settings_array *settings, int offset, NMEM nmem)
{
    assert(offset >= 0);
    assert(settings);
    if (offset >= settings->num_settings)
    {
        int i, n_num = offset + 10;
        struct setting **n_ar = nmem_malloc(nmem, n_num * sizeof(*n_ar));
        for (i = 0; i < settings->num_settings; i++)
            n_ar[i] = settings->settings[i];
        for (; i < n_num; i++)
            n_ar[i] = 0;
        settings->num_settings = n_num;
        settings->settings = n_ar;
    }
}

static void update_settings(struct setting *set, struct settings_array *settings, int offset, NMEM nmem)
{
    struct setting **sp;
    yaz_log(YLOG_DEBUG, "update service settings offset %d with %s=%s", offset, set->name, set->value);
    expand_settings_array2(settings, offset, nmem);

    // First we determine if this setting is overriding any existing settings
    // with the same name.
    assert(offset < settings->num_settings);
    for (sp = &settings->settings[offset]; *sp; )
        if (!strcmp((*sp)->name, set->name))
        {
            if ((*sp)->precedence < set->precedence)
            {
                // We discard the value (nmem keeps track of the space)
                *sp = (*sp)->next; // unlink value from existing setting
            }
            else if ((*sp)->precedence > set->precedence)
            {
                // Db contains a higher-priority setting. Abort search
                break;
            }
            else if (zurl_wildcard((*sp)->target) > zurl_wildcard(set->target))
            {
                // target-specific value trumps wildcard. Delete.
                *sp = (*sp)->next; // unlink.....
            }
            else if (zurl_wildcard((*sp)->target) < zurl_wildcard(set->target))
                // Db already contains higher-priority setting. Abort search
                break;
            else
                sp = &(*sp)->next;
        }
        else
            sp = &(*sp)->next;
    if (!*sp) // is null when there are no higher-priority settings, so we add one
    {
        struct setting *new = nmem_malloc(nmem, sizeof(*new));
        memset(new, 0, sizeof(*new));
        new->precedence = set->precedence;
        new->target = nmem_strdup_null(nmem, set->target);
        new->name = nmem_strdup_null(nmem, set->name);
        new->value = nmem_strdup_null(nmem, set->value);
        new->next = settings->settings[offset];
        settings->settings[offset] = new;
    }
}


// This is called from grep_databases -- adds/overrides setting for a target
// This is also where the rules for precedence of settings are implemented
static void update_database_fun(void *context, struct database *db)
{
    struct setting *set = ((struct update_database_context *)
                           context)->set;
    struct conf_service *service = ((struct update_database_context *)
                                    context)->service;
    struct setting **sp;
    int offset;

    // Is this the right database?
    if (!match_zurl(db->id, set->target))
        return;

    offset = settings_create_offset(service, set->name);
    expand_settings_array(&db->settings, &db->num_settings, offset, service->nmem);

    // First we determine if this setting is overriding  any existing settings
    // with the same name.
    assert(offset < db->num_settings);
    for (sp = &db->settings[offset]; *sp; )
        if (!strcmp((*sp)->name, set->name))
        {
            if ((*sp)->precedence < set->precedence)
            {
                // We discard the value (nmem keeps track of the space)
                *sp = (*sp)->next; // unlink value from existing setting
            }
            else if ((*sp)->precedence > set->precedence)
            {
                // Db contains a higher-priority setting. Abort search
                break;
            }
            else if (zurl_wildcard((*sp)->target) > zurl_wildcard(set->target))
            {
                // target-specific value trumps wildcard. Delete.
                *sp = (*sp)->next; // unlink.....
            }
            else if (zurl_wildcard((*sp)->target) < zurl_wildcard(set->target))
                // Db already contains higher-priority setting. Abort search
                break;
            else
                sp = &(*sp)->next;
        }
        else
            sp = &(*sp)->next;
    if (!*sp) // is null when there are no higher-priority settings, so we add one
    {
        struct setting *new = nmem_malloc(service->nmem, sizeof(*new));

        memset(new, 0, sizeof(*new));
        new->precedence = set->precedence;
        new->target = nmem_strdup(service->nmem, set->target);
        new->name = nmem_strdup(service->nmem, set->name);
        new->value = nmem_strdup(service->nmem, set->value);
        new->next = db->settings[offset];
        db->settings[offset] = new;
    }
}

// Callback -- updates database records with dictionary entries as appropriate
// This is used in pass 2 to assign name/value pairs to databases
static void update_databases(void *client_data, struct setting *set)
{
    struct conf_service *service = (struct conf_service *) client_data;
    struct update_database_context context;
    context.set = set;
    context.service = service;
    predef_grep_databases(&context, service, update_database_fun);
}

// This simply copies the 'hard' (application-specific) settings
// to the settings dictionary.
static void initialize_hard_settings(struct conf_service *service)
{
    struct setting_dictionary *dict = service->dictionary;
    dict->dict = nmem_malloc(service->nmem, sizeof(hard_settings) - sizeof(char*));
    dict->size = (sizeof(hard_settings) - sizeof(char*)) / sizeof(char*);
    memcpy(dict->dict, hard_settings, dict->size * sizeof(char*));
    dict->num = dict->size;
}

// Read any settings names introduced in service definition (config) and add to dictionary
// This is done now to avoid errors if user settings are declared in session overrides
void initialize_soft_settings(struct conf_service *service)
{
    int i;
    for (i = 0; i < service->num_metadata; i++)
    {
        struct conf_metadata *md = &service->metadata[i];

        if (md->setting != Metadata_setting_no)
            settings_create_offset(service, md->name);

        // Also create setting for some metadata attributes.
        if (md->limitmap) {
            int index;
            WRBUF wrbuf = wrbuf_alloc();
            yaz_log(YLOG_DEBUG, "Metadata %s has limitmap: %s ",md->name,  md->limitmap);
            wrbuf_printf(wrbuf, "pz:limitmap:%s", md->name);
            index = settings_create_offset(service, wrbuf_cstr(wrbuf));
            if (index >= 0) {
                struct setting new;
                int offset;
                yaz_log(YLOG_DEBUG, "Service %s default %s=%s",
                        (service->id ? service->id: "unknown"), wrbuf_cstr(wrbuf), md->limitmap);
                new.name = (char *) wrbuf_cstr(wrbuf);
                new.value = md->limitmap;
                new.next = 0;
                new.target = 0;
                new.precedence = 0;
                offset = settings_create_offset(service, new.name);
                update_settings(&new, service->settings, offset, service->nmem);
            }
            wrbuf_destroy(wrbuf);
        // TODO same for facetmap
        }
    }
}

static void prepare_target_dictionary(void *client_data, struct setting *set)
{
    struct conf_service *service = (struct conf_service *) client_data;

    // If target address is not wildcard, add the database
    if (*set->target && !zurl_wildcard(set->target))
        create_database_for_service(set->target, service);
}

void init_settings(struct conf_service *service)
{
    struct setting_dictionary *new;

    assert(service->nmem);

    new = nmem_malloc(service->nmem, sizeof(*new));
    memset(new, 0, sizeof(*new));
    service->dictionary = new;
    initialize_hard_settings(service);
    initialize_soft_settings(service);
}

int settings_read_file(struct conf_service *service, const char *path,
                       int pass)
{
    if (pass == 1)
        return read_settings(path, service, prepare_target_dictionary);
    else
        return read_settings(path, service, update_databases);
}

int settings_read_node(struct conf_service *service, xmlNode *n,
                        int pass)
{
    if (pass == 1)
        return settings_read_node_x(n, service, prepare_target_dictionary);
    else
        return settings_read_node_x(n, service, update_databases);
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

