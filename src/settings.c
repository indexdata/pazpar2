// $Id: settings.c,v 1.1 2007-03-27 15:31:34 quinn Exp $
// This module implements a generic system of settings (attribute-value) that can 
// be associated with search targets. The system supports both default values,
// per-target overrides, and per-user settings.
//

#include <string.h>

#include <yaz/nmem.h>

static NMEM nmem = 0;

struct setting
{
};

struct setting_dictionary
{
};

// Recursively read files in a directory structure, calling 
static void read_settings(const char *path, void *context,
		void (*fun)(void *context, struct setting *set))
{
}

static void prepare_dictionary(void *context, struct setting *set)
{
}

static void update_databases(void *context, struct setting *set)
{
}

// If we ever decide we need to be able to specify multiple settings directories,
// the two calls to read_settings must be split -- so the dictionary is prepared
// for the contents of every directory before the databases are updated.
void settings_read(const char *path)
{
    struct setting_dictionary *new;
    if (!nmem)
        nmem = nmem_create();
    else
        nmem_reset(nmem);
    new = nmem_malloc(nmem, sizeof(*new));
    memset(new, sizeof(*new), 0);
    read_settings(path, new, prepare_dictionary);
    read_settings(path, new, update_databases);
}

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
