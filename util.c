/* $Id: util.c,v 1.2 2006-11-18 05:00:38 quinn Exp $ */

#include <stdlib.h>
#include <yaz/yaz-util.h>

void die(char *string, char *add)
{
    yaz_log(YLOG_FATAL, "Fatal error: %s (%s)", string, add ? add : "");
    abort();
}

