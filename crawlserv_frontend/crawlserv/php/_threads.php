<?php
require "../config.php";

$result = $dbConnection->query("SELECT id,module,status,progress,paused,website,urllist,config FROM crawlserv_threads");
if(!$result) http_response_code(503);
$num = $result->num_rows;
$none = true;

if($num) {
    echo "<div class=\"content-block\">\n";
    echo "<div class=\"entry-row\">\n";
    if($num == 1) echo "<div class=\"value\">$num thread active: <span id=\"threads-ping\"></span></div>";
    else echo "<div class=\"value\">$num threads active: <span id=\"threads-ping\"></span></div>";
    echo "</div>\n";
    echo "</div>\n";

    while($row = $result->fetch_assoc()) {
        
        // get website namespace
        $result2 =
            $dbConnection->query("SELECT namespace FROM crawlserv_websites WHERE id=".$row["website"]." LIMIT 1");
        if(!$result2) http_response_code(503);
        $row2 = $result2->fetch_assoc();
        if(!$row2) http_response_code(503);
        $website = $row2["namespace"];
        $result2->close();
        
        // get URL list name
        $result2 =
            $dbConnection->query("SELECT namespace FROM crawlserv_urllists WHERE id=".$row["urllist"]." AND website=".$row["website"]." LIMIT 1");
        if(!$result2) http_response_code(503);
        $row2 = $result2->fetch_assoc();
        if(!$row2) http_response_code(503);
        $urllist = $row2["namespace"];
        $result2->close();
        
        // get config namespace
        $result2 =
            $dbConnection->query("SELECT name FROM crawlserv_configs WHERE id=".$row["config"]." AND website=".$row["website"]." LIMIT 1");
        if(!$result2) http_response_code(503);
        $row2 = $result2->fetch_assoc();
        if(!$row2) http_response_code(503);
        $config = $row2["name"];
        $result2->close();
     
        $none = false;
        
        echo "<div class=\"thread\">\n";
        echo "<div class=\"content-block\">\n";
        echo "<div class=\"entry-row small-text\">\n";
        echo "<b>".$row["module"]." #".$row["id"]."</b> - <i>".$config." on ".$website."_".$urllist."</i>\n";
        echo "</div>\n";
        echo "<div class=\"entry-row";
        if($row["paused"]) echo " value";
        echo "\">\n";
        echo "<span class=\"nowrap";
        if(substr($row["status"], 0, 6) == "ERROR ")
            echo " error\" title=\"".htmlentities(substr($row["status"], 6))."\">".substr($row["status"], 6);
        else if(substr($row["status"], 0, 12) == "INTERRUPTED ")
            echo " interrupted\" title=\"".htmlentities(substr($row["status"], 12))."\">".substr($row["status"], 12);
        else if(substr($row["status"], 0, 5) == "IDLE ")
            echo " idle\" title=\"".htmlentities(substr($row["status"], 5))."\">".substr($row["status"], 5);
        else echo "\" title=\"".htmlentities($row["status"])."\">".$row["status"];
        echo "</span>\n";
        echo "</div>\n";
        
        echo "<div class=\"action-link-box\">\n";
        echo "<div class=\"action-link\">\n";
        if($row["paused"]) {
            echo "<a href=\"#\" class=\"action-link thread-unpause\" data-id=\"".$row["id"]."\" data-module=\"".$row["module"]."\">";
            echo "Unpause</a>\n";
        }
        else {
            echo "<progress value=\"";
            if(floatval($row["progress"]))
                echo $row["progress"]."\" title=\"".number_format(round(floatval($row["progress"]) * 100, 2), 2)."%\" max=\"1";
            echo "\" />\n";
            echo "<a href=\"#\" class=\"action-link thread-pause\" data-id=\"".$row["id"]."\" data-module=\"".$row["module"]."\">";
            echo "Pause</a>\n";
        }
        echo " &middot; ";
        echo "<a href=\"#\" class=\"action-link thread-stop\" data-id=\"".$row["id"]."\" data-module=\"".$row["module"]."\">Stop</a>\n";
        echo "</div>\n";
        echo "</div>\n";
        echo "</div>\n";
    }
}
else {
    echo "<div class=\"content-block\">\n";
    echo "<div class=\"entry-row\">\n";
    echo "<div class=\"value\">No threads active. <span id=\"threads-ping\"></span></div>";
    echo "</div>\n";
    echo "</div>\n";
}
$result->close();
?>
