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

#ifndef DATABASE_H
#define DATABASE_H

struct session_database;
struct session;
struct conf_service;
struct settings_array;

struct database *create_database_for_service(const char *id,
					     struct conf_service *service);
int session_grep_databases(struct session *se, const char *filter,
        void (*fun)(struct session *se, struct session_database *db));
int predef_grep_databases(void *context, struct conf_service *service,
			  void (*fun)(void *context, struct database *db),
			  int all);
int match_zurl(const char *zurl, const char *pattern);
struct database *new_database(const char *id, NMEM nmem);
// inherit values from (service) settings
struct database *new_database_inherit_settings(const char *id, NMEM nmem, struct settings_array *settings);


#endif
