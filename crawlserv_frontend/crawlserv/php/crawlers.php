
<?php

/*
 * 
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
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
 * crawlers.php
 *
 * Manage configurations for crawler threads.
 *
 */

$db_init = true;
$cc_init = true;

require "include/helpers.php";

require "config.php";

if(isset($_POST["website"]))
    $website = $_POST["website"];

if(isset($_POST["config"]))
    $config = $_POST["config"];

if(isset($_POST["mode"]))
    $mode = $_POST["mode"];
else
    $mode = "simple";

if(isset($_POST["current"]))
    $current = $_POST["current"];

?>

<h2>Crawling options<?php

echo "<span id=\"opt-mode\">\n";

if($mode == "simple")
    echo "<b>simple</b>";
else
    echo "<a href=\"#\" class=\"action-link post-redirect-mode\" data-m=\"$m\" data-mode=\"simple\">simple</a>\n";

echo " &middot; ";

if($mode == "advanced")
    echo "<b>advanced</b>";
else
    echo "<a href=\"#\" class=\"action-link post-redirect-mode\" data-m=\"$m\" data-mode=\"advanced\">advanced</a>\n";

echo "</span>\n";

?></h2>

<div class="content-block">

<?php

echo rowWebsiteSelect();

if(isset($website))
    echo rowConfigSelect("Crawler", true);

?>

<div class="action-link-box">
<div class="action-link">

<?php

if($config) {
    echo "<a id=\"config-duplicate\" href=\"#\" class=\"action-link\" data-m=\"$m\" data-mode=\"$mode\">";
    echo "Duplicate configuration";
    echo "</a>\n";
}

?>

</div>
</div>
</div>

<?php

if(isset($website)) {
    echo "<div class =\"content-block\">\n";
    echo "<div class=\"opt-block\">\n";
    
    echo "<div class=\"opt-label\">Name:</div>\n";
    echo "<div class=\"opt-value\" data-tippy=\"Name of the crawling configuration.\""
        ." data-tippy-delay=\"0\" data-tippy-duration=\"0\" data-tippy-arrow=\"true\" data-tippy-placement=\"left-start\""
        ." data-tippy-size=\"small\">\n";
    
    echo "<input type=\"text\" class=\"opt\" id=\"config-name\" value=\"";
    
    if(isset($_POST["name"]))
        echo $_POST["name"];
    else if($config)
        echo $configName;
    
    echo "\" />\n";
    
    echo "</div>\n";
    echo "</div>\n";
    
    echo "<div class=\"action-link-box\">\n";
    echo "<div class=\"action-link\">\n";
    if($config) {
        echo "<a href=\"#\" class=\"action-link config-update\" data-m=\"$m\" data-mode=\"$mode\">";
        echo "Change crawler</a>\n";
    }
    else {
        echo "<a href=\"#\" class=\"action-link config-add\" data-module=\"crawler\" data-m=\"$m\" data-mode=\"$mode\">";
        echo "Add crawler</a>\n";
    }
    
    echo "</div>\n";
    echo "</div>\n";
    
    echo "<div id=\"config-view\"></div>\n";
    echo "<div class=\"action-link-box\">\n";
    echo "<div class=\"action-link\">\n";
    
    if($config) {
        echo "<a href=\"#\" class=\"action-link config-update\" data-m=\"$m\" data-mode=\"$mode\">";
        echo "Change crawler</a>\n";
    }
    else {
        echo "<a href=\"#\" class=\"action-link config-add\" data-module=\"crawler\" data-m=\"$m\" data-mode=\"$mode\">";
        echo "Add crawler</a>\n";
    }
    
    echo "</div>\n";
    echo "</div>\n";
    echo "</div>\n";
}

?>

<script>

<?php

echo scriptModule();

?>

// create configuration object

var config = new Config("crawler", <?php

echo json_encode($current);

?>, "<?php

echo $mode;

?>");

</script>
