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
 * parsed.php
 *
 * Show parsed data of currently selected URL and version.
 *
 */

// parsed data
$ctable = "crawlserv_".$namespace."_".$urllistNamespace."_crawled";

// get parsing table
$result = $dbConnection->query(
        "SELECT id, name, updated
         FROM `crawlserv_parsedtables`
         WHERE website = $website
         AND urllist = $urllist
         ORDER BY id DESC"
);

if(!$result) {
    die("ERROR: Could not get parsing tables.");
}
    
if($result->num_rows) {
    $tables = array();
    
    while($row = $result->fetch_assoc()) {
        $tables[] = array(
            "id" => $row["id"],
            "name" => $row["name"],
            "updated" => $row["updated"]
        );
    }
        
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
                    "SELECT EXISTS(
                            SELECT 1 FROM `$ctable` AS a,
                             `crawlserv_"
                             .$namespace
                             ."_"
                             .$urllistNamespace
                             ."_parsed_"
                             .$table["name"]."`
                             AS b
                             WHERE a.id = b.content
                             AND a.url=$url
                    )"
            );
            
            if(!$result) {
                die("ERROR: Could not read parsing table.");
            }
                
            $row = $result->fetch_row();
            
            if(!$row) {
                die("ERROR: Could not read parsing table.");
            }
                
            $result->close();
            
            if($row[0]) {
                $parsingtable = $table;
                
                break;
            }
        }
        
        if(!isset($parsingtable)) {
            $parsingtable = $tables[0];
        }
    }
    
    if(!isset($parsingtable)) {
        die("ERROR: Could not get parsing table.");
    }
        
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
    
    if(!$result) {
        die("ERROR: Could not get columns from parsing table.");
    }
        
    $columns = array();
    
    while($row = $result->fetch_assoc()) {
        $columns[] = $row["name"];
    }
        
    $result->close();
    
    if(count($columns)) {
        $query = "SELECT ";
        
        foreach($columns as $column) {
            $query .= "b.`".$column."`, ";
        }
            
        $query .= "b.content";
        
        $query .=   " FROM `$ctable` AS a,".
            " `$ptable` AS b".
            " WHERE a.id = b.content".
            " AND a.url = $url".
            " ORDER BY b.id DESC".
            " LIMIT 1";
        
        $result = $dbConnection->query($query);
        
        if(!$result) {
            die("ERROR: Could not get data from parsing table.");
        }
            
        $row = $result->fetch_assoc();
        
        $result->close();
        
        if($row) {
            $data = true;
            
            echo "<button id=\"content-fullscreen\" title=\"Show Fullscreen\">&#9727;</button>\n";
            
            echo "<div id=\"content-table\" class=\"fs-div\">\n";
            
            echo "<table class=\"fs-content\">\n";
            echo "<thead>\n";
            echo "<tr>\n";
            
            echo "<th class=\"content-field\">Field</th>\n";
            echo "<th>Parsed value</th>\n";
            
            echo "</tr>\n";
            echo "</thead>\n";
            
            echo "<tbody>\n";
            
            $parsed_id = $row["parsed_id"];
            
            // show parsed values
            foreach($columns as $column) {
                if(strlen($column) > 8 && substr($column, 0, 8) == "parsed__") {
                    $cname = substr($column, 8);
                }
                else {
                    $cname = substr($column, 7);
                }
                    
                echo "<tr>\n";
                echo "<td>".html($cname)."</td>\n";
                
                echo "<td>\n";
                
                if(!strlen(trim($row[$column]))) {
                    echo "<i>[empty]</i>";
                }
                else if(isJSON($row[$column])) {
                    echo "<i>JSON</i><pre><code class=\"language-json\">\n";
                    
                    echo html($row[$column])."\n\n";
                    
                    echo "</code></pre>\n";
                }
                else {
                    echo html($row[$column])."\n";
                }
                    
                echo "</td>\n";
                echo "</tr>\n";
            }
            
            // show content ID of and link to the source for the parsed data
            echo "<tr>\n";
            echo "<td>[source]</td>\n";
            echo "<td>\n";
            
            echo "<a href=\"#\"
                id=\"content-goto\"
                class=\"action-link\"
                data-m=\"$m\"
                data-version=\""
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
    }
    else {
        echo "<br><br><i>No parsing data available for this website.</i><br><br>\n";
    }
    
    // show tables (aka versions of parsed data)
    echo "<div id=\"content-status\">\n";
    
    echo "<select id=\"content-version\" class=\"wide\" data-m=\"$m\" data-tab=\"$tab\">\n";
    
    $count = 0;
    $num = sizeof($tables);
    
    foreach($tables as $table) {
        $count++;
        
        echo "<option value=\"".$table["id"]."\"";
        
        if($table["id"] == $parsingtable["id"]) {
            echo " selected";
        }
        
        echo ">Table ".number_format($count)." of ".number_format($num).": '".$table["name"]."'";
        
        if($table["updated"]) {
            echo " &ndash; last updated on ".$table["updated"];
        }
        
        echo "</option>\n";
    }
    
    echo "</select>\n";
    
    // show optional link to JSON data
    if(isset($data) && $data) {
        echo "<span id=\"content-info\">\n";
        
        echo "<a href=\"#\" id=\"content-download\" target=\"_blank\"
                data-type=\"parsed\" data-website-namespace=\""
                .html($namespace)
                ."\" data-namespace=\""
                .html($urllistNamespace)
                ."\" data-version=\""
                .$parsingtable["id"]
                ."\" data-filename=\""
                .html(
                        $namespace."_"
                        .$urllistNamespace
                        ."_"
                        .$url
                )
                ."_parsed.json\">[Download as JSON]</a>\n";
                            
        echo "</span>\n";
    }
    
    echo "</div>\n";
    
    // show function to search for parsed ID
    echo "</div>\n";
    
    echo "<div class=\"content-block\">\n";
    
    echo "<div class=\"entry-row\">\n";
    
    echo "<div class=\"entry-label\">\n";
    echo "Parsed ID:";
    echo "</div>\n";
    
    echo "<div class=\"entry-input\"";
    
    if(isset($parsed_id)) {
        echo " value=\"".html($parsed_id)."\"";
    }
    
    echo ">\n";
    echo "<input id=\"content-parsed-id\" class=\"trigger\" type=\"text\" data-trigger=\"content-search-parsed\" />\n";
    echo    "<a
                href=\"#\"
                id=\"content-search-parsed\"
                class=\"action-link-inline\"
                data-m=\"$m\"
                data-tab=\"$tab\"
                >Go
                </a>\n";
    echo "</div>\n";
    
    echo "</div>";
}
else {
    $result->close();
    
    echo "<br><br><i>No parsing data available for this website.</i><br><br>\n";
}

?>
