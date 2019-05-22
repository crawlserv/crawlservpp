
<?php

/*
 * 
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
 * 
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * search.php
 *
 * Manage the threads running on the command-and-control server.
 *
 */

$db_init = true;
$cc_init = true;

require "_helpers.php";
require "config.php";

if(isset($_POST["website"]))
    $website = $_POST["website"];

if(isset($_POST["module"]))
    $module = $_POST["module"];
else
    $module = "crawler";

if(isset($_POST["config"]))
    $config = $_POST["config"];

if(isset($_POST["urllist"]))
    $urllist = $_POST["urllist"];

?>

<h2>Threads</h2>

<div id="threads">

<div class="content-block">
<div class="entry-row">
<div class="entry-value">Loading...</div>

</div>
</div>
</div>

<div class="content-block">

<?php 

echo rowWebsiteSelect(false, false, true);

?>

<div class="entry-row">
<div class="entry-label">Module:</div><div class="entry-input">

<select id="module-select" class="entry-input" data-m="threads">

<option value="crawler" <?php

if($module == "crawler")
    echo " selected";

?>>Crawler</option>

<option value="parser" <?php

if($module == "parser")
    echo " selected";

?>>Parser</option>

<option value="extractor" <?php

if($module == "extractor")
    echo " selected";

?>>Extractor</option>

<option value="analyzer" <?php
if($module == "analyzer")
    echo " selected";

?>>Analyzer</option>

</select>

</div>
</div>

<?php

if(isset($website)) {    
    echo rowConfigSelect(ucfirst($module), false, true);    
    echo rowUrlListSelect(false, false, false, true);
}

?>

<div class="action-link-box" id="scroll-to">
<div class="action-link">

<?php

if(isset($website))
    echo "<a id=\"thread-start\" href=\"#\" class=\"action-link\""
        ." data-module=\"$module\" data-noreload>Start thread</a>\n";

?>

</div>
</div>
</div>
