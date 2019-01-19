<?php
// datetime to time elapsed string function based on https://stackoverflow.com/a/18602474 by Glavić
function time_elapsed_string($datetime) {
    $now = new DateTime;
    $ago = new DateTime($datetime);
    $diff = $now->diff($ago);
    
    $diff->w = floor($diff->d / 7);
    $diff->d -= $diff->w * 7;
    
    $string = array(
        'y' => 'y',
        'm' => 'm',
        'w' => 'w',
        'd' => 'd',
        'h' => 'h',
        'i' => 'min',
        's' => 's',
    );
    foreach ($string as $k => &$v) {
        if ($diff->$k) {
            $v = $diff->$k . $v;
        } else {
            unset($string[$k]);
        }
    }
    
    $string = array_slice($string, 0, 1);
    return $string ? implode(', ', $string) . ' ago' : 'just now';
}

if(isset($_POST["website"])) $website = $_POST["website"];
if(isset($_POST["urllist"])) $urllist = $_POST["urllist"];
?>
<h2>Websites</h2>
<div class="content-block">
<div class="entry-row">
<div class="entry-label">Website:</div><div class="entry-x-input">
<select class="entry-x-input" id="website-select" data-m="websites">
<?php
$result = $dbConnection->query("SELECT id,name FROM crawlserv_websites ORDER BY name");
if(!$result) exit("ERROR: Could not get ids and names of websites.");
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
        $websiteName = $name;
    }
    echo ">".htmlspecialchars($name)."</option>\n";
}
$result->close();

if(isset($website)) {
    if($website) {
        $result = $dbConnection->query("SELECT namespace,domain FROM crawlserv_websites WHERE id=$website");
        if(!$result) exit("ERROR: Could not get namespace and domain of website.");
        $row = $result->fetch_assoc();
        
        $namespace = $row["namespace"];
        $domain = $row["domain"];
        
        $result->close();
    }
}
else $website = 0;
?>
<option value="0"<?php if(!$website) echo " selected"; ?>>Add new</option>
</select>
<a href="#" class="actionlink website-delete"><span class="remove-entry">X</span></a>
</div>
</div>
<div class="action-link-box">
<div class="action-link">
<?php
if($website) echo "<a href=\"#\" class=\"action-link website-duplicate\">Duplicate configuration</a>";
?>
</div>
</div>
</div>
<div class="content-block">
<div class="entry-row">
<div class="entry-label">Name:</div><div class="entry-input">
<input type="text" class="entry-input" id="website-name" value="<?php if($website) echo htmlspecialchars($websiteName); ?>" />
</div>
</div>
<div class="entry-row">
<div class="entry-label">Namespace:</div><div class="entry-input">
<input type="text" class="entry-input" id="website-namespace" value="<?php if($website) echo htmlspecialchars($namespace); ?>" />
</div>
</div>
<div class="entry-row">
<div class="entry-label">Domain:</div><div class="entry-input">
<input type="text" class="entry-input" id="website-domain" value="<?php if($website) echo htmlspecialchars($domain); ?>" />
</div>
</div>
<div class="action-link-box">
<div class="action-link">
<?php
if($website) echo "<a href=\"#\" class=\"action-link website-update\">Change website</a>";
else echo "<a href=\"#\" class=\"action-link website-add\">Add website</a>";
?>
</div>
</div>
</div>
<?php 
if($website) {
    flush();
    echo "<div class=\"content-block\">\n";
    echo "<div class=\"entry-label\">URL list:</div><div class=\"entry-x-input\">\n";
    echo "<select class=\"entry-x-input\" id=\"urllist-select\" data-m=\"$m\">\n";
    
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
    
    if(isset($urllist) && $urllist) {
        // get size of selected URL list
        $result = $dbConnection->query("SELECT COUNT(*) FROM crawlserv_".$namespace."_".$urllistNamespace);
        if(!$result) exit("ERROR: Could not get size of URL list from crawlserv_".$namespace."_".$urllistNamespace);
        $row = $result->fetch_row();
        $urllistSize = $row[0];
        $result->close();
        
        // get number of crawled URLs
        $result = $dbConnection->query("SELECT COUNT(*) FROM crawlserv_".$namespace."_".$urllistNamespace." WHERE crawled = TRUE");
        if(!$result) exit("ERROR: Could not get number of crawled URLs from crawlserv_".$namespace."_".$urllistNamespace);
        $row = $result->fetch_row();
        $urllistCrawled = $row[0];
        $result->close();
        
        // get number of parsed URLs
        $result = $dbConnection->query("SELECT COUNT(*) FROM crawlserv_".$namespace."_".$urllistNamespace." WHERE parsed = TRUE");
        if(!$result) exit("ERROR: Could not get number of crawled URLs from crawlserv_".$namespace."_".$urllistNamespace);
        $row = $result->fetch_row();
        $urllistParsed = $row[0];
        $result->close();
        
        // get number of extracted URLs
        $result = $dbConnection->query("SELECT COUNT(*) FROM crawlserv_".$namespace."_".$urllistNamespace." WHERE extracted = TRUE");
        if(!$result) exit("ERROR: Could not get number of crawled URLs from crawlserv_".$namespace."_".$urllistNamespace);
        $row = $result->fetch_row();
        $urllistExtracted = $row[0];
        $result->close();
        
        // get number of analyzed URLs
        $result = $dbConnection->query("SELECT COUNT(*) FROM crawlserv_".$namespace."_".$urllistNamespace." WHERE analyzed = TRUE");
        if(!$result) exit("ERROR: Could not get number of crawled URLs from crawlserv_".$namespace."_".$urllistNamespace);
        $row = $result->fetch_row();
        $urllistAnalyzed = $row[0];
        $result->close();
        
        // get last update of selected URL list
        $result = $dbConnection->query("SELECT UPDATE_TIME FROM information_schema.tables WHERE TABLE_SCHEMA='".$db_name."'"
           ." AND TABLE_NAME='crawlserv_".$namespace."_".$urllistNamespace."'");
        if($result) {
            $row = $result->fetch_assoc();
            if($row) $urllistUpdate = $row["UPDATE_TIME"];
        }
        
        // get last update of any parsing table
        $result = $dbConnection->query("SELECT updated FROM crawlserv_parsedtables WHERE urllist=$ulId ORDER BY updated LIMIT 1");
        if($result) {
            $row = $result->fetch_assoc();
            if($row) $parsedUpdate = $row["updated"];
        }
        
        // get last update of any extracting table
        $result = $dbConnection->query("SELECT updated FROM crawlserv_extractedtables WHERE urllist=$ulId ORDER BY updated LIMIT 1");
        if($result) {
            $row = $result->fetch_assoc();
            if($row) $extractedUpdate = $row["updated"];
        }
        
        // get last update of any analyzing table
        $result = $dbConnection->query("SELECT updated FROM crawlserv_analyzedtables WHERE urllist=$ulId ORDER BY updated LIMIT 1");
        if($result) {
            $row = $result->fetch_assoc();
            if($row) $analyzedUpdate = $row["updated"];
        }
    }
    else {
        $urllist = 0;
        $urllistName = "";
        $urllistNamespace = "";
    }
    
    if(!$urllist) echo "<option value=\"0\" selected>Add new</option>\n";
    else echo "<option value=\"0\">Add new</option>\n";
    echo "</select>\n";
    echo "<a href=\"#\" class=\"actionlink urllist-delete\" ><span class=\"remove-entry\">X</span></a>\n";
    echo "</div>\n</div>\n";
    echo "<div class=\"content-block\">\n";
    echo "<div class=\"entry-row\">\n";
    echo "<div class=\"entry-label\">Name:</div><div class=\"entry-input\">\n";
    echo "<input type=\"text\" class=\"entry-input\" id=\"urllist-name\" value=\"".htmlspecialchars($urllistName)."\" /></div>\n";
    echo "</div>\n";
    echo "<div class=\"entry-row\">\n";
    echo "<div class=\"entry-label\">Namespace:</div><div class=\"entry-input\">\n";
    echo "<input type=\"text\" class=\"entry-input\" id=\"urllist-namespace\" value=\"".htmlspecialchars($urllistNamespace)."\" /></div>\n";
    echo "</div>\n";
    if($urllist) {
        echo "<div class=\"entry-row\">\n";
        echo "<div class=\"entry-label\">Size:</div>\n";
        echo "<div class=\"entry-value\">\n";
        if($urllistSize == 1) echo "1 entry";
        else echo number_format($urllistSize)." entries";
        echo " <a href=\"#\" class=\"urllist-download entry-value\" target=\"_blank\"";
        echo " data-website-namespace=\"$namespace\" data-namespace=\"".htmlspecialchars($urllistNamespace)."\">[Download]</a>\n";
        echo "</div>\n";
        echo "</div>\n";
        if($urllistCrawled) {
            echo "<div class=\"entry-row\">\n";
            echo "<div class=\"entry-label\">Crawled:</div>\n";
            echo "<div class=\"entry-value\">\n";
            if($urllistCrawled == 1) echo "1 entry";
            else echo number_format($urllistCrawled)." entries";
            echo " (";
            echo number_format((float) $urllistCrawled / $urllistSize * 100, 1);
            echo "%";
            if($urllistUpdate) {
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
            if($urllistParsed == 1) echo "1 entry";
            else echo number_format($urllistParsed)." entries";
            echo " (";
            echo number_format((float) $urllistParsed / $urllistSize * 100, 1);
            echo "%";
            if($parsedUpdate) {
                echo ", ";
                echo time_elapsed_string($parsedUpdate);
            }
            echo ") <a href=\"#\" class=\"urllist-reset-parsing entry-value\">[Reset]</a>";
            echo "</div>\n";
            echo "</div>\n";
        }
        if($urllistExtracted) {
            echo "<div class=\"entry-row\">\n";
            echo "<div class=\"entry-label\">Extracted:</div>\n";
            echo "<div class=\"entry-value\">\n";
            if($urllistExtracted == 1) echo "1 entry";
            else echo number_format($urllistExtracted)." entries";
            echo " (";
            echo number_format((float) $urllistExtracted / $urllistParsed * 100, 1);
            echo "%";
            if($extractedUpdate) {
                echo ", ";
                echo time_elapsed_string($extractedUpdate);
            }
            echo ") <a href=\"#\" class=\"urllist-reset-extracting entry-value\">[Reset]</a>";
            echo "</div>\n";
            echo "</div>\n";
        }
        if($urllistAnalyzed) {
            echo "<div class=\"entry-row\">\n";
            echo "<div class=\"entry-label\">Analyzed:</div>\n";
            echo "<div class=\"entry-value\">\n";
            if($urllistAnalyzed == 1) echo "1 entry";
            else echo number_format($urllistAnalyzed)." entries";
            echo " (";
            echo number_format((float) $urllistAnalyzed / $urllistParsed * 100, 1);
            echo "%";
            if($analyzedUpdate) {
                echo ", ";
                echo time_elapsed_string($analyzedUpdate);
            }
            echo ") <a href=\"#\" class=\"urllist-reset-analyzing entry-value\">[Reset]</a>";
            echo "</div>\n";
            echo "</div>\n";
        }
    }
    echo "<div class=\"action-link-box\">\n";
    echo "<div class=\"action-link\">\n";
    if($urllist) {
        echo "<a href=\"#\" class=\"action-link urllist-update\">";
        echo "Change URL list</a>\n";
    }
    else echo "<a href=\"#\" class=\"action-link urllist-add\">Add URL list</a>\n";
    echo "</div>\n";
    echo "</div>\n";
    echo "</div>\n";
}
?>
