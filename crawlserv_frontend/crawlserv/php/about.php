
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
 * about.php
 * 
 * Version and licensing information.
 *
 */

$db_init = true;

require "include/version.php";

require "config.php";

// get version information from the database
$versions = array();

$result = $dbConnection->query(
    "SELECT name, version".
    " FROM crawlserv_versions"
    );

if($result) {
    while($row = $result->fetch_assoc())
        $versions[$row["name"]] = $row["version"];
    
    $result->close();
}

if(!array_key_exists("crawlserv++", $versions))
    $versions["crawlserv++"] = "<i>[NOT FOUND]</i>"

?>

<h2>About</h2>

<div class="content-block version-info">

<div class="entry-row">

<b>Client: PHP/JavaScript frontend for crawlserv++</b><br>
by Anselm Schmidt (ans[ät]ohai.su)<br>
version <?php

echo $GLOBALS["crawlservpp_version_string"];

?>

</div>

</div>

<div class="content-block version-info">

<div class="entry-row">

<b>Server: crawlserv++</b><br>
by Anselm Schmidt (ans[ät]ohai.su)<br>
version <?php

echo $versions["crawlserv++"]

?>

</div>

<?php

if(sizeof($versions) > 1) {
    echo "<div class=\"entry-row\">\n\n";
    echo "<div class=\"entry-label\">\n\n";
    echo "using:\n\n";
    echo "</div>\n\n";
    
    $first = true;

    foreach($versions as $module => $version) {
        if($module == "crawlserv++")
            continue;
        
        if($first)
            $first = false;
        else {
            echo "<div class=\"entry-row\">\n\n";
            echo "<div class=\"entry-label\"></div>\n\n";
        }
        
        echo "<div class=\"entry-value\">\n\n";
        
        echo $module;
    
        if(strlen($version))
            echo " v$version";
        
        echo "\n";
        
        echo "</div>\n\n";
        echo "</div>\n\n";
    }
}
?>

</div>

<div class="content-block version-info">

<div class="entry-row">

<b>License:</b>

</div>

<div class="entry-row">

<textarea id="license" readonly>
<?php 

if(file_exists("LICENSE")) {    
    include "LICENSE";
}
else
    echo "<i>[NOT FOUND]</i>";
    
?>
</textarea>

</div>

</div>
