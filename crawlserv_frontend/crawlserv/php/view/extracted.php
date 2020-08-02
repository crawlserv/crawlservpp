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
 * extracted.php
 *
 * Show extracted data of the currently selected URL and version.
 *
 */

$ctable = "crawlserv_".$namespace."_".$urllistNamespace."_crawled";

// get extracting table
$result = $dbConnection->query(
        "SELECT id, name, updated".
        " FROM crawlserv_extractedtables".
        " WHERE website = $website".
        " AND urllist = ".$urllist.
        " ORDER BY id DESC"
);

if(!$result) {
    die("ERROR: Could not get extracting tables.");
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
            $fullTableName = 
                    "crawlserv_"
                    .$namespace
                    ."_"
                    .$urllistNamespace
                    ."_extracted_"
                    .$table["name"];
            $linked = false;
            
            // only show tables that are content-dependent
            $result = $dbConnection->query(
                "SELECT 1".
                " FROM INFORMATION_SCHEMA.COLUMNS".
                " WHERE TABLE_SCHEMA LIKE 'crawled'".
                " AND TABLE_NAME LIKE '$fullTableName'".
                " AND COLUMN_NAME LIKE 'content'".
                " LIMIT 1"
            );
            
            if(!$result) {
                die("ERROR: Could not check extracting table.");
            }
            
            if(!($result->fetch_row())) {
                $linked = true;
            }
            
            $result->close();
            
            if($linked) {
                continue;
            }
            
            $result = $dbConnection->query(
                    "SELECT EXISTS(
                            SELECT 1
                            FROM `$ctable`
                            AS a,
                             `$fullTableName`
                            AS b
                            WHERE a.id = b.content
                            AND a.url = $url
                    )"
            );
            
            if(!$result) {
                die("ERROR: Could not read extracting table.");
            }
                
            $row = $result->fetch_row();
            
            if(!$row) {
                die("ERROR: Could not read extracting table.");
            }
                
            $result->close();
            
            if($row[0]) {
                $extractingtable = $table;
                
                break;
            }
        }
    }
    
    if(isset($extractingtable)) {
        // get column names
        $etable = "crawlserv_"
                        .$namespace
                        ."_"
                        .$urllistNamespace
                        ."_extracted_"
                        .$extractingtable["name"];
        
        $result = $dbConnection->query(
                "SELECT COLUMN_NAME AS name
                 FROM INFORMATION_SCHEMA.COLUMNS
                 WHERE TABLE_SCHEMA LIKE 'crawled'
                 AND TABLE_NAME LIKE '$etable'"
        );
        
        if(!$result) {
            die("ERROR: Could not get columns from extracting table.");
        }
            
        $columns = array();
        $linked = false;
        $content = false;
        
        while($row = $result->fetch_assoc()) {        
            if(
                    strlen($row["name"]) > 10
                    && substr($row["name"], 0, 10) == "extracted_"
            ) {
                $columns[] = $row["name"];
            }
            else if($row["name"] == "link") {
                $linked = true;
            }
            else if($row["name"] == "content") {
                $content = true;
            }
        }
                
        $result->close();
        
        if(!$content) {
            echo "<br><br><i>This table does not contain URL-specific data.</i><br><br>\n";
        }
        else if(count($columns)) {
            if($linked) {
                // get reference for linked data
                $rtable = "";
                
                $result = $dbConnection->query("SHOW CREATE TABLE $etable");
                
                if(!$result) {
                    die("ERROR: Could not get constraints from extracting table.");
                }
                
                $row = $result->fetch_row();
                
                $result->close();
                
                if(!$row || count($row) < 2) {
                    die("ERROR: Could not get constraints from extracting table.");
                }
                
                $tableInfo = $row[1];
                
                $len = strlen($tableInfo);
                $pos = 0;
                
                while($pos < $len) {
                    $pos = strpos($tableInfo, "FOREIGN KEY (`", $pos);
                    
                    if($pos === FALSE) {
                        break;
                    }
                    
                    $pos += 14;
                    
                    $end = strpos($tableInfo, "`)", $pos);
                    
                    if($end === FALSE) {
                        die("ERROR: Unexpected end of table information when getting constraints from extracting table.");
                    }
                    
                    if(substr($tableInfo, $pos, $end - $pos) != "link") {
                        continue;
                    }
                    
                    $pos = $end + 2;
                    
                    break;
                }
                
                if($pos !== FALSE && $pos < $len) {
                    if(substr($tableInfo, $pos, 13) != " REFERENCES `") {
                        die("ERROR: Could not identify referenced table when getting constraints from extracting table.");
                    }
                    
                    $pos += 13;
                    
                    $end = strpos($tableInfo, "`", $pos);
                    
                    if($end === FALSE) {
                        die("ERROR: Unexpected end of table information when getting referenced table from extracting table.");
                    }
                    
                    $rtable = substr($tableInfo, $pos, $end - $pos);
                    
                    $linked = strlen($rtable) > 0;
                }
            }
            
            // get number of datasets
            $query = "SELECT COUNT(*)".
                " FROM `$ctable` AS a,".
                " `$etable` AS b".
                " WHERE a.id = b.content".
                " AND a.url = $url";
            
            $result = $dbConnection->query($query);
            
            if(!$result) {
                die("ERROR: Could not get number of datasets from extracting table.");
            }
            
            $row = $result->fetch_row();
            
            $result->close();
            
            if(!$row) {
                die("ERROR: Could not get number of datasets from extracting table (empty result).");
            }
            
            $numdata = $row[0];
            
            if($numdata > 0) {
                // get offset
                if(isset($_POST["offset"])) {
                    $offset = $_POST["offset"];
                    
                    if($offset < 0) {
                        $offset = $numdata + $offset;
                    }
                    
                    while($offset >= $numdata) {
                        $offset -= $numdata;
                    }
                }
                else {
                    $offset = 0;
                }
                
                // show dataset navigation
                $currentdata = $offset + 1;
                
                echo "<button class=\"content-dataset-to\" data-m=\"$m\" data-tab=\"$tab\" data-to=\"0\">|&lt;";
                echo "</button>";
                echo "<button class=\"content-dataset-to\" data-m=\"$m\" data-tab=\"$tab\" data-to=\"".($offset - 1)."\">&lt;";
                echo "</button>";
                
                echo "<span class=\"content-smalltext\"> Dataset </span>";
                
                echo "<input type=\"text\" id=\"content-dataset\" data-m=\"$m\" data-tab=\"$tab\"";
                echo " data-max=\"$numdata\" value=\"$currentdata\" />\n";
                
                echo "<span class=\"content-smalltext\"> of ".number_format($numdata)." </span>";
                
                echo "<button class=\"content-dataset-to\" data-m=\"$m\" data-tab=\"$tab\" data-to=\"".($offset + 1)."\">&gt;";
                echo "</button>";
                echo "<button class=\"content-dataset-to\" data-m=\"$m\" data-tab=\"$tab\" data-to=\"".($numdata - 1)."\">&gt;|";
                echo "</button>";

                echo "<span class=\"fs-insert-after\"></span>";
                
                // get dataset
                $query = "SELECT ";
                
                foreach($columns as $column) {
                    $query .= "b.`".$column."`, ";
                }
                    
                $query = substr($query, 0, -2);
                
                if($linked) {
                    $query .= ", link";
                }
                
                $query .=   " FROM `$ctable` AS a,".
                    " `$etable` AS b".
                    " WHERE a.id = b.content".
                    " AND a.url = $url".
                    " ORDER BY b.id DESC".
                    " LIMIT 1";
                
                if($offset > 0) {
                    $query .= " OFFSET $offset";
                }
                
                $result = $dbConnection->query($query);
                
                if(!$result) {
                    die("ERROR: Could not get current dataset from extracting table.");
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
                    echo "<th>Extracted value</th>\n";
                    
                    echo "</tr>\n";
                    echo "</thead>\n";
                    
                    echo "<tbody>\n";
                    
                    foreach($columns as $column) {
                        if(
                                strlen($column) > 11
                                && substr($column, 0, 11) == "extracted__"
                        ) {
                            $cname = substr($column, 11);
                        }
                        else {
                            $cname = substr($column, 10);
                        }
                            
                        echo "<tr>\n";
                        echo "<td>".html($cname)."</td>\n";
                        
                        echo "<td>\n";
                        
                        if(!strlen(trim($row[$column]))) {
                            echo "<i>[empty]</i>\n";
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
                    
                    echo "</tbody>\n";
                    echo "</table>\n";
                    
                    // show linked data if it exists
                    
                    if($linked) {
                        if($row["link"] !== NULL) {
                            echo "<p><br>Linked to dataset #";
                            echo $row["link"];
                            echo " from table '";
                            
                            // get table name from crawlserv_$namespace_$urllistNamespace_extracted_TABLE
                            echo html(substr($rtable, 22 + strlen($namespace) + strlen($urllistNamespace)));
                            
                            echo "':</p>\n";
                            
                            $result = $dbConnection->query(
                                "SELECT * FROM `$rtable` WHERE id=".$row["link"]." LIMIT 1"
                            );
                            
                            if($result) {
                                $row = $result->fetch_assoc();
                                
                                if($row) {
                                    echo "<table class=\"fs-content\">\n";
                                    echo "<thead>\n";
                                    echo "<tr>\n";
                                    
                                    echo "<th class=\"content-field\">Field</th>\n";
                                    echo "<th>Extracted value</th>\n";
                                    
                                    echo "</tr>\n";
                                    echo "</thead>\n";
                                    
                                    echo "<tbody>\n";
                                    
                                    foreach($row as $key => $value) {
                                        if(
                                            strlen($key) > 10
                                            && substr($key, 0, 10) == "extracted_"
                                        ) {
                                                if(
                                                    strlen($key) > 11
                                                    && $key[10] == "_"
                                                    ) {
                                                        $cname = substr($key, 11);
                                                    }
                                                    else {
                                                        $cname = substr($key, 10);
                                                    }
                                                    
                                                    echo "<tr>\n";
                                                    echo "<td>".html($cname)."</td>\n";
                                                    
                                                    echo "<td>\n";
                                                    
                                                    if(!strlen(trim($value))) {
                                                        echo "<i>[empty]</i>\n";
                                                    }
                                                    else if(isJSON($value)) {
                                                        echo "<i>JSON</i><pre><code class=\"language-json\">\n";
                                                        
                                                        echo html($value)."\n\n";
                                                        
                                                        echo "</code></pre>\n";
                                                    }
                                                    else {
                                                        echo html($value)."\n";
                                                    }
                                                    
                                                    echo "</td>\n";
                                                    echo "</tr>\n";
                                            }
                                    }
                                    
                                    echo "</tbody>\n";
                                    echo "</table>\n";
                                }
                                else {
                                    echo "<p><b>[N/A]</b></p>\n";
                                }
                            }
                            else {
                                echo "<p><b>[N/A]</b></p>\n";
                            }
                        }
                    }
                    
                    echo "</div>\n"; // id="content-table"
                }
                else {
                    echo "<br><br><i>This dataset does not exist.</i><br><br>\n";
                }
            }
            else {
                $data = false;
                
                echo "<br><br><i>No data available for this specific URL.</i><br><br>\n";
            }
        }
        else {
            echo "<br><br><i>No extracted data available for this specific URL.</i><br><br>\n";
        }
        
        echo "<div id=\"content-status\">\n";
        
        echo "<select id=\"content-version\" class=\"wide\" data-m=\"$m\" data-tab=\"$tab\">\n";
        
        $count = 0;
        $num = sizeof($tables);
        
        foreach($tables as $table) {
            $count++;
            
            echo "<option value=\"".$table["id"]."\"";
            
            if($table["id"] == $extractingtable["id"]) {
                echo " selected";
            }
            
            echo ">Table ";
            echo number_format($count);
            echo " of ";
            echo number_format($num);
            echo ": '";
            echo $table["name"];
            echo "'";
            
            if($table["updated"]) {
                echo " &ndash; last updated on ".$table["updated"];
            }
            
            echo "</option>\n";
        }
        
        echo "</select>\n";
        
        if(isset($data) && $data) {
            echo "<span id=\"content-info\">\n";
            
            echo "<a href=\"#\" id=\"content-download\" target=\"_blank\""
                    ."data-type=\"extracted\" data-website-namespace=\""
                    .html($namespace)
                    ."\" data-namespace=\""
                    .html($urllistNamespace)
                    ."\" data-version=\""
                    .$extractingtable["id"]
                    ."\" data-filename=\""
                    .html(
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
    else {
        echo "<br><br><i>No extracted data available.</i><br><br>\n";
    }
}
else {
    $result->close();
    
    echo "<br><br><i>No extracted data available for this website.</i><br><br>\n";
}

?>
