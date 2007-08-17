/* $Id: logic.c,v 1.62 2007-08-17 12:39:11 adam Exp $
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

/** \file logic.c
    \brief high-level logic; mostly user sessions and settings
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <ctype.h>
#include <assert.h>

#include <yaz/marcdisp.h>
#include <yaz/comstack.h>
#include <yaz/tcpip.h>
#include <yaz/proto.h>
#include <yaz/readconf.h>
#include <yaz/pquery.h>
#include <yaz/otherinfo.h>
#include <yaz/yaz-util.h>
#include <yaz/nmem.h>
#include <yaz/query-charset.h>
#include <yaz/querytowrbuf.h>
#include <yaz/oid_db.h>
#include <yaz/snprintf.h>

#if HAVE_CONFIG_H
#include "cconfig.h"
#endif

#define USE_TIMING 0
#if USE_TIMING
#include <yaz/timing.h>
#endif

#include <netinet/in.h>

#include "pazpar2.h"
#include "eventl.h"
#include "http.h"
#include "termlists.h"
#include "reclists.h"
#include "relevance.h"
#include "config.h"
#include "database.h"
#include "client.h"
#include "settings.h"
#include "normalize7bit.h"

#define TERMLIST_HIGH_SCORE 25

#define MAX_CHUNK 15

// Note: Some things in this structure will eventually move to configuration
struct parameters global_parameters = 
{
    "",
    "",
    "", 
    0,
    0, /* dump_records */
    0, /* debug_mode */
    30,
    "81",
    "Index Data PazPar2",
    VERSION,
    600, // 10 minutes
    60,
    100,
    MAX_CHUNK,
    0,
    0,
    180,
    30
};

// Recursively traverse query structure to extract terms.
void pull_terms(NMEM nmem, struct ccl_rpn_node *n, char **termlist, int *num)
{
    char **words;
    int numwords;
    int i;

    switch (n->kind)
    {
        case CCL_RPN_AND:
        case CCL_RPN_OR:
        case CCL_RPN_NOT:
        case CCL_RPN_PROX:
            pull_terms(nmem, n->u.p[0], termlist, num);
            pull_terms(nmem, n->u.p[1], termlist, num);
            break;
        case CCL_RPN_TERM:
            nmem_strsplit(nmem, " ", n->u.t.term, &words, &numwords);
            for (i = 0; i < numwords; i++)
                termlist[(*num)++] = words[i];
            break;
        default: // NOOP
            break;
    }
}



static void add_facet(struct session *s, const char *type, const char *value)
{
    int i;

    if (!*value)
        return;
    for (i = 0; i < s->num_termlists; i++)
        if (!strcmp(s->termlists[i].name, type))
            break;
    if (i == s->num_termlists)
    {
        if (i == SESSION_MAX_TERMLISTS)
        {
            yaz_log(YLOG_FATAL, "Too many termlists");
            return;
        }

        s->termlists[i].name = nmem_strdup(s->nmem, type);
        s->termlists[i].termlist 
            = termlist_create(s->nmem, s->expected_maxrecs,
                              TERMLIST_HIGH_SCORE);
        s->num_termlists = i + 1;
    }
    termlist_insert(s->termlists[i].termlist, value);
}

xmlDoc *record_to_xml(struct session_database *sdb, Z_External *rec)
{
    struct database *db = sdb->database;
    xmlDoc *rdoc = 0;
    const Odr_oid *oid = rec->direct_reference;

    /* convert response record to XML somehow */
    if (rec->which == Z_External_octet && oid
        && !oid_oidcmp(oid, yaz_oid_recsyn_xml))
    {
        /* xml already */
        rdoc = xmlParseMemory((char*) rec->u.octet_aligned->buf,
                              rec->u.octet_aligned->len);
        if (!rdoc)
        {
            yaz_log(YLOG_FATAL, "Non-wellformed XML received from %s",
                    db->url);
            return 0;
        }
    }
    else if (rec->which == Z_External_OPAC)
    {
        if (!sdb->yaz_marc)
        {
            yaz_log(YLOG_WARN, "MARC decoding not configured");
            return 0;
        }
        else
        {
            /* OPAC gets converted to XML too */
            WRBUF wrbuf_opac = wrbuf_alloc();
            /* MARCXML inside the OPAC XML. Charset is in effect because we
               use the yaz_marc handle */
            yaz_marc_xml(sdb->yaz_marc, YAZ_MARC_MARCXML);
            yaz_opac_decode_wrbuf(sdb->yaz_marc, rec->u.opac, wrbuf_opac);
            
            rdoc = xmlParseMemory((char*) wrbuf_buf(wrbuf_opac),
                                  wrbuf_len(wrbuf_opac));
            if (!rdoc)
            {
                yaz_log(YLOG_WARN, "Unable to parse OPAC XML");
                /* Was used to debug bug #1348 */
#if 0
                FILE *f = fopen("/tmp/opac.xml.txt", "wb");
                if (f)
                {
                    fwrite(wrbuf_buf(wrbuf_opac), 1, wrbuf_len(wrbuf_opac), f);
                    fclose(f);
                }
#endif
            }
            wrbuf_destroy(wrbuf_opac);
        }
    }
    else if (oid && yaz_oid_is_iso2709(oid))
    {
        /* ISO2709 gets converted to MARCXML */
        if (!sdb->yaz_marc)
        {
            yaz_log(YLOG_WARN, "MARC decoding not configured");
            return 0;
        }
        else
        {
            xmlNode *res;
            char *buf;
            int len;
            
            if (rec->which != Z_External_octet)
            {
                yaz_log(YLOG_WARN, "Unexpected external branch, probably BER %s",
                        db->url);
                return 0;
            }
            buf = (char*) rec->u.octet_aligned->buf;
            len = rec->u.octet_aligned->len;
            if (yaz_marc_read_iso2709(sdb->yaz_marc, buf, len) < 0)
            {
                yaz_log(YLOG_WARN, "Failed to decode MARC %s", db->url);
                return 0;
            }
            
            if (yaz_marc_write_xml(sdb->yaz_marc, &res,
                                   "http://www.loc.gov/MARC21/slim", 0, 0) < 0)
            {
                yaz_log(YLOG_WARN, "Failed to encode as XML %s",
                        db->url);
                return 0;
            }
            rdoc = xmlNewDoc((xmlChar *) "1.0");
            xmlDocSetRootElement(rdoc, res);
        }
    }
    else
    {
        char oid_name_buf[OID_STR_MAX];
        const char *oid_name = yaz_oid_to_string_buf(oid, 0, oid_name_buf);
        yaz_log(YLOG_FATAL, 
                "Unable to handle record of type %s from %s", 
                oid_name, db->url);
        return 0;
    }
    
    if (global_parameters.dump_records)
    {
        FILE *lf = yaz_log_file();
        if (lf)
        {
            yaz_log(YLOG_LOG, "Un-normalized record from %s", db->url);
#if LIBXML_VERSION >= 20600
            xmlDocFormatDump(lf, rdoc, 1);
#else
            xmlDocDump(lf, rdoc);
#endif
            fprintf(lf, "\n");
        }
    }
    return rdoc;
}

#define MAX_XSLT_ARGS 16

// Add static values from session database settings if applicable
static void insert_settings_parameters(struct session_database *sdb,
        struct session *se, char **parms)
{
    struct conf_service *service = global_parameters.server->service;
    int i;
    int nparms = 0;
    int offset = 0;

    for (i = 0; i < service->num_metadata; i++)
    {
        struct conf_metadata *md = &service->metadata[i];
        int setting;

        if (md->setting == Metadata_setting_parameter &&
                (setting = settings_offset(md->name)) > 0)
        {
            const char *val = session_setting_oneval(sdb, setting);
            if (val && nparms < MAX_XSLT_ARGS)
            {
                char *buf;
                int len = strlen(val);
                buf = nmem_malloc(se->nmem, len + 3);
                buf[0] = '\'';
                strcpy(buf + 1, val);
                buf[len+1] = '\'';
                buf[len+2] = '\0';
                parms[offset++] = md->name;
                parms[offset++] = buf;
                nparms++;
            }
        }
    }
    parms[offset] = 0;
}

// Add static values from session database settings if applicable
static void insert_settings_values(struct session_database *sdb, xmlDoc *doc)
{
    struct conf_service *service = global_parameters.server->service;
    int i;

    for (i = 0; i < service->num_metadata; i++)
    {
        struct conf_metadata *md = &service->metadata[i];
        int offset;

        if (md->setting == Metadata_setting_postproc &&
                (offset = settings_offset(md->name)) > 0)
        {
            const char *val = session_setting_oneval(sdb, offset);
            if (val)
            {
                xmlNode *r = xmlDocGetRootElement(doc);
                xmlNode *n = xmlNewTextChild(r, 0, (xmlChar *) "metadata",
                        (xmlChar *) val);
                xmlSetProp(n, (xmlChar *) "type", (xmlChar *) md->name);
            }
        }
    }
}

xmlDoc *normalize_record(struct session_database *sdb, struct session *se,
        Z_External *rec)
{
    struct database_retrievalmap *m;
    xmlDoc *rdoc = record_to_xml(sdb, rec);
    if (rdoc)
    {
        for (m = sdb->map; m; m = m->next)
        {
            xmlDoc *new = 0;
            
            {
                xmlNodePtr root = 0;
                char *parms[MAX_XSLT_ARGS*2+1];

                insert_settings_parameters(sdb, se, parms);

                new = xsltApplyStylesheet(m->stylesheet, rdoc, (const char **) parms);
                root= xmlDocGetRootElement(new);
                if (!new || !root || !(root->children))
                {
                    yaz_log(YLOG_WARN, "XSLT transformation failed from %s",
                            sdb->database->url);
                    xmlFreeDoc(new);
                    xmlFreeDoc(rdoc);
                    return 0;
                }
            }
            
            xmlFreeDoc(rdoc);
            rdoc = new;
        }

        insert_settings_values(sdb, rdoc);

        if (global_parameters.dump_records)
        {
            FILE *lf = yaz_log_file();
            
            if (lf)
            {
                yaz_log(YLOG_LOG, "Normalized record from %s", 
                        sdb->database->url);
#if LIBXML_VERSION >= 20600
                xmlDocFormatDump(lf, rdoc, 1);
#else
                xmlDocDump(lf, rdoc);
#endif
                fprintf(lf, "\n");
            }
        }
    }
    return rdoc;
}

// Retrieve first defined value for 'name' for given database.
// Will be extended to take into account user associated with session
const char *session_setting_oneval(struct session_database *db, int offset)
{
    if (!db->settings[offset])
        return "";
    return db->settings[offset]->value;
}



// Initialize YAZ Map structures for MARC-based targets
static int prepare_yazmarc(struct session_database *sdb)
{
    const char *s;

    if (!sdb->settings)
    {
        yaz_log(YLOG_WARN, "No settings for %s", sdb->database->url);
        return -1;
    }
    if ((s = session_setting_oneval(sdb, PZ_NATIVESYNTAX)) 
        && !strncmp(s, "iso2709", 7))
    {
        char *encoding = "marc-8s", *e;
        yaz_iconv_t cm;

        // See if a native encoding is specified
        if ((e = strchr(s, ';')))
            encoding = e + 1;

        sdb->yaz_marc = yaz_marc_create();
        yaz_marc_subfield_str(sdb->yaz_marc, "\t");
        
        cm = yaz_iconv_open("utf-8", encoding);
        if (!cm)
        {
            yaz_log(YLOG_FATAL, 
                    "Unable to map from %s to UTF-8 for target %s", 
                    encoding, sdb->database->url);
            return -1;
        }
        yaz_marc_iconv(sdb->yaz_marc, cm);
    }
    return 0;
}

// Prepare XSLT stylesheets for record normalization
// Structures are allocated on the session_wide nmem to avoid having
// to recompute this for every search. This would lead
// to leaking if a single session was to repeatedly change the PZ_XSLT
// setting. However, this is not a realistic use scenario.
static int prepare_map(struct session *se, struct session_database *sdb)
{
    const char *s;

    if (!sdb->settings)
    {
        yaz_log(YLOG_WARN, "No settings on %s", sdb->database->url);
        return -1;
    }
    if ((s = session_setting_oneval(sdb, PZ_XSLT)))
    {
        char **stylesheets;
        struct database_retrievalmap **m = &sdb->map;
        int num, i;
        char auto_stylesheet[256];

        if (!strcmp(s, "auto"))
        {
            const char *request_syntax = session_setting_oneval(
                sdb, PZ_REQUESTSYNTAX);
            if (request_syntax)
            {
                char *cp;
                yaz_snprintf(auto_stylesheet, sizeof(auto_stylesheet),
                             "%s.xsl", request_syntax);
                for (cp = auto_stylesheet; *cp; cp++)
                {
                    /* deliberately only consider ASCII */
                    if (*cp > 32 && *cp < 127)
                        *cp = tolower(*cp);
                }
                s = auto_stylesheet;
            }
            else
            {
                yaz_log(YLOG_WARN, "No pz:requestsyntax for auto stylesheet");
            }
        }
        nmem_strsplit(se->session_nmem, ",", s, &stylesheets, &num);
        for (i = 0; i < num; i++)
        {
            (*m) = nmem_malloc(se->session_nmem, sizeof(**m));
            (*m)->next = 0;
            if (!((*m)->stylesheet = conf_load_stylesheet(stylesheets[i])))
            {
                yaz_log(YLOG_FATAL|YLOG_ERRNO, "Unable to load stylesheet: %s",
                        stylesheets[i]);
                return -1;
            }
            m = &(*m)->next;
        }
    }
    if (!sdb->map)
        yaz_log(YLOG_WARN, "No Normalization stylesheet for target %s",
                sdb->database->url);
    return 0;
}

// This analyzes settings and recomputes any supporting data structures
// if necessary.
static int prepare_session_database(struct session *se, 
                                    struct session_database *sdb)
{
    if (!sdb->settings)
    {
        yaz_log(YLOG_WARN, 
                "No settings associated with %s", sdb->database->url);
        return -1;
    }
    if (sdb->settings[PZ_NATIVESYNTAX] && !sdb->yaz_marc)
    {
        if (prepare_yazmarc(sdb) < 0)
            return -1;
    }
    if (sdb->settings[PZ_XSLT] && !sdb->map)
    {
        if (prepare_map(se, sdb) < 0)
            return -1;
    }
    return 0;
}

// called if watch should be removed because http_channel is to be destroyed
static void session_watch_cancel(void *data, struct http_channel *c)
{
    struct session_watchentry *ent = data;

    ent->fun = 0;
    ent->data = 0;
    ent->obs = 0;
}

// set watch. Returns 0=OK, -1 if watch is already set
int session_set_watch(struct session *s, int what, 
                      session_watchfun fun, void *data,
                      struct http_channel *chan)
{
    if (s->watchlist[what].fun)
        return -1;
    s->watchlist[what].fun = fun;
    s->watchlist[what].data = data;
    s->watchlist[what].obs = http_add_observer(chan, &s->watchlist[what],
                                               session_watch_cancel);
    return 0;
}

void session_alert_watch(struct session *s, int what)
{
    if (!s->watchlist[what].fun)
        return;
    http_remove_observer(s->watchlist[what].obs);
    (*s->watchlist[what].fun)(s->watchlist[what].data);
    s->watchlist[what].fun = 0;
    s->watchlist[what].data = 0;
    s->watchlist[what].obs = 0;
}

//callback for grep_databases
static void select_targets_callback(void *context, struct session_database *db)
{
    struct session *se = (struct session*) context;
    struct client *cl = client_create();
    client_set_database(cl, db);
    client_set_session(cl, se);
}

// Associates a set of clients with a session;
// Note: Session-databases represent databases with per-session 
// setting overrides
int select_targets(struct session *se, struct database_criterion *crit)
{
    while (se->clients)
        client_destroy(se->clients);

    return session_grep_databases(se, crit, select_targets_callback);
}

int session_active_clients(struct session *s)
{
    struct client *c;
    int res = 0;

    for (c = s->clients; c; c = client_next_in_session(c))
        if (client_is_active(c))
            res++;

    return res;
}

// parses crit1=val1,crit2=val2|val3,...
static struct database_criterion *parse_filter(NMEM m, const char *buf)
{
    struct database_criterion *res = 0;
    char **values;
    int num;
    int i;

    if (!buf || !*buf)
        return 0;
    nmem_strsplit(m, ",", buf,  &values, &num);
    for (i = 0; i < num; i++)
    {
        char **subvalues;
        int subnum;
        int subi;
        struct database_criterion *new = nmem_malloc(m, sizeof(*new));
        char *eq = strchr(values[i], '=');
        if (!eq)
        {
            yaz_log(YLOG_WARN, "Missing equal-sign in filter");
            return 0;
        }
        *(eq++) = '\0';
        new->name = values[i];
        nmem_strsplit(m, "|", eq, &subvalues, &subnum);
        new->values = 0;
        for (subi = 0; subi < subnum; subi++)
        {
            struct database_criterion_value *newv
                = nmem_malloc(m, sizeof(*newv));
            newv->value = subvalues[subi];
            newv->next = new->values;
            new->values = newv;
        }
        new->next = res;
        res = new;
    }
    return res;
}

enum pazpar2_error_code search(struct session *se,
                               char *query, char *filter,
                               const char **addinfo)
{
    int live_channels = 0;
    int no_working = 0;
    int no_failed = 0;
    struct client *cl;
    struct database_criterion *criteria;

    yaz_log(YLOG_DEBUG, "Search");

    *addinfo = 0;
    nmem_reset(se->nmem);
    se->relevance = 0;
    se->total_records = se->total_hits = se->total_merged = 0;
    se->reclist = 0;
    se->num_termlists = 0;
    criteria = parse_filter(se->nmem, filter);
    se->requestid++;
    live_channels = select_targets(se, criteria);
    if (live_channels)
    {
        int maxrecs = live_channels * global_parameters.toget;
        se->reclist = reclist_create(se->nmem, maxrecs);
        se->expected_maxrecs = maxrecs;
    }
    else
        return PAZPAR2_NO_TARGETS;

    for (cl = se->clients; cl; cl = client_next_in_session(cl))
    {
        if (prepare_session_database(se, client_get_database(cl)) < 0)
        {
            *addinfo = client_get_database(cl)->database->url;
            return PAZPAR2_CONFIG_TARGET;
        }
        // Parse query for target
        if (client_parse_query(cl, query) < 0)
            no_failed++;
        else
        {
            no_working++;
            client_prep_connection(cl);
        }
    }

    // If no queries could be mapped, we signal an error
    if (no_working == 0)
    {
        *addinfo = "query";
        return PAZPAR2_MALFORMED_PARAMETER_VALUE;
    }
    return PAZPAR2_NO_ERROR;
}

// Creates a new session_database object for a database
static void session_init_databases_fun(void *context, struct database *db)
{
    struct session *se = (struct session *) context;
    struct session_database *new = nmem_malloc(se->session_nmem, sizeof(*new));
    int num = settings_num();
    int i;

    new->database = db;
    new->yaz_marc = 0;
    
#ifdef HAVE_ICU
    if (global_parameters.server && global_parameters.server->icu_chn)
        new->pct = pp2_charset_create(global_parameters.server->icu_chn);
    else
        new->pct = pp2_charset_create(0);
#else // HAVE_ICU
    new->pct = pp2_charset_create(0);
#endif // HAVE_ICU

    new->map = 0;
    new->settings 
        = nmem_malloc(se->session_nmem, sizeof(struct settings *) * num);
    memset(new->settings, 0, sizeof(struct settings*) * num);

    if (db->settings)
    {
        for (i = 0; i < num; i++)
            new->settings[i] = db->settings[i];
    }
    new->next = se->databases;
    se->databases = new;
}

// Doesn't free memory associated with sdb -- nmem takes care of that
static void session_database_destroy(struct session_database *sdb)
{
    struct database_retrievalmap *m;

    for (m = sdb->map; m; m = m->next)
        xsltFreeStylesheet(m->stylesheet);
    if (sdb->yaz_marc)
        yaz_marc_destroy(sdb->yaz_marc);
    if (sdb->pct)
        pp2_charset_destroy(sdb->pct);
}

// Initialize session_database list -- this represents this session's view
// of the database list -- subject to modification by the settings ws command
void session_init_databases(struct session *se)
{
    se->databases = 0;
    predef_grep_databases(se, 0, session_init_databases_fun);
}

// Probably session_init_databases_fun should be refactored instead of
// called here.
static struct session_database *load_session_database(struct session *se, 
                                                      char *id)
{
    struct database *db = find_database(id, 0);

    session_init_databases_fun((void*) se, db);
    // New sdb is head of se->databases list
    return se->databases;
}

// Find an existing session database. If not found, load it
static struct session_database *find_session_database(struct session *se, 
                                                      char *id)
{
    struct session_database *sdb;

    for (sdb = se->databases; sdb; sdb = sdb->next)
        if (!strcmp(sdb->database->url, id))
            return sdb;
    return load_session_database(se, id);
}

// Apply a session override to a database
void session_apply_setting(struct session *se, char *dbname, char *setting,
                           char *value)
{
    struct session_database *sdb = find_session_database(se, dbname);
    struct setting *new = nmem_malloc(se->session_nmem, sizeof(*new));
    int offset = settings_offset_cprefix(setting);

    if (offset < 0)
    {
        yaz_log(YLOG_WARN, "Unknown setting %s", setting);
        return;
    }
    // Jakub: This breaks the filter setting.
    /*if (offset == PZ_ID)
    {
        yaz_log(YLOG_WARN, "No need to set pz:id setting. Ignoring");
        return;
    }*/
    new->precedence = 0;
    new->target = dbname;
    new->name = setting;
    new->value = value;
    new->next = sdb->settings[offset];
    sdb->settings[offset] = new;

    // Force later recompute of settings-driven data structures
    // (happens when a search starts and client connections are prepared)
    switch (offset)
    {
        case PZ_NATIVESYNTAX:
            if (sdb->yaz_marc)
            {
                yaz_marc_destroy(sdb->yaz_marc);
                sdb->yaz_marc = 0;
            }
            break;
        case PZ_XSLT:
            if (sdb->map)
            {
                struct database_retrievalmap *m;
                // We don't worry about the map structure -- it's in nmem
                for (m = sdb->map; m; m = m->next)
                    xsltFreeStylesheet(m->stylesheet);
                sdb->map = 0;
            }
            break;
    }
}

void destroy_session(struct session *s)
{
    struct session_database *sdb;

    while (s->clients)
        client_destroy(s->clients);
    for (sdb = s->databases; sdb; sdb = sdb->next)
        session_database_destroy(sdb);
    nmem_destroy(s->nmem);
    wrbuf_destroy(s->wrbuf);
}

struct session *new_session(NMEM nmem) 
{
    int i;
    struct session *session = nmem_malloc(nmem, sizeof(*session));

    yaz_log(YLOG_DEBUG, "New Pazpar2 session");
    
    session->relevance = 0;
    session->total_hits = 0;
    session->total_records = 0;
    session->num_termlists = 0;
    session->reclist = 0;
    session->requestid = -1;
    session->clients = 0;
    session->expected_maxrecs = 0;
    session->session_nmem = nmem;
    session->nmem = nmem_create();
    session->wrbuf = wrbuf_alloc();
    session->databases = 0;
    for (i = 0; i <= SESSION_WATCH_MAX; i++)
    {
        session->watchlist[i].data = 0;
        session->watchlist[i].fun = 0;
    }

    return session;
}

struct hitsbytarget *hitsbytarget(struct session *se, int *count)
{
    static struct hitsbytarget res[1000]; // FIXME MM
    struct client *cl;

    *count = 0;
    for (cl = se->clients; cl; cl = client_next_in_session(cl))
    {
        const char *name = session_setting_oneval(client_get_database(cl),
                                                  PZ_NAME);

        res[*count].id = client_get_database(cl)->database->url;
        res[*count].name = *name ? name : "Unknown";
        res[*count].hits = client_get_hits(cl);
        res[*count].records = client_get_num_records(cl);
        res[*count].diagnostic = client_get_diagnostic(cl);
        res[*count].state = client_get_state_str(cl);
        res[*count].connected  = client_get_connection(cl) ? 1 : 0;
        (*count)++;
    }

    return res;
}

struct termlist_score **termlist(struct session *s, const char *name, int *num)
{
    int i;

    for (i = 0; i < s->num_termlists; i++)
        if (!strcmp((const char *) s->termlists[i].name, name))
            return termlist_highscore(s->termlists[i].termlist, num);
    return 0;
}

#ifdef MISSING_HEADERS
void report_nmem_stats(void)
{
    size_t in_use, is_free;

    nmem_get_memory_in_use(&in_use);
    nmem_get_memory_free(&is_free);

    yaz_log(YLOG_LOG, "nmem stat: use=%ld free=%ld", 
            (long) in_use, (long) is_free);
}
#endif

struct record_cluster *show_single(struct session *s, const char *id)
{
    struct record_cluster *r;

    reclist_rewind(s->reclist);
    while ((r = reclist_read_record(s->reclist)))
        if (!strcmp(r->recid, id))
            return r;
    return 0;
}

struct record_cluster **show(struct session *s, struct reclist_sortparms *sp, 
                             int start, int *num, int *total, int *sumhits, 
                             NMEM nmem_show)
{
    struct record_cluster **recs = nmem_malloc(nmem_show, *num 
                                       * sizeof(struct record_cluster *));
    struct reclist_sortparms *spp;
    int i;
#if USE_TIMING    
    yaz_timing_t t = yaz_timing_create();
#endif

    if (!s->relevance)
    {
        *num = 0;
        *total = 0;
        *sumhits = 0;
        recs = 0;
    }
    else
    {
        for (spp = sp; spp; spp = spp->next)
            if (spp->type == Metadata_sortkey_relevance)
            {
                relevance_prepare_read(s->relevance, s->reclist);
                break;
            }
        reclist_sort(s->reclist, sp);
        
        *total = s->reclist->num_records;
        *sumhits = s->total_hits;
        
        for (i = 0; i < start; i++)
            if (!reclist_read_record(s->reclist))
            {
                *num = 0;
                recs = 0;
                break;
            }
        
        for (i = 0; i < *num; i++)
        {
            struct record_cluster *r = reclist_read_record(s->reclist);
            if (!r)
            {
                *num = i;
                break;
            }
            recs[i] = r;
        }
    }
#if USE_TIMING
    yaz_timing_stop(t);
    yaz_log(YLOG_LOG, "show %6.5f %3.2f %3.2f", 
            yaz_timing_get_real(t), yaz_timing_get_user(t),
            yaz_timing_get_sys(t));
    yaz_timing_destroy(&t);
#endif
    return recs;
}

void statistics(struct session *se, struct statistics *stat)
{
    struct client *cl;
    int count = 0;

    memset(stat, 0, sizeof(*stat));
    for (cl = se->clients; cl; cl = client_next_in_session(cl))
    {
        if (!client_get_connection(cl))
            stat->num_no_connection++;
        switch (client_get_state(cl))
        {
            case Client_Connecting: stat->num_connecting++; break;
            case Client_Initializing: stat->num_initializing++; break;
            case Client_Searching: stat->num_searching++; break;
            case Client_Presenting: stat->num_presenting++; break;
            case Client_Idle: stat->num_idle++; break;
            case Client_Failed: stat->num_failed++; break;
            case Client_Error: stat->num_error++; break;
            default: break;
        }
        count++;
    }
    stat->num_hits = se->total_hits;
    stat->num_records = se->total_records;

    stat->num_clients = count;
}

void start_http_listener(void)
{
    char hp[128] = "";
    struct conf_server *ser = global_parameters.server;

    if (*global_parameters.listener_override)
        strcpy(hp, global_parameters.listener_override);
    else
    {
        strcpy(hp, ser->host ? ser->host : "");
        if (ser->port)
        {
            if (*hp)
                strcat(hp, ":");
            sprintf(hp + strlen(hp), "%d", ser->port);
        }
    }
    http_init(hp);
}

void start_proxy(void)
{
    char hp[128] = "";
    struct conf_server *ser = global_parameters.server;

    if (*global_parameters.proxy_override)
        strcpy(hp, global_parameters.proxy_override);
    else if (ser->proxy_host || ser->proxy_port)
    {
        strcpy(hp, ser->proxy_host ? ser->proxy_host : "");
        if (ser->proxy_port)
        {
            if (*hp)
                strcat(hp, ":");
            sprintf(hp + strlen(hp), "%d", ser->proxy_port);
        }
    }
    else
        return;

    http_set_proxyaddr(hp, ser->myurl ? ser->myurl : "");
}


// Master list of connections we're handling events to
static IOCHAN channel_list = 0; 
void pazpar2_add_channel(IOCHAN chan)
{
    chan->next = channel_list;
    channel_list = chan;
}

void pazpar2_event_loop()
{
    event_loop(&channel_list);
}

static struct record_metadata *record_metadata_init(
    NMEM nmem, char *value, enum conf_metadata_type type)
{
    struct record_metadata *rec_md = record_metadata_create(nmem);
    if (type == Metadata_type_generic)
    {
        char * p = value;
        p = normalize7bit_generic(p, " ,/.:([");
        
        rec_md->data.text = nmem_strdup(nmem, p);
    }
    else if (type == Metadata_type_year)
    {
        int first, last;
        if (extract7bit_years((char *) value, &first, &last) < 0)
            return 0;
        rec_md->data.number.min = first;
        rec_md->data.number.max = last;
    }
    else
        return 0;
    return rec_md;
}

struct record *ingest_record(struct client *cl, Z_External *rec,
                             int record_no)
{
    xmlDoc *xdoc = normalize_record(client_get_database(cl),
        client_get_session(cl), rec);
    xmlNode *root, *n;
    struct record *record;
    struct record_cluster *cluster;
    struct session *se = client_get_session(cl);
    xmlChar *mergekey, *mergekey_norm;
    xmlChar *type = 0;
    xmlChar *value = 0;
    struct conf_service *service = global_parameters.server->service;

    if (!xdoc)
        return 0;

    root = xmlDocGetRootElement(xdoc);
    if (!(mergekey = xmlGetProp(root, (xmlChar *) "mergekey")))
    {
        yaz_log(YLOG_WARN, "No mergekey found in record");
        xmlFreeDoc(xdoc);
        return 0;
    }

    record = record_create(se->nmem, 
                           service->num_metadata, service->num_sortkeys, cl,
                           record_no);

    mergekey_norm = (xmlChar *) nmem_strdup(se->nmem, (char*) mergekey);
    xmlFree(mergekey);
    normalize7bit_mergekey((char *) mergekey_norm, 0);

    cluster = reclist_insert(se->reclist, 
                             global_parameters.server->service, 
                             record, (char *) mergekey_norm, 
                             &se->total_merged);
    if (global_parameters.dump_records)
        yaz_log(YLOG_LOG, "Cluster id %s from %s (#%d)", cluster->recid,
                client_get_database(cl)->database->url, record_no);
    if (!cluster)
    {
        /* no room for record */
        xmlFreeDoc(xdoc);
        return 0;
    }
    relevance_newrec(se->relevance, cluster);


    // now parsing XML record and adding data to cluster or record metadata
    for (n = root->children; n; n = n->next)
    {
        if (type)
            xmlFree(type);
        if (value)
            xmlFree(value);
        type = value = 0;

        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp((const char *) n->name, "metadata"))
        {
            struct conf_metadata *ser_md = 0;
            struct conf_sortkey *ser_sk = 0;
            struct record_metadata **wheretoput = 0;
            struct record_metadata *rec_md = 0;
            int md_field_id = -1;
            int sk_field_id = -1;

            type = xmlGetProp(n, (xmlChar *) "type");
            value = xmlNodeListGetString(xdoc, n->children, 1);

            if (!type || !value || !*value)
                continue;

            md_field_id 
                = conf_service_metadata_field_id(service, (const char *) type);
            if (md_field_id < 0)
            {
                yaz_log(YLOG_WARN, 
                        "Ignoring unknown metadata element: %s", type);
                continue;
            }

            ser_md = &service->metadata[md_field_id];

            if (ser_md->sortkey_offset >= 0){
                sk_field_id = ser_md->sortkey_offset;
                ser_sk = &service->sortkeys[sk_field_id];
            }

            // non-merged metadata
            rec_md = record_metadata_init(se->nmem, (char *) value,
                                          ser_md->type);
            if (!rec_md)
            {
                yaz_log(YLOG_WARN, "bad metadata data '%s' for element '%s'",
                        value, type);
                continue;
            }
            rec_md->next = record->metadata[md_field_id];
            record->metadata[md_field_id] = rec_md;

            // merged metadata
            rec_md = record_metadata_init(se->nmem, (char *) value,
                                          ser_md->type);
            wheretoput = &cluster->metadata[md_field_id];

            // and polulate with data:
            // assign cluster or record based on merge action
            if (ser_md->merge == Metadata_merge_unique)
            {
                struct record_metadata *mnode;
                for (mnode = *wheretoput; mnode; mnode = mnode->next)
                    if (!strcmp((const char *) mnode->data.text, 
                                rec_md->data.text))
                        break;
                if (!mnode)
                {
                    rec_md->next = *wheretoput;
                    *wheretoput = rec_md;
                }
            }
            else if (ser_md->merge == Metadata_merge_longest)
            {
                if (!*wheretoput 
                    || strlen(rec_md->data.text) 
                       > strlen((*wheretoput)->data.text))
                {
                    *wheretoput = rec_md;
                    if (ser_sk)
                    {
                        char *s = nmem_strdup(se->nmem, rec_md->data.text);
                        if (!cluster->sortkeys[sk_field_id])
                            cluster->sortkeys[sk_field_id] = 
                                nmem_malloc(se->nmem, 
                                            sizeof(union data_types));
                        normalize7bit_mergekey(s,
                             (ser_sk->type == Metadata_sortkey_skiparticle));
                        cluster->sortkeys[sk_field_id]->text = s;
                    }
                }
            }
            else if (ser_md->merge == Metadata_merge_all)
            {
                rec_md->next = *wheretoput;
                *wheretoput = rec_md;
            }
            else if (ser_md->merge == Metadata_merge_range)
            {
                if (!*wheretoput)
                {
                    *wheretoput = rec_md;
                    if (ser_sk)
                        cluster->sortkeys[sk_field_id] 
                            = &rec_md->data;
                }
                else
                {
                    int this_min = rec_md->data.number.min;
                    int this_max = rec_md->data.number.max;
                    if (this_min < (*wheretoput)->data.number.min)
                        (*wheretoput)->data.number.min = this_min;
                    if (this_max > (*wheretoput)->data.number.max)
                        (*wheretoput)->data.number.max = this_max;
                }
#ifdef GAGA
                if (ser_sk)
                {
                    union data_types *sdata 
                        = cluster->sortkeys[sk_field_id];
                    yaz_log(YLOG_LOG, "SK range: %d-%d",
                            sdata->number.min, sdata->number.max);
                }
#endif
            }


            // ranking of _all_ fields enabled ... 
            if (ser_md->rank)
                relevance_countwords(se->relevance, cluster, 
                                     (char *) value, ser_md->rank);

            // construct facets ... 
            if (ser_md->termlist)
            {
                if (ser_md->type == Metadata_type_year)
                {
                    char year[64];
                    sprintf(year, "%d", rec_md->data.number.max);
                    add_facet(se, (char *) type, year);
                    if (rec_md->data.number.max != rec_md->data.number.min)
                    {
                        sprintf(year, "%d", rec_md->data.number.min);
                        add_facet(se, (char *) type, year);
                    }
                }
                else
                    add_facet(se, (char *) type, (char *) value);
            }

            // cleaning up
            xmlFree(type);
            xmlFree(value);
            type = value = 0;
        }
        else
            yaz_log(YLOG_WARN,
                    "Unexpected element %s in internal record", n->name);
    }
    if (type)
        xmlFree(type);
    if (value)
        xmlFree(value);

    xmlFreeDoc(xdoc);

    relevance_donerecord(se->relevance, cluster);
    se->total_records++;

    return record;
}



/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
