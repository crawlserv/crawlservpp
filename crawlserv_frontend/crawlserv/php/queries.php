<?php

$db_init = true;
$cc_init = true;

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

<select id="query-select" class="entry-x-input" data-m="queries">

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

<a id="query-delete" href="#" class="actionlink"><span class="remove-entry">X</span></a>

</div>
</div>

<div class="action-link-box">
<div class="action-link">

<?php

if($query)
    echo "<a id=\"query-duplicate\" href=\"#\" class=\"action-link\">Duplicate query</a>\n";

?>

</div>
</div>
</div>

<div id="query-properties" class="content-block">
<div class="entry-row">
<div class="entry-label">Name:</div><div class="entry-input">

<input id="query-name" type="text" class="entry-input" value="<?php if($query) echo $queryName; ?>" />

</div>
</div>

<div class="entry-row">
<div class="entry-label">Type:</div><div class="entry-input">

<select id="query-type-select" class="entry-input">

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

<input id="query-result-bool" type="checkbox" class="entry-check-first"<?php

if(!$query || ($query && $queryResultBool))
    echo " checked";

?> /> boolean

<input id="query-result-single" type="checkbox" class="entry-check-next"<?php

if($query && $queryResultSingle)
    echo " checked";

?> /> single

<input id="query-result-multi" type="checkbox" class="entry-check-next"<?php

if($query && $queryResultMulti)
    echo " checked";
?> /> multiple

<input id="query-text-only" type="checkbox" class="entry-check-next"<?php

if($query && $queryTextOnly)
    echo " checked";

?> /> <span id="query-text-only-label">text only</span>

</div>
</div>

<div class="entry-row">
<div class="entry-label-top">Query text:</div><div class="entry-input">

<textarea id="query-text" class="entry-input" spellcheck="false" autocomplete="off"><?php

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
    echo "<a id=\"query-update\" href=\"#\" class=\"action-link\">Change query</a>";
else
    echo "<a id=\"query-add\" href=\"#\" class=\"action-link\">Add query</a>";

?>

</div>
</div>
</div>

<div id="xpath-helper" class="content-block">
<div class="entry-row">
<div class="entry-label-top helper">XPath Helper:</div><div class="entry-input xpath-helper-code">

<span class="tag">&lt;</span><input id="xpath-element" type="text" class="entry-input-inline xpath-helper tag" value="div"
	data-tippy="Name of the HTML tag" data-tippy-delay="0" data-tippy-duration="0" data-tippy-arrow="true" data-tippy-placement="left-start"
	data-tippy-size="small" />
	
<input id="xpath-property" type="text" class="entry-input-inline xpath-helper property" value="class"
	data-tippy="Name of the first HTML attribute" data-tippy-delay="0" data-tippy-duration="0" data-tippy-arrow="true"
	data-tippy-placement="left-start" data-tippy-size="small" />=<input id="xpath-value" type="text" 
	class="entry-input-inline-wide xpath-helper value" placeholder="?" data-tippy="Value of the first HTML attribute"
	data-tippy-delay="0" data-tippy-duration="0" data-tippy-arrow="true" data-tippy-placement="left-start"
	data-tippy-size="small" /> <input id="xpath-result-property" type="text" class="entry-input-inline xpath-helper property"
	value="href" data-tippy="Name of the second HTML attribute" data-tippy-delay="0" data-tippy-duration="0" data-tippy-arrow="true"
	data-tippy-placement="left-start" data-tippy-size="small" disabled />=<span class="value">&hellip;<input type="radio"
	name="xpath-result" value="property" data-tippy="Get the value of a HTML attribute" data-tippy-delay="0" data-tippy-duration="0"
	data-tippy-arrow="true" data-tippy-placement="left-start" data-tippy-size="small" />&hellip;</span><span
	class="tag">&gt;</span><span class="content">&hellip;<input type="radio" name="xpath-result" value="text"
	data-tippy="Get the inner content of the whole HTML tag" data-tippy-delay="0" data-tippy-duration="0" data-tippy-arrow="true"
	data-tippy-placement="left-start" data-tippy-size="small" checked />&hellip;</span>
	
</div>
</div>

<div class="action-link-box">
<div class="action-link">

<a id="xpath-generate" href="#" class="action-link" data-tippy="Let XPath Helper generate a XPath query for you"
	data-tippy-delay="0" data-tippy-duration="0" data-tippy-arrow="true" data-tippy-placement="left-start" data-tippy-size="small">
	Generate query for HTML tag
</a>

</div>
</div>
</div>

<div id="regex-helper" class="content-block">
<div class="entry-row">
<div class="entry-label-top helper">Help?!</div><div class="entry-input">

<p>Try <a href="https://regexr.com/" target="_blank">RegExr</a> or <a href="https://regex101.com/" target="_blank">regex101</a>
for help with your query.</p>

</div>
</div>
</div>

<div id="jsonpointer-helper" class="content-block">
<div class="entry-row">
<div class="entry-label-top helper">Note</div><div class="entry-input">

<p>Use '$$' as placeholder for multiple matches.<br>The placeholder will be replaced by 0...n.</p>

</div>
</div>
</div>

<div id="jsonpath-helper" class="content-block">
<div class="entry-row">
<div class="entry-label-top helper">Help?!</div><div class="entry-input">

<p>Try <a href="https://jsonpath.herokuapp.com/" target="_blank">Jayway JSONPath Evaluator</a> or <a href="https://jsonpath.curiousconcept.com/" target="_blank">JSONPATH Expression Tester</a>
for help with your query.</p>

</div>
</div>
</div>

<div class="content-block">
<div class="entry-row">
<div id="query-test-label" class="entry-label-top">Test text:</div><div class="entry-input">

<textarea id="query-test-text" class="entry-input" spellcheck="false" autocomplete="off"><?php

if(isset($_POST["test"]))
    echo htmlspecialchars($_POST["test"], ENT_QUOTES);

?>

</textarea>

<textarea id="query-test-result" class="entry-input" disabled></textarea>

</div>
</div>

<div class="action-link-box">
<div class="action-link">

<a id="query-test" href="#" class="action-link">Test query</a>

</div>
</div>
</div>
