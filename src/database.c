/* $Id: database.c,v 1.22 2007-04-20 16:21:19 quinn Exp $
   Copyright (c) 2006-2007, Index Data.

This file is part of Pazpar2.

Pazpar2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

Pazpar2 is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with Pazpar2; see the file LICENSE.  If not, write to the
Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.
 */

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxslt/xslt.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "pazpar2.h"
#include "config.h"
#include "settings.h"
#include "http.h"
#include "zeerex.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

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
    struct addrinfo *addrinfo, hints;
    struct host *host;
    char *port;
    char ipport[128];
    unsigned char addrbuf[4];
    int res;

    host = xmalloc(sizeof(struct host));
    host->hostport = xstrdup(hostport);
    host->connections = 0;

    if ((port = strchr(hostport, ':')))
        *(port++) = '\0';
    else
        port = "210";

    hints.ai_flags = 0;
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_addrlen = 0;
    hints.ai_addr = 0;
    hints.ai_canonname = 0;
    hints.ai_next = 0;
    // This is not robust code. It assumes that getaddrinfo always
    // returns AF_INET address.
    if ((res = getaddrinfo(hostport, port, &hints, &addrinfo)))
    {
        yaz_log(YLOG_WARN, "Failed to resolve %s: %s", hostport, gai_strerror(res));
        xfree(host->hostport);
        xfree(host);
        return 0;
    }
    assert(addrinfo->ai_family == PF_INET);
    memcpy(addrbuf, &((struct sockaddr_in*)addrinfo->ai_addr)->sin_addr.s_addr, 4);
    sprintf(ipport, "%u.%u.%u.%u:%s",
            addrbuf[0], addrbuf[1], addrbuf[2], addrbuf[3], port);
    host->ipport = xstrdup(ipport);
    freeaddrinfo(addrinfo);
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
        dbname = "Default";
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

// This whole session_grep database thing should be moved to pazpar2.c

int match_zurl(const char *zurl, const char *pattern)
{
    if (!strcmp(pattern, "*"))
        return 1;
    else if (!strncmp(pattern, "*/", 2))
    {
        char *db = strchr(zurl, '/');
        if (!db)
            return 0;
        if (!strcmp(pattern + 2, db))
            return 1;
        else
            return 0;
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
        if (database_match_criteria(p->settings, cl))
        {
            (*fun)(se, p);
            i++;
        }
    }
    return i;
}

int grep_databases(void *context, struct database_criterion *cl,
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

// This function will most likely vanish when a proper target profile mechanism is
// introduced.
void load_simpletargets(const char *fn)
{
    FILE *f = fopen(fn, "r");
    char line[256];

    if (!f)
    {
        yaz_log(YLOG_WARN|YLOG_ERRNO, "open %s", fn);
        exit(1);
    }

    while (fgets(line, 255, f))
    {
        char *url;
        char *name;
        struct database *db;

        if (strncmp(line, "target ", 7))
            continue;
        line[strlen(line) - 1] = '\0';

        if ((name = strchr(line, ';')))
            *(name++) = '\0';

        url = line + 7;

        if (!(db = find_database(url, 0)))
            yaz_log(YLOG_WARN, "Unable to load database %s", url);
    }
    fclose(f);
}


/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
