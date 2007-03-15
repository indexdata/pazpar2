#ifndef DATABASE_H
#define DATABASE_H

void load_simpletargets(const char *fn);
int grep_databases(void *context, void (*fun)(void *context, struct database *db));

#endif
