#ifndef DATABASE_H
#define DATABASE_H

void load_simpletargets(const char *fn);
void prepare_databases(void);
int grep_databases(void *context, struct database_criterion *cl,
        void (*fun)(void *context, struct database *db));
int database_match_criteria(struct database *db, struct database_criterion *cl);

#endif
