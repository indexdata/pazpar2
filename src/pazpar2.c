/* This file is part of Pazpar2.
   Copyright (C) 2006-2013 Index Data

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
#include <direct.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <signal.h>
#include <assert.h>

#include "parameters.h"
#include "session.h"
#include "ppmutex.h"
#include <yaz/daemon.h>
#include <yaz/log.h>
#include <yaz/options.h>
#include <yaz/sc.h>

// #define MTRACE
#ifdef MTRACE
#include <mcheck.h>
#endif

static struct conf_config *sc_stop_config = 0;

void child_handler(void *data)
{
    struct conf_config *config = (struct conf_config *) data;

    config_process_events(config);

    config_destroy(config);
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
    printf(" icu:enabled");
#else
    printf(" icu:disabled");
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
    const char *record_fname = 0;
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

    while ((ret = options("dDf:h:l:m:p:R:tu:v:Vw:X", argv, argc, &arg)) != -2)
    {
	switch (ret)
        {
        case 'd':
            global_parameters.dump_records++;
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
        case 'm':
            yaz_log_time_format(arg);
            break;
        case 'p':
            pidfile = arg;
            break;
        case 'R':
            record_fname = arg;
            global_parameters.predictable_sessions = 1;
            break;
        case 't':
            test_mode = 1;
            break;
        case 'u':
            uid = arg;
            break;
        case 'v':
            yaz_log_init_level(yaz_log_mask_str(arg));
            break;
        case 'V':
            show_version();
            break;
        case 'w':
            if (
#ifdef WIN32
              _chdir
#else
              chdir
#endif
                (arg))
            {
                yaz_log(YLOG_FATAL|YLOG_ERRNO, "chdir %s", arg);
                return 1;
            }
            break;
        case 'X':
            global_parameters.debug_mode++;
            global_parameters.predictable_sessions = 1;
            break;
        default:
            fprintf(stderr, "Usage: pazpar2\n"
                    "    -d                      Show internal records\n"
                    "    -D                      Daemon mode (background)\n"
                    "    -f configfile           Configuration\n"
                    "    -h [host:]port          Listener port\n"
                    "    -l file                 Log to file\n"
                    "    -m logformat            log time format (strftime)\n"
                    "    -p pidfile              PID file\n"
                    "    -R recfile              HTTP recording file\n"
                    "    -t                      Test configuration\n"
                    "    -u uid                  Change user to uid\n"
                    "    -V                      Show version\n"
                    "    -v level                Set log level\n"
                    "    -w dir                  Working directory\n"
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
    pazpar2_mutex_init();

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
        yaz_log(YLOG_LOG, "Pazpar2 start " VERSION  " "
#ifdef PAZPAR2_VERSION_SHA1
                PAZPAR2_VERSION_SHA1
#else
                "-"
#endif
                );
        ret = 0;
        if (daemon && !log_file_in_use)
        {
            yaz_log(YLOG_FATAL, "Logfile must be given (option -l) for daemon "
                    "mode");
            ret = 1;
        }
        if (!ret)
            ret = config_start_listeners(config, listener_override,
                                         record_fname);
        if (!ret)
        {
            yaz_sc_running(s);
            yaz_daemon("pazpar2",
                       (global_parameters.debug_mode ? YAZ_DAEMON_DEBUG : 0) +
                       (daemon ? YAZ_DAEMON_FORK : 0) + YAZ_DAEMON_KEEPALIVE,
                       child_handler, config /* child_data */,
                       pidfile, uid);
        }
        yaz_log(YLOG_LOG, "Pazpar2 stop");
        return ret;
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

#ifdef MTRACE
    mtrace();
#endif

    ret = yaz_sc_program(s, argc, argv, sc_main, sc_stop);

    yaz_sc_destroy(&s);

#ifdef MTRACE
    muntrace();
#endif


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

