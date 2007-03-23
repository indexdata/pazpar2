/* $Id: database.c,v 1.4 2007-03-23 03:26:22 quinn Exp $ */

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxslt/xslt.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>
#include <assert.h>

#include "pazpar2.h"
#include "config.h"
#include "http.h"
#include "zeerex.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

static struct host *hosts = 0;  // The hosts we know about 
static struct database *databases = 0; // The databases we know about
static NMEM nmem = 0;

// This needs to be extended with selection criteria
static struct conf_retrievalprofile *database_retrievalprofile(const char *id)
{
    if (!config)
    {
        yaz_log(YLOG_FATAL, "Must load configuration (-f)");
        exit(1);
    }
    if (!config->retrievalprofiles)
    {
        yaz_log(YLOG_FATAL, "No retrieval profiles defined");
    }
    return config->retrievalprofiles;
}

static struct conf_queryprofile *database_queryprofile(const char *id)
{
    return (struct conf_queryprofile*) 1;
}

static xmlDoc *get_explain_xml(const char *id)
{
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
    yaz_log(YLOG_LOG, "Path: %s", path);
    return xmlParseFile(path);
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
    xmlDoc *doc = get_explain_xml(id);
    struct zr_explain *explain;
    struct conf_retrievalprofile *retrieval;
    struct conf_queryprofile *query;
    struct database *db;
    struct host *host;
    char hostport[256];
    char *dbname;

    if (!nmem)
        nmem = nmem_create();
    if (doc)
    {
        explain = zr_read_xml(nmem, xmlDocGetRootElement(doc));
        if (!explain)
            return 0;
    }
    if (!(retrieval = database_retrievalprofile(id)) ||
            !(query = database_queryprofile(id)))
    {
        xmlFree(doc);
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
    db->name = 0;
    db->databases = xmalloc(2 * sizeof(char *));
    db->databases[0] = nmem_strdup(nmem, dbname);
    db->databases[1] = 0;
    db->errors = 0;
    db->explain = explain;
    db->qprofile = query;
    db->rprofile = retrieval;
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

// This will be generalized at some point
static int match_criterion(struct database *db, struct database_criterion *c)
{
    if (!strcmp(c->name, "id"))
    {
        struct database_criterion_value *v;
        for (v = c->values; v; v = v->next)
            if (!strcmp(v->value, db->url))
                return 1;
        return 0;
    }
    else
        return 0;
}

int database_match_criteria(struct database *db, struct database_criterion *cl)
{
    for (; cl; cl = cl->next)
        if (!match_criterion(db, cl))
            break;
    if (cl) // one of the criteria failed to match -- skip this db
        return 0;
    else
        return 1;
}

// Cycles through databases, calling a handler function on the ones for
// which all criteria matched.
int grep_databases(void *context, struct database_criterion *cl,
        void (*fun)(void *context, struct database *db))
{
    struct database *p;
    int i;

    for (p = databases; p; p = p->next)
    {
        if (database_match_criteria(p, cl))
        {
            (*fun)(context, p);
            i++;
        }
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
        if (name && db)
            db->name = nmem_strdup(nmem, name);
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
