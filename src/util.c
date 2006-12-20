/* $Id: util.c,v 1.1 2006-12-20 20:47:16 quinn Exp $ */

#include <stdlib.h>
#include <yaz/yaz-util.h>

void die(char *string, char *add)
{
    yaz_log(YLOG_FATAL, "Fatal error: %s (%s)", string, add ? add : "");
    abort();
}

