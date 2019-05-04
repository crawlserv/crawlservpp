<?php

$db_init = true;
$cc_init = true;

require "_helpers.php";
require "config.php";

if(isset($_POST["action"]))
    $action = $_POST["action"];
else
    $action = "import";

if(isset($_POST["datatype"]))
    $datatype = $_POST["datatype"];
else
    $datatype = "urllist";

if(isset($_POST["filetype"]))
    $filetype = $_POST["filetype"];
else
    $filetype = "text";

if(isset($_POST["compression"]))
    $compression = $_POST["compression"];
else
    $compression = "none";

if(isset($_POST["website"]))
    $website = $_POST["website"];

if(isset($_POST["urllist"]))
    $urllist = $_POST["urllist"];

?>

<h2>Import/Export</h2>

<div class="content-block">
<div class="entry-row">

<div class="entry-label">Action:</div><div class="entry-input">

<input type="radio" name="action" value="import" data-m="data" <?php

if($action == "import")
    echo " checked";

?> /> Import &emsp;

<input type="radio" name="action" value="merge" data-m="data"<?php

if($action == "merge")
    echo " checked";

?> /> Merge &emsp;

<input type="radio" name="action" value="export" data-m="data"<?php

if($action == "export")
    echo " checked";

?> /> Export

</div>
</div>

<div class="entry-row">

<div class="entry-label">Data Type:</div><div class="entry-input">

<select id="data-type-select" class="entry-input" data-m="data">

<option value="urllist" <?php 

if($datatype == "urllist")
    echo " selected";

?>>URL list</option>

</select>

</div>
</div>
</div>

<div class="content-block">

<?php

if($action != "merge") {
    // file type
    echo "<div class=\"entry-row\">\n";
    
    echo "<div class=\"entry-label\">File Type:</div><div class=\"entry-input\">\n";
    
    echo "<select id=\"file-type-select\" class=\"entry-input\" data-m=\"$m\">\n";
    
    echo "<option value=\"text\"";
    
    if($filetype == "text")
        echo " selected";
    
    echo ">Text file</option>\n";
    
    echo "</select>";
    
    echo "</div>\n</div>\n";
    
    // compression
    $result = $dbConnection->query(
        "SELECT name, version".
        " FROM crawlserv_versions"
        );
    
    if($result) {
        while($row = $result->fetch_assoc()) {
            if(strtolower($row["name"]) == "boost")
                $boost_version = $row["version"];
            else if(strtolower($row["name"]) == "zlib")
                $zlib_version = $row["version"];
        }
    
        $result->close();
    }
    
    if(!isset($boost_version))
        $boost_version = "[NOT FOUND]";
    
    if(!isset($zlib_version))
        $zlib_version = "[NOT FOUND]";
    
    echo "<div class=\"entry-row\">\n";
    
    echo "<div class=\"entry-label\">Compression:</div><div class=\"entry-input\">\n";
    
    echo "<select id=\"compression-select\" class=\"entry-input\">\n";
    
    echo "<option value=\"none\"";
    
    if($compression == "none")
        echo " selected";
    
    echo ">[none]</option>\n";
    
    echo "<option value=\"gzip\"";
    
    if($compression == "gzip")
        echo " selected";
    
    echo ">gzip (Boost $boost_version)</option>\n";
    
    echo "<option value=\"zlib\"";
    
    if($compression == "zlib")
        echo " selected";
        
        echo ">zlib (zlib $zlib_version)</option>\n";
    
    echo "</select>";
    
    echo "</div>\n</div>\n";
}

?>

</div>

<?php

if($action != "export") {
    // target
    echo "<div class=\"content-block\">\n";
    
    echo "<div class=\"entry-row\"><b>Target</b></div>\n";
    
    if($datatype == "urllist") {
        echo rowWebsiteSelect();
        echo rowUrlListSelect($action == "import", false, false, $action == "merge", "urllist-target");
            
        if($action == "import") {
            echo "<div id=\"urllist-name-div\" class=\"entry-row\">\n";
                
            echo "<div class=\"entry-label\">Name:</div><div class=\"entry-input\">\n";
                
            echo "<input type=\"text\" id=\"urllist-name\" class=\"entry-input\" />\n";
                
            echo "</div>\n</div>\n";
            
            echo "<div id=\"urllist-namespace-div\" class=\"entry-row\">\n";
            
            echo "<div class=\"entry-label\">Namespace:</div><div class=\"entry-input\">\n";
            
            echo "<input type=\"text\" id=\"urllist-namespace\" class=\"entry-input\" />\n";
            
            echo "</div>\n</div>\n";
        }
    }
    
    echo "</div>\n";
}

if($action != "import") {
    // source
    echo "<div class=\"content-block\">\n";
    
    echo "<div class=\"entry-row\"><b>Source</b></div>\n";
    
    if($datatype == "urllist") {
        if($action != "merge")
            echo rowWebsiteSelect();
        
        echo rowUrlListSelect(false, false, false, true, "urllist-source");
    }
    
    echo "</div>\n";
}

if($action == "import") {
    // file
    echo "<div class=\"content-block\">\n";
    
    echo "<div class=\"entry-row\">\n";
    
    echo "<div class=\"entry-label\">File:</div><div class=\"entry-input\">\n";
    
    echo "<form id=\"file-form\" action=\"$cc_host\" method=\"POST\" enctype=\"multipart/form-data\">\n";
    
    echo "<input id=\"file-select\" type=\"file\" name=\"fileToUpload\" />\n";
    
    echo "</form>\n";
    
    echo "</div>\n</div>\n</div>\n";
}

?>

<div class="content-block">

<?php 

if($action != "merge" || $datatype != "urllist") {
    echo "<div class=\"entry-row\"><b>Options</b></div>\n";

    if($action == "import") {
        // import options
        if($datatype == "urllist") {
            if($filetype == "text") {
                // options for importing URL list from text file
                echo "<div class=\"entry-row\">\n";
                echo "<div class=\"entry-label\"></div>\n";
                
                echo "<input id=\"is-firstline-header\" type=\"checkbox\" /> First line is header (will be ignored)\n";
                
                echo "</div>\n";
            }
        }
    }
    else if($action == "export") {
        // export options
        if($datatype == "urllist") {
            if($filetype == "text") {
                // options for exporting URL list to text file
                echo "<div class=\"entry-row\">\n";
                echo "<div class=\"entry-label\"></div>\n";
                echo "<div class=\"entry-input\">\n";
                
                echo "<input id=\"write-firstline-header\" type=\"checkbox\" /> Write header to first line:\n";
                
                echo "</div>\n</div>\n";
                
                echo "<div class=\"entry-row\">\n";
                echo "<div class=\"entry-label\"></div>\n";
                echo "<div class=\"entry-input\">\n";
                
                echo "<input id=\"firstline-header\" type=\"text\" />\n";
                
                echo "</div>\n</div>\n";
            }
        }
    }
}

?>

<div class="entry-row-right">

<input id="perform-action" type="button" value="<?php 

echo ucfirst($action);

?>" data-action="<?php 

echo $action;

?>"/>

</div>
</div>
