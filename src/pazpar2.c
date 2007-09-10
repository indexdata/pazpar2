/* $Id: pazpar2.c,v 1.93 2007-09-10 08:42:48 adam Exp $
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

#if HAVE_CONFIG_H
#include "cconfig.h"
#endif


#include <signal.h>
#include <assert.h>

#include "pazpar2.h"
#include "database.h"
#include "settings.h"

void child_handler(void *data)
{
    start_proxy();
    init_settings();

    if (*global_parameters.settings_path_override)
        settings_read(global_parameters.settings_path_override);
    else if (global_parameters.server->settings)
        settings_read(global_parameters.server->settings);
    else
        yaz_log(YLOG_WARN, "No settings-directory specified");
    global_parameters.odr_in = odr_createmem(ODR_DECODE);
    global_parameters.odr_out = odr_createmem(ODR_ENCODE);


    pazpar2_event_loop();

}

static void show_version(void)
{
    char yaz_version_str[80];
    printf("Pazpar2 " VERSION "\n");

    yaz_version(yaz_version_str, 0);

    printf("Configuration:");
#if HAVE_ICU
    printf(" icu:?");
#endif
    printf(" yaz:%s", yaz_version_str);
    printf("\n");
    exit(0);
}            

int main(int argc, char **argv)
{
    int daemon = 0;
    int ret;
    int log_file_in_use = 0;
    char *arg;
    const char *pidfile = 0;
    const char *uid = 0;

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        yaz_log(YLOG_WARN|YLOG_ERRNO, "signal");

    yaz_log_init_prefix("pazpar2");

    while ((ret = options("dDf:h:l:p:t:u:VX", argv, argc, &arg)) != -2)
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
            strcpy(global_parameters.settings_path_override, arg);
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
                    "    -u uid\n"
                    "    -V                      show version\n"
                    "    -X                      debug mode\n"
                );
            exit(1);
	}
    }

    yaz_log(YLOG_LOG, "Pazpar2 %s started", VERSION);
    if (daemon && !log_file_in_use)
    {
        yaz_log(YLOG_FATAL, "Logfile must be given (option -l) for daemon "
                "mode");
        exit(1);
    }
    if (!config)
    {
        yaz_log(YLOG_FATAL, "Load config with -f");
        exit(1);
    }
    global_parameters.server = config->servers;

    start_http_listener();
    pazpar2_process(global_parameters.debug_mode, daemon,
                    child_handler, 0 /* child_data */,
                    pidfile, uid);
    return 0;
}


/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
