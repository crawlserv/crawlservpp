
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
 * content.php
 *
 * Show raw parsed, crawled and extracted content as well as the analyzing results. 
 *
 */

$db_init = true;
$cc_init = true;

require "_helpers.php";
require "config.php";

// function to wrap long code lines
function codeWrapper($str, $limit, $indent_limit) {
    $out = "";
    
    foreach(preg_split("/((\r?\n)|(\r\n?))/", $str) as $line) {
        while(strlen($line) > $limit) {
            // get indent
            $indent = strspn($line, " \t");
            
            if($indent !== false) {
                if($indent < strlen($line)) {
                    if($indent > $indent_limit)
                        $indent = $indent_limit;
                    
                    $begin = substr($line, 0, $indent);
                }
                else
                    break;
            }
            else
                $begin = "";
            
            // find last space, tab or tag in line inside $limit
            $allowed = substr($line, 0, $limit);
            
            $space = strrpos($allowed, " ", $indent + 1);
            
            if($space === false)
                $space = 0;
            
            $tab = strrpos($allowed, "\t", $indent + 1);
            
            if($tab === false)
                $tab = 0;
            
            $tag = strrpos($allowed, "<", $indent + 1);
            
            if($tag === false)
                $tag = 0;
            else
                $tag--;
            
            $cut = max($space, $tab, $tag);
            
            if($cut > 0) {
                $out .= substr($line, 0, $cut + 1)."\n";
                
                $line = $begin.substr($line, $cut + 1);
            }
            else {
                // find last hyphen, comma or semicolon in line inside $limit
                $allowed = substr($line, 0, $limit);
                
                $hyphen = strrpos($allowed, "-", $indent + 1);
                
                if($space === false)
                    $space = 0;
                
                $comma = strrpos($allowed, ",", $indent + 1);
                
                if($tab === false)
                    $tab = 0;
                
                $semicolon = strrpos($allowed, ";", $indent + 1);
                
                if($tag === false)
                    $tag = 0;
                else
                    $tag--;
                
                $cut = max($hyphen, $comma, $semicolon);
                
                if($cut > 0) {
                    $out .= substr($line, 0, $cut + 1)."\n";
                    
                    $line = $begin.substr($line, $cut + 1);
                }
                else {
                    // find first space, tab or tag outside $limit
                    $l = strlen($line);
                    
                    $space = strpos($line, " ", $limit);
                    
                    if($space === false)
                        $space = $l;
                    
                    $tab = strpos($line, "\t", $limit);
                    
                    if($tab === false)
                        $tab = $l;
                    
                    $tag = strpos($line, "<", $limit);
                    
                    if($tag === false)
                        $tag = $l;
                    else
                        $tag--;
                    
                    $cut = min($space, $tab, $tag);
                    
                    if($cut < $l) {
                        $out .= substr($line, 0, $cut + 1)."\n";
                        
                        $line = $begin.substr($line, $cut + 1);
                    }
                    else {
                        // try to break at hyphen or comma
                        $hyphen = strpos($line, "-", $limit);
                        
                        if($hyphen === false)
                            $hyphen = $l;
                        
                        $comma = strpos($line, ",", $limit);
                        
                        if($comma === false)
                            $comma = $l;
                        
                        $cut = min($hyphen, $comma);
                        
                        if($cut < $l) {
                            $out .= substr($line, 0, $cut + 1)."\n";
                            
                            $line = $begin.substr($line, $cut + 1);
                        }
                        else {
                            $out .= $line."\n";
                            
                            $line = "";
                        }
                    }
                }
            }
        }
        
        $out .= $line."\n";
    }
    
    return $out."\n";
}

// function to check for JSON code
function isJSON($str) {
    if($str[0] == '{' || $str[0] == "[") {
        json_decode($str);
        
        return json_last_error() == JSON_ERROR_NONE;
    }
    
    return false;
}

$is404 = false;

if(isset($_POST["website"]))
    $website = $_POST["website"];

if(isset($_POST["urllist"]))
    $urllist = $_POST["urllist"];

if(isset($_POST["url"]) && isset($website) && $website && isset($urllist) && $urllist) {
    // get namespace of website and whether it is cross-domain
    $result = $dbConnection->query(
            "SELECT namespace, domain".
            " FROM crawlserv_websites".
            " WHERE id=$website".
            " LIMIT 1"
    );
    
    if(!$result)
        die("ERROR: Could not get namespace of website.");
    
    $row = $result->fetch_assoc();
    
    if(!$row)
        die("ERROR: Could not get namespace of website.");
    
    $namespace = $row["namespace"];
    $crossDomain = is_null($row["domain"]);
    
    $result->close();
    
    // get namespace of URL list
    $result = $dbConnection->query(
            "SELECT namespace".
            " FROM crawlserv_urllists".
            " WHERE website=$website".
            " AND id=$urllist".
            " LIMIT 1"
    );
    
    if(!$result)
        die("ERROR: Could not get namespace of URL list.");
    
    $row = $result->fetch_assoc();
    
    if(!$row)
        die("ERROR: Could not get namespace of URL list.");
    
    $urllistNamespace = $row["namespace"];
    
    $result->close();
    
    if(isset($_POST["last"])) {
        // search for last URL with content, starting from $url
        $result = $dbConnection->query(
                "SELECT a.id AS id, a.url AS url".
                " FROM `crawlserv_".$namespace."_".$urllistNamespace."` AS a,".
                " `crawlserv_".$namespace."_".$urllistNamespace."_crawled` AS b".
                " WHERE a.id = b.url".
                " AND a.id < ".$_POST["url"].
                " ORDER BY a.id DESC".
                " LIMIT 1"
        );
        
        if(!$result)
            die("ERROR: Could not search for URL.");
        
        $row = $result->fetch_assoc();
        
        if($row) {
            $url = $row["id"];
            $urltext = $row["url"];
        }
        else {
            // search for last URL with content in general
            $result = $dbConnection->query(
                    "SELECT a.id AS id, a.url AS url".
                    " FROM `crawlserv_".$namespace."_".$urllistNamespace."` AS a,".
                    " `crawlserv_".$namespace."_".$urllistNamespace."_crawled` AS b".
                    " WHERE a.id = b.url".
                    " ORDER BY a.id DESC".
                    " LIMIT 1"
            );
            
            if(!$result)
                die("ERROR: Could not search for last URL.");
            
            $row = $result->fetch_assoc();
            
            if($row) {
                $url = $row["id"];
                $urltext = $row["url"];
            }
            else
                $url = 0;
        }
        
        $result->close();
    }
    else {
        // search for next URL with content, starting from $url-1
        $result = $dbConnection->query("SELECT a.id AS id, a.url AS url FROM `crawlserv_".$namespace."_".$urllistNamespace
            ."` AS a, `crawlserv_".$namespace."_".$urllistNamespace."_crawled` AS b WHERE a.id = b.url AND a.id >= ".$_POST["url"]
            ." ORDER BY a.id LIMIT 1");
        if(!$result) die("ERROR: Could not search for URL.");
        $row = $result->fetch_assoc();
        if($row) {
            $url = $row["id"];
            $urltext = $row["url"];
        }
        else $url = 0;
        $result->close();
    }
}

if((!isset($_POST["url"]) || !$url) && isset($_POST["urltext"]) && isset($website) && $website && isset($urllist) && $urllist) {
    // get namespace of website and whether it is cross-domain
    $result = $dbConnection->query(
            "SELECT namespace, domain ".
            " FROM crawlserv_websites".
            " WHERE id=$website".
            " LIMIT 1"
    );
    
    if(!$result)
        die("ERROR: Could not get namespace of website.");
    
    $row = $result->fetch_assoc();
    
    if(!$row)
        die("ERROR: Could not get namespace of website.");
    
    $namespace = $row["namespace"];
    $crossDomain = is_null($row["domain"]);
    
    $result->close();
    
    // get namespace of URL list
    $result = $dbConnection->query(
            "SELECT namespace".
            " FROM crawlserv_urllists".
            " WHERE website=$website".
            " AND id=$urllist".
            " LIMIT 1"
    );
    
    if(!$result)
        die("ERROR: Could not get namespace of URL list.");
    
    $row = $result->fetch_assoc();
    
    if(!$row)
        die("ERROR: Could not get namespace of URL list.");
    
    $urllistNamespace = $row["namespace"];
    
    $result->close();
    
    // search for matching URL with content
    $trimmedUrl = trim($_POST["urltext"]);
    
    if(!$crossDomain AND strlen($trimmedUrl) > 0 AND substr($trimmedUrl, 0, 1) == "/")
        $trimmedUrl = substr($trimmedUrl, 1);
    
    $sql = "SELECT a.id AS id, a.url AS url".
            " FROM `crawlserv_".$namespace."_".$urllistNamespace."` AS a,".
            " `crawlserv_".$namespace."_".$urllistNamespace."_crawled` AS b".
            " WHERE a.id = b.url".
            " AND a.url LIKE ?".
            " ORDER BY a.url".
            " LIMIT 1";
    
    $stmt = $dbConnection->prepare($sql);
    
    if(!$stmt)
        die("ERROR: Could not prepare SQL statement to search for URL");
    
    $argument = "";
    
    if(!$crossDomain)
        $argument = "/";
    
    $argument .= $trimmedUrl."%";
    
    $stmt->bind_param("s", $argument);
    
    if(!$stmt->execute())
        die("ERROR: Could not search for URL (execution failed.");
    
    $result = $stmt->get_result();
    
    if(!$result)
        die("ERROR: Could not search for URL (getting result failed.");
    
    $row = $result->fetch_assoc();
    
    if($row) {
        $url = $row["id"];
        $urltext = $row["url"];
    }
    else {
        $is404 = true;
        $url = 0;
        
        if($crossDomain)
            $urltext = $trimmedUrl;
        else
            $urltext = "/".$trimmedUrl;
    }
    
    $result->close();
}

if(isset($_POST["tab"]))
    $tab = $_POST["tab"];
else
    $tab = "parsed";

?>

<h2>Content<?php

echo "<span id=\"opt-mode\">\n";

if($tab == "crawled")
    echo "<b>crawled</b>";
else
    echo "<a href=\"#\" class=\"action-link post-redirect-tab\" data-m=\"$m\" data-tab=\"crawled\">crawled</a>\n";

echo " &middot; ";

if($tab == "parsed")
    echo "<b>parsed</b>";
else
    echo "<a href=\"#\" class=\"action-link post-redirect-tab\" data-m=\"$m\" data-tab=\"parsed\">parsed</a>\n";

echo " &middot; ";

if($tab == "extracted")
    echo "<b>extracted</b>";
else
    echo "<a href=\"#\" class=\"action-link post-redirect-tab\" data-m=\"$m\" data-tab=\"extracted\">extracted</a>\n";

echo " &middot; ";

if($tab == "analyzed")
    echo "<b>analyzed</b>";
else
    echo "<a href=\"#\" class=\"action-link post-redirect-tab\" data-m=\"$m\" data-tab=\"analyzed\">analyzed</a>\n";

echo "</span>\n";

?></h2>

<div class="content-block">

<?php

echo rowWebsiteSelect();

if($website)
    echo rowUrlListSelect();

?>

</div>

<div class="content-block">

<?php

if($website && $urllist) {
    if(!$is404 && (!isset($url) || !$url)) {
        // get namespace of website and whether it is cross-domain
        $result = $dbConnection->query(
                "SELECT namespace, domain".
                " FROM crawlserv_websites".
                " WHERE id=$website".
                " LIMIT 1"
        );
        
        if(!$result)
            die("ERROR: Could not get namespace of website.");
        
        $row = $result->fetch_assoc();
        
        if(!$row)
            die("ERROR: Could not get namespace of website.");
        
        $namespace = $row["namespace"];
        $crossDomain = is_null($row["domain"]);
        
        $result->close();
        
        // get namespace of URL list
        $result = $dbConnection->query(
                "SELECT namespace".
                " FROM crawlserv_urllists".
                " WHERE website=$website".
                " AND id=$urllist".
                " LIMIT 1"
        );
        
        if(!$result)
            die("ERROR: Could not get namespace of URL list.");
        
        $row = $result->fetch_assoc();
        
        if(!$row)
            die("ERROR: Could not get namespace of URL list.");
        
        $urllistNamespace = $row["namespace"];
        
        $result->close();
        
        // get first URL with content from URL list
        $result = $dbConnection->query(
                "SELECT a.id AS id, a.url AS url".
                " FROM `crawlserv_".$namespace."_".$urllistNamespace."` AS a,".
                " `crawlserv_".$namespace."_".$urllistNamespace."_crawled` AS b".
                " WHERE a.id = b.url".
                " ORDER BY a.id".
                " LIMIT 1"
        );
        
        if(!$result)
            die("ERROR: Could not search for URL.");
        
        $row = $result->fetch_assoc();
        
        if($row) {
            $url = $row["id"];
            $urltext = $row["url"];
        }
        else {
            $url = 0;
            $is404 = true;
        }
        
        $result->close();
    }
    
    echo "<button id=\"content-last\" data-m=\"$m\" data-tab=\"$tab\">&lt;</button>";
    
    echo "<input type=\"text\" id=\"content-url\" data-m=\"$m\" data-tab=\"$tab\" value=\"$url\" title=\"$url\" />\n";
    
    echo "<span id=\"content-slash\">/</span>";
    
    if($crossDomain)
        $displayedUrl = htmlspecialchars($urltext);
    else
        $displayedUrl = htmlspecialchars(substr($urltext, 1));
    
    echo "<input type=\"text\" id=\"content-url-text\" data-m=\"$m\" data-tab=\"$tab\" value=\""
        .$displayedUrl
        ."\" title=\""
        .$displayedUrl
        ."\" />";
    
    echo "<button id=\"content-next\" class=\"fs-insert-after\" data-m=\"$m\" data-tab=\"$tab\">&gt;</button>\n";
    
    if($is404)
        echo "<br><br><i>URL not found. Maybe it was not successfully crawled (yet).</i><br><br>\n";
    else {
       // show content dependent on tab
        switch($tab) {
            case "parsed":
                // parsed data
                $ctable = "crawlserv_".$namespace."_".$urllistNamespace."_crawled";
                
                // get parsing table
                $result = $dbConnection->query(
                        "SELECT id, name, updated".
                        " FROM crawlserv_parsedtables".
                        " WHERE website = $website".
                        " AND urllist = ".$urllist.
                        " ORDER BY id DESC"
                );
                
                if(!$result)
                    die("ERROR: Could not get parsing tables.");
                
                if($result->num_rows) {
                    $tables = array();
                    
                    while($row = $result->fetch_assoc())
                        $tables[] = array(
                                "id" => $row["id"],
                                "name" => $row["name"],
                                "updated" => $row["updated"]
                        );

                    $result->close();
                    
                    if(isset($_POST["version"])) {
                        // select specified parsing table
                        foreach($tables as $table) {
                            if($table["id"] == $_POST["version"]) {
                                $parsingtable = $table;
                                
                                break;
                            }
                        }
                    }
                    else {
                        // select newest parsing table with parsed data in it
                        foreach($tables as $table) {
                            $result = $dbConnection->query(
                                    "SELECT EXISTS(".
                                            "SELECT 1 FROM `$ctable` AS a,".
                                            " `crawlserv_".$namespace."_".$urllistNamespace."_parsed_".$table["name"]."`".
                                            " AS b".
                                            " WHERE a.id = b.content".
                                            " AND a.url=".$url.
                                    ")"
                            );
                            
                            if(!$result)
                                die("ERROR: Could not read parsing table.");
                            
                            $row = $result->fetch_row();
                            
                            if(!$row)
                                die("ERROR: Could not read parsing table.");
                            
                            $result->close();
                            
                            if($row[0]) {
                                $parsingtable = $table;
                                
                                break;
                            }
                        }
                        
                        if(!isset($parsingtable))
                            $parsingtable = $tables[0];
                    }
                    
                    if(!isset($parsingtable))
                        die("ERROR: Could not get parsing table.");
                        
                    // get column names
                    $ptable = "crawlserv_".$namespace."_".$urllistNamespace."_parsed_".$parsingtable["name"];
                    
                    $result = $dbConnection->query(
                            "SELECT COLUMN_NAME AS name".
                            " FROM INFORMATION_SCHEMA.COLUMNS".
                            " WHERE TABLE_SCHEMA LIKE 'crawled'".
                            " AND TABLE_NAME LIKE '$ptable'".
                            " AND COLUMN_NAME LIKE 'parsed_%'".
                            " ORDER BY REPLACE(REPLACE(name, '__', ''), 'parsed_id', '')"
                    );
                    
                    if(!$result) 
                        die("ERROR: Could not get columns from parsing table.");
                    
                    $columns = array();
                    
                    while($row = $result->fetch_assoc())
                        $columns[] = $row["name"];

                    $result->close();
                    
                    if(count($columns)) {
                        $query = "SELECT ";
                        
                        foreach($columns as $column)
                            $query .= "b.`".$column."`, ";
                        
                        $query .= "b.content";
                        
                        $query .=   " FROM `$ctable` AS a,".
                                    " `$ptable` AS b".
                                    " WHERE a.id = b.content".
                                    " AND a.url = $url".
                                    " ORDER BY b.id DESC".
                                    " LIMIT 1";
                        
                        $result = $dbConnection->query($query);
                        
                        if(!$result)
                            die("Could not get data from parsing table.");
                        
                        $row = $result->fetch_assoc();
                        
                        $result->close();
                        
                        if($row) {
                            $data = true;
                            
                            echo "<button id=\"content-fullscreen\" title=\"Show Fullscreen\">&#9727;</button>\n";
                            
                            echo "<div id=\"content-table\" class=\"fs-div\">\n";
                            
                            echo "<table class=\"fs-content\">\n";
                            echo "<thead>\n";
                            echo "<tr>\n";
                            
                            echo "<th>Field</th>\n";
                            echo "<th>Parsed value</th>\n";
                            
                            echo "</tr>\n";
                            echo "</thead>\n";
                            
                            echo "<tbody>\n";
                            
                            // show parsed values
                            foreach($columns as $column) {
                                if(strlen($column) > 8 && substr($column, 0, 8) == "parsed__")
                                    $cname = substr($column, 8);
                                else
                                    $cname = substr($column, 7);
                                
                                echo "<tr>\n";
                                echo "<td>".htmlspecialchars($cname)."</td>\n";
                                
                                echo "<td>\n";
                                
                                if(!strlen(trim($row[$column])))
                                    echo "<i>[empty]</i>";
                                else if(isJSON($row[$column])) {
                                    echo "<i>JSON</i><pre><code class=\"language-json\">\n";
                                    
                                    echo htmlspecialchars($row[$column])."\n\n";
                                    
                                    echo "</code></pre>\n";
                                }
                                else
                                    echo htmlspecialchars($row[$column])."\n";
                                
                                echo "</td>\n";
                                echo "</tr>\n";
                            }
                            
                            // show content ID of and link to the source for the parsed data 
                            echo "<tr>\n";
                            echo "<td>[source]</td>\n";
                            echo "<td>\n";
                            
                            echo "<a href=\"#\" id=\"content-goto\" class=\"action-link\" data-m=\"$m\" data-version=\""
                                    .$row["content"]
                                    ."\">";
                            echo "#".$row["content"];
                            echo "</a>\n";
                            
                            echo "</td>\n";
                            echo "</tr>\n";
                            
                            echo "</tbody>\n";
                            echo "</table>\n";
                            
                            echo "</div>\n";
                        }
                        else {
                            $data = false;
                            
                            echo "<br><br><i>No parsing data available for this specific URL.</i><br><br>\n";
                        }
                        
                        echo "<div id=\"content-status\">\n";
                        
                        echo "<select id=\"content-version\" class=\"wide\" data-m=\"$m\" data-tab=\"$tab\">\n";
                        
                        $count = 0;
                        $num = sizeof($tables);
                        
                        foreach($tables as $table) {
                            $count++;
                            
                            echo "<option value=\"".$table["id"]."\"";
                            
                            if($table["id"] == $parsingtable["id"])
                                echo " selected";
                            
                            echo ">Table ".number_format($count)." of ".number_format($num).": '".$table["name"]."'";
                            
                            if($table["updated"])
                                echo " &ndash; last updated on ".$table["updated"];
                            
                            echo "</option>\n";
                        }
                        
                        echo "</select>\n";
                        
                        if($data) {
                            echo "<span id=\"content-info\">\n";
                            
                            echo "<a href=\"#\" id=\"content-download\" target=\"_blank\""
                                    ."data-type=\"parsed\" data-website-namespace=\""
    
                                    .htmlspecialchars($namespace)
                                    
                                    ."\" data-namespace=\""
        
                                    .htmlspecialchars($urllistNamespace)
                                    
                                    ."\" data-version=\""
        
                                    .$parsingtable["id"]
                                    
                                    ."\" data-filename=\""
            
                                    .htmlspecialchars(
                                            $namespace."_"
                                            .$urllistNamespace
                                            ."_"
                                            .$url
                                    )
                                    
                                    ."_parsed.json\">[Download as JSON]</a>\n";
                            
                            echo "</span>\n";
                        }
                        
                        echo "</div>\n";
                    }
                    else
                        echo "<br><br><i>No parsing data available for this website.</i><br><br>\n";
                }
                else {
                    $result->close();
                    
                    echo "<br><br><i>No parsing data available for this website.</i><br><br>\n";
                }
                
                break;
                
            case "extracted":
                // extracted data
                $ctable = "crawlserv_".$namespace."_".$urllistNamespace."_extracted";
                
                // get extracting table
                $result = $dbConnection->query(
                        "SELECT id, name, updated".
                        " FROM crawlserv_extractedtables".
                        " WHERE website = $website".
                        " AND urllist = ".$urllist.
                        " ORDER BY id DESC"
                );
                
                if(!$result)
                    die("ERROR: Could not get extracting tables.");
                    
                    if($result->num_rows) {
                        $tables = array();
                        
                        while($row = $result->fetch_assoc())
                            $tables[] = array(
                                "id" => $row["id"],
                                "name" => $row["name"],
                                "updated" => $row["updated"]
                            );
                            
                        $result->close();
                        
                        if(isset($_POST["version"])) {
                            // select specified extracting table
                            foreach($tables as $table) {
                                if($table["id"] == $_POST["version"]) {
                                    $extractingtable = $table;
                                    
                                    break;
                                }
                            }
                        }
                        else {
                            // select newest extracting table with extracted data in it
                            foreach($tables as $table) {
                                $result = $dbConnection->query(
                                        "SELECT EXISTS(".
                                        "SELECT 1 FROM `$ctable` AS a,".
                                        " `crawlserv_".$namespace."_".$urllistNamespace."_extracted_".$table["name"]."`".
                                        " AS b".
                                        " WHERE a.id = b.content".
                                        " AND a.url=".$url.
                                        ")"
                                );
                                
                                if(!$result)
                                    die("ERROR: Could not read extracting table.");
                                    
                                $row = $result->fetch_row();
                                
                                if(!$row)
                                    die("ERROR: Could not read extracting table.");
                                    
                                $result->close();
                                
                                if($row[0]) {
                                    $extractingtable = $table;
                                    
                                    break;
                                }
                            }
                            
                            if(!isset($extractingtable))
                                $extractingtable = $tables[0];
                        }
                        
                        if(!isset($extractingtable))
                            die("ERROR: Could not get extracting table.");
                            
                        // get column names
                        $etable = "crawlserv_".$namespace."_".$urllistNamespace."_extracted_".$extractingtable["name"];
                        
                        $result = $dbConnection->query(
                                "SELECT COLUMN_NAME AS name".
                                " FROM INFORMATION_SCHEMA.COLUMNS".
                                " WHERE TABLE_SCHEMA = 'crawled'".
                                " AND TABLE_NAME = N'$etable'"
                        );
                        
                        if(!$result)
                            die("ERROR: Could not get columns from extracting table.");
                            
                        $columns = array();
                        
                        while($row = $result->fetch_assoc())
                            if(strlen($row["name"]) > 7 && substr($row["name"], 0, 7) == "extracted_")
                                $columns[] = $row["name"];
                                
                        $result->close();
                        
                        if(count($columns)) {
                            $query = "SELECT ";
                            
                            foreach($columns as $column)
                                $query .= "b.`".$column."`, ";
                                
                                $query = substr($query, 0, -2);
                                
                                $query .=   " FROM `$ctable` AS a,".
                                    " `$etable` AS b".
                                    " WHERE a.id = b.content".
                                    " AND a.url = $url".
                                    " ORDER BY b.id DESC".
                                    " LIMIT 1";
                                
                                $result = $dbConnection->query($query);
                                
                                if(!$result)
                                    die("Could not get data from extracting table.");
                                    
                                    $row = $result->fetch_assoc();
                                    
                                    $result->close();
                                    
                                    if($row) {
                                        $data = true;
                                        
                                        echo "<button id=\"content-fullscreen\" title=\"Show Fullscreen\">&#9727;</button>\n";
                                        
                                        echo "<div id=\"content-table\" class=\"fs-div\">\n";
                                        
                                        echo "<table class=\"fs-content\">\n";
                                        echo "<thead>\n";
                                        echo "<tr>\n";
                                        
                                        echo "<th>Field</th>\n";
                                        echo "<th>Extracted value</th>\n";
                                        
                                        echo "</tr>\n";
                                        echo "</thead>\n";
                                        
                                        echo "<tbody>\n";
                                        
                                        foreach($columns as $column) {
                                            if(strlen($column) > 8 && substr($column, 0, 8) == "extracted__")
                                                $cname = substr($column, 8);
                                                else
                                                    $cname = substr($column, 7);
                                                    
                                                    echo "<tr>\n";
                                                    echo "<td>".htmlspecialchars($cname)."</td>\n";
                                                    
                                                    echo "<td>\n";
                                                    
                                                    if(!strlen(trim($row[$column])))
                                                        echo "<i>[empty]</i>";
                                                        else if(isJSON($row[$column])) {
                                                            echo "<i>JSON</i><pre><code class=\"language-json\">\n";
                                                            
                                                            echo htmlspecialchars($row[$column])."\n\n";
                                                            
                                                            echo "</code></pre>\n";
                                                        }
                                                        else
                                                            echo htmlspecialchars($row[$column])."\n";
                                                            
                                                            echo "</td>\n";
                                                            echo "</tr>\n";
                                        }
                                        
                                        echo "</tbody>\n";
                                        echo "</table>\n";
                                        
                                        echo "</div>\n";
                                    }
                                    else {
                                        $data = false;
                                        
                                        echo "<br><br><i>No extracting data available for this specific URL.</i><br><br>\n";
                                    }
                                    
                                    echo "<div id=\"content-status\">\n";
                                    
                                    echo "<select id=\"content-version\" class=\"wide\" data-m=\"$m\" data-tab=\"$tab\">\n";
                                    
                                    $count = 0;
                                    $num = sizeof($tables);
                                    
                                    foreach($tables as $table) {
                                        $count++;
                                        
                                        echo "<option value=\"".$table["id"]."\"";
                                        
                                        if($table["id"] == $extractingtable["id"])
                                            echo " selected";
                                            
                                            echo ">Table ".number_format($count)." of ".number_format($num).": '".$table["name"]."'";
                                            
                                            if($table["updated"])
                                                echo " &ndash; last updated on ".$table["updated"];
                                                
                                                echo "</option>\n";
                                    }
                                    
                                    echo "</select>\n";
                                    
                                    if($data) {
                                        echo "<span id=\"content-info\">\n";
                                        
                                        echo "<a href=\"#\" id=\"content-download\" target=\"_blank\""
                                             ."data-type=\"extracted\" data-website-namespace=\""
                            
                                             .htmlspecialchars($namespace)
                                             
                                             ."\" data-namespace=\""
                        
                                             .htmlspecialchars($urllistNamespace)
                                             
                                             ."\" data-version=\""
                    
                                             .$extractingtable["id"]
                                             
                                             ."\" data-filename=\""
                            
                                             .htmlspecialchars(
                                                    $namespace."_"
                                                    .$urllistNamespace
                                                    ."_"
                                                    .$url
                                             )
                                             
                                             ."_extracted.json\">[Download as JSON]</a>\n";
                                
                                echo "</span>\n";
                                    }
                                    
                                    echo "</div>\n";
                        }
                        else
                            echo "<br><br><i>No extracting data available for this website.</i><br><br>\n";
                    }
                    else {
                        $result->close();
                        
                        echo "<br><br><i>No extracting data available for this website.</i><br><br>\n";
                    }
                    
                    break;
                
            case "analyzed":
                // analyzed data
                // TODO
                
                echo "<br><br><i>This feature is not implemented yet.<br><br></i>\n";
                
                break;
                
            default:
                // crawled data
                echo "<button id=\"content-fullscreen\" title=\"Show Fullscreen\">&#9727;</button>\n";
                
                echo "<div id=\"content-text\" class=\"fs-div\">\n";
                
                echo "<pre class=\"fs-content\"><code id=\"content-code\" class=\"language-html fs-content\">\n";
                
                if(isset($_POST["version"])) {
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
        }
    }
}

?>

</div>
