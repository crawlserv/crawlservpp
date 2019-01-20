<?php
$is404 = false;
if(isset($_POST["website"])) $website = $_POST["website"];
if(isset($_POST["urllist"])) $urllist = $_POST["urllist"];
if(isset($_POST["url"]) && isset($website) && $website && isset($urllist) && $urllist) {
    // get namespace of website
    $result = $dbConnection->query("SELECT namespace FROM crawlserv_websites WHERE id=$website LIMIT 1");
    if(!$result) die("ERROR: Could not get namespace of website.");
    $row = $result->fetch_assoc();
    if(!$row) die("ERROR: Could not get namespace of website.");
    $namespace = $row["namespace"];
    $result->close();
    
    // get namespace of URL list
    $result = $dbConnection->query("SELECT namespace FROM crawlserv_urllists WHERE website=$website AND id=$urllist LIMIT 1");
    if(!$result) die("ERROR: Could not get namespace of URL list.");
    $row = $result->fetch_assoc();
    if(!$row) die("ERROR: Could not get namespace of URL list.");
    $urllistNamespace = $row["namespace"];
    $result->close();
    
    if(isset($_POST["last"])) {
        // search for last URL with content, starting from $url
        $result = $dbConnection->query("SELECT a.id AS id, a.url AS url FROM `crawlserv_".$namespace."_".$urllistNamespace
            ."` AS a, `crawlserv_".$namespace."_".$urllistNamespace."_crawled` AS b WHERE a.id = b.url AND a.id < ".$_POST["url"]
            ." ORDER BY a.id DESC LIMIT 1");
        if(!$result) die("ERROR: Could not search for URL.");
        $row = $result->fetch_assoc();
        if($row) {
            $url = $row["id"];
            $urltext = $row["url"];
        }
        else {
            // search for last URL with content in general
            $result = $dbConnection->query("SELECT a.id AS id, a.url AS url FROM `crawlserv_".$namespace."_".$urllistNamespace
                ."` AS a, `crawlserv_".$namespace."_".$urllistNamespace."_crawled` AS b WHERE a.id = b.url ORDER BY a.id DESC LIMIT 1");
            if(!$result) die("ERROR: Could not search for last URL.");
            $row = $result->fetch_assoc();
            if($row) {
                $url = $row["id"];
                $urltext = $row["url"];
            }
            else $url = 0;
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
    // get namespace of website
    $result = $dbConnection->query("SELECT namespace FROM crawlserv_websites WHERE id=$website LIMIT 1");
    if(!$result) die("ERROR: Could not get namespace of website.");
    $row = $result->fetch_assoc();
    if(!$row) die("ERROR: Could not get namespace of website.");
    $namespace = $row["namespace"];
    $result->close();
    
    // get namespace of URL list
    $result = $dbConnection->query("SELECT namespace FROM crawlserv_urllists WHERE website=$website AND id=$urllist LIMIT 1");
    if(!$result) die("ERROR: Could not get namespace of URL list.");
    $row = $result->fetch_assoc();
    if(!$row) die("ERROR: Could not get namespace of URL list.");
    $urllistNamespace = $row["namespace"];
    $result->close();
    
    // search for matching URL with content
    $result = $dbConnection->query("SELECT a.id AS id, a.url AS url FROM `crawlserv_".$namespace."_".$urllistNamespace."` AS a, `crawlserv_"
        .$namespace."_".$urllistNamespace."_crawled` AS b WHERE a.id = b.url AND a.url LIKE '/".$_POST["urltext"]."%' ORDER BY a.url LIMIT 1");
    if(!$result) die("ERROR: Could not search for URL.");
    $row = $result->fetch_assoc();
    if($row) {
        $url = $row["id"];
        $urltext = $row["url"];
    }
    else $is404 = true;
    $result->close();
}
if(isset($_POST["tab"])) $tab = $_POST["tab"];
else $tab = "crawled";
?>
<h2>Content<?php 
echo "<span id=\"opt-mode\">\n";
if($tab == "crawled") echo "<b>crawled</b>";
else echo "<a href=\"#\" class=\"action-link post-redirect-tab\" data-m=\"$m\" data-tab=\"crawled\">crawled</a>\n";
echo " &middot; ";
if($tab == "parsed") echo "<b>parsed</b>";
else echo "<a href=\"#\" class=\"action-link post-redirect-tab\" data-m=\"$m\" data-tab=\"parsed\">parsed</a>\n";
echo " &middot; ";
if($tab == "extracted") echo "<b>extracted</b>";
else echo "<a href=\"#\" class=\"action-link post-redirect-tab\" data-m=\"$m\" data-tab=\"extracted\">extracted</a>\n";
echo " &middot; ";
if($tab == "analyzed") echo "<b>analyzed</b>";
else echo "<a href=\"#\" class=\"action-link post-redirect-tab\" data-m=\"$m\" data-tab=\"analyzed\">analyzed</a>\n";
echo "</span>\n";
?></h2>
<div class="content-block">
<div class="entry-row">
<div class="entry-label">Website:</div><div class="entry-input">
<select class="entry-input" id="website-select" data-m="content" data-tab="<?php echo $tab; ?>">
<?php
$result = $dbConnection->query("SELECT id,name FROM crawlserv_websites ORDER BY name");
if(!$result) die("ERROR: Could not get ids and names of websites.");
$first = true;
while($row = $result->fetch_assoc()) {
    $id = $row["id"];
    $name = $row["name"];
    
    if($first) {
        if(!isset($website)) $website = $id;
        $first = false;
    }
    echo "<option value=\"".$id."\"";
    if($website == $id) {
        echo " selected";
    }
    echo ">".htmlspecialchars($name)."</option>\n";
}
$result->close();

if(!isset($website)) {
    $website = 0;
    echo "<option disabled>No website available</option>\n";
}
?>
</select>
</div>
</div>
<?php
if($website) {
    flush();
    echo "<div class=\"entry-label\">URL list:</div><div class=\"entry-input\">\n";
    echo "<select class=\"entry-input\" id=\"urllist-select\" data-m=\"$m\" data-tab=\"$tab\">\n";
    
    $result = $dbConnection->query("SELECT id,name,namespace FROM crawlserv_urllists WHERE website=$website ORDER BY name");
    if($result) {
        $first = true;
        while($row = $result->fetch_assoc()) {
            $ulId = $row["id"];
            $ulName = $row["name"];
            $ulNamespace = $row["namespace"];
            
            if($first) {
                if(!isset($urllist)) $urllist = $ulId;
                $first = false;
            }
            echo "<option value=\"".$ulId."\"";
            if($urllist == $ulId) {
                $urllistName = $ulName;
                $urllistNamespace = $ulNamespace;
                echo " selected";
            }
            echo ">".htmlspecialchars($ulName)."</option>\n";
        }
        $result->close();
    }
    
    if(!isset($urllist)) $urllist = 0;    
    if(!$urllist) echo "<option disabled>No URL list available</option>\n";
    echo "</select>\n";
    echo "</div>\n";
}
?>
</div>
<div class="content-block">
<?php
if($website && $urllist) {
    if(!$is404 && (!isset($url) || !$url)) {
        // get namespace of website
        $result = $dbConnection->query("SELECT namespace FROM crawlserv_websites WHERE id=$website LIMIT 1");
        if(!$result) die("ERROR: Could not get namespace of website.");
        $row = $result->fetch_assoc();
        if(!$row) die("ERROR: Could not get namespace of website.");
        $namespace = $row["namespace"];
        $result->close();
        
        // get namespace of URL list
        $result = $dbConnection->query("SELECT namespace FROM crawlserv_urllists WHERE website=$website AND id=$urllist LIMIT 1");
        if(!$result) die("ERROR: Could not get namespace of URL list.");
        $row = $result->fetch_assoc();
        if(!$row) die("ERROR: Could not get namespace of URL list.");
        $urllistNamespace = $row["namespace"];
        $result->close();
        
        // get first URL with content from URL list
        $result = $dbConnection->query("SELECT a.id AS id, a.url AS url FROM `crawlserv_".$namespace."_".$urllistNamespace
            ."` AS a, `crawlserv_".$namespace."_".$urllistNamespace."_crawled` AS b WHERE a.id = b.url ORDER BY a.id LIMIT 1");
        if(!$result) die("ERROR: Could not search for URL.");
        $row = $result->fetch_assoc();
        if($row) {
            $url = $row["id"];
            $urltext = $row["url"];
        }
        else $is404 = true;
        $result->close();
    }
    
    echo "<button id=\"content-last\" data-m=\"$m\" data-tab=\"$tab\">&lt;</button>";
    echo "<input type=\"text\" id=\"content-url\" data-m=\"$m\" data-tab=\"$tab\" value=\"$url\" title=\"$url\" />\n";
    echo "<span id=\"content-slash\">/</span>";
    echo "<input type=\"text\" id=\"content-url-text\" data-m=\"$m\" data-tab=\"$tab\" value=\"".htmlspecialchars(substr($urltext, 1))
        ."\" title=\"".htmlspecialchars(substr($urltext, 1))."\" />";
    echo "<button id=\"content-next\" class=\"fs-insert-after\" data-m=\"$m\" data-tab=\"$tab\">&gt;</button>\n";
    
    if($is404) {
        // no content available
    }
    else {
       // show content dependent on tab
        switch($tab) {
            case "parsed":
                // parsed data
                if(isset($_POST["version"])) {
                    // select specified parsing table
                    $version = $_POST["version"];
                    $result = $dbConnection->query("SELECT name FROM crawlserv_parsedtables WHERE website = $website AND id = $version LIMIT 1");
                    if(!$result) die("ERROR: Could not get parsing table.");
                    $row = $result->fetch_assoc();
                    if(!$row) die("ERROR: Could not get parsing table.");
                    $result->close();
                }
                else {
                    // select newest parsing table
                    $result =
                        $dbConnection->query("SELECT id, name FROM crawlserv_parsedtables WHERE website = $website ORDER BY id DESC LIMIT 1");
                    if(!$result) die("ERROR: Could not get parsing table.");
                    $row = $result->fetch_assoc();
                    if(!$row) $version = 0;
                    $version = $row["id"];
                    $result->close();
                }
                if($version) {
                    // get column names
                    $crawlingtable = "crawlserv_".$namespace."_".$urllistNamespace."_crawled";
                    $parsingtable = "crawlserv_".$namespace."_".$urllistNamespace."_parsed_".$row["name"];
                    $result = $dbConnection->query("SELECT COLUMN_NAME AS name FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'crawled'"
                        ." AND TABLE_NAME = N'$parsingtable'");
                    if(!$result) die("ERROR: Could not get columns from parsing table.");
                    $columns = array();
                    while($row = $result->fetch_assoc())
                        if(strlen($row["name"]) > 7 && substr($row["name"], 0, 7) == "parsed_") $columns[] = $row["name"];
                    $result->close();
                    
                    if(count($columns)) {
                        $query = "SELECT ";
                        foreach($columns as $column) {
                            $query .= "b.`".$column."`, ";
                        }
                        $query = substr($query, 0, -2);
                        $query .= " FROM `$crawlingtable` AS a, `$parsingtable` AS b WHERE a.id = b.content AND a.url = "
                            .$url." ORDER BY b.id DESC LIMIT 1";
                        $result = $dbConnection->query($query);
                        if(!$result) die("Could not get data from parsing table.");
                        $row = $result->fetch_assoc();
                        if($row) {
                            $result->close();
                            echo "<div id=\"content-table\" class=\"fs-div\">\n";
                            echo "<button id=\"content-fullscreen\" title=\"Show Fullscreeen\">&#9727;</button>\n";
                            echo "<table class=\"fs-content\">\n";
                            echo "<thead>\n";
                            echo "<tr>\n";
                            echo "<th>Field</th>\n";
                            echo "<th>Value</th>\n";
                            echo "</tr>\n";
                            echo "</thead>\n";
                            echo "<tbody>\n";
                            foreach($columns as $column) {
                                echo "<tr>\n";
                                if(strlen($column) > 8 && substr($column, 0, 8) == "parsed__") echo "<td>".substr($column, 8)."</td>\n";
                                else if(strlen($column) > 7 && substr($column, 0, 7) == "parsed_") echo "<td>".substr($column, 7)."</td>\n";
                                echo "<td>".$row[$column]."</td>\n";
                                echo "</tr>\n";
                            }
                            echo "</tbody>\n";
                            echo "</table>\n";
                            echo "</div>\n";
                        }
                        else echo "<br><br><i>No parsing data available for this URL.</i><br><br>\n";
                    }
                    else echo "<br><br><i>No parsing data available for this website.</i><br><br>\n";
                }
                else echo "<br><br><i>No parsing data available for this website.</i><br><br>\n";
                break;
            case "extracted":
                // extracted data
                // TODO
                echo "<br><br><i>This feature is not implemented yet.<br><br></i>\n";
                break;
            case "analyzed":
                // analyzed data
                // TODO
                echo "<br><br><i>This feature is not implemented yet.<br><br></i>\n";
                break;
            default:
                // crawled data
                echo "<div id=\"content-text\" class=\"fs-div\">\n";
                echo "<button id=\"content-fullscreen\" title=\"Show Fullscreeen\">&#9727;</button>\n";
                echo "<pre><code id=\"content-code\" class=\"language-html fs-content\">\n";
                if(isset($_POST["version"])) {
                    // select specified version
                    $version = $_POST["version"];
                    $result = $dbConnection->query("SELECT b.content FROM `crawlserv_".$namespace."_".$urllistNamespace."` AS a, `crawlserv_"
                        .$namespace."_".$urllistNamespace."_crawled` AS b WHERE a.id = b.url AND a.id = $url AND b.id = $version LIMIT 1");
                    if(!$result) die("ERROR: Could not get content of URL.");
                    $row = $result->fetch_assoc();
                    if(!$row) die("ERROR: Could not get content of URL.");
                    $result->close();
                }
                else {
                    // select newest version
                    $result = $dbConnection->query("SELECT b.id AS id, b.content AS content FROM `crawlserv_".$namespace."_".$urllistNamespace.
                        "` AS a, `crawlserv_".$namespace."_".$urllistNamespace."_crawled` AS b WHERE a.id = b.url AND a.id = ".$url
                        ." ORDER BY b.crawltime DESC LIMIT 1");
                    if(!$result) die("ERROR: Could not get content of URL.");
                    $row = $result->fetch_assoc();
                    if(!$row) die("ERROR: Could not get content of URL.");
                    $version = $row["id"];
                    $result->close();
                }
                echo htmlspecialchars($row["content"]);
                $info = number_format(strlen($row["content"]))
                        ." bytes <a href=\"#\" id=\"content-download\" target=\"_blank\" data-website-namespace=\""
                        .htmlspecialchars($namespace)."\" data-namespace=\"".htmlspecialchars($urllistNamespace)
                        ."\" data-version=\"$version\">[Download]</a>";
                echo "</code></pre></div>\n";
                echo "<div id=\"content-status\">\n";
                echo "<select id=\"content-version\" data-m=\"$m\" data-tab=\"$tab\">\n";
                $result = $dbConnection->query("SELECT b.id AS id, b.crawltime AS crawltime, b.archived AS archived FROM `crawlserv_".$namespace."_"
                    .$urllistNamespace."` AS a, `crawlserv_".$namespace."_".$urllistNamespace."_crawled` AS b WHERE a.id = b.url AND a.id = ".$url
                    ." ORDER BY b.crawltime DESC");
                if(!$result) die("ERROR: Could not get content versions.");
                $num = $result->num_rows;
                $count = 0;
                while($row = $result->fetch_assoc()) {
                    $count++;
                    echo "<option value=\"".$row["id"]."\"";
                    if($row["id"] == $version) echo " selected";
                    echo ">Version ".number_format($count)." of ".number_format($num)." &ndash; crawled on ".$row["crawltime"];
                    if($row["archived"]) echo " (archived)";
                    echo "</option>\n";
                }
                echo "</select>\n";
                echo "<span id=\"content-info\">$info</span></div>";
        }
    }
}
?>
</div>