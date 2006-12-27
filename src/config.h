#ifndef CONFIG_H
#define CONFIG_H

struct conf_termlist
{
    char *name;
    struct conf_termlist *next;
};

struct conf_service
{
    struct conf_termlist *termlists;
};

struct conf_server
{
    char *host;
    int port;
    char *proxy_host;
    int proxy_port;
    struct conf_service *service;
    struct conf_server *next;
};

struct conf_queryprofile
{
};

struct conf_retrievalprofile
{
};

struct conf_config
{
    struct conf_server *servers;
    struct conf_queryprofile *queryprofiles;
    struct conf_retrievalprofile *retrievalprofiles;
};

#ifndef CONFIG_NOEXTERNS

extern struct conf_config *config;

#endif

int read_config(const char *fname);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
