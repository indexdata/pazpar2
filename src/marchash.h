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

#ifndef MARCHASH_H
#define MARCHASH_H

#define MARCHASH_MASK 127

struct marchash
{
    struct marcfield *table[MARCHASH_MASK + 1];
    NMEM nmem;
};

struct marcfield
{
   char key[4];
   char *val;
   struct marcsubfield *subfields;
   struct marcfield *next;
};

struct marcsubfield
{
   char key;
   char *val;
   struct marcsubfield *next;
};

struct marchash *marchash_create (NMEM nmem);
void marchash_ingest_marcxml (struct marchash *marchash, xmlNodePtr rec_node);
struct marcfield *marchash_add_field (struct marchash *marchash,
                                      const char *key, const char *value);
struct marcsubfield *marchash_add_subfield (struct marchash *marchash, struct marcfield *field, const char key, const char *value);
struct marcfield *marchash_get_field (struct marchash *marchash, const char *key, struct marcfield *last);
struct marcsubfield *marchash_get_subfield (char key, struct marcfield *field, struct marcsubfield *last);

char *marchash_catenate_subfields(struct marcfield *field,
                                  const char *delim, NMEM nmem);

#endif
/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
