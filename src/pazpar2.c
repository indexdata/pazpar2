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

#include "pazpar2.h"
#include "database.h"
#include "settings.h"
#include <yaz/daemon.h>

#include <yaz/sc.h>

static char *path_override = 0;

void child_handler(void *data)
{
    start_proxy();

    config_read_settings(path_override);

    global_parameters.odr_in = odr_createmem(ODR_DECODE);
    global_parameters.odr_out = odr_createmem(ODR_ENCODE);

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
    int session_timeout = 60;

#ifndef WIN32
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        yaz_log(YLOG_WARN|YLOG_ERRNO, "signal");
#else
    tcpip_init();
#endif

    yaz_log_init_prefix("pazpar2");
    yaz_log_xml_errors(0, YLOG_WARN);

    while ((ret = options("dDf:h:l:p:t:T:u:VX", argv, argc, &arg)) != -2)
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
            if (!read_config(arg))
                exit(1);
            break;
        case 'h':
            strcpy(global_parameters.listener_override, arg);
            break;
        case 'l':
            yaz_log_init_file(arg);
            log_file_in_use = 1;
            break;
        case 'p':
            pidfile = arg;
            break;
        case 't':
            path_override = arg;
            break;
        case 'T':
	    session_timeout = atoi(arg);
	    if (session_timeout < 9 || session_timeout > 86400)
            {
                yaz_log(YLOG_FATAL, "Session timeout out of range 10..86400: %d",
                        session_timeout);
                return 1;
            }
            global_parameters.session_timeout = session_timeout;
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
                    "    -d                      (show internal records)\n"
                    "    -D                      Daemon mode (background)\n"
                    "    -f configfile\n"
                    "    -h [host:]port          (REST protocol listener)\n"
                    "    -l file                 log to file\n"
                    "    -p pidfile              PID file\n"
                    "    -t settings\n"
                    "    -T session_timeout\n"
                    "    -u uid\n"
                    "    -V                      show version\n"
                    "    -X                      debug mode\n"
#ifdef WIN32
                    "    -install                install windows service\n"
                    "    -remove                 remove windows service\n"
#endif
                );
            return 1;
	}
    }

    yaz_log(YLOG_LOG, "Pazpar2 %s started", VERSION);
    if (daemon && !log_file_in_use)
    {
        yaz_log(YLOG_FATAL, "Logfile must be given (option -l) for daemon "
                "mode");
        return 1;
    }
    if (!config)
    {
        yaz_log(YLOG_FATAL, "Load config with -f");
        return 1;
    }
    global_parameters.server = config->servers;

    ret = start_http_listener();
    if (ret)
        return ret; /* error starting http listener */

    yaz_sc_running(s);

    yaz_daemon("pazpar2",
               (global_parameters.debug_mode ? YAZ_DAEMON_DEBUG : 0) +
               (daemon ? YAZ_DAEMON_FORK : 0) + YAZ_DAEMON_KEEPALIVE,
               child_handler, 0 /* child_data */,
               pidfile, uid);
    return 0;
}


static void sc_stop(yaz_sc_t s)
{
    http_close_server();
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

