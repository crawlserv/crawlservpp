
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
 * crawled.php
 *
 * Show crawled data of currently selected URL and version.
 *
 */

echo "<button id=\"content-fullscreen\" title=\"Show Fullscreen\">&#9727;</button>\n";

echo "<div id=\"content-text\" class=\"fs-div\">\n";

echo "<pre class=\"fs-content\"><code id=\"content-code\" class=\"language-html fs-content\">\n";

if(isset($_POST["version"]) && $_POST["version"]) {
    // select specified version
    $version = $_POST["version"];
    
    $result = $dbConnection->query(
            "SELECT b.content".
            " FROM `crawlserv_".$namespace."_".$urllistNamespace."` AS a,".
            " `crawlserv_".$namespace."_".$urllistNamespace."_crawled` AS b ".
            " WHERE a.id = b.url".
            " AND a.id = $url".
            " AND b.id = $version".
            " LIMIT 1"
    );
    
    if(!$result)
        die("ERROR: Could not get content of URL.");
        
    $row = $result->fetch_assoc();
    
    if(!$row)
        die("ERROR: Could not get content of URL.");
        
    $result->close();
}
else {
    // select newest version
    $result = $dbConnection->query(
            "SELECT b.id AS id, b.content AS content".
            " FROM `crawlserv_".$namespace."_".$urllistNamespace."` AS a,".
            " `crawlserv_".$namespace."_".$urllistNamespace."_crawled` AS b".
            " WHERE a.id = b.url".
            " AND a.id = $url".
            " ORDER BY b.crawltime DESC".
            " LIMIT 1"
    );
    
    if(!$result)
        die("ERROR: Could not get content of URL.");
        
    $row = $result->fetch_assoc();
    
    if(!$row)
        die("ERROR: Could not get content of URL.");
        
    $version = $row["id"];
    
    $result->close();
}

echo htmlspecialchars(codeWrapper($row["content"], 220, 120));

$info = number_format(strlen($row["content"]))
        ." bytes <a href=\"#\" id=\"content-download\" target=\"_blank\""
        ." data-type=\"content\" data-website-namespace=\""
        .htmlspecialchars($namespace)
        ."\" data-namespace=\""
        .htmlspecialchars($urllistNamespace)
        ."\" data-version=\"$version\" data-filename=\""
        .htmlspecialchars($namespace."_".$urllistNamespace."_".$url)
        .".htm\">[Download]</a>";
        
echo "</code></pre></div>\n";

echo "<div id=\"content-status\">\n";

echo "<select id=\"content-version\" class=\"short\" data-m=\"$m\" data-tab=\"$tab\">\n";

$result = $dbConnection->query(
        "SELECT b.id AS id, b.crawltime AS crawltime, b.archived AS archived".
        " FROM `crawlserv_".$namespace."_".$urllistNamespace."` AS a,".
        " `crawlserv_".$namespace."_".$urllistNamespace."_crawled` AS b".
        " WHERE a.id = b.url".
        " AND a.id = $url".
        " ORDER BY b.crawltime DESC"
);

if(!$result)
    die("ERROR: Could not get content versions.");
    
$num = $result->num_rows;

$count = 0;

while($row = $result->fetch_assoc()) {
    $count++;
    
    echo "<option value=\"".$row["id"]."\"";
    
    if($row["id"] == $version)
        echo " selected";
        
    echo ">Version "
            .number_format($count)
            ." of "
            .number_format($num)
            ." &ndash; crawled on "
            .$row["crawltime"];
                
    if($row["archived"])
        echo " (archived)";
        
    echo "</option>\n";
}

echo "</select>\n";

echo "<span id=\"content-info\">$info</span></div>\n";

?>
