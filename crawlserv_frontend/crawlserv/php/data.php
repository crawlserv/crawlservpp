<?php

require "_helpers.php";
require "config.php";

if(isset($_POST["action"]))
    $action = $_POST["action"];
else
    $action = "import";

if(isset($_POST["website"]))
    $website = $_POST["website"];

?>

<h2>Import/Export</h2>

<div class="content-block">
<div class="entry-row">

<div class="entry-label">Action:</div><div class="entry-input">

<input type="radio" name="action" value="import"<?php

if($action == "import")
    echo " checked";

?> /> Import &emsp;

<input type="radio" name="action" value="merge"<?php

if($action == "merge")
    echo " checked";

?> /> Merge &emsp;

<input type="radio" name="action" value="export"<?php

if($action == "export")
    echo " checked";

?> /> Export

</div>
</div>

<div class="entry-row">

</div>
</div>
