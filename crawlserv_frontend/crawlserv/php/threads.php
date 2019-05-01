<?php

require "_helpers.php";
require "config.php";

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

<?php 

echo rowWebsiteSelect(false, false, true);

?>

<div class="entry-row">
<div class="entry-label">Module:</div><div class="entry-input">

<select id="module-select" class="entry-input" data-m="threads">

<option value="crawler" <?php

if($module == "crawler")
    echo " selected";

?>>Crawler</option>

<option value="parser" <?php

if($module == "parser")
    echo " selected";

?>>Parser</option>

<option value="extractor" <?php

if($module == "extractor")
    echo " selected";

?>>Extractor</option>

<option value="analyzer" <?php
if($module == "analyzer")
    echo " selected";

?>>Analyzer</option>

</select>

</div>
</div>

<?php

if(isset($website)) {
    flush();
    
    echo rowConfigSelect(ucfirst($module), false, true);
    
    flush();
    
    echo rowUrlListSelect(false, false, true);
}

?>

<div class="action-link-box" id="scroll-to">
<div class="action-link">

<?php

if(isset($website))
    echo "<a href=\"#\" class=\"action-link thread-start\""
        ." data-module=\"$module\" data-noreload>Start thread</a>\n";

?>

</div>
</div>
</div>
