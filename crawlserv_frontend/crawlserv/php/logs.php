
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
 * logs.php
 *
 * Show and modify the log entries in the database.
 *
 */

$db_init = true;
$cc_init = true;

require "config.php";

function filterlink($filter) {
    return "<a href=\"#\" class=\"action-link post-redirect-filter\""
            ." data-m=\"logs\" data-filter=\"$filter\">$filter</a>";
}

if(isset($_POST["filter"]) && $_POST["filter"] != "all")
    $filter = $_POST["filter"];
else
    $filter = "";

?>

<h2>Logs</h2>

<div id="top-box">

<?php

if(empty($filter))
    echo "<b>all</b>";
else
    echo filterlink("all");

echo " &middot; ";

if($filter == "server")
    echo "<b>server</b>";
else
    echo filterlink("server");

echo " &middot; ";

if($filter == "frontend")
    echo "<b>frontend</b>";
else
    echo filterlink("frontend");

echo " &middot; ";

if($filter == "crawler")
    echo "<b>crawler</b>";
else
    echo filterlink("crawler");

echo " &middot; ";

if($filter == "parser")
    echo "<b>parser</b>";
else
    echo filterlink("parser");

echo " &middot; ";

if($filter == "extractor")
    echo "<b>extractor</b>";
else
    echo filterlink("extractor");

echo " &middot; ";

if($filter == "analyzer")
    echo "<b>analyzer</b>";
else
    echo filterlink("analyzer");

echo " &middot; ";

if($filter == "worker")
    echo "<b>worker</b>";
else
    echo filterLink("worker");

if(isset($_POST["offset"]))
    $offset = $_POST["offset"];
else
    $offset = 0;

if(isset($_POST["limit"]))
    $limit = $_POST["limit"];
else
    $limit = 100;

?>

</div>

<div class="content-list">

<select size="<?php echo $limit; ?>" class="content-list">

<?php

$no = 0;

if(empty($filter))
    $result = $dbConnection->query(
            "SELECT COUNT(id)".
            " FROM crawlserv_log"
    );
else
    $result = $dbConnection->query(
            "SELECT COUNT(id)".
            " FROM crawlserv_log".
            " WHERE module='$filter'"
    );

$row = $result->fetch_row();

$total = $row[0];

if(empty($filter))
    $result = $dbConnection->query(
            "SELECT time, module, entry".
            " FROM crawlserv_log".
            " ORDER BY id DESC".
            " LIMIT $offset, $limit"
    );
else
    $result = $dbConnection->query(
            "SELECT time, module, entry".
            " FROM crawlserv_log".
            " WHERE module='$filter'".
            " ORDER BY id DESC".
            " LIMIT $offset, $limit"
    );

if($result->num_rows > 0)
    while($row = $result->fetch_assoc()) {
        $entry = "[".$row["time"]."] ".$row["module"].": ".$row["entry"];
        
        echo "<option>".html($entry)."</option>\n";
        
        $no++;
    }
else
    echo "<option disabled>No log entries available.</option>";

?>

</select>

</div>

<div id="bottom-box">

<?php

if($no) {
    if($offset > 0) {
        echo "<a href=\"#\" class=\"action-link logs-nav\" data-filter=\"$filter\" data-offset=\"";
        echo ($offset - $limit)."\" data-limit=\"$limit\">&larr;Previous</a>";
    }
    else
        echo "<span class=\"inactive-link\">&larr;Previous</span>";
    
    echo " &middot; ";
    
    if($offset + $no < $total) {
        echo "<a href=\"#\" class=\"action-link logs-nav\" data-filter=\"$filter\" data-offset=\"";
        echo ($offset + $limit)."\" data-limit=\"$limit\">Next&rarr;</a>";
    }
    else
        echo "<span class=\"inactive-link\">Next&rarr;</span>";
    
    echo " &middot; ";
    
    echo "<a href=\"#\" class=\"action-link logs-nav\" data-filter=\"$filter\" data-offset=\"0\" data-limit=\"$limit\">";
    echo "&#8635;Refresh</a> &middot; ";
    
    echo "<a href=\"#\" class=\"action-link logs-clear\" data-filter=\"$filter\">&cross;Clear</a>";
    
    echo "<br>Showing "
        .number_format($offset + 1)
        ." to "
        .number_format($offset + $no)
        ." of "
        .number_format($total)
        ." entries.<br>\n";
}
else
    echo "<a href=\"#\" class=\"action-link logs-nav\" data-filter=\"$filter\">&#8635;Refresh</a>";

?>

</div>
