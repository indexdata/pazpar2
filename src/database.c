/* This file is part of Pazpar2.
   Copyright (C) 2006-2009 Index Data

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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "pazpar2.h"
#include "host.h"
#include "settings.h"
#include "http.h"
#include "zeerex.h"

#include <sys/types.h>
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

static struct host *hosts = 0;  // The hosts we know about 
static struct database *databases = 0; // The databases we know about
static NMEM nmem = 0;

static xmlDoc *get_explain_xml(const char *id)
{
    struct stat st;
    char *dir;
    char path[256];
    char ide[256];
    if (!config || !config->targetprofiles)
    {
        yaz_log(YLOG_WARN, "Config must be loaded and specify targetprofiles");
        return 0;
    }
    if (config->targetprofiles->type != Targetprofiles_local)
    {
        yaz_log(YLOG_FATAL, "Only supports local type");
        return 0;
    }
    dir = config->targetprofiles->src;
    urlencode(id, ide);
    sprintf(path, "%s/%s", dir, ide);
    if (!stat(path, &st))
        return xmlParseFile(path);
    else
        return 0;
}

// Create a new host structure for hostport
static struct host *create_host(const char *hostport)
{
    struct host *host;

    host = xmalloc(sizeof(struct host));
    host->hostport = xstrdup(hostport);
    host->connections = 0;
    host->ipport = 0;

    if (host_getaddrinfo(host))
    {
        xfree(host->hostport);
        xfree(host);
        return 0;
    }
    host->next = hosts;
    hosts = host;
    return host;
}

static struct host *find_host(const char *hostport)
{
    struct host *p;
    for (p = hosts; p; p = p->next)
        if (!strcmp(p->hostport, hostport))
            return p;
    return create_host(hostport);
}

static struct database *load_database(const char *id)
{
    xmlDoc *doc = 0;
    struct zr_explain *explain = 0;
    struct database *db;
    struct host *host;
    char hostport[256];
    char *dbname;
    struct setting *idset;

    yaz_log(YLOG_LOG, "New database: %s", id);
    if (!nmem)
        nmem = nmem_create();

    if (config && config->targetprofiles 
        && (doc = get_explain_xml(id)))
    {
        explain = zr_read_xml(nmem, xmlDocGetRootElement(doc));
        if (!explain)
            return 0;
    }

    if (strlen(id) > 255)
        return 0;
    strcpy(hostport, id);
    if ((dbname = strchr(hostport, '/')))
        *(dbname++) = '\0';
    else
        dbname = "";
    if (!(host = find_host(hostport)))
        return 0;
    db = nmem_malloc(nmem, sizeof(*db));
    memset(db, 0, sizeof(*db));
    db->host = host;
    db->url = nmem_strdup(nmem, id);
    db->databases = xmalloc(2 * sizeof(char *));
    db->databases[0] = nmem_strdup(nmem, dbname);
    db->databases[1] = 0;
    db->errors = 0;
    db->explain = explain;

    db->settings = 0;

    db->settings = nmem_malloc(nmem, sizeof(struct settings*) * settings_num());
    memset(db->settings, 0, sizeof(struct settings*) * settings_num());
    idset = nmem_malloc(nmem, sizeof(*idset));
    idset->precedence = 0;
    idset->name = "pz:id";
    idset->target = idset->value = db->url;
    idset->next = 0;
    db->settings[PZ_ID] = idset;

    db->next = databases;
    databases = db;

    return db;
}

// Return a database structure by ID. Load and add to list if necessary
// new==1 just means we know it's not in the list
struct database *find_database(const char *id, int new)
{
    struct database *p;
    if (!new)
    {
        for (p = databases; p; p = p->next)
            if (!strcmp(p->url, id))
                return p;
    }
    return load_database(id);
}

// This whole session_grep database thing should be moved elsewhere

int match_zurl(const char *zurl, const char *pattern)
{
    int len;

    if (!strcmp(pattern, "*"))
        return 1;
    else if (!strncmp(pattern, "*/", 2))   // host wildcard.. what the heck is that for?
    {
        char *db = strchr(zurl, '/');
        if (!db)
            return 0;
        if (!strcmp(pattern + 2, db))
            return 1;
        else
            return 0;
    }
    else if (*(pattern + (len = strlen(pattern) - 1)) == '*')  // db wildcard
    {
        if (!strncmp(pattern, zurl, len))
            return 1;
        else
            return 2;
    }
    else if (!strcmp(pattern, zurl))
        return 1;
    else
        return 0;
}

// This will be generalized at some point
static int match_criterion(struct setting **settings, struct database_criterion *c)
{
    int offset = settings_offset(c->name);
    struct database_criterion_value *v;

    if (offset < 0)
    {
        yaz_log(YLOG_WARN, "Criterion not found: %s", c->name);
        return 0;
    }
    if (!settings[offset])
        return 0;
    for (v = c->values; v; v = v->next)
    {
        if (offset == PZ_ID)
        {
            if (match_zurl(settings[offset]->value, v->value))
                break;
        }
        else 
        {
            if (!strcmp(settings[offset]->value, v->value))
                break;
        }
    }
    if (v)
        return 1;
    else
        return 0;
}

int database_match_criteria(struct setting **settings, struct database_criterion *cl)
{
    for (; cl; cl = cl->next)
        if (!match_criterion(settings, cl))
            break;
    if (cl) // one of the criteria failed to match -- skip this db
        return 0;
    else
        return 1;
}

// Cycles through databases, calling a handler function on the ones for
// which all criteria matched.
int session_grep_databases(struct session *se, struct database_criterion *cl,
        void (*fun)(void *context, struct session_database *db))
{
    struct session_database *p;
    int i = 0;

    for (p = se->databases; p; p = p->next)
    {
        if (p->settings && p->settings[PZ_ALLOW] && *p->settings[PZ_ALLOW]->value == '0')
            continue;
        if (!p->settings[PZ_NAME])
            continue;
        if (database_match_criteria(p->settings, cl))
        {
            (*fun)(se, p);
            i++;
        }
    }
    return i;
}

int predef_grep_databases(void *context, struct database_criterion *cl,
                          void (*fun)(void *context, struct database *db))
{
    struct database *p;
    int i = 0;

    for (p = databases; p; p = p->next)
        if (database_match_criteria(p->settings, cl))
        {
            (*fun)(context, p);
            i++;
        }
    return i;
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

