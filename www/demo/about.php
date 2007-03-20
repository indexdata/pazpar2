<?php

/* $Id: about.php,v 1.2 2007-03-20 05:23:06 quinn Exp $
 * --------------------------------------------------------------------
 * Keystone retreiver about page
 */

require('includes.phpi');

insert_header();

?>

<h3>What is This?</h3>

<p>
This is an early prototype of a new metasearch technology developed by Index Data.
It is scheduled for general release during the spring of 2007. It enables
efficient metasearching of up to hundreds of databases at the same time
using Z39.50, SRU/W, or proprietary protocols. It is a powerful,
open-source-based alternative to proprietary, closed-source metasearch
alternatives.
</p>

<p>
The technology supports on-the-fly merging, relevance-ranking, or sorting by
arbitrary data elements. It also supports any number of result facets for
limiting result sets by subject, author, etc.
</p>

<p>
Please <a href="mailto:info@indexdata.com">contact us</a> if you are
interested in more information about our metasearch technology.
</p>

<?php

insert_footer();

?>
