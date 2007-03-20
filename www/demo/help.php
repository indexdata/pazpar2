<?php

/* $Id: help.php,v 1.2 2007-03-20 05:23:06 quinn Exp $
 * --------------------------------------------------------------------
 * Keystone retreiver help page
 */

require('includes.phpi');

insert_header();

?>

<h3>How to Use This Portal</h3>

<p>
You can enter search keywords into the search bar, just as you would for
google. However, you can also limit your search to specific fields.
Examples:
</p>
<p>
ti=old christmas
</p>
<p>
ti=old christmas and au=washington
</p>
<p>
You can use boolean operators, and, or, not, to combine terms, or even
parantheses to group complex expressions. The search fields supported are
au=author, ti=title, and su=subject.
</p>

<?php

insert_footer();

?>
