/* $Id: parameters.h,v 1.6 2007-10-02 10:08:39 adam Exp $
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

#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <yaz/odr.h>

/** \brief global parameters */
struct parameters {
    char proxy_override[128];
    char listener_override[128];
    char settings_path_override[128];
    struct conf_server *server;
    int dump_records;
    int debug_mode;
    int timeout;		/* operations timeout, in seconds */
    char implementationId[128];
    char implementationName[128];
    char implementationVersion[128];
    int session_timeout;
    int toget;
    int chunk;
    ODR odr_out;
    ODR odr_in;
    int z3950_session_timeout;
    int z3950_connect_timeout;
};
extern struct parameters global_parameters;

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
