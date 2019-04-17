<?php
if(isset($_POST["website"])) $website = $_POST["website"];
if(isset($_POST["config"])) $config = $_POST["config"];
if(isset($_POST["mode"])) $mode = $_POST["mode"];
else $mode = "simple";
if(isset($_POST["current"])) $current = $_POST["current"];
?>
<h2>Parsing options<?php 
echo "<span id=\"opt-mode\">\n";
if($mode == "simple") echo "<b>simple</b>";
else echo "<a href=\"#\" class=\"action-link post-redirect-mode\" data-m=\"$m\" data-mode=\"simple\">simple</a>\n";
echo " &middot; ";
if($mode == "advanced") echo "<b>advanced</b>";
else echo "<a href=\"#\" class=\"action-link post-redirect-mode\" data-m=\"$m\" data-mode=\"advanced\">advanced</a>\n";
echo "</span>\n";
?></h2>
<div class="content-block">
<div class="entry-row">
<div class="entry-label">Website:</div><div class="entry-input">
<select class="entry-input" id="website-select" data-m="<?php echo $m; ?>" data-mode="<?php echo $mode; ?>">
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
    }
    echo ">".htmlspecialchars($name)."</option>\n";
}
$result->close();
if(!isset($website)) echo "<option disabled>No websites available</option>\n";
?>
</select>
</div>
</div>
<?php
if(isset($website)) {
    echo "<div class=\"entry-row\">\n";
    echo "<div class=\"entry-label\">Parser:</div><div class=\"entry-x-input\">\n";
    echo "<select id=\"config-select\" class=\"entry-x-input\" data-m=\"$m\" data-mode=\"$mode\">\n";
    
    $result = $dbConnection->query("SELECT id, name FROM crawlserv_configs WHERE website=$website AND module='parser' ORDER BY name");
    if(!$result) exit("ERROR: Could not get ids and names of URL lists.");
    $first = true;
    while($row = $result->fetch_assoc()) {
        $id = $row["id"];
        $name = $row["name"];
        
        if($first) {
            if(!isset($config)) $config = $id;
            $first = false;
        }
        echo "<option value=\"".$id."\"";
        
        if($id == $config) echo " selected";
        
        echo ">$name</option>\n";
        
    }
    $result->close();
    if(!isset($config)) $config = 0;
    
    if($config) {
        $result = $dbConnection->query("SELECT name, config FROM crawlserv_configs WHERE id=$config LIMIT 1");
        if(!$result) exit("ERROR: Could not get configuration data.");
        $row = $result->fetch_assoc();
        $configName = $row["name"];
        if(!isset($current)) $current = $row["config"];
        $result->close();
    }
    else if(!isset($current)) $current = '[]';
    
    echo "<option value=\"0\"";
    if(!$config) echo " selected";
    echo ">Add new</option>\n";
    
    echo "</select>\n<a href=\"#\" class=\"actionlink config-delete\" data-m=\"$m\"><span class=\"remove-entry\">X</span></a></div>\n</div>\n";
}
?>
<div class="action-link-box">
<div class="action-link">
<?php
if($config) echo "<a href=\"#\" class=\"action-link config-duplicate\" data-m=\"$m\" data-mode=\"$mode\">Duplicate configuration</a>\n";
?>
</div>
</div>
</div>
<?php
if(isset($website)) {
    echo "<div class =\"content-block\">\n";
    echo "<div class=\"opt-block\">\n";
    echo "<div class=\"opt-label\">Name:</div>\n";
    echo "<div class=\"opt-value\" data-tippy=\"Name of the parsing configuration.\""
        ." data-tippy-delay=\"0\" data-tippy-duration=\"0\" data-tippy-arrow=\"true\" data-tippy-placement=\"left-start\""
        ." data-tippy-size=\"small\">\n";
    echo "<input type=\"text\" class=\"opt\" id=\"config-name\" value=\"";
    if(isset($_POST["name"])) echo $_POST["name"];
    else if($config) echo $configName;
    echo "\" />\n";
    echo "</div>\n";
    echo "</div>\n";
    echo "<div class=\"action-link-box\">\n";
    echo "<div class=\"action-link\">\n";
    if($config) {
        echo "<a href=\"#\" class=\"action-link config-update\" data-m=\"$m\" data-mode=\"$mode\">";
        echo "Change parser</a>\n";
    }
    else {
        echo "<a href=\"#\" class=\"action-link config-add\" data-module=\"parser\" data-m=\"$m\" data-mode=\"$mode\">";
        echo "Add parser</a>\n";
    }
    echo "</div>\n";
    echo "</div>\n";
    echo "<div id=\"config-view\"></div>\n";
    echo "<div class=\"action-link-box\">\n";
    echo "<div class=\"action-link\">\n";
    if($config) {
        echo "<a href=\"#\" class=\"action-link config-update\" data-m=\"$m\" data-mode=\"$mode\">";
        echo "Change parser</a>\n";
    }
    else {
        echo "<a href=\"#\" class=\"action-link config-add\" data-module=\"parser\" data-m=\"$m\" data-mode=\"$mode\">";
        echo "Add parser</a>\n";
    }
    echo "</div>\n";
    echo "</div>\n";
    echo "</div>\n";
}
?>
<script>
// load queries and configuration
<?php 
if($website) {
    echo "var db_queries = [\n";
    $result = $dbConnection->query("SELECT id,name,type,resultbool,resultsingle,resultmulti FROM crawlserv_queries WHERE website="
        .$website." OR website IS NULL ORDER BY name");
    if(!$result) exit("ERROR: Could not get ids and names of queries from database.");
    while($row = $result->fetch_assoc()) {
        echo " { \"id\": ".$row["id"].", \"name\": \"".$row["name"]."\", \"type\": \"".$row["type"]."\", \"resultbool\": "
            .($row["resultbool"] ? "true" : "false").", \"resultsingle\": ".($row["resultsingle"] ? "true" : "false")
            .", \"resultmulti\": ".($row["resultmulti"] ? "true" : "false")." },\n";
    }
    $result->close();
    echo "];\n";
    
    echo "var db_config = \n";
    if($config) {
        $result = $dbConnection->query("SELECT config FROM crawlserv_configs WHERE id=$config LIMIT 1");
        if(!$result) exit("ERROR: Could not get current config from database");
        $row = $result->fetch_assoc();
        echo " ".$row["config"];
        $result->close();
    }
    else echo " []\n";
    
    echo ";\n";
}
?>
	
// create configuration object
var config = new Config("parser", '<?php echo $current; ?>', "<?php echo $mode; ?>");
</script>

