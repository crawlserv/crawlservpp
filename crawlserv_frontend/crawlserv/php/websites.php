<?php

$db_init = true;
$cc_init = true;

require "_helpers.php";
require "config.php";

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
    
    foreach($string as $k => &$v) {
        if ($diff->$k)
            $v = $diff->$k . $v;
        else
            unset($string[$k]);
    }
    
    $string = array_slice($string, 0, 1);
    
    return $string ? implode(', ', $string) . ' ago' : 'just now';
}

if(isset($_POST["website"]))
    $website = $_POST["website"];

if(isset($_POST["urllist"]))
    $urllist = $_POST["urllist"];

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
    echo "<a href=\"#\" class=\"action-link website-duplicate\">Duplicate configuration</a>";

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

<input type="text" class="entry-input" id="website-domain" value="<?php

if($website)
    echo htmlspecialchars($domain);

?>" />

</div>
</div>

<div class="entry-row">
<div class="entry-label"></div>
<div class="entry-input">

<input type="checkbox" id="website-crossdomain" <?php

if($website && !strlen($domain))
    echo "checked";

?> /> cross-domain website

</div>
</div>

<div class="entry-row">
<div class="entry-label"></div>
<div class="entry-input">

<?php

if(!$website)
    echo '<input type="checkbox" id="website-externaldir" /> use external directory';

?>

</div>
</div>

<div class="entry-row">
<div class="entry-label">Directory:</div>
<div class="entry-input">

<input type="text" class="entry-input" <?php

if($website)
    echo 'disabled';
else
    echo 'id="website-dir"';

?> value="<?php

echo $website && strlen($dir) ? htmlspecialchars($dir) : "[default]";

?>" />

</div>
</div>

<div class="action-link-box">
<div class="action-link">

<?php

if($website)
    echo "<a href=\"#\" class=\"action-link website-update\">Change website</a>";
else
    echo "<a href=\"#\" class=\"action-link website-add\">Add website</a>";

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
    
    echo "<input type=\"text\" class=\"entry-input\" id=\"urllist-name\" value=\"";
    
    echo htmlspecialchars($urllistName);
    
    echo "\" /></div>\n";
    
    echo "</div>\n";
    
    echo "<div class=\"entry-row\">\n";
    echo "<div class=\"entry-label\">Namespace:</div><div class=\"entry-input\">\n";
    
    echo "<input type=\"text\" class=\"entry-input\" id=\"urllist-namespace\" value=\"";
    
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
        
        echo " <a href=\"#\" class=\"urllist-download entry-value\" target=\"_blank\""
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
            
            echo ") <a href=\"#\" class=\"urllist-reset-parsing entry-value\">[Reset]</a>";
            
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
            
            echo ") <a href=\"#\" class=\"urllist-reset-extracting entry-value\">[Reset]</a>";
            
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
    else
        echo "<a href=\"#\" class=\"action-link urllist-add\">Add URL list</a>\n";
    
    echo "</div>\n";
    echo "</div>\n";
    echo "</div>\n";
}

?>
