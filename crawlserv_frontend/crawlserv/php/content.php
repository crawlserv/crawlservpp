
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
 * content.php
 *
 * Show raw parsed, crawled and extracted content as well as the analyzing results. 
 *
 */

$db_init = true;
$cc_init = true;

require "include/helpers.php";
require "include/content.php";

require "config.php";

$is404 = false;

// get tab
if(isset($_POST["tab"])) {
    $tab = $_POST["tab"];
}
else {
    $tab = "parsed";
}

// get website
if(isset($_POST["website"])) {
    $website = $_POST["website"];
    
    // get namespace of website and whether it is cross-domain
    $result = $dbConnection->query(
            "SELECT namespace, domain".
            " FROM crawlserv_websites".
            " WHERE id = $website".
            " LIMIT 1"
    );
    
    if(!$result) {
        die("ERROR: Could not get namespace of website.");
    }
        
    $row = $result->fetch_assoc();
    
    if($row == NULL) {
        die("ERROR: Could not get namespace of website.");
    }
        
    $namespace = $row["namespace"];
    $crossDomain = is_null($row["domain"]);
    
    $result->close();
}

// get URL list
if(isset($_POST["urllist"])) {
    $urllist = $_POST["urllist"];
    
    // get namespace of URL list
    $result = $dbConnection->query(
        "SELECT namespace".
        " FROM crawlserv_urllists".
        " WHERE website = $website".
        " AND id = $urllist".
        " LIMIT 1"
    );
    
    if(!$result) {
        die("ERROR: Could not get namespace of URL list.");
    }
        
    $row = $result->fetch_assoc();
    
    if($row == NULL) {
        die("ERROR: Could not get namespace of URL list.");
    }
        
    $urllistNamespace = $row["namespace"];
    
    $result->close();
}

$url = "";
$urltext = "";

// get specific URL by parsed ID
if(
        $tab == "parsed"
        && isset($_POST["version"])
        && $_POST["version"]
        && isset($_POST["parsed_id"])
        && $_POST["parsed_id"]
        && isset($website)
        && $website
        && isset($urllist)
        && $urllist
) {
        // get name of parsing table by version (= ID of parsing table)
        $result = $dbConnection->query(
                "SELECT name
                 FROM `crawlserv_parsedtables`
                 WHERE website = $website
                 AND urllist = $urllist
                 AND id = ".$_POST["version"]."
                 LIMIT 1"
        );
        
        if(!$result) {
            die("ERROR: Could not get name of parsing table.");
        }
            
        $row = $result->fetch_assoc();
            
        if($row == NULL) {
            die("ERROR: Could not get name of parsing table.");
        }
                
        $utable = "crawlserv_".$namespace."_".$urllistNamespace;
        $ctable = $utable."_crawled";
        $ptable = $utable."_parsed_".$row["name"];
        
        $result->close();
        
        // search for URL
        $result = $dbConnection->query(
                "SELECT a.id, a.url
                 FROM `$utable` a
                 JOIN `$ctable` b
                 ON a.id = b.url
                 JOIN `$ptable` c
                 ON b.id = c.content
                 WHERE c.parsed_id LIKE '".$_POST["parsed_id"]."%'
                 LIMIT 1"
        );
                
        if(!$result) {
            die("Could not search for URL by parsed ID");
        }
                    
        $row = $result->fetch_assoc();
        
        if($row != NULL) {
            // found matching parsed ID -> set URL accordingly
            $url = $row["id"];
            $urltext = $row["url"];
        }
    }

// get specific URL by ID
if(
        ($tab == "crawled" || $tab == "parsed" || $tab == "extracted")
        && !$url
        && (isset($_POST["url"]))
        && $_POST["url"]
        && isset($website)
        && $website
        && isset($urllist)
        && $urllist
) {    
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
        
        if(!$result) {
            die("ERROR: Could not search for URL (getting last URL with content failed).");
        }
        
        $row = $result->fetch_assoc();
        
        if($row != NULL) {
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
            
            if(!$result) {
                die("ERROR: Could not search for last URL.");
            }
            
            $row = $result->fetch_assoc();
            
            if($row != NULL) {
                $url = $row["id"];
                $urltext = $row["url"];
            }
            else {
                $url = 0;
            }
        }
        
        $result->close();
    }
    else {
        // search for next URL with content, starting from $url-1
        $result = $dbConnection->query(
                "SELECT a.id AS id, a.url AS url
                 FROM `crawlserv_".$namespace."_".$urllistNamespace."` AS a,
                 `crawlserv_".$namespace."_".$urllistNamespace."_crawled` AS b
                 WHERE a.id = b.url
                 AND a.id >= ".$_POST["url"]."
                 ORDER BY a.id
                 LIMIT 1"
        );
        
        if(!$result) {
            die("ERROR: Could not search for URL (getting next URL with content failed).");
        }
        
        $row = $result->fetch_assoc();
        
        if($row != NULL) {
            $url = $row["id"];
            $urltext = $row["url"];
        }
        else {
            $url = 0;
        }
        
        $result->close();
    }
}

// get specific URL by text
if(
        ($tab == "crawled" || $tab == "parsed" || $tab == "extracted")
        && !$url
        && isset($_POST["urltext"])
        && isset($website)
        && $website
        && isset($urllist)
        && $urllist
) {    
    // search for matching URL with content
    $trimmedUrl = trim($_POST["urltext"]);
    
    if(!$crossDomain AND strlen($trimmedUrl) > 0 AND substr($trimmedUrl, 0, 1) == "/") {
        $trimmedUrl = substr($trimmedUrl, 1);
    }
    
    $sql = "SELECT a.id AS id, a.url AS url".
            " FROM `crawlserv_".$namespace."_".$urllistNamespace."` AS a,".
            " `crawlserv_".$namespace."_".$urllistNamespace."_crawled` AS b".
            " WHERE a.id = b.url".
            " AND a.url LIKE ?".
            " ORDER BY a.url".
            " LIMIT 1";
    
    $stmt = $dbConnection->prepare($sql);
    
    if(!$stmt) {
        die("ERROR: Could not prepare SQL statement to search for URL");
    }
    
    $argument = "";
    
    if(!$crossDomain) {
        $argument = "/";
    }
    
    $argument .= $trimmedUrl."%";
    
    $stmt->bind_param("s", $argument);
    
    if(!$stmt->execute()) {
        die("ERROR: Could not search for URL (execution failed.");
    }
    
    $result = $stmt->get_result();
    
    if(!$result) {
        die("ERROR: Could not search for URL (getting result failed.");
    }
    
    $row = $result->fetch_assoc();
    
    if($row != NULL) {
        $url = $row["id"];
        $urltext = $row["url"];
    }
    else {
        $is404 = true;
        $url = 0;
        
        if($crossDomain) {
            $urltext = $trimmedUrl;
        }
        else {
            $urltext = "/".$trimmedUrl;
        }
    }
    
    $result->close();
}

?>

<h2>Content<?php

echo "<span id=\"opt-mode\">\n";

if($tab == "crawled") {
    echo "<b>crawled</b>";
}
else {
    echo "<a href=\"#\" class=\"action-link post-redirect-tab\" data-m=\"$m\" data-tab=\"crawled\">crawled</a>\n";
}

echo " &middot; ";

if($tab == "parsed") {
    echo "<b>parsed</b>";
}
else {
    echo "<a href=\"#\" class=\"action-link post-redirect-tab\" data-m=\"$m\" data-tab=\"parsed\">parsed</a>\n";
}

echo " &middot; ";

if($tab == "extracted") {
    echo "<b>extracted</b>";
}
else {
    echo "<a href=\"#\" class=\"action-link post-redirect-tab\" data-m=\"$m\" data-tab=\"extracted\">extracted</a>\n";
}

echo " &middot; ";

if($tab == "analyzed") {
    echo "<b>analyzed</b>";
}
else {
    echo "<a href=\"#\" class=\"action-link post-redirect-tab\" data-m=\"$m\" data-tab=\"analyzed\">analyzed</a>\n";
}

echo " &middot; ";

if($tab == "corpora") {
    echo "<b>corpora</b>";
}
else {
    echo "<a href=\"#\" class=\"action-link post-redirect-tab\" data-m=\"$m\" data-tab=\"corpora\">corpora</a>\n";
}

echo "</span>\n";

?></h2>

<div class="content-block">

<?php

echo rowWebsiteSelect();

if($website) {
    echo rowUrlListSelect();
}

?>

</div>

<div class="content-block">

<?php

if($website && $urllist) {
    if(
            ($tab == "crawled" || $tab == "parsed" || $tab == "extracted")
            && !$is404
            && !$url
    ) {
        // get namespace of website and whether it is cross-domain
        $result = $dbConnection->query(
                "SELECT namespace, domain".
                " FROM crawlserv_websites".
                " WHERE id = $website".
                " LIMIT 1"
        );
        
        if(!$result) {
            die("ERROR: Could not get namespace of website.");
        }
        
        $row = $result->fetch_assoc();
        
        if($row == NULL){
            die("ERROR: Could not get namespace of website.");
        }
        
        $namespace = $row["namespace"];
        $crossDomain = is_null($row["domain"]);
        
        $result->close();
        
        // get namespace of URL list
        $result = $dbConnection->query(
                "SELECT namespace".
                " FROM crawlserv_urllists".
                " WHERE website = $website".
                " AND id = $urllist".
                " LIMIT 1"
        );
        
        if(!$result) {
            die("ERROR: Could not get namespace of URL list.");
        }
        
        $row = $result->fetch_assoc();
        
        if($row == NULL) {
            die("ERROR: Could not get namespace of URL list.");
        }
        
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
        
        if(!$result) {
            die("ERROR: Could not search for URL (getting first URL with content failed).");
        }
        
        $row = $result->fetch_assoc();
        
        if($row != NULL) {
            $url = $row["id"];
            $urltext = $row["url"];
        }
        else {
            $url = 0;
            $is404 = true;
        }
    
        $result->close();
    }
    
    if($tab == "crawled" || $tab == "parsed" || $tab == "extracted") {
        echo "<button id=\"content-last\" data-m=\"$m\" data-tab=\"$tab\">&lt;</button>";
        
        echo "<input type=\"text\" id=\"content-url\" data-m=\"$m\" data-tab=\"$tab\" value=\"$url\" title=\"$url\" />\n";
        
        echo "<span class=\"content-smalltext\">/</span>";
        
        if($crossDomain) {
            $displayedUrl = html($urltext);
        }
        else {
            $displayedUrl = html(substr($urltext, 1));
        }
        
        echo "<input type=\"text\" id=\"content-url-text\" data-m=\"$m\" data-tab=\"$tab\" value=\""
            .$displayedUrl
            ."\" title=\""
            .$displayedUrl
            ."\" />";
        
        echo "<button id=\"content-next\" data-m=\"$m\" data-tab=\"$tab\">&gt;</button>\n";
    }
    
    if($is404) {
        echo "<br><br><i>URL not found. Maybe it was not successfully crawled (yet).</i><br><br>\n";
    }
    else {
       // show content view dependent on tab
       require "view/$tab.php";
    }
}

?>

</div>

<script>

<?php 

if(
        isset($corpus_where)
        && isset($source)
        && $source == "parsed"
) {
    // get all the dates from the corpus for the datepicker
    echo "var corpusDates = [\n";
    
    $result = $dbConnection->query(
            "SELECT tmp.v
             FROM `crawlserv_corpora` t,
              JSON_TABLE(
                    t.datemap,
                    '$[*]'
                    COLUMNS (
                            v DATE PATH '$.v'
                    )
              ) tmp
              WHERE $corpus_where
              ORDER BY tmp.v"
    );
    
    if(!$result) {
        die("ERROR: Could not get dates from corpus");
    }
    
    while($row = $result->fetch_assoc()) {
        echo "\"".$row["v"]."\", ";
    }
    
    echo "];\n";
}

?>

</script>