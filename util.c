/* $Id: util.c,v 1.1.1.1 2006-11-14 20:44:38 quinn Exp $ */

#include <stdio.h>
#include <stdlib.h>

extern char *myname;

void die(char *string, char *add)
{
    fprintf(stderr, "%s: %s (%s)\n", myname, string, add ? add : "");
    abort();
}

