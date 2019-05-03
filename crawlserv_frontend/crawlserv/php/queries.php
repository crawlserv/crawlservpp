
<?php

require "_helpers.php";
require "config.php";

if(isset($_POST["website"]))
    $website = $_POST["website"];
else
    $website = 0;

if(isset($_POST["query"]))
    $query = $_POST["query"];

?>

<h2>Queries</h2>

<div class="content-block">

<?php

echo rowWebsiteSelect(false, true);

?>

<div class="entry-row">
<div class="entry-label">Query:</div><div class="entry-x-input">

<select class="entry-x-input" id="query-select" data-m="queries">

<?php

$result = $dbConnection->query(
    "SELECT name, version".
    " FROM crawlserv_versions"
);

if($result) {
    while($row = $result->fetch_assoc()) {
        if(strtolower($row["name"]) == "pcre2")
            $pcre_version = $row["version"];
        else if(strtolower($row["name"]) == "tidy-html5")
            $tidy_version = $row["version"];
        else if(strtolower($row["name"]) == "pugixml")
            $pugixml_version = $row["version"];
        else if(strtolower($row["name"]) == "rapidjson")
            $rapidjson_version = $row["version"];
        else if(strtolower($row["name"]) == "jsoncons")
            $jsoncons_version = $row["version"];
    }
    
    $result->close();
}

if(!isset($pcre_version))
    $pcre_version = "[NOT FOUND]";

if(!isset($tidy_version))
    $tidy_version = "[NOT FOUND]";

if(!isset($pugixml_version))
    $pugixml_version = "[NOT FOUND]";

if(!isset($rapidjson_version))
    $rapidjson_version = "[NOT FOUND]";

if(!isset($jsoncons_version))
    $jsoncons_version = "[NOT FOUND]";

if(!$website)
    $result = $dbConnection->query("SELECT id,name FROM crawlserv_queries WHERE website IS NULL ORDER BY name");
else
    $result = $dbConnection->query("SELECT id,name FROM crawlserv_queries WHERE website=$website ORDER BY name");

if(!$result)
    die("ERROR: Could not get IDs and names of queries.");

$first = true;

while($row = $result->fetch_assoc()) {
    $id = $row["id"];
    $name = $row["name"];
    
    if($first) {
        if(!isset($query)) $query = $id;
        
        $first = false;
    }
    
    echo "<option value=\"".$id."\"";
    
    if($query == $id) {
        echo " selected";
        
        $queryName = $name;
    }
    
    echo ">".htmlspecialchars($name)."</option>\n";
}

$result->close();

if(isset($query)) {
    if($query) {
        $result = $dbConnection->query(
            "SELECT type,query,resultbool,resultsingle,resultmulti,textonly FROM crawlserv_queries".
            " WHERE id=".$query." LIMIT 1"
        );
        
        if(!$result)
            die("ERROR: Could not get query properties from database.");
        
        $row = $result->fetch_assoc();
        
        $queryType = $row["type"];
        $queryText = $row["query"];
        $queryResultBool = $row["resultbool"];
        $queryResultSingle = $row["resultsingle"];
        $queryResultMulti = $row["resultmulti"];
        $queryTextOnly = $row["textonly"];
        
        $result->close();
    }
}
else
    $query = 0;

?>

<option value="0"<?php if(!$query) echo " selected"; ?>>Add new</option>

</select>

<a href="#" class="actionlink query-delete"><span class="remove-entry">X</span></a>

</div>
</div>

<div class="action-link-box">
<div class="action-link">

<?php

if($query)
    echo "<a href=\"#\" class=\"action-link query-duplicate\">Duplicate query</a>\n";

?>

</div>
</div>
</div>

<div class="content-block" id="query-properties">
<div class="entry-row">
<div class="entry-label">Name:</div><div class="entry-input">

<input type="text" class="entry-input" id="query-name" value="<?php if($query) echo $queryName; ?>" />

</div>
</div>

<div class="entry-row">
<div class="entry-label">Type:</div><div class="entry-input">

<select class="entry-input" id="query-type-select">

<option value="regex"<?php

if($query && $queryType == "regex")
    echo " selected";

?>>RegEx (PCRE2 v<?php echo $pcre_version; ?>)</option>

<option value="xpath"<?php

if($query && $queryType == "xpath")
    echo " selected";

?>>XPath (tidy v<?php echo $tidy_version; ?>, pugixml v<?php echo $pugixml_version; ?>)</option>

<option value="jsonpointer"<?php

if($query && $queryType == "jsonpointer")
    echo " selected";

?>>JSONPointer (RapidJSON v<?php echo $rapidjson_version; ?>)</option>
 
<option value="jsonpath"<?php

if($query && $queryType == "jsonpath")
    echo " selected";

?>>JSONPath (jsoncons v<?php echo $jsoncons_version; ?>)</option>

</select>

</div>
</div>

<div class="entry-row">
<div class="entry-label">Result:</div><div class="entry-input">

<input type="checkbox" id="query-result-bool" class="entry-check-first"<?php

if(!$query || ($query && $queryResultBool))
    echo " checked";

?> /> boolean

<input type="checkbox" id="query-result-single" class="entry-check-next"<?php

if($query && $queryResultSingle)
    echo " checked";

?> /> single

<input type="checkbox" id="query-result-multi" class="entry-check-next"<?php

if($query && $queryResultMulti)
    echo " checked";
?> /> multiple

<input type="checkbox" id="query-text-only" class="entry-check-next"<?php

if($query && $queryTextOnly)
    echo " checked";

?> /> <span id="query-text-only-label">text only</span>

</div>
</div>

<div class="entry-row">
<div class="entry-label-top">Query text:</div><div class="entry-input">

<textarea class="entry-input" id="query-text" spellcheck="false" autocomplete="off"><?php

if($query)
    echo htmlspecialchars($queryText, ENT_QUOTES);

?>

</textarea>

</div>
</div>

<div class="action-link-box">
<div class="action-link">

<?php

if($query)
    echo "<a href=\"#\" class=\"action-link query-update\">Change query</a>";
else
    echo "<a href=\"#\" class=\"action-link query-add\">Add query</a>";

?>

</div>
</div>
</div>

<div class="content-block" id="xpath-helper">
<div class="entry-row">
<div class="entry-label-top helper">XPath Helper:</div><div class="entry-input xpath-helper-code">

<span class="tag">&lt;</span><input type="text" class="entry-input-inline xpath-helper tag" id="xpath-element" value="div"
	data-tippy="Name of the HTML tag" data-tippy-delay="0" data-tippy-duration="0" data-tippy-arrow="true" data-tippy-placement="left-start"
	data-tippy-size="small" />
	
<input type="text" class="entry-input-inline xpath-helper property" id="xpath-property" value="class"
	data-tippy="Name of the first HTML attribute" data-tippy-delay="0" data-tippy-duration="0" data-tippy-arrow="true"
	data-tippy-placement="left-start" data-tippy-size="small" />=<input type="text" class="entry-input-inline-wide xpath-helper value"
	id="xpath-value" placeholder="?" data-tippy="Value of the first HTML attribute" data-tippy-delay="0" data-tippy-duration="0"
	data-tippy-arrow="true" data-tippy-placement="left-start" data-tippy-size="small" /> <input type="text"
	class="entry-input-inline xpath-helper property" id="xpath-result-property" value="href" disabled
	data-tippy="Name of the second HTML attribute" data-tippy-delay="0" data-tippy-duration="0" data-tippy-arrow="true"
	data-tippy-placement="left-start" data-tippy-size="small" />=<span class="value">&hellip;<input type="radio" name="xpath-result" value="property"
	data-tippy="Get the value of a HTML attribute" data-tippy-delay="0" data-tippy-duration="0" data-tippy-arrow="true"
	data-tippy-placement="left-start" data-tippy-size="small" />&hellip;</span><span class="tag">&gt;</span><span class="content">&hellip;<input
	type="radio" name="xpath-result" value="text" checked data-tippy="Get the inner content of the whole HTML tag" data-tippy-delay="0"
	data-tippy-duration="0" data-tippy-arrow="true" data-tippy-placement="left-start" data-tippy-size="small" />&hellip;</span>
	
</div>
</div>

<div class="action-link-box">
<div class="action-link">

<a href="#" class="action-link xpath-generate" id="xpath-generate"  data-tippy="Let XPath Helper generate a XPath query for you"
	data-tippy-delay="0" data-tippy-duration="0" data-tippy-arrow="true" data-tippy-placement="left-start" data-tippy-size="small">
	Generate query for HTML tag
</a>

</div>
</div>
</div>

<div class="content-block" id="regex-helper">
<div class="entry-row">
<div class="entry-label-top helper">Help?!</div><div class="entry-input">

<p>Try <a href="https://regexr.com/" target="_blank">RegExr</a> or <a href="https://regex101.com/" target="_blank">regex101</a>
for help with your query.</p>

</div>
</div>
</div>

<div class="content-block" id="jsonpointer-helper">
<div class="entry-row">
<div class="entry-label-top helper">Note</div><div class="entry-input">

<p>Use '$$' as placeholder for multiple matches.<br>The placeholder will be replaced by 0...n.</p>

</div>
</div>
</div>

<div class="content-block" id="jsonpath-helper">
<div class="entry-row">
<div class="entry-label-top helper">Help?!</div><div class="entry-input">

<p>Try <a href="https://jsonpath.herokuapp.com/" target="_blank">Jayway JSONPath Evaluator</a> or <a href="https://jsonpath.curiousconcept.com/" target="_blank">JSONPATH Expression Tester</a>
for help with your query.</p>

</div>
</div>
</div>

<div class="content-block">
<div class="entry-row">
<div class="entry-label-top" id="query-test-label">Test text:</div><div class="entry-input">

<textarea class="entry-input" id="query-test-text" spellcheck="false" autocomplete="off"><?php

if(isset($_POST["test"]))
    echo htmlspecialchars($_POST["test"], ENT_QUOTES);

?>

</textarea>

<textarea class="entry-input" id="query-test-result" disabled></textarea>

</div>
</div>

<div class="action-link-box">
<div class="action-link">

<a href="#" class="action-link query-test">Test query</a>

</div>
</div>
</div>
