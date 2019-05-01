<?php

require "_helpers.php";
require "config.php"; 

if(isset($_POST["website"]))
    $website = $_POST["website"];

if(isset($_POST["config"]))
    $config = $_POST["config"];

if(isset($_POST["mode"]))
    $mode = $_POST["mode"];
else
    $mode = "simple";

if(isset($_POST["current"]))
    $current = $_POST["current"];

?>

<h2>Parsing options<?php

echo "<span id=\"opt-mode\">\n";

if($mode == "simple")
    echo "<b>simple</b>";
else
    echo "<a href=\"#\" class=\"action-link post-redirect-mode\" data-m=\"$m\" data-mode=\"simple\">simple</a>\n";

echo " &middot; ";

if($mode == "advanced")
    echo "<b>advanced</b>";
else
    echo "<a href=\"#\" class=\"action-link post-redirect-mode\" data-m=\"$m\" data-mode=\"advanced\">advanced</a>\n";

echo "</span>\n";

?></h2>

<div class="content-block">

<?php

echo rowWebsiteSelect(false);

if(isset($website)) {
    flush();

    echo rowConfigSelect("Parser", true);
}

?>

<div class="action-link-box">
<div class="action-link">

<?php

if($config)
    echo "<a href=\"#\" class=\"action-link config-duplicate\" data-m=\"$m\" data-mode=\"$mode\">Duplicate configuration</a>\n";

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
    
    if(isset($_POST["name"]))
        echo $_POST["name"];
    else if($config)
        echo $configName;
    
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

// load locales, queries and configuration

<?php

echo scriptModule();

?>
	
// create configuration object

var config = new Config("parser", '<?php

echo $current;

?>', "<?php

echo $mode;

?>");

</script>

