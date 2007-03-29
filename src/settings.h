#ifndef SETTINGS_H
#define SETTINGS_H

#define PZ_PIGGYBACK    0 
#define PZ_ELEMENTS     1
#define PZ_SYNTAX       2

struct setting
{
    int precedence;
    char *target;
    char *name;
    char *value;
    char *user;
    struct setting *next;
};

void settings_read(const char *path);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
