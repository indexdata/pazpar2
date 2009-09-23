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
#ifdef WIN32
#include <winsock.h>
#endif

#include <signal.h>
#include <assert.h>

#include "parameters.h"
#include "pazpar2.h"
#include <yaz/daemon.h>
#include <yaz/log.h>
#include <yaz/options.h>
#include <yaz/sc.h>

static struct conf_config *sc_stop_config = 0;

void child_handler(void *data)
{
    struct conf_config *config = (struct conf_config *) data;

    config_start_databases(config);

    pazpar2_event_loop();
}

static void show_version(void)
{
    char yaz_version_str[80];
    printf("Pazpar2 " PACKAGE_VERSION 
#ifdef PAZPAR2_VERSION_SHA1
           " "
           PAZPAR2_VERSION_SHA1
#endif
"\n");

    yaz_version(yaz_version_str, 0);

    printf("Configuration:");
#if YAZ_HAVE_ICU
    printf(" icu:?");
#endif
    printf(" yaz:%s", yaz_version_str);
    printf("\n");
    exit(0);
}            

#ifdef WIN32
static int tcpip_init (void)
{
    WORD requested;
    WSADATA wd;

    requested = MAKEWORD(1, 1);
    if (WSAStartup(requested, &wd))
        return 0;
    return 1;
}
#endif


static int sc_main(
    yaz_sc_t s, 
    int argc, char **argv)
{
    int daemon = 0;
    int ret;
    int log_file_in_use = 0;
    char *arg;
    const char *pidfile = 0;
    const char *uid = 0;
    const char *listener_override = 0;
    const char *config_fname = 0;
    struct conf_config *config = 0;
    int test_mode = 0;

#ifndef WIN32
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        yaz_log(YLOG_WARN|YLOG_ERRNO, "signal");
#else
    tcpip_init();
#endif

    yaz_log_init_prefix("pazpar2");
    yaz_log_xml_errors(0, YLOG_WARN);

    while ((ret = options("dDf:h:l:p:tu:VX", argv, argc, &arg)) != -2)
    {
	switch (ret)
        {
        case 'd':
            global_parameters.dump_records = 1;
            break;
        case 'D':
            daemon = 1;
            break;
        case 'f':
            config_fname = arg;
            break;
        case 'h':
            listener_override = arg;
            break;
        case 'l':
            yaz_log_init_file(arg);
            log_file_in_use = 1;
            break;
        case 'p':
            pidfile = arg;
            break;
        case 't':
            test_mode = 1;
            break;
        case 'u':
            uid = arg;
            break;
        case 'V':
            show_version();
        case 'X':
            global_parameters.debug_mode = 1;
            break;
        default:
            fprintf(stderr, "Usage: pazpar2\n"
                    "    -d                      Show internal records\n"
                    "    -D                      Daemon mode (background)\n"
                    "    -f configfile           Configuration\n"
                    "    -h [host:]port          Listener port\n"
                    "    -l file                 Log to file\n"
                    "    -p pidfile              PID file\n"
                    "    -t                      Test configuration\n"
                    "    -u uid                  Change user to uid\n"
                    "    -V                      Show version\n"
                    "    -X                      Debug mode\n"
#ifdef WIN32
                    "    -install                Install windows service\n"
                    "    -remove                 Remove windows service\n"
#endif
                );
            return 1;
	}
    }
    if (!config_fname)
    {
        yaz_log(YLOG_FATAL, "Configuration must be given with option -f");
        return 1;
    }
    config = config_create(config_fname, global_parameters.dump_records);
    if (!config)
        return 1;
    sc_stop_config = config;
    if (test_mode)
    {
        yaz_log(YLOG_LOG, "Configuration OK");
        config_destroy(config);
    }
    else
    {
        yaz_log(YLOG_LOG, "Pazpar2 %s started", VERSION);
        if (daemon && !log_file_in_use)
        {
            yaz_log(YLOG_FATAL, "Logfile must be given (option -l) for daemon "
                    "mode");
            return 1;
        }
        ret = config_start_listeners(config, listener_override);
        if (ret)
            return ret; /* error starting http listener */
        
        yaz_sc_running(s);
        
        yaz_daemon("pazpar2",
                   (global_parameters.debug_mode ? YAZ_DAEMON_DEBUG : 0) +
                   (daemon ? YAZ_DAEMON_FORK : 0) + YAZ_DAEMON_KEEPALIVE,
                   child_handler, config /* child_data */,
                   pidfile, uid);
    }
    return 0;
}


static void sc_stop(yaz_sc_t s)
{
    config_stop_listeners(sc_stop_config);
}

int main(int argc, char **argv)
{
    int ret;
    yaz_sc_t s = yaz_sc_create("pazpar2", "Pazpar2");

    ret = yaz_sc_program(s, argc, argv, sc_main, sc_stop);

    yaz_sc_destroy(&s);
    exit(ret);
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

