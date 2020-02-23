
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
 * websites.php
 *
 * Show status of and manage the domains, websites and URL lists in the database.
 *
 */

$db_init = true;
$cc_init = true;

require "_helpers.php";
require "config.php";

if(isset($_POST["website"]))
    $website = $_POST["website"];

if(isset($_POST["urllist"]))
    $urllist = $_POST["urllist"];

if(isset($_POST["query"]))
    $query = $_POST["query"];

?>

<h2>Websites</h2>

<div class="content-block">

<?php

echo rowWebsiteSelect(true);

?>

<div class="action-link-box">
<div class="action-link">

<?php

if($website)
    echo "<a id=\"website-duplicate\" href=\"#\" class=\"action-link\">Duplicate configuration</a>";

?>

</div>
</div>
</div>

<div class="content-block">
<div class="entry-row">

<div class="entry-label">Name:</div><div class="entry-input">

<input type="text" class="entry-input" id="website-name" value="<?php

if($website)
    echo htmlspecialchars($websiteName);

?>" />

</div>
</div>

<div class="entry-row">
<div class="entry-label">Namespace:</div><div class="entry-input">

<input type="text" class="entry-input" id="website-namespace" value="<?php

if($website)
    echo htmlspecialchars($namespace);

?>" />

</div>
</div>

<div class="entry-row">
<div class="entry-label">Domain:</div><div class="entry-input">

<input id="website-domain" type="text" class="entry-input" value="<?php

if($website)
    echo htmlspecialchars($domain);

?>" />

</div>
</div>

<div class="entry-row">
<div class="entry-label"></div>
<div class="entry-input">

<input id="website-crossdomain" type="checkbox" <?php

if($website && !strlen($domain))
    echo "checked";

?> /> cross-domain website

</div>
</div>

<div class="entry-row">
<div class="entry-label"></div>
<div class="entry-input">

<input id="website-externaldir" type="checkbox" /> use external directory

</div>
</div>

<div class="entry-row">
<div class="entry-label">Directory:</div>
<div class="entry-input">

<input type="text" class="entry-input" id="website-dir" value="<?php

echo $website && strlen($dir) ? htmlspecialchars($dir) : "[default]";

?>" />

</div>
</div>

<div class="action-link-box">
<div class="action-link">

<?php

if($website)
    echo "<a id=\"website-update\" href=\"#\" class=\"action-link\">Change website</a>";
else
    echo "<a id=\"website-add\" href=\"#\" class=\"action-link\">Add website</a>";

?>

</div>
</div>
</div>

<?php

if($website) {    
    echo "<div class=\"content-block\">\n";
    
    echo rowUrlListSelect(true, true);
    
    echo "</div>\n";
    
    if(isset($urllist) && $urllist) {
        // get size of selected URL list
        $result = $dbConnection->query(
                "SELECT COUNT(*)".
                " FROM `crawlserv_".$namespace."_".$urllistNamespace."`"
        );
        
        if(!$result)
            die(
                    "ERROR: Could not get size of URL list from `crawlserv_"
                    .$namespace
                    ."_"
                    .$urllistNamespace
                    ."`"
            );
            
        $row = $result->fetch_row();
        
        if($row)
            $urllistSize = $row[0];
            
        $result->close();
        
        // get number of crawled URLs
        $result = $dbConnection->query(
                "SELECT COUNT(DISTINCT url)".
                " FROM `crawlserv_".$namespace."_".$urllistNamespace."_crawling`".
                " WHERE success = TRUE"
        );
        
        if(!$result)
            die(
                    "ERROR: Could not get number of crawled URLs from `crawlserv_"
                    .$namespace
                    ."_"
                    .$urllistNamespace
                    ."_crawling`"
            );
            
        $row = $result->fetch_row();
            
        if($row)
            $urllistCrawled = $row[0];
                
        $result->close();
                
        // get number of parsed URLs
        $result = $dbConnection->query(
                "SELECT COUNT(DISTINCT url)".
                " FROM `crawlserv_".$namespace."_".$urllistNamespace."_parsing`".
                " WHERE success = TRUE"
        );
                
        if(!$result)
            die(
                    "ERROR: Could not get number of parsed URLs from `crawlserv_"
                    .$namespace
                    ."_"
                    .$urllistNamespace
                    ."_parsing`"
            );
                    
        $row = $result->fetch_row();
                
        if($row)
            $urllistParsed = $row[0];
                    
        $result->close();
                    
        // get number of extracted URLs
        $result = $dbConnection->query(
                "SELECT COUNT(DISTINCT url)".
                " FROM `crawlserv_".$namespace."_".$urllistNamespace."_extracting`".
                " WHERE success = TRUE"
        );
                        
        if(!$result)
            die(
                    "ERROR: Could not get number of extracted URLs from `crawlserv_"
                    .$namespace
                    ."_"
                    .$urllistNamespace
                    ."_extracting`"
            );
                            
        $row = $result->fetch_row();
                                    
        if($row)
            $urllistExtracted = $row[0];
                                        
        $result->close();
                                        
        // get last update of selected URL list
        $result = $dbConnection->query(
                "SELECT MAX(crawltime) AS updated".
                " FROM `crawlserv_".$namespace."_".$urllistNamespace."_crawled`"
        );
                                        
        if($result) {
            $row = $result->fetch_assoc();
            
            if($row)
                $urllistUpdate = $row["updated"];
        }
                                        
        // get last update of any parsing table
        $result = $dbConnection->query(
                "SELECT updated".
                " FROM crawlserv_parsedtables".
                " WHERE website=$website".
                " AND urllist=$urllist".
                " ORDER BY updated DESC".
                " LIMIT 1"
        );
                                        
        if($result) {
            $row = $result->fetch_assoc();
            
            if($row)
                $parsedUpdate = $row["updated"];
        }
                                        
        // get last update of any extracting table
        $result = $dbConnection->query(
                "SELECT updated".
                " FROM crawlserv_extractedtables".
                " WHERE website=$website".
                " AND urllist=$urllist".
                " ORDER BY updated DESC".
                " LIMIT 1"
        );
                                        
        if($result) {
            $row = $result->fetch_assoc();
            
            if($row)
                $extractedUpdate = $row["updated"];
        }
    }
    else {
        $urllist = 0;
        $urllistName = "";
        $urllistNamespace = "";
    }
    
    echo "<div class=\"content-block\">\n";
    echo "<div class=\"entry-row\">\n";
    echo "<div class=\"entry-label\">Name:</div><div class=\"entry-input\">\n";
    
    echo "<input type=\"text\" class=\"entry-input trigger\" id=\"urllist-name\"";
    
    if($urllist)
        echo " data-trigger=\"urllist-update\"";
    else
        echo " data-trigger=\"urllist-add\"";

    echo " value=\"";
    
    echo htmlspecialchars($urllistName);
    
    echo "\" /></div>\n";
    
    echo "</div>\n";
    
    echo "<div class=\"entry-row\">\n";
    echo "<div class=\"entry-label\">Namespace:</div><div class=\"entry-input\">\n";
    
    echo "<input type=\"text\" class=\"entry-input trigger\" id=\"urllist-namespace\"";
    
    if($urllist)
        echo " data-trigger=\"urllist-update\"";
    else
        echo " data-trigger=\"urllist-add\"";
    
    echo " value=\"";
    
    echo htmlspecialchars($urllistNamespace);
    
    echo "\" /></div>\n";
    
    echo "</div>\n";
    
    if($urllist) {
        echo "<div class=\"entry-row\">\n";
        echo "<div class=\"entry-label\">Size:</div>\n";
        echo "<div class=\"entry-value\">\n";
        
        if($urllistSize == 1)
            echo "1 entry";
        else
            echo number_format($urllistSize)." entries";
        
        echo " <a id=\"urllist-download\" href=\"#\" class=\"entry-value\" target=\"_blank\""
            ." data-website-namespace=\"";
        
        echo htmlspecialchars($namespace);
        
        echo "\" data-namespace=\"";
        
        echo htmlspecialchars($urllistNamespace);
        
        echo "\">[Download]</a>\n";
        
        echo "</div>\n";
        echo "</div>\n";
        
        if($urllistCrawled) {
            echo "<div class=\"entry-row\">\n";
            echo "<div class=\"entry-label\">Crawled:</div>\n";
            echo "<div class=\"entry-value\">\n";
            
            if($urllistCrawled == 1)
                echo "1 entry";
            else
                echo number_format($urllistCrawled)." entries";
            
            echo " ("
            
                .number_format(
                        (float) $urllistCrawled / $urllistSize * 100,
                        1
                )
                
                ."%";
            
            if(isset($urllistUpdate) && $urllistUpdate) {
                echo ", ";
                
                echo time_elapsed_string($urllistUpdate);
            }
            
            echo ")";
            
            echo "</div>\n";
            echo "</div>\n";
        }
        
        if($urllistParsed) {
            echo "<div class=\"entry-row\">\n";
            echo "<div class=\"entry-label\">Parsed:</div>\n";
            echo "<div class=\"entry-value\">\n";
            
            if($urllistParsed == 1)
                echo "1 entry";
            else
                echo number_format($urllistParsed)." entries";
            
            echo " ("
            
                .number_format(
                    (float) $urllistParsed / $urllistSize * 100,
                    1
                )
            
                ."%";
            
            if(isset($parsedUpdate) && $parsedUpdate) {
                echo ", ";
                
                echo time_elapsed_string($parsedUpdate);
            }
            
            echo ") <a id=\"urllist-reset-parsing\" href=\"#\" class=\"entry-value\">[Reset]</a>";
            
            echo "</div>\n";
            echo "</div>\n";
        }
        
        if($urllistExtracted) {
            echo "<div class=\"entry-row\">\n";
            echo "<div class=\"entry-label\">Extracted:</div>\n";
            echo "<div class=\"entry-value\">\n";
            
            if($urllistExtracted == 1)
                echo "1 entry";
            else
                echo number_format($urllistExtracted)." entries";
            
            echo " ("
            
                .number_format(
                        (float) $urllistExtracted / $urllistParsed * 100,
                        1
                )
            
                ."%";
            
            if(isset($extractedUpdate) && $extractedUpdate) {
                echo ", ";
                
                echo time_elapsed_string($extractedUpdate);
            }
            
            echo ") <a id=\"urllist-reset-extracting\" href=\"#\" class=\"entry-value\">[Reset]</a>";
            
            echo "</div>\n";
            echo "</div>\n";
        }
    }
    
    echo "<div class=\"action-link-box\">\n";
    echo "<div class=\"action-link\">\n";
    
    if($urllist) {
        echo "<a id=\"urllist-update\" href=\"#\" class=\"action-link\">";
        
        echo "Change URL list</a>\n";
    }
    else
        echo "<a id=\"urllist-add\" href=\"#\" class=\"action-link\">Add URL list</a>\n";
    
    echo "</div>\n";
    echo "</div>\n";
    echo "</div>\n";
    
    if(isset($urllist) && $urllist) {
        // delete URLs by query
        echo "<div class=\"content-block\">\n";
        
        echo "<div id=\"urls-delete-div\" class=\"entry-row\">\n";
        
        echo "<div class=\"entry-label\">URL deletion:</div><div class=\"entry-input\">\n";
        
        echo "<select id=\"urls-delete-query\" class=\"entry-input\">\n";
        
        $result = $dbConnection->query(
                "SELECT id,name".
                " FROM crawlserv_queries".
                " WHERE (".
                        "website=$website".
                        " OR website IS NULL".
                ")".
                " AND type LIKE 'regex'".
                " AND resultbool".
                " ORDER BY name"
        );
        
        if(!$result)
            die("ERROR: Could not get IDs and names of queries.");
        
        while($row = $result->fetch_assoc()) {
            $id = $row["id"];
            $name = $row["name"];
            
            echo "<option value=\"".$id."\"";
            
            if(isset($query) && $id == $query)
                echo " selected";
           
            echo ">".htmlspecialchars($name)."</option>\n";
        }
        
        echo "</select>\n";
        
        echo "</div>\n";
        echo "</div>\n";
        
        echo "<div class=\"action-link-box\">\n";
        echo "<div class=\"action-link\">\n";
        
        echo "<a id=\"urls-delete\" href=\"#\" class=\"action-link\">Delete all matching URLs</a>\n";
        
        echo "</div>\n";
        echo "</div>\n";
        echo "</div>\n";
    }
    else if(isset($query))
        // temporarily save selected query
        echo "<input id=\"urls-delete-query\" type=\"hidden\" value=\"$query\" />\n";
}

?>

<script>

var queries = {};

enumQueries(queries);

</script>
