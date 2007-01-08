/* $Id: util.c,v 1.2 2007-01-08 12:43:41 adam Exp $ */

#include <stdlib.h>

#if HAVE_CONFIG_H
#include <cconfig.h>
#endif

#include <yaz/yaz-util.h>

void die(char *string, char *add)
{
    yaz_log(YLOG_FATAL, "Fatal error: %s (%s)", string, add ? add : "");
    abort();
}

