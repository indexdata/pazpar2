/* $Id: test_normalize.c,v 1.1 2007-09-07 10:46:33 adam Exp $
   Copyright (c) 2006-2007, Index Data.

This file is part of Pazpar2.

Pazpar2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

Pazpar2 is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with Pazpar2; see the file LICENSE.  If not, write to the
Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.
 */

#if HAVE_CONFIG_H
#include "cconfig.h"
#endif

#include <string.h>
#include <yaz/xmalloc.h>
#include <yaz/test.h>

#include "normalize7bit.h"

int test_normalize7bit_generic(const char *rm_chars, const char *input, 
                               const char *expect_output)
{
    int ret = 0;
    char *tmp = xstrdup(input);
    char *output = normalize7bit_generic(tmp, rm_chars);
    if (!strcmp(expect_output, output))
        ret = 1;
    xfree(tmp);
    return ret;
}

int test_normalize7bit_mergekey(int skiparticle, const char *input,
                                const char *expect_output)
{
    int ret = 0;

    char *tmp = xstrdup(input);
    char *output = normalize7bit_mergekey(tmp, skiparticle);
    if (!strcmp(expect_output, output))
        ret = 1;
    xfree(tmp);
    return ret;
}

int main(int argc, char **argv)
{
    YAZ_CHECK_INIT(argc, argv); 
    YAZ_CHECK_LOG();

    YAZ_CHECK(test_normalize7bit_generic("/; ", " how are you; ", "how are you"));
    YAZ_CHECK(!test_normalize7bit_generic("/; ", " how are you; ", "how are youx"));
 
    YAZ_CHECK(test_normalize7bit_generic("/; "," ", ""));

    YAZ_CHECK(test_normalize7bit_mergekey(0, "the art of computer", "the art of computer"));
    YAZ_CHECK(test_normalize7bit_mergekey(1, "the art of computer", "art of computer"));

    YAZ_CHECK(test_normalize7bit_mergekey(1, "The Art Of Computer", "art of computer"));
   
    YAZ_CHECK_TERM;
}




/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
