
<?php

/*
 * 
 * ---
 * 
 *  Copyright (C) 2023 Anselm Schmidt (ans[ät]ohai.su)
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
 * data.php
 *
 * Import, export or merge data into/from/in the database.
 *
 */

$db_init = true;
$cc_init = true;

require "include/helpers.php";

require "config.php";

if(isset($_POST["action"])) {
    $action = $_POST["action"];
}
else {
    $action = "import";
}

if(isset($_POST["datatype"])) {
    $datatype = $_POST["datatype"];
}
else {
    $datatype = "urllist";
}

if(isset($_POST["filetype"])) {
    $filetype = $_POST["filetype"];
}
else {
    $filetype = "text";
}

if(isset($_POST["compression"])) {
    $compression = $_POST["compression"];
}
else {
    $compression = "none";
}

if(isset($_POST["website"])) {
    $website = $_POST["website"];
}

if(isset($_POST["urllist"])) {
    $urllist = $_POST["urllist"];
}

?>

<h2>Import/Export</h2>

<div class="content-block">
<div class="entry-row">

<div class="entry-label">Action:</div><div class="entry-input">

<input type="radio" name="action" value="import" data-m="data" <?php

if($action == "import") {
    echo " checked";
}

?> /> Import &emsp;

<input type="radio" name="action" value="merge" data-m="data"<?php

if($action == "merge") {
    echo " checked";
}

?> /> Merge &emsp;

<input type="radio" name="action" value="export" data-m="data"<?php

if($action == "export") {
    echo " checked";
}

?> /> Export

</div>
</div>

<div class="entry-row">

<div class="entry-label">Data Type:</div><div class="entry-input">

<select id="data-type-select" class="entry-input" data-m="data">

<option value="urllist" <?php 

if($datatype == "urllist") {
    echo " selected";
}

?>>URL list</option>

<?php 
if($action == "export") {    
    echo "<option value=\"analyzed\" ";

    if($datatype == "analyzed") {
        echo " selected";
    }

    echo ">Analyzed data</option>\n";
    
    echo "<option value=\"corpus\" ";
    
    if($datatype == "corpus") {
        echo " selected";
    }
    
    echo ">Corpus data</option>\n";
}
?>

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
    
    if($datatype == "urllist" || $datatype == "corpus") {
        echo "<option value=\"text\"";
        
        if($filetype == "text") {
            echo " selected";
        }
        
        echo ">Text file (*.txt)</option>\n";
    }
    
    if($action != "import") {
        echo "<option value=\"spreadsheet\"";
        
        if($filetype == "spreadsheet") {
            echo " selected";
        }
        
        echo ">OpenDocument spreadsheet (*.ods)</option>\n";
    }
    
    echo "</select>";
    
    echo "</div>\n</div>\n";
    
    // compression
    $result = $dbConnection->query(
        "SELECT name, version".
        " FROM crawlserv_versions"
        );
    
    if($result) {
        while($row = $result->fetch_assoc()) {
            if(strtolower($row["name"]) == "boost") {
                $boost_version = $row["version"];
            }
            else if(strtolower($row["name"]) == "zlib") {
                $zlib_version = $row["version"];
            }
            else if(strtolower($row["name"]) == "libzip") {
                $libzip_version = $row["version"];
            }
        }
    
        $result->close();
    }
    
    if(!isset($boost_version)) {
        $boost_version = "[NOT FOUND]";
    }
    
    if(!isset($zlib_version)) {
        $zlib_version = "[NOT FOUND]";
    }
    
    if(!isset($libzip_version)) {
        $libzip_version = "[NOT FOUND]";
    }
    
    echo "<div class=\"entry-row\">\n";
    
    echo "<div class=\"entry-label\">Compression:</div><div class=\"entry-input\">\n";
    
    echo "<select id=\"compression-select\" class=\"entry-input\">\n";
    
    echo "<option value=\"none\"";
    
    if($compression == "none") {
        echo " selected";
    }
    
    echo ">[none]</option>\n";
    
    echo "<option value=\"gzip\"";
    
    if($compression == "gzip") {
        echo " selected";
    }
    
    echo ">gzip (Boost $boost_version)</option>\n";
    
    echo "<option value=\"zlib\"";
    
    if($compression == "zlib") {
        echo " selected";
    }
        
    echo ">zlib (zlib $zlib_version)</option>\n";
    
    echo "<option value=\"zip\"";
    
    if($compression == "zip") {
        echo " selected";
    }
    
    echo ">zip (libzip $libzip_version)</option>\n";
    
    echo "</select>";
    
    echo "</div>\n</div>\n";
}

?>

</div>

<?php

if($action != "import") {
    // source
    echo "<div class=\"content-block\">\n";
    
    echo "<div class=\"entry-row\"><b>Source</b></div>\n";
    
    if($datatype == "urllist") {
        echo rowWebsiteSelect();
        echo rowUrlListSelect(false, false, false, true, "urllist-source");
    }
    else if($datatype == "analyzed") {
        echo rowWebsiteSelect();
        echo rowUrlListSelect(false, false, false, false);
        echo rowTableSelect($datatype);
    }
    else if($action == "export" && $datatype == "corpus") {
        echo rowWebsiteSelect();
        echo rowUrlListSelect(false, false, false, false);
        echo rowCorpusSelect();
        
        echo "<div class=\"entry-row\">\n";
        echo "<div class=\"entry-label\">Data:</div>";
        echo "<select id=\"what\" class=\"entry-input\">";
        echo "<option value=\"text\">Content</option>\n";
        echo "<option value=\"articles\">IDs</option>\n";
        echo "<option value=\"dates\">Dates</option>\n";
        echo "</select>\n";
        echo "</div>\n";
        echo "</div>\n";
    } 
    
    echo "</div>\n";
}

if($action != "export") {
    // target
    echo "<div class=\"content-block\">\n";
    
    echo "<div class=\"entry-row\"><b>Target</b></div>\n";
    
    if($datatype == "urllist") {
        if($action != "merge") {
            echo rowWebsiteSelect();
        }
        
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

if($action == "import") {
    // file
    echo "<div class=\"content-block\">\n";
    
    echo "<div class=\"entry-row\">\n";
    
    echo "<div class=\"entry-label\">File:</div><div class=\"entry-input\">\n";
    
    echo "<form id=\"file-form\" action=\"$cc_host/upload\" method=\"POST\" enctype=\"multipart/form-data\">\n";
    
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
            if($filetype == "text" ) {
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
            if($filetype == "text" || $filetype == "spreadsheet") {
                // options for exporting URL list to text file or spreadsheet
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
        else if($datatype == "analyzed") {
            if($filetype == "text" || $filetype == "spreadsheet") {
                // option for exporting analyzed datato text file or spreadsheet
                echo "<div class=\"entry-row\">\n";
                echo "<div class=\"entry-label\"></div>\n";
                echo "<div class=\"entry-input\">\n";
                
                echo "<input id=\"column-names\" type=\"checkbox\" checked /> Include column names\n";
                
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
