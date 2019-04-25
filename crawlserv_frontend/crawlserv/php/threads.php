<?php
if(isset($_POST["website"]))
    $website = $_POST["website"];

if(isset($_POST["module"]))
    $module = $_POST["module"];
else
    $module = "crawler";

if(isset($_POST["config"]))
    $config = $_POST["config"];

if(isset($_POST["urllist"]))
    $urllist = $_POST["urllist"];
?>
<h2>Threads</h2>
<div id="threads">
<div class="content-block">
<div class="entry-row">
<div class="entry-value">Loading...</div>
</div>
</div>
</div>
<div class="content-block">
<div class="entry-row">
<div class="entry-label">Website:</div><div class="entry-input">
<select class="entry-input" id="website-select" data-m="threads" data-scrolldown >
<?php
$result = $dbConnection->query("SELECT id,name FROM crawlserv_websites ORDER BY name");

if(!$result)
    exit("ERROR: Could not get ids and names of websites.");

$first = true;

while($row = $result->fetch_assoc()) {
    $id = $row["id"];
    $name = $row["name"];
    
    if($first) {
        if(!isset($website))
            $website = $id;
        
        $first = false;
    }
    
    echo "<option value=\"".$id."\"";
    
    if($website == $id)
        echo " selected";

    echo ">".htmlspecialchars($name)."</option>\n";
}

$result->close();

if(!isset($website))
    echo "<option disabled>No websites available</option>\n";
?>
</select>
</div>
</div>
<div class="entry-row">
<div class="entry-label">Module:</div><div class="entry-input">
<select id="module-select" class="entry-input" data-m="threads">
<option value="crawler" <?php if($module == "crawler") echo " selected"; ?>>Crawler</option>
<option value="parser" <?php if($module == "parser") echo " selected"; ?>>Parser</option>
<option value="extractor" <?php if($module == "extractor") echo " selected"; ?>>Extractor</option>
<option value="analyzer" <?php if($module == "analyzer") echo " selected"; ?>>Analyzer</option>
</select>
</div>
</div>
<?php
if(isset($website)) {
    echo "<div class=\"entry-row\">\n";
    echo "<div class=\"entry-label\">Config:</div><div class=\"entry-input\">\n";
    echo "<select id=\"config-select\" class=\"entry-input\" data-noreload>\n";
    
    $result = $dbConnection->query("SELECT id, name FROM crawlserv_configs WHERE website=$website AND module='$module'");
    
    if(!$result)
        exit("ERROR: Could not get IDs and names of configurations.");
    
    $first = true;
    
    while($row = $result->fetch_assoc()) {
        $id = $row["id"];
        $name = $row["name"];
        
        if($first) {
            if(!isset($config))
                $config = $id;
            
            $first = false;
        }
        
        echo "<option value=\"".$id."\"";
        
        if($id == $config)
            echo " selected";
        
        echo ">$name</option>\n";
        
    }
    
    $result->close();
    
    if(!isset($config))
        echo "<option disabled>No configurations available</option>\n";
    
    echo "</select>\n";
    echo "</div>\n";
    echo "</div>\n";
    echo "<div class=\"entry-row\">\n";
    echo "<div class=\"entry-label\">URL list:</div><div class=\"entry-input\">\n";
    echo "<select id=\"urllist-select\" class=\"entry-input\" data-noreload>\n";
    
    $result = $dbConnection->query("SELECT id, name FROM crawlserv_urllists WHERE website=$website");
    
    if(!$result)
        exit("ERROR: Could not get IDs and names of URL lists.");
    
    $first = true;
    
    while($row = $result->fetch_assoc()) {
        $id = $row["id"];
        $name = $row["name"];
        
        if($first) {
            if(!isset($urllist))
                $urllist = $id;
            
            $first = false;
        }
        
        echo "<option value=\"".$id."\"";
        
        if($id == $urllist)
            echo " selected";
        
        echo ">$name</option>\n";
        
    }
    
    $result->close();
    
    if(!isset($urllist))
        echo "<option disabled>No URL list available</option>\n";
    
    echo "</select>\n";
    echo "</div>\n";
    echo "</div>\n";
}
?>
<div class="action-link-box" id="scroll-to">
<div class="action-link">
<?php
if(isset($website))
    echo "<a href=\"#\" class=\"action-link thread-start\" data-module=\"$module\" data-noreload>Start thread</a>\n";
?>
</div>
</div>
</div>
