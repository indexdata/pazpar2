/* This file is part of Pazpar2.
   Copyright (C) Index Data

Pazpar2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

Pazpar2 is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/

#if HAVE_CONFIG_H
#include <config.h>
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

int test_normalize7bit_mergekey(const char *input,
                                const char *expect_output)
{
    int ret = 0;

    char *tmp = xstrdup(input);
    char *output = normalize7bit_mergekey(tmp);
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

    YAZ_CHECK(test_normalize7bit_mergekey("the art of computer", "the art of computer"));
    YAZ_CHECK(test_normalize7bit_mergekey("The Art Of Computer", "the art of computer"));

    YAZ_CHECK_TERM;
}




/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

